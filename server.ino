#include "main.h"

// Chân kết nối
#define NRST 12
#define BOOT0 13
#define LED LED_BUILTIN // dùng cho phiên bản có led hiển thị

// Chế độ chạy cho STM32
#define mFlash 1
#define mRun 0

#define MAX_SRV_CLIENTS 5
#define _err  255
#define _cliStop  254

//Thông tin wifi mặc định
String ssid;
String password;

IPAddress local_IP(192, 168, 0, 2);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

const char* resErr0 = "Err0";

// Biến phục vụ flash smt32
uint8_t read[256];// buffer đệm cho quá trình flash
int i = 0;
int rdtmp;
int stm32ver;

// Biến phục vụ server
WiFiServer TCPserver(8686);
WiFiClient TCPserverClients[MAX_SRV_CLIENTS];
uint32_t clientTimeout[MAX_SRV_CLIENTS];

ESP8266WebServer server(80); //port 80
File fsUploadFile;

/**
 * kiểm tra kết nối mới
 * Trả về trạng thái kết nối
 * - kết nối thành công -> số thứ tự kết nối
 * - Kết nối không thành công -> mã lỗi(255)
 * - Quá thời gian kết nối -> số thứ tự của kết nối bị hủy
 */
uint8_t IsNewClient(){
  uint8_t i;
  //check if there are any new clients
  if (TCPserver.hasClient()){

    for(i = 0; i < MAX_SRV_CLIENTS; i++){
  /// timeout connect
      if(clientTimeout[i] < millis() && TCPserverClients[i]){
          TCPserverClients[i].stop();
        }
        
      //find free/disconnected slot//spot
      if (!TCPserverClients[i] || !TCPserverClients[i].connected()){ 
        if(TCPserverClients[i]){
          TCPserverClients[i].stop();
        }
        TCPserverClients[i] = TCPserver.available();
        clientTimeout[i] = millis()+60000;
        return i;
      }
    }
    //no free/disconnected spot so reject
    WiFiClient TCPserverClient = TCPserver.available();
    TCPserverClient.stop();
    return _cliStop;    
  }
  return _err; 
}

/**
 * Kiểm tra request từ client
 * Trả về số thứ tự của kết nối gửi request
 */
uint8_t  IsRequest(){
  uint8_t i;

  //check clients for data
  for(i = 0; i < MAX_SRV_CLIENTS; i++){
    if (TCPserverClients[i] && TCPserverClients[i].connected()){
      if(TCPserverClients[i].available())      {
        digitalWrite(LED, LOW);

        // chạy tuần tự cho đến khi hết số byte trong bộ đệm
        while (TCPserverClients[i].available()>0) {
          Serial.write(TCPserverClients[i].read());         
          }
        clientTimeout[i] = millis()+60000;              
        digitalWrite(LED, HIGH);
        return i;
      }    
    }
  }
  return _err;
}
/**
 * Tiến trình xử lý lệnh Flash STM32
 */
void handleFlash()
{
  String flashwr;
  int lastbuf = 0;
  uint8_t cflag, fnum = 256;
  //get file name
  String f = server.arg("file");
  f='/'+f;
  if (!SPIFFS.exists(f))
  return server.send(200, "text/plain", "Not Found:" +f);

  return server.send(200, "text/plain", "Found:" +f);

  //Giảm tốc độ nạp, đảm bảo giữ liệu truyền nhận
  Serial.begin(9600, SERIAL_8E1); 

  delay(50);
//===== init
unsigned char reInit=5,tmp8;

// kết nối chip tối đa 5 lần
  while(reInit){
   
    STM32Mode(mFlash); // reset chip về chế độ nạp
    Serial.write(STM32INIT);
    delay(100);
    if (Serial.available() > 0)
    {
      tmp8 = (unsigned char)Serial.read();
      if (tmp8 == STM32ACK) 
        break;
    }
    
    reInit--;
  }
  
  if(reInit ==0){
    server.send(200, "text/plain", "Not found Chip!");
    goto endflash;
  }
else{
  // lấy thông tin chip 
  flashwr ="ID: " + String(stm32GetId(),HEX);
  tmp8 = stm32Version();
  flashwr +=" Bootloader Ver: ";
  flashwr += String((tmp8 >> 4) & 0x0F) + "." + String(tmp8 & 0x0F); 
}

///======= delete file in stm32
  if (stm32Erase() == STM32ACK)
        flashwr += "Erase OK";
      else if (stm32Erasen() == STM32ACK)
        flashwr += "Erase OK";
      else{
        flashwr += "Erase ER";
        goto endflash;
      }
//==== open file in flash(esp8266)      
  if (SPIFFS.exists(f)){
  fsUploadFile = SPIFFS.open(f, "r");
  if (fsUploadFile) {
    flashwr += f + "; ";
    i = fsUploadFile.size() / 256;
    lastbuf = fsUploadFile.size() % 256;
    flashwr += String(i) + "-" + String(lastbuf) + "; ";
    // gửi nội dung file sang chip
    for (int j = 0; j < i; j++) {
      fsUploadFile.read(read, 256);
      stm32SendCommand(STM32WR);
      delay(10);
      while (!Serial.available()) ;
      cflag = Serial.read();
      if (cflag == STM32ACK){
        if (stm32Address(STM32STADDR + (256 * j)) == STM32ACK) {
          if (stm32SendData(read, 255) == STM32ACK){
            //flashwr += ".";
          }   
          else {
            flashwr += " ErPg " + String(j);// lỗi-> trả về số thứ tự của gói lỗi
            break;
          }
        }else {
            flashwr += " ErAd " + String(j);// lỗi tràn bộ nhớ
            break;
          }
      }else {
            flashwr += " ErNaWr " + String(j);// lỗi không nhận phản hồi từ chip
            break;
          }
    }
    fsUploadFile.read(read, lastbuf);
    stm32SendCommand(STM32WR);
    delay(10);
    while (!Serial.available()) ;
    cflag = Serial.read();
    if (cflag == STM32ACK)
      if (stm32Address(STM32STADDR + (256 * i)) == STM32ACK) {
        if (stm32SendData(read, lastbuf) == STM32ACK)
          flashwr += " Finished";
        else {
          flashwr += " ErPg last";
        }
      }
    
    fsUploadFile.close();
    goto endflash;
  }
  }else {
    flashwr +="file not found!"; // không thấy file trong flash(esp8266)
  }
  endflash:
      STM32Mode(mRun); // reset chip về chế độ thường
      server.send(200, "text/plain", flashwr);
// trả về tốc độ giao tiếp
      Serial.begin(115200);
      delay(50);
}

