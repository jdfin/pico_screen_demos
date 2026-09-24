#include <Arduino.h>
#include <cstdint>
#include <SPI.h>
#include <xassert.h>
#include "font.h"
#include "fd24.h"


Fd24::Fd24(SPIClass& spi,
           int gpio_spi_cs, int gpio_dc, int gpio_reset, int gpio_bl,
           uint8_t *work, int work_bytes) :
    _spi(spi),
    _spi_settings(spi_clock, spi_bitorder, spi_mode),
    _gpio_spi_cs(gpio_spi_cs),
    _gpio_dc(gpio_dc),
    _gpio_reset(gpio_reset),
    _gpio_bl(gpio_bl),
    _br(0), // off
    _height(phy_height),
    _width(phy_width),
    _font(nullptr),
    _fg(Pixel::black),
    _bg(Pixel::white),
    _work(work),
    _work_bytes(work_bytes)
{
    digitalWrite(_gpio_spi_cs, 1);
    digitalWrite(_gpio_reset, 0);
    digitalWrite(_gpio_dc, 0);
    pinMode(_gpio_spi_cs, OUTPUT);
    pinMode(_gpio_reset, OUTPUT);
    pinMode(_gpio_dc, OUTPUT);
    pinMode(_gpio_bl, OUTPUT);
    brightness(_br);
}


Fd24::~Fd24()
{
}


// Reset and initialize
//
// This takes about 120 msec (mostly in hw_reset)
//
// rotation:
//   0 - ribbon cable at bottom
//   90 - ribbon cable to right
//   -90, 270 - ribbon cable to left
//   180 - ribbon cable at top
bool Fd24::begin(int rotate)
{
    if ((rotate % 180) == 0) {
        _height = phy_height;
        _width = phy_width;
    } else {
        _height = phy_width;
        _width = phy_height;
    }

    uint8_t madctl_param;
    if (rotate == 0)
        madctl_param = 0x00;
    else if (rotate == 90)
        madctl_param = 0x60;
    else if (rotate == 180)
        madctl_param = 0xc0;
    else if (rotate == -90 || rotate == 270)
        madctl_param = 0xa0;

    struct {
        Cmd cmd;
        uint8_t num_params;
        uint8_t params[16];
    } init[] = {
        { Cmd::ramctrl, 2, { 0x00, 0xc4 } },
        { Cmd::colmod,  1, { 0x55 } },
        { Cmd::madctl,  1, { madctl_param } },
        { Cmd::gctrl,   1, { 0x75 } },
        { Cmd::vcoms,   1, { 0x35 } },
        { Cmd::vrhs,    1, { 0x11 } },
        { Cmd::pwctrl1, 2, { 0xa4, 0xa1 } },
        { Cmd::pvgamctrl, 14, { 0xd0, 0x0b, 0x11, 0x0b, 0x0a, 0x26, 0x36,
                                0x44, 0x4b, 0x38, 0x14, 0x14, 0x2a, 0x30 } },
        { Cmd::nvgamctrl, 14, { 0xd0, 0x0b, 0x11, 0x0b, 0x0a, 0x26, 0x35,
                                0x43, 0x4a, 0x38, 0x14, 0x14, 0x2a, 0x30 } },
        { Cmd::slpout,    0,  { 0x00 } },
        { Cmd::invon,     0,  { 0x00 } },
    };
    const int init_len = sizeof(init) / sizeof(init[0]);

    hw_reset(); // 120 msec

    for (int i = 0; i < init_len; i++)
        write(init[i].cmd, init[i].params, init[i].num_params);

    //delay(120); // 120 msec

    write(Cmd::dispon);

    //delay(120); // 120 msec

    return true;
}


// write a rectangle of pixels to screen
void Fd24::write(uint16_t row, uint16_t col,
                 uint16_t height, uint16_t width,
                 Pixel *data) // data[height * width], overwritten
{
    write(Cmd::raset, row, row + height - 1);
    write(Cmd::caset, col, col + width - 1);
    write(Cmd::ramwr, data, height * width * 2);
}


