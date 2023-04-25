
#include "josh-adxl345.hpp"
#include "streaming-goertzel.hpp"

#define SAMPLING_FREQUENCY 1600
#define DATARATE ADXL345_DATARATE_1600_HZ
#define PUSHBUTTON_PIN A7
#define DATA_READY_PIN 7
Josh_ADXL345 sensor;
StreamingGoertzel<128, 8> resonator;
int32_t total = 0;
int32_t start_micros = 0;
int16_t lowpass = 0;
bool detected = false;

void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10);     // will pause Zero, Leonardo, etc until serial console opens

  // We configure the ADXL345 accelerometer.
  if (! sensor.begin()) { 
    Serial.println("Couldnt initialize the sensor.");
    while (1) yield();
  }
  Serial.println("Sensor initialized successfully.");
  sensor.setRange(ADXL345_RANGE_2_G);
  sensor.setDataRate(DATARATE);

  // When the ADXL345 has data ready, we configure it to pull down the INT1/D7 pin.
  // But we don't set up an actual interrupt.  Instead, we just poll the D7 pin using
  // a digitalRead.  This is a fast way to check for data ready condition.
  sensor.writeRegister(ADXL345_REG_INT_MAP, 0);
  sensor.writeRegister(ADXL345_REG_INT_ENABLE, 0x80);
  pinMode(DATA_READY_PIN, INPUT);

  // Pin A7 is a pushbutton.  It's sometimes useful for triggering debugging output.
  pinMode(PUSHBUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Record the start time.
  start_micros = micros();
}

int32_t gathered[256];
int ngathered;

void loop() {
  // Wait for DATA_READY
  while (digitalRead(DATA_READY_PIN) == 0);

  // Fetch a single sample.
  int16_t x, y, z;
  sensor.getXYZ(&x, &y, &z);

  // Remove low frequencies.
  lowpass = ((lowpass * 7) + x) / 8;
  int16_t filtered = (x - lowpass);
  
  // Scale and clamp.
  if (filtered > 30) filtered=30;
  if (filtered < -30) filtered=-30;

  // Update the frequency sensor.
  resonator.Update(int8_t(filtered));

  // Get the magnitude
  int32_t mag = resonator.Magnitude();

  total++;
  if ((total & 0x3F) == 0) {
    Serial.println(mag);
  }
  
  if (mag > 100000) {
    detected = true;
  }
  if (mag < 20000) {
    detected = false;
  }
  //digitalWrite(LED_BUILTIN, detected?1:0);
}
