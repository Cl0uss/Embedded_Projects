#include "connector.h"


// ======================================================
// HTML ESCAPE
// ======================================================

static String htmlEscape(const String& input)
{
    String result;

    result.reserve(
        input.length() + 16
    );

    for (size_t i = 0; i < input.length(); i++) {
        char c = input[i];

        switch (c) {
            case '&':
                result += "&amp;";
                break;

            case '"':
                result += "&quot;";
                break;

            case '<':
                result += "&lt;";
                break;

            case '>':
                result += "&gt;";
                break;

            default:
                result += c;
                break;
        }
    }

    return result;
}


// ======================================================
// PAGE
// ======================================================

String getWebPage()
{
    String html = R"rawliteral(
<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta
    name="viewport"
    content="width=device-width, initial-scale=1"
>

<title>
Clock Display
</title>

<style>

*
{
    box-sizing: border-box;
}

body
{
    background: #111;
    color: white;

    font-family: Arial, sans-serif;

    max-width: 520px;

    margin: auto;

    padding: 28px;
}

h1
{
    text-align: center;

    margin-bottom: 32px;
}

.section
{
    background: #1c1c1c;

    padding: 18px;

    border-radius: 14px;

    margin-bottom: 18px;
}

label
{
    display: block;

    font-size: 18px;

    margin-bottom: 8px;
}

input
{
    width: 100%;

    padding: 14px;

    font-size: 20px;

    border: none;

    border-radius: 9px;

    outline: none;
}

small
{
    display: block;

    color: #aaa;

    margin-top: 8px;
}

.buttons
{
    display: grid;

    gap: 10px;

    margin-top: 22px;
}

button
{
    width: 100%;

    padding: 15px;

    border: none;

    border-radius: 10px;

    font-size: 18px;
    font-weight: bold;

    cursor: pointer;
}

.save-all
{
    background: white;
    color: black;
}

.save-one
{
    background: #333;
    color: white;

    border: 1px solid #555;
}

.status
{
    margin-top: 22px;

    text-align: center;

    color: #888;

    font-size: 14px;
}

</style>

</head>


<body>


<h1>
Clock Display
</h1>


<form
    action="/save"
    method="POST"
>


<div class="section">

<label>
Время
</label>

<input
    type="time"
    name="time"
    value=")rawliteral";


    html += getTimeString();


    html += R"rawliteral("
>

</div>


<div class="section">

<label>
Текст
</label>

<input
    type="text"
    name="text"
    maxlength="20"
    value=")rawliteral";


    html += htmlEscape(outputText);


    html += R"rawliteral("
>

<small>
До 20 символов. Кириллица поддерживается.
</small>

</div>


<div class="buttons">

<button
    class="save-all"
    type="submit"
    name="mode"
    value="both"
>
Сохранить всё
</button>


<button
    class="save-one"
    type="submit"
    name="mode"
    value="time"
>
Только время
</button>


<button
    class="save-one"
    type="submit"
    name="mode"
    value="text"
>
Только текст
</button>

</div>


<div class="status">

Wi-Fi: ClockDisplay

<br>

192.168.4.1

</div>


</form>


</body>

</html>
)rawliteral";


    return html;
}


// ======================================================
// ROOT
// ======================================================

void handleRoot()
{
    server.send(
        200,
        "text/html; charset=utf-8",
        getWebPage()
    );
}


// ======================================================
// SAVE
// ======================================================

void handleSave()
{
    String mode = "both";

    if (server.hasArg("mode")) {
        mode = server.arg("mode");
    }


    bool saveTime =
        mode == "both"
        ||
        mode == "time";

    bool saveText =
        mode == "both"
        ||
        mode == "text";


    if (!saveTime && !saveText) {
        saveTime = true;
        saveText = true;
    }


    bool textChanged = false;


    // ==================================================
    // TEXT
    // ==================================================

    if (saveText && server.hasArg("text")) {
        String newText = server.arg("text");

        newText.trim();

        newText = truncateUtf8(
            newText,
            MAX_OUTPUT_LENGTH
        );

        if (
            newText.length() > 0
            &&
            newText != outputText
        ) {
            outputText = newText;
            textChanged = true;
        }
    }


    // ==================================================
    // TIME
    // ==================================================

    if (saveTime && server.hasArg("time")) {
        String value = server.arg("time");

        if (
            value.length() >= 5
            &&
            value.charAt(2) == ':'
        ) {
            int hour =
                value.substring(0, 2).toInt();

            int minute =
                value.substring(3, 5).toInt();


            if (
                hour >= 0
                &&
                hour <= 23
                &&
                minute >= 0
                &&
                minute <= 59
            ) {
                currentHour = hour;
                currentMinute = minute;

                lastMinuteTick = millis();

                drawTime();
            }
        }
    }


    // Пересоздаём анимацию только если текст реально
    // поменялся.
    if (textChanged) {
        setupAnimation();
    }


    saveSettings();


    Serial.println();
    Serial.println("===== WIFI SAVE =====");

    Serial.print("Mode: ");
    Serial.println(mode);

    Serial.print("Time: ");
    Serial.println(getTimeString());

    Serial.print("Text: ");
    Serial.println(outputText);

    Serial.println("=====================");


    server.sendHeader(
        "Location",
        "/"
    );

    server.send(
        303,
        "text/plain; charset=utf-8",
        ""
    );
}


// ======================================================
// WIFI
// ======================================================

void setupWiFi()
{
    Serial.print("Free heap before WiFi: ");
    Serial.println(ESP.getFreeHeap());


    WiFi.mode(WIFI_AP);

    bool ok = WiFi.softAP(
        WIFI_SSID,
        WIFI_PASSWORD,
        6,
        false,
        4
    );


    if (!ok) {
        Serial.println("WiFi AP FAILED");
        return;
    }


    Serial.println("WiFi AP started");

    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);

    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());

    Serial.print("Free heap after WiFi: ");
    Serial.println(ESP.getFreeHeap());
}


// ======================================================
// WEB SERVER
// ======================================================

void setupWebServer()
{
    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );

    server.on(
        "/save",
        HTTP_POST,
        handleSave
    );

    server.begin();

    Serial.println("Web server started");
}