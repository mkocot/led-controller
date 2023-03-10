#include "config.hpp"
#include "led.hpp"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncUDP.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <map>
#include <string>

using LED = LED_10B;

std::array<LED, 4> LEDS = {
    LED(D1, 0),
    LED(D2, 0),
    LED(D5, 0),
    LED(D6),
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

std::map<String, LED_ID> NAME2ID = {
    {"red", LED_RED},
    {"green", LED_GREEN},
    {"blue", LED_BLUE},
    {"white", LED_WHITE},
    {"all", LED_ALL},
};

AsyncUDP udp;

AsyncWebServer server(80);

using led_action = void(AsyncWebServerRequest *req, LED &led);
// Wrapper for led api
static std::function<void(AsyncWebServerRequest *)> api_led(const led_action f);

static void api_pwm_frequency(AsyncWebServerRequest *req);
static void api_pwm_range(AsyncWebServerRequest *req);
static void api_led_duty(AsyncWebServerRequest *req, LED &l);
static void api_led_on(AsyncWebServerRequest *req, LED &l);
static void api_led_off(AsyncWebServerRequest *req, LED &l);
static void udp_api_packet(AsyncUDPPacket packet);

static int extract_number(const AsyncWebServerRequest *req, const String &name, int &value)
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

static LED_ID extract_led_id(const AsyncWebServerRequest *req)
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

static std::function<void(AsyncWebServerRequest *)> api_led(const led_action f)
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

void api_pwm_range(AsyncWebServerRequest *req) {
    String resp;
    resp += LED::duty_max;
    req->send(200, "text/plain", resp);
}

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

  l.dutycycle(static_cast<LED::duty_type>(duty));
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

void udp_api_packet(AsyncUDPPacket packet) {
  // Expecting 4 * sizeof(LED::duty_type) bytes
  // So it's either uint32_t or uint64_t
  // [R]x[G]x[B]x[W]x
  if (packet.length() != 4 * sizeof(LED::duty_type)) {
    return;
  }

  auto data = reinterpret_cast<LED::duty_type*>(packet.data());
  LEDS[LED_RED].dutycycle(data[0]);
  LEDS[LED_GREEN].dutycycle(data[1]);
  LEDS[LED_BLUE].dutycycle(data[2]);
  LEDS[LED_WHITE].dutycycle(data[3]);
}

static void setupUdp() {
  if (!udp.listen(6969)) {
    return;
  }

  udp.onPacket(udp_api_packet);
}

static void setupAPI() {
  // OTA
  AsyncElegantOTA.begin(&server);

  // PWM
  server.on("/api/v1/pwm/frequency", HTTP_PUT, api_pwm_frequency);
  server.on("/api/v1/pwm/range", HTTP_GET, api_pwm_range);

  // LED
  server.on("/api/v1/led/duty", HTTP_GET | HTTP_PUT, api_led(api_led_duty));
  server.on("/api/v1/led/on", HTTP_GET | HTTP_PUT, api_led(api_led_on));
  server.on("/api/v1/led/off", HTTP_GET | HTTP_PUT, api_led(api_led_off));
}

// Common Arduino functions
void setup()
{
  // should be set on and another or just settings range is sufficient
  //analogWriteResolution(10);// 10bits
  analogWriteRange(LED::duty_max);
  // First ensure PWM is correctly set so we don't wait for wifi with all LEDs
  // at full power
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
  setupAPI();

  server.begin();

  // listen for udp packets
  setupUdp();
}

void loop()
{
  // NOTHING
}
