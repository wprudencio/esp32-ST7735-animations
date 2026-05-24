#include <Adafruit_NeoPixel.h>

#define TOUCH_PIN   0
#define LED_PIN     10
#define NUMPIXELS   1

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pixels.begin();
  pixels.setBrightness(50);
  pixels.clear();
  pixels.show();
}

void loop() {
  // TTP223 default mode: output HIGH when touched, LOW when not
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);

  if (touched) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
  } else {
    pixels.clear();
  }
  pixels.show();

  delay(50);
}
