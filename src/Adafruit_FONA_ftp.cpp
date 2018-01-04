#include "Adafruit_FONA_ftp.h"

Adafruit_FONA_ftp::Adafruit_FONA_ftp(int8_t r)
  : Adafruit_FONA(r)
{
}

/*
boolean Adafruit_FONA_ftp::sendCheckOK(FONAFlashStringPtr send, uint16_t timeout) {
  return (sendCheckReply(send, ok_reply, timeout));
}

boolean Adafruit_FONA_ftp::sendCheckOK(char* send, uint16_t timeout) {
  return (sendCheckReply(send, ok_reply, timeout));
}
*/





/*
void Adafruit_FONA_ftp::showline(char *buf) {
  int x;

  readline();

  for (x = 0; x < 128; x++) {
    if (!(buf[x] = replybuffer[x])) break;
  }
}



boolean Adafruit_FONA_ftp::getFtp(void) {
  uint16_t status = 1;

  while (status) {

    //parseReply(F("+FTPGETTOFS:"), &status, ',', 0);
    //AT+FTPGETTOFS?
  }

}
*/

//boolean Adafruit_FONA::parseReply(FONAFlashStringPtr toreply,
          //uint16_t *v, char divider, uint8_t index) {
