/*
 * Project LedRingLamp
 * Description:
 * Author:
 * Date:
 */

#include "Particle.h"
#include "neopixel.h"
#include "string.h"

// CJ: === Global State (you don't need to change this!) ===
TCPClient TheClient;

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
#define PIXEL_COUNT 68 * 2

#define DEFAULT_H 20
#define DEFAULT_S 100
#define DEFAULT_L 50

// === VARIABLES ===
// CJ: LED CONTROL
volatile uint32_t rgbColour;
volatile uint8_t buttonMode;

volatile int16_t hueControl;
volatile int16_t hueBed;
volatile int16_t hueSaved;
String hueCurr;
uint8_t hueInc;

volatile int8_t satControl;
volatile int8_t satSaved;
uint8_t satInc;

volatile int8_t lumControl;
volatile int8_t lumSaved;
uint8_t lumInc;

volatile int8_t disControl;
uint8_t disInc;

volatile int8_t modeControl;
uint8_t modeInc;

volatile int16_t brightnessControl;
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

struct hsl
{
  uint16_t h;
  uint8_t s;
  uint8_t l;
} hslVal;

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, 0x02);

// Prototypes for local build, ok to leave in for Build IDE
int setBedState(String input);

// CJ: Interrupt functions
void toggleLight();
void toggleMode();

void incrementBri();
void incrementHue();
void incrementSat();
void incrementLum();
void incrementDisable();

void lightMode();

void initDisableLEDArray();

uint32_t Wheel(byte WheelPos);
uint32_t hsl2rgb(uint16_t ih, uint8_t is, uint8_t il);
hsl rgb2hsl(uint8_t ir, uint8_t ig, uint8_t ib);
uint8_t hsl_convert(float c, float t1, float t2);

int randn(int minVal, int maxVal);

int disableArray[PIXEL_COUNT] = {0};
int disableArrayIdx[PIXEL_COUNT] = {0};

double disArrayMod = 0;
int disCon = 0;

// setup() runs once, when the device is first turned on.
void setup()
{
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
  attachInterrupt(ENCODER_1A, incrementBri, CHANGE);

  pinMode(ENCODER_2A, INPUT_PULLUP);
  pinMode(ENCODER_2B, INPUT_PULLUP);
  attachInterrupt(ENCODER_2A, incrementColour, CHANGE);

  // CJ: Initialise light off
  lightOff = false;

  // CJ: Initialise the colour and brightness
  hueBed = DEFAULT_H;

  hslVal.h = DEFAULT_H;
  hslVal.s = DEFAULT_S;
  hslVal.l = DEFAULT_L;
  rgbColour = hsl2rgb(hslVal.h, hslVal.s, hslVal.l);

  // CJ: Initialise brightness
  brightnessInc = 5;
  brightnessControl = 80; // 0-255
  brightness = (uint8_t)brightnessControl;
  lastBrightness = brightness;

  // CJ: Initialise colour increment
  hueControl = hslVal.h;
  hueSaved = hslVal.h;
  hueInc = 2;

  satControl = hslVal.s;
  satSaved = hslVal.s;
  satInc = 2;

  lumControl = hslVal.l;
  lumSaved = hslVal.l;
  lumInc = 2;

  modeControl = 0;
  modeInc = 1;

  disControl = 0;
  disInc = 1;

  buttonMode = 1; // set hue

  // CJ: Init debounce timeout
  pb1DebounceTimeout = 250; // ms
  pb1LastDebounceTime = millis();

  pb2DebounceTimeout = 250; // ms
  pb2LastDebounceTime = millis();

  // CJ: Init disable array
  initDisableLEDArray();

  // CJ: Declare particle functions
  Particle.function("setBedState", setBedState);

  // CJ: Declare particle variables
  // Particle.variable("disArrayMod", disArrayMod);
  // Particle.variable("disControl", disCon);
  Particle.variable("hueCurr", hueCurr);
}

// loop() runs over and over again, as quickly as it can execute.
void loop()
{
  noInterrupts();

  for (int i = 0; i < strip.numPixels(); i++)
  {
    if (buttonMode == 3)
    {
      hslVal.h = hueBed;
    }

    if (buttonMode == 2 || buttonMode == 3)
    {
      disCon = (int)disControl;
      disArrayMod = disableArrayIdx[i] % (strip.numPixels() / 4 + 1);

      if (disArrayMod < disControl)
      {
        hslVal.l = 0;
      }
      else
      {
        hslVal.l = DEFAULT_L;
      }
    }

    rgbColour = hsl2rgb(hslVal.h, hslVal.s, hslVal.l);

    strip.setPixelColor(disableArray[i], rgbColour);
  }

  strip.setBrightness(brightness);
  strip.show();

  hueCurr = String(hueControl);

  interrupts();

  delay(100);
}

