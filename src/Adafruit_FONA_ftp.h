#include <Adafruit_FONA.h>

class Adafruit_FONA_ftp : public Adafruit_FONA
{
  public:
    Adafruit_FONA_ftp(int8_t);
    /*
    boolean Adafruit_FONA_ftp::sendCheckOK(FONAFlashStringPtr, uint16_t);
    boolean Adafruit_FONA_ftp::sendCheckOK(char*, uint16_t);
    */
};
