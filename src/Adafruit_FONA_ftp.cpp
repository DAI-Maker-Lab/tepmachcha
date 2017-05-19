#include "Adafruit_FONA_ftp.h"

Adafruit_FONA_ftp::Adafruit_FONA_ftp(int8_t r)
  : Adafruit_FONA(r)
{
}

void Adafruit_FONA_ftp::showline(char *buf) {
  int x;

  readline();

  for (x = 0; x < 128; x++) {
    if (!(buf[x] = replybuffer[x])) break;
  }
}
