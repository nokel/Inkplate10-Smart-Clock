#include <PCF85063A-SOLDERED.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "Inkplate.h"
#include "time.h"

// ==== FONTS ====
#include "Verdana10pt7b.h"
#include "Triforce20pt7b.h"
#include "Triforce40pt7b.h"
#include "Triforce60pt7b.h"
#include "Triforce90pt7b.h"
#include "Triforce120pt7b.h"
#include "chiarostd_b20pt7b.h"
#include "chiarostd_b30pt7b.h"
#include "chiarostd_b40pt7b.h"

bool debug = false;

// ==== WIFI / MQTT CONFIG ====
// creds.ino
extern const char* ssid;
extern const char* password;
extern const char* mqtt_user;
extern const char* mqtt_pass;
extern const char* mqtt_server;
extern const int mqtt_port;

// MQTT topics
const char* topic_temp_in = "/TOPIC/LOCATION";
const char* topic_hum_in = "/TOPIC/LOCATION";

// ==== TIME CONFIG (Sydney w/ DST) ====
const char* ntpServer = "pool.ntp.org";

// ==== INKPLATE ====
Inkplate display(INKPLATE_1BIT);
PCF85063A rtc;  // RTC object

// ==== NETWORK ====
WiFiClient espClient;
PubSubClient client(espClient);

// ==== DATA STORAGE ====
String roomTemp = "--";
String roomHum = "--";
String currentDate = "--";
String currentTime = "--";
String ampmStr = "--";
String loadingLayout = "Loading Static Layout";
String loadingWiFi = "Connecting to WiFi";
String loadingIP = "IP Address =";
String loadingRTC = "Syncing web time with RTC...";

unsigned long lastTimeUpdate = 0;
unsigned long lastTempUpdate = 0;

bool loading = true;
bool wifiOk = false;
bool mqttOk = false;
bool rtcOk = false;
bool haveTemp = false;
bool haveHum = false;
const unsigned long MQTT_WAIT_MS = 120000;

String loadedRoomTemp = "--";
String loadedRoomHum = "--";
String loadedIP = "--";
String loadedDate = "--";
String loadedTime = "--";
String loadedAmpm = "--";

// Room Temp
int tempPosX = 40;
int tempPosY = 180;
int tempBoxW = 380;
int tempBoxH = 120;

// Humidity
int humPosX = 750;
int humPosY = 180;
int humBoxW = 380;
int humBoxH = 120;

// Time
// hours
int hoursPosX = 250;
int hoursPosY = 500;
int hoursBoxW = 300;
int hoursBoxH = 220;

// :
int colonPosX = 570;  // Position between hours and minutes
int colonPosY = 500;
int colonBoxW = 50;
int colonBoxH = 220;

// Minutes
int minutesPosX = 650;
int minutesPosY = 500;
int minutesBoxW = 300;
int minutesBoxH = 220;

// am/pm
int ampmPosX = 950;
int ampmPosY = 500;
int ampmBoxW = 100;
int ampmBoxH = 40;

// Date
int datePosX = 400;
int datePosY = 700;
int dateBoxW = 350;
int dateBoxH = 90;

void updatePartialHours();
void updatePartialColon();
void updatePartialMinutes();
void updatePartialAMPM();
void updatePartialData();
void updatePartialDate();
void clearBox(int x, int y, int width, int height);
bool getTimeFromRTC(struct tm* timeinfo);
void setRTCTime(const struct tm* timeinfo);

// MQTT message type
enum MqttType
{
    MT_TEMP,
    MT_HUM
};

struct MqttMsg
{
    MqttType type;
    char payload[16];
};

// Time message
struct TimeMsg
{
    bool valid;
    int year, mon, mday, hour, min, sec, wday;
};

// queues
QueueHandle_t mqttQueue = NULL;
QueueHandle_t timeQueue = NULL;

// Task handles (optional)
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t timeTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;

// Loading configuration
const TickType_t WAIT_TICKS = pdMS_TO_TICKS(120000); 
const TickType_t MQTT_POLL_DELAY = pdMS_TO_TICKS(10);

