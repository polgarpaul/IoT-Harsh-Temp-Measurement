#define DHT_DEBUG

#define TEMP_SNSR_BOARD_V3

#include <TemperatureProbes.h>
#include <Arduino.h>
#include <NuvIoT.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"


#define TEMP_SNSR_SKU "Temperature Sensor"
#define FIRMWARE_VERSION "0.5.0"
#define HARDWARE_REVISION "3.0"

#define BATT_SENSE_PIN 27
#define TEMP_SENSE_PIN 13

#define IO1_PIN 25
#define IO2_PIN 26

bool hasDHT22 = false;
bool hasProbe1 = false;
bool hasProbe2 = false;

DHT *dht;
DallasTemperature *probe1;
DallasTemperature *probe2;

byte buffer[8];
bool running = true;

void determineSensorConfiguration()
{
  dht = new DHT(IO1_PIN, DHT22);
  int retryCount = 0;

  dht->begin();
  while (retryCount++ < 5 && !hasDHT22)
  {
    float temp = dht->readTemperature(true,true);
    if (!isnan(temp))
    {
      hasDHT22 = true;
      console.println("Found DHT22");
    }
    else {
      console.println("Attempt " + String(retryCount));

      delay(2250);
  }
  }

  if (!hasDHT22)
  {
    console.println("Did Not Find DHT22");
  
    delete dht;
    dht = NULL;
  }
  else
  {
    console.println("Found DHT22");
  
    ioConfig.GPIO1Config = GPIO_CONFIG_DHT22;
    ioConfig.GPIO1Name = "Digital Temperature";
    ioConfig.GPIO1Scaler = 1;
    ioConfig.GPIO1Zero = 0;
    ioConfig.GPIO1Calibration = 1;

    ioConfig.GPIO2Config = GPIO_CONFIG_DHT22_HUMIDITY;
    ioConfig.GPIO2Name = "Digital Humidity";
    ioConfig.GPIO2Scaler = 1;
    ioConfig.GPIO2Zero = 0;
    ioConfig.GPIO2Calibration = 1;
  }

  if (dht == NULL)
  {
    probe1 = new DallasTemperature(new OneWire(IO1_PIN));
    retryCount = 0;
    while (retryCount++ < 5 && !hasProbe1)
    {
      float temp = probe1->getTempFByIndex(0);
      if (!isnan(temp) && temp != -196.60f)
      {
        hasProbe1 = true;
      }
    }

    if (!hasProbe1)
    {
      probe1 = NULL;
      console.println("Does not have DS18B Probe 1");
    }
    else
    {
      console.println("Has DS18B Probe 1");
      ioConfig.GPIO3Config = GPIO_CONFIG_DBS18;
      if (ioConfig.GPIO3Name == "")
        ioConfig.GPIO3Name = "Digital Temperature - Port 1";
      ioConfig.GPIO3Scaler = 1;
      ioConfig.GPIO3Zero = 0;
      ioConfig.GPIO3Calibration = 1;
    }

    probe2 = new DallasTemperature(new OneWire(IO2_PIN));
    retryCount = 0;
    while (retryCount++ < 5 && !hasProbe2)
    {
      float temp = probe2->getTempFByIndex(0);
      if (!isnan(temp) && temp != -196.60f)
      {
        hasProbe2 = true;
      }
    }

    if (!hasProbe2)
    {
      probe2 = NULL;
      console.println("Does not have DS18B Probe 2");
    }
    else
    {
      console.println("Has DS18B Probe 2");
      ioConfig.GPIO4Config = GPIO_CONFIG_DBS18;
      if (ioConfig.GPIO4Name == "")
        ioConfig.GPIO4Name = "Digital Temperature - Port 2";
      ioConfig.GPIO4Scaler = 1;
      ioConfig.GPIO4Zero = 0;
      ioConfig.GPIO4Calibration = 1;
    }

    ioConfig.ADC1Config = ADC_CONFIG_ADC;
    ioConfig.ADC1Name = "Analog Temperature";
    ioConfig.ADC1Scaler = 100;
    ioConfig.ADC1Zero = 32;
    ioConfig.ADC1Calibration = 1;
  }
}

BLE BT(&console, &hal, &state, &ioConfig, &sysConfig, &relayManager, payload);

void setup()
{  
  delay(1000);

  configureFileSystem();

  ioConfig.load();
  sysConfig.load();
  sysConfig.WiFiEnabled = true;
  sysConfig.SendUpdateRate = 5000;
  state.init(TEMP_SNSR_SKU, FIRMWARE_VERSION, HARDWARE_REVISION, "pcl001", 010);
  initPins();

  // ledManager.setup(&ioConfig);

  configureConsole();
  console.registerCallback(handleConsoleCommand);
  welcome(TEMP_SNSR_SKU, FIRMWARE_VERSION);

  String btName = "NuvIoT - " + (sysConfig.DeviceId == "" ? "Temp Sensor" : sysConfig.DeviceId);

  BT.begin(btName.c_str(), "TMPS-001");

  wifiMgr.setup();

  determineSensorConfiguration();
}

int nextPrint = 0;
int idx = 0;

long nextSend = 0;

void loop()
{
  uint32_t adcCountsBatt = 0;
  uint32_t adcCountsTemperature = 0;

  for (int idx = 0; idx < 10; ++idx)
  {
    //  adcCountsBatt += analogRead(27);
    // adcCountsTemperature += analogRead(TEMP_SENSE_PIN);
    delay(10);
  }

  console.loop();
  ledManager.loop();
  wifiMgr.loop();
  BT.update();

  if (hasDHT22)
  {
    ioValues.setValue(0, dht->readTemperature(true));
    ioValues.setValue(1, dht->readHumidity());
  }

  if (hasProbe1)
  {
    probe1->requestTemperatures();
    delay(250);
    ioValues.setValue(2, probe1->getTempFByIndex(0));
    delay(250);
  }

  if (hasProbe2)
  {
    probe2->requestTemperatures();
    delay(250);
    ioValues.setValue(3, probe2->getTempFByIndex(0));
    delay(250);
  }

  if(wifiMgr.isConnected() && millis() > nextSend)
  {
    String path = "/sensor/" + sysConfig.DeviceId + "/temperature";
    String json = "";
    if(hasDHT22)
      json = "{'temperature':" + String(ioValues.getValue(0) + ",'humidity':" + String(ioValues.getValue(1)) + "}");
    else if(hasProbe1 && hasProbe2) {
      json = "{'temperature':" + String(ioValues.getValue(2) + ",'temperature2':" + String(ioValues.getValue(3)) + "}");
    }
    else if(hasProbe1) {
      json = "{'temperature':" + String(ioValues.getValue(2) + "}");

    }
    else if(hasProbe2) {
      json = "{'temperature':" + String(ioValues.getValue(3) + "}");
    }
    wifiMgr.post(sysConfig.SrvrHostName, 8081, path, json );
    nextSend = millis() + sysConfig.SendUpdateRate;
  }

  console.setVerboseLogging(true);
  ioValues.debugPrint();

  delay(2000);
}