#include <ESP8266React.h>
#include <LightMqttSettingsService.h>
#include <LightStateService.h>
#include <FS.h>
#include <RainbowEffect.h>
#include <ConfettiEffect.h>

#define SERIAL_BAUD_RATE 115200

AsyncWebServer server(80);
ESP8266React esp8266React(&server, &SPIFFS);
LightMqttSettingsService lightMqttSettingsService =
    LightMqttSettingsService(&server, &SPIFFS, esp8266React.getSecurityManager());

LightStateService lightStateService = LightStateService(&server,
                                                        &SPIFFS,
                                                        esp8266React.getSecurityManager(),
                                                        esp8266React.getMqttClient(),
                                                        &lightMqttSettingsService);

RainbowEffect rainbowEffect = RainbowEffect(lightStateService.getLedController());
ConfettiEffect confettiEffect = ConfettiEffect(lightStateService.getLedController());

void setup() {
  // start serial and filesystem
  Serial.begin(SERIAL_BAUD_RATE);

  // start the file system (must be done before starting the framework)
#ifdef ESP32
  SPIFFS.begin(true);
#elif defined(ESP8266)
  SPIFFS.begin();
#endif
  lightStateService.addEffect<ConfettiEffectSettings>(
      &confettiEffect, ConfettiEffectSettings::read, ConfettiEffectSettings::update);
  lightStateService.addEffect<RainbowEffectSettings>(
      &rainbowEffect, RainbowEffectSettings::read, RainbowEffectSettings::update);

  // start the framework and demo project
  esp8266React.begin();

  // load the initial light settings
  lightStateService.begin();

  // start the light service
  lightMqttSettingsService.begin();

  // start the server
  server.begin();
}

void loop() {
  // run the framework's loop function
  esp8266React.loop();

  // service the lights
  lightStateService.loop();
}
