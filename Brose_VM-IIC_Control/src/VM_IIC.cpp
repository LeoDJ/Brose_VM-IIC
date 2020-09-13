#include <VM_IIC.h>

VM_IIC::VM_IIC(int16_t w, int16_t h, uint16_t flipTime, void (*i2cWriteFunc)(uint8_t, uint8_t)) : Adafruit_GFX(w, h) {
    _debugSerial = &Serial; // set default debug serial
    _flipTime = flipTime;

    i2cWriteByte = i2cWriteFunc;

    frameBufferSize = ((w + 7) / 8) * h;


    frameBuffer = (uint8_t*)malloc(frameBufferSize);
    previousFrameBuffer = (uint8_t*)malloc(frameBufferSize);

    // check if memory allocation succeeded
    if(frameBuffer && previousFrameBuffer) { 
        memset(frameBuffer, 0, frameBufferSize);
        memset(previousFrameBuffer, 0, frameBufferSize);
    } else {
        _debugSerial->println("[VM_IIC] Error allocating memory, probably due to insufficient space");
        _debugSerial->print(frameBufferSize * 2);
        _debugSerial->println(" bytes needed for given display resolution");
    }

}


void VM_IIC::setModuleMapping(uint8_t m1, uint8_t m2, uint8_t m3, uint8_t m4, uint8_t m5, uint8_t m6, uint8_t m7, uint8_t m8) {
    moduleMapping[0] = m1;
    moduleMapping[1] = m2;
    moduleMapping[2] = m3;
    moduleMapping[3] = m4;
    moduleMapping[4] = m5;
    moduleMapping[5] = m6;
    moduleMapping[6] = m7;
    moduleMapping[7] = m8;
}


void VM_IIC::setDebugSerial(Stream* serialObj) {
    _debugSerial = serialObj;
}


// overwrites Adafruit
void VM_IIC::drawPixel(int16_t x, int16_t y, uint16_t color) {
    setDot(x, y, (color > 0));
}


uint8_t VM_IIC::reverse(uint8_t b) { // converts MSB first to LSB first (and vice versa)
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}


void VM_IIC::generateDataPacket(uint8_t moduleSelect, uint8_t colAddr, bool colData, uint8_t rowAddr, bool rowLData, bool rowLEnable, bool rowHData, bool rowHEnable) {
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


void VM_IIC::writeDot(uint8_t x, uint8_t y, bool state) {
    // calculate digit (B0/B1) and segment (A0-A2) of adress
    uint8_t colFpDigit = (x % 28) / 7;
    uint8_t colFpSegment = x % 7 + 1;

    uint8_t moduleNum = moduleMapping[(x / 28)] - 1;
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

    delayMicroseconds(_flipTime);

    i2cBuf[2] &= 0x0F; // only clear row driver enables
    i2cWriteByte(0x44, i2cBuf[2]);
}


void VM_IIC::update() {
    for(uint8_t x = 0; x < WIDTH; x++) {
        for(uint8_t y = 0; y < HEIGHT; y++) {
            if(dotChanged(x, y)) {
                writeDot(x, y, getDot(x, y));
            }
        }
    }
    i2cWriteByte(0x40, 0xFF); // disable all modules

    //store previous frame Buffer
    memcpy(previousFrameBuffer, frameBuffer, frameBufferSize);
}

bool VM_IIC::updateProgressive() {
    for(uint8_t y = 0; y < HEIGHT; y++) {
        if(dotChanged(progressiveXPos, y)) {
            writeDot(progressiveXPos, y, getDot(progressiveXPos, y));
        }
    }
    progressiveXPos++;
    if(progressiveXPos >= WIDTH) {
        progressiveXPos = 0;
        i2cWriteByte(0x40, 0xFF); // disable all modules
        //store previous frame Buffer
        memcpy(previousFrameBuffer, frameBuffer, frameBufferSize);
        return true;
    }

    return false;
}


void VM_IIC::setDot(uint8_t x, uint8_t y, bool state) {
    if(x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        if(state) {
            frameBuffer[(y * WIDTH + x) / 8] |= 0x80 >> (x % 8);
        } else {
            frameBuffer[(y * WIDTH + x) / 8] &= ~(0x80 >> (x % 8));
        }
    }
}


bool VM_IIC::getDotFromBuffer(uint8_t x, uint8_t y, uint8_t* buf) {
    return !!(buf[(y * WIDTH + x) / 8] & (0x80 >> (x % 8)));
}


bool VM_IIC::dotChanged(uint8_t x, uint8_t y) {
    return getDotFromBuffer(x, y, frameBuffer) ^ getDotFromBuffer(x, y, previousFrameBuffer);
}


bool VM_IIC::getDot(uint8_t x, uint8_t y) {
    return getDotFromBuffer(x, y, frameBuffer);
}


void VM_IIC::setFrameBuffer(uint8_t value) {
    memset(frameBuffer, value, frameBufferSize);
}


uint16_t VM_IIC::getTextWidth(const char* text) {
    int16_t x_, y_;
    uint16_t w, h;
    getTextBounds(text, 0, 0, &x_, &y_, &w, &h);
    return w;
}


void VM_IIC::drawCenteredText(int16_t xOffset, int16_t y, const char* text) {
    int16_t x = WIDTH / 2 - getTextWidth(text) / 2;
    setCursor(x + xOffset, y);
    print(text);
}


// clears display without using framebuffers
void VM_IIC::clearDisplay() {
    for(uint8_t y = 0; y < HEIGHT; y++) {
        for(uint8_t x = 0; x < WIDTH; x++) {
            writeDot(x, y, 0);
        }
    }
}


void VM_IIC::startScrollText(int16_t x, int16_t y, const char* text) {
    scrollTextText = text;
    scrollTextY = y;

    setCursor(x, y);
    setTextColor(1);
    setTextWrap(0);

    int16_t x_, y_;
    uint16_t h;
    getTextBounds(text, x, y, &x_, &y_, &scrollTextWidth, &h);

}


void VM_IIC::scrollTextTick() {
    if(scrollTextText) {
        setCursor(-scrollTextIdx, scrollTextY);
        fillScreen(0);
        print(scrollTextText);
        scrollTextIdx+=2;

        if(scrollTextIdx >= scrollTextWidth) {
            scrollTextText = 0;
        }

        update();
    }
}