void toggleLight()
{
  system_tick_t currentTime = millis();
  if (currentTime - pb1LastDebounceTime > pb1DebounceTimeout)
  {
    if (lightOff)
    {
      brightness = lastBrightness;
      lightOff = false;
    }
    else
    {
      lastBrightness = brightness;
      brightness = 0;
      lightOff = true;
    }
  }
  pb1LastDebounceTime = currentTime;
}

void toggleMode()
{
  system_tick_t currentTime = millis();
  if (currentTime - pb2LastDebounceTime > pb2DebounceTimeout)
  {
    buttonMode++;

    if (buttonMode > 3)
    {
      buttonMode = 1;
    }
  }
  pb2LastDebounceTime = currentTime;
}

void incrementBri()
{
  e1aState = digitalRead(ENCODER_1A);

  if (e1aState != e1aLastState && !lightOff)
  {
    if (digitalRead(ENCODER_1B) == e1aState)
    {
      brightnessControl += brightnessInc;
    }
    else
    {
      brightnessControl -= brightnessInc;
    }

    brightnessControl = max(10, min(255, brightnessControl));
    brightness = (uint8_t)brightnessControl;
    e1aLastState = e1aState;
  }

  // e1aLastState = e1aState;
}

void incrementColour()
{
  noInterrupts();
  switch (buttonMode)
  {
  case 1:
    incrementHue();
    break;
  case 2:
    incrementDisable();
    break;
  case 3:
    incrementDisable();
    // incrementSat();
    break;
  // case 4:
  //   incrementLum();
  //   break;
  default:
    break;
  }

  rgbColour = hsl2rgb(hslVal.h, hslVal.s, hslVal.l);
  interrupts();
}

void incrementHue()
{
  e2aState = digitalRead(ENCODER_2A);

  if (e2aState != e2aLastState && !lightOff)
  {
    if (digitalRead(ENCODER_2B) == e2aState)
    {
      hueControl += hueInc;
      if (hueControl > 359)
      {
        hueControl = 0;
      }
    }
    else
    {
      hueControl -= hueInc;
      if (hueControl < 0)
      {
        hueControl = 359;
      }
    }

    hueControl = max(0, min(359, hueControl));
    hslVal.h = (uint16_t)hueControl;
    e2aLastState = e2aState;
  }

  // e2aLastState = e2aState;
}

void incrementSat()
{
  e2aState = digitalRead(ENCODER_2A);

  if (e2aState != e2aLastState && !lightOff)
  {
    if (digitalRead(ENCODER_2B) == e2aState)
    {
      satControl += satInc;
    }
    else
    {
      satControl -= satInc;
    }

    satControl = max(0, min(100, satControl));
    hslVal.s = (uint8_t)satControl;
    e2aLastState = e2aState;
  }

  // e2aLastState = e2aState;
}

void incrementLum()
{
  e2aState = digitalRead(ENCODER_2A);

  if (e2aState != e2aLastState && !lightOff)
  {
    if (digitalRead(ENCODER_2B) == e2aState)
    {
      lumControl += satInc;
    }
    else
    {
      lumControl -= satInc;
    }

    lumControl = max(0, min(100, lumControl));
    hslVal.l = (uint8_t)lumControl;
    e2aLastState = e2aState;
  }

  // e2aLastState = e2aState;
}

void incrementDisable()
{
  e2aState = digitalRead(ENCODER_2A);

  if (e2aState != e2aLastState && !lightOff)
  {
    if (digitalRead(ENCODER_2B) == e2aState)
    {
      disControl += disInc;
    }
    else
    {
      disControl -= disInc;
    }

    disControl = max(0, min(34, disControl));
    e2aLastState = e2aState;
  }

  // e2aLastState = e2aState;
}

void lightMode()
{
  e2aState = digitalRead(ENCODER_2A);

  if (e2aState != e2aLastState && !lightOff)
  {
    if (digitalRead(ENCODER_2B) == e2aState)
    {
      modeControl += modeInc;
    }
    else
    {
      modeControl -= modeInc;
    }

    modeControl = max(0, min(1, modeControl));
    e2aLastState = e2aState;
  }

  // e2aLastState = e2aState;
}