void drawStaticLayout()
{
    display.clearDisplay();
    display.setFont(&chiarostd_b20pt7b);
    display.setTextColor(BLACK, WHITE);

    // "Room temp"
    display.setCursor(100, 50);
    display.print("Room temp");

    // "Humidity"
    display.setCursor(900, 50);
    display.print("Humidity");

    // partial refresh boxes
    clearBox(tempPosX - 10, tempPosY - tempBoxH + 20, tempBoxW, tempBoxH);
    clearBox(humPosX - 10, humPosY - humBoxH + 20, humBoxW, humBoxH);
    clearBox(hoursPosX, hoursPosY - hoursBoxH + 20, hoursBoxW, hoursBoxH);
    clearBox(colonPosX, colonPosY - colonBoxH + 20, colonBoxW, colonBoxH);
    clearBox(minutesPosX, minutesPosY - minutesBoxH + 20, minutesBoxW, minutesBoxH);
    clearBox(ampmPosX, ampmPosY - ampmBoxH + 20, ampmBoxW, ampmBoxH);
    clearBox(datePosX, datePosY - dateBoxH + 20, dateBoxW, dateBoxH);

    display.display();
}

void updatePartialData()
{
    // TEMP BOX
    int tempBoxX = tempPosX - 10;
    int tempBoxTop = tempPosY - tempBoxH + 20;
    int tempBoxWidth = tempBoxW;
    int tempBoxHeight = tempBoxH;

    clearBox(tempBoxX, tempBoxTop, tempBoxWidth, tempBoxHeight);

    String tempValue = roomTemp;
    String tempUnit = "°";

    display.setFont(&Triforce40pt7b);
    int16_t tempValueOffsetX, tempValueOffsetY;
    uint16_t tempValueWidth, tempValueHeight;
    display.getTextBounds(tempValue.c_str(), 0, 0, &tempValueOffsetX, &tempValueOffsetY, &tempValueWidth, &tempValueHeight);

    display.setFont(&chiarostd_b30pt7b);
    int16_t tempUnitOffsetX, tempUnitOffsetY;
    uint16_t tempUnitWidth, tempUnitHeight;
    display.getTextBounds(tempUnit.c_str(), 0, 0, &tempUnitOffsetX, &tempUnitOffsetY, &tempUnitWidth, &tempUnitHeight);

    uint16_t combinedWidth = tempValueWidth + 8 + tempUnitWidth;
    uint16_t maxHeight = max(tempValueHeight, tempUnitHeight);

    int desiredLeft = tempBoxX + (tempBoxWidth - combinedWidth) / 2;
    int desiredTop = tempBoxTop + (tempBoxHeight - maxHeight) / 2;

    int tempCursorX = desiredLeft - tempValueOffsetX;
    int tempCursorY = desiredTop - tempValueOffsetY;

    display.setFont(&Triforce40pt7b);
    display.setCursor(tempCursorX, tempCursorY);
    display.print(tempValue);

    int16_t tempActualX, tempActualY;
    uint16_t tempActualWidth, tempActualHeight;
    display.getTextBounds(tempValue.c_str(), tempCursorX, tempCursorY, &tempActualX, &tempActualY, &tempActualWidth, &tempActualHeight);

    int tempUnitCursorX = tempActualX + tempActualWidth + 8;
    int tempUnitCursorY = desiredTop - tempUnitOffsetY;

    display.setFont(&chiarostd_b30pt7b);
    display.setCursor(tempUnitCursorX, tempUnitCursorY);
    display.print(tempUnit);

    // HUM BOX
    int humidityBoxX = humPosX - 10;
    int humidityBoxTop = humPosY - humBoxH + 20;
    int humidityBoxWidth = humBoxW;
    int humidityBoxHeight = humBoxH;

    clearBox(humidityBoxX, humidityBoxTop, humidityBoxWidth, humidityBoxHeight);

    String humValue = roomHum;
    String humUnit = "%";

    display.setFont(&Triforce40pt7b);
    int16_t humValueOffsetX, humValueOffsetY;
    uint16_t humValueWidth, humValueHeight;
    display.getTextBounds(humValue.c_str(), 0, 0, &humValueOffsetX, &humValueOffsetY, &humValueWidth, &humValueHeight);

    display.setFont(&chiarostd_b30pt7b);
    int16_t humUnitOffsetX, humUnitOffsetY;
    uint16_t humUnitWidth, humUnitHeight;
    display.getTextBounds(humUnit.c_str(), 0, 0, &humUnitOffsetX, &humUnitOffsetY, &humUnitWidth, &humUnitHeight);

    combinedWidth = humValueWidth + 8 + humUnitWidth;
    maxHeight = max(humValueHeight, humUnitHeight);

    desiredLeft = humidityBoxX + (humidityBoxWidth - combinedWidth) / 2;
    desiredTop = humidityBoxTop + (humidityBoxHeight - maxHeight) / 2;

    int humCursorX = desiredLeft - humValueOffsetX;
    int humCursorY = desiredTop - humValueOffsetY;

    display.setFont(&Triforce40pt7b);
    display.setCursor(humCursorX, humCursorY);
    display.print(humValue);

    int16_t humActualX, humActualY;
    uint16_t humActualWidth, humActualHeight;
    display.getTextBounds(humValue.c_str(), humCursorX, humCursorY, &humActualX, &humActualY, &humActualWidth, &humActualHeight);

    int humUnitCursorX = humActualX + humActualWidth + 8;
    int humUnitCursorY = desiredTop - humUnitOffsetY;

    display.setFont(&chiarostd_b30pt7b);
    display.setCursor(humUnitCursorX, humUnitCursorY);
    display.print(humUnit);

    display.partialUpdate();
}

