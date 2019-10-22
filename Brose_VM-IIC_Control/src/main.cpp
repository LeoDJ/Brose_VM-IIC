// Possible Speed Improvements:
// * Hardware I²C at 400kHz (disable ACK check)
// * faster software serial implementation with ignorable ACK check


#include <Arduino.h>
#include <Wire.h>
#include <XantoI2C.h>

#include <SPI.h> // only needed so AdafruitGFX compiles
#include <Adafruit_GFX.h>
#include <Fonts/TomThumb.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

#define DISPLAY_WIDTH   112
#define DISPLAY_HEIGHT  19

#define FLIP_TIME       550

XantoI2C i2c(22, 21, 0);

uint8_t i2cBuf[3] = {0};

// this works, because 112 is divisible by 8, be careful, when changing
uint8_t frameBuffer[((DISPLAY_WIDTH * DISPLAY_HEIGHT) / 8)] = {};
uint8_t previousFrameBuffer[sizeof(frameBuffer)] = {0};


void i2cWriteByte(uint8_t addr, uint8_t data) {
    i2c.start();
    i2c.writeByte(addr);
    i2c.readAck(); // discard ack
    i2c.writeByte(data);
    i2c.readNack(); // discard nack
    i2c.stop();
}

uint8_t reverse(uint8_t b) { // converts MSB first to LSB first (and vice versa)
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

void generateDataPacket(uint8_t moduleSelect, uint8_t colAddr, bool colData, uint8_t rowAddr, bool rowLData, bool rowLEnable, bool rowHData, bool rowHEnable) {
    // display expects it in the order A0 A1 A2 B0 B1 (LSB first)
    colAddr = (colAddr & 0x01) << 4
                | (colAddr & 0x02) << 2
                | (colAddr & 0x04) << 0
                | (colAddr & 0x08) >> 2
                | (colAddr & 0x10) >> 4;

    // order: B0 B1 A0 A1 A2
    rowAddr = (rowAddr & 0x01) << 2
                | (rowAddr & 0x02) << 0
                | (rowAddr & 0x04) >> 2
                | (rowAddr & 0x08) << 1
                | (rowAddr & 0x10) >> 1;

    // module select is LSB fist
    i2cBuf[0] = ~(reverse(moduleSelect));

    i2cBuf[1] = (colAddr % 0x20) << 1; 
    i2cBuf[1] |= colData << 6;
    i2cBuf[1] |= (rowAddr & 0x01) << 7;

    i2cBuf[2] = rowAddr >> 1;
    i2cBuf[2] |= rowLData << 4;
    i2cBuf[2] |= rowLEnable << 5;
    i2cBuf[2] |= rowHData << 6;
    i2cBuf[2] |= rowHEnable << 7;
}


void writeDot(uint8_t x, uint8_t y, bool state) {
    // calculate digit (B0/B1) and segment (A0-A2) of adress
    uint8_t colFpDigit = (x % 28) / 7;
    uint8_t colFpSegment = x % 7 + 1;

    uint8_t moduleNum = 7 - (x / 28);
    uint8_t moduleBits = 1 << moduleNum;

    uint8_t rowFpDigit = ((y % 14) / 7) * 2 + !state; // even digits are set, odd digits are unset
    uint8_t rowFpSegment = y % 7 + 1;

    bool rowLowDriver = y < 14;

    uint8_t colAddr = colFpDigit << 3 | colFpSegment;
    uint8_t rowAddr = rowFpDigit << 3 | rowFpSegment;

    generateDataPacket(moduleBits, colAddr, !state, rowAddr, state, rowLowDriver, state, !rowLowDriver);

    // Serial.printf("x=%3d y=%2d state=%d moduleBits=%02X colAddr=%02X rowAddr=%02X buf= %02X %02X %02X\n", x, y, state, moduleBits, colAddr, rowAddr, i2cBuf[0], i2cBuf[1], i2cBuf[2]);

    i2cWriteByte(0x40, i2cBuf[0]);
    i2cWriteByte(0x42, i2cBuf[1]);
    i2cWriteByte(0x44, i2cBuf[2]);

    delayMicroseconds(FLIP_TIME);

    i2cBuf[2] &= 0x0F; // only clear row driver enables
    i2cWriteByte(0x44, i2cBuf[2]);
}


bool getDotFromBuffer(uint8_t x, uint8_t y, uint8_t* buf) {
    return !!(buf[(y * DISPLAY_WIDTH + x) / 8] & (0x80 >> (x % 8)));
}

bool dotChanged(uint8_t x, uint8_t y) {
    return getDotFromBuffer(x, y, frameBuffer) ^ getDotFromBuffer(x, y, previousFrameBuffer);
}

bool getDot(uint8_t x, uint8_t y) {
    return getDotFromBuffer(x, y, frameBuffer);
}

bool setFrameBuffer(uint8_t value) {
    memset(frameBuffer, value, sizeof(frameBuffer));
}

void setDot(uint8_t x, uint8_t y, bool state) {
    if(x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT) {
        if(state) {
            frameBuffer[(y * DISPLAY_WIDTH + x) / 8] |= 0x80 >> (x % 8);
        } else {
            frameBuffer[(y * DISPLAY_WIDTH + x) / 8] &= ~(0x80 >> (x % 8));
        }
    }
}

void writeFrameBuffer() {
    for(uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
        for(uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
            if(dotChanged(x, y)) {
                writeDot(x, y, getDot(x, y));
            }
        }
    }
    i2cWriteByte(0x40, 0xFF); // disable all modules

    //store previous frame Buffer
    memcpy(previousFrameBuffer, frameBuffer, sizeof(previousFrameBuffer));
}

class GFX : public Adafruit_GFX {
    public:
        GFX(int16_t w, int16_t h) : Adafruit_GFX(w, h) {}
        void drawPixel(int16_t x, int16_t y, uint16_t color) {
            setDot(x, y, !!color);
        }
};

GFX gfx(DISPLAY_WIDTH, DISPLAY_HEIGHT);


void drawCenteredText(int16_t y, const char* text) {
    int16_t x_, y_;
    uint16_t w, h;
    gfx.getTextBounds(text, 0, y, &x_, &y_, &w, &h);
    
    int16_t x = DISPLAY_WIDTH / 2 - w/2;
    gfx.setCursor(x, y);
    gfx.print(text);

}


uint16_t scrollTextWidth = 0;
int16_t scrollTextIdx = 0, scrollTextY;
const char* scrollTextText;

void startScrollText(int16_t x, int16_t y, const char* text) {
    scrollTextText = text;
    scrollTextY = y;

    gfx.setFont(&FreeSans12pt7b);
    gfx.setCursor(x, y);
    gfx.setTextColor(1);
    gfx.setTextWrap(0);

    int16_t x_, y_;
    uint16_t h;
    gfx.getTextBounds(text, x, y, &x_, &y_, &scrollTextWidth, &h);

}

void scrollTextTick() {
    if(scrollTextText) {
        gfx.setCursor(-scrollTextIdx, scrollTextY);
        setFrameBuffer(0);
        gfx.print(scrollTextText);
        scrollTextIdx+=2;

        if(scrollTextIdx >= scrollTextWidth) {
            scrollTextText = 0;
        }

        writeFrameBuffer();
    }
}

void setup() {
    Serial.begin(115200);

    // blank display
    for(uint8_t y = 0; y < 19; y++) {
        for(uint8_t x = 0; x < 112; x++) {
            writeDot(x, y, 0);
        }
    }
    writeDot(0, 0, 1);

    // startScrollText(0, 16, "Maker Space");


}

unsigned long lastTick = 0;
void loop() {

    // memset(frameBuffer, 0x01, sizeof(frameBuffer));
    // writeFrameBuffer();
    // memset(frameBuffer, 0x00, sizeof(frameBuffer));
    // writeFrameBuffer();

    // gfx.setFont(&FreeSans9pt7b);
    // gfx.setCursor(0, 0);
    // gfx.setTextColor(1);
    // gfx.print("This is a list of the scorpions of Trinidad and Tobago. The first synopsis of the scorpion fauna came from E");

    // writeFrameBuffer();
    // if(millis() - lastTick >= 600) {
    //     lastTick = millis();
    //     scrollTextTick();
    // }

    gfx.setFont(&FreeMonoBold12pt7b);
    gfx.setTextColor(1);
    setFrameBuffer(0);
    drawCenteredText(14, "Maker");
    writeFrameBuffer();
    delay(1000);
    setFrameBuffer(0);
    drawCenteredText(14, "Space");
    writeFrameBuffer();
    delay(1000);


}