// Fill a rectangle with a solid color.
// Use the work buffer, sending that much repeatedly until enough pixels have
// been sent.
void Fd24::write(uint16_t row, uint16_t col,
                 uint16_t height, uint16_t width, Pixel pixel)
{
    uint32_t cnt_pixels = (uint32_t)(height) * (uint32_t)(width);
    Cmd cmd_byte = Cmd::ramwr;

    // work buffer, used to hold Pixels (two bytes each)
    const int work_pixels = _work_bytes / sizeof(Pixel);
    Pixel *work_pix = (Pixel *)_work;

    xassert(row >= 0 && row < _height, row);
    xassert(col >= 0 && col < _width, col);

    write(Cmd::raset, row, row + height - 1);
    write(Cmd::caset, col, col + width - 1);

    while (cnt_pixels > 0) {

        // spi_pixels is minimum of pixels left and work size in pixels
        uint32_t spi_pixels = cnt_pixels;
        if (spi_pixels > work_pixels)
            spi_pixels = work_pixels;

        // fill work_pix with pixel (it gets clobbered each time)
        for (int i = 0; i < spi_pixels; i++)
            work_pix[i] = pixel;

        // Write data. First time command is ramwr, after that ramwrc.
        write(cmd_byte, _work, spi_pixels * 2);

        cnt_pixels -= spi_pixels;
        cmd_byte = Cmd::ramwrc;
    }
}


// set backlight brightness (0..100)
void Fd24::brightness(int br)
{
    xassert(br >= 0 && br <= 100, br);

    _br = br;

    // 0..100 -> 0..255
    analogWrite(_gpio_bl, (_br * 255) / 100);
}


// print character to screen
void Fd24::print(const Font& font, uint16_t row, uint16_t col,
                 Pixel fg, Pixel bg, char c)
{
    xassert(c >= 0 && c < 128, c);

    // pixels we need for this particular glyph
    int num_pixels = font.y_adv * font.info[c].x_adv;

    Pixel *pix_buf = (Pixel *)_work;
    int pix_buf_len = _work_bytes / sizeof(Pixel);

    if (num_pixels > pix_buf_len)
        return;

    // get rgb components of foreground/background; used for smoothing
    uint8_t fg_r, fg_g, fg_b;
    fg.rgb(fg_r, fg_g, fg_b);
    uint8_t bg_r, bg_g, bg_b;
    bg.rgb(bg_r, bg_g, bg_b);

    // start of glyph data
    const uint8_t *gs = font.data + font.info[c].off;

    Pixel *pix;

    // for each pixel in glyph:
    // *gs = 0 means background color
    // *gs = 255 means foreground color
    // 0 <= *gs <= 255: interpolate between bg and fg
    //   r = bg_r + (fg_r - bg_r) * *gs / 255
    //   same for g and b
    const int d_r = (int)fg_r - (int)bg_r;
    const int d_g = (int)fg_g - (int)bg_g;
    const int d_b = (int)fg_b - (int)bg_b;

    const int8_t x_off = font.info[c].x_off;
    const int8_t y_off = font.info[c].y_off;
    const int8_t w = font.info[c].w;
    const int8_t h = font.info[c].h;
    const int8_t x_adv = font.info[c].x_adv;

    // fill box with background
    pix = pix_buf;
    for (int row = 0; row < font.y_adv; row++)
        for (int col = 0; col < x_adv; col++)
            *pix++ = bg;

    // put glyph in box (cropping edges)
    pix = pix_buf;
    for (int g_row = 0; g_row < h; g_row++) {
        for (int g_col = 0; g_col < w; g_col++) {
            int p_row = g_row + y_off;
            int p_col = g_col + x_off;
            if (p_row < 0 || p_row >= font.height())
                continue;
            if (p_col < 0 || p_col >= x_adv)
                continue;
            uint8_t gray = gs[g_row * w + g_col];
            uint8_t r = bg_r + d_r * gray / 255;
            uint8_t g = bg_g + d_g * gray / 255;
            uint8_t b = bg_b + d_b * gray / 255;
            pix[p_row * x_adv + p_col] = Pixel(r, g, b);
        }
    }

    // plop
    write(row, col, font.y_adv, font.info[c].x_adv, (Pixel *)_work);
}


