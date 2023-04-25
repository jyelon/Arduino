/**************************************************************************
 *
 *  @file     Adafruit_ADXL345_U.cpp
 *  @author   K.Townsend (Adafruit Industries)
 *            Modified by J. Yelon to use hardwired port access.
 *
 **************************************************************************/
#include "Arduino.h"
#include <limits.h>
#include "josh-adxl345.hpp"

/**************************************************************************
 *
 * Low level SPI routines.
 *
 **************************************************************************/

// Since we're using bit-banged SPI, we can use any pins.  These four
// pins are good since they're all on PORTB and they don't interfere
// with anything else.
inline void spi_setup_pins() {
  pinMode(9, OUTPUT);  // SCK
  pinMode(10, OUTPUT); // CS
  pinMode(11, OUTPUT); // MOSI
  pinMode(12, INPUT);  // MISO
  digitalWrite(9, HIGH);
  digitalWrite(10, HIGH);
  digitalWrite(11, HIGH);
}

inline void spi_chip_select() {
  PORTB = PORTB & (0xFF ^ 0x04);
}

inline void spi_chip_deselect() {
  PORTB = PORTB | 0x04;
}

static uint8_t spi_transfer(uint8_t data) {
  uint8_t reply = 0;
  for (int i = 7; i >= 0; i--) {
    reply <<= 1;
    PORTB = PORTB & (0xFF ^ 0x02); // SCK LOW
    uint8_t bit = (data >> i) & 1;
    PORTB = (PORTB & 0xF7) | (bit << 3); // PUT BIT ON MOSI
    PORTB = PORTB | 0x02; // SCK HIGH
    reply |= ((PINB >> 4) & 1); // READ MISO
  }
  return reply;
}

/**************************************************************************/
/*!
    @brief  Writes one byte to the specified destination register
    @param reg The address of the register to write to
    @param value The value to set the register to
*/
/**************************************************************************/
void Josh_ADXL345::writeRegister(uint8_t reg, uint8_t value) {
  spi_chip_select();
  spi_transfer(reg);
  spi_transfer(value);
  spi_chip_deselect();
}

/**************************************************************************/
/*!
    @brief Reads one byte from the specified register
    @param reg The address of the register to read from
    @returns The single byte value of the requested register
*/
/**************************************************************************/
uint8_t Josh_ADXL345::readRegister(uint8_t reg) {
  reg |= 0x80; // read byte
  spi_chip_select();
  spi_transfer(reg);
  uint8_t reply = spi_transfer(0xFF);
  spi_chip_deselect();
  return reply;
}

/**************************************************************************/
/*!
    @brief Reads two bytes from the specified register
    @param reg The address of the register to read from
    @return The two bytes read from the sensor starting at the given address
*/
/**************************************************************************/
int16_t Josh_ADXL345::read16(uint8_t reg) {
  reg |= 0x80 | 0x40; // read byte | multibyte
  spi_chip_select();
  spi_transfer(reg);
  uint16_t reply = spi_transfer(0xFF) | (spi_transfer(0xFF) << 8);
  spi_chip_deselect();
  return reply;
}

/**************************************************************************/
/*!
    @brief  Gets the most recent X,Y,Z axis values
*/
/**************************************************************************/
void Josh_ADXL345::getXYZ(int16_t *x, int16_t *y, int16_t *z) {
  *x = read16(ADXL345_REG_DATAX0);
  *y = read16(ADXL345_REG_DATAY0);
  *z = read16(ADXL345_REG_DATAZ0);
}

/**************************************************************************/
/*!
    @brief  The Constructor
*/
/**************************************************************************/

Josh_ADXL345::Josh_ADXL345() {
}

/**************************************************************************/
/*!
    @brief  Setups the HW (reads coefficients values, etc.)
    @param i2caddr The I2C address to begin communication with
    @return true: success false: a sensor with the correct ID was not found
*/
/**************************************************************************/
bool Josh_ADXL345::begin() {
  spi_setup_pins();

  uint8_t deviceid = readRegister(ADXL345_REG_DEVID);
  if (deviceid != 0xE5) {
    return false;
  }

  writeRegister(ADXL345_REG_POWER_CTL, 0x08);

  return true;
}

/**************************************************************************/
/*!
    @brief  Sets the g range for the accelerometer
    @param range The new `range_t` to set the accelerometer to
*/
/**************************************************************************/
void Josh_ADXL345::setRange(range_t range) {
  /* Read the data format register to preserve bits */
  uint8_t format = readRegister(ADXL345_REG_DATA_FORMAT);

  /* Update the data rate */
  format &= ~0x0F;
  format |= range;

  /* Make sure that the FULL-RES bit is enabled for range scaling */
  format |= 0x08;

  /* Write the register back to the IC */
  writeRegister(ADXL345_REG_DATA_FORMAT, format);
}

/**************************************************************************/
/*!
    @brief  Sets the data rate for the ADXL345 (controls power consumption)
    @param dataRate The `dataRate_t` to set
*/
/**************************************************************************/
void Josh_ADXL345::setDataRate(dataRate_t dataRate) {
  /* Note: The LOW_POWER bits are currently ignored and we always keep
     the device in 'normal' mode */
  writeRegister(ADXL345_REG_BW_RATE, dataRate);
}
