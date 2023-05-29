#define SCPI_ARRAY_SYZE 3
#define SCPI_MAX_TOKENS 21
#define SCPI_MAX_COMMANDS 25
#define SCPI_HASH_TYPE uint16_t

//#define I2C_SDA_PIN 18
//#define I2C_SCL_PIN 19

#include "Adafruit_AHTX0.h"
#include "Vrekrer_scpi_parser.h"
#include "WiFi.h"
#include "Preferences.h"
#include "Wire.h"

Adafruit_AHTX0 AHT;
Adafruit_Sensor *AHT_temperature_sensor, *AHT_humidity_sensor;
Preferences preferences;
String ssid; //network SSID (name) 
String pass; //network password
String host; //Host that recieve the data
uint16_t port; //Port used to send the data
uint16_t timer; //time_interval for sendind data

SCPI_Parser my_instrument;
WiFiServer scpi_server = WiFiServer(5025);


//Timers

unsigned long main_timer = 0;
unsigned long measure_timer = 0;
unsigned long socket_timer = 0;

bool sensors_online;
float temperature_value;
float humidity_value;

void loop() {
  //Run this every 10 seconds
  //Try to connect to WIFI
  if (millis() > main_timer) {
    main_timer = millis() + 10000;
    //If not connected, try to connect to WIFI
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid.c_str(), pass.c_str());
    }
    if (!sensors_online) {
      AHT_temperature_sensor = AHT.getTemperatureSensor();
      AHT_humidity_sensor = AHT.getHumiditySensor();
    }
  }

  //Run this every 2 seconds
  //Get new Temperature and humidity data
  if (millis() > measure_timer) {
    measure_timer = millis() + 2000;
    if (sensors_online) {
      sensors_event_t humidity;
      sensors_event_t temp;
      AHT_temperature_sensor->getEvent(&temp);
      AHT_humidity_sensor->getEvent(&humidity);
      temperature_value = temp.temperature;
      humidity_value = humidity.relative_humidity;      
    } else {
      temperature_value = 0;
      humidity_value = 0;      
    }
  }

  //Process Serial Input
  my_instrument.ProcessInput(Serial, "\n");


  if (WiFi.status() != WL_CONNECTED) return;

  //Process WiFi Input
  WiFiClient scpi_client = scpi_server.available();
  if (scpi_client) {
    if ( scpi_client.connected() ) {
      my_instrument.ProcessInput(scpi_client, "\n");
    }
    else {
      scpi_client.stop();
    }
  }

  //Run this every "timer" seconds
  //Send temperature and humidity data to socket
  if ((millis() > socket_timer) && (timer > 0)) {
    socket_timer = millis() + timer*1000;
    WiFiClient socket_writer;
    if (sensors_online && socket_writer.connect(host.c_str(), port)) {
      socket_writer.print(WiFi.macAddress());
      socket_writer.write(',');
      socket_writer.print(temperature_value);
      socket_writer.write(',');
      socket_writer.print(humidity_value);
      socket_writer.write('\n');
      socket_writer.flush();
      socket_writer.stop();      
    }
  }

  if (timer == 0) socket_timer = 0;
}

void Identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.print(F("CBPF,Temperature/Humidity recorder,"));
  interface.print(ESP.getEfuseMac());
  interface.print(F(",v1.0\n"));
}

void GetTemperature(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.print(temperature_value);
  interface.write('\n');
}

void GetHumidity(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.print(humidity_value);
  interface.write('\n');
}

void GetTemperatureSensorDetails(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (sensors_online) {
    sensor_t sensor;
    AHT_temperature_sensor->getSensor(&sensor);
    PrintSensorInfo(interface, sensor);
  } else {
    interface.print(F("{Sensor:NotFound}\n"));
  }
}

void GetHumiditySensorDetails(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (sensors_online) {
    sensor_t sensor;
    AHT_humidity_sensor->getSensor(&sensor);
    PrintSensorInfo(interface, sensor);
  } else {
    interface.print(F("{Sensor:NotFound}\n"));
  }
}

void PrintSensorInfo(Stream& interface, sensor_t sensor){
  interface.print(F("{Sensor:"));
  interface.print(sensor.name);
  interface.print(F(",Driver Ver:"));
  interface.print(sensor.version);
  interface.print(F(",Unique ID:"));
  interface.print(sensor.sensor_id);
  interface.print(F(",Min Value:"));
  interface.print(sensor.min_value);
  interface.print(F(",Max Value:"));
  interface.print(sensor.max_value);
  interface.print(F(",Resolution:"));
  interface.print(sensor.resolution);
  interface.print(F("}\n"));
}

void GetIP(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  IPAddress myAddress = WiFi.localIP();
  interface.print(myAddress);
  interface.write('\n');
}

void GetGW(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  IPAddress myGateWay = WiFi.gatewayIP();
  interface.print(myGateWay);
  interface.write('\n');
}

void GetMAC(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  interface.print(WiFi.macAddress());
  interface.write('\n');
}

