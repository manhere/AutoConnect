/*
  ESP8266/ESP32 publish the RSSI as the WiFi signal strength to ThingSpeak channel.
  This example is for explaining how to use the AutoConnect library.

  In order to execute this example, the ThingSpeak account is needed. Sing up
  for New User Account and create a New Channel via My Channels.
  For details, please refer to the project page.
  https://hieromon.github.io/AutoConnect/examples/index.html#used-with-mqtt-as-a-client-application

  Also, this example uses AutoConnectAux menu customization which stored in SPIFFS.
  To evaluate this example, you upload the contents as mqtt_setting.json of
  the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu
  in Arduino IDE).

  This example is based on the thinkspeak.com environment as of Dec. 20, 2018.
  Copyright (c) 2018 Hieromon Ikasamo.
  This software is released under the MIT License.
  https://opensource.org/licenses/MIT
*/

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define GET_CHIPID()  (ESP.getChipId())
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#define GET_CHIPID()  ((uint16_t)(ESP.getEfuseMac()>>32))
#endif
#include <FS.h>
#include <PubSubClient.h>
#include <AutoConnect.h>

#define PARAM_FILE      "/param.json"
#define AUX_MQTTSETTING "/mqtt_setting"
#define AUX_MQTTSAVE    "/mqtt_save"
#define AUX_MQTTCLEAR   "/mqtt_clear"

// Adjusting WebServer class with between ESP8266 and ESP32.
#if defined(ARDUINO_ARCH_ESP8266)
typedef ESP8266WebServer  WiFiWebServer;
#elif defined(ARDUINO_ARCH_ESP32)
typedef WebServer WiFiWebServer;
#endif

AutoConnect  portal;
AutoConnectConfig config;
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);
String  serverName;
String  channelId;
String  userKey;
String  apiKey;
String  apid;
String  hostName;
unsigned int  updateInterval = 0;
unsigned long lastPub = 0;

#define MQTT_USER_ID  "anyone"

bool mqttConnect() {
  static const char alphanum[] = "0123456789"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.
  char    clientId[9];

  uint8_t retry = 10;
  while (!mqttClient.connected()) {
    if (serverName.length() <= 0)
      break;

    mqttClient.setServer(serverName.c_str(), 1883);
    Serial.println(String("Attempting MQTT broker:") + serverName);

    for (uint8_t i = 0; i < 8; i++) {
      clientId[i] = alphanum[random(62)];
    }
    clientId[8] = '\0';

    if (mqttClient.connect(clientId, MQTT_USER_ID, userKey.c_str())) {
      Serial.println("Established:" + String(clientId));
      return true;
    } else {
      Serial.println("Connection failed:" + String(mqttClient.state()));
      if (!--retry)
        break;
    }
    delay(3000);
  }
  return false;
}

void mqttPublish(String msg) {
  String path = String("channels/") + channelId + String("/publish/") + apiKey;
  int    tLen = path.length();
  char   topic[tLen];
  path.toCharArray(topic, tLen + 1);

  int    mLen = msg.length();
  char   payload[mLen];
  msg.toCharArray(payload, mLen + 1);

  mqttClient.publish(topic, payload);
}

int getStrength(uint8_t points) {
  uint8_t sc = points;
  long    rssi = 0;

  while (sc--) {
    rssi += WiFi.RSSI();
    delay(20);
  }
  return points ? static_cast<int>(rssi / points) : 0;
}

String loadParams(AutoConnectAux& aux, PageArgument& args) {
  (void)(args);
  SPIFFS.begin();
  File param = SPIFFS.open(PARAM_FILE, "r");
  if (param) {
    aux.loadElement(param);
    param.close();
  }
  else {
    Serial.println(PARAM_FILE " open failed");
    Serial.println("If you get error as 'SPIFFS: mount failed, -10025', Please modify with 'SPIFFS.begin(true)'.");
  }
  SPIFFS.end();
  return "";
}

String saveParams(AutoConnectAux& aux, PageArgument& args) {
  serverName = args.arg("mqttserver");
  serverName.trim();

  channelId = args.arg("channelid");
  channelId.trim();
  
  userKey = args.arg("userkey");
  userKey.trim();
  
  apiKey = args.arg("apikey");
  apiKey.trim();
  
  String upd = args.arg("period");
  updateInterval = upd.substring(0, 2).toInt() * 1000;

  String uniqueid = args.arg("uniqueid");

  hostName = args.arg("hostname");
  hostName.trim();
  
  // In order to retrieve the elements of /mqtt_setting,
  // it is necessary to get the AutoConnectAux object of /mqtt_setting.
  SPIFFS.begin();
  File param = SPIFFS.open(PARAM_FILE, "w");
  portal.aux("/mqtt_setting")->saveElement(param, { "mqttserver", "channelid", "userkey", "apikey", "period", "uniqueid", "hostname" });
  param.close();
  SPIFFS.end();

  // Echo back saved parameters to AutoConnectAux page.
  AutoConnectText&  echo = aux.getElement<AutoConnectText>("parameters");
  echo.value = "Server: " + serverName + "<br>";
  echo.value += "Channel ID: " + channelId + "<br>";
  echo.value += "User Key: " + userKey + "<br>";
  echo.value += "API Key: " + apiKey + "<br>";
  echo.value += "Update period: " + String(updateInterval / 1000) + " sec.<br>";
  echo.value += "Use APID unique: " + uniqueid + "<br>";
  echo.value += "ESP host name: " + hostName + "<br>";

  return "";
}

