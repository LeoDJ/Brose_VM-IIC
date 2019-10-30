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

const char* scareTexts[] = {
    "Hell",
    "Candy",
    "Boo!",
    "kV",
    "Doors",
    "Pumpkin",
    "SAP"
};
uint8_t scareTextNum = sizeof(scareTexts) / sizeof(scareTexts[0]);


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

void drawEvil(int16_t x, int16_t y) {
    flipdot.setDot(x+0, y+0, 1);
    flipdot.setDot(x+1, y+1, 1);
    flipdot.setDot(x+4, y+0, 1);
    flipdot.setDot(x+3, y+1, 1);
    flipdot.setDot(x+0, y+3, 1);
    flipdot.setDot(x+1, y+4, 1);
    flipdot.setDot(x+2, y+4, 1);
    flipdot.setDot(x+3, y+4, 1);
    flipdot.setDot(x+4, y+3, 1);

}

void setup() {
    Serial.begin(115200);

    flipdot.setModuleMapping(8, 7, 6, 5);
    
    // blank display
    flipdot.clearDisplay();
    // flipdot.writeDot(0, 0, 1); // indicate done

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


    drawNumber("666");
    // drawLetzterHalt();
    // flipdot.update();
}

uint16_t w;

void drawNumber(const char* number) {
    flipdot.fillRect(0, 0, w + 3, 18, 0); // black background
    flipdot.setFont(&FreeSans12pt7b);
    flipdot.setCursor(-1, 17);
    flipdot.print(number);
    w = flipdot.getTextWidth(number);
}

void drawLetzterHalt() {
    flipdot.setFont();
    flipdot.setTextWrap(0);
    flipdot.setCursor(w + 3, 1);
    flipdot.print("Letzter Halt");
    flipdot.setCursor(w + 3, 11);
    flipdot.print("Maker Space");

    drawEvil(flipdot.width() - 5, flipdot.height() - 6);
}

void drawScare() {
    flipdot.setFont(&FreeSans12pt7b);
    flipdot.drawCenteredText(w/2, 17, scareTexts[random(0, scareTextNum)]);
}


uint32_t lastScare = 0, scareDelay = 6666;

void loop() {
    // flipdot.scrollTextTick();

    // flipdot.setFont(&FreeMonoBold12pt7b);
    // flipdot.setTextColor(1);
    // flipdot.fillScreen(0);
    // flipdot.drawCenteredText(0, 14, "Maker");
    // flipdot.update();
    // delay(1000);
    // flipdot.fillScreen(0);
    // flipdot.drawCenteredText(0, 14, "Space");
    // flipdot.update();
    // delay(1000);

    flipdot.fillScreen(0);
    if(millis() - lastScare >= scareDelay) {
        lastScare = millis();
        scareDelay = random(6666, 66666);
        // scareDelay = 3000;
        drawScare();
    } else {
        drawLetzterHalt();
    }
    drawNumber("666");
    flipdot.update();
    

    loopOTA();
    wifiMulti.run();

}