// print string to screen
void Fd24::print(const Font& font, uint16_t row, uint16_t col,
                 Pixel fg, Pixel bg, const char *s)
{
    while (*s != '\0') {
        char c = *s++;
        print(font, row, col, fg, bg, c);
        col += font.width(c);
    }
}


// pulse hardware reset signal to controller
void Fd24::hw_reset()
{
    //delay(120);
    digitalWrite(_gpio_reset, 0);
    delayMicroseconds(20);          // 10 usec min
    digitalWrite(_gpio_reset, 1);
    delay(120);                     // 120 msec min
}


// write command with no parameters to controller
void Fd24::write(Fd24::Cmd cmd)
{
    _spi.beginTransaction(_spi_settings);
    digitalWrite(_gpio_spi_cs, 0);

    digitalWrite(_gpio_dc, 0);
    _spi.transfer(static_cast<uint8_t>(cmd));

    digitalWrite(_gpio_spi_cs, 1);
    _spi.endTransaction();
}


// write command with one 8-bit parameter to controller
void Fd24::write(Fd24::Cmd cmd, uint8_t p1)
{
    _spi.beginTransaction(_spi_settings);
    digitalWrite(_gpio_spi_cs, 0);

    digitalWrite(_gpio_dc, 0);
    _spi.transfer(static_cast<uint8_t>(cmd));

    digitalWrite(_gpio_dc, 1);
    _spi.transfer(&p1, sizeof(p1));

    digitalWrite(_gpio_spi_cs, 1);
    _spi.endTransaction();
}


// write command with two 16-bit parameters to controller
void Fd24::write(Fd24::Cmd cmd, uint16_t p1, uint16_t p2)
{
    _spi.beginTransaction(_spi_settings);
    digitalWrite(_gpio_spi_cs, 0);

    digitalWrite(_gpio_dc, 0);
    _spi.transfer(static_cast<uint8_t>(cmd));

    uint8_t buf[4] = {
        uint8_t(p1 >> 8), uint8_t(p1), uint8_t(p2 >> 8), uint8_t(p2)
    };
    digitalWrite(_gpio_dc, 1);
    _spi.transfer(buf, sizeof(buf));

    digitalWrite(_gpio_spi_cs, 1);
    _spi.endTransaction();
}


// write command with arbitrary parameters to controller
void Fd24::write(Fd24::Cmd cmd, void *buf, int buf_len)
{
    // buf is overwritten

    _spi.beginTransaction(_spi_settings);
    digitalWrite(_gpio_spi_cs, 0);

    digitalWrite(_gpio_dc, 0);
    _spi.transfer(static_cast<uint8_t>(cmd));

    digitalWrite(_gpio_dc, 1);
    _spi.transfer(buf, buf_len);

    digitalWrite(_gpio_spi_cs, 1);
    _spi.endTransaction();
}


// read command with arbitrary parameters
void Fd24::read(Fd24::Cmd cmd, void *buf, int buf_len)
{
    memset(buf, 0, buf_len);

    _spi.beginTransaction(_spi_settings);
    digitalWrite(_gpio_spi_cs, 0);

    digitalWrite(_gpio_dc, 0);
    _spi.transfer(static_cast<uint8_t>(cmd));

    digitalWrite(_gpio_dc, 1);
    _spi.transfer(buf, buf_len);

    digitalWrite(_gpio_spi_cs, 1);
    _spi.endTransaction();

    // Reading a single byte yields the byte in buf[0], so the first
    // byte in the buffer is the correct data.

    // Reading more than one byte gives a "dummy" bit as the first bit
    // returned (msb of buf[0]), so the whole buffer needs to be shifted
    // up one bit.

    // For reads of more than one byte, the caller has to know to supply
    // a buffer with one extra byte at the end.

    if (buf_len > 1) {
        uint8_t *b8 = static_cast<uint8_t*>(buf);
        for (int i = 0; i < (buf_len - 1); i++) {
            b8[i] = (b8[i] << 1) | ((b8[i+1] >> 7) & 1);
        }
        b8[buf_len - 1] = 0;
    }
}
