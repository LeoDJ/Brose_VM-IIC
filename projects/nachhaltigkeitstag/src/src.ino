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

#define PULSE_PIN       13
#define RESET_PIN       5
#define PULSE_DEBOUNCE  25

#define COUNTER_DIVISOR 24

#define REVS_PER_METER  67.5

volatile uint32_t counter = 0;


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

bool newCounterValue = false;
uint32_t lastPulse = 0;
void IRAM_ATTR handlePulse() {
    if(millis() - lastPulse > PULSE_DEBOUNCE) {
        lastPulse = millis();
        counter++;
        newCounterValue = true;
        Serial.print(counter);
        Serial.print(" - ");
        Serial.println(counter / COUNTER_DIVISOR);
    }
}

uint32_t lastReset = 0;
void IRAM_ATTR handleReset() {
    if(millis() - lastReset > PULSE_DEBOUNCE) {
        lastReset = millis();
        counter = 0;
        newCounterValue = true;
        Serial.println(counter);
    }
}

void drawMaker() {
    flipdot.setFont(&FreeMonoBold12pt7b);
    flipdot.setTextColor(1);
    flipdot.fillScreen(0);
    flipdot.drawCenteredText(0, 14, "Maker");
    flipdot.update();
}

void drawSpace() {
    flipdot.setFont(&FreeMonoBold12pt7b);
    flipdot.setTextColor(1);
    flipdot.fillScreen(0);
    flipdot.drawCenteredText(0, 14, "Space");
    flipdot.update();
}

void drawCounter() {
    flipdot.setFont(&FreeSans12pt7b);
    flipdot.setTextColor(1);
    flipdot.fillScreen(0);
    char buf[16];
    snprintf(buf, 16, "%05d", counter / COUNTER_DIVISOR);
    // flipdot.drawCenteredText(0, 17, buf);
    uint16_t w = flipdot.getTextWidth(buf);
    flipdot.fillRect(0, 0, w + 3, 18, 0); // black background
    flipdot.setCursor(-1, 17);
    flipdot.print(buf);
    flipdot.setFont();
    snprintf(buf, 16, "  %05.1fm", counter / REVS_PER_METER / COUNTER_DIVISOR);
    flipdot.print(buf);
    flipdot.update();
}

uint32_t lastTimeSliceReset = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n >> Flipdot Counter <<");
    Serial.println("\nEnter Number to set counter to a new value");

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

    digitalWrite(19, LOW); // additional GND Pin
    pinMode(19, OUTPUT);
    pinMode(PULSE_PIN, INPUT_PULLUP);
    attachInterrupt(PULSE_PIN, handlePulse, FALLING);
    pinMode(RESET_PIN, INPUT_PULLUP);
    attachInterrupt(RESET_PIN, handleReset, FALLING);
    drawCounter();
    lastTimeSliceReset = millis();
}


char serialBuf[128];
uint16_t serialBufIdx = 0;


uint32_t timeSlices[] = {       0,              30000,      31000,       32000}; // last time is delay before starting over
void (*timeSliceFuncs[])() = {  drawCounter,    drawMaker,  drawSpace};

uint8_t timeSliceCount = sizeof(timeSliceFuncs)/sizeof(timeSliceFuncs[0]);
uint8_t timeSliceIdx = 0;

void loop() {

    // cycle through texts to display
    if((millis() - lastTimeSliceReset) > timeSlices[timeSliceIdx]) {
        // Serial.print("Time slice ");
        // Serial.println(timeSliceIdx);
        if(timeSliceIdx < timeSliceCount) {
            (*timeSliceFuncs[timeSliceIdx])();
        }
        timeSliceIdx++;
        if(timeSliceIdx >= timeSliceCount + 1) {
            timeSliceIdx = 0;
            lastTimeSliceReset = millis();
        }
    }

    if(newCounterValue && timeSliceIdx == 1) {
        newCounterValue = false;
        drawCounter();
    }

    loopOTA();
    // wifiMulti.run();

    // Serial number input
    while(Serial.available()) {
        char c = Serial.read();
        serialBuf[serialBufIdx] = c;
        if(c == '\r' || c == '\n') {
            uint32_t prevCounter = counter;
            sscanf(serialBuf, "%d", &counter);
            if(counter != prevCounter) { // if no number is found scanf leaves the value alone
                counter *= COUNTER_DIVISOR;
            }
            newCounterValue = true;
            memset(serialBuf, 0, 128);
            serialBufIdx = 0;
        }
        else {
            serialBufIdx++;
            if(serialBufIdx >= 128) {
                memset(serialBuf, 0, 128);
                serialBufIdx = 0;
            }
        }
    }
}

