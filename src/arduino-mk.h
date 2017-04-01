/*
 * arduino-mk.h
 * generate this file with cproto (first copy .ino to .c, then cproto -s file.c)
 * should be a new arduino-preproc command that uses ctags
 */
#include <Arduino.h>

void setup(void);
void loop(void);
void upload(int);
void wait(unsigned long);
void clockSet(void);
void flushFona(void);
boolean fonaOn(void);
void fonaOff(void);
int takeReading(void);
int mode(int*, int);
void checkSMS(void);
boolean sendReading (int);
boolean validate (int);
boolean ivr (const char*);
boolean getFirmware ();
