
#include <cassert>
#include <cstdint>
#include <cstdio>
// pico
#include "hardware/spi.h"
#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
// misc
#include "i2c_dev.h"
#include "sys_led.h"
// framebuffer
#include "color.h"
#include "font.h"
#include "st7796.h"
// touchscreen
#include "gt911.h"
#include "touchscreen.h"

// Pico
//        +----------| USB |----------+
//        | 1  GPIO0      VBUS_OUT 40 |
//        | 2  GPIO1   VSYS_IN_OUT 39 |
//        | 3  GND             GND 38 |
//        | 4  GPIO2        3V3_EN 37 |
//        | 5  GPIO3       3V3_OUT 36 |
// TP_SDA | 6  I2C0_SDA/4     AREF 35 |
// TP_SCL | 7  I2C0_SCL/5   GPIO28 34 |
//        | 8  GND             GND 33 |
// TP_RST | 9  GPIO6        GPIO27 32 |
// TP_INT | 10 GPIO7        GPIO26 31 |
//        | 11 GPIO8           RUN 30 |
//        | 12 GPIO9        GPIO22 29 | LED
//        | 13 GND             GND 28 |
//        | 14 GPIO10       GPIO21 27 | RST
//        | 15 GPIO11       GPIO20 26 | CD
//        | 16 GPIO12   SPI0_TX/19 25 | MOSI
//        | 17 GPIO13  SPI0_SCK/18 24 | SCK
//        | 18 GND             GND 23 |
//        | 19 GPIO14  SPI0_CSn/17 22 | CS
//        | 20 GPIO15   SPI0_RX/16 21 | MISO
//        +---------------------------+

static const int spi_miso_pin = 16;
static const int spi_cs_pin = 17;
static const int spi_clk_pin = 18;
static const int spi_mosi_pin = 19;
static const int spi_baud = 15'000'000;

static const int lcd_cd_pin = 20;
static const int lcd_rst_pin = 21;
static const int lcd_led_pin = 22;

static const int tp_sda_pin = 4;
static const int tp_scl_pin = 5;
static const int tp_rst_pin = 6;
static const int tp_int_pin = 7;

//static const Font &font = roboto_32;

// for st7796
[[maybe_unused]]
static const int work_bytes = 128;
[[maybe_unused]]
static uint8_t work[work_bytes];

static const int i2c_freq = 400'000;

static const uint8_t tp_adrs = 0x14; // 0x14 or 0x5d

static void test_1(Touchscreen &ts);
static void test_2(Touchscreen &ts);
static void test_3(Touchscreen &ts, Framebuffer &fb);
static void rotations(Touchscreen &ts);


static void reinit_screen(Framebuffer &fb)
{
    fb.set_rotation(Framebuffer::Rotation::landscape);
    fb.fill_rect(0, 0, fb.width(), fb.height(), Color::black());
}


int main()
{
    stdio_init_all();
    SysLed::init();

    SysLed::pattern(50, 950);

#if 1
    while (!stdio_usb_connected()) {
        SysLed::loop();
        tight_loop_contents();
    }
    sleep_ms(10); // small delay needed or we lose the first prints
#endif

    SysLed::off();

    // Display

    St7796 st7796(spi0, spi_miso_pin, spi_mosi_pin, spi_clk_pin, spi_cs_pin,
                  spi_baud, lcd_cd_pin, lcd_rst_pin, lcd_led_pin, 480, 320,
                  work, work_bytes);

    printf("St7796: spi running at %lu Hz\n", st7796.spi_freq());

    st7796.init();

    // Turning on the backlight here shows whatever happens to be in RAM
    // (previously displayed or random junk), so we turn it on after filling
    // the screen with something.

    reinit_screen(st7796); // set rotation, fill background

    // Now turn on backlight
    st7796.brightness(100);

    // Touchscreen

#if 1
    I2cDev i2c_dev(i2c0, tp_scl_pin, tp_sda_pin, i2c_freq);
    printf("Gt911: i2c running at %u Hz\n", i2c_dev.baud());
    Gt911 gt911(i2c_dev, tp_adrs, tp_rst_pin, tp_int_pin);
#else
    Gt911 gt911(i2c0, tp_adrs, tp_sda_pin, tp_scl_pin, tp_rst_pin, tp_int_pin,
                i2c_freq);
    printf("Gt911: i2c running at %u Hz\n", gt911.i2c_freq());
#endif

    constexpr int verbosity = 2;
    if (!gt911.init(verbosity)) {
        printf("Gt911: ERROR initializing\n");
        assert(false);
    }
    printf("Gt911: ready\n");

    Framebuffer &fb = st7796;
    Touchscreen &ts = gt911;

    fb.set_rotation(Framebuffer::Rotation::landscape);
    ts.set_rotation(Touchscreen::Rotation::landscape);

    sleep_ms(100);

    fb.draw_rect(0, 0, fb.width(), fb.height(), Color::lime());

    while (true) {
        //test_1(gt911); sleep_ms(1000);
        //test_2(gt911); sleep_ms(10);
        test_3(gt911, fb);
        sleep_ms(10);
        //rotations(gt911);
    }

    sleep_ms(100);

    return 0;
}


