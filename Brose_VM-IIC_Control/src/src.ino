// Possible Speed Improvements:
// * Hardware I²C at 400kHz (disable ACK check)
// * faster software serial implementation with ignorable ACK check


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


// doesn't work, because it waits for ACK that never comes
void i2cWriteByteHardware(uint8_t addr, uint8_t data) {
    Wire.beginTransmission(addr);
    Wire.write(data);
    Wire.endTransmission();
}

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

    flipdot.setModuleMapping(8, 7, 6, 5);
    
    // blank display
    flipdot.clearDisplay();
    flipdot.writeDot(0, 0, 1); // indicate done

    // flipdot.setFont(&FreeSans12pt7b);
    // flipdot.startScrollText(0, 16, "Maker Space");

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
    // flipdot.scrollTextTick();

    flipdot.setFont(&FreeMonoBold12pt7b);
    flipdot.setTextColor(1);
    flipdot.fillScreen(0);
    flipdot.drawCenteredText(14, "Maker");
    flipdot.update();
    delay(1000);
    flipdot.fillScreen(0);
    flipdot.drawCenteredText(14, "Space");
    flipdot.update();
    delay(1000);

    loopOTA();
    wifiMulti.run();

}

