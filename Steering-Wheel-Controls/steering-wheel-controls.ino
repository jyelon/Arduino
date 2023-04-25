#include "HID-Consumer.h"

const bool USE_CONSUMER = true;
const int LONG_MILLIS = 400;
const int TRANSMIT_TIME = 20;
  
void C_Begin() {
  if (USE_CONSUMER) {
    Consumer.begin();
  } else {
    Serial.begin(9600);
  }
}

void C_Press(ConsumerKeycode c, int elapsed) {
  if (USE_CONSUMER) {
    Consumer.press(c);
  } else {
    char buffer[80];
    sprintf(buffer, "press %d (elapsed: %d)\n", c, elapsed);
    Serial.write(buffer);
  }
}

void C_Release(ConsumerKeycode c) {
  if (USE_CONSUMER) {
    Consumer.release(c);
  } else {
    char buffer[80];
    sprintf(buffer, "release %d\n", c);
    Serial.write(buffer);
  }
}

void setup() {
  C_Begin();
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(7, INPUT_PULLUP);
}


class button {
public:
  enum States {
    STATE_IDLE,
    STATE_WAITING,
    STATE_SHORT_TRIGGERED,
    STATE_LONG_TRIGGERED,
    STATE_WAIT_RELEASE,
  };
  
  ConsumerKeycode short_key;
  ConsumerKeycode long_key;
  
  bool pressed;
  int threshold_lo;
  int threshold_hi;
  int debouncer;
  int last_millis;
  int now_millis;
  int state;
  int elapsed; // Amount of time in current state

  button(int correct, ConsumerKeycode s, ConsumerKeycode l) {
    threshold_lo = correct-5;
    threshold_hi = correct+5;
    short_key = s;
    long_key = l;
    state = STATE_IDLE;
  }

  button(int range_lo, int range_hi, ConsumerKeycode s, ConsumerKeycode l) {
    threshold_lo = range_lo;
    threshold_hi = range_hi;
    short_key = s;
    long_key = l;
    state = STATE_IDLE;
  }

  void debounce(int signal) {
    if ((signal >= threshold_lo) && (signal <= threshold_hi)) {
      debouncer += (now_millis - last_millis);
      if (debouncer > 15) {
        debouncer = 15;
        if (!pressed) {
          pressed = true;
        }
      }
    } else {
      debouncer -= (now_millis - last_millis);
      if (debouncer < 0) {
        debouncer = 0;
        if (pressed) {
          pressed = false;
        }
      }
    }
  }

  void update_single() {
    if ((state == STATE_IDLE) && (pressed)) {
      state = STATE_SHORT_TRIGGERED;
      C_Press(short_key, 0);
    }
    if ((state == STATE_SHORT_TRIGGERED) && (!pressed)) {
      state = STATE_IDLE;
      C_Release(short_key);
    }
  }

  void update_double() {
    elapsed += (now_millis - last_millis);
    
    if ((state == STATE_IDLE) && pressed) {
      state = STATE_WAITING;
      elapsed = 0;
    }

    if ((state == STATE_WAITING) && pressed && (elapsed > LONG_MILLIS)) {
      C_Press(long_key, 0);
      state = STATE_LONG_TRIGGERED;
      elapsed = 0;
    }

    if ((state == STATE_WAITING) && (!pressed)) {
      C_Press(short_key, elapsed);
      state = STATE_SHORT_TRIGGERED;
      elapsed = 0;
    }

    if ((state == STATE_LONG_TRIGGERED) && (elapsed > TRANSMIT_TIME)) {
      C_Release(long_key);
      state = STATE_WAIT_RELEASE;
      elapsed = 0;
    }

    if ((state == STATE_SHORT_TRIGGERED) && (elapsed > TRANSMIT_TIME)) {
      C_Release(short_key);
      state = STATE_IDLE;
      elapsed = 0;
    }

    if ((state == STATE_WAIT_RELEASE) && (!pressed)) {
      state = STATE_IDLE;
      elapsed = 0;
    }
  }

  void update(int signal) {
    last_millis = now_millis;
    now_millis = millis();
    debounce(signal);
    if (short_key == long_key) {
      update_single();
    } else {
      update_double();
    }
  }
};

button btn_dpad_n(403, HID_CONSUMER_MENU_UP, HID_CONSUMER_MENU_UP);
button btn_dpad_s(159, HID_CONSUMER_MENU_DOWN, HID_CONSUMER_MENU_DOWN);
button btn_dpad_w(599, HID_CONSUMER_MENU_LEFT, HID_CONSUMER_SCAN_PREVIOUS_TRACK);
button btn_dpad_e(774, HID_CONSUMER_MENU_RIGHT, HID_CONSUMER_SCAN_NEXT_TRACK);
button btn_dpad_center(893, HID_CONSUMER_MENU_PICK, HID_CONSUMER_MENU_PICK);
button btn_dpad_ne(407, HID_CONSUMER_PLAY, HID_CONSUMER_STOP);
button btn_dpad_se(62, HID_CONSUMER_MENU_ESCAPE, HID_CONSUMER_AC_HOME);
button btn_hfl_1(298, HID_CONSUMER_VOLUME_INCREMENT, HID_CONSUMER_AL_WORD_PROCESSOR);
button btn_hfl_2(84, HID_CONSUMER_VOLUME_DECREMENT, HID_CONSUMER_AL_TEXT_EDITOR);
button btn_hfl_3(824, HID_CONSUMER_AL_SELECT_TASK_SLASH_APPLICATION, HID_CONSUMER_AL_SPREADSHEET);
button btn_stick(0, 0, HID_CONSUMER_AC_SEARCH, HID_CONSUMER_AC_SEARCH);

void loop() {
  int a0 = analogRead(A0);
  int a1 = analogRead(A1);
  int a2 = analogRead(A2);
  int d7 = digitalRead(7);

  btn_dpad_ne.update(a0);
  btn_dpad_se.update(a0);
  
  btn_dpad_n.update(a1);
  btn_dpad_s.update(a1);
  btn_dpad_w.update(a1);
  btn_dpad_e.update(a1);
  btn_dpad_center.update(a1);
  
  btn_hfl_1.update(a2);
  btn_hfl_2.update(a2);
  btn_hfl_3.update(a2);

  btn_stick.update(d7);
}
