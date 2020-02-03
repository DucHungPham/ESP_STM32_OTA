#include "main.h"

#define NRST 12
#define BOOT0 13
#define LED LED_BUILTIN

#define mFlash 1
#define mRun 0

#define MAX_SRV_CLIENTS 5
#define port  8686
#define _err  255
#define _cliStop  254

//SSID and Password of your WiFi router
String ssid = "VMS_" + String(ESP.getChipId());
const char* password = "12345678";

const char* resErr0 = "Err0";
IPAddress local_IP(192, 168, 0, 2);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

uint8_t read[256];
int i = 0;
int rdtmp;
int stm32ver;

WiFiServer TCPserver(port);
WiFiClient TCPserverClients[MAX_SRV_CLIENTS];
uint32_t clientTimeout[MAX_SRV_CLIENTS];

ESP8266WebServer server(80); //Server on port 80
File fsUploadFile;

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
        clientTimeout[i] = millis()+5000;
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

// return ID of client in ClientList
uint8_t  IsRequest(){
  uint8_t i;

  //check clients for data
  for(i = 0; i < MAX_SRV_CLIENTS; i++){
    if (TCPserverClients[i] && TCPserverClients[i].connected()){
      if(TCPserverClients[i].available())      {
        digitalWrite(LED, LOW);

        String recCli ="";
        while (TCPserverClients[i].available()>0) {
          Serial.write(TCPserverClients[i].read());         
          }
        clientTimeout[i] = millis()+5000;
              
        digitalWrite(LED, HIGH);
        return i;
      }    
    }
  }
  return _err;
}

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
  
  Serial.begin(9600, SERIAL_8E1);

  delay(50);
//===== init
unsigned char reInit=5,tmp8;
  while(reInit){
   
    STM32Mode(mFlash);
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
    server.send(200, "text/plain", "Khong ket noi dc");
    goto endflash;
  }
else{ 
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
//==== open file in flash      
  if (SPIFFS.exists(f)){
  fsUploadFile = SPIFFS.open(f, "r");
  if (fsUploadFile) {
    flashwr += f + "; ";
    i = fsUploadFile.size() / 256;
    lastbuf = fsUploadFile.size() % 256;
    flashwr += String(i) + "-" + String(lastbuf) + "; ";
    for (int j = 0; j < i; j++) {// sua i-->j
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
            flashwr += " ErPg " + String(j);
            break;
          }
        }else {
            flashwr += " ErAd " + String(j);
            break;
          }
      }else {
            flashwr += " ErNaWr " + String(j);
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
    flashwr +="file not found!";
  }
  endflash:
      STM32Mode(mRun);
      server.send(200, "text/plain", flashwr);

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

void hibernate(int pInterval) {
  WiFi.disconnect();
  ESP.deepSleep(10 * 600000 * pInterval, WAKE_RFCAL);
  delay(100);
}

void startServer() {
  server.on("/",handleIndexFile);
  server.on("/read",handleFileRead);
  server.on("/delete", HTTP_GET,handleFileDelete);
  // list available files
  server.on("/list", HTTP_GET, handleFileList);
  
  // handle file upload
 
  server.on("/upload", HTTP_POST, [](){

    // if(!server.authenticate("admin","admin")) {
  //  return server.requestAuthentication();
  //}
    server.send(200, "text/plain", "Tải lên thành công.");// Send status 200 (OK) to tell the client we are ready to receive
  }, handleFileUpload); // Receive and save the file
  
  server.on("/wfconfig", HTTP_POST, []() {
      String res;
      //nWf = server.hasArg("nWf")? server.arg("nWf"):"None";
      //pWf = server.hasArg("pWf")? server.arg("pWf"):"None";
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
    
  server.on("/Accconfig", HTTP_POST, []() {
      String res;
      if(server.hasArg("nAcc")&&server.hasArg("pAcc")){
        writeString(nAcc,server.arg("nAcc"));
        writeString(pAcc,server.arg("pAcc"));
        res = "save ok.";
      }
      else 
        res = resErr0;
      server.send(200, "text/plain", res);
    });
  server.on("/infor", HTTP_GET, []() {
      String res;
       res = read_String(nWf)+ ';';
    res += read_String(pWf)+ ';';
    res +=read_String(nAcc)+ ';';
    res += read_String(pAcc)+ ';';
    
      server.send(200, "text/plain", res);
    });
  server.on("/flash", HTTP_POST,handleFlash); 
    
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
  Serial.begin(115200);
  SPIFFS.begin();
  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println("");
  EEPROM.begin(80);
  delay(10);
  if(EEPROM.read(0)==0||EEPROM.read(0)==0xff){
    writeString(nWf,"VMS_" + String(ESP.getChipId()));delay(5);
    writeString(pWf,"12345678");delay(5);
    //writeString(nAcc,"admin");delay(5);
    //writeString(pAcc,"admin");delay(5);
  }
      
  //Onboard LED port Direction output
  pinMode(BOOT0, OUTPUT);
  pinMode(NRST, OUTPUT);
  pinMode(LED, OUTPUT);
  
  digitalWrite(LED, HIGH);
  
  startWiFi();

  startServer();

  TCPserver.begin();

  //STM32Mode(mRun); 
  digitalWrite(BOOT0, LOW);
  digitalWrite(NRST, HIGH);
  
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
