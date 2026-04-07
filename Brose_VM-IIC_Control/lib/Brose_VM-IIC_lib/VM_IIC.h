#pragma once

#include <Arduino.h>
#include <SPI.h> // Adafruit_GFX needs this
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>

class VM_IIC;   // forward declaration

class ScrollText {
    public:
        ScrollText(VM_IIC *fd, int16_t x, int16_t y, const char* text, int timeOffset = 0);
        ~ScrollText();

        // void start(int16_t x, int16_t y, const char* text);
        void tick(bool doUpdate = true);
        void setSpeed(int interval, int increment);
        void stop();
        bool running();

    protected:
        VM_IIC *_fd;
        uint16_t _width = 0, _areaWidth;
        int16_t _idx = 0, _y, _xOffset = 0;
        char* _text = NULL;
        uint32_t _startedMs = 0;
        int _interval = 300;
        int _increment = 4;
        bool _needsScrolling = true;
};


class VM_IIC : public Adafruit_GFX {
    public:
        // allocates w * h / 4 bytes, make sure you have enough space in RAM
        // needs a function pointer to a I²C byte write function write(address, data)
        VM_IIC(int16_t w, int16_t h, uint16_t flipTime, void (*i2cWriteFunc)(uint8_t, uint8_t));
        
        // set mapping of 28px wide modules
        void setModuleMapping(uint8_t m1 = 8, uint8_t m2 = 7, uint8_t m3 = 6, uint8_t m4 = 5, uint8_t m5 = 0, uint8_t m6 = 0, uint8_t m7 = 0, uint8_t m8 = 0);
        
        void setDebugSerial(Stream* serialObj);
        void drawPixel(int16_t x, int16_t y, uint16_t color);

        uint8_t reverse(uint8_t b);
        void generateDataPacket(uint8_t moduleSelect, uint8_t colAddr, bool colData, uint8_t rowAddr, bool rowLData, bool rowLEnable, bool rowHData, bool rowHEnable);
        void writeDot(uint8_t x, uint8_t y, bool state);
        void update(bool forceFullUpdate = false);
        bool updateProgressive(); // returns true when cycle finished

        void setDot(uint8_t x, uint8_t y, bool state);
        bool getDotFromBuffer(uint8_t x, uint8_t y, uint8_t* buf);
        bool dotChanged(uint8_t x, uint8_t y);
        bool getDot(uint8_t x, uint8_t y);
        void setFrameBuffer(uint8_t value);

        uint16_t getTextWidth(const char* text);
        void drawCenteredText(int16_t xOffset, int16_t y, const char* text);
        void clearDisplay();
        
        void startScrollText(int16_t x, int16_t y, const char* text);
        void scrollTextTick(bool doUpdate = true);
        bool scrollTextRunning();

    protected:
        void (*i2cWriteByte)(uint8_t, uint8_t);
        uint8_t moduleMapping[8];
        Stream* _debugSerial;
        uint16_t _flipTime = 500;
        uint8_t i2cBuf[3] = {0};
        // buffer byte allocation is horizontal, MSB first
        // +7 so it gets rounded up instead of down
        uint8_t* frameBuffer;
        uint8_t* previousFrameBuffer;
        uint16_t frameBufferSize;
        uint16_t frameBufferWidth;
        uint8_t prevColumnState;
        uint8_t prevModuleState;

        static const size_t SCROLL_TEXT_NUM = 4;
        ScrollText *_scrollText[SCROLL_TEXT_NUM] = {NULL};

        uint16_t progressiveXPos = 0;
};