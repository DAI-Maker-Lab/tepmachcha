// Maxbotix Sonar

#include "tepmachcha.h"

// insertion sort
void sort(int16_t *a, uint8_t n)
{
  for (uint8_t i = 1; i < n; i++)
  {
    for (uint8_t j = i; j > 0 && a[j] < a[j-1]; j--)
    {
      int16_t tmp = a[j-1];
      a[j-1] = a[j];
      a[j] = tmp;
    }
  }
}


// Calculate mode, or median of sorted samples
int16_t mode (int16_t *sample, uint8_t n)
{
    int16_t mode;
    uint8_t count = 1;
    uint8_t max_count = 1;

    for (int i = 1; i < n; i++)
    {
      if (sample[i] == sample[i - 1])
        count++;
      else
        count = 1;

      if (count > max_count)  // current sequence is the longest
      {
          max_count = count;
          mode = sample[i];
      }
      else if (count == max_count)
      {
        mode = sample[(n/2)];  // use median if no sequence > 1 or bimodal
      }
    }

    return mode;
}


#define SAMPLES 11
int16_t takeReading (void)
{
    int16_t sample[SAMPLES];

    digitalWrite (RANGE, HIGH);           //  sonar on
    wait (1000);

    // read samples into array
    for (uint8_t sampleCount = 0; sampleCount < SAMPLES; sampleCount++)
    {
      sample[sampleCount] = pulseIn (PING, HIGH);
      Serial.print (F("Sample "));
      Serial.print (sampleCount);
      Serial.print (F(": "));
      Serial.println (sample[sampleCount]);
      wait (50);
    }

    // sort the samples
		sort (sample, SAMPLES);

    // take the mode, or median
    int16_t sampleMode = mode (sample, SAMPLES);

    // offset by programmed stream-bed height
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
