#include "svhandle.h"
//=================
// server handle
//=================

void handleFileUpload()
{
    if (server.uri() != "/upload") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START)
  {
    String filename = upload.filename;
    //if(!filename.startsWith("/"))
    //  filename = "/"+filename;
    //Serial.print("handleFileUpload Name: "); Serial.println(filename);

    //  (1) Rename the old file
    /*
    if (SPIFFS.exists(upload.filename.c_str()))
    {
      SPIFFS.rename(upload.filename.c_str(),(upload.filename+".BAK").c_str());
    }
    */
    // check file extension?
    if(!SPIFFS.exists("/factory.bin")){
      filename = "/factory.bin"; 
    }
    else{
        if (SPIFFS.exists("/backup.bin")){
            SPIFFS.remove("/backup.bin");
        }
        if (SPIFFS.exists("/update.bin")){
          SPIFFS.rename("/update.bin","/backup.bin");         
        }
        filename = "/update.bin";
    } 
   
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE)
  {
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END)
  {
    if(fsUploadFile){
      fsUploadFile.close();
    //Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
    //server.sendHeader("Location","/success.html");      // Redirect the client to the success page
     // server.send(303);
    
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

// Delete STM32 Bin file from the flash of ESP8266 and shown on HTTPserver
//http://192.168.1.42/delete?file=CBT6.bin
void handleFileDelete() {
String f = server.arg("file");
 f='/'+f;
  if (SPIFFS.exists(f)) {
    //server.send(200, "text/html", makePage("Deleted", "<h2>" + FileList + " be deleted!<br><br><a style=\"color:white\" href=\"/list\">Return </a></h2>"));
    SPIFFS.remove(f);
     return server.send(200, "text/plain", "đã xóa:" + f);
  }
  else
    return server.send(200, "text/plain", "Not Found:" +f);
}

/*
String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}
*/
// List STM32 Bin files in the flash of ESP8266 and shown on HTTPserver
void handleFileList()
{
  String path = "/";
  // Assuming there are no subdirectories
  Dir dir = SPIFFS.openDir(path);
  String output = "[";
  while(dir.next())
  {
    File entry = dir.openFile("r");
    // Separate by comma if there are multiple files
    if(output != "[")
      output += ",";
    output += String(entry.name()).substring(1);
    entry.close();
  }
  output += "]";
  server.send(200, "text/plain", output);
  
}

bool handleFileRead() { // send the right file to the client (if it exists)
  //Serial.println("handleFileRead: " + path);
  //if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  //String contentType = getContentType(path);             // Get the MIME type
  //String pathWithGz = path + ".gz";
  //if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
String f = server.arg("file");
 f='/'+f;
   if (SPIFFS.exists(f))
  { // If the file exists, either as a compressed archive, or normal
    //if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
     // path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(f, "r");                    // Open the file
    size_t sent = server.streamFile(file,"application/octet-stream" );    // Send it to the clientcontentType
    file.close();                                          // Close the file again
    server.send(200, "text/plain", "Sent file:" +f);
    //Serial.println(String("\tSent file: ") + f);
    return true;
  }
  server.send(200, "text/plain", "Not Found:" +f);
  //Serial.println(String("\tFile Not Found: ") + f);   // If the file doesn't exist, return false
  return false;
}
void handleIndexFile()
{
  File file = SPIFFS.open("/index.html","r");
  server.streamFile(file, "text/html");
  file.close();

}

void handleNotFound(){ // if the requested file or page doesn't exist, return a 404 not found error
  //if(!handleFileRead(server.uri()))
  {          // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

//==============================================
