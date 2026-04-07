#include <Arduino.h>
#include <Wire.h>
#include <driver/i2c.h>
#include "esp_sntp.h"

#include "VM_IIC.h"
#include <Fonts/TomThumb.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include "fonts/SansSerif_plain_12.h"
#include "fonts/DSEG7_Classic_Regular_11.h"

#include <WiFiMulti.h>
#include <time.h>
#include "config.h"
#include "ota.h"
#include "udp_logging.h"
#include "mqttHA.h"


#define DISPLAY_WIDTH   112
#define DISPLAY_HEIGHT  19
#define FLIP_TIME       550
#define WIFI_TIMEOUT    (15 * 60 * 1000)    // reboot after 15 minutes without wifi (there's currently a bug in WiFiMulti that doesn't handle reconnects correctly when AP briefly disappears)
#define NTP_TIMEOUT     (31 * 60 * 1000)    // reboot after 31 minutes without NTP sync

const char* ntpServer = "de.pool.ntp.org";
const char* TZ_INFO =   "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"; // Berlin, for others, see: https://remotemonitoringsystems.ca/time-zone-abbreviations.php

#define PIN_SDA 21
#define PIN_SCL 22

#define DEBUG_println(...) ESP_LOGI("log", __VA_ARGS__)

void i2cWriteByteNoACK(uint8_t addr, uint8_t data) {
    // from esp32-hal-i2c.c > i2cWrite(), but with ack_en = false
    esp_err_t ret = ESP_FAIL;
    uint8_t cmd_buff[I2C_LINK_RECOMMENDED_SIZE(1)] = { 0 };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(cmd_buff, I2C_LINK_RECOMMENDED_SIZE(1));
    ret = i2c_master_start(cmd);
    if (ret != ESP_OK) { goto end; }
    ret = i2c_master_write_byte(cmd, (addr) | I2C_MASTER_WRITE, false);
    if (ret != ESP_OK) { goto end; }
    ret = i2c_master_write_byte(cmd, data, false);
    if (ret != ESP_OK) { goto end; }
    ret = i2c_master_stop(cmd);
    if (ret != ESP_OK) { goto end; }
    ret = i2c_master_cmd_begin((i2c_port_t)0, cmd, 10 / portTICK_RATE_MS);

end:
    if(cmd != NULL){
        i2c_cmd_link_delete_static(cmd);
    }
}


VM_IIC flipdot(DISPLAY_WIDTH, DISPLAY_HEIGHT, FLIP_TIME, i2cWriteByteNoACK);

WiFiMulti wifiMulti;
tm timeinfo;
time_t now;


uint32_t lastConnectionTime = 0;
uint32_t lastNtpUpdate = 0;

void checkNTPStatus() {
    sntp_sync_status_t stat = sntp_get_sync_status();
    if (stat != 0) {
        DEBUG_println("Sync status: %d", stat);
    }

    if (stat == SNTP_SYNC_STATUS_COMPLETED) {
        lastNtpUpdate = millis();
    }

    // reboot when last two time updates didn't work
    if (millis() - lastNtpUpdate > NTP_TIMEOUT) {
        DEBUG_println("NTP Timeout. Rebooting...");
        delay(10);
        ESP.restart();
    }
}

bool getNTPtime(int sec) {
    checkNTPStatus();
    
    uint32_t start = millis();
    do {
        time(&now);
        localtime_r(&now, &timeinfo);
        // Serial.print(".");
        delay(10);
    } while (((millis() - start) <= (1000 * sec)) && (timeinfo.tm_year < (2016 - 1900)));
    if (timeinfo.tm_year <= (2016 - 1900))
    return false; // the NTP call was not successful
    
    // Serial.print("now ");
    // Serial.println(now);
    // char time_output[30];
    // strftime(time_output, 30, "%a  %d-%m-%y %T", localtime(&now));
    // Serial.println(time_output);
    // Serial.println();
    return true;
}

