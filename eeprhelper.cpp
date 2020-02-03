#include "eeprhelper.h"

//=================
// EEPROM
//=================
void writeString(char add,String data)
{
  int _size = data.length();
  int i;
  for(i=0;i<_size;i++)
  {
    EEPROM.write(add+i,data[i]);
  }
  EEPROM.write(add+_size,'\0');   
  EEPROM.commit();
}

String read_String(char add)
{
  int i;
  char data[15]; //Max 16 Bytes
  int len=0;
  unsigned char k;
  k=EEPROM.read(add);
  while(k != '\0' && len<16)   
  {    
    k=EEPROM.read(add+len);
    data[len]=k;
    len++;
  }
  data[len]='\0';
  return String(data);
}
//==============================================
