/*
 * Project LedRingLamp
 * Description:
 * Author:
 * Date:
 */

#include "Particle.h"
#include "neopixel.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h" 
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h" 
#include "Adafruit_MQTT/Adafruit_MQTT.h" 

// CJ: === Adafruit.io Setup ===
#define AIO_SERVER      "io.adafruit.com" 
#define AIO_SERVERPORT  1883                   // use 8883 for SSL 
#define AIO_USERNAME    "cj0410" 
#define AIO_KEY         "4c896e648ca84193aaecad4f38b29fe4" 

// CJ: === Global State (you don't need to change this!) ===
TCPClient TheClient; 
// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details. 
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// CJ: === Feeds ===
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname> 
Adafruit_MQTT_Publish hue = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/hue"); 
Adafruit_MQTT_Publish sat = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/sat"); 
Adafruit_MQTT_Publish lum = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/lum"); 
Adafruit_MQTT_Publish bright = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/bright"); 
Adafruit_MQTT_Publish onoff = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/onoff");

Adafruit_MQTT_Subscribe rgbSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/rgb");

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
#define PIXEL_COUNT 68*2 - 1

#define DEFAULT_H 20
#define DEFAULT_S 100
#define DEFAULT_L 75

// === VARIABLES ===
// CJ: LED CONTROL
volatile uint32_t rgbColour;
volatile uint8_t buttonMode;

volatile int16_t hueControl;
uint8_t hueInc;

volatile int8_t satControl;
uint8_t satInc;

volatile int8_t lumControl;
uint8_t lumInc;

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

// CJ: MQTT Ping and connection checks
uint8_t publishMode;

uint32_t checkMQTTConnectionTimeout;
system_tick_t lastCheckMQTTConnectionTime;

uint32_t publishMQTTTimeout;
system_tick_t lastPublishMQTTTime;

struct hsl {
  uint16_t h;
  uint8_t s;
  uint8_t l;
} hslVal;

Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, 0x02);

// Prototypes for local build, ok to leave in for Build IDE
int setColour(String input);
int setBri(String input);
int setHue(String input);
int setSat(String input);
int setLum(String input);

// CJ: Interrupt functions
void toggleLight();
void toggleMode();
void incrementBri();
void incrementHue();
void incrementSat();
void incrementLum();

uint32_t Wheel(byte WheelPos);
uint32_t hsl2rgb(uint16_t ih, uint8_t is, uint8_t il);
hsl rgb2hsl(uint8_t ir, uint8_t ig, uint8_t ib);
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
  attachInterrupt(ENCODER_1A, incrementBri, CHANGE);

  pinMode(ENCODER_2A, INPUT_PULLUP);
  pinMode(ENCODER_2B, INPUT_PULLUP);
  attachInterrupt(ENCODER_2A, incrementColour, CHANGE);

  // CJ: Initialise light off
  lightOff = false;

  // CJ: Initialise the colour and brightness
  hslVal.h = DEFAULT_H;
  hslVal.s = DEFAULT_S;
  hslVal.l = DEFAULT_L;
  rgbColour = hsl2rgb(hslVal.h, hslVal.s, hslVal.l);

  // CJ: Initialise brightness
  brightnessInc = 5;
  brightnessControl = 20;
  brightness =  (uint8_t) brightnessControl;
  lastBrightness = brightness;

  // CJ: Initialise colour increment
  hueControl = hslVal.h;
  hueInc = 5;

  satControl = hslVal.s;
  satInc = 5;

  lumControl = hslVal.l;
  lumInc = 5;

  buttonMode = 1; // set hue
  publishMode = 1;

  // CJ: Init debounce timeout
  pb1DebounceTimeout = 250; //ms
  pb1LastDebounceTime = millis();

  pb2DebounceTimeout = 250; //ms
  pb2LastDebounceTime = millis();

  // CJ: Init MQTT connection check
  checkMQTTConnectionTimeout = MQTT_CONN_KEEPALIVE*1000/2;
  lastCheckMQTTConnectionTime = millis();

  publishMQTTTimeout = 5000;
  lastPublishMQTTTime = millis();

  // CJ: Declare particle functions
  Particle.function("setColour", setColour);
  Particle.function("setBri", setBri);
  Particle.function("setHue", setHue);
  Particle.function("setSat", setSat);
  Particle.function("setLum", setLum);
  Particle.function("setButtonMode", setButtonMode);

  // CJ: Connect to MQTT
  mqtt.subscribe(&rgbSub); 
  mqtt.connect();
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  int i;
  noInterrupts();
  system_tick_t currentTime = millis();
  if(currentTime - lastCheckMQTTConnectionTime > checkMQTTConnectionTimeout) {
    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect
    // function definition further below.
    MQTT_connect();
    lastCheckMQTTConnectionTime = currentTime;
  }

  if(currentTime - lastPublishMQTTTime > publishMQTTTimeout) {
    switch(publishMode) { 
      case 1: 
        hue.publish(hueControl);
        publishMode++;
        break; 
      case 2: 
        sat.publish(satControl);
        publishMode++;
        break; 
      case 3: 
        lum.publish(lumControl);
        publishMode++;
        break; 
      case 4:
        bright.publish(brightnessControl);
        publishMode = 1;
        break;
      default:
        break;
    }

    if(lightOff) {
      onoff.publish("OFF");
    } else {
      onoff.publish("ON");
    }

    lastPublishMQTTTime = currentTime;
  }

  for(i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, rgbColour);
  }
  strip.setBri(brightness);
  strip.show();

  interrupts();
  delay(100);
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

