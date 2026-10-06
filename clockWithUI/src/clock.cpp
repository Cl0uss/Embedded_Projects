#include "connector.h"


String getTimeString()
{
    char buffer[6];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02d:%02d",
        currentHour,
        currentMinute
    );

    return String(buffer);
}


void updateTimeOneMinute()
{
    currentMinute++;

    if (currentMinute >= 60) {
        currentMinute = 0;
        currentHour++;
    }

    if (currentHour >= 24) {
        currentHour = 0;
    }

    drawTime();
    saveCurrentTime();

    Serial.print("Current time: ");
    Serial.println(getTimeString());
}


void handleClockTick()
{
    unsigned long now = millis();

    if (now - lastMinuteTick < ONE_MINUTE) {
        return;
    }

    lastMinuteTick += ONE_MINUTE;

    updateTimeOneMinute();
}