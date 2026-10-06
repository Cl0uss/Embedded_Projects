#include "connector.h"


void loadSettings()
{
    preferences.begin("settings", true);

    outputText = preferences.getString("text", "forever young");

    currentHour = preferences.getInt("hour", 12);
    currentMinute = preferences.getInt("minute", 0);

    preferences.end();

    if (currentHour < 0 || currentHour > 23) {
        currentHour = 12;
    }

    if (currentMinute < 0 || currentMinute > 59) {
        currentMinute = 0;
    }

    outputText = truncateUtf8(
        outputText,
        MAX_OUTPUT_LENGTH
    );

    if (outputText.length() == 0) {
        outputText = "forever young";
    }

    Serial.println("Settings loaded");

    Serial.print("Time: ");
    Serial.println(getTimeString());

    Serial.print("Text: ");
    Serial.println(outputText);
}


void saveSettings()
{
    preferences.begin("settings", false);

    preferences.putString("text", outputText);

    preferences.putInt("hour", currentHour);
    preferences.putInt("minute", currentMinute);

    preferences.end();

    Serial.println("Settings saved");
}


void saveCurrentTime()
{
    preferences.begin("settings", false);

    preferences.putInt("hour", currentHour);
    preferences.putInt("minute", currentMinute);

    preferences.end();

    Serial.print("Time saved: ");
    Serial.println(getTimeString());
}