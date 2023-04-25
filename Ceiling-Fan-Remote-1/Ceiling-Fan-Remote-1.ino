#include "SPI.h"
#include "Arduino.h"
#include "ArduinoLowPower.h"
#include "Josh-RFM69.h"

#define RETRANSMIT 3

#define LONG_DELAY 750
#define SHORT_DELAY 250

const unsigned long CODE_Fan1 = 0x63061800; // 0110 0011 0000 0110 0001 1000 0000 0000
const unsigned long CODE_Fan2 = 0x63061100; // 0110 0011 0000 0110 0001 0001 0000 0000
const unsigned long CODE_Fan3 = 0x63061280; // 0110 0011 0000 0110 0001 0010 1000 0000
const unsigned long CODE_Fan4 = 0x63061300; // 0110 0011 0000 0110 0001 0011 0000 0000
const unsigned long CODE_Fan5 = 0x63061980; // 0110 0011 0000 0110 0001 1001 1000 0000
const unsigned long CODE_Fan6 = 0x63061180; // 0110 0011 0000 0110 0001 0001 1000 0000
const unsigned long CODE_FPow = 0x63061080; // 0110 0011 0000 0110 0001 0000 1000 0000
const unsigned long CODE_Revs = 0x63061380; // 0110 0011 0000 0110 0001 0011 1000 0000
const unsigned long CODE_ColY = 0x63061200; // 0110 0011 0000 0110 0001 0010 0000 0000
const unsigned long CODE_ColB = 0x63061600; // 0110 0011 0000 0110 0001 0110 0000 0000
const unsigned long CODE_ColW = 0x63061780; // 0110 0011 0000 0110 0001 0111 1000 0000
const unsigned long CODE_LPow = 0x63061580; // 0110 0011 0000 0110 0001 0101 1000 0000
const unsigned long CODE_LDim = 0x63061680; // 0110 0011 0000 0110 0001 0110 1000 0000
const unsigned long CODE_LBrt = 0x63061900; // 0110 0011 0000 0110 0001 1001 0000 0000
const unsigned long CODE_Wavy = 0x63061880; // 0110 0011 0000 0110 0001 1000 1000 0000
const unsigned long CODE_XX2H = 0x63061a00; // 0110 0011 0000 0110 0001 1010 0000 0000
const unsigned long CODE_XX4H = 0x63061500; // 0110 0011 0000 0110 0001 0101 0000 0000

#define COMMON_BUTTON_PIN 12

using ButtonFunction = void (*)(unsigned long arg);

struct ButtonInfo {
  const char *name;
  int pin;
  unsigned char probes;    // Record of last 8 digitalReads
  unsigned char debounced; // Smoothed version of digitalReads
  ButtonFunction func;
  unsigned long arg;
};

void simple_isr() {
  Serial.printf("ISR\n");
}

