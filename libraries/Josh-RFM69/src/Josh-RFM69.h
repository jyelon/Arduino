#include "RFM69_registers.h"

// The feather comes with this connection:
#define RADIO_SPI_SELECT 8

// We added wires to the feather connecting:
//    Arduino Pin 21 <--> RFM Pin DIO1
//    Arduino Pin 20 <--> RFM Pin DIO2
#define DIO1 21
#define DIO2 20

byte RFM69readReg(byte addr)
{
  noInterrupts();
  digitalWrite(RADIO_SPI_SELECT, LOW);
  SPI.transfer(addr & 0x7F);
  byte regval = SPI.transfer(0);
  digitalWrite(RADIO_SPI_SELECT, HIGH);
  interrupts();
  return regval;
}

void RFM69writeReg(byte addr, byte value)
{
  noInterrupts();
  digitalWrite(RADIO_SPI_SELECT, LOW);
  SPI.transfer(addr | 0x80);
  SPI.transfer(value);
  digitalWrite(RADIO_SPI_SELECT, HIGH);
  interrupts();
}

void RFM69setFrequency(float x) 
{
    long frequency = (x * (1 << 14));
    RFM69writeReg(REG_FRFMSB, (byte)(frequency >> 16));
    RFM69writeReg(REG_FRFMID, (byte)(frequency >>  8));
    RFM69writeReg(REG_FRFLSB, (byte)(frequency >>  0));  
}

void RFM69setModeTX() {
      pinMode(DIO1, OUTPUT);
			RFM69writeReg(REG_OPMODE, (RFM69readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_TRANSMITTER);
      RFM69writeReg(REG_TESTPA1, 0x5D);
      RFM69writeReg(REG_TESTPA2, 0x7C);
      digitalWrite(DIO1, LOW);
}

void RFM69setModeRX() {
      pinMode(DIO1, INPUT);
			RFM69writeReg(REG_OPMODE, (RFM69readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_RECEIVER);
      RFM69writeReg(REG_TESTPA1, 0x55);
      RFM69writeReg(REG_TESTPA2, 0x70);
}

void RFM69setModeStandby() {
      pinMode(DIO1, INPUT);
			RFM69writeReg(REG_OPMODE, (RFM69readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_STANDBY);
      RFM69writeReg(REG_TESTPA1, 0x55);
      RFM69writeReg(REG_TESTPA2, 0x70);
}

void RFM69setModeSleep() {
      pinMode(DIO1, INPUT);
			RFM69writeReg(REG_OPMODE, (RFM69readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_SLEEP);
      RFM69writeReg(REG_TESTPA1, 0x55);
      RFM69writeReg(REG_TESTPA2, 0x70);
}

