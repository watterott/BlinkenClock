/*
  BlinkenClock (https://github.com/watterott/BlinkenClock)
  ------------
  A Wall-Clock based on an Arduino and RGB-LED Stripe.
  
  Inspired by Bjoern Knorr (http://netaddict.de/blinkenlights:blinkenclock)
  
  Required Arduino Libraries
  --------------------------
  Adafruit NeoPixel: https://github.com/adafruit/Adafruit_NeoPixel
  DS1307: https://github.com/watterott/mSD-Shield/tree/master/src/libraries/DS1307
  
  Serial Commands (57600 Baud 8N1)
  --------------------------------
  d24.12.2013 -> date: dd.mm.yyyy
  t12:10      -> time: hh:mm
  c0          -> clock mode: 0...1
  o0,0,0      -> off color: r,g,b
  h255,0,0    -> hour color: r,g,b
  m0,255,0    -> minute color: r,g,b
  s0,0,255    -> second color: r,g,b
  b5          -> brightness min: 0...255
  B255        -> brightness max: 0...255
  a0          -> analog min: 0...1023
  A1023       -> analog max: 0...1023
 */

#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <DS1307.h>
#include <avr/eeprom.h>


//#define DEBUG //define for debug output

#define PIXEL_12OCLOCK 5 //first pixel (12 o'clock)

#define DATA_PIN     6  //data pin from stripe
#define LDR_PWR_PIN  A1 //        ______  READ   ___
#define LDR_READ_PIN A0 //PWR ---|VT93N1|---+---|10k|-- GND

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, DATA_PIN, NEO_GRB + NEO_KHZ800);

DS1307 rtc; //RTC connected via I2C (SDA+SCL)

typedef struct 
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
} COLOR;

typedef struct 
{
  uint8_t magic; //0xAA
  uint8_t clock_mode;
  COLOR hour;
  COLOR minute;
  COLOR second;
  COLOR off;
  uint8_t brightness_min;
  uint8_t brightness_max;
  uint16_t analog_min;
  uint16_t analog_max;
} SETTINGS;
SETTINGS settings;


void setup()
{
  //init Serial port
  Serial.begin(57600); 
  while(!Serial); //wait for serial port to connect - needed for Leonardo only
  Serial.println("Go!");

  //init strip
  strip.begin();
  strip.show();

  //init RTC
  rtc.start();

  //init LDR
  pinMode(LDR_PWR_PIN, OUTPUT);
  digitalWrite(LDR_PWR_PIN, LOW);
  pinMode(LDR_READ_PIN, INPUT);

  //read settings from EEPROM
  eeprom_read_block((void*)&settings, 0, sizeof(SETTINGS));
  if((settings.magic != 0xAA) || 
     (settings.brightness_min >= settings.brightness_max) || 
     (settings.analog_min >= settings.analog_max))
  {
    Serial.println("No settings.");
    //set default settings
    settings.magic = 0xAA;
    settings.clock_mode = 0;
    settings.hour.r = 255;
    settings.hour.g = 0;
    settings.hour.b = 0;
    settings.minute.r = 0;
    settings.minute.g = 255;
    settings.minute.b = 0;
    settings.second.r = 0;
    settings.second.g = 0;
    settings.second.b = 255;
    settings.off.r = 0;
    settings.off.g = 0;
    settings.off.b = 0;
    settings.brightness_min = 5;
    settings.brightness_max = 255;
    settings.analog_min = 0;
    settings.analog_max = 800;
    //write settings to EEPROM
    eeprom_write_block((void*)&settings, 0, sizeof(SETTINGS));
    //set RTC/clock
    rtc.set(0, 0, 12, 24, 12, 2013); //12:00:00 24.12.2013 (sec, min, hour, day, month, year)
  }
}


void set_pixel(uint8_t p, uint8_t r, uint8_t g, uint8_t b) //set pixel p (0...59) with color r,g,b
{
       if(p == 255){ p = 59; }
  else if(p ==  60){ p =  0; }

  p = p + PIXEL_12OCLOCK;

  if(p >= 60)
  {
    p -= 60;
  }

  strip.setPixelColor(p, r, g, b);
}


void set_clock1(uint8_t s, uint8_t m, uint8_t h)
{
  uint8_t i;

  if(h >= 12) { h -= 12; }

  //set second, minute, hour
  for(i=0; i<60; i++)
  {
         if(i == s)              { set_pixel(i, settings.second.r, settings.second.g, settings.second.b); }
    else if(i == ((h*5)+(m/12))) { set_pixel(i, settings.hour.r,   settings.hour.g,   settings.hour.b);   }
    else if(i == m)              { set_pixel(i, settings.minute.r, settings.minute.g, settings.minute.b); }
    else                         { set_pixel(i, settings.off.r,    settings.off.g,    settings.off.b);    }
  }

  strip.show();
}


void set_clock2(uint8_t s, uint8_t m, uint8_t h)
{
  uint8_t i;

  if(h >= 12) { h -= 12; }
  
  //clear all pixels
  for(i=0; i<60; i++)
  {
    set_pixel(i, settings.off.r, settings.off.g, settings.off.b);
  }

  //set hour
  for(i=0; i<60; i++)
  {
    if(i == ((h*5)+(m/12)))
    {
      set_pixel(i-1, settings.hour.r, settings.hour.g, settings.hour.b);
      set_pixel(i+0, settings.hour.r, settings.hour.g, settings.hour.b);
      set_pixel(i+1, settings.hour.r, settings.hour.g, settings.hour.b);
    }
  }

  //set second and minute
  for(i=0; i<60; i++)
  {
    if(i == s)
    {
      set_pixel(i, settings.second.r, settings.second.g, settings.second.b);
    }
    else if(i == m)
    {
      set_pixel(i, settings.minute.r, settings.minute.g, settings.minute.b);
    }
  }

  strip.show();
}


