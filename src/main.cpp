#include "FS.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include "WiFi.h"
#include <string.h>
#include "time.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <spiffs/spiffs.hpp>
#include <battery/battery.hpp>

TFT_eSPI tft = TFT_eSPI();
AsyncWebServer server(80);

#define CALIBRATION_FILE "/cData"

#define BUTTON_X 52
#define BUTTON_Y 150
#define BUTTON_W 20
#define BUTTON_H 40
#define BUTTON_SPACING_X 26
#define BUTTON_SPACING_Y 30
#define BUTTON_TEXTSIZE 1

// Timer vars
unsigned long previousMillis = 0;
const long interval = 10000; // interval to wait for Wi-Fi connection (milliseconds)

const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";

String ssid;
String password;

const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";

// const for Time
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// TODO: Use a pointer to Local ip (this may increase performance?)
String LocalIp = "0.0.0.0";

// function definitions
void SerialAndTFTLog(String log);
void CalibrationRun();
void drawCross(int x, int y, unsigned int color);
void getLocalTime();
bool initWiFi();

void setup(void)
{
  Serial.begin(115200);

  SerialAndTFTLog("[System]: Starting");
  Serial.println("[System]: Initialising TFT display");
  tft.init();
  tft.setRotation(3);
  tft.fillScreen((0x0000));

  // init SPIFFS
  SerialAndTFTLog("[System]: Initialising filesystem");
  initSPIFFS();

  SerialAndTFTLog("[System]: Screen calibration");
  CalibrationRun();

  SerialAndTFTLog("[System]: Initialising Wifi");
  // initWiFi();
  if (!initWiFi())
  {
    SerialAndTFTLog("[System]: Starting AP mode");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);
    IPAddress IP = WiFi.softAPIP();
    LocalIp = IP.toString();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    SerialAndTFTLog("[System]: 1. Connect to this device using Wifi ");
    SerialAndTFTLog("[System]: 2. Go to the your browser and connect to: ");
    SerialAndTFTLog("[System]: Web browser address: " + IP.toString() + "/setup");
    SerialAndTFTLog("[System]: 3. Submit the wifi credentials");

    // Web Server Root URL
    server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/wifimanager.html", "text/html"); });
    server.serveStatic("/", SPIFFS, "/");

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
              {

      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            password = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(password);
            // Write file to save value
            writeFile(SPIFFS, passPath, password.c_str());
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart");
      delay(3000);
      ESP.restart(); });
    server.begin();
  }
  else
  {
    SerialAndTFTLog("[System]: Setting time");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    getLocalTime();

    SerialAndTFTLog("[System]: DONE!");

    // Clear screen
    tft.fillScreen((0xFFFF));

    // Display Ip in topbar
    tft.setCursor(0, 0, 2);
    tft.fillRect(0, 0, tft.width(), 18, TFT_BLACK);
    tft.print("IP:" + LocalIp + " | Network: " + WiFi.SSID());
  }
}

void loop()
{
  // TODO: draw this seperately in a function with other usefull info
  // tft.setCursor(260, 0);
  // tft.printf("Bat: %i", (int)get_battery_percentage());

  uint16_t x, y;
  static uint16_t color;

  // TFT_eSPI_Button button;
  // button.initButton(&tft, 60, 60, // x, y, w, h, outline, fill, text
  //                   60, 60, TFT_WHITE, TFT_BLUE, TFT_WHITE,
  //                   "test", 1);

  // char labesls[2] = {"test1", "test2"};

  TFT_eSPI_Button buttons[3];
  char buttonlabels[3][5] = {"R", "G", "B"};
  uint16_t buttoncolors[3] = {ILI9341_RED, ILI9341_GREEN, ILI9341_BLUE};

  for (uint8_t row = 0; row < 3; row++)
  {
    for (uint8_t col = 0; col < 3; col++)
    {
      buttons[col + row * 3].initButton(&tft, BUTTON_X + col * (BUTTON_W + BUTTON_SPACING_X),
                                        BUTTON_Y + row * (BUTTON_H + BUTTON_SPACING_Y), // x, y, w, h, outline, fill, text
                                        BUTTON_W, BUTTON_H, ILI9341_WHITE, buttoncolors[col + row * 3], ILI9341_WHITE,
                                        buttonlabels[col + row * 3], BUTTON_TEXTSIZE);
      buttons[col + row * 3].drawButton();
    }
  }

  bool pressed = tft.getTouch(&x, &y);

  for (uint8_t b = 0; b < 3; b++) {
    if (pressed && buttons[b].contains(x, y)) {
      buttons[b].press(true);  // tell the button it is pressed
      Serial.println("a button is pressed");
    } else {
      buttons[b].press(false);  // tell the button it is NOT pressed
    }
  }



  delay(100);


}

void SerialAndTFTLog(String item)
{
  Serial.println(item);
  tft.println(item);
}
void CalibrationRun()
{
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
void getLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
bool initWiFi()
{
  // false go to ap mode
  ssid = readFile(SPIFFS, ssidPath);
  password = readFile(SPIFFS, passPath);

  // check if ssid and password excist
  if (ssid == "" || password == "")
  {
    SerialAndTFTLog("[System]: No SSID or Password");
    return false;
  }

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  SerialAndTFTLog("[System]: Starting Wifi connection...");
  while (WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      SerialAndTFTLog("[System]: Failed to connect");
      return false;
    }
  }

  SerialAndTFTLog("[System]: Successfully made connection");
  IPAddress IP = WiFi.localIP();
  LocalIp = IP.toString();
  return true;
}