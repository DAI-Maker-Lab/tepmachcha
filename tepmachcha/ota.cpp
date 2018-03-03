#include "tepmachcha.h"

uint8_t error;
const uint8_t CHIP_SELECT = SS;  // SD chip select pin (SS = 10)
SdCard card;
Fat16 file;
char file_name[13];              // 8.3
uint16_t file_size;


boolean fileInit(void)
{
  digitalWrite (SD_POWER, LOW);        //  SD card on
  wait (1000);

  // init sd card
  if (!card.begin(CHIP_SELECT))
  {
    Serial.print(F("failed card.begin ")); Serial.println(card.errorCode);
	  return false;
  }
  
  // initialize FAT16 volume
  // if (!Fat16::init(&card)) // JACK
  if (!Fat16::init(&card, 1))
  {
	  Serial.println(F("Fat16::init failed"));
	  return false;
  }

  return true;
}


boolean fileOpen(uint8_t mode)
{
  Serial.print(F("opening file (mode 0x"));
  Serial.print(mode, HEX);
  Serial.print(F("):"));
  Serial.println(file_name);
  return file.open(file_name, mode);
}

boolean fileOpenWrite(void) { return(fileOpen(O_CREAT | O_WRITE | O_TRUNC)); }

boolean fileOpenRead(void)  { return(fileOpen(O_READ)); }

boolean fileClose(void)
{
  file.close();
  digitalWrite (SD_POWER, HIGH);        //  SD card off
}


uint32_t fileCRC(uint32_t len)
{
  char c;
  uint32_t crc = ~0L;

  for(int i = 0; i < len; i++)
  {
    c = file.read();
    crc = crc_update(crc, c);
  }
  return ~crc;
}


// write firmware filename to EEPROM and toggle boot-from-SDcard flag at EEPROM[E2END]
void eepromWrite(void)
{
  uint8_t x;

  for (x = 0; x < 12 && file_name[x] != 0; x++)
  {
    //if (EEPROM.read( (EEPROM_FILENAME_ADDR - x) != file_name[x] ))
      EEPROM.write( (EEPROM_FILENAME_ADDR - x), file_name[x] );
  }
  EEPROM.write(E2END, 0); // 0 triggers an attempt to flash from SD card on power-on or reset
}

// serial flow control on
#define XON  17
void xon(void)
{
  //digitalWrite(FONA_RTS, HIGH);
}

// serial flow control off
#define XOFF 19
void xoff(void)
{
  //digitalWrite(FONA_RTS, LOW);
}


//const static uint32_t key[4] = {
const static PROGMEM uint32_t key[4] = {
  KEY1, KEY2, KEY3, KEY4
};

// decrypt 8 bytes
void xtea(uint32_t v[2])
{
    uint8_t i;
    uint32_t v0=v[0], v1=v[1], delta=0x9E3779B9, sum=delta * 32;

    for (i=0; i < 32; i++)
    {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + pgm_read_dword_near(&key[(sum>>11) & 3]));
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + pgm_read_dword_near(&key[sum & 3]));
    }
    v[0]=v0; v[1]=v1;
}


boolean fileDecrypt() {
  byte c;
  uint8_t i;
  uint32_t crc;
  union xtea_block_t
  {
    char chr[8];
    uint32_t v[2];
  };
  xtea_block_t x;

  crc = ~0L;

      c = c ^ x.chr[i];
      if (i++ == 8)
      {
        i = 0;
        xtea(x.v);
      }
      crc = crc_update(crc, c);
}


const static PROGMEM uint32_t crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};
uint32_t crc_update(uint32_t crc, byte data)
{
    byte tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = pgm_read_dword_near(&crc_table[tbl_idx & 0x0f]) ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = pgm_read_dword_near(&crc_table[tbl_idx & 0x0f]) ^ (crc >> 4);
    return crc;
}


// Read len bytes from fona serial and write to file buffer
uint16_t fonaReadBlock(uint16_t len)
{
  uint16_t n = 0;
  uint32_t timeout = millis() + 2500;

  xon();
  while (n < len && millis() < timeout)
  {
    if (fona.available())
    {
      if (!file.write(fona.read())) break;
      n++;
    }
  }
  xoff();

  return n;
}


boolean fonaFileSize()
{
  fona.sendCheckReply (F("AT+FSFLSIZE=C:\\User\\ftp\\tmp.bin"), OK);
}


