#include <Arduino.h>

#ifndef ESP32
#error This code is intended to run on the ESP32 platform!
#endif

// system includes

// Arduino includes
#include <WiFi.h>

// 3rdparty lib includes
#include <PubSubClient.h>

#define HOSTNAME "r3deskctrl"
#define MQTT_SERVER "mqtt.realraum.at"
#define MQTT_PORT 1883
#define MQTT_WILL_TOPIC "r3deskctrl/status"
#define MQTT_WILL_PAYLOAD "offline"
#define MQTT_WILL_RETAIN true
#define MQTT_WILL_QOS 1

WiFiClient espClient;
PubSubClient client{espClient};

// mqtt
bool mqttConnected{false};
void mqttCallback(char* topic, byte* payload, unsigned int length);

void setup()
{
  Serial.begin(115200);

  delay(100);

  Serial.println("Starting up...");

  WiFi.begin(PIO_WIFI_SSID, PIO_WIFI_PASSWORD);
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

  // Setup MQTT
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqttCallback);
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
}