void GetWiFiStatus(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  switch( WiFi.status() ) {
    case WL_CONNECTED:
      interface.print(F("Connected\n"));
      break;
    case WL_IDLE_STATUS:
      interface.print(F("Idle\n"));
      break;
    case WL_NO_SSID_AVAIL:
      interface.print(F("No SSID available\n"));
      break;
    case WL_CONNECT_FAILED:
      interface.print(F("Connection failed\n"));
      break;
    case WL_CONNECTION_LOST:
      interface.print(F("Connection lost\n"));
      break;
    case WL_DISCONNECTED:
      interface.print(F("Disconnected\n"));
      break;
    default:
      interface.print(F("Unknown\n"));
  }
}

void SocketGetStatus(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  if (WiFi.status() != WL_CONNECTED) {
    interface.print("WIFI_ERROR\n");
    return;
  }
  WiFiClient socket_writer;
  if (socket_writer.connect(host.c_str(), port)) {
    interface.print("OK\n");
    socket_writer.stop();
  } else {
    interface.print("ERROR\n");
  }
}

void SetWiFiSSID(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  preferences.putString("ssid", parameters[0]);
  ssid = preferences.getString("ssid");
  WiFi.begin(ssid.c_str(), pass.c_str());
}

void GetWiFiSSID(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  interface.print(ssid);
  interface.write('\n');
}

void SetWiFiPassword(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  preferences.putString("pass", parameters[0]);
  pass = preferences.getString("pass");
  WiFi.begin(ssid.c_str(), pass.c_str());
}

void GetWiFiPassword(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  interface.print(pass);
  interface.write('\n');
}

void SocketSetHost(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  preferences.putString("host", parameters[0]);
  host = preferences.getString("host");
}

void SocketGetHost(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  interface.print(host);
  interface.write('\n');
}

void SocketSetPort(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  preferences.putUShort("port", int16_t(String(parameters[0]).toInt()));
  port = preferences.getUShort("port", 54321);
}

void SocketGetPort(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  interface.print(port);
  interface.write('\n');
}

void SetTimer(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  preferences.putUShort("timer", int16_t(String(parameters[0]).toInt()));
  timer = preferences.getUShort("timer", 5);
}

void GetTimer(SCPI_C commands, SCPI_P parameters, Stream &interface) {
  interface.print(timer);
  interface.write('\n');
}

void setup() {
  my_instrument.RegisterCommand(F("*IDN?"), &Identify);
  my_instrument.SetCommandTreeBase(F("SENSsors"));
    my_instrument.RegisterCommand(F(":TEMP?"), &GetTemperature);
    my_instrument.RegisterCommand(F(":HUMIdity?"), &GetHumidity);
    my_instrument.RegisterCommand(F(":TEMP:INFO?"), &GetTemperatureSensorDetails);
    my_instrument.RegisterCommand(F(":HUMIdity:INFO?"), &GetHumiditySensorDetails);
  my_instrument.SetCommandTreeBase(F("WIFI")); 
    my_instrument.RegisterCommand(F(":ADDRess?"), &GetIP);
    my_instrument.RegisterCommand(F(":DGATeway?"), &GetGW);
    my_instrument.RegisterCommand(F(":MAC?"), &GetMAC);
    my_instrument.RegisterCommand(F(":STATus?"), &GetWiFiStatus);
    my_instrument.RegisterCommand(F(":SSID"), &SetWiFiSSID);
    my_instrument.RegisterCommand(F(":SSID?"), &GetWiFiSSID);
    my_instrument.RegisterCommand(F(":PASSword"), &SetWiFiPassword);
    my_instrument.RegisterCommand(F(":PASSword?"), &GetWiFiPassword);
  my_instrument.SetCommandTreeBase(F("SOCKet"));
    my_instrument.RegisterCommand(F(":STATus?"), &SocketGetStatus);
    my_instrument.RegisterCommand(F(":HOST"), &SocketSetHost);
    my_instrument.RegisterCommand(F(":HOST?"), &SocketGetHost);
    my_instrument.RegisterCommand(F(":PORT"), &SocketSetPort);
    my_instrument.RegisterCommand(F(":PORT?"), &SocketGetPort);
    my_instrument.RegisterCommand(F(":TIMEr?"), &GetTimer);
    my_instrument.RegisterCommand(F(":TIMEr"), &SetTimer);
    
  delay(100);
  //Wire.begin( I2C_SDA_PIN, I2C_SCL_PIN );
  Wire.begin();
  Wire.setClock(100000UL);
  Serial.begin(115200);
  delay(100);
  
  sensors_online = false;
  sensors_online = AHT.begin(&Wire, 0, AHTX0_I2CADDR_DEFAULT);

  preferences.begin("memory", false);
  ssid = preferences.getString("ssid", "no_ssid"); 
  pass = preferences.getString("pass", "no_pass");
  host = preferences.getString("host", "no_host");
  port = preferences.getUShort("port", 54321);
  timer = preferences.getUShort("timer", 5);
  //my_instrument.PrintDebugInfo(Serial);
}
