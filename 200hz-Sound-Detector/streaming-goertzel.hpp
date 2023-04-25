
// StreamingGoertzel
//
// Useful when you have a stream of samples coming from a data source,
// and you want to check for the presence of a particular frequency in the
// samples.  This filter will tell you the amplitude of the target frequency.
// Each time you feed it a sample, it updates its amplitude estimate.
//
// The way it works is this: it maintains a rotating vector called the "clock
// vector."  Imagine the second hand of a clock - it's a vector of a fixed
// length that just keeps revolving.  The clock hand revolves at N revolutions
// per second where N is the target frequency you're searching for.  Each time
// you feed this filter a sample, it multiplies the sample (a scalar) times the
// clock vector to get a sample vector.  Then, it maintains an accurate sum of
// the last N sample vectors.  This running sum is called the "accumulator."
// The magnitude of the accumulator vector is the amplitude of your target
// frequency.
//
// This class calculates a sum over the last WINDOW samples.  When a new sample
// is added to the window, the oldest sample is removed from the window, and the
// oldest sample is deducted from the accumulator.
//
// To use this class, the sample rate must be a multiple of the target frequency.
// To configure the target frequency, take the sample rate and divide it by
// the target frequency.  The result is the DIVISOR.
//
// To determine the magnitude of the result vector, we provide a magnitude
// function which is an approximation.  The approximation is within 3% of correct.
// For most uses, this is plenty accurate.
//
// About bit-lengths: The input samples are in the range +-128.  The clock vector
// has a range of +-127.  Multiplying the two together yields a number in
// the range +-16256.  So vectorized samples fit neatly within 16 bits.
// The accumulator can add up 256 vectorized samples, yielding values
// that do not exceed +-4,161,536.  The magnitude routine briefly multiplies
// the accumulator values by 32, which doesn't exceed +-133,169,152.  Therefore,
// the magnitude routine cannot overflow a 32-bit number.
//

#ifndef STREAMING_GOERTZEL
#define STREAMING_GOERTZEL

template <int WINDOW, int DIVISOR>
class StreamingGoertzel
{
  int8_t sin_table_[DIVISOR];
  int8_t cos_table_[DIVISOR];
  int16_t x_buffer_[WINDOW];
  int16_t y_buffer_[WINDOW];
  int32_t acc_x_;
  int32_t acc_y_;
  uint8_t bufpos_;
  uint8_t phase_;
  
public:
  StreamingGoertzel() {
    if (WINDOW > 256) {
      Serial.print("WINDOW cannot exceed 256");
      while (true);
    }
    if (DIVISOR > 256) {
      Serial.print("DIVISOR cannot exceed 256");
      while (true);
    }
    for (int i = 0; i < DIVISOR; i++) {
      float angle = (2.0 * PI * i) / DIVISOR;
      sin_table_[i] = int8_t(round(sin(angle) * 127.0));
      cos_table_[i] = int8_t(round(cos(angle) * 127.0));
    }
    for (int i = 0; i < WINDOW; i++) {
      x_buffer_[i] = 0;
      y_buffer_[i] = 0;
    }
    phase_ = 0;
    bufpos_ = 0;
    acc_x_ = 0;
    acc_y_ = 0;
  }

  void Update(int8_t sample) {
    // Obtain a rotating clock vector.  This turns at a steady pace.
    int8_t clock_x = cos_table_[phase_];
    int8_t clock_y = sin_table_[phase_];

    // Set up for the next phase.
    phase_ += 1;
    if (phase_ == DIVISOR) phase_ = 0;

    // Convert the sample to a vector.  Max possible value is 128 * 127 = 16256
    int16_t vec_x = int16_t(sample) * clock_x;
    int16_t vec_y = int16_t(sample) * clock_y;

    // Remove the old sample from the accumulator.
    acc_x_ -= x_buffer_[bufpos_];
    acc_y_ -= y_buffer_[bufpos_];

    // Add the new sample to the accumulator.
    acc_x_ += vec_x;
    acc_y_ += vec_y;
    
    // Replace the oldest vector in the circular buffer with the new one.
    x_buffer_[bufpos_] = vec_x;
    y_buffer_[bufpos_] = vec_y;

    // Set up for the next buffer position.
    bufpos_ += 1;
    if (bufpos_ == WINDOW) bufpos_ = 0;
  }

  int32_t Magnitude() {
    int32_t ax = acc_x_;
    int32_t ay = acc_y_;
    if (ax < 0) ax = -ax;
    if (ay < 0) ay = -ay;
    int32_t opt1, opt2;
    if (ax > ay) {
      opt1 = 28*ax + 17*ay;
      opt2 = 32*ax;
    } else {
      opt1 = 28*ay + 17*ax;
      opt2 = 32*ay;
    }
    int32_t result = (opt1 > opt2) ? opt1 : opt2;
    return result >> 5;
  }
};

#endif // STREAMING_GOERTZEL
