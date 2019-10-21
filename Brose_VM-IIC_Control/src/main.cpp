// Possible Speed Improvements:
// * Hardware I²C at 400kHz (disable ACK check)
// * better software serial implementation with ignorable ACK check
// * writing to both rows and 4 modules at the same time


#include <Arduino.h>
#include <Wire.h>
#include <XantoI2C.h>

XantoI2C i2c(22, 21, 0);

uint8_t i2cBuf[3] = {0};

void i2cWriteByte(uint8_t addr, uint8_t data) {
    i2c.start();
    i2c.writeByte(addr);
    i2c.readAck(); // discard ack
    i2c.writeByte(data);
    i2c.readNack(); // discard nack
    i2c.stop();
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

    i2cBuf[0] = ~(0x80 >> (moduleSelect % 9));
    i2cBuf[1] = (colAddr % 0x20) << 1; // LSB First
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

    uint8_t module = 7 - (x / 28);

    uint8_t rowFpDigit = ((y % 14) / 7) * 2 + !state; // even digits are set, odd digits are unset
    uint8_t rowFpSegment = y % 7 + 1;

    bool rowLowDriver = y < 14;

    uint8_t colAddr = colFpDigit << 3 | colFpSegment;
    uint8_t rowAddr = rowFpDigit << 3 | rowFpSegment;

    generateDataPacket(module, colAddr, !state, rowAddr, state, rowLowDriver, state, !rowLowDriver);

    i2cWriteByte(0x40, i2cBuf[0]);
    i2cWriteByte(0x42, i2cBuf[1]);
    i2cWriteByte(0x44, i2cBuf[2]);

    delayMicroseconds(550);

    i2cBuf[2] &= 0x0F; // only clear row driver enables
    i2cWriteByte(0x44, i2cBuf[2]);
}

void setup() {
    for(uint8_t y = 0; y < 19; y++) {
        for(uint8_t x = 0; x < 112; x++) {
            writeDot(x, y, 0);
        }
    }
    writeDot(0, 10, 1);
}

void loop() {
    for(uint8_t y = 0; y < 19; y++) {
        for(uint8_t x = 0; x < 112; x++) {
            writeDot(x, y, 1);
        }
    }

    for(uint8_t y = 0; y < 19; y++) {
        for(uint8_t x = 0; x < 112; x++) {
            writeDot(x, y, 0);
        }
    }
}