void outputLowOnPin(int pin) {
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

unsigned long applySeqno(unsigned long btn, int seqno) {
  // Strip off old seqno and checksum
  btn &= 0xFFFFFF80;
  // Apply the seqno.
  unsigned long okseq = (seqno & 7);
  btn |= (okseq << 4);
  // Calculate and apply checksum
  unsigned char nyb0 = (btn >> 4) & 15;
  unsigned char nyb1 = (btn >> 8) & 15;
  unsigned char nyb2 = (btn >> 12) & 15;
  unsigned char nyb3 = (btn >> 16) & 15;
  unsigned char nyb4 = (btn >> 20) & 15;
  unsigned long cksum = (nyb0 ^ nyb1 ^ nyb2 ^ nyb3 ^ nyb4 ^ 15);
  btn |= cksum;
  return btn;
}

// This bit transmission routine takes 1ms.
void sendBit(int bit) {
  if (bit == 0) {
    digitalWrite(DIO2, 1);
    delayMicroseconds(SHORT_DELAY);
    digitalWrite(DIO2, 0);
    delayMicroseconds(LONG_DELAY);
  } else if (bit == 1) {
    digitalWrite(DIO2, 1);
    delayMicroseconds(LONG_DELAY);
    digitalWrite(DIO2, 0);
    delayMicroseconds(SHORT_DELAY);
  } else {
    digitalWrite(DIO2, 0);
    delayMicroseconds(LONG_DELAY + SHORT_DELAY);
  }
}

// The bitqueue can contain:
//  0 - send a 0 bit.
//  1 - send a 1 bit.
//  2 - send a stop bit.
//  3 - dummy value meaning queue is empty.
// The queue is a circular buffer.  When queuehead==queuetail, the queue is empty.

const int QUEUESIZE = 4096;
const int QUEUEMASK = QUEUESIZE - 1;
unsigned char bitqueue[QUEUESIZE];
int queuehead; // Index of first used slot.
int queuetail; // Index of first unused slot.

int queuePopBit() {
  if (queuehead == queuetail) {
    return 3;
  }
  int bit = bitqueue[queuehead];
  queuehead = (queuehead + 1) & QUEUEMASK;
  return bit;
}

inline int queueSpace() {
  int fill = (queuetail - queuehead) & QUEUEMASK;
  return (QUEUESIZE - 1) - fill;
}

inline void queueBit(int bit) {
  bitqueue[queuetail] = bit;
  queuetail = (queuetail + 1) & QUEUEMASK;
}

void queueDelay(int count) {
  if (queueSpace() < count) return;
  for (int i = 0; i < count; i++) {
    queueBit(2);
  }
}

void queueRawCode(unsigned long code) {
  if (queueSpace() < 60) return;
  for (int i = 0; i < 32; i++) {
    queueBit((code & 0x80000000) ? 1 : 0);
    code <<= 1;
  }
  queueBit(1);
  for (int i = 0; i < 8; i++) {
    queueBit(2);
  }
}

int next_seqno;

void queueCode(unsigned long code) {
  code = applySeqno(code, next_seqno++);
  for (int i = 0; i < RETRANSMIT; i++) {
    queueRawCode(code);
  }
  queueDelay(20);
}

void queueCodeManyCopies(unsigned long code, int copies) {
  for (int i = 0; i < copies; i++) {
    code = applySeqno(code, next_seqno++);
    queueRawCode(code);
    queueDelay(20);
  }
}


void do_fan_off(unsigned long arg) {
  queueCode(CODE_Fan1);
  queueCode(CODE_FPow);
}

void do_dim_light(unsigned long arg) {
  queueCode(CODE_ColY);
  queueCodeManyCopies(CODE_LDim, 20);
}

void do_light(unsigned long arg) {
  queueCode(CODE_LPow);
  queueCode(CODE_ColW);
  queueCodeManyCopies(CODE_LBrt, 20);
}

ButtonInfo buttons[] = {
  { "FANOFF", 0,  0, 0, do_fan_off , 0 },
  { "FAN1", A5, 0, 0, queueCode, CODE_Fan1 },
  { "FAN2", A3, 0, 0, queueCode, CODE_Fan2 },
  { "FAN3", A2, 0, 0, queueCode, CODE_Fan4 },
  { "FAN4", A1, 0, 0, queueCode, CODE_Fan6 },
  { "REV", 1,  0, 0, queueCode, CODE_Revs },
  { "DIM", A4, 0, 0, do_dim_light, 0 },
  { "LIGHT", A0, 0, 0, do_light, 0 },
  { nullptr, 0, 0, 0, 0, 0 }
};

// To read the buttons, call this once per millisecond.
// The 'debounced' value will remain unchanged (hysteresis)
// until you get three consistent probes in a row (ie,
// the button has to return the same value for 3ms).
// When the debounced value falls, the button's function
// will be called.  This won't work unless you enable
// button mode first.
void callButtonFunctions() {
  for (int i = 0; buttons[i].name; i++) {
    ButtonInfo &btn = buttons[i];
    int probe = digitalRead(btn.pin) ? 1:0;
    btn.probes = (btn.probes << 1) | probe;
    if ((btn.probes & 7) == 0) {
      if (btn.debounced == 1) {
        btn.func(btn.arg);
      }
      btn.debounced = 0;
    }
    if ((btn.probes & 7) == 7) btn.debounced = 1;
  }
}

void enableInterruptMode() {
  pinMode(COMMON_BUTTON_PIN, INPUT_PULLUP);
  for (int i = 0; buttons[i].name; i++) {
    outputLowOnPin(buttons[i].pin);
  }
  LowPower.attachInterruptWakeup(digitalPinToInterrupt(COMMON_BUTTON_PIN), simple_isr, FALLING);
}

void enableButtonMode() {
  detachInterrupt(digitalPinToInterrupt(COMMON_BUTTON_PIN));
  for (int i = 0; buttons[i].name; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }
  outputLowOnPin(COMMON_BUTTON_PIN);
}

bool buttonsIdle() {
  for (int i = 0; buttons[i].name; i++) {
    if (buttons[i].probes != 255) {
      return false;
    }
  }
  return true;
}

void setup() {
  USBDevice.detach();
  enableButtonMode();

  // The LED is on when we're running (not sleeping)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  pinMode(RADIO_SPI_SELECT, OUTPUT); 
  digitalWrite(RADIO_SPI_SELECT, HIGH);
  pinMode(DIO1, OUTPUT);
  digitalWrite(DIO1, LOW);
  pinMode(DIO2, OUTPUT);
  digitalWrite(DIO2, LOW);
  SPI.beginTransaction(SPISettings());
  SPI.begin();

  RFM69writeReg(REG_OPMODE, RF_OPMODE_SEQUENCER_ON | RF_OPMODE_LISTEN_OFF | RF_OPMODE_STANDBY);
  RFM69writeReg(REG_DATAMODUL, RF_DATAMODUL_DATAMODE_CONTINUOUSNOBSYNC | RF_DATAMODUL_MODULATIONTYPE_OOK | RF_DATAMODUL_MODULATIONSHAPING_00);
  RFM69writeReg(REG_BITRATEMSB, RF_BITRATEMSB_115200);
  RFM69writeReg(REG_BITRATELSB, RF_BITRATELSB_115200);
  RFM69writeReg(REG_RXBW, RF_RXBW_DCCFREQ_010 | RF_RXBW_MANT_16 | RF_RXBW_EXP_2);
  RFM69writeReg(REG_OOKPEAK, RF_OOKPEAK_THRESHTYPE_FIXED);
  RFM69writeReg(REG_OOKFIX, 100);
  RFM69writeReg(REG_TESTDAGC, RF_DAGC_IMPROVED_LOWBETA0);
  RFM69writeReg(REG_TESTLNA, 0x2D);
  RFM69writeReg(REG_OCP, RF_OCP_OFF);
  RFM69writeReg(REG_PALEVEL, RF_PALEVEL_PA1_ON | RF_PALEVEL_PA2_ON | RF_PALEVEL_OUTPUTPOWER_11111);
  RFM69setFrequency(433.939);
  RFM69setModeStandby();
  while ((RFM69readReg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00);
  RFM69writeReg(REG_OSC1, RF_OSC1_RCCAL_START);
  while ((RFM69readReg(REG_OSC1) & RF_OSC1_RCCAL_DONE) == 0x00);
  RFM69setModeTX();
}

void sleepUntilButton() {
  RFM69setModeSleep();
  enableInterruptMode();
  digitalWrite(LED_BUILTIN, LOW);
  LowPower.sleep();
  digitalWrite(LED_BUILTIN, HIGH);
  enableButtonMode();
  RFM69setModeTX();
}

void loop() {
  // Call a button function whenever a button
  // is pressed.  Typically, button functions add
  // bits to the outgoing bit queue.
  callButtonFunctions();
  
  // Send a bit.
  int bit = queuePopBit();
  sendBit(bit);

  // Possibly sleep.
  if ((bit == 3)&&(buttonsIdle())) {
    sleepUntilButton();
    queueDelay(2);
  }
}
