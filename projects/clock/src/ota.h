#include "config.h"
#include <ArduinoOTA.h>

extern VM_IIC flipdot;

void initOTA() {
    ESP_LOGI("OTA", "Initializing OTA... ");
    // Port defaults to 3232
    // ArduinoOTA.setPort(3232);

    // Hostname defaults to esp3232-[MAC]
    ArduinoOTA.setHostname(MY_HOSTNAME);

    // No authentication by default
    ArduinoOTA.setPassword(OTA_PASSWORD);

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        ESP_LOGI("OTA", "Start updating %s", type.c_str());
        flipdot.fillScreen(0);
        flipdot.setFont();
        flipdot.setCursor(0, 7);
        flipdot.print("OTA...");
        flipdot.update();
    });
    ArduinoOTA.onEnd([]() { ESP_LOGI("OTA", "\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { 
        ESP_LOGI("OTA", "Progress: %u%%\r", (progress / (total / 100))); 
        flipdot.setDot((progress / (total / flipdot.width())), flipdot.height() - 1, 1);
        flipdot.update();
        });
    ArduinoOTA.onError([](ota_error_t error) {
        ESP_LOGI("OTA", "Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            ESP_LOGI("OTA", "Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            ESP_LOGI("OTA", "Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            ESP_LOGI("OTA", "Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            ESP_LOGI("OTA", "Receive Failed");
        else if (error == OTA_END_ERROR)
            ESP_LOGI("OTA", "End Failed");
        flipdot.fillScreen(0);
        flipdot.print("OTA failed :(");
        flipdot.update();
    });

    ArduinoOTA.begin();
    ESP_LOGI("OTA", "done.");
}

void loopOTA() { ArduinoOTA.handle(); }