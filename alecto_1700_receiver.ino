/*
  The MIT License (MIT)

  Copyright (c) 2015 François GUILLIER <dev @ guillier . org>
  Modified 2016 by Dennis Heitmann <dennis@dennisheitmann.de>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <avr/wdt.h>

// Should be ~ 1300uS (long - high)
#define LP_MIN 1100
#define LP_MAX 1500
// Should be ~ 500uS (short - high)
#define SP_MIN 300
#define SP_MAX 700
// Should be ~ 1900uS (short - low for Alecto)
#define ASP_MIN 1600
#define ASP_MAX 2200
// Should be ~ 3800uS (long - low for Alecto)
#define ALP_MIN 3200
#define ALP_MAX 4400
// Should be ~ 9200uS (end - low for Alecto)
#define AEP_MIN 8600
#define AEP_MAX 11000

#define PIN 11 // From 433Mhz receiver

unsigned long dur; // pulse duration
byte bits[34]; // Alecto
int t_min[4] = { 1000, 1000, 1000 };
int t_max[4] = { -1000, -1000, -1000 };
int h_min[4] = { 100, 100, 100 };
int h_max[4] = { 0, 0, 0 };
int counter = 0;

void setup()
{
  pinMode(PIN, INPUT);
  Serial.begin(9600);
  Serial.println("Enabling 8s Watchdog...");
  wdt_enable(WDTO_8S);
  Serial.println("Watchdog enabled.");
  Serial.println("Start receiving...");
}

void print_values(int t, int h, int channel)
{
  Serial.println();
  Serial.print("KANAL: ");
  Serial.println(channel+1);
  Serial.print("TEMPC: ");
  if (t < 0)
  {
    Serial.print("-");
  }
  Serial.print(abs(t) / 10);
  Serial.print(".");
  Serial.print(abs(t) % 10);
  Serial.println(" C");
  Serial.print("HYGRO: ");
  Serial.print(h);
  Serial.println(" %");
  Serial.print("T_MIN: ");
  if (t_min < 0)
  {
    Serial.print("-");
  }
  Serial.print(abs(t_min[channel]) / 10);
  Serial.print(".");
  Serial.print(abs(t_min[channel]) % 10);
  Serial.println(" C");
  Serial.print("T_MAX: ");
  if (t_max < 0)
  {
    Serial.print("-");
  }
  Serial.print(abs(t_max[channel]) / 10);
  Serial.print(".");
  Serial.print(abs(t_max[channel]) % 10);
  Serial.println(" C");
  Serial.print("H_MIN: ");
  Serial.print(h_min[channel]);
  Serial.println(" %");
  Serial.print("H_MAX: ");
  Serial.print(h_max[channel]);
  Serial.println(" %");
  Serial.print("MILLI: ");
  Serial.println(millis() / 1000);
}

void print_batt()
{
  Serial.println("BATTERIE WECHSELN!");
}

void temp_hygr_min_max(int t, int h, int channel)
{
  if (t < t_min[channel]) t_min[channel] = t;
  if (h < h_min[channel]) h_min[channel] = h;
  if (t > t_max[channel]) t_max[channel] = t;
  if (h > h_max[channel]) h_max[channel] = h;
}

void alecto_ws1700()
{
  int p;
  for (p = 0; p < 100; p++)
  {
    dur = pulseIn(PIN, LOW);
    if ((dur > AEP_MIN) && (dur < AEP_MAX))
      break;
  }
  if (p == 100)
    return;

  byte v = 0;
  for (int i = 0; i < 4; i++)
  {
    v <<= 2;
    dur = pulseIn(PIN, LOW);
    if ((dur > ASP_MIN) && (dur < ASP_MAX))
    {
      v |= 2;
    } else if ((dur > ALP_MIN) && (dur < ALP_MAX))
    {
      v |= 3;
    }
  }

  if (v != 187) // 10111011b = 187d
    return;

  for (int i = 0; i < 32; i++)
  {
    dur = pulseIn(PIN, LOW);
    if ((dur > ASP_MIN) && (dur < ASP_MAX))
    {
      bits[i] = 0;
    } else if ((dur > ALP_MIN) && (dur < ALP_MAX))
    {
      bits[i] = 1;
    }
  }

  dur = pulseIn(PIN, LOW);
  if ((dur < AEP_MIN) || (dur > AEP_MAX))
    return;

  int h = 0;
  int t = 0;
  for (int i = 0; i < 32; i++)
  {
    if (i > 11)
    {
      if (i > 23)
      {
        h <<= 1;
        h |= bits[i];
      } else
      {
        t <<= 1;
        t |= bits[i];
      }
    }
  }

  if (h == 0)
    return;

  if (t > 3840)
    t -= 4096;

  int channel = (bits[10] << 1) + bits[11];
  if (channel == 3)  // 00=CH1, 01=CH2, 10=CH3, 11=Error
    return;

  if (bits[8]) {
    temp_hygr_min_max(t, h, channel);
    print_values(t, h, channel);
  } else {
    temp_hygr_min_max(t, h, channel);
    print_values(t, h, channel);
    print_batt();
  }
  wdt_reset();
  delay(500); // Signal is repeated by sensor. If values are OK then additional transmissions can be ignored.
  counter = 0;
}

void loop()
{
  int v = 0;
  do
  {
    v <<= 1;
    dur = pulseIn(PIN, HIGH);
    if ((dur > SP_MIN) && (dur < SP_MAX))
      v = v + 1;  // Short Pulse => 1
    else if ((dur <= LP_MIN) || (dur >= LP_MAX))
      v = 0;  // Not Long Pulse => Reset
  } while (v <= 8);

  // At least 8 short pulses => Alecto detected
  if (v >= 8)
  {
    alecto_ws1700();
  }
  if (millis() % 1000 < 100)
  {
    counter = counter + 1;
    Serial.println(counter);
  }
  Serial.print(".");
  wdt_reset();
}
