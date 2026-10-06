#include "connector.h"


void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================");
    Serial.println("CLOCK DISPLAY STARTING");
    Serial.println("========================");

    randomSeed(micros());

    setupDisplays();
    loadSettings();

    drawTime();

    if (!setupAnimation()) {
        Serial.println("Animation initialization failed");
    }

    setupWiFi();
    setupWebServer();

    lastMinuteTick = millis();
    lastAnimationFrame = micros();

    Serial.println();
    Serial.println("Ready!");
}


void loop()
{
    server.handleClient();

    handleClockTick();
    updateAnimation();
}