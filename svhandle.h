#ifndef __svhandle_h_
#define __svhandle_h_

#include "main.h"

extern ESP8266WebServer server;
extern File fsUploadFile;
void handleFileUpload();
void handleFileDelete();
void handleFileList();
void handleIndexFile();
void handleNotFound();
bool handleFileRead();

#endif
