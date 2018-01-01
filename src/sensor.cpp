#include "tepmachcha.h"

// insertion sort
void sort(int16_t *a, uint8_t n) {
  for (uint8_t i = 1; i < n; i++) {

    for (uint8_t j = i; j > 0 && a[j] < a[j-1]; j--) {
      int16_t tmp = a[j-1];
      a[j-1] = a[j];
      a[j] = tmp;
    }
  }
}


// Calculate the mode of an array of readings
// FIXED From http://playground.arduino.cc/Main/MaxSonar
int16_t mode (int16_t *a, uint8_t n)    
{
		uint8_t i = 0;
		uint8_t count = 0;
		uint8_t maxCount = 0;
		int16_t mode = 0;
		boolean bimodal;

		sort (a, n);

		while(i < (n - 1))
		{
      count = 0;
      while(a[i] == a[i + 1])
      {
        count++;
        i++;
      }

      if(count > maxCount)
      {
        mode = a[i];
        maxCount = count;
        bimodal = 0;
      }
      else if(count == 0)
      {
        i++;
      }
      else if(count == maxCount)     // the dataset has 2 or more modes
      {
        bimodal = 1;
      }
    }
				
    if(mode == 0 || bimodal == 1) // Return the median if no mode
    {
      mode = a[(n / 2)];
    }

    return mode;
}


#define SAMPLES 13
int16_t takeReading (void)
{
		int16_t sample[SAMPLES];
		
    digitalWrite (RANGE, HIGH);           //  sonar on
    wait (1000);

		for (uint8_t sampleCount = 0; sampleCount < SAMPLES; sampleCount++)
		{
				sample[sampleCount] = pulseIn (PING, HIGH);
				Serial.print (F("Sample "));
				Serial.print (sampleCount);
				Serial.print (F(": "));
				Serial.println (sample[sampleCount]);
				wait (50);
		}

		int16_t sampleMode = mode (sample, SAMPLES);

		int16_t streamHeight = (SENSOR_HEIGHT - (sampleMode / 10)); //  1 µs pulse = 1mm distance

		Serial.print (F("Surface distance from sensor is "));
		Serial.print (sampleMode);
		Serial.println (F("mm."));
		Serial.print (F("Calculated surface height is "));
		Serial.print (streamHeight);
		Serial.println (F("cm."));

    digitalWrite (RANGE, LOW);           //  sonar off
		return streamHeight;
}