void handleRoot() {
  String  content =
    "<html>"
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "</head>"
    "<body>"
    "<iframe width=\"450\" height=\"260\" style=\"transform:scale(0.79);-o-transform:scale(0.79);-webkit-transform:scale(0.79);-moz-transform:scale(0.79);-ms-transform:scale(0.79);transform-origin:0 0;-o-transform-origin:0 0;-webkit-transform-origin:0 0;-moz-transform-origin:0 0;-ms-transform-origin:0 0;border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/454951/charts/1?bgcolor=%23ffffff&color=%23d62020&dynamic=true&type=line\"></iframe>"
    "<p style=\"padding-top:10px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>"
    "</body>"
    "</html>";

  WiFiWebServer&  server = portal.host();
  server.send(200, "text/html", content);
}

// Clear channel using Thingspeak's API.
void handleClearChannel() {
  HTTPClient  httpClient;
  String  endpoint = serverName;
  endpoint.replace("mqtt", "api");
  String  delUrl = "http://" + endpoint + "/channels/" + channelId + "/feeds.json?api_key=" + userKey;

  Serial.print("DELETE " + delUrl);
  if (httpClient.begin(delUrl)) {
    Serial.print(":");
    int resCode = httpClient.sendRequest("DELETE");
    String  res = httpClient.getString();
    httpClient.end();
    Serial.println(String(resCode) + "," + res);
  }
  else
    Serial.println(" failed");

  // Returns the redirect response. The page is reloaded and its contents
  // are updated to the state after deletion.
  WiFiWebServer&  server = portal.host();
  server.sendHeader("Location", String("http://") + server.client().localIP().toString() + String("/"));
  server.send(302, "text/plain", "");
  server.client().flush();
  server.client().stop();
}

// Load AutoConnectAux JSON from SPIFFS.
bool loadAux(const String auxName) {
  bool  rc = false;
  String  fn = auxName + ".json";
  File fs = SPIFFS.open(fn.c_str(), "r");
  if (fs) {
    rc = portal.load(fs);
    fs.close();
  }
  else
    Serial.println("SPIFFS open failed: " + fn);
  return rc;
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  SPIFFS.begin();
  loadAux(AUX_MQTTSETTING);
  loadAux(AUX_MQTTSAVE);
  SPIFFS.end();

  AutoConnectAux* setting = portal.aux(AUX_MQTTSETTING);
  if (setting) {
    PageArgument  args;
    loadParams(*setting, args);
    AutoConnectCheckbox&  uniqueidElm = setting->getElement<AutoConnectCheckbox>("uniqueid");
    AutoConnectInput&     hostnameElm = setting->getElement<AutoConnectInput>("hostname");
    if (uniqueidElm.checked) {
      config.apid = String("ESP") + "-" + String(GET_CHIPID(), HEX);
      Serial.println("apid set to " + config.apid);
    }
    if (hostnameElm.value.length()) {
      config.hostName = hostnameElm.value;
      Serial.println("hostname set to " + config.hostName);
    }
    config.bootUri = AC_ONBOOTURI_HOME;
    config.homeUri = "/";
    portal.config(config);

    portal.on(AUX_MQTTSETTING, loadParams);
    portal.on(AUX_MQTTSAVE, saveParams);
  }
  else
    Serial.println("aux. load error");

  Serial.print("WiFi ");
  if (portal.begin()) {
    Serial.println("connected:" + WiFi.SSID());
    Serial.println("IP:" + WiFi.localIP().toString());
  } else {
    Serial.println("connection failed:" + String(WiFi.status()));
    while (1) {
      delay(100);
      yield();
    }
  }

  WiFiWebServer&  server = portal.host();
  server.on("/", handleRoot);
  server.on(AUX_MQTTCLEAR, handleClearChannel);
}

void loop() {
  if (updateInterval > 0) {
    if (millis() - lastPub > updateInterval) {
      if (!mqttClient.connected()) {
        mqttConnect();
      }
      String item = String("field1=") + String(getStrength(7));
      mqttPublish(item);
      mqttClient.loop();
      lastPub = millis();
    }
  }
  portal.handleClient();
}
