#include "connector.h"


// ======================================================
// WIFI
// ======================================================

const char* WIFI_SSID = "ClockDisplay";
const char* WIFI_PASSWORD = "I love you";

WebServer server(80);


// ======================================================
// STORAGE
// ======================================================

Preferences preferences;

String outputText = "forever young";

int currentHour = 12;
int currentMinute = 0;


// ======================================================
// DISPLAYS
// ======================================================

Adafruit_SSD1306 oled(
    OLED_WIDTH,
    OLED_HEIGHT,
    &Wire,
    -1
);

Adafruit_GC9A01A tft(
    TFT_CS,
    TFT_DC,
    TFT_RST
);


// ======================================================
// CLOCK
// ======================================================

unsigned long lastMinuteTick = 0;


// ======================================================
// ANIMATION
// ======================================================

GFXcanvas16* animationSprite = nullptr;

U8G2_FOR_ADAFRUIT_GFX textRenderer;

int animationTextWidth = 0;
int animationTextHeight = 0;

int animationSpriteWidth = 0;
int animationSpriteHeight = ANIMATION_HEIGHT;

unsigned long lastAnimationFrame = 0;