void test_rtc(int *s, int *m, int *h) //virtual clock for testing
{
  static uint8_t sec=0, min=0, hour=0;

  if(++sec >= 60)
  {
    sec = 0;
    if(++min >= 60)
    {
      min = 0;
      if(++hour >= 24)
      {
        hour = 0;
      }
    }
  }
  *s = sec;
  *m = min;
  *h = hour;
}


void loop()
{
  int c, i, r, g, b;
  int s, m, h, day, month, year;
  unsigned long ms;
  static unsigned int last_analog=0;
  static unsigned long ms_clock=0, ms_ldr=0;
  char tmp[32];

  //current milliseconds since power on
  ms = millis();

  //get time from RTC and set LEDs
#ifdef DEBUG
  if((ms - ms_clock) > 80) //every 80ms
  {
    ms_clock = ms;
    test_rtc(&s, &m, &h);
#else
  if((ms - ms_clock) > 500) //every 500ms
  {
    ms_clock = ms;
    rtc.get(&s, &m, &h, &day, &month, &year);
#endif

    if(settings.clock_mode == 0)
    {
      set_clock1(s, m, h);
    }
    else
    {
      set_clock2(s, m, h);
    }
  }

  //check light intensity (LDR)
  if((ms - ms_ldr) > 2000) //every 2s
  {
    ms_ldr = ms;

    digitalWrite(LDR_PWR_PIN, HIGH);
    i = (analogRead(LDR_READ_PIN) + last_analog) / 2; //make average from last and current analog reading
    last_analog = i;
    digitalWrite(LDR_PWR_PIN, LOW);
    if(i < settings.analog_min)
    {
      i = settings.brightness_min;
    }
    else if(i > settings.analog_max)
    {
      i = settings.brightness_max;
    }
    else
    {
      i = ((i-settings.analog_min) / ((settings.analog_max-settings.analog_min)/(settings.brightness_max-settings.brightness_min))) + settings.brightness_min;
    }
    strip.setBrightness(i); //0...255
    strip.show();

#ifdef DEBUG
    Serial.print("\nLDR: ");
    Serial.print(i, DEC);
    Serial.print(" (raw ");
    Serial.print(last_analog, DEC);
    Serial.print(")");
#endif
  }

  //check serial data
  if(Serial.available() > 1)
  {
    //read serial data
    for(i=0; (Serial.available()>0) && (i<31);)
    {
      tmp[i++] = Serial.read();
      tmp[i] = 0;
      delay(10); //wait to receive data
    }
    
    //get current time and date
    rtc.get(&s, &m, &h, &day, &month, &year);

    //interpret commands
    c = 0;
    switch(tmp[0])
    {
      case 'd': //date
        if(sscanf(&tmp[1], "%d%*c%d%*c%d", &day, &month, &year))
        {
          c = 1; //set rtc
#ifdef DEBUG
          Serial.print("\nd: ");
          Serial.print(day, DEC);
          Serial.print(".");
          Serial.print(month, DEC);
          Serial.print(".");
          Serial.print(year, DEC);
#endif
        }
        break;

      case 't': //time
        if(sscanf(&tmp[1], "%d%*c%d%*c%d", &h, &m, &s))
        {
          c = 1; //set rtc
#ifdef DEBUG
          Serial.print("\nt: ");
          Serial.print(h, DEC);
          Serial.print(":");
          Serial.print(m, DEC);
          Serial.print(":");
          Serial.print(s, DEC);
#endif
        }
        break;

      case 'o': //off color
      case 'h': //hour color
      case 'm': //minute color
      case 's': //second color
        if(sscanf(&tmp[1], "%d%*c%d%*c%d", &r, &g, &b))
        {
          switch(tmp[0])
          {
            case 'o':
              settings.off.r = r;
              settings.off.g = g;
              settings.off.b = b;
              break;
            case 'h':
              settings.hour.r = r;
              settings.hour.g = g;
              settings.hour.b = b;
              break;
            case 'm':
              settings.minute.r = r;
              settings.minute.g = g;
              settings.minute.b = b;
              break;
            case 's':
              settings.second.r = r;
              settings.second.g = g;
              settings.second.b = b;
              break;
          }
          c = 2; //save settings
        }
#ifdef DEBUG
        Serial.println();
        Serial.print(tmp[0]);
        Serial.print(": ");
        Serial.print(r, DEC);
        Serial.print(",");
        Serial.print(g, DEC);
        Serial.print(",");
        Serial.print(b, DEC);
#endif
        break;

      case 'c': //clock mode
      case 'b': //brightness min
      case 'B': //brightness max
      case 'a': //analog min
      case 'A': //analog max
        if(sscanf(&tmp[1], "%d", &i))
        {
          switch(tmp[0])
          {
            case 'c': settings.clock_mode     = i; break;
            case 'b': settings.brightness_min = i; break;
            case 'B': settings.brightness_max = i; break;
            case 'a': settings.analog_min     = i; break;
            case 'A': settings.analog_max     = i; break;
          }
          c = 2; //save settings
        }
#ifdef DEBUG
        Serial.println();
        Serial.print(tmp[0]);
        Serial.print(": ");
        Serial.print(i, DEC);
#endif
        break;
    }
    if(c == 1) //set RTC/clock
    {
      rtc.set(s, m, h, day, month, year);
    }
    else if(c == 2) //write settings to EEPROM
    {
      eeprom_write_block((void*)&settings, 0, sizeof(SETTINGS));
    }
    //clear serial buffer
    Serial.flush();
  }
}
