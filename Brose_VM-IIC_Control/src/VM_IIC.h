#pragma once

#include <Arduino.h>
#include <SPI.h> // Adafruit_GFX needs this
#include <Adafruit_GFX.h>


class VM_IIC : public Adafruit_GFX {
    public:
        // allocates w * h / 4 bytes, make sure you have enough space in RAM
        // needs a function pointer to a I²C byte write function write(address, data)
        VM_IIC(int16_t w, int16_t h, uint16_t flipTime, void (*i2cWriteFunc)(uint8_t, uint8_t));
        
        void setDebugSerial(Stream* serialObj);
        void drawPixel(int16_t x, int16_t y, uint16_t color);

        uint8_t reverse(uint8_t b);
        void generateDataPacket(uint8_t moduleSelect, uint8_t colAddr, bool colData, uint8_t rowAddr, bool rowLData, bool rowLEnable, bool rowHData, bool rowHEnable);
        void writeDot(uint8_t x, uint8_t y, bool state);
        void update();

        void setDot(uint8_t x, uint8_t y, bool state);
        bool getDotFromBuffer(uint8_t x, uint8_t y, uint8_t* buf);
        bool dotChanged(uint8_t x, uint8_t y);
        bool getDot(uint8_t x, uint8_t y);
        void setFrameBuffer(uint8_t value);


        void drawCenteredText(int16_t y, const char* text);
        void clearDisplay();

        void startScrollText(int16_t x, int16_t y, const char* text);
        void scrollTextTick();

    protected:
        void (*i2cWriteByte)(uint8_t, uint8_t);
        Stream* _debugSerial;
        uint16_t _flipTime = 500;
        uint8_t i2cBuf[3] = {0};
        // buffer byte allocation is horizontal, MSB first
        // +7 so it gets rounded up instead of down
        uint8_t* frameBuffer;
        uint8_t* previousFrameBuffer;
        uint16_t frameBufferSize;

        uint16_t scrollTextWidth = 0;
        int16_t scrollTextIdx = 0, scrollTextY;
        const char* scrollTextText;
};
