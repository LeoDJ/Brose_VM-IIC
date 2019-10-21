// Possible Speed Improvements:
// * Hardware I²C at 400kHz (disable ACK check)
// * better software serial implementation with ignorable ACK check
// * writing to both rows and 4 modules at the same time


#include <Arduino.h>
#include <Wire.h>
#include <XantoI2C.h>

#define DISPLAY_WIDTH   112
#define DISPLAY_HEIGHT  19

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



void writeDot(uint8_t x, uint8_t y, bool state, uint8_t overwriteModuleBits = 0) {
    // calculate digit (B0/B1) and segment (A0-A2) of adress
    uint8_t colFpDigit = (x % 28) / 7;
    uint8_t colFpSegment = x % 7 + 1;

    uint8_t moduleNum = 7 - (x / 28);
    uint8_t moduleBits = 1 << moduleNum;
    if(overwriteModuleBits)
        moduleBits = overwriteModuleBits;

    uint8_t rowFpDigit = ((y % 14) / 7) * 2 + !state; // even digits are set, odd digits are unset
    uint8_t rowFpSegment = y % 7 + 1;

    bool rowLowDriver = y < 14;

    uint8_t colAddr = colFpDigit << 3 | colFpSegment;
    uint8_t rowAddr = rowFpDigit << 3 | rowFpSegment;

    generateDataPacket(moduleBits, colAddr, !state, rowAddr, state, rowLowDriver, state, !rowLowDriver);

    Serial.printf("x=%3d y=%2d state=%d moduleBits=%02X colAddr=%02X rowAddr=%02X buf= %02X %02X %02X\n", x, y, state, moduleBits, colAddr, rowAddr, i2cBuf[0], i2cBuf[1], i2cBuf[2]);

    i2cWriteByte(0x40, i2cBuf[0]);
    i2cWriteByte(0x42, i2cBuf[1]);
    i2cWriteByte(0x44, i2cBuf[2]);

    delayMicroseconds(550);

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
    for(uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for(uint8_t x = 0; x < 28; x++) {
            for(uint8_t state = 0; state < 2; state++) { // runs code for dots to set and to unset

                uint8_t moduleBits = 0;
                for(uint8_t i = 0; i < 4; i++) { // x "multiplexing"
                    if(dotChanged(x + i * 28, y)) {
                        if(getDot(x + i * 28, y) == state) {
                            moduleBits |= 0x80 >> i; // enable module containing current x
                        }
                    }
                }

                if(moduleBits) { // check if any dots need to be written to
                    uint8_t colFpDigit = x / 7;
                    uint8_t colFpSegment = x % 7 + 1;

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

                    delayMicroseconds(550);

                    i2cBuf[2] &= 0x0F; // only clear row driver enables
                    i2cWriteByte(0x44, i2cBuf[2]);
                }
            }
        }
    }

    //store previous frame Buffer
    memcpy(previousFrameBuffer, frameBuffer, sizeof(previousFrameBuffer));
}

void setup() {
    Serial.begin(115200);

    for(uint8_t y = 0; y < 19; y++) {
        for(uint8_t x = 0; x < 112; x++) {
            writeDot(x, y, 0);
        }
    }
    writeDot(0, 10, 1);
}

void printBuf(uint8_t* buf, uint32_t size) {
    for(uint32_t i = 0; i < size; i++) {
        if(i % 16 == 0) {
            Serial.printf("%04X:  ", i);
        }
        Serial.printf("%02X ", buf[i]);
        if(i % 16 == 15) {
            Serial.printf("\n");
        }
    }
    Serial.println();
}

void loop() {
    // for(uint8_t y = 0; y < 19; y++) {
    //     for(uint8_t x = 0; x < 112; x++) {
    //         writeDot(x, y, 1);
    //     }
    // }

    // for(uint8_t y = 0; y < 19; y++) {
    //     for(uint8_t x = 0; x < 112; x++) {
    //         writeDot(x, y, 0);
    //     }
    // }

    // memset(frameBuffer, 0xFF, sizeof(frameBuffer));
    // writeFrameBuffer();
    // memset(frameBuffer, 0x00, sizeof(frameBuffer));
    // writeFrameBuffer();

    writeDot(0, 0, 1);
    delay(500);
    writeDot(0, 0, 0);
    delay(500);
    writeDot(28, 0, 1);
    delay(500);
    writeDot(28, 0, 0);
    delay(500);
    writeDot(0, 0, 1, 0xC0);
    delay(500);
    writeDot(0, 0, 0, 0xC0);
    delay(500);
    



    // uint32_t count = 0;
    // for(uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
    //     for(uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
    //         count += dotChanged(x, y);
    //     }
    // }
    // Serial.println(count);
    // Serial.print("getDot:         ");
    // for(int i = 0; i < DISPLAY_HEIGHT; i++) {
    //     Serial.print(String(getDot(i, i)) + " ");
    // }
    // Serial.println();
    // Serial.print("getDotFromPrev: ");
    // for(int i = 0; i < DISPLAY_HEIGHT; i++) {
    //     Serial.print(String(getDotFromBuffer(i, i, previousFrameBuffer)) + " ");
    // }
    // Serial.println();
    // Serial.print("dotChanged:     ");
    // for(int i = 0; i < DISPLAY_HEIGHT; i++) {
    //     Serial.print(String(dotChanged(i, i)) + " ");
    // }
    // Serial.println();
}

