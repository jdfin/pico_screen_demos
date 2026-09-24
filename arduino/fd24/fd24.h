#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "pixel.h"


struct Font;


class Fd24 {

    public:

        Fd24(SPIClass& spi,
             int gpio_spi_cs, int gpio_dc,
             int gpio_reset, int gpio_bl,
             uint8_t *work, int work_bytes);

        virtual ~Fd24();

        // reset and initialize
        // rotate=0, 90, 180, 270, -90
        // rotate=90 means screen is rotated 90 clockwise
        // returns true on success, false on error
        bool begin(int rotate=0);

        // write a rectangle of pixels to screen
        void write(uint16_t row, uint16_t col,
                   uint16_t height, uint16_t width,
                   Pixel *data); // data[height * width], overwritten

        // fill a rectangle with a solid color
        void write(uint16_t row, uint16_t col,
                   uint16_t height, uint16_t width,
                   Pixel pixel); // one Pixel to fill rectangle

        // filled rectangle
        void fill(uint16_t r1, uint16_t c1, uint16_t r2, uint16_t c2, Pixel p)
        {
            if (r2 < r1 || c2 < c1)
                return;
            write(r1, c1, r2 - r1 + 1, c2 - c1 + 1, p);
        }

        // horizontal line
        void hline(uint16_t r, uint16_t c1, uint16_t c2, Pixel p)
        {
            fill(r, c1, r, c2, p);
        }

        // vertical line
        void vline(uint16_t r1, uint16_t r2, uint16_t c, Pixel p)
        {
            fill(r1, c, r2, c, p);
        }

        // outline rectangle
        void rect(uint16_t r1, uint16_t c1, uint16_t r2, uint16_t c2, Pixel p)
        {
            hline(r1, c1, c2, p);   // top
            hline(r2, c1, c2, p);   // bottom
            vline(r1, r2, c1, p);   // left
            vline(r1, r2, c2, p);   // right
        }

        // get/set backlight brightness (0..255)
        int brightness() const { return _br; }
        void brightness(int br);

        // get screen height and width; these change with rotation
        uint16_t height() const { return _height; }
        uint16_t width() const { return _width; }

        // get/set font
        const Font &font() const { return *_font; }
        void font(const Font& f) { _font = &f; }

        // get/set foreground/background colors
        Pixel fg() const { return _fg; }
        void fg(Pixel color) { _fg = color; }
        Pixel bg() const { return _bg; }
        void bg(Pixel color) { _bg = color; }

        // print character to display
        void print(const Font& font, uint16_t row, uint16_t col,
                   Pixel fg, Pixel bg, char c);

        // print character using current font/foreground/background
        void print(uint16_t row, uint16_t col, char c)
        {
            if (_font == nullptr)
                return; // there is no default font
            print(*_font, row, col, _fg, _bg, c);
        }

        // print string to display
        void print(const Font& font, uint16_t row, uint16_t col,
                   Pixel fg, Pixel bg, const char *s);

        // print string using current font/foreground/background
        void print(uint16_t row, uint16_t col, const char *s)
        {
            if (_font == nullptr)
                return;
            print(*_font, row, col, _fg, _bg, s);
        }

    private:

