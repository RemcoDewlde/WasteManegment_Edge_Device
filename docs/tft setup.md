
# setup
voor de firebeetle32 2 moet deze instellingen worden aangepast in de TFT_eSPI package > User_Setup.h

```cpp
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 22
#define TFT_CS   15  // Chip select control pin
#define TFT_DC    2  // Data Command control pin
#define TFT_RST   4  // Reset pin (could connect to RST pin)
// #define TFT_RST  -1  // Set TFT_RST to -1 if display RESET is connected to ESP32 board RST
#define TOUCH_CS 21     // Chip select pin (T_CS) of touch screen
```