void updatePartialHours()
{
    clearBox(hoursPosX, hoursPosY - hoursBoxH + 20, hoursBoxW, hoursBoxH);

    String timeStr = currentTime;
    int colonPos = timeStr.indexOf(':');
    String hours = (colonPos != -1) ? timeStr.substring(0, colonPos) : timeStr;

    display.setFont(&Triforce120pt7b);
    int16_t hoursOffsetX, hoursOffsetY;
    uint16_t hoursWidth, hoursHeight;
    display.getTextBounds(hours.c_str(), 0, 0, &hoursOffsetX, &hoursOffsetY, &hoursWidth, &hoursHeight);

    int desiredLeft = hoursPosX + (hoursBoxW - hoursWidth) / 2;
    int desiredTop = hoursPosY - hoursBoxH + 20 + (hoursBoxH - hoursHeight) / 2;

    int hoursCursorX = desiredLeft - hoursOffsetX;
    int hoursCursorY = desiredTop - hoursOffsetY;

    display.setCursor(hoursCursorX, hoursCursorY);
    display.print(hours);

    display.partialUpdate();
}

void updatePartialColon()
{
    clearBox(colonPosX, colonPosY - colonBoxH + 20, colonBoxW, colonBoxH);

    display.setFont(&Triforce120pt7b);
    int16_t colonOffsetX, colonOffsetY;
    uint16_t colonWidth, colonHeight;
    display.getTextBounds(":", 0, 0, &colonOffsetX, &colonOffsetY, &colonWidth, &colonHeight);

    if (colonWidth == 0)
    {
        display.setFont(&chiarostd_b40pt7b);
        display.getTextBounds(":", 0, 0, &colonOffsetX, &colonOffsetY, &colonWidth, &colonHeight);
    }

    int desiredLeft = colonPosX + (colonBoxW - colonWidth) / 2;
    int desiredTop = colonPosY - colonBoxH + 20 + (colonBoxH - colonHeight) / 2;

    int colonCursorX = desiredLeft - colonOffsetX;
    int colonCursorY = desiredTop - colonOffsetY;

    display.setCursor(colonCursorX, colonCursorY);
    display.print(":");

    display.partialUpdate();
}

