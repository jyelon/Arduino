// This is the code for Josh's LED glasses, which he made as part of a
// halloween costume in 2019.
//
// The glasses consist of a Beetle microcontroller (which shows up in
// the arduino IDE as an "Arduino/Genuino Micro"), connected to four
// LED rings.  Each LED ring has 12 LEDs.  The LEDs form a single 48
// LED strip, connected to arduino pin 3.
//

#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel strip = Adafruit_NeoPixel(48, 3, NEO_GRB + NEO_KHZ800);

// The raw LED numbering is very confusing.  The following arrays
// simplify matters:
//
// small_l - the small ring on left eye, starting near nose and going upward.
// small_r - the small ring on right eye, starting near nose and going upward.
// large_l - the large ring on left eye, starting near nose and going upward.
// large_r - the large ring on right eye, starting near nose and going upward.
// ring_l - all 24 LEDs on the left eye, starting near nose and going upward.
// ring_r - all 24 LEDs on the right eye, starting near nose and going upward.
//

int small_l[12] = { 23, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 };
int large_l[12] = { 11,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10 };
int small_r[12] = { 25, 24, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26 };
int large_r[12] = { 38, 37, 36, 47, 46, 45, 44, 43, 42, 41, 40, 39 };
int ring_l[24] = { 23, 11, 12, 0, 13, 1, 14, 2, 15, 3, 16, 4, 17, 5, 18, 6, 19, 7, 20, 8, 21, 9, 22, 10 };
int ring_r[24] = { 25, 38, 24, 37, 35, 36, 34, 47, 33, 46, 32, 45, 31, 44, 30, 43, 29, 42, 28, 41, 27, 40, 26, 39 };

// Set the color of the Nth LED. Clamps R,G,B into the valid range.
// You can specify the minimum value for the clamping.

void setPixel(int i, int r, int g, int b, int minv) {
  r = min(255, max(minv, r));
  g = min(255, max(minv, g));
  b = min(255, max(minv, b));
  strip.setPixelColor(i, r, g, b);
}

// Ramp: as phase goes from 0 to 32768, the ramp gradually rises
// from 0 to 20480.  It rises slowly at first, then faster.
// As phase goes from 32768 to 65535, the ramp comes back down.

uint16_t ramp(uint16_t phase) {
  uint32_t iphase = phase;
  if (iphase > 32768) {
    iphase = 65536 - iphase;
  }
  if (iphase < 16384) {
    return iphase / 4;
  } else {
    return iphase - 12288;
  }
}

// Ramp: as phase goes from 0 to 32768, the ramp gradually rises
// from 0 to 32767.  It rises very slowly at first, then faster.
// As phase goes from 32768 to 65535, the ramp comes back down.

uint16_t zramp(uint16_t phase) {
  uint32_t iphase = phase;
  if (iphase > 32768) {
    iphase = 65536 - iphase;
  }
  if (iphase < 8192) {
    return 0;
  } else if (iphase < 24576) {
    return (iphase - 8192);
  } else {
    return (iphase - 16384) * 2;
  }
}

void setup() {
  strip.begin();
  strip.setBrightness(255);
  strip.show();
}


int32_t t = 1000000;

void setSwirl(int index[24], int rspeed, int gspeed, int bspeed) {
  const int speedreduce = 1;
  rspeed >>= speedreduce;
  gspeed >>= speedreduce;
  bspeed >>= speedreduce;
  for (int i = 0; i < 24; i++) {
    int offset = (i * (65536/24));
    uint16_t r = ramp(t * rspeed + offset);
    uint16_t g = ramp(t * gspeed + offset);
    uint16_t b = ramp(t * bspeed + offset);
    setPixel(index[i], r>>10, g>>10, b>>10, 3);
  }
}

void modeSwirl() {
  setSwirl(ring_l, 88,  133, -111);
  setSwirl(ring_r, 83,  127, -107);
}

void googly(int index[24], int speed1, int speed2) {
  const int speedreduce = 1;
  speed1 >>= speedreduce;
  speed2 >>= speedreduce;
  for (int i = 0; i < 24; i++) {
    int offset = (i * (65536/24));
    uint32_t b1 = zramp(t * speed1 + offset);
    uint32_t b2 = zramp(t * speed2 + offset);
    uint32_t bright = (b1 + b2) >> 12;
    uint32_t minv = (i & 1) ? 3:1;
    setPixel(index[i], bright, bright, bright, minv);
  }
}

void modeGoogly() {
  googly(ring_l, 88, -131);
  googly(ring_r, 83, -127);
}


void loop() {
  t += 1;
  modeGoogly();
  strip.show();
}
