#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>

// Define Pins. This is default setting for Sonoff basic R2
#define BUTTON 0
#define RELAY 12
#define LED 13

// thumbprint of the API server that it is going to connect to. Manual change for now.
const char fingerprint[] PROGMEM = "fe f0 5c 8e 16 b6 04 ff 13 7e 2f a7 3f d1 98 9b 66 d8 1a 72";

// Config size constants
const int TOKEN_SIZE = 36;
const int ADDRESS_SIZE = 170;
const int SSID_SIZE = 32;
const int WIFI_PASSWORD_SIZE = 32;
const int DEVICE_NAME_SIZE = 20;

ESP8266WebServer HTTP(80);
////////////////////////////////////////////////////////////////////////////////////////
//  Do not change this variables. They are set automatically once Setup is finished
////////////////////////////////////////////////////////////////////////////////////////
String callbackUrl = "";      // SmartThings API location
String callbackToken = "";    // SmartThings access token
String ssid = "";             // SSID of the router
String password = "";         // Password of wifi router
String deviceName = "";       // Device name
////////////////////////////////////////////////////////////////////////////////////////
bool lastState = HIGH;


////////////////////////////////////////////////////////////////////////////////////////
//  Operation Modes
////////////////////////////////////////////////////////////////////////////////////////
//  This device has two distinctive opration modes
//  1) Setup Mode
//     In this mode, users will be able to connect to the device directly. Once
//    connected, a setup web page will be served at http://192.168.4.1
//    Once all the necessary information is provided, the device will reboot into
//    the Switch mode
//  2) Switch Mode
//     In this mode, the device is operating like a normal smart switch. One thing
//    that is different from other smart switches is that the device is solely rely
//    on SmartThings for the operation. Aside from a physical button support, all other
//    action is designed to work with SmartThings in mind.
//     There is a set of API endpoint that your can directly call using http.
//     Keep in mind that there is no real security measure in place so use it at your
//     own risk.
////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);

  // Set pin mode
  pinMode(BUTTON, INPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(LED, OUTPUT);

  // Setup config section of flash
  EEPROM.begin(512);
  LoadConfig();

  // If SSID is not set, the device is going into a Setup mode.
  if (ssid == "")
  {
    WiFi.mode(WIFI_AP);
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    String macID = String(mac[WL_MAC_ADDR_LENGTH - 3], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    String id = "ESPSwitch" + macID;

    char ssidChar[id.length() + 1];
    memset(ssidChar, 0, id.length() + 1);

    for (int i = 0; i < id.length(); i++)
      ssidChar[i] = id.charAt(i);

    WiFi.softAP(ssidChar);

    // Serve Setup page
    HTTP.on("/", HTTP_GET, []() {
      String apSetup = "<html><head><title>Setup WIFI</title></head><body><h1>Enter Wifi Information this device will join</h1><form action=\"/update\" method=\"post\"><div>Device name(Default: ESPSwitch): <input type=\"text\" name=\"device\" /></div><div>Wifi Network Name(SSID): <input type=\"text\" name=\"id\" /></div><div>Wifi Password: <input type=\"password\" name=\"pwd\"/></div><div><input type=\"submit\" value=\"Submit\"></div></form></body></html>";
      HTTP.send(200, "text/html", apSetup);
    });

    // Process user input
    HTTP.on("/update", HTTP_POST, []() {
      if (HTTP.hasArg("device"))
        deviceName = HTTP.arg("device");
      else
        deviceName = "ESPSwitch";

      if (HTTP.hasArg("id"))
        ssid = HTTP.arg("id");

      if (HTTP.hasArg("pwd"))
        password = HTTP.arg("pwd");

      if (ssid != "" && password != "")
      {
        SaveConfig();

        HTTP.send(200, "text/plain", "Setup completed! Device will restart. Please join your network to continue.");
        delay(5000); // Give the user some time to read.
        ESP.restart();
      }
      else
        HTTP.send(500, "text/plain", "Incomplete Information. Please go back and add required information");
    });

    HTTP.begin();
  }
  else {
    // This is Operation mode
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      // API - On. Turn on the swtich. This is exposed API to the public to call to control the switch
      HTTP.on("/on", HTTP_GET, []() {
        TurnOn();
        SendStatusMessage();
      });

      // API - Off. Turn off the swtich. This is exposed API to the public to call to control the switch
      HTTP.on("/off", HTTP_GET, []() {
        TurnOff();
        SendStatusMessage();
      });

      // API - forceUpdate. this will force ESP to send the current status data to the SmartThings API. This will likely be removed
      HTTP.on("/forceUpdate", HTTP_GET, [] () {
        sendUpdate();
        String status = "";
        if (digitalRead(RELAY) == HIGH)
          status = "On";
        else
          status = "Off";
        HTTP.send(200, "text/plain", "");
      });

      // API - Status. Not sure if this is necessary. If the Device handler supports health check, this may be useful but typically it cannot be polled often which make the UI update delayed.
      HTTP.on("/status", HTTP_GET, []() {
        SendStatusMessage();
      });

      // API - Exposed Device description based on UPnP specification.
      HTTP.on("/description.xml", HTTP_GET, []() {
        SSDP.schema(HTTP.client());
      });

      // API - Updatig the endpoint location which will be called when the switch status is changed at the device level. This currently relys on the smart app to update every so often.
      //       This can be saved in the flash memory so that it can only be updated when the device is added and won't get updated.
      HTTP.on("/callback", HTTP_GET, []() {
        bool needUpdate = false;

        for (int i = 0; i < HTTP.args(); i++) {
          if (HTTP.argName(i) == "url") {
            if (callbackUrl != HTTP.arg(i))
            {
              callbackUrl = HTTP.arg(i);
              needUpdate = true;
            }
          }

          if (HTTP.argName(i) == "token")
          {
            if (callbackToken != HTTP.arg(i)) {
              callbackToken = HTTP.arg(i);
              needUpdate = true;
            }
          }
        }

        HTTP.send(200, "text/plain", "");
      });
      HTTP.begin();

      // Defining UPnP device definition for M-Search
      SSDP.setSchemaURL("description.xml");
      SSDP.setHTTPPort(80);
      SSDP.setName(deviceName);
      SSDP.setSerialNumber(WiFi.macAddress());
      SSDP.setURL("index.html");
      SSDP.setModelName("SmartThings Compatible ESP Switch");
      SSDP.setModelNumber("20191003");
      SSDP.setModelURL("https://github.com/pnjacket/SmartThings");
      SSDP.setManufacturer("David Jung");
      SSDP.setManufacturerURL("https://github.com/pnjacket/SmartThings");
      SSDP.setDeviceType("urn:schemas-upnp-org:device:Basic:1");
      SSDP.begin();

    } else {
      // Wifi connection failed. wait 10 seconds and retry
      delay(10000);
      ESP.restart();
    }
  }
}