        // ST7789V command bytes
        enum class Cmd : uint8_t {
            // system function command table 1
            nop = 0x00,         // 0 parameters
            swreset = 0x01,     // 0
            rddid = 0x04,       // 3
            rddst = 0x09,       // 4
            rddpm = 0x0a,       // 1
            rddmadctl = 0x0b,   // 1
            rddcolmod = 0x0c,   // 1
            rddim = 0x0d,       // 1
            rddsm = 0x0e,       // 1
            rddsdr = 0x0f,      // 1
            slpin = 0x10,       // 0
            slpout = 0x11,      // 0
            ptlon = 0x12,       // 0
            noron = 0x13,       // 0
            invoff = 0x20,      // 0
            invon = 0x21,       // 0
            gamset = 0x26,      // 1
            dispoff = 0x28,     // 0
            dispon = 0x29,      // 0
            caset = 0x2a,       // 4
            raset = 0x2b,       // 4
            ramwr = 0x2c,       // N
            ramrd = 0x2e,       // N
            ptlar = 0x30,       // 4
            vscrdef = 0x33,     // 6
            teoff = 0x34,       // 0
            teon = 0x35,        // 1
            madctl = 0x36,      // 1
            vscrsadd = 0x37,    // 2
            idmoff = 0x38,      // 0
            idmon = 0x39,       // 0
            colmod = 0x3a,      // 1
            ramwrc = 0x3c,      // N
            ramrdc = 0x3e,      // N
            tescan = 0x44,      // 2
            rdtescan = 0x45,    // 2
            wrdisbv = 0x51,     // 1
            rddisbv = 0x52,     // 1
            wrctrld = 0x53,     // 1
            rdctrld = 0x54,     // 1
            wrcace = 0x55,      // 1
            rdcabc = 0x56,      // 1
            wrcabcmb = 0x5e,    // 1
            rdcabcmb = 0x5f,    // 1
            rdabcsdr = 0x68,    // 1
            rdid1 = 0xda,       // 1
            rdid2 = 0xdb,       // 1
            rdid3 = 0xdc,       // 1
            // system function command table 2
            ramctrl = 0xb0,     // 2
            rgbctrl = 0xb1,     // 3
            porctrl = 0xb2,     // 5
            frctrl1 = 0xb3,     // 3
            parctrl = 0xb5,     // 1
            gctrl = 0xb7,       // 1
            gtadj = 0xb8,       // 4
            dgmen = 0xba,       // 1
            vcoms = 0xbb,       // 1
            lcmctrl = 0xc0,     // 1
            idset = 0xc1,       // 3
            vdvvrhen = 0xc2,    // 2
            vrhs = 0xc3,        // 1
            vdvs = 0xc4,        // 1
            vcmofset = 0xc5,    // 1
            frctrl2 = 0xc6,     // 1
            cabcctrl = 0xc7,    // 1
            regsel1 = 0xc8,     // 1
            regsel2 = 0xca,     // 1
            pwmfrsel = 0xcc,    // 1
            pwctrl1 = 0xd0,     // 2
            vapvanen = 0xd2,    // 1
            cmd2en = 0xdf,      // 4
            pvgamctrl = 0xe0,   // 14
            nvgamctrl = 0xe1,   // 14
            dgmlutr = 0xe2,     // 64
            dgmlutb = 0xe3,     // 64
            gatectrl = 0xe4,    // 3
            spi2en = 0xe7,      // 1
            pwctrl2 = 0xe8,     // 1
            eqctrl = 0xe9,      // 3
            promctrl = 0xec,    // 1
            promen = 0xfa,      // 4
            nvmset = 0xfc,      // 2
            promact = 0xfe,     // 2
        };

        // physical height/width (portrait, ribbon cable at bottom)
        static const uint16_t phy_height = 320;
        static const uint16_t phy_width = 240;

        // SPI settings
        static const uint32_t spi_clock = 4000000;
        static const BitOrder spi_bitorder = MSBFIRST;
        static const uint8_t spi_mode = SPI_MODE3;

        SPIClass& _spi;

        SPISettings _spi_settings;

        int _gpio_spi_cs;
        int _gpio_dc;
        int _gpio_reset;
        int _gpio_bl;

        // brightness, 0..100
        int _br;

        // logical height/width, possibly rotated from physical
        uint16_t _height;
        uint16_t _width;

        // current font
        const Font *_font;

        // current foreground and background
        Pixel _fg;
        Pixel _bg;

        // Work buffer used in a few places:
        // * initializing colors lut (must be at least 128 bytes)
        // * filling rectangles on screen (any size is okay, but bigger means
        //   fewer transfers)
        // * rendering character (must be big enough for biggest font used)
        // Supplied to constructor because that's where needed size is known
        uint8_t *_work;
        int _work_bytes;

        void hw_reset();

        void write(Cmd cmd);
        void write(Cmd cmd, uint8_t p1);
        void write(Cmd cmd, uint16_t p1, uint16_t p2);
        void write(Cmd cmd, void *buf, int buf_len);

        void read(Cmd cmd, void *buf, int buf_len);
};
