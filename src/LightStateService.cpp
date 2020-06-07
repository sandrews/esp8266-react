#include <LightStateService.h>

LightStateService::LightStateService(AsyncWebServer* server,
                                     FS* fs,
                                     SecurityManager* securityManager,
                                     AsyncMqttClient* mqttClient,
                                     LightMqttSettingsService* lightMqttSettingsService) :
    _server(server),
    _fs(fs),
    _securityManager(securityManager),
    _httpEndpoint(std::bind(&LightStateService::readForService, this, std::placeholders::_1, std::placeholders::_2),
                  std::bind(&LightStateService::updateFromService, this, std::placeholders::_1, std::placeholders::_2),
                  this,
                  server,
                  LIGHT_SETTINGS_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttPubSub(std::bind(&LightStateService::readForHa, this, std::placeholders::_1, std::placeholders::_2),
                std::bind(&LightStateService::updateFromHa, this, std::placeholders::_1, std::placeholders::_2),
                this,
                mqttClient),
    _webSocket(std::bind(&LightStateService::readForService, this, std::placeholders::_1, std::placeholders::_2),
               std::bind(&LightStateService::updateFromService, this, std::placeholders::_1, std::placeholders::_2),
               this,
               server,
               LIGHT_SETTINGS_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttClient(mqttClient),
    _lightMqttSettingsService(lightMqttSettingsService) {
  // set up led controller
  // TODO - make controller types and pin configurable
  _ledController = &FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(_leds, NUM_LEDS);

  // configure MQTT callback
  _mqttClient->onConnect(std::bind(&LightStateService::registerConfig, this));

  // configure update handler for when the light settings change
  _lightMqttSettingsService->addUpdateHandler([&](const String& originId) { registerConfig(); }, false);

  // configure settings service update handler to update LED state
  addUpdateHandler([&](const String& originId) { onConfigUpdated(); }, false);
}

CLEDController* LightStateService::getLedController() {
  return _ledController;
}

void LightStateService::begin() {
  FastLED.setMaxPowerInMilliWatts(10000);
  _state.ledOn = DEFAULT_LED_STATE;
  _state.color = CRGB::White;
  _state.brightness = 100;
  onConfigUpdated();
}

void LightStateService::onConfigUpdated() {
  _refresh = true;
}

void LightStateService::loop() {
  LightEffect* effect = _state.effect;
  if (_refresh) {
    FastLED.setBrightness(_state.brightness);
    if (effect == nullptr) {
      if (_state.ledOn) {
        fill_solid(_leds, NUM_LEDS, _state.color);
        FastLED.show();
      } else {
        _ledController->clearLeds(NUM_LEDS);
      }
    }
    _refresh = false;
  }
  if (effect != nullptr) {
    effect->loop();
  }
}

void LightStateService::registerConfig() {
  if (!_mqttClient->connected()) {
    return;
  }
  String configTopic;
  String subTopic;
  String pubTopic;

  DynamicJsonDocument doc(1024);
  _lightMqttSettingsService->read([&](LightMqttSettings& settings) {
    configTopic = settings.mqttPath + "/config";
    subTopic = settings.mqttPath + "/set";
    pubTopic = settings.mqttPath + "/state";
    doc["~"] = settings.mqttPath;
    doc["name"] = settings.name;
    doc["unique_id"] = settings.uniqueId;
  });
  doc["cmd_t"] = "~/set";
  doc["stat_t"] = "~/state";
  doc["schema"] = "json";
  doc["brightness"] = true;
  doc["rgb"] = true;
  doc["effect"] = true;

  JsonArray effectList = doc.createNestedArray("effect_list");
  effectList.add("Color");
  for (auto const& effectPtr : _lightEffects) {
    effectList.add(effectPtr.get()->getId());
  }
  String payload;
  serializeJson(doc, payload);
  _mqttClient->publish(configTopic.c_str(), 0, false, payload.c_str());

  _mqttPubSub.configureTopics(pubTopic, subTopic);
}
