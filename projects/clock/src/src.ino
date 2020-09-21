#include <Arduino.h>
#include <Wire.h>
#include <XantoI2C.h>

#include "VM_IIC.h"
#include <Fonts/TomThumb.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

#include "config.h"
#include <WiFiMulti.h>
#include "ota.h"


#define DISPLAY_WIDTH   112
#define DISPLAY_HEIGHT  19
#define FLIP_TIME       550


XantoI2C i2c(22, 21, 0);

void i2cWriteByteSoftware(uint8_t addr, uint8_t data) {
    i2c.start();
    i2c.writeByte(addr);
    i2c.readAck(); // discard ack
    i2c.writeByte(data);
    i2c.readNack(); // discard nack
    i2c.stop();
}


VM_IIC flipdot(DISPLAY_WIDTH, DISPLAY_HEIGHT, FLIP_TIME, i2cWriteByteSoftware);

WiFiMulti wifiMulti;

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n >> Flipdot Counter <<");
    Serial.println("\nEnter Number to set counter to a new value");

    flipdot.setModuleMapping(8, 7, 6, 5);
    
    // blank display
    flipdot.clearDisplay();

    flipdot.print("Connecting to WiFi...");
    flipdot.update();

    wifiMulti.addAP(WIFI_SSID, WIFI_PASS);
    if(wifiMulti.run() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }
    initOTA();
}

void loop() {
    loopOTA();
    wifiMulti.run();
}

