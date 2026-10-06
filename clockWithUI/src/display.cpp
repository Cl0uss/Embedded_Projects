#include "connector.h"


void setupDisplays()
{
    // SSD1306
    Wire.begin(OLED_SDA, OLED_SCL);

    if (oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println("SSD1306 OK");
    } else {
        Serial.println("SSD1306 FAILED");
    }

    // GC9A01
    SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);

    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(GC9A01A_BLACK);

    Serial.println("GC9A01 OK");
}


void drawTime()
{
    String value = getTimeString();

    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(3);

    int16_t x1;
    int16_t y1;

    uint16_t width;
    uint16_t height;

    oled.getTextBounds(
        value,
        0,
        0,
        &x1,
        &y1,
        &width,
        &height
    );

    int x = (OLED_WIDTH - width) / 2;
    int y = (OLED_HEIGHT - height) / 2;

    oled.setCursor(x, y);
    oled.print(value);
    oled.display();
}