void TurnOn() {
  digitalWrite(RELAY, HIGH);
  digitalWrite(LED, LOW);
}
void TurnOff() {
  digitalWrite(RELAY, LOW);
  digitalWrite(LED, HIGH);
}

void SendStatusMessage() {
  HTTP.send(200, "text/plain", digitalRead(RELAY) == HIGH ? "On" : "Off");
}

// Send update using SmartApps API.
void sendUpdate()
{
  if (callbackUrl != "")
  {
    String status = "";
    if (digitalRead(RELAY) == HIGH)
      status = "On";
    else
      status = "Off";

    WiFiClientSecure httpsClient;
    httpsClient.setFingerprint(fingerprint);
    httpsClient.setTimeout(15000); // 15 Seconds

    String host = callbackUrl;
    host.replace("https://", "");
    host.replace(":443", "");
    String resource = host.substring(host.indexOf('/')) + "/" + status + "?access_token=" + callbackToken;
    host = host.substring(0, host.indexOf('/'));

    httpsClient.setFingerprint(fingerprint);
    httpsClient.setTimeout(15000); // 15 Seconds
    delay(1000);

    // Not even sure if all the things below are necessary.
    int retry = 0; //retry counter
    while ((!httpsClient.connect(host, 443)) && (retry < 10)) {
      delay(300);
      retry++;
    }

    httpsClient.print(String("GET ") + resource + " HTTP/1.1\r\n" +
                      "Host: " + host + "\r\n" +
                      "Connection: close\r\n\r\n");

    while (httpsClient.connected()) {
      String line = httpsClient.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    }

    String line;
    while (httpsClient.available()) {
      line = httpsClient.readStringUntil('\n');  //Read Line by Line
    }

    httpsClient.stop();
    //// HTTP implementation
    //    HTTPClient httpClient;
    //    Serial.print("[HTTP] begin...\n");
    //    if (httpClient.begin(callbackUrl + "/" + status + "?access_token=" + callbackToken)) {  // HTTP
    //      Serial.print("[HTTP] GET...\n");
    //      // start connection and send HTTP header
    //      int httpCode = httpClient.GET();
    //
    //      // httpCode will be negative on error
    //      if (httpCode > 0) {
    //        // HTTP header has been send and Server response header has been handled
    //        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    //
    //        // file found at server
    //        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
    //          String payload = httpClient.getString();
    //        }
    //      } else {
    //        Serial.printf("[HTTP] GET... failed, error: %s\n", httpClient.errorToString(httpCode).c_str());
    //      }
    //
    //      httpClient.end();
  }
}
////////////////////////////////////////////////////////////////////////////////////////
//  Configuration helper functions
////////////////////////////////////////////////////////////////////////////////////////
void LoadConfig()
{
  String x = LoadSingleConfig(8, ADDRESS_SIZE + TOKEN_SIZE + SSID_SIZE + WIFI_PASSWORD_SIZE + DEVICE_NAME_SIZE);
  if (x == "xxxxxxxx") {
    callbackUrl = LoadSingleConfig(ADDRESS_SIZE, 0);
    callbackToken = LoadSingleConfig(TOKEN_SIZE, ADDRESS_SIZE);
    ssid = LoadSingleConfig(SSID_SIZE, ADDRESS_SIZE + TOKEN_SIZE);
    password = LoadSingleConfig(WIFI_PASSWORD_SIZE, ADDRESS_SIZE + TOKEN_SIZE + SSID_SIZE);
    deviceName = LoadSingleConfig(DEVICE_NAME_SIZE, ADDRESS_SIZE + TOKEN_SIZE + SSID_SIZE + WIFI_PASSWORD_SIZE);
  }
}

