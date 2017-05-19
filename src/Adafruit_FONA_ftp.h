#include <Adafruit_FONA.h>

class Adafruit_FONA_ftp : public Adafruit_FONA
{
  public:
    Adafruit_FONA_ftp(int8_t);
    void showline(char *);
};
