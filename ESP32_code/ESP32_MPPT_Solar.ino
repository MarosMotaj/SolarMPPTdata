#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <Adafruit_ADS1X15.h>
#include<PubSubClient.h>

//MQTT Broker
const char *mqtt_broker = "broker server name or IP";
const char *topic_volts = "topic name";
const char *topic_current = "topic name";
const char *topic_power = "topic name";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;

// WiFi AP SSID
#define WIFI_SSID "ssid name"
// WiFi password
#define WIFI_PASSWORD "password"
// InfluxDB v2 server url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "IP or cloud"
// InfluxDB v2 server or cloud API token (Use: InfluxDB UI -> Data -> API Tokens -> <select token>)
#define INFLUXDB_TOKEN "token code"
// InfluxDB v2 organization id (Use: InfluxDB UI -> User -> About -> Common Ids )
#define INFLUXDB_ORG "org"
// InfluxDB v2 bucket name (Use: InfluxDB UI ->  Data -> Buckets)
#define INFLUXDB_BUCKET "SolarMPPTdata"

// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time: "PST8PDT"
//  Eastern: "EST5EDT"
//  Japanesse: "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

//Measuring values
float voltageData, currentData;
char voltageResult[10];
char currentResult[10];
char powerResult[10];

//Energy calculation
unsigned long last_time =0;
unsigned long current_time =0;
float energy = 0;

WiFiClient espClient;
PubSubClient pubSubClient(espClient); //MQTT
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
//Adafruit_ADS1015 ads;     /* Use this for the 12-bit version */
// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Data point
Point sensor("solar_system_data");

void setup() {
  Serial.begin(115200);
  //**************************************************************************
  // Initialize ADS1015
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
  }
  //**************************************************************************
  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  //**************************************************************************
  //influxDB
  // Add tags
  sensor.addTag("device", DEVICE);
  
  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
  //****************************************************************************
  //Connecting to a mqtt broker
 pubSubClient.setServer(mqtt_broker, mqtt_port);
 pubSubClient.setCallback(callback);
 
 while (!pubSubClient.connected()) {
     String client_id = "esp32-client-";
     client_id += String(WiFi.macAddress());
     Serial.printf("The client connects to mqtt broker\n");
     
     if (pubSubClient.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
         Serial.println("Broker connected");
     } else {
         Serial.print("failed with state ");
         Serial.print(pubSubClient.state());
         delay(2000);
     }
 }
}
  //****************************************************************************

void loop() {
  // Clear fields for reusing the point. Tags will remain untouched
  sensor.clearFields();

  float voltage = return_voltage_value();
  float current = return_current_value();

  //Calculating power in kW
  float power = (voltage * current)/1000; 
    
  //Calculating energy in kWh
  last_time = current_time;
  current_time = millis();
  energy = energy + (voltage * current) *((current_time - last_time) /3600000.0);
  //Calculate power in Watt-Hour // 1 Hour = 60mins x 60 Secs x 1000 Milli Secs

  //Print data on serial monitor
  Serial.print(voltage, 3); Serial.println(" V");
  Serial.print(current, 3); Serial.println(" A");
  Serial.print(power, 3); Serial.println(" kW");
  Serial.print(energy/1000, 3); Serial.println(" kWh");
  
  //Publish to MQTT broker
  //Converting values from float to char needed for MQTT
  dtostrf(voltage, 0, 2,voltageResult);
  dtostrf(current, 0, 2,currentResult);
  dtostrf(power, 0, 3,powerResult); 
  pubSubClient.publish(topic_volts, voltageResult);
  pubSubClient.publish(topic_current, currentResult);
  pubSubClient.publish(topic_power, powerResult);
  
  //Saving data to influxDB database
  sensor.addField("volt",voltage);
  sensor.addField("amper",current);
  sensor.addField("energy kWh",energy/1000);
  sensor.addField("power kW", power);

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(sensor.toLineProtocol());

  // Check WiFi connection and reconnect if needed
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }

  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  delay(1000);
}

//*******************************METHODS*******************************************************

void callback(char *topic, byte *payload, unsigned int length){
 Serial.print("Volts:");
 for (int i = 0; i < length; i++) {
     Serial.print((char) payload[i]);
 }
 Serial.print(" V");
 Serial.println();
 Serial.println("-----------------------");
}

float return_voltage_value(){
  voltageData = ads.readADC_SingleEnded(0);
  float calcVoltage = (voltageData * 12.2929 /65536.0)*236; //236 is for calculating real Volts
  return calcVoltage;  
}

float return_current_value(){
  currentData = ads.readADC_SingleEnded(1);
  float calcCurrent = ((currentData*1.102)/1000);
  return calcCurrent;
}