void updatePartialMinutes()
{
    clearBox(minutesPosX, minutesPosY - minutesBoxH + 20, minutesBoxW, minutesBoxH);

    String timeStr = currentTime;
    int colonPos = timeStr.indexOf(':');
    String minutes = (colonPos != -1) ? timeStr.substring(colonPos + 1) : " ";

    display.setFont(&Triforce120pt7b);
    int16_t minutesOffsetX, minutesOffsetY;
    uint16_t minutesWidth, minutesHeight;
    display.getTextBounds(minutes.c_str(), 0, 0, &minutesOffsetX, &minutesOffsetY, &minutesWidth, &minutesHeight);

    int desiredLeft = minutesPosX + (minutesBoxW - minutesWidth) / 2;
    int desiredTop = minutesPosY - minutesBoxH + 20 + (minutesBoxH - minutesHeight) / 2;

    int minutesCursorX = desiredLeft - minutesOffsetX;
    int minutesCursorY = desiredTop - minutesOffsetY;

    display.setCursor(minutesCursorX, minutesCursorY);
    display.print(minutes);

    display.partialUpdate();
}

void updatePartialAMPM()
{
    clearBox(ampmPosX, ampmPosY - ampmBoxH + 20, ampmBoxW, ampmBoxH);

    display.setFont(&chiarostd_b20pt7b);
    int16_t ampmOffsetX, ampmOffsetY;
    uint16_t ampmWidth, ampmHeight;
    display.getTextBounds(ampmStr.c_str(), 0, 0, &ampmOffsetX, &ampmOffsetY, &ampmWidth, &ampmHeight);

    int desiredLeft = ampmPosX + (ampmBoxW - ampmWidth) / 2;
    int desiredTop = ampmPosY - ampmBoxH + 20 + (ampmBoxH - ampmHeight) / 2;

    int ampmCursorX = desiredLeft - ampmOffsetX;
    int ampmCursorY = desiredTop - ampmOffsetY;

    display.setCursor(ampmCursorX, ampmCursorY);
    display.print(ampmStr);

    display.partialUpdate();
}

void updatePartialDate()
{
    int dateBoxX = datePosX;
    int dateBoxTop = datePosY - dateBoxH + 20;
    int dateBoxWidth = dateBoxW;
    int dateBoxHeight = dateBoxH;

    clearBox(dateBoxX, dateBoxTop, dateBoxWidth, dateBoxHeight);

    String dateString = currentDate;

    display.setFont(&Triforce40pt7b);
    int16_t dateOffsetX, dateOffsetY;
    uint16_t dateWidth, dateHeight;
    display.getTextBounds(dateString.c_str(), 0, 0, &dateOffsetX, &dateOffsetY, &dateWidth, &dateHeight);

    int desiredLeft = dateBoxX + (dateBoxWidth - dateWidth) / 2;
    int desiredTop = dateBoxTop + (dateBoxHeight - dateHeight) / 2;

    int dateCursorX = desiredLeft - dateOffsetX;
    int dateCursorY = desiredTop - dateOffsetY;

    display.setCursor(dateCursorX, dateCursorY);
    display.print(dateString);

    display.partialUpdate();
}

void showLoadingStatus(int line, const String& text)
{
    int y = 120 + (line * 40);
    int x = 50;
    int w = 900;
    int h = 32;

    display.fillRect(x, y - 24, w, h, WHITE);
    display.setCursor(x, y);
    display.setFont(&chiarostd_b20pt7b);
    display.print(text);
    display.partialUpdate();
}

void clearBox(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    if (x < 0)
    {
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        height += y;
        y = 0;
    }

    if (width <= 0 || height <= 0)
        return;

    display.fillRect(x, y, width, height, WHITE);

    if (debug)
        display.drawRect(x, y, width, height, BLACK);
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    MqttMsg m;
    m.payload[0] = 0;
    int copyLen = min((int)length, (int)sizeof(m.payload) - 1);

    if (copyLen > 0)
    {
        memcpy(m.payload, payload, copyLen);
        m.payload[copyLen] = 0;
        float value = atof(m.payload);
        dtostrf(value, 0, 1, m.payload);
    }
    else
    {
        m.payload[0] = 0;
    }

    if (String(topic) == topic_temp_in)
    {
        m.type = MT_TEMP;
        xQueueSend(mqttQueue, &m, 0);
    }
    else if (String(topic) == topic_hum_in)
    {
        m.type = MT_HUM;
        xQueueSend(mqttQueue, &m, 0);
    }
}