static void poll_touch(Touchscreen &ts)
{
    int col, row;
    int cnt = ts.get_touch(col, row);
    printf("cnt=%d", cnt);
    while (cnt-- > 0)
        printf(" (%d,%d)", col, row);
    printf("\n");
}


// read and print touch info once
[[maybe_unused]]
static void test_1(Touchscreen &ts)
{
    poll_touch(ts);
}


// read status and report touches only if something changed
[[maybe_unused]]
static void test_2(Touchscreen &ts)
{
    constexpr int t_max = 5;

    static int col[t_max] = {-1, -1, -1, -1, -1};
    static int row[t_max] = {-1, -1, -1, -1, -1};
    static int cnt = -1;

    int new_col[t_max];
    int new_row[t_max];
    int new_cnt = ts.get_touches(new_col, new_row, t_max);

    bool changed = false;
    if (new_cnt != cnt) {
        changed = true;
    } else {
        for (int t = 0; t < new_cnt; t++) {
            if (new_col[t] != col[t] || new_row[t] != row[t]) {
                changed = true;
                break;
            }
        }
    }

    for (int t = 0; t < t_max; t++) {
        col[t] = new_col[t];
        row[t] = new_row[t];
    }
    cnt = new_cnt;

    if (changed) {
        printf("cnt=%d", cnt);
        for (int t = 0; t < cnt; t++) //
            printf(" (%d,%d)", col[t], row[t]);
        printf("\n");
    }
}


[[maybe_unused]]
static void test_3(Touchscreen &ts, Framebuffer &fb)
{
    static int old_col = -1;
    static int old_row = -1;

    int col, row;
    if (ts.get_touch(col, row) >= 1) {
        if (col == old_col && row == old_row)
            return; // already drawn, and point didn't move
        if (old_col >= 0 && old_row >= 0) {
            // erase previous crosshairs
            fb.line(0, old_row, fb.width() - 1, old_row, Color::black());
            fb.line(old_col, 0, old_col, fb.height() - 1, Color::black());
            old_col = -1;
            old_row = -1;
        }
        // draw crosshairs at the touch point
        fb.line(0, row, fb.width() - 1, row, Color::white());
        fb.line(col, 0, col, fb.height() - 1, Color::white());
        old_col = col;
        old_row = row;
    } else if (old_col >= 0 && old_row >= 0) {
        // erase previous crosshairs
        fb.line(0, old_row, fb.width() - 1, old_row, Color::black());
        fb.line(old_col, 0, old_col, fb.height() - 1, Color::black());
        old_col = -1;
        old_row = -1;
    }
}


[[maybe_unused]]
static void rotations(Touchscreen &ts)
{
    ts.set_rotation(Touchscreen::Rotation::landscape);
    printf("Rotation::landscape\n");
    for (int i = 0; i < 10; i++) {
        poll_touch(ts);
        sleep_ms(200);
    }

    ts.set_rotation(Touchscreen::Rotation::portrait);
    printf("Rotation::portrait\n");
    for (int i = 0; i < 10; i++) {
        poll_touch(ts);
        sleep_ms(200);
    }

    ts.set_rotation(Touchscreen::Rotation::landscape2);
    printf("Rotation::landscape2\n");
    for (int i = 0; i < 10; i++) {
        poll_touch(ts);
        sleep_ms(200);
    }

    ts.set_rotation(Touchscreen::Rotation::portrait2);
    printf("Rotation::portrait2\n");
    for (int i = 0; i < 10; i++) {
        poll_touch(ts);
        sleep_ms(200);
    }
}
