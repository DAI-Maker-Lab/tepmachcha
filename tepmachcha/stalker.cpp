// Stalker V3/3.1 functions

#include "tepmachcha.h"

// Non-blocking delay function
void wait (uint32_t period)
{
  uint32_t waitend = millis() + period;
  while (millis() <= waitend)
  {
    Serial.flush();
  }
}


// Get battery reading on ADC pin BATT, in mV
//
// VBAT is divided by a 10k/2k voltage divider (ie /6) to BATT, which is then
// measured relative to AREF, on a scale of 0-1023, so
//
//   mV = BATT * (AREF * ( (10+2)/2 ) / 1.023)
//
// We use integer math to avoid including ~1.2K of FP library
// AREF ADC->mV factor   approx integer fraction
// ---  --------------   -----------------------
// 1.1      6.45          1651/256 (~103/16)
// 3.3     19.35          4955/256 (~155/8)
//
// Note, if we ONLY needed to compare the ADC reading to a FIXED voltage,
// we'd simplify by letting the compiler calculate the equivalent 0-1023 value
// at compile time, and compare against that directly.
// eg to check voltage is < 3500mV:
//   if (analogueRead(BATT) < 3500/19.35) {...}
//
// Also the calculations don't need to be too accurate, because the ADC
// itself can be be quite innacurate (up to 10% with internal AREF!)
//
uint16_t batteryRead(void)
{
  uint32_t mV = 0;

  analogReference(DEFAULT); // stalkerv3: DEFAULT=3.3V, INTERNAL=1.1V, EXTERNAL=3.3V
  analogRead(BATT);         // must read once after changing reference

  for (uint8_t i = 0; i < 155; i++)
  {
    mV += analogRead(BATT);
  }
  return mV/8;
}


// Solar panel charging status
//
// Detect status of the CN3065 charge-controller 'charging' and 'done' pins,
// which have a voltage divider between vbatt and status pins:
//
//  vbatt----10M----+-----+---1M----DONE
//                  |     |
//             SOLAR(A6)  +---2M----CHARGING
//
// SLEEPING: Both pins are gnd when solar voltage is < battery voltage +40mv
//
// AREF      1.1    3.3
// =====    ====   ====
// ERROR      0-     0-
// DONE     350-   115-  ( vbatt(4.2v) / (10M + 1M)/1M ) => 0.38v
// CHARGING 550-   180-  ( vbatt(3.6v+) / (10M + 2M)/2M ) => 0.6v
// SLEEPING 900+   220+  ( vbatt ) => 3.6v -> 4.2v
//
boolean solarCharging(void)
{
    uint16_t solar;

    // Get an average of 64 readings (fits uint16)
    for (uint8_t i = 0; i < 64; i++) 
      solar += analogRead(SOLAR);
    solar = solar / 64;

    Serial.print (F("solar analog: "));
    Serial.println (solar);
    if ( solar > 180 && solar <= 220 )    // charging, 3.3v analogue ref
    {
       return true;
    }
    return false;
}

// read temperature of the atmega328 itself
int16_t internalTemp(void)
{
  uint16_t wADC = 0;
  int16_t t;

  // The internal temperature has to be used
  // with the internal reference of 1.1V.
  // Channel 8 can not be selected with
  // the analogRead function yet.

  // Set the internal reference and mux.
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
  ADCSRA |= _BV(ADEN);  // enable the ADC

  delay(20);            // wait for voltages to become stable.

  for (uint8_t i = 0 ; i < 64 ; i++)
  {
    ADCSRA |= _BV(ADSC);  // Start the ADC

    // wait for conversion to complete
    while (bit_is_set(ADCSRA,ADSC));

    // Reading register "ADCW" takes care of how to read ADCL and ADCH.
    wADC += ADCW;
  }

  // offset ~324.31, scale by 1/1.22 to give C
  //wADC = wADC / 64;
  //t = (wADC - 324.31 ) / 1.22;
  return (wADC - (324*64) ) / 78;    // 64/78 ~= 1/1.22
}