void STM32Mode(unsigned char m)  {   
  digitalWrite(BOOT0, (bool)m);
  delay(10);
  digitalWrite(NRST, LOW);
  delay(10);
  digitalWrite(NRST, HIGH);
  delay(500);
}

void startServer() {
  server.on("/",handleIndexFile);
  server.on("/read",handleFileRead);
  server.on("/delete", HTTP_GET,handleFileDelete);
  // list available files
  server.on("/list", HTTP_GET, handleFileList);
  
  // handle file upload
 
  server.on("/upload", HTTP_POST, [](){

     if(!server.authenticate("admin","admin")) {
    return server.requestAuthentication();
  }
    server.send(200, "text/plain", "Tải lên thành công.");// Send status 200 (OK) to tell the client we are ready to receive
  }, handleFileUpload); // Receive and save the file
  
  server.on("/wfconfig", HTTP_POST, []() {
      String res;
      if(server.hasArg("nWf")&&server.hasArg("pWf")&&(!server.hasArg("defa"))){
        writeString(nWf,server.arg("nWf"));
        writeString(pWf,server.arg("pWf"));
        res = "save ok.";
      }
      else if(server.hasArg("defa")){
        if(server.arg("defa")=="true")
          res = "VMS_"+String(ESP.getChipId());
      } else 
        res = resErr0;
      server.send(200, "text/plain", res);
    });
    
  server.on("/infor", HTTP_GET, []() {
      String res;
       res = read_String(nWf)+ ';';
    res += read_String(pWf)+ ';';  
      server.send(200, "text/plain", res);
    });
  server.on("/flash", HTTP_POST,handleFlash); 
  server.on("/wifi_rst", HTTP_POST, []() {     
      server.send(200, "text/plain","wifi_rst");
      
    }); 
  server.on("/syst_rst", HTTP_POST, []() {     
      server.send(200, "text/plain","syst_rst");
      STM32Mode(mRun);
    });   
  server.onNotFound(handleNotFound); 
  server.begin();                  //Start server

}

void startWiFi() { 
  digitalWrite(LED, LOW);

  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  delay(100);
   
  digitalWrite(LED, HIGH);

}

void setup(void){
  
  pinMode(BOOT0, OUTPUT);
  pinMode(NRST, OUTPUT);
  pinMode(LED, OUTPUT);
  
  digitalWrite(LED, HIGH);
  digitalWrite(BOOT0, LOW);
  digitalWrite(NRST, HIGH);
  
  Serial.begin(115200);
  SPIFFS.begin();

  EEPROM.begin(80);
  delay(10);
  if(EEPROM.read(0)==0||EEPROM.read(0)==0xff){
    ssid ="VMS_" + String(ESP.getChipId());//Lấy tên thiết bị làm tên wifi
    password ="12345678";
    writeString(nWf,ssid);delay(5); 
    writeString(pWf,password);delay(5);
  } else{
    ssid =read_String(nWf);
    password =read_String(pWf);
  }
        
  startWiFi();

  startServer();

  TCPserver.begin();

  //STM32Mode(mRun); 
  
}

void loop(void){

  server.handleClient();          //Handle client requests

  IsNewClient();
  IsRequest();

 while(Serial.available() >0){
         
  uint8_t i=0;
  unsigned char tmp; 
  tmp = Serial.read();
  for(i = 0; i < MAX_SRV_CLIENTS; i++){    
  if (TCPserverClients[i] && TCPserverClients[i].connected()){
        //digitalWrite(LED, LOW);      
        TCPserverClients[i].write(hex2str(tmp>>4));
        TCPserverClients[i].write(hex2str(tmp&0xf));
        //digitalWrite(LED, HIGH);
    }
  }
  }  
 
}
/**
 * input 4bit value
 */
char hex2str(unsigned char niNum){ 
  return (niNum > 9) ? ('A' + (niNum-10)) : ('0' + niNum);
}
