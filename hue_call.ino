#include <M5StickCPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string>
#include <M5_DLight.h>
#include <QubitroMqttClient.h>

const char* ssid       = ""; // SSID for WiFi connection
const char* password   = ""; // password for said SSID

String lights_url = "http://<INSERT PHILIPS HUE BRIDGE IP HERE>/api/<INSERT PHILIPS HUE API KEY HERE>/lights/<INSERT LIGHT NUMBER HERE>/state"; // Must specify Bridge IP, API Key and Light Number

M5_DLight sensor;
uint16_t lux;

WiFiClient wifiClient;
QubitroMqttClient mqttClient(wifiClient);

char deviceID[] = ""; // DeviceID for Qubitro Data Registry
char deviceToken[] = ""; // Device Token for Qubitro Data Registry

int press_home = 0;
int press_rst = 0;
bool lightOn = false;
bool sensorTrigger = false;

void setup() {
  M5.begin();

  sensor.begin(&Wire, 0, 26);
  sensor.setMode(CONTINUOUSLY_H_RESOLUTION_MODE);

  M5.Lcd.print("Trying to connect to:\n");
  M5.Lcd.println(ssid);
  
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }
  
  M5.Lcd.println("\nCONNECTED SUCCESSFULLY");
  
  qubitroInit();

  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0,0);

  pinMode(M5_BUTTON_HOME, INPUT_PULLUP);
  pinMode(M5_BUTTON_RST, INPUT_PULLUP);

  HTTPClient http;
  http.begin(lights_url);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode;
  httpResponseCode = http.PUT("{\"on\":false}");
  http.end();

}

void loop() {
  lux = sensor.getLUX();

  if(lux >= 5){
    if(!sensorTrigger){
      delay(3000);
      const int new_lux = lux;
      sensorTrigger = true;
      turnLight();
      getWeather();
    }
  }

  if(digitalRead(M5_BUTTON_HOME) == 0){
    if (press_home == 0) {
      sensorTrigger = false;
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(0,0);
      turnLight();
      press_home = 1;
    }
  } else {
    press_home = 0;
  }
  if(digitalRead(M5_BUTTON_RST) == 0){
    if (press_rst == 0) {
      turnLight();
      press_rst = 1;
    }
  } else {
    press_rst = 0;
  }
}

void qubitroInit(){
  char host[] = "broker.qubitro.com";
  int port = 1883;
  mqttClient.setId(deviceID);
  mqttClient.setDeviceIdToken(deviceID, deviceToken);
  M5.Lcd.println("Connecting to Qubitro...");
  if (!mqttClient.connect(host, port)){
    M5.Lcd.print("Connection failed. Error code: ");
    M5.Lcd.println(mqttClient.connectError());
    M5.Lcd.println("Visit docs.qubitro.com or create a new issue on github.com/qubitro");
  }
  M5.Lcd.println("Connected to Qubitro.");
  mqttClient.subscribe(deviceID);
  delay(5000);
}

void turnLight() {
  HTTPClient http;

  http.begin(lights_url);
  
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode;
  if (!lightOn) {
    httpResponseCode = http.PUT("{\"on\":true}");
    lightOn = true;
  } else {
    httpResponseCode = http.PUT("{\"on\":false}");
    lightOn = false;
  }
  
  if (httpResponseCode != 200) {
    String response = http.getString();
    M5.Lcd.print(httpResponseCode);
  }

  http.end();
}

void getWeather() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0,0);

  HTTPClient http;
  
  String weather_url = "http://api.weatherapi.com/v1/forecast.json?key=<INSERT WEATHER API KEY HERE>&q=<INSERT US ZIP CODE HERE>&days=1"; // Must change API key and US Zip Code for desired location

  http.begin(weather_url);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == HTTP_CODE_OK) { 
    String response = http.getString(); 
    
    DynamicJsonDocument jsonDoc(50000);
    DeserializationError error = deserializeJson(jsonDoc, response);
    
    if (error) {

      M5.Lcd.println("Error parsing JSON");
      M5.Lcd.println(error.c_str());
    
    } else {
    
      const String last_updated = jsonDoc["current"]["last_updated"];
      const String day = last_updated.substring(0, 10);
      const String condition = jsonDoc["forecast"]["forecastday"][0]["day"]["condition"]["text"];
      const int max = jsonDoc["forecast"]["forecastday"][0]["day"]["maxtemp_c"];
      const int min = jsonDoc["forecast"]["forecastday"][0]["day"]["mintemp_c"];
      const int rain = jsonDoc["forecast"]["forecastday"][0]["day"]["daily_will_it_rain"];
      M5.Lcd.print("Day: ");
      M5.Lcd.println(day);
      M5.Lcd.println();
      M5.Lcd.print("Min: ");
      M5.Lcd.print(min);
      M5.Lcd.print(" Max: ");
      M5.Lcd.println(max);
      M5.Lcd.println();

      if(rain == 0){
      
        M5.Lcd.println("No rain!");
        M5.Lcd.println();
      
      } else {
      
        M5.Lcd.println("Get a poncho!");
        M5.Lcd.println();
      
      }
      
      M5.Lcd.println(condition);

      mqttClient.poll();
      static char payload[256];
      snprintf(payload, sizeof(payload) - 1, "{\"min\":%d, \"max\":%d, \"rain\":%d, \"condition\":\"%s\"}", min, max, rain, condition);
      mqttClient.beginMessage(deviceID);
      mqttClient.print(payload); 
      mqttClient.endMessage();
    }
  } else {
    
    M5.Lcd.println("Failed to fetch data.");
  
  }
  
  http.end();

}
