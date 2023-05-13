/*
   MQTT Light for Home-Assistant - NodeMCU (ESP8266)
   https://home-assistant.io/components/light.mqtt/

   Libraries :
    - ESP8266 core for Arduino : https://github.com/esp8266/Arduino
    - PubSubClient : https://github.com/knolleary/pubsubclient

   Sources :
    - File > Examples > ES8266WiFi > WiFiClient
    - File > Examples > PubSubClient > mqtt_auth
    - File > Examples > PubSubClient > mqtt_esp8266

   Schematic :
    - https://github.com/mertenats/open-home-automation/blob/master/ha_mqtt_light/Schematic.png
    - GND - LED - Resistor 220 Ohms - D1/GPIO5

   Configuration (HA) :
    sensor:
      - name: "Temperature Sensor"
        state_topic: "/temperatura"
        icon: mdi:thermometer-lines
        unit_of_measurement: "      C"
    light:
      - name: "Luz Cuarto"
        state_topic: 'cuarto/light1/status'
        command_topic: 'cuarto/light1/switch'
        optimistic: false

    switch:
      - unique_id: fan_switch
        name: "Ventilador Cuarto"
        state_topic: "cuarto/fan1/status"
        command_topic: "cuarto/fan1/switch"
        optimistic: false
        icon: mdi:fan


   Samuel M. - v1.1 - 08.2016
   If you like this example, please add a star! Thank you!
   https://github.com/mertenats/open-home-automation
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h" // Click here to get the library: http://librarymanager/All#DHTesp
#include <ArduinoJson.h>

#define MQTT_VERSION MQTT_VERSION_3_1_1

// Wifi: SSID and password
const char *WIFI_SSID = "FamiliaLondono";
const char *WIFI_PASSWORD = "Cont301574";

// MQTT: ID, server IP, port, username and password
const PROGMEM char *MQTT_CLIENT_ID = "luz_cuarto";
const PROGMEM char *MQTT_SERVER_IP = "192.168.1.109";
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;
const PROGMEM char *MQTT_USER = "pi";
const PROGMEM char *MQTT_PASSWORD = "raspberry";

// MQTT: topics
const char *MQTT_LIGHT_STATE_TOPIC = "cuarto/light1/status";
const char *MQTT_LIGHT_COMMAND_TOPIC = "cuarto/light1/switch";
const char *MQTT_FAN_STATE_TOPIC = "cuarto/fan1/status";
const char *MQTT_FAN_COMMAND_TOPIC = "cuarto/fan1/switch";
// make 2 chars const for h and t with value "cuarto/temp1"
const char *MQTT_TEMP_TOPIC = "cuarto/temphumm1";

// payloads by default (on/off)
const char *LIGHT_ON = "ON";
const char *LIGHT_OFF = "OFF";
const char *FAN_ON = "ON";
const char *FAN_OFF = "OFF";

/// vars ///////////////
const PROGMEM uint8_t LED_PIN = 5;     // Wemos D1
const PROGMEM uint8_t BTN_PIN = 2;     // Wemos D4
const PROGMEM uint8_t FAN_PIN = 13;    // Wemos D7
const PROGMEM uint8_t BTNFan_PIN = 12; // Wemos D6
const PROGMEM uint8_t DHT_PIN = 4;     // Wemos D2
boolean m_light_state = false;         // light is turned off by default
boolean prev_lightbtn_st = false;      // previous button state
boolean m_fan_state = false;           // fan is turned off by default
boolean prev_fanbtn_st = false;        // previous button state

///  timers /////////////
unsigned long prev_btn_ms = 0;    // ms
unsigned long prev_btnfan_ms = 0; // ms
unsigned long prev_dht_ms = 0;    // ms

WiFiClient wifiClient;
PubSubClient client(wifiClient);
DHTesp dht;

// function called to publish the state of the light (on/off)
void publishLightState()
{
  if (m_light_state)
  {
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_ON, true);
  }
  else
  {
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_OFF, true);
  }
}
void publishfantState()
{
  if (m_fan_state)
  {
    Serial.print("publicando estado de FAN, topic y estado ");
    Serial.print(MQTT_FAN_STATE_TOPIC);
    Serial.print(", ");
    Serial.println(FAN_ON);
    client.publish(MQTT_FAN_STATE_TOPIC, FAN_ON, true);
  }
  else
  {
    Serial.print("publicando estado de FAN, topic y estado ");
    Serial.print(MQTT_FAN_STATE_TOPIC);
    Serial.print(", ");
    Serial.println(FAN_OFF);
    client.publish(MQTT_FAN_STATE_TOPIC, FAN_OFF, true);
  }
}

void publishData(float p_temperature, float p_humidity)
{
  // create a JSON object
  // doc : https://github.com/bblanchon/ArduinoJson/wiki/API%20Reference
  DynamicJsonDocument doc(1024);
  // INFO: the data must be converted into a string; a problem occurs when using floats...
  doc["temperature"] = (String)p_temperature;
  doc["humidity"] = (String)p_humidity;
  serializeJson(doc, Serial);
  Serial.println("");
  /*
    {
        "temperature": "23.20" ,
        "humidity": "43.70"
    }
  */
  char data[200];
  serializeJson(doc, data);
  client.publish(MQTT_TEMP_TOPIC, data, true);
  yield();
}
// function called to turn on/off the light
void setLightState()
{
  if (m_light_state)
  {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("INFO: Turn light on...");
  }
  else
  {
    digitalWrite(LED_PIN, LOW);
    Serial.println("INFO: Turn light off...");
  }
}
void setFanState()
{
  if (m_fan_state)
  {
    digitalWrite(FAN_PIN, HIGH);
    Serial.println("INFO: Turn fan on...");
  }
  else
  {
    digitalWrite(FAN_PIN, LOW);
    Serial.println("INFO: Turn fan off...");
  }
}

