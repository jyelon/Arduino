// This program generates a 200hz size wave.
//
// It could probably be done to reprogram it to generate any waveform
// at any frequency.  It would take some knowledge of arduino timers,
// but it could likely be done fairly easily.
//
// To use it, you must connect a resistor ladder to output
// pins 8,9,10,11,12,13 in such a way that 13 is the most
// significant bit.
//
// When generating 200hz sine wave, the update rate will be about
// 30,000 updates per second. It probably isn't necessary to put a
// low-pass filter on the output to mask the 30,000 hz noise, but it
// couldn't hurt.
//

byte timer_postscale = 0;

void setup() {
  // Set up the serial port for debugging output.
  //
  Serial.begin(115200);
  while (!Serial) delay(10);     // will pause Zero, Leonardo, etc until serial console opens
  Serial.print("Howdy.\n");

  // Depending on the CPU clock speed, we choose one of three timer configs.
  //
  // 8  MHZ CPU: prescaler 1/64, 8 microsec timer tick, timer counts up to 625.
  // 16 MHZ CPU: prescaler 1/64, 4 microsec timer tick, timer counts up to 1250.
  // 32 MHZ CPU: prescaler 1/64, 2 microsec timer tick, timer counts up to 2500.
  //
  // Regardless, the timer cycles at a rate of 200HZ.
  //
  TIMSK1 = 0x00;
  TCCR1A = 0x00;
  TCCR1B = 0x0B;
  OCR1A = 1000;
  while (TCNT1 != 0);
  long timert0 = micros();
  while (TCNT1 != 1000);
  long timert1 = micros();
  long elapsed = timert1 - timert0;
  long limit;
  if ((elapsed >= 1999) && (elapsed < 2001)) {
    Serial.print("Timer configured for 32MHZ CPU\n");
    limit = 2500;
    timer_postscale = 2;
  } else if ((elapsed > 3999) && (elapsed <= 4001)) {
    Serial.print("Timer configured for 16MHZ CPU\n");
    limit = 1250;
    timer_postscale = 1;
  } else if ((elapsed > 7999) && (elapsed <= 8001)) {
    Serial.print("Timer configured for 8MHZ CPU\n");
    limit = 625;
    timer_postscale = 0;
  } else {
    Serial.print("Timer could not be configured.\n");
    while (true);
  }
//  limit <<= 1;
//  timer_postscale += 1;
  OCR1A = limit - 1;

  // Configure the output pins for the resistor ladder.
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
}

byte waveform[625] = {
32,32,32,32,33,33,33,34,34,34,35,35,35,36,36,36,37,37,37,37,38,38,38,39,39,
39,40,40,40,41,41,41,41,42,42,42,43,43,43,44,44,44,44,45,45,45,46,46,46,46,
47,47,47,48,48,48,48,49,49,49,49,50,50,50,50,51,51,51,51,52,52,52,52,53,53,
53,53,54,54,54,54,54,55,55,55,55,55,56,56,56,56,56,57,57,57,57,57,58,58,58,
58,58,58,59,59,59,59,59,59,60,60,60,60,60,60,60,60,61,61,61,61,61,61,61,61,
61,62,62,62,62,62,62,62,62,62,62,62,62,62,63,63,63,63,63,63,63,63,63,63,63,
63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,62,
62,62,62,62,62,62,62,62,62,62,62,62,62,61,61,61,61,61,61,61,61,61,60,60,60,
60,60,60,60,59,59,59,59,59,59,59,58,58,58,58,58,57,57,57,57,57,57,56,56,56,
56,56,55,55,55,55,55,54,54,54,54,53,53,53,53,52,52,52,52,52,51,51,51,51,50,
50,50,49,49,49,49,48,48,48,48,47,47,47,47,46,46,46,45,45,45,45,44,44,44,43,
43,43,43,42,42,42,41,41,41,40,40,40,39,39,39,39,38,38,38,37,37,37,36,36,36,
35,35,35,35,34,34,34,33,33,33,32,32,32,31,31,31,30,30,30,29,29,29,28,28,28,
28,27,27,27,26,26,26,25,25,25,24,24,24,24,23,23,23,22,22,22,21,21,21,20,20,
20,20,19,19,19,18,18,18,18,17,17,17,16,16,16,16,15,15,15,15,14,14,14,14,13,
13,13,12,12,12,12,11,11,11,11,11,10,10,10,10, 9, 9, 9, 9, 8, 8, 8, 8, 8, 7,
 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3,
 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5,
 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9,10,
10,10,10,11,11,11,11,12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,15,16,16,
16,17,17,17,17,18,18,18,19,19,19,19,20,20,20,21,21,21,22,22,22,22,23,23,23,
24,24,24,25,25,25,26,26,26,26,27,27,27,28,28,28,29,29,29,30,30,30,31,31,31
};

void loop() {
  // Fetch the current timer value, map it to a sine wave, output it via
  // the resistor ladder.
  //
  uint16_t index = TCNT1 >> timer_postscale;
  byte wave = waveform[index];
  PORTB = wave;
}
