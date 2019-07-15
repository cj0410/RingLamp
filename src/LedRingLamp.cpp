/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "c:/Users/cjeffree/Documents/01_Projects/99_Workspace/03_LED_Ring_Lamp/LedRingLamp/src/LedRingLamp.ino"
/*
 * Project LedRingLamp
 * Description:
 * Author:
 * Date:
 */

#include "Particle.h"
#include "neopixel.h"

void setup();
void loop();
void incrementColour();
int setButtonMode(String input);
#line 11 "c:/Users/cjeffree/Documents/01_Projects/99_Workspace/03_LED_Ring_Lamp/LedRingLamp/src/LedRingLamp.ino"
SYSTEM_MODE(AUTOMATIC);

// CJ: === DEFINES ===
// CJ: DIGITAL IN
#define PUSH_BUTTON_1 D1
#define PUSH_BUTTON_2 D2
#define ENCODER_1A D6
#define ENCODER_1B D7
#define ENCODER_2A D4
#define ENCODER_2B D5

#define LED_PIN D7

// CJ: LED
#define PIXEL_PIN D3
#define PIXEL_COUNT 24

#define DEFAULT_H 45
#define DEFAULT_S 100
#define DEFAULT_L 50

// === VARIABLES ===
// CJ: LED CONTROL
volatile uint32_t rgbColour;
volatile uint8_t buttonMode;

volatile uint16_t hue;
uint8_t hueInc;

volatile uint8_t sat;
uint8_t satInc;

volatile uint8_t lum;
uint8_t lumInc;

volatile uint8_t brightness;
volatile uint8_t lastBrightness;
uint8_t brightnessInc;

// CJ: INTERRUPTS
volatile bool e1aState;
volatile bool e1aLastState;

volatile bool e2aState;
volatile bool e2aLastState;

volatile bool lightOff;

uint8_t pb1DebounceTimeout;
system_tick_t pb1LastDebounceTime;

uint8_t pb2DebounceTimeout;
system_tick_t pb2LastDebounceTime;

struct hsl {
  uint16_t h;
  uint8_t s;
  uint8_t l;
} hsl;

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, 0x02);

// Prototypes for local build, ok to leave in for Build IDE
int setColour(String input);
int setBrightness(String input);
int setHue(String input);
int setSaturation(String input);
int setLuminance(String input);

// CJ: Interrupt functions
void toggleLight();
void toggleMode();
void incrementBrightness();
void incrementHue();
void incrementSat();
void incrementLum();

uint32_t Wheel(byte WheelPos);
uint32_t hsl2rgb(uint16_t ih, uint8_t is, uint8_t il);
uint8_t hsl_convert(float c, float t1, float t2);

// setup() runs once, when the device is first turned on.
void setup() {
  // Put initialization like pinMode and begin functions here.
  // CJ: Initialise the LED strip.
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // CJ: Initialise the pinout on the Particle.
  pinMode(PUSH_BUTTON_1, INPUT_PULLUP);
  attachInterrupt(PUSH_BUTTON_1, toggleLight, FALLING);

  pinMode(PUSH_BUTTON_2, INPUT_PULLUP);
  attachInterrupt(PUSH_BUTTON_2, toggleMode, FALLING);

  pinMode(ENCODER_1A, INPUT_PULLUP);
  pinMode(ENCODER_1B, INPUT_PULLUP);
  attachInterrupt(ENCODER_1A, incrementBrightness, CHANGE);

  pinMode(ENCODER_2A, INPUT_PULLUP);
  pinMode(ENCODER_2B, INPUT_PULLUP);
  attachInterrupt(ENCODER_2A, incrementColour, CHANGE);

  // CJ: Initialise light off
  lightOff = false;

  // CJ: Initialise the colour and brightness
  hsl.h = DEFAULT_H;
  hsl.s = DEFAULT_S;
  hsl.l = DEFAULT_L;
  rgbColour = hsl2rgb(hsl.h, hsl.s, hsl.l);

  // CJ: Initialise brightness
  brightnessInc = 5;
  brightness = 2;
  lastBrightness = brightness;

  // CJ: Initialise colour increment
  hue = hsl.h;
  hueInc = 5;

  sat = hsl.s;
  satInc = 5;

  lum = hsl.l;
  lumInc = 5;

  buttonMode = 1; // set hue

  // CJ: Init debounce timeout
  pb1DebounceTimeout = 250; //ms
  pb1LastDebounceTime = millis();

  pb2DebounceTimeout = 250; //ms
  pb2LastDebounceTime = millis();

  // CJ: Declare particle functions
  Particle.function("setColour", setColour);
  Particle.function("setBrightness", setBrightness);
  Particle.function("setHue", setHue);
  Particle.function("setSaturation", setSaturation);
  Particle.function("setLuminance", setLuminance);
  Particle.function("setButtonMode", setButtonMode);
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  int i;

  noInterrupts();
  for(i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, rgbColour);
  }
  strip.setBrightness(brightness);
  interrupts();
  strip.show();

  delay(50);
}

void toggleLight() {
  system_tick_t currentTime = millis();
  if(currentTime - pb1LastDebounceTime > pb1DebounceTimeout) {
    if(lightOff) {
      brightness = lastBrightness;
      lightOff = false;
    } else {
      lastBrightness = brightness;
      brightness = 0;
      lightOff = true;
    }
  }
  pb1LastDebounceTime = currentTime;
}

