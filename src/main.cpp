#include "config.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <map>
#include <string>

class LED
{
private:
  uint8_t pin;
  // TODO(m): Set to 0 if done fiddling with it
  uint16_t duty{128};
  bool is_enabled{false};

public:
  LED(uint8_t _pin) : pin(_pin) {}

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

enum LED_ID
{
  LED_RED = 0,
  LED_GREEN = 1,
  LED_BLUE = 2,
  LED_WHITE = 3,
  LED_ALL = -1,
  LED_NONE = -2,
};

std::array<LED, 4> LEDS = {
    LED(D1),
    LED(D2),
    LED(D5),
    LED(D6),
};

std::map<String, LED_ID> NAME2ID = {
    {"red", LED_RED},
    {"green", LED_GREEN},
    {"blue", LED_BLUE},
    {"white", LED_WHITE},
    {"all", LED_ALL},
};

AsyncWebServer server(80);


using led_action = void(AsyncWebServerRequest *req, LED &led);

static std::function<void(AsyncWebServerRequest *)> api_led(led_action f);

static void api_pwm_frequency(AsyncWebServerRequest *req);
static void api_pwm_range(AsyncWebServerRequest *req);
static void api_led_duty(AsyncWebServerRequest *req, LED &l);
static void api_led_on(AsyncWebServerRequest *req, LED &l);
static void api_led_off(AsyncWebServerRequest *req, LED &l);

static int extract_number(AsyncWebServerRequest *req, const String &name, int &value)
{
  auto param = req->getParam(name);
  if (param == nullptr)
  {
    return 1;
  }
  auto str = param->value();
  char *str_end;
  value = std::strtol(str.c_str(), &str_end, 10);
  if (str.c_str() == str_end || errno == ERANGE) {
    return 1;
  }
  return 0;
}

static LED_ID extract_led_id(AsyncWebServerRequest *req)
{
  auto name = req->getParam("name");
  if (name == nullptr)
  {
    return LED_NONE;
  }

  auto possible_name = NAME2ID.find(name->value());
  if (possible_name == NAME2ID.end())
  {
    return LED_NONE;
  }

  return possible_name->second;
}

using led_action = void(AsyncWebServerRequest *req, LED &led);

static std::function<void(AsyncWebServerRequest *)> api_led(led_action f)
{
  return [f](AsyncWebServerRequest *req)
  {
    auto id = extract_led_id(req);
    switch (id)
    {
    case LED_ALL:
      for (auto &led : LEDS)
      {
        f(req, led);
      }
      return;
    case LED_RED:
    // fallthrough
    case LED_GREEN:
    // fallthrough
    case LED_BLUE:
    // fallthrough
    case LED_WHITE:
      f(req, LEDS.at(id));
      return;
    case LED_NONE:
    // fallthrough
    default:
      // error
      req->send(400, "text/plain", "Invalid or missing 'name'");
      return;
    }
  };
}

void api_pwm_range(AsyncWebServerRequest *req) {}
void api_pwm_frequency(AsyncWebServerRequest *req)
{
  if (req->method() != HTTP_PUT)
  {
    return;
  }

  int freq;
  if (extract_number(req, "value", freq) || freq < 100 || freq > 40000) {
    req->send(400, "text/plain", "Invalid or missing 'value'");
    return;
  }

  analogWriteFreq(freq);
  req->send(200, "text/plain", "OK");
}

void api_led_duty(AsyncWebServerRequest *req, LED &l)
{
  if (req->method() == HTTP_GET)
  {
    String resp;
    resp += l.dutycycle();
    req->send(200, "text/plain", resp);
    return;
  }

  int duty = 0;
  if (extract_number(req, "value", duty)) {
    req->send(400, "text/plain", "Invalid or missing 'value'");
    return;
  }

  duty &= 0xFF;
  l.dutycycle(static_cast<uint8_t>(duty));
  req->send(200, "text/plain", "OK");
}

void api_led_on(AsyncWebServerRequest *req, LED &l)
{
  if (req->method() == HTTP_GET) {
    req->send(200, "text/plain", l.is_on() ? "1" : "0");
    return;
  }

  l.on();
  req->send(200, "text/plain", "OK");
}

void api_led_off(AsyncWebServerRequest *req, LED &l)
{
  if (req->method() == HTTP_GET) {
    req->send(200, "text/plain", l.is_on() ? "0" : "1");
    return;
  }

  l.off();
  req->send(200, "text/plain", "OK");
}

void setup()
{
  for (auto &led : LEDS)
  {
    led.begin();
  }
  Serial.begin(9600);
  delay(1000);
  Serial.println("Serial done");
  // connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wait for wifi");
    delay(500);
  }
  Serial.println("wifi ok");
  Serial.println(WiFi.localIP());

  // prepare server
  AsyncElegantOTA.begin(&server);

  // PWM
  server.on("/api/v1/pwm/frequency", HTTP_PUT, api_pwm_frequency);
  server.on("/api/v1/pwm/range", HTTP_PUT, api_pwm_range);

  // LED
  server.on("/api/v1/led/duty", HTTP_GET | HTTP_PUT, api_led(api_led_duty));
  server.on("/api/v1/led/on", HTTP_GET | HTTP_PUT, api_led(api_led_on));
  server.on("/api/v1/led/off", HTTP_GET | HTTP_PUT, api_led(api_led_off));
  server.begin();
}

void loop()
{
  // NOTHING
}