void mqttTask(void* pvParameters)
{
    (void)pvParameters;

    while (WiFi.status() != WL_CONNECTED)
    {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(mqttCallback);

    while (1)
    {
        if (!client.connected())
        {
            if (client.connect("Inkplate10Client", mqtt_user, mqtt_pass))
            {
                client.subscribe(topic_temp_in);
                client.subscribe(topic_hum_in);
            }
            else
            {

                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
        }

        client.loop();
        vTaskDelay(MQTT_POLL_DELAY);
    }
}

void timeTask(void* pvParameters)
{
    (void)pvParameters;

    unsigned long wifiWaitStart = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        // optional: give up after a very long time if you want:
        // if (millis() - wifiWaitStart > SOME_LIMIT_MS) { vTaskDelete(NULL); return; }
    }

    configTzTime("AEST-10AEDT,M10.1.0,M4.1.0/3", ntpServer);

    struct tm timeinfo;
    const TickType_t successDelay = pdMS_TO_TICKS(60000);
    const TickType_t failDelay = pdMS_TO_TICKS(5000);

    for (;;)
    {

        if (WiFi.status() != WL_CONNECTED)
        {

            while (WiFi.status() != WL_CONNECTED)
            {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            configTzTime("AEST-10AEDT,M10.1.0,M4.1.0/3", ntpServer);
        }

        if (getLocalTime(&timeinfo, 3000))
        {

            TimeMsg t;
            t.valid = true;
            t.year = timeinfo.tm_year + 1900;
            t.mon = timeinfo.tm_mon + 1;
            t.mday = timeinfo.tm_mday;
            t.hour = timeinfo.tm_hour;
            t.min = timeinfo.tm_min;
            t.sec = timeinfo.tm_sec;
            t.wday = timeinfo.tm_wday;

            xQueueSend(timeQueue, &t, 0);
            vTaskDelay(successDelay);
        }
        else
        {

            vTaskDelay(failDelay);
        }
    }
}

void displayTask(void* pvParameters)
{
    (void)pvParameters;

    display.clearDisplay();
    display.setFont(&chiarostd_b30pt7b);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(50, 70);
    display.print("Inkplate Loading");

    display.setFont(&chiarostd_b20pt7b);
    display.setCursor(50, 120);
    display.print("Layout: --");
    display.setCursor(50, 160);
    display.print("WiFi: --");
    display.setCursor(50, 200);
    display.print("MQTT: --");
    display.setCursor(50, 240);
    display.print("RTC: --");
    display.display();

    String bufTemp = "--";
    String bufHum = "--";
    String bufIP = "--";
    String bufDate = "--";
    String bufTime = "--";
    String bufAmpm = "--";

    bool gotTemp = false, gotHum = false, gotTime = false;
    bool wifiOk = false, mqttOk = false, rtcOk = false;

    showLoadingStatus(0, "Layout: drawn");

    if (WiFi.status() == WL_CONNECTED)
    {
        wifiOk = true;
        bufIP = WiFi.localIP().toString();
        showLoadingStatus(1, "WiFi: connected - " + bufIP);
    }
    else
    {
        showLoadingStatus(1, "WiFi: connecting...");
        unsigned long startWiFi = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startWiFi < 15000)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            wifiOk = true;
            bufIP = WiFi.localIP().toString();
            showLoadingStatus(1, "WiFi: connected - " + bufIP);
        }
        else
        {
            wifiOk = false;
            bufIP = "--";
            showLoadingStatus(1, "WiFi: FAILED");
        }
    }

    const TickType_t loadTimeout = pdMS_TO_TICKS(120000);
    TickType_t startTick = xTaskGetTickCount();

    while ((xTaskGetTickCount() - startTick) < loadTimeout)
    {
        MqttMsg m;
        if (xQueueReceive(mqttQueue, &m, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            if (m.type == MT_TEMP)
            {
                bufTemp = String(m.payload);
                gotTemp = true;
            }
            else if (m.type == MT_HUM)
            {
                bufHum = String(m.payload);
                gotHum = true;
            }

            if (gotTemp && gotHum)
                showLoadingStatus(2, "MQTT: got temp & hum");
            else if (gotTemp)
                showLoadingStatus(2, "MQTT: got temp (waiting hum)");
            else if (gotHum)
                showLoadingStatus(2, "MQTT: got hum (waiting temp)");
            else
                showLoadingStatus(2, "MQTT: waiting for data...");
        }

        TimeMsg t;
        if (xQueueReceive(timeQueue, &t, 0) == pdTRUE)
        {
            if (t.valid)
            {
                char dateBuf[20], timeBuf[16];
                struct tm tmBuf;
                tmBuf.tm_year = t.year - 1900;
                tmBuf.tm_mon = t.mon - 1;
                tmBuf.tm_mday = t.mday;
                tmBuf.tm_hour = t.hour;
                tmBuf.tm_min = t.min;
                tmBuf.tm_sec = t.sec;
                tmBuf.tm_wday = t.wday;
                tmBuf.tm_sec += 30;
                if (tmBuf.tm_sec >= 60) {
                    tmBuf.tm_sec -= 60;
                    tmBuf.tm_min += 1;
                    if (tmBuf.tm_min >= 60) {
                        tmBuf.tm_min -= 60;
                        tmBuf.tm_hour += 1;
                        if (tmBuf.tm_hour >= 24) {
                            tmBuf.tm_hour -= 24;
                        }
                    }
                }
                strftime(dateBuf, sizeof(dateBuf), "%d/%m/%y", &tmBuf);
                bufDate = String(dateBuf);

                int hour12 = tmBuf.tm_hour;
                bool pm = false;
                if (hour12 == 0)
                    hour12 = 12;
                else if (hour12 > 12)
                {
                    hour12 -= 12;
                    pm = true;
                }
                else if (hour12 == 12)
                    pm = true;

                snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", hour12, tmBuf.tm_min);
                bufTime = String(timeBuf);
                bufAmpm = pm ? "pm" : "am";
                gotTime = true;

                showLoadingStatus(3, "RTC: got NTP time");

                setRTCTime(&tmBuf);
                rtcOk = true;
            }
            else
            {
                showLoadingStatus(3, "RTC: NTP failed (will try local RTC)");
            }
        }

        if (gotTemp && gotHum && gotTime)
            break;

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!gotTime)
    {
        struct tm rtcTm;
        if (getTimeFromRTC(&rtcTm))
        {
            char dateBuf[20], timeBuf[16];
            strftime(dateBuf, sizeof(dateBuf), "%d/%m/%y", &rtcTm);
            bufDate = String(dateBuf);

            int hour12 = rtcTm.tm_hour;
            bool pm = false;
            if (hour12 == 0)
                hour12 = 12;
            else if (hour12 > 12)
            {
                hour12 -= 12;
                pm = true;
            }
            else if (hour12 == 12)
                pm = true;

            snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", hour12, rtcTm.tm_min);
            bufTime = String(timeBuf);
            bufAmpm = pm ? "pm" : "am";
            rtcOk = true;

            showLoadingStatus(3, "RTC: read local RTC");
            gotTime = true;
        }
        else
        {
            showLoadingStatus(3, "RTC: FAILED (no time)");
        }
    }

    if (gotTemp && gotHum)
    {
        showLoadingStatus(2, "MQTT: got temp & hum");
    }
    else if (gotTemp)
    {
        showLoadingStatus(2, "MQTT: got temp only");
    }
    else if (gotHum)
    {
        showLoadingStatus(2, "MQTT: got hum only");
    }
    else
    {
        showLoadingStatus(2, "MQTT: timeout (no data)");
    }

    roomTemp = gotTemp ? bufTemp : String("--");
    roomHum = gotHum ? bufHum : String("--");
    currentDate = gotTime ? bufDate : String("--");
    currentTime = gotTime ? bufTime : String("--");
    ampmStr = gotTime ? bufAmpm : String("--");

    drawStaticLayout(); 
    updatePartialHours();
    updatePartialColon();
    updatePartialMinutes();
    updatePartialAMPM();
    updatePartialData();
    updatePartialDate();

    display.display();

    for (;;)
    {
        MqttMsg m2;
        while (xQueueReceive(mqttQueue, &m2, 0) == pdTRUE)
        {
            if (m2.type == MT_TEMP)
            {
                roomTemp = String(m2.payload);
            }
            else if (m2.type == MT_HUM)
            {
                roomHum = String(m2.payload);
            }
            updatePartialData();
        }

        unsigned long nowMs = millis();
        if (nowMs - lastTimeUpdate > 60000 || lastTimeUpdate == 0)
        {
            lastTimeUpdate = nowMs;
            struct tm timeinfo;
            bool ok = false;

            if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo, 1000))
            {
                ok = true;
                setRTCTime(&timeinfo);
            }
            else if (getTimeFromRTC(&timeinfo))
            {
                ok = true;
            }

            if (ok)
            {
                char timeBuf[16], dateBuf[20];
                int hour12 = timeinfo.tm_hour;
                bool pm = false;
                if (hour12 == 0)
                    hour12 = 12;
                else if (hour12 > 12)
                {
                    hour12 -= 12;
                    pm = true;
                }
                else if (hour12 == 12)
                    pm = true;

                snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", hour12, timeinfo.tm_min);
                strftime(dateBuf, sizeof(dateBuf), "%d/%m/%y", &timeinfo);

                currentTime = String(timeBuf);
                currentDate = String(dateBuf);
                ampmStr = pm ? "pm" : "am";

                updatePartialHours();
                updatePartialColon();
                updatePartialMinutes();
                updatePartialAMPM();
                updatePartialDate();
            }
            else
            {
                currentTime = "TimeErr";
                ampmStr = "Err";
                updatePartialHours();
                updatePartialColon();
                updatePartialMinutes();
                updatePartialAMPM();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

bool getTimeFromRTC(struct tm* timeinfo)
{
    rtc.begin();  
    rtc.readTime();  
    timeinfo->tm_sec = rtc.getSecond();
    timeinfo->tm_min = rtc.getMinute();
    timeinfo->tm_hour = rtc.getHour();
    timeinfo->tm_mday = rtc.getDay();
    timeinfo->tm_mon = rtc.getMonth() - 1;
    timeinfo->tm_year = rtc.getYear() - 1900; 
    timeinfo->tm_wday = rtc.getWeekday();

    // basic sanity
    if (timeinfo->tm_hour > 23 || timeinfo->tm_min > 59)
        return false;

    return true;
}

void setRTCTime(const struct tm* timeinfo)
{
    rtc.begin();
    rtc.setDate(timeinfo->tm_wday, timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
    rtc.setTime(timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

void setup()
{
    Serial.begin(115200);
    rtc.begin();

    if (!display.begin())
    {
        Serial.println("Display init failed");
        while (1)
            ;
    }

    display.setTextColor(BLACK, WHITE);

    WiFi.begin(ssid, password);

    mqttQueue = xQueueCreate(8, sizeof(MqttMsg));
    timeQueue = xQueueCreate(2, sizeof(TimeMsg));

#if CONFIG_FREERTOS_UNICORE
    const BaseType_t coreForDisplay = 0;
#else
    const BaseType_t coreForDisplay = 1;
#endif

    xTaskCreatePinnedToCore(mqttTask, "MQTT", 4096, NULL, 2, &mqttTaskHandle, 0);
    xTaskCreatePinnedToCore(timeTask, "TIME", 4096, NULL, 1, &timeTaskHandle, coreForDisplay);
    xTaskCreatePinnedToCore(displayTask, "DISPLAY", 8192, NULL, 2, &displayTaskHandle, coreForDisplay);

}

void loop()
{
    vTaskDelay(portMAX_DELAY);
}