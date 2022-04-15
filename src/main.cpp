#include "FS.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include "WiFi.h"
#include <string.h>

TFT_eSPI tft = TFT_eSPI();

#define CALIBRATION_FILE "/cData"

const char *ssid = "RemNet";
const char *password = "WhyIsThisToWeak@12";

void SerialAndTFTLog(String log);

void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  SerialAndTFTLog("[System] Connecting to WiFi ...");
  while (WiFi.status() != WL_CONNECTED)
  {
    SerialAndTFTLog("[System] Connecting ...");
    delay(1000);
  }
  SerialAndTFTLog("[System] " + WiFi.localIP().toString());
  delay(5000);
}

void setup(void)
{
  uint16_t calibrationData[5];
  uint8_t calDataOK = 0;

  Serial.begin(115200);

  SerialAndTFTLog("[System] Starting");

  Serial.println("[System]: initialising TFT display");
  tft.init();
  tft.setRotation(3);
  tft.fillScreen((0x0000));

  SerialAndTFTLog("[System]: initialising Wifi");
  initWiFi();

  tft.setCursor(20, 0, 2);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(1);
  tft.println("calibration run");

  // check file system
  if (!SPIFFS.begin())
  {
    Serial.println("formating file system");

    SPIFFS.format();
    SPIFFS.begin();
  }

  // check if calibration file exists
  if (SPIFFS.exists(CALIBRATION_FILE))
  {
    File f = SPIFFS.open(CALIBRATION_FILE, "r");
    if (f)
    {
      if (f.readBytes((char *)calibrationData, 14) == 14)
        calDataOK = 1;
      f.close();
    }
  }
  if (calDataOK)
  {
    // calibration data valid
    tft.setTouch(calibrationData);
  }
  else
  {
    // data not valid. recalibrate
    tft.calibrateTouch(calibrationData, TFT_WHITE, TFT_RED, 15);
    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f)
    {
      f.write((const unsigned char *)calibrationData, 14);
      f.close();
    }
  }

  tft.fillScreen((0xFFBF));
}

void loop()
{
  uint16_t x, y;
  static uint16_t color;
  if (tft.getTouch(&x, &y))
  {
    tft.setCursor(5, 40, 2);
    tft.printf("x: %i     ", x);
    tft.setCursor(5, 20, 2);
    tft.printf("y: %i    ", y);

    tft.drawPixel(x, y, color);
    color += 155;
  }
}

void SerialAndTFTLog(String item)
{
  Serial.print(item);
  tft.println(item);
}

// tft.drawRect(0,0, tft.width(), 20, TFT_BLACK);
// tft.setCursor(0,0);
// tft.setTextColor(TFT_WHITE);
// tft.println("" + WiFi.broadcastIP());