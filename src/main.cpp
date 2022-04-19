#include "FS.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include "WiFi.h"
#include <string.h>
#include "time.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

TFT_eSPI tft = TFT_eSPI();
AsyncWebServer server(80);

#define CALIBRATION_FILE "/cData"

// Timer vars
unsigned long previousMillis = 0;
const long interval = 10000; // interval to wait for Wi-Fi connection (milliseconds)

const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";

String ssid;
String password;

const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";

// const for Time
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// TODO: Use a pointer to Local ip (this may increase performance?)
String LocalIp = "0.0.0.0";

// function definitions
void initSPIFFS();
void writeFile(fs::FS &fs, const char *path, const char *message);
String readFile(fs::FS &fs, const char *path);
void SerialAndTFTLog(String log);
void CalibrationRun();
void drawCross(int x, int y, unsigned int color);
void getLocalTime();
void initWiFi();
bool initWiFi2();

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
  if (!initWiFi2())
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
    SerialAndTFTLog(" ");

    // Web Server Root URL
    server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/wifimanager.html", "text/html"); });
    server.serveStatic("/", SPIFFS, "/");

server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {

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
      ESP.restart();
    });
    server.begin();
  }
  else
  {
    SerialAndTFTLog("[System]: Continue normal code");

    SerialAndTFTLog("[System]: Setting time");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    getLocalTime();

    SerialAndTFTLog("[System]: DONE!");

    // Clear screen
    tft.fillScreen((0xFFFF));

    // Display Ip in topbar
    tft.setCursor(0, 0, 2);
    tft.fillRect(0, 0, tft.width(), 18, TFT_BLACK);
    tft.print("IP:" + LocalIp);
  }
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
void drawCross(int x, int y, unsigned int color)
{
  tft.drawLine(x - 5, y, x + 5, y, color);
  tft.drawLine(x, y - 5, x, y + 5, color);
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

bool initWiFi2()
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

void initWiFi()
{
  AsyncWebServer server(80);

  ssid = readFile(SPIFFS, ssidPath);
  password = readFile(SPIFFS, passPath);

  if (ssid == "" || password == "")
  {

    SerialAndTFTLog("[System]: No values saved for ssid or password");
    WiFi.softAP("ESP-WIFI-MANAGER", "");
    IPAddress IP = WiFi.softAPIP();
    SerialAndTFTLog("[System]: AP IP address: " + IP);

    delay(4000);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html"); });

    server.serveStatic("/", SPIFFS, "/");
    SerialAndTFTLog("Web server started");
    server.begin();
  }
  else
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED)
    {
      SerialAndTFTLog("[System]: Connecting ...");
      delay(1000);
    }
    LocalIp = WiFi.localIP().toString();
    SerialAndTFTLog("[System]: " + WiFi.localIP().toString());
    delay(1000);
  }
}

void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  File root = SPIFFS.open("/");

  File file = root.openNextFile();

  while (file)
  {

    Serial.print("FILE: ");
    Serial.println(file.name());
    SerialAndTFTLog(file.name());
    file = root.openNextFile();
  }
  Serial.println("SPIFFS mounted successfully");
}

// Read File from SPIFFS
String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- frite failed");
  }
}