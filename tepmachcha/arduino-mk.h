/*
 * arduino-mk.h
 * This is required when compiling with arduino-mk, and gets included with
 * a -I addtion to gcc command line in the makefile
 *
 * Generate this file with cproto (first copy .ino to .c, then cproto -s file.c)
 * there may be a new arduino-preproc command that uses ctags - investigate
 */
#include <Arduino.h>
#include "secrets.h" // override config.h template defs

// tepmachcha
void setup(void);
void loop(void);
void upload(void);
boolean dmisPost(int, boolean,  uint16_t);
boolean ews1294Post (int, boolean, uint16_t);

// sonar
int16_t takeReading(void);

// stalker
uint16_t batteryRead(void);
boolean solarCharging(void);
void wait (uint32_t);

// fona
void fonaFlush(void);
char fonaRead(void);
boolean fonaOn(void);
boolean fonaPowerOn(void);
boolean fonaSerialOn(void);
boolean fonaGSMOn(void);
boolean fonaGPRSOn(void);
void fonaOff(void);
void fonaGPRSOff(void);
uint16_t fonaBattery(void);
void smsDeleteAll(void);
void smsCheck(void);
void checkSMS(void);
boolean fonaSendCheckOK(const __FlashStringHelper*);

// rtc
void clockSet(void);
void clockSet3(void);

// XBee
void XBeeOn();
void XBeeOnMessage(char *);
void XBee(void);

// ota
uint32_t crc_update(uint32_t, uint8_t);
void xtea(uint32_t v[2]);
boolean fileInit(void);
boolean fileOpen(uint8_t);
boolean fileOpenWrite(void);
boolean fileOpenRead(void);
boolean fileClose(void);
uint32_t fileCRC(uint32_t);
boolean ftpGet(void);
boolean fonaFileCopy(uint16_t len);
void reflash (void);
void eepromWrite(void);
boolean firmwareGet();
extern char file_name[];
extern uint16_t file_size;
extern uint8_t error;

// test
int freeRam(void);
void ram(void);
void test(void);
void testSMS(void);
