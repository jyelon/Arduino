#include <Bounce2.h>
#include <Adafruit_NeoPixel.h>

Bounce debounce_rotc = Bounce();
Bounce debounce_less = Bounce();
Bounce debounce_more = Bounce();

Adafruit_NeoPixel strip1(180, D2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(30,  D1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3(30,  D4, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip4(60,  D3, NEO_GRB + NEO_KHZ800);

void illuminate(int n, uint32_t color) {
    for (int i = 0; i < 180; i++) {
        if (n > i) {
            strip1.setPixelColor(i, color);
        } else {
            strip1.setPixelColor(i, 0);
        }
    }
    for (int i = 0; i < 30; i++) {
        if (n > i + 180) {
            strip2.setPixelColor(i, color);
        } else {
            strip2.setPixelColor(i, 0);
        }
        if (n > i + 210) {
            strip3.setPixelColor(i, color);
        } else {
            strip3.setPixelColor(i, 0);
        }
    }
    for (int i = 0; i < 60; i++) {
        if (n > i + 240) {
            strip4.setPixelColor(i, color);
        } else {
            strip4.setPixelColor(i, 0);
        }
    }
    strip1.show();
    strip2.show();
    strip3.show();
    strip4.show();
}

int total = 0;
int color = 0;

void setup() {
    debounce_rotc.attach(D7, INPUT_PULLUP);
    debounce_less.attach(D6, INPUT_PULLUP);
    debounce_more.attach(D5, INPUT_PULLUP);
    debounce_rotc.interval(25);
    debounce_less.interval(25);
    debounce_more.interval(25);
    Serial.begin(9600);
    strip1.begin();
    strip2.begin();
    strip3.begin();
    strip4.begin();
    illuminate(5, strip1.Color(10, 10, 10));
}

int xtotal(int total) {
    int msb = total >> 3;
    int lsb = total & 7;
    return (msb * (msb + 1) * 4) + (lsb * (msb + 1));
}


void loop() {
    debounce_rotc.update();
    debounce_less.update();
    debounce_more.update();
    int prev = total;
    if (debounce_rotc.fell()) {
        Serial.printf("Button1\n");
        Serial.printf("Color: %d LEDs: %d\n", color, xtotal(total));
        color = (color + 1) & 7;
    }
    if (debounce_less.fell()) {
        Serial.printf("Button2\n");
        Serial.printf("Color: %d LEDs: %d\n", color, xtotal(total));
        if (total > 0) total--;
    }
    if (debounce_more.fell()) {
        Serial.printf("Button3\n");
        Serial.printf("Color: %d LEDs: %d\n", color, xtotal(total));
        if (total < 270) total++;
    }
    uint32_t c;
    switch(color) {
        case 0: c = strip1.Color(255, 255, 255); break;
        case 1: c = strip1.Color(128, 128, 128); break;
        case 2: c = strip1.Color(64, 64, 64); break;
        case 3: c = strip1.Color(16, 16, 16); break;
        case 4: c = strip1.Color(255, 0, 0); break;
        case 5: c = strip1.Color(128, 0, 0); break;
        case 6: c = strip1.Color(64, 0, 0); break;
        case 7: c = strip1.Color(16, 0, 0); break;
    }
    illuminate(xtotal(total), c);
}