void toggleMode() {
  system_tick_t currentTime = millis();
  if(currentTime - pb2LastDebounceTime > pb2DebounceTimeout) {
    buttonMode++;

    if(buttonMode > 3) {
      buttonMode = 1;
    }
  }
  pb2LastDebounceTime = currentTime;
}

void incrementBrightness() {
  e1aState = digitalRead(ENCODER_1A);

  if(e1aState != e1aLastState && !lightOff) { 
    if(digitalRead(ENCODER_1B) == e1aState) {
      if(brightness < 255) {
        brightness += brightnessInc;
      }
    } else {
      if(brightness > 0) {
        brightness -= brightnessInc;
      }
    }
    brightness = min((uint8_t) 255, brightness);
    brightness = max((uint8_t) 0, brightness);
  }

  e1aLastState = e1aState;
}

void incrementColour() {
  noInterrupts();
  switch(buttonMode) { 
    case 1: 
        incrementHue();
        break; 
    case 2: 
        incrementSat();
        break; 
    case 3: 
        incrementLum();
        break; 
    default:
        break;
  }

  rgbColour = hsl2rgb(hsl.h, hsl.s, hsl.l);
  interrupts();
}

void incrementHue() {
  e2aState = digitalRead(ENCODER_2A);

  if(e2aState != e2aLastState && !lightOff) { 
    if(digitalRead(ENCODER_2B) == e2aState) {
      hue += hueInc;
      if(hue > 359) {
        hue = 0;
      }
    } else {
      hue -= hueInc;
      if(hue < 0) {
        hue = 359;
      }
    }
  }

  e2aLastState = e2aState;

  hsl.h = hue;
}

void incrementSat() {
  e2aState = digitalRead(ENCODER_2A);

  if(e2aState != e2aLastState && !lightOff) { 
    if(digitalRead(ENCODER_2B) == e2aState) {
      if(sat < 100) {
        sat += satInc;
      }
    } else {
      if(sat > 0) {
        sat -= satInc;
      }
    }
  }

  e2aLastState = e2aState;

  sat = min((uint8_t) 100, sat);
  sat = max((uint8_t) 0, sat);

  hsl.s = sat;
}

void incrementLum() {
  e2aState = digitalRead(ENCODER_2A);

  if(e2aState != e2aLastState && !lightOff) { 
    if(digitalRead(ENCODER_2B) == e2aState) {
      if(lum < 100) {
        lum += satInc;
      }
    } else {
      if(lum > 0) {
        lum -= satInc;
      }
    }
  }

  e2aLastState = e2aState;

  lum = min((uint8_t) 100, lum);
  lum = max((uint8_t) 0, lum);

  hsl.l = lum;
}

// CJ: Particle Functions
int setHue(String input) {
  hsl.h = max(min(input.toInt(), 100), 0)*359/100;

  rgbColour = hsl2rgb(hsl.h, hsl.s, hsl.l);
  return hsl.h;
}

int setSaturation(String input) {
  hsl.s = max(min(input.toInt(), 100), 0);

  rgbColour = hsl2rgb(hsl.h, hsl.s, hsl.l);
  return hsl.s;
}

int setLuminance(String input) {
  hsl.l = max(min(input.toInt(), 100), 0);

  rgbColour = hsl2rgb(hsl.h, hsl.s, hsl.l);
  return hsl.l;
}

int setButtonMode(String input) {
  buttonMode = max(min(input.toInt(), 3), 1);

  return buttonMode;
}

int setColour(String input) {
  uint8_t val = max(min(input.toInt(), 100), 0);
  byte wheelPos = val*255/100;
  rgbColour = Wheel(wheelPos);

  return wheelPos;
}

int setBrightness(String input) {
  
  noInterrupts();
  uint8_t val = max(min(input.toInt(), 100), 0);
  brightness = val*255/100;
  interrupts();

  return brightness;
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

/**
 * Map HSL color space to RGB
 * 
 * Totally borrowed from:
 * http://www.niwa.nu/2013/05/math-behind-colorspace-conversions-rgb-hsl/ 
 * 
 * Probably not the most efficient solution, but 
 * it get's the job done.
 */
uint32_t hsl2rgb(uint16_t ih, uint8_t is, uint8_t il) {

  float h, s, l, t1, t2, tr, tg, tb;
  uint8_t r, g, b;

  h = (ih % 360) / 360.0;
  s = constrain(is, 0, 100) / 100.0;
  l = constrain(il, 0, 100) / 100.0;

  if ( s == 0 ) { 
    r = g = b = 255 * l;
    return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
  } 
  
  if ( l < 0.5 ) t1 = l * (1.0 + s);
  else t1 = l + s - l * s;
  
  t2 = 2 * l - t1;
  tr = h + 1/3.0;
  tg = h;
  tb = h - 1/3.0;

  r = hsl_convert(tr, t1, t2);
  g = hsl_convert(tg, t1, t2);
  b = hsl_convert(tb, t1, t2);

  // NeoPixel packed RGB color
  return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}

/**
 * HSL Convert
 * Helper function
 */
uint8_t hsl_convert(float c, float t1, float t2) {

  if ( c < 0 ) c+=1; 
  else if ( c > 1 ) c-=1;

  if ( 6 * c < 1 ) c = t2 + ( t1 - t2 ) * 6 * c;
  else if ( 2 * c < 1 ) c = t1;
  else if ( 3 * c < 2 ) c = t2 + ( t1 - t2 ) * ( 2/3.0 - c ) * 6;
  else c = t2;
  
  return (uint8_t)(c*255); 
}