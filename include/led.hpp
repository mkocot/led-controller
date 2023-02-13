#ifndef KOCIOLEK_LED_CONTROLLER_LED_H
#define KOCIOLEK_LED_CONTROLLER_LED_H

#include <Arduino.h>

#include <type_traits>

template<int RANGE>
class LED_T
{
  public:
  // use uint8_t up to (inclusive) uint8_t max
  using duty_type = typename std::conditional<(RANGE <= std::numeric_limits<uint8_t>::max()), uint8_t, uint16_t>::type;
  static constexpr auto duty_max = RANGE;
private:
  uint8_t pin;
  // TODO(m): Set to 0 if done fiddling with it
  duty_type duty{RANGE / 2};
  bool is_enabled{false};

public:

  LED_T(uint8_t _pin) : pin(_pin) {}
  LED_T(uint8_t _pin, duty_type _duty): pin(_pin), duty(_duty) {}

  void begin()
  {
    on();
  }

  // void brightness(float _brightness)
  // {
  //   log10f(brightness) * 255;
  // }

  void dutycycle(const duty_type _duty)
  {
    duty = _duty;
    on();
  }

  duty_type dutycycle() const
  {
    return duty;
  }

  // Frequency is shared, no way around it
  static int frequency(uint16_t _freq)
  {
    if (_freq < 100 || _freq > 40000)
    {
      return 1;
    }
    analogWriteFreq(_freq);
    return 0;
  }

  void on()
  {
    analogWrite(pin, duty);
    is_enabled = true;
  }

  void off()
  {
    analogWrite(pin, 0);
    is_enabled = false;
  }

  bool is_on() const {
    return is_enabled;
  }
};

using LED_8B = LED_T<255>;
using LED_10B = LED_T<1023>;

#endif
