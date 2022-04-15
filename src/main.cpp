#include "FS.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include "WiFi.h"
#include <string.h>

TFT_eSPI tft = TFT_eSPI();

#define CALIBRATION_FILE "/cData"

const char *ssid = "RemNet";
const char *password = "WhyIsThisToWeak@12";
String LocalIp = "0.0.0.0";

void SerialAndTFTLog(String log);
void CalibrationRun();
void drawCross(int x, int y, unsigned int color);

void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    SerialAndTFTLog("[System]: Connecting ...");
    delay(1000);
  }
  LocalIp = WiFi.localIP().toString();
  SerialAndTFTLog("[System]: " + WiFi.localIP().toString());
  delay(1000);
}

void setup(void)
{
  Serial.begin(115200);

  SerialAndTFTLog("[System]: Starting");

  Serial.println("[System]: initialising TFT display");
  tft.init();
  tft.setRotation(3);
  tft.fillScreen((0x0000));

  SerialAndTFTLog("[System]: initialising Wifi");
  initWiFi();

  SerialAndTFTLog("[System]: TFT calibration");
  CalibrationRun();
  SerialAndTFTLog("[System]: DONE!");

  // Clear screen
  tft.fillScreen((0xFFFF));

  // Display Ip in topbar
  tft.setCursor(0, 0, 2);
  tft.fillRect(0, 0, tft.width(), 18, TFT_BLACK);
  tft.print("IP:" + LocalIp);
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
  Serial.println(item);
  tft.println(item);
}
void CalibrationRun(){
  uint16_t calibrationData[5];
  uint8_t calDataOK = 0;

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
}
void drawCross(int x, int y, unsigned int color)
{
  tft.drawLine(x - 5, y, x + 5, y, color);
  tft.drawLine(x, y - 5, x, y + 5, color);
}