// function called when a MQTT message arrived
void callback(char *p_topic, byte *p_payload, unsigned int p_length)
{
  // concat the payload into a string
  Serial.print("mensaje recibido con Topic: ");
  // Serial.print(((char)p_payload[0,5]));
  Serial.println(p_topic);
  String payload;
  for (uint8_t i = 0; i < p_length; i++)
  {
    payload.concat((char)p_payload[i]);
  }
  Serial.print("payload recibido: ");
  Serial.println(payload);
  // handle message topic
  if (String(MQTT_LIGHT_COMMAND_TOPIC).equals(p_topic))
  {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(LIGHT_ON)))
    {
      if (m_light_state != true)
      {
        m_light_state = true;
        setLightState();
        publishLightState();
      }
    }
    else if (payload.equals(String(LIGHT_OFF)))
    {
      if (m_light_state != false)
      {
        m_light_state = false;
        setLightState();
        publishLightState();
      }
    }
  }
  if (String(MQTT_FAN_COMMAND_TOPIC).equals(p_topic))
  {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(FAN_ON)))
    {
      if (m_fan_state != true)
      {
        m_fan_state = true;
        setFanState();
        publishfantState();
      }
    }
    else if (payload.equals(String(FAN_OFF)))
    {
      if (m_fan_state != false)
      {
        m_fan_state = false;
        setFanState();
        publishfantState();
      }
    }
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.println("INFO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD))
    {
      Serial.println("INFO: connected");
      // Once connected, publish an announcement...
      publishLightState();
      publishfantState();
      // ... and resubscribe
      client.subscribe(MQTT_LIGHT_COMMAND_TOPIC);
      client.subscribe(MQTT_FAN_COMMAND_TOPIC);
    }
    else
    {
      Serial.print("ERROR: failed, rc=");
      Serial.print(client.state());
      Serial.println("DEBUG: try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  // init the serial
  Serial.begin(115200);

  // init the led
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BTNFan_PIN, INPUT);
  analogWriteRange(255);
  setLightState();
  setFanState();

  // init the WiFi connection
  Serial.println();
  Serial.println();
  Serial.print("INFO: Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  dht.setup(DHT_PIN, DHTesp::DHT22); // Connect DHT sensor to GPIO 17
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("INFO: WiFi connected");
  Serial.print("INFO: IP address: ");
  Serial.println(WiFi.localIP());

  // init the MQTT connection
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);
}

void loop()
{
  // timer with millis() every 2 seconds for dht readings
  if (millis() - prev_dht_ms > 2000)
  {
    prev_dht_ms = millis();
    float h = dht.getHumidity();
    float t = dht.getTemperature();
    if (isnan(h) || isnan(t))
    {
      Serial.println("ERROR: DHT reading failed");
    }
    else
    {
      Serial.print("INFO: Temperature: ");
      Serial.print(t);
      Serial.println("C");
      Serial.print("INFO: Humidity: ");
      Serial.print(h);
      Serial.println("%");
      publishData(t, h);
    }
  }
  // check if the light button is pressed
  if (digitalRead(BTN_PIN) != prev_lightbtn_st)
  {
    if (prev_lightbtn_st && millis() - prev_btn_ms > 200)
    {
      prev_btn_ms = millis();
      m_light_state = !m_light_state;
      setLightState();
      publishLightState();
    }
  }
  // check if the fan button is pressed same as light button
  if (digitalRead(BTNFan_PIN) != prev_fanbtn_st)
  {
    if (prev_fanbtn_st && millis() - prev_btnfan_ms > 200)
    {
      prev_btnfan_ms = millis();
      m_fan_state = !m_fan_state;
      setFanState();
      publishfantState();
    }
  }
  if (!client.connected())
  {
    reconnect();
  }
  prev_lightbtn_st = digitalRead(BTN_PIN);
  prev_fanbtn_st = digitalRead(BTNFan_PIN);
  client.loop();
}