void incrementBri() {
  e1aState = digitalRead(ENCODER_1A);

  if(e1aState != e1aLastState && !lightOff) { 
    if(digitalRead(ENCODER_1B) == e1aState) {
      brightnessControl += brightnessInc;
    } else {
      brightnessControl -= brightnessInc;
    }

    brightnessControl = max(0, min(255, brightnessControl));
    brightness = (uint8_t) brightnessControl; 
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

  rgbColour = hsl2rgb(hslVal.h, hslVal.s, hslVal.l);
  interrupts();
}

void incrementHue() {
  e2aState = digitalRead(ENCODER_2A);

  if(e2aState != e2aLastState && !lightOff) { 
    if(digitalRead(ENCODER_2B) == e2aState) {
      hueControl += hueInc;
      if(hueControl > 359) {
        hueControl = 0;
      }
    } else {
      hueControl -= hueInc;
      if(hueControl < 0) {
        hueControl = 359;
      }
    }

  hueControl = max(0, min(359, hueControl));
  hslVal.h = (uint16_t) hueControl;
  }

  e2aLastState = e2aState;
}

void incrementSat() {
  e2aState = digitalRead(ENCODER_2A);

  if(e2aState != e2aLastState && !lightOff) { 
    if(digitalRead(ENCODER_2B) == e2aState) {
        satControl += satInc;
    } else {
        satControl -= satInc;
    }

    satControl = max(0, min(100, satControl));
    hslVal.s = (uint8_t) satControl;
  }

  e2aLastState = e2aState;
}

void incrementLum() {
  e2aState = digitalRead(ENCODER_2A);

  if(e2aState != e2aLastState && !lightOff) { 
    if(digitalRead(ENCODER_2B) == e2aState) {
        lumControl += satInc;
    } else {
        lumControl -= satInc;
    }

    lumControl = max(0, min(100, lumControl));
    hslVal.l = (uint8_t) lumControl;
  }

  e2aLastState = e2aState;
}

// CJ: Particle Functions
int setHue(String input) {
  hslVal.h = max(min(input.toInt(), 100), 0)*359/100;

  rgbColour = hsl2rgb(hslVal.h, hslVal.s, hslVal.l);
  return hslVal.h;
}

int setSat(String input) {
  hslVal.s = max(min(input.toInt(), 100), 0);

  rgbColour = hsl2rgb(hslVal.h, hslVal.s, hslVal.l);
  return hslVal.s;
}

int setLum(String input) {
  hslVal.l = max(min(input.toInt(), 100), 0);

  rgbColour = hsl2rgb(hslVal.h, hslVal.s, hslVal.l);
  return hslVal.l;
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

int setBri(String input) {
  
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
 * Map RGB color space to HSL
 * 
 * Totally borrowed from:
 * https://www.programmingalgorithms.com/algorithm/rgb-to-hsl?lang=C%2B%2B
 * 
 */
hsl rgb2hsl(uint8_t ir, uint8_t ig, uint8_t ib) {
  hsl tempHsl;

	float r = (ir / 255.0f);
	float g = (ig / 255.0f);
	float b = (ib / 255.0f);

	float vmin = min(min(r, g), b);
	float vmax = max(max(r, g), b);
	float delta = vmax - vmin;

	tempHsl.l = (int) ((vmax + vmin)/2)*100;

	if (delta == 0.0f)
	{
		tempHsl.h = 0;
		tempHsl.s = 0;
	}
	else
	{
		tempHsl.s = (int) (tempHsl.l <= 50) ? (delta / (vmax + vmin))*100 : (delta / (2 - vmax - vmin))*100;

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

		tempHsl.h = (int)(hue*360);
	}

  return tempHsl;
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

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;
 
  // Stop if already connected.
  if (mqtt.connected()) {
    // CJ: Service the MQTT broker
    if(!mqtt.ping()) {
      mqtt.disconnect();
    }

    return;
  }
 
  //Serial.print("Connecting to MQTT... ");
 
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       //Serial.println(mqtt.connectErrorString(ret));
       //Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
  }
  //Serial.println("MQTT Connected!");
}