#define BLOCK_ATTEMPTS 3
#define BLOCK_SIZE 512
boolean fonaFileCopy(uint16_t len)
{
  uint32_t address = 0;
  uint16_t size = BLOCK_SIZE;
  uint16_t n;
  uint8_t retry_attempts = BLOCK_ATTEMPTS;
  DEBUG_RAM

  while (address < len)
  {
    fonaFlush();  // flush any notifications

    if (!retry_attempts--) return false;

    file.seekSet(address);   // rewind to beginning of block
    Serial.print(address); Serial.print(':');

    fona.print(F("AT+FSREAD=C:\\User\\ftp\\tmp.bin,1,"));
    fona.print(size);
    fona.print(',');
    fona.println(address);

    // fona returns \r\n from submitting command
    if (fonaRead() != '\r') continue;  // start again if it's something else
    if (fonaRead() != '\n') continue;

    n = fonaReadBlock(size);
    Serial.print(n);

    if (n == size && fona.expectReply(OK))
    {
      if (!file.sync())
      {
        return false;
      }
    } else { // didn't get a full block, or missing OK status response
      Serial.println();
      continue;
    }

    // success, reset attempts counter, move to next block
    retry_attempts = BLOCK_ATTEMPTS;
    address += size;
    if (address + size > len)  // CHECKME should by >= ??
    {
      size = len - address;    // CHECKME len-address, or len-address-1 ??
    }
  }
  return true;
}


void ftpEnd(void)
{
  fona.sendCheckReply (F("AT+FTPQUIT"), OK);
  fona.sendCheckReply (F("AT+FTPSHUT"), OK);
}


/*
 * Fetch firmware from FTP server to FONA's internal filesystem
 */
boolean ftpGet(void)
{
  // configure FTP
  fona.sendCheckReply (F("AT+SSLOPT=0,1"), OK); // 0,x dont check cert, 1,x client auth
  fona.sendCheckReply (F("AT+FTPSSL=0"), OK);   // 0 ftp, 1 implicit (port is an FTPS port), 2 explicit
  fona.sendCheckReply (F("AT+FTPCID=1"), OK);
  fona.sendCheckReply (F("AT+FTPMODE=1"), OK);     // 0 ACTIVE, 1 PASV
  fona.sendCheckReply (F("AT+FTPTYPE=\"I\""), OK); // "I" binary, "A" ascii
  fona.sendCheckReply (F("AT+FTPSERV=\"" FTPSERVER "\""), OK);
  fona.sendCheckReply (F("AT+FTPUN=\"" FTPUSER "\""), OK);
  fona.sendCheckReply (F("AT+FTPPW=\"" FTPPW "\""), OK);
  fona.sendCheckReply (F("AT+FTPGETPATH=\"" FTPPATH "\""), OK);

  // remote filename
  Serial.print(F("AT+FTPGETNAME=\""));
  Serial.print(file_name);
  Serial.println(F("\""));

  fona.print(F("AT+FTPGETNAME=\""));
  fona.print(file_name);
  fona.println(F("\""));
  fona.expectReply (OK);

  // local file path on fona
  fona.sendCheckReply (F("AT+FSDEL=C:\\User\\ftp\\tmp.bin"), OK); // delete previous download file

  // start the download to local file
  if ( !fona.sendCheckReply (F("AT+FTPGETTOFS=0,\"tmp.bin\""), OK))
  {
    return false;
  }

  // Wait for download complete; FTPGETOFS status 0
  uint32_t timeout = millis() + 90000;
  while( !fona.sendCheckReply (F("AT+FTPGETTOFS?"), F("+FTPGETTOFS: 0")) ) {
    delay(2000);
    if (millis() > timeout)
    {
      ftpEnd();
      return false;
    }
  }
  ftpEnd();

  // Check the file exists
  if (fona.sendCheckReply (F("AT+FSFLSIZE=C:\\User\\ftp\\tmp.bin"), F("ERROR")))
  {
    return false;
  }

  return true;
}


boolean firmwareGet(void)
{ 
  // Ensure GPRS is on
  if (fona.GPRSstate() != 1)
  {
    Serial.println(F("no GPRS"));
    return false;
  }

  Serial.println(F("Fetching FW"));

  if ( ftpGet() )
  {
    for (uint8_t tries=3 ;tries;tries--)
    {
      if ( fileInit() && fileOpenWrite() )
      {
        if (fonaFileCopy(file_size))
        {
          fileClose();
          return true;
        } else {
          error = 3;
        }
      } else {
        error = 2;
      }
    }
  } else {
    error = 1;
  }
  fileClose();
  Serial.println(F("fona copy failed"));
  return false;
}


void reflash (void) {
    Serial.println(F("updating eeprom...."));
    eepromWrite();

    Serial.println(F("reflashing...."));
    delay(100);

    //SP=RAMEND;
    flash_firmware(file_name);
}
