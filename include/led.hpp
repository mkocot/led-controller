#ifndef KOCIOLEK_LED_CONTROLLER_LED_H
#define KOCIOLEK_LED_CONTROLLER_LED_H

#include <Arduino.h>

class LED
{
private:
  uint8_t pin;
  // TODO(m): Set to 0 if done fiddling with it
  uint16_t duty{128};
  bool is_enabled{false};

public:
  LED(uint8_t _pin) : pin(_pin) {}
  LED(uint8_t _pin, uint8_t _duty): pin(_pin), duty(_duty) {}

  void begin()
  {
    on();
  }

  // void brightness(float _brightness)
  // {
  //   log10f(brightness) * 255;
  // }

  void dutycycle(const uint8_t _duty)
  {
    duty = _duty;
    on();
  }
  uint8_t dutycycle() const
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

#endif
