// Code for the machine that goes bing.

#include <Adafruit_NeoPixel.h>
#include <avr/power.h>
#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <tables/sin2048_int8.h> // sine table for oscillator
#include <tables/pinknoise8192_int8.h>
#include <RollingAverage.h>

// The Meter on the front panel is controlled by a simple PWM pin.
#define METER_PIN 11

/////////////////////////////////////////////////////////
//
// NeoPixel LEDs
//
// All the LEDS in the machine are neopixels.  There is one inside the
// so-called "neon" indicator, and there are two in each tube, for a total
// of 21 LEDs.
//
// I have some routines here that accept "tube indices" which range
// from 0-9, to make it easier to color the 10 tubes.
//
/////////////////////////////////////////////////////////

#define NEOPIXEL_PIN 8
Adafruit_NeoPixel strip = Adafruit_NeoPixel(21, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

struct Color {
  Color() : r(0), g(0), b(0) {}
  Color(int ir, int ig, int ib) : r(ir), g(ig), b(ib) {}
  void clear() { r = 0; g = 0; b = 0; }
  int r;
  int g;
  int b;
};

Color hue(byte hue) {
  if(hue < 85) {
    return Color(255 - hue * 3, 0, hue * 3);
  } else if (hue < 170) {
    hue -= 85;
    return Color(0, hue * 3, 255 - hue * 3);
  } else {
    hue -= 170;
    return Color(hue * 3, 255 - hue * 3, 0);
  }
}

struct Color led[21];

void strip_update() {
  for(int i=0; i<21; i++) {
    int r= led[i].r;
    if (r > 255) r = 255;
    int g= led[i].g;
    if (g > 255) g = 255;
    int b= led[i].b;
    if (b > 255) b = 255;
    strip.setPixelColor(i, r, g, b);
  }
  strip.show();
}

void tube_add_r(int i, int v1, int v2) {
  led[i*2 + 1].r += v1;
  led[i*2 + 2].r += v2;
}

void tube_add_g(int i, int v1, int v2) {
  led[i*2 + 1].g += v1;
  led[i*2 + 2].g += v2;
}

void tube_add_b(int i, int v1, int v2) {
  led[i*2 + 1].b += v1;
  led[i*2 + 2].b += v2;
}

void tube_add(int i, struct Color c) {
  led[i*2 + 1].r += c.r;
  led[i*2 + 1].g += c.g;
  led[i*2 + 1].b += c.b;
  led[i*2 + 2].r += c.r;
  led[i*2 + 2].g += c.g;
  led[i*2 + 2].b += c.b;
}

void tube_add_hue(int i, byte h) {
  tube_add(i, hue(h));
}

/////////////////////////////////////////////////////////
//
// Low-Level Mozzi Sound Output.
//
// The updateAudio routine is responsible for generating the final
// waveform.  Our version of updateAudio uses three oscillators at all
// times.  Oscillators A and B are both sine waves,
// and oscillator P is for pink noise (a drum-like noise).  The amplitude of the
// oscillators is controlled by three variables: aAmp, bAmp, and
// pAmp.  The control routine can change the frequency of the oscillators
// and it can update the amplitudes stored in the three variables aAmp,
// bAmp, and pAmp.
//
// The amplitudes are controlled by aAmp, bAmp, and pAmp.  The control routine
// can only update these once every 1/64 second, which results in fairly large
// jumps.  These jumps would cause clicks in the output if I didn't do anything
// to smooth them.  The variables saAmp, sbAmp, and spAmp are smoothed versions
// of these variables.
//
// Since the B oscillator is used for low-frequency, we amplify it a little more
// than the other oscillators to get a real pleasing bass boost.
//
/////////////////////////////////////////////////////////

#define CONTROL_RATE 64 // powers of 2 please

Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> bSin(SIN2048_DATA);
Oscil <PINKNOISE8192_NUM_CELLS, AUDIO_RATE> pink(PINKNOISE8192_DATA);

int aAmp = 0;
int bAmp = 0;
int pAmp = 0;

int saAmp = 0;
int sbAmp = 0;
int spAmp = 0;

int updateAudio(){
  if (saAmp < aAmp) saAmp++;
  if (saAmp > aAmp) saAmp--;
  if (sbAmp < bAmp) sbAmp++;
  if (sbAmp > bAmp) sbAmp--;
  if (spAmp < pAmp) spAmp++;
  if (spAmp > pAmp) spAmp--;
  int awave = (aSin.next() * saAmp) >> 8;
  int bwave = (bSin.next() * sbAmp) >> 7;
  int pwave = (pink.next() * spAmp) >> 8;
  int total = awave + bwave + pwave;
  if (total > 240) total = 240;
  if (total < -240) total = -240;
  return total;
}

/////////////////////////////////////////////////////////
//
// The Four Knobs
//
// There are four knobs on the front panel.  Two, the "scale" and "load"
// knobs, are ordinary potentiometers.  The other two, the "filament" and
// "range" knobs, are actually multi-position switches.  I've rigged up 
// the filament and range knobs with a different resistor on each position,
// to make them act like pseudo-potentiometers.  The four knobs come
// into the arduino as four analog signals A0-A3.
//
// However, I still have two different classes: AnalogKnob for the
// range and filament switches, and AveragedPot for the load and scale
// knobs.
//
// The AnalogKnob class update routine reads the analog input, then it
// checks it against a table of known values to try do determine which
// position the knob is in.  It debounces the switch by making sure that
// the last 8 readings are all the same before it updates the last known
// value.  In practice, it converges very fast.
//
// The AveragedPot class is used to read the potentiometers, which are
// very old and a little gritty.  Feeding the input value through a sliding
// window averaging routine helps a lot, and it adds a nice "wheeoooo"
// delay effect when you turn the knob.
//
/////////////////////////////////////////////////////////


class AnalogKnob {
  private:
  int data[8];
  int pin;
  int *levels;
  int nlevels;
  int lastlock;

  bool match(int n) {
    if (n != data[0]) return false;
    if (n != data[1]) return false;
    if (n != data[2]) return false;
    if (n != data[3]) return false;
    if (n != data[4]) return false;
    if (n != data[5]) return false;
    if (n != data[6]) return false;
    if (n != data[7]) return false;
    return true;
  }

  void shift(int n) {
    data[0]=data[1];
    data[1]=data[2];
    data[2]=data[3];
    data[3]=data[4];
    data[4]=data[5];
    data[5]=data[6];
    data[6]=data[7];
    data[7]=n;
  }
  
public:
  AnalogKnob(int ipin, int *ilevels, int inlevels) {
    pin = ipin;
    levels = ilevels;
    nlevels = inlevels;
    lastlock = 0;
    data[0]=data[1]=data[2]=data[3]=0;
    data[4]=data[5]=data[6]=data[7]=0;
  }

  int value() {
    return lastlock;
  }

  bool update() {
    bool changed = false;
    int n = mozziAnalogRead(pin);
    for (int i = 0; i < nlevels; i++) {
      int target = levels[i];
      if ((n > target - 10) && (n < target + 10)) {
        if (match(i)) {
          if (i != lastlock) changed = true;
          lastlock = i;
        }
        shift(i);
        return changed;
      }
    }
    shift(-1);
    return changed;
  }
};

class AveragedPot {
private:
  RollingAverage<int, 8> stage2;
  RollingAverage<int, 8> stage1;
  int pin;
  int last;
  
public:
  AveragedPot(int ipin) {
    pin = ipin;
  }
  
  bool update() {
    int a = mozziAnalogRead(pin);
    int s = stage1.next(a);
    last = stage2.next(s);
    int diff = (last - s);
    return (diff > 15) || (diff < -15);
  }
  
  int value() {
    return last;
  }
};

int knob_range_levels[] = { 730, 710, 688, 641, 600, 555, 459, 407, 319, 240 };
int knob_filament_levels[] = { 238, 319, 408, 510, 555, 600, 643 };

AnalogKnob knob_range(A1, knob_range_levels, 10);
AnalogKnob knob_filament(A0, knob_filament_levels, 7);
AveragedPot knob_scale(A2);
AveragedPot knob_load(A3);

/////////////////////////////////////////////////////////
//
// The front panel switches.
//
// There are a lot of switches on the front panel - way more
// switches than the arduino has inputs.  To read that many switches,
// I've hooked them up the way one hooks up a multiplexed keyboard
// matrix.   I've attached a diode to every switch so that the switches
// don't "ghost" each other.  To read the switches, you pull one of the
// output lines low, you wait for a short delay, and then you can read
// five of the switches from the input lines. If you pull a different
// output line low, you can read a different five switches. 
//
// The SwitchPanel has an update routine whose precondition is that
// one of the output lines is *already* low.  The update routine reads the
// five switches that it can, then it pulls a *different* output line low.
// Then it returns, having only read 5 switches.  You should delay a little
// before calling update again. When you call update again, it will read
// a different set of 5 switches. It takes 4 calls to update to fully read
// all the switches.
//
// There is a small table of defines indicating which front panel switch
// is connected to which grid position.
//
// Three of the knobs on the front panel are read as if they were switches:
// "master switch", "selector A", and "selector B".  Selector A has
// four positions, so it actually gets treated as 3 switches (the fourth
// position is off).
//
/////////////////////////////////////////////////////////

int kb_outputs[4] = { A4, A5, 13, 12 };
int kb_inputs[5] = { 2,3,4,5,6 };

#define KEY_NONE 0
#define KEY_A 0x0001
#define KEY_B 0x0002
#define KEY_C 0x0004
#define KEY_D 0x0008
#define KEY_E 0x0010
#define KEY_F 0x0101
#define KEY_G 0x0102
#define KEY_H 0x0104
#define KEY_I 0x0108
#define KEY_J 0x0110
#define KEY_MASTER_SWITCH   0x8201
#define KEY_HIGH_PRI_LOW    0x8202
#define KEY_HIGH_FIL_LOW    0x8204
#define KEY_ON_POWER_OFF    0x8208
#define KEY_QUALITY_LEAKAGE 0x8210
#define KEY_SELECTOR_A1     0x0301
#define KEY_SELECTOR_A2     0x0302
#define KEY_SELECTOR_A3     0x0304
#define KEY_SELECTOR_B      0x8310


class SwitchPanel {
  private:
  uint8_t data[4];
  uint8_t active_output;

public:
  void setup() {
    for (int i = 0; i < 4; i++) {
      data[i] = 0;
      pinMode(kb_outputs[i], OUTPUT);
      digitalWrite(kb_outputs[i], HIGH);
    }
    for (int i = 0; i < 5; i++) {
      pinMode(kb_inputs[i], INPUT_PULLUP);
    }
    active_output = 0;
    digitalWrite(kb_outputs[0], LOW);
  }
  
  bool update() {
    int result = 0;
    for (int j = 0; j < 5; j++) {
      if (digitalRead(kb_inputs[j]) == 0) {
        result |= (1<<j);
      }
    }
    bool changed = (result != data[active_output]);
    data[active_output] = result;
    digitalWrite(kb_outputs[active_output], HIGH);
    active_output = (active_output + 1) & 3;
    digitalWrite(kb_outputs[active_output], LOW);
    return changed;
  }
  
  bool get(uint16_t key) {
    if (key & 0x8000) {
      return !(data[(key>>8) & 3] & key);
    } else {
      return data[(key>>8) & 3] & key;
    }
  }

  void print() {
    char buffer[80];
    sprintf(buffer, "%02x %02x %02x %02x\n", data[0], data[1], data[2], data[3]);
    Serial.write(buffer);
  }
};


SwitchPanel switches;

// Fetch several switches and add them up as a binary number.
// Key1 is the least significant bit.

uint8_t switches_to_int(int key1, int key2, int key3) {
  int result = 0;
  if (switches.get(key1)) result += 1;
  if (switches.get(key2)) result += 2;
  if (switches.get(key3)) result += 4;
  return result;
}

/////////////////////////////////////////////////////////
//
// Bending:
//
// The control routine assigns initial constant frequencies to oscillators
// A and B.  It then "bends" those frequencies upward or downward every time
// that the update control routine is called.  The bending follows a selectable
// repeating pattern: you can bend in a ramp, in a sine wave, in discrete steps,
// and so forth.  The following code supports bending.
//
/////////////////////////////////////////////////////////

#define BEND_NONE 0
#define BEND_RAMP_UP 1
#define BEND_RAMP_DOWN 2
#define BEND_SINE 3
#define BEND_2_DISCRETE 4
#define BEND_4_DISCRETE 5
#define BEND_DRAMP_UP 6

int apply_bend(uint16_t clock, uint8_t kind, int old) {
  switch (kind) {
  case BEND_NONE: return old;
  case BEND_RAMP_UP: return apply_bend_ramp_up(clock, old);
  case BEND_RAMP_DOWN: return apply_bend_ramp_down(clock, old);
  case BEND_SINE: return apply_bend_sine(clock, old);
  case BEND_2_DISCRETE: return apply_bend_2_discrete(clock, old);
  case BEND_4_DISCRETE: return apply_bend_4_discrete(clock, old);
  case BEND_DRAMP_UP: return apply_bend_dramp_up(clock, old);
  }
}

int apply_bend_ramp_up(uint16_t clock, int old) {
  int phase = (clock & 0xFF);
  int adjust = (int32_t(old) * phase) >> 8;
  return (old >> 1) + adjust;
}

int apply_bend_ramp_down(uint16_t clock, int old) {
  int phase = 0x100 - (clock & 0xFF);
  int adjust = (int32_t(old) * phase) >> 8;
  return (old >> 1) + adjust;
}

int apply_bend_dramp_up(uint16_t clock, int old) {
  int phase = (clock & 0xFF);
  int adjust = (int32_t(old * 3 / 2) * phase) >> 8;
  return (old >> 1) + adjust;
}

int apply_bend_sine(uint16_t clock, int old) {
  int phase = (clock & 0xFF) << 3;
  int offset = 128 + mozzi_pgm_read_wrapper(SIN2048_DATA + phase);
  int adjust = (int32_t(old) * offset) >> 8;
  return (old >> 1) + adjust;
}

int apply_bend_2_discrete(uint16_t clock, int old) {
  if (clock & 0x80) {
    return (old * 3) / 4;
  } else {
    return (old * 5) / 4;
  }
}

int apply_bend_4_discrete(uint16_t clock, int old) {
  switch (clock & 0xC0) {
    case 0x00: return (old * 2) / 4;
    case 0x40: return (old * 3) / 4;
    case 0x80: return (old * 4) / 4;
    case 0xC0: return (old * 5) / 4;
  }
}

int apply_mramp(uint16_t clock, int old) {
  int adjust = (int32_t(old) * (clock & 0xFF)) >> 9;
  return ((old * 3) / 4) + adjust;
}

/////////////////////////////////////////////////////////
//
// Chopping:
//
// The control routine assigns initial amplitudes to the oscillators.
// It then "chops" the amplitude - it creates gaps in the audio output by
// cranking the amplitude way down in a repeating pattern.  There are several
// patterns: a square wave, a "strike" (a brief burst of sound), a sine wave,
// or no chopping.
//
/////////////////////////////////////////////////////////

#define CHOP_NONE 0
#define CHOP_SQUARE 1
#define CHOP_STRIKE 2
#define CHOP_SINE 3
#define CHOP_DRUM 4


int chop_strike(int phase) {
  if (phase < 0x10) {
    return phase * 16;
  } else if (phase < 0x20) {
    return 256;
  } else if (phase < 0x40) {
    return (0x40 - phase) * 8;
  } else {
    return 0;
  }
}

int chop(int phase, uint8_t kind) {
  switch (kind) {
    case CHOP_NONE: return 256;
    case CHOP_SQUARE: return phase & 0x80 ? 256 : 0;
    case CHOP_STRIKE: return chop_strike(phase);
    case CHOP_SINE: return 128 + int(mozzi_pgm_read_wrapper(SIN2048_DATA + (phase * 8)));
    case CHOP_DRUM: return (phase < 16) ? 16 * (16 - phase) : 0;
    default: return 256;
  }
}

int apply_chop(int chop, int old) {
  int32_t lo = old >> 2;
  int32_t hi = old * 3 / 2;
  return ((lo * (256 - chop)) + (hi * chop)) >> 8;
}

/////////////////////////////////////////////////////////
//
// The master control routines.
//
/////////////////////////////////////////////////////////

void setup(){
  Serial.begin(57600);
  strip.begin();
  strip.setBrightness(50);
  strip.show();
  switches.setup();
  pinMode(METER_PIN, OUTPUT);
  analogWrite(METER_PIN, 255);
  startMozzi(CONTROL_RATE); // set a control rate of 64 (powers of 2 please)
  pink.setFreq(220);
}


// Increments at a rate of 4096 per second.
uint32_t rclock = 0;
int idle_t = 0;
const int fade_begin = 30 * CONTROL_RATE;
const int fade_time = 8 * CONTROL_RATE;
const int fade_end = fade_begin + fade_time;

void updateControl(){
  bool changed = false;
  changed |= knob_load.update();
  changed |= knob_scale.update();
  changed |= knob_filament.update();
  changed |= knob_range.update();
  changed |= switches.update();
  if (changed) {
    idle_t = 0;
  } else {
    idle_t ++;
    if (idle_t > fade_end) idle_t = fade_end;
  }
  int fade = 256;
  if (idle_t > fade_begin) {
    fade -= (int32_t(idle_t - fade_begin) * 256 / fade_time);
  }

  // Update the rhythm clock.  This is the master clock that
  // drives all control updates.
  uint32_t increment = 256 + (knob_scale.value() >> 2);
  uint32_t prev_rclock = rclock;
  rclock += increment;

  uint16_t aFreq = 300;
  uint16_t bFreq = 70;
  int aAmpl = 40; // Max meaningful value for amplitude is 240.
  int bAmpl = 160; // Max meaningful value for amplitude is 240.
  int pAmpl = 0;
  
  int a_bender_speed = 9;
  int b_bender_speed = 10;
  uint8_t a_bender_type = BEND_NONE;
  uint8_t b_bender_type = BEND_NONE;
  
  int a_chop_speed = 0;
  uint8_t a_chop_type = CHOP_NONE;
  int b_chop_speed = 0;
  uint8_t b_chop_type = CHOP_NONE;
  

  aFreq = knob_load.value() + 200;
  
  switch (knob_range.value()) {
    case 0: bFreq = 55; break;
    case 1: bFreq = 73; break;
    case 2: bFreq = 87; break;
    case 3: bFreq = 110; break;
    case 4: bFreq = 131; break;
    case 5: bFreq = 164; break;
    case 6: bFreq = 196; break;
    case 7: bFreq = 220; break;
    case 8: bFreq = 246; break;
    case 9: bFreq = 294; break;
  }

  switch (knob_filament.value()) {
    case 0: a_bender_type = BEND_NONE; break;
    case 1: a_bender_type = BEND_RAMP_DOWN; break;
    case 2: a_bender_type = BEND_SINE; break;
    case 3: a_bender_type = BEND_2_DISCRETE; break;
    case 4: a_bender_type = BEND_4_DISCRETE; break;
    case 5: a_bender_type = BEND_DRAMP_UP; break;
    case 6: a_bender_type = BEND_RAMP_UP; break;
    default: break;
  }

  if (switches.get(KEY_SELECTOR_A1)) b_bender_type = BEND_NONE;
  else if (switches.get(KEY_SELECTOR_A2)) b_bender_type = BEND_DRAMP_UP;
  else if (switches.get(KEY_SELECTOR_A3)) b_bender_type = BEND_4_DISCRETE;
  else b_bender_type = BEND_RAMP_DOWN;

  a_chop_type = switches_to_int(KEY_HIGH_FIL_LOW, KEY_HIGH_PRI_LOW, KEY_NONE);
  b_chop_type = switches_to_int(KEY_QUALITY_LEAKAGE, KEY_ON_POWER_OFF, KEY_NONE);
  
  a_chop_speed = 7 - switches_to_int(KEY_A, KEY_B, KEY_NONE);
  a_bender_speed = 9 - switches_to_int(KEY_C, KEY_D, KEY_E);
  b_chop_speed = 8 - switches_to_int(KEY_F, KEY_G, KEY_H);
  b_bender_speed = 7 - switches_to_int(KEY_I, KEY_J, KEY_NONE);

  int p_mode = switches_to_int(KEY_MASTER_SWITCH, KEY_SELECTOR_B, KEY_NONE);
  if (p_mode > 0) {
    int p_chop_speed = a_chop_speed;
    if (p_mode == 1) p_chop_speed = a_chop_speed + 1;
    else if (p_mode == 2) p_chop_speed = a_chop_speed;
    else if (p_mode == 3) p_chop_speed = a_chop_speed - 1;
    int p_chop_phase = (rclock >> p_chop_speed) & 0xFF;
    int p_chop = chop(p_chop_phase, CHOP_DRUM);
    pAmpl = p_chop * 5 / 8;
  }

  aFreq = apply_bend(rclock >> a_bender_speed, a_bender_type, aFreq);
  bFreq = apply_bend(rclock >> b_bender_speed, b_bender_type, bFreq);
  
  int a_chop_phase = (rclock >> a_chop_speed) & 0xFF;
  int b_chop_phase = (rclock >> b_chop_speed) & 0xFF;
  
  int a_chop = chop(a_chop_phase, a_chop_type);
  int b_chop = chop(b_chop_phase, b_chop_type);
  
  if (aAmpl > 240) aAmpl = 240;
  if (bAmpl > 240) bAmpl = 240;
  if (aAmpl < 0) aAmpl = 0;
  if (bAmpl < 0) bAmpl = 0;

  aAmpl = apply_chop(a_chop, aAmpl);
  bAmpl = apply_chop(b_chop, bAmpl);
  
  if (prev_rclock >> 11 != rclock >> 11) {
    for (int i = 0; i < 21; i++) led[i].clear();
    
    if (a_chop > 255) a_chop = 255;
    if (b_chop > 255) b_chop = 255;

    led[0].r = 255 - pAmpl;
    led[0].g = 255 - pAmpl;
    if (fade > 0) {
      led[0].b = 255 - pAmpl;

    
      tube_add_hue(0, aFreq);
      tube_add_hue(1, aFreq);
      tube_add_hue(2, aFreq);
      tube_add_hue(3, aFreq);
      tube_add_hue(4, aFreq);
      
      tube_add_hue(5, bFreq);
      tube_add_hue(6, bFreq);
      tube_add_hue(7, bFreq);
      tube_add_hue(8, bFreq);
      tube_add_hue(9, bFreq);
      
      tube_add_r(0, 255 - a_chop_phase, a_chop);
      tube_add_r(1, 255 - a_chop_phase, a_chop);
      tube_add_r(4, 255 - a_chop_phase, a_chop);
      tube_add_r(6, 255 - a_chop_phase, a_chop);
      tube_add_r(7, 255 - a_chop_phase, a_chop);
  
      tube_add_b(2, 255 - b_chop_phase, b_chop);
      tube_add_b(3, 255 - b_chop_phase, b_chop);
      tube_add_b(5, 255 - b_chop_phase, b_chop);
      tube_add_b(8, 255 - b_chop_phase, b_chop);
      tube_add_b(9, 255 - b_chop_phase, b_chop);
    }
    
    strip_update();

    char buffer[80];
    sprintf(buffer, "changed=%d idle=%d\n", changed, idle_t);
    Serial.write(buffer);
  }
  aSin.setFreq((int)aFreq);
  bSin.setFreq((int)bFreq);
  pink.setFreq((int)aFreq);
  if (fade < 256) {
    aAmpl = aAmpl * fade >> 8;
    bAmpl = bAmpl * fade >> 8;
    pAmpl = pAmpl * fade >> 8;
  }
  aAmp = aAmpl;
  bAmp = bAmpl;
  pAmp = pAmpl;
  analogWrite(METER_PIN, aAmpl + bAmpl);
}


void loop(){
  audioHook();
}
