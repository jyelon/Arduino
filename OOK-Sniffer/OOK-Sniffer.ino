#include "SPI.h"
#include "Arduino.h"
#include "Josh-RFM69.h"

#define PRINT_DETAILS true
#define PRINT_DECODING true

long nextexec;

void setup() {
  pinMode(RADIO_SPI_SELECT, OUTPUT); 
  digitalWrite(RADIO_SPI_SELECT, HIGH);
  pinMode(DIO1, OUTPUT);
  digitalWrite(DIO1, LOW);
  pinMode(DIO2, INPUT);
  SPI.beginTransaction(SPISettings());
  SPI.begin();

  RFM69writeReg(REG_OPMODE, RF_OPMODE_SEQUENCER_ON | RF_OPMODE_LISTEN_OFF | RF_OPMODE_STANDBY);
  RFM69writeReg(REG_DATAMODUL, RF_DATAMODUL_DATAMODE_CONTINUOUSNOBSYNC | RF_DATAMODUL_MODULATIONTYPE_OOK | RF_DATAMODUL_MODULATIONSHAPING_00);
  RFM69writeReg(REG_BITRATEMSB, RF_BITRATEMSB_115200);
  RFM69writeReg(REG_BITRATELSB, RF_BITRATELSB_115200);
  RFM69writeReg(REG_RXBW, RF_RXBW_DCCFREQ_010 | RF_RXBW_MANT_16 | RF_RXBW_EXP_1);
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
  RFM69setModeRX();
  
  Serial.begin(115200);

  long nextexec = micros() + 100;
  Serial.printf("\n\n\n\n\n\n\nQKSniff2 Operational.\n\n");
}

int sample[7];
int nextsample;
int sampletotal;

int lastbit;
long start;

int bitlen[100];
char decode[100];
int bits;

void store(long len) {
  if (bits >= 100) return;
  if (len > 9999) len = 9999;
  bitlen[bits] = len;
  bits++;
}

bool is_short(int t) {
  return (t >= 150) && (t <= 400);
}

bool is_long(int t) {
  return (t >= 600) && (t <= 1000);
}

void display() {
  // Print the data in long form.
  if (PRINT_DETAILS) {
    for (int i = 0; i < bits; i++) {
      Serial.printf("%d:%4d  ", 1-(i&1), bitlen[i]);
      if ((i & 7) == 7) Serial.printf("\n");
    }
    Serial.printf("\n");
  }

  // Print the data in short form.
  if (PRINT_DECODING) {
    int ndecode = bits/2;
    int slen = 0;
    for (int i = 0; i < ndecode; i++) {
      int ta = bitlen[i*2+0];
      int tb = bitlen[i*2+1];
      if (is_short(ta)&&is_long(tb)) {
        decode[slen++] = '0';
      } else if (is_long(ta)&&is_short(tb)) {
        decode[slen++] = '1';
      } else {
        decode[slen++] = '?';
      }
      if ((i&3)==3) decode[slen++] = ' ';
    }
    decode[slen] = 0;
    Serial.printf("DEC: %s\n", decode);
  }
}

void loop() {
  // Ensure loop iterations take at least 5us
  long mic;
  while (true) {
    mic = micros();
    if (mic - nextexec >= 0) break;
  }
  nextexec = mic + 5;

  // Smoothing/debouncing.
  sampletotal -= sample[nextsample];
  sample[nextsample] = digitalRead(DIO2) ? 1:0;
  sampletotal += sample[nextsample];
  nextsample += 1;
  if (nextsample == 7) nextsample = 0;
  int bit = sampletotal / 4;

  // State machine
  long elapsed = mic - start;
  if (bit != lastbit) {
    if ((bits > 0)||(lastbit == 1)) store(elapsed);
    start = mic;
    lastbit = bit;
    elapsed = 0;
  }
  if ((bit == 0) && (elapsed > 2000) && (bits > 0)) {
    store(2000);
    display();
    bits = 0;
  }
}