// didn't work for some reason
void timeSyncCallback(struct timeval *tv) {
    DEBUG_println("NTP sync successful");
    // ntpSynced = true;
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n >> Flipdot Clock <<");
    
    Wire.begin(PIN_SDA, PIN_SCL, 1000e3);

    flipdot.setModuleMapping(8, 7, 6, 5);
    
    // blank display
    flipdot.clearDisplay();

    flipdot.print("Connecting to WiFi...");
    flipdot.update();

    wifiMulti.addAP(WIFI_SSID, WIFI_PASS);
    if(wifiMulti.run() == WL_CONNECTED) {
        WiFi.setHostname(MY_HOSTNAME);
        udp_logging_init("224.1.33.7", 1234);
        // udp_logging_init("172.18.3.62", 1234);
        DEBUG_println("");
        DEBUG_println("WiFi connected");
        DEBUG_println("IP address: ");
        DEBUG_println("%s", WiFi.localIP().toString().c_str());
    }

    initOTA();

    sntp_set_sync_interval(15 * 60 * 1000);  // set to 15min, default is 3h
    DEBUG_println("Time Sync Interval: %d s", sntp_get_sync_interval()/1000);
    // sntp_set_time_sync_notification_cb(timeSyncCallback); // didn't work
    configTime(0, 0, ntpServer);
    setenv("TZ", TZ_INFO, 1);
    if (getNTPtime(10)) {}  // wait up to 10sec to sync
    else {
        DEBUG_println("Time not set");
    }

    flipdot.fillScreen(0);
    flipdot.update();

    mqttInit();

    // flipdot.setFont();
    // // flipdot.startScrollText(68, 0, "        Aufstaaand! ##################");
    // flipdot.startScrollText(68, 0, "        Code for Heilbronn        Maker Space experimenta");
}

String timeStr = "00:00:00";
String dateStr = "Do, 01.01.";
String weeknames[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};

// Formats: http://www.cplusplus.com/reference/ctime/strftime/
void buildTimeString() {
    getNTPtime(0);
    if(timeinfo.tm_year > (2018 - 1900)) {
        char buf[20];
        strftime(buf, 20, "%T", localtime(&now));
        timeStr = String(buf);
        dateStr = String(weeknames[timeinfo.tm_wday]);
        strftime(buf, 20, " %d.%m.%y", localtime(&now));
        dateStr += String(buf);
    }
}

void drawCompactTime(uint16_t x, String title, String h, String m, String s) {
    flipdot.setCursor(x, 1);
    flipdot.print(title);
    flipdot.setCursor(x, 12);
    flipdot.print(h);
    flipdot.setCursor(flipdot.getCursorX() - 2, flipdot.getCursorY());
    flipdot.print(":");
    flipdot.setCursor(flipdot.getCursorX() - 2, flipdot.getCursorY());
    flipdot.print(m);
    flipdot.setCursor(flipdot.getCursorX() - 2, flipdot.getCursorY());
    flipdot.print(":");
    flipdot.setCursor(flipdot.getCursorX() - 2, flipdot.getCursorY());
    flipdot.print(s);

}

// TODO: only update when time/second actually changed
void drawTime() {
    buildTimeString();
    // Serial.println(timeStr);
    // Serial.println(dateStr);
    // flipdot.setFont(&SansSerif_plain_12);
    flipdot.setFont(&DSEG7_Classic_Regular_11);
    // flipdot.setFont();
    uint16_t timeWidth = flipdot.getTextWidth(timeStr.c_str());
    // flipdot.fillScreen(0);
    flipdot.fillRect(0, 0, 68, DISPLAY_HEIGHT, 0);
    flipdot.setCursor(0, 19);
    flipdot.print(timeStr);

    flipdot.setFont();
    flipdot.setCursor(0, 0);
    flipdot.print(dateStr);
    uint16_t dateWidth = flipdot.getTextWidth(dateStr.c_str());

    uint16_t lineX = max(timeWidth, dateWidth) + 1;
    flipdot.drawLine(lineX, 0, lineX, DISPLAY_HEIGHT - 1, 1);

    // drawCompactTime(lineX + 3, "Standup", "01", "13", "37");

    flipdot.update();
}

uint32_t lastUpdate = 0;

void loop() {
    loopOTA();
    uint8_t wifiStatus = wifiMulti.run();
    if (wifiStatus == WL_CONNECTED) {
        lastConnectionTime = millis();
    }
    else if (millis() - lastConnectionTime > WIFI_TIMEOUT) {
        DEBUG_println("WiFi Timeout. Rebooting...");
        delay(10);
        ESP.restart();  // just reboot the ESP, because of the bug in WiFiMulti
    }

    mqttLoop();

    if(millis() - lastUpdate >= 50) {
        // DEBUG_println("Test %d", millis() - lastUpdate);
        lastUpdate = millis();

        flipdot.fillScreen(0);
        flipdot.scrollTextTick(false);
        drawTime();

        // if (!flipdot.scrollTextRunning()) { // start scroll text loop
        //     flipdot.setFont();
        //     flipdot.startScrollText(68, 6, "        Code for Heilbronn\n        Maker Space experimenta");
        // }
    }

    // flip screen constantly 
    // flipdot.fillScreen(0);
    // flipdot.update();
    // delay(50);
    // flipdot.fillScreen(1);
    // flipdot.update();
    // delay(50);
}

