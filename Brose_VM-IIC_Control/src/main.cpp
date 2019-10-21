// Speed Improvements:
// * Hardware I²C (disable ACK check)
// * 400kHz I²C
// * better register addressing
// * writing to both rows and 4 modules at the same time


#include <Arduino.h>
#include <Wire.h>
#include <XantoI2C.h>

XantoI2C i2c(22, 21, 0);

uint8_t i2cBuf[3] = {0};

typedef union {
    uint8_t raw[3];
    struct {
        union {
            uint8_t moduleSelect;
            struct {
                uint8_t modSel8 : 1;
                uint8_t modSel7 : 1;
                uint8_t modSel6 : 1;
                uint8_t modSel5 : 1;
                uint8_t modSel4 : 1;
                uint8_t modSel3 : 1;
                uint8_t modSel2 : 1;
                uint8_t modSel1 : 1;
            };
        };
        uint8_t unknown : 1;
        union {
            uint8_t colAddrRaw : 5;
            struct {
                uint8_t colAddrB1 : 1;
                uint8_t colAddrB0 : 1;
                uint8_t colAddrA2 : 1;
                uint8_t colAddrA1 : 1;
                uint8_t colAddrA0 : 1;
            };
        };
        uint8_t colData : 1;
        union {
            uint8_t rowAddrRaw : 5; // attention, B and A swapped
            struct {
                uint8_t rowAddrA : 3;
                uint8_t rowAddrB : 2;
            };
            struct {
                uint8_t rowAddrA2 : 1;
                uint8_t rowAddrA1 : 1;
                uint8_t rowAddrA0 : 1;
                uint8_t rowAddrB1 : 1;
                uint8_t rowAddrB0 : 1;
            };
        };
        uint8_t rowDataL : 1;
        uint8_t rowEnableL : 1;
        uint8_t rowDataH : 1;
        uint8_t rowEnableH : 1;
    };
} dataFrame_t;

// dataFrame_t test;

void i2cWriteByte(uint8_t addr, uint8_t data) {
    i2c.start();
    i2c.writeByte(addr);
    i2c.readAck(); // discard ack
    i2c.writeByte(data);
    i2c.readNack(); // discard nack
    i2c.stop();
}

void sendData() {
    // Wire.beginTransmission(0x21);
    // Wire.write(i2cBuf[1]);
    // Wire.endTransmission();
    // Wire.beginTransmission(0x20);
    // Wire.write(i2cBuf[0]);
    // Wire.endTransmission();
    // Wire.beginTransmission(0x22);
    // Wire.write(i2cBuf[2]);
    // Wire.endTransmission();

    i2cWriteByte(0x42, i2cBuf[1]);
    i2cWriteByte(0x40, i2cBuf[0]);
    i2cWriteByte(0x44, i2cBuf[2]);
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

    delayMicroseconds(1000);

    i2cBuf[2] &= 0x0F; // clear row driver enables
    i2cWriteByte(0x44, i2cBuf[2]);



}

void setup() {
    // Serial.begin(115200);
    // delay(3000);
    // Serial.println("Size: " + String(sizeof(dataFrame_t)));
    // dataFrame_t test;
    // test.modSel8 = 1;
    // test.colAddrB1 = 1;
    // test.rowEnableH = 1;
    // Serial.printf("%02X %02X %02X\n", test.raw[0], test.raw[1], test.raw[2]);
    // Wire.begin();

    for(uint8_t y = 0; y < 19; y++) {
        for(uint8_t x = 0; x < 112; x++) {
            writeDot(x, y, 0);
        }
    }
    writeDot(0, 10, 1);
}

void loop() {
    // put your main code here, to run repeatedly:
    // generateDataPacket(7, 0x08, 0, 0x06, 1, 1, 1, 0);
    // sendData();
    // delay(10);
    // generateDataPacket(7, 0x08, 0, 0x1E, 0, 0, 0, 0);
    // sendData();
    // delay(500);
    // generateDataPacket(7, 0x08, 1, 0x06, 0, 1, 0, 0);
    // sendData();
    // delay(10);
    // generateDataPacket(8, 0x08, 0, 0x1E, 0, 0, 0, 0);
    // sendData();
    // delay(500);



    // #define col 28
    // #define row 0x01

    // generateDataPacket(7, col, 0, row, 1, 1, 1, 0);
    // sendData();
    // delay(1);
    // generateDataPacket(7, col, 0, row, 0, 0, 0, 0);
    // sendData();
    // delay(500);
    // generateDataPacket(7, col, 1, row + 0x08, 0, 1, 0, 0);
    // sendData();
    // delay(1);
    // generateDataPacket(7, col, 0, row + 0x08, 0, 0, 0, 0);
    // sendData();
    // delay(500);



    // for(uint8_t row = 1; row < 8; row++) {
    //     if ((row / 8) % 2 == 1) {
    //         row += 8;
    //     }
    //     for(uint8_t col = 1; col < 32; col++) {
    //         generateDataPacket(7, col, 0, row, 1, 1, 0, 0);
    //         sendData();
    //         delay(1);
    //         generateDataPacket(7, col, 0, row, 0, 0, 0, 0);
    //         sendData();
    //     }
    // }

    // for(uint8_t row = 1; row < 8; row++) {
    //     if ((row / 8) % 2 == 1) {
    //         row += 8;
    //     }
    //     for(uint8_t col = 1; col < 32; col++) {
    //         generateDataPacket(7, col, 1, row + 0x08, 0, 1, 0, 0);
    //         sendData();
    //         delay(1);
    //         generateDataPacket(7, col, 0, row + 0x08, 0, 0, 0, 0);
    //         sendData();
    //     }
    // }


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