void initDisableLEDArray()
{
  for (int j = 0; j < strip.numPixels() / 4; j++)
  {
    int offset = 1;
    disableArray[4 * j] = (strip.numPixels() / 4) - (j + 1 - offset);
    disableArray[4 * j + 1] = (strip.numPixels() / 4) + (j + offset);
    disableArray[4 * j + 2] = (3 * strip.numPixels() / 4) + (j - offset);
    disableArray[4 * j + 3] = (3 * strip.numPixels() / 4) - (j + 1 + offset);

    disableArrayIdx[4 * j] = j;
    disableArrayIdx[4 * j + 1] = j;
    disableArrayIdx[4 * j + 2] = j;
    disableArrayIdx[4 * j + 3] = j;
  }
}

// CJ: Particle Functions
int setBedState(String input)
{
  noInterrupts();

  switch (input.toInt())
  {
    case 0: // STAY IN ROOM
    {
      hueBed = DEFAULT_H;
      break;
    }
    case 1: // CAN COME TO OUR ROOM
    {
      hueBed = 270;
      break;
    }
    case 2: // BIG GIRL SLEEP SUCCESS!
    {
      hueBed = 350;
      break;
    }
  }

  interrupts();

  return 0;
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos)
{
  if (WheelPos < 85)
  {
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
  else if (WheelPos < 170)
  {
    WheelPos -= 85;
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  else
  {
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
uint32_t hsl2rgb(uint16_t ih, uint8_t is, uint8_t il)
{

  float h, s, l, t1, t2, tr, tg, tb;
  uint8_t r, g, b;

  h = (ih % 360) / 360.0;
  s = constrain(is, 0, 100) / 100.0;
  l = constrain(il, 0, 100) / 100.0;

  if (s == 0)
  {
    r = g = b = 255 * l;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }

  if (l < 0.5)
    t1 = l * (1.0 + s);
  else
    t1 = l + s - l * s;

  t2 = 2 * l - t1;
  tr = h + 1 / 3.0;
  tg = h;
  tb = h - 1 / 3.0;

  r = hsl_convert(tr, t1, t2);
  g = hsl_convert(tg, t1, t2);
  b = hsl_convert(tb, t1, t2);

  // NeoPixel packed RGB color
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

/**
 * Map RGB color space to HSL
 *
 * Totally borrowed from:
 * https://www.programmingalgorithms.com/algorithm/rgb-to-hsl?lang=C%2B%2B
 *
 */
hsl rgb2hsl(uint8_t ir, uint8_t ig, uint8_t ib)
{
  hsl tempHsl;

  float r = (ir / 255.0f);
  float g = (ig / 255.0f);
  float b = (ib / 255.0f);

  float vmin = min(min(r, g), b);
  float vmax = max(max(r, g), b);
  float delta = vmax - vmin;

  tempHsl.l = (int)((vmax + vmin) / 2) * 100;

  if (delta == 0.0f)
  {
    tempHsl.h = 0;
    tempHsl.s = 0;
  }
  else
  {
    tempHsl.s = (int)(tempHsl.l <= 50) ? (delta / (vmax + vmin)) * 100 : (delta / (2 - vmax - vmin)) * 100;

    float hue;

    if (r == vmax)
    {
      hue = ((g - b) / 6) / delta;
    }
    else if (g == vmax)
    {
      hue = (1.0f / 3) + ((b - r) / 6) / delta;
    }
    else
    {
      hue = (2.0f / 3) + ((r - g) / 6) / delta;
    }

    if (hue < 0)
      hue += 1;
    if (hue > 1)
      hue -= 1;

    tempHsl.h = (int)(hue * 360);
  }

  return tempHsl;
}

/**
 * HSL Convert
 * Helper function
 */
uint8_t hsl_convert(float c, float t1, float t2)
{

  if (c < 0)
    c += 1;
  else if (c > 1)
    c -= 1;

  if (6 * c < 1)
    c = t2 + (t1 - t2) * 6 * c;
  else if (2 * c < 1)
    c = t1;
  else if (3 * c < 2)
    c = t2 + (t1 - t2) * (2 / 3.0 - c) * 6;
  else
    c = t2;

  return (uint8_t)(c * 255);
}

int randn(int minVal, int maxVal)
{
  // int rand(void); included by default from newlib
  return rand() % (maxVal - minVal + 1) + minVal;
}