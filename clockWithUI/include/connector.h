#pragma once

#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include <Wire.h>
#include <SPI.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GC9A01A.h>

#include <U8g2_for_Adafruit_GFX.h>


// ======================================================
// WIFI
// ======================================================

extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

extern WebServer server;


// ======================================================
// STORAGE
// ======================================================

extern Preferences preferences;

extern String outputText;

extern int currentHour;
extern int currentMinute;


// ======================================================
// SSD1306
// ======================================================

constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;

constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 32;

constexpr uint8_t OLED_ADDRESS = 0x3C;

extern Adafruit_SSD1306 oled;


// ======================================================
// GC9A01
// ======================================================

constexpr int TFT_RST = 33;
constexpr int TFT_CS = 5;
constexpr int TFT_DC = 27;

constexpr int TFT_MOSI = 23;
constexpr int TFT_SCK = 18;

constexpr int TFT_WIDTH = 240;
constexpr int TFT_HEIGHT = 240;

extern Adafruit_GC9A01A tft;


// ======================================================
// CLOCK
// ======================================================

constexpr unsigned long ONE_MINUTE = 60000UL;

extern unsigned long lastMinuteTick;


// ======================================================
// FONTS
// ======================================================

// Самый большой вариант.
#define ANIMATION_FONT_LARGE u8g2_font_inr27_t_cyrillic

// Если большая строка не помещается.
#define ANIMATION_FONT_MEDIUM u8g2_font_inr24_t_cyrillic

// Для очень длинных строк.
#define ANIMATION_FONT_SMALL u8g2_font_10x20_t_cyrillic


// ======================================================
// ANIMATION
// ======================================================

// Целимся примерно в 60 FPS.
constexpr unsigned long ANIMATION_FRAME_INTERVAL_US = 16667UL;

// globals.cpp использует это как начальное значение.
// Реальная высота sprite рассчитывается автоматически.
constexpr int ANIMATION_HEIGHT = 120;


// ======================================================
// TEXT BADGE
// ======================================================

constexpr int TEXT_BADGE_PADDING_X = 10;
constexpr int TEXT_BADGE_PADDING_Y = 8;
constexpr int TEXT_BADGE_RADIUS = 10;


// ======================================================
// TEXT
// ======================================================

constexpr int MAX_OUTPUT_LENGTH = 20;


// ======================================================
// UTF-8
// ======================================================

inline String truncateUtf8(const String& text, size_t maxCharacters)
{
    size_t bytePosition = 0;
    size_t characterCount = 0;

    while (bytePosition < text.length() && characterCount < maxCharacters) {
        uint8_t firstByte = (uint8_t)text[bytePosition];
        size_t characterSize = 1;

        if ((firstByte & 0x80) == 0x00) {
            characterSize = 1;
        } else if ((firstByte & 0xE0) == 0xC0) {
            characterSize = 2;
        } else if ((firstByte & 0xF0) == 0xE0) {
            characterSize = 3;
        } else if ((firstByte & 0xF8) == 0xF0) {
            characterSize = 4;
        }

        if (bytePosition + characterSize > text.length()) {
            break;
        }

        bytePosition += characterSize;
        characterCount++;
    }

    return text.substring(0, bytePosition);
}


// ======================================================
// ANIMATION GLOBALS
// ======================================================

extern GFXcanvas16* animationSprite;
extern U8G2_FOR_ADAFRUIT_GFX textRenderer;

extern int animationTextWidth;
extern int animationTextHeight;

extern int animationSpriteWidth;
extern int animationSpriteHeight;

extern unsigned long lastAnimationFrame;


// ======================================================
// DISPLAY
// ======================================================

void setupDisplays();
void drawTime();


// ======================================================
// STORAGE
// ======================================================

void loadSettings();
void saveSettings();
void saveCurrentTime();


// ======================================================
// CLOCK
// ======================================================

String getTimeString();

void updateTimeOneMinute();
void handleClockTick();


// ======================================================
// ANIMATION
// ======================================================

uint16_t colorWheel(uint8_t position);

bool setupAnimation();
void updateAnimation();


// ======================================================
// WIFI / WEB
// ======================================================

void setupWiFi();
void setupWebServer();

String getWebPage();

void handleRoot();
void handleSave();