void SaveConfig()
{
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
  WriteSingleConfig(callbackUrl, ADDRESS_SIZE, 0);
  WriteSingleConfig(callbackToken, TOKEN_SIZE, ADDRESS_SIZE);
  WriteSingleConfig(ssid, SSID_SIZE, ADDRESS_SIZE + TOKEN_SIZE);
  WriteSingleConfig(password, WIFI_PASSWORD_SIZE, ADDRESS_SIZE + TOKEN_SIZE + SSID_SIZE);
  WriteSingleConfig(deviceName, DEVICE_NAME_SIZE, ADDRESS_SIZE + TOKEN_SIZE + SSID_SIZE + WIFI_PASSWORD_SIZE);
  WriteSingleConfig("xxxxxxxx", 8, ADDRESS_SIZE + TOKEN_SIZE + SSID_SIZE + WIFI_PASSWORD_SIZE+DEVICE_NAME_SIZE);
  EEPROM.commit();
}

void WriteSingleConfig(String value, int maxSize, int offset)
{
  char data[value.length() + 1];

  value.toCharArray(data, value.length() + 1);
  for (int ctr = 0; ctr < maxSize; ctr++)
  {
    if (ctr < value.length())
    {
      EEPROM.write(ctr + offset, data[ctr]);
    }
    else
    {
      EEPROM.write(ctr + offset, ' ');
    }
  }
}

String LoadSingleConfig(int maxSize, int offset)
{
  char data[maxSize];

  for (int ctr = 0; ctr < maxSize; ctr++)
  {
    data[ctr] = EEPROM.read(ctr + offset );
  }
  data[maxSize] = '\0';
  String ret = String(data);
  ret.trim();
  
  return ret;
}


////////////////////////////////////////////////////////////////////////////////////////
//  Main loop
////////////////////////////////////////////////////////////////////////////////////////
bool resetTimer = false;
unsigned long startTime;
unsigned long endTime;
void loop() {
  HTTP.handleClient();

  // The current implementation assumes that the button is momentary button.
  // Action will occur on release of the button instead of the press. This is design decision to support the press & hold action at a later time.
  if (lastState == HIGH)
  {
    if (digitalRead(BUTTON) == LOW)
    {
      lastState = LOW;
      resetTimer = true;
      startTime = millis();
    }
  }
  else
  {
    if (digitalRead(BUTTON) == HIGH)
    {
      // ACT - defining the toggle action
      digitalWrite(LED, digitalRead(RELAY));
      digitalWrite(RELAY, !digitalRead(RELAY));

      lastState = HIGH;
      resetTimer = false;
      sendUpdate();
    }
    else
    {
      // if the users hold the switch for more than 5 seconds, it will reset the SSID and going in to Setup mode.
      if (resetTimer)
      {
        endTime = millis();
        if (endTime - startTime > 5000)
        {
          ssid = "";
          SaveConfig();
          ESP.restart();
        }
      }
    }
  }
  delay(1);
}
