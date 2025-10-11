#include <Arduino.h>

#ifndef ESP32
#error This code is intended to run on the ESP32 platform!
#endif

// system includes
#include <string>

// Arduino includes
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>

// 3rdparty lib includes
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define HOSTNAME "r3deskctrl"
#define MQTT_SERVER "mqtt.realraum.at"
#define MQTT_PORT 1883
#define MQTT_WILL_TOPIC "r3deskctrl/status"
#define MQTT_WILL_PAYLOAD "offline"
#define MQTT_WILL_RETAIN true
#define MQTT_WILL_QOS 1
#define MQTT_MAX_HEADER_SIZE 10

// buttons
#define MAX_BUTTONS 6

#define BUTTON_PRESSED LOW
#define BUTTON_RELEASED HIGH

#define DEBOUNCE_DELAY 50 // milliseconds

const int buttonPins[MAX_BUTTONS] = {13, 14, 27, 26, 25, 33};
bool buttonStates[MAX_BUTTONS] = {BUTTON_RELEASED, BUTTON_RELEASED, BUTTON_RELEASED, BUTTON_RELEASED, BUTTON_RELEASED, BUTTON_RELEASED};
bool lastButtonStates[MAX_BUTTONS] = {BUTTON_RELEASED, BUTTON_RELEASED, BUTTON_RELEASED, BUTTON_RELEASED, BUTTON_RELEASED, BUTTON_RELEASED};
unsigned long lastDebounceTimes[MAX_BUTTONS] = {0, 0, 0, 0, 0, 0};

WiFiClient espClient;
PubSubClient client{espClient};
wl_status_t wifiStatus;

// mqtt
bool mqttConnected{false};
void mqttCallback(char* topic, byte* payload, unsigned int length);

void setup()
{
  esp_log_level_set("*", ESP_LOG_INFO);        // set all components to ERROR level
  Serial.begin(115200);

  delay(100);

  Serial.println("Starting up...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);

  Serial.printf("Connecting to WiFi %s...\n", WIFI_SSID);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);

    // if millis() is greater than 1 minute, reboot
    if (millis() > 60000)
    {
      Serial.println("Rebooting...");
      ESP.restart();
      return;
    }
  }

  if (!MDNS.begin(HOSTNAME))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  else
  {
    Serial.println("MDNS responder started");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

  // Setup MQTT
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqttCallback);
  client.setBufferSize(512);

  // setup buttons
  for (int i = 0; i < MAX_BUTTONS; i++)
  {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
}

void publishButtonState(int buttonIndex, bool pressed)
{
  if (!mqttConnected)
    return;

  std::string topic = "r3deskctrl/button/" + std::to_string(buttonIndex);
  std::string payload = pressed ? "pressed" : "released";

  Serial.printf("Publishing to topic %s: %s\n", topic.c_str(), payload.c_str());

  if (client.publish(topic.c_str(), payload.c_str()) == false)
  {
    Serial.printf("Failed to publish button %d state\n", buttonIndex);
  }
  else
  {
    Serial.printf("Published button %d state\n", buttonIndex);
  }
}

void sendHomeassistantDiscovery()
{
  JsonDocument doc;

  Serial.println("Sending Home Assistant discovery messages...");

  auto createButton = [&](const int buttonIndex){
    doc.clear();

    std::string topic = "homeassistant/binary_sensor/r3deskctrl_button" + std::to_string(buttonIndex) + "/config";
    std::string unique_id = "r3deskctrl_button" + std::to_string(buttonIndex);
    std::string name = "Button " + std::to_string(buttonIndex + 1);
    std::string state_topic = "r3deskctrl/button/" + std::to_string(buttonIndex);

    auto obj = doc.to<JsonObject>();
    
    obj["name"] = name;
    obj["icon"] = "mdi:gesture-tap-button";
    obj["payload_on"] = "pressed";
    obj["payload_off"] = "released";
    obj["state_topic"] = state_topic;
    obj["unique_id"] = unique_id;

    // availability
    obj["availability_topic"] = "r3deskctrl/status";
    obj["payload_available"] = "online";
    obj["payload_not_available"] = "offline";
    
    obj["device"]["identifiers"] = WiFi.macAddress();
    obj["device"]["name"] = HOSTNAME;
    obj["device"]["model"] = "ESP32";
    obj["device"]["manufacturer"] = "realraum";

    // also include the software version from git
    obj["device"]["sw_version"] = std::string(GIT_HASH) + (std::string(GIT_DIRTY) == "dirty" ? "-dirty" : "");

    std::string payload;
    if (serializeJson(doc, payload) == 0)
    {
      Serial.println("Failed to serialize JSON");
      return;
    }

    Serial.print("Publishing discovery for button ");
    Serial.print(buttonIndex);
    Serial.print(" to topic ");
    Serial.println(topic.c_str());
    Serial.println(payload.c_str());

    if (client.publish(topic.c_str(), payload.c_str(), true) == false)
    {
      Serial.printf("Failed to publish discovery for button %d\n", buttonIndex);
    }
    else
    {
      Serial.printf("Published discovery for button %d\n", buttonIndex);
    }
  };

  for (int i = 0; i < MAX_BUTTONS; i++)
  {
    createButton(i);
  }
}

void loop()
{
  if (!WiFi.isConnected())
  {
    Serial.println("WiFi not connected!");
    delay(1000);
    return;
  }

  if (!client.connected())
  {
    Serial.println("MQTT not connected, trying to connect...");
    if (client.connect(HOSTNAME, MQTT_WILL_TOPIC, MQTT_WILL_QOS, MQTT_WILL_RETAIN, MQTT_WILL_PAYLOAD))
    {
      Serial.println("MQTT connected");
      mqttConnected = true;
      client.publish("r3deskctrl/status", "online", true);
      client.publish("r3deskctrl/git", GIT_HASH "-" GIT_DIRTY, true);
      sendHomeassistantDiscovery();

      for (int i = 0; i < MAX_BUTTONS; i++)
      {
        publishButtonState(i, buttonStates[i] == BUTTON_PRESSED);
      }
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      return;
    }
  }

  client.loop();

  // read buttons
  for (int i = 0; i < MAX_BUTTONS; i++)
  {
    int reading = digitalRead(buttonPins[i]);
    if (reading != lastButtonStates[i])
    {
      lastDebounceTimes[i] = millis();
    }

    if ((millis() - lastDebounceTimes[i]) > DEBOUNCE_DELAY)
    {
      if (reading != buttonStates[i])
      {
        buttonStates[i] = reading;
        if (buttonStates[i] == BUTTON_PRESSED)
        {
          Serial.printf("Button %d pressed\n", i);
        }
        else
        {
          Serial.printf("Button %d released\n", i);
        }

        publishButtonState(i, buttonStates[i] == BUTTON_PRESSED);
      }
    }

    lastButtonStates[i] = reading;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}