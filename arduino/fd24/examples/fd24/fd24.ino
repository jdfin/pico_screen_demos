
#include "consolas_24.h"
#include "fd24.h"

// GPIOs
static const int fd24_res = 0;  // reset
static const int fd24_cs = 19;  // spi chip select
static const int fd24_dc = 1;   // data/command
static const int fd24_bl = 4;   // backlight (pwm)

SPIClass& fd24_spi = SPI;

constexpr int work_bytes =
    consolas_24_max_height * consolas_24_max_width * sizeof(Pixel);
uint8_t work[work_bytes];

static Fd24 fd24(fd24_spi, fd24_cs, fd24_dc, fd24_res, fd24_bl,
                 work, work_bytes);

// Screen should look like this:
//   - = white pixel
//   R = red pixel
//   G = green pixel
//   B = blue pixel
//   T = text area (black background, white text)
//
//                        3 3 3 3 3 3 3
//                        1 1 1 1 1 1 1
//      0 1 2 3 4 5 6     3 4 5 6 7 8 9
//
//   0  - - - - - - - ... - - - - - - -
//   1  - R R R R R R ... R R R R R R -
//   2  - R - - - - - ... - - - - - R -
//   3  - R - - - G G ... G G - - - R -
//   4  - R - - - - - ... - - - - - R -
//   5  - R - B - T T ... T T - B - R -
//   6  - R - B - T T ... T T - B - R -
//      : : : : : : :     : : : : : : :
// 233  - R - B - T T ... T T - B - R -
// 234  - R - B - T T ... T T - B - R -
// 235  - R - - - - - ... - - - - - R -
// 236  - R - - - G G ... G G - - - R -
// 237  - R - - - - - ... - - - - - R -
// 238  - R R R R R R ... R R R R R R -
// 239  - - - - - - - ... - - - - - - -

void setup()
{
    fd24_spi.begin();

    fd24.begin(90);

    fd24.write(0, 0, fd24.height(), fd24.width(), Pixel::white);

    fd24.brightness(75);

    const uint16_t screen_h = fd24.height();
    const uint16_t screen_w = fd24.width();

    const uint16_t rect_off = 1; // offset from edge

    fd24.rect(rect_off, rect_off,
              screen_h - 1 - rect_off, screen_w - 1 - rect_off,
              Pixel::red);

    const uint16_t line_off = 3; // line offset from edge
    const uint16_t end_off = 5;  // end offset from edge

    //         row                      col1     col2
    fd24.hline(line_off,                end_off, screen_w - 1 - end_off, Pixel::green);
    fd24.hline(screen_h - 1 - line_off, end_off, screen_w - 1 - end_off, Pixel::green);

    //         row1     row2                    col
    fd24.vline(end_off, screen_h - 1 - end_off, line_off,                Pixel::blue);
    fd24.vline(end_off, screen_h - 1 - end_off, screen_w - 1 - line_off, Pixel::blue);

    fd24.font(consolas_24);
    fd24.fg(Pixel::white);
    fd24.bg(Pixel::black);

    const char *s;

    const uint16_t font_h = consolas_24.height();

    const uint16_t text_off = 5;

    s = "Top Left";
    fd24.print(text_off, text_off, s);

    s = "Top Right";
    fd24.print(text_off, screen_w - text_off - consolas_24.width(s), s);

    s = "Bottom Left";
    fd24.print(screen_h - text_off - font_h, text_off, s);

    s = "Bottom Right";
    fd24.print(screen_h - text_off - font_h,
               screen_w - text_off - consolas_24.width(s), s);

    s = "Red";
    fd24.print(consolas_24, screen_h/2 - 2 * font_h,    // above middle
               screen_w/2 - consolas_24.width(s) / 2,   // centered
               Pixel::red, Pixel::white, s);

    s = "Green";
    fd24.print(consolas_24, screen_h/2 - font_h,        // above middle
               screen_w/2 - consolas_24.width(s) / 2,   // centered
               Pixel::green, Pixel::white, s);

    s = "Blue";
    fd24.print(consolas_24, screen_h/2,                 // below middle
               screen_w/2 - consolas_24.width(s) / 2,   // centered
               Pixel::blue, Pixel::white, s);

} // setup


void loop()
{
    static uint32_t next_ms = millis();

    if (millis() < next_ms)
        return;

    char s[40];

    static int b = 0; // 0..100
    static int binc = +1; // or -1

    b += binc;
    if (b == 100) {
        next_ms += 2000;
        binc = -1;
    } else if (b == 0) {
        next_ms += 2000;
        binc = +1;
    } else {
        next_ms += 50;
    }
    fd24.brightness(b);

    sprintf(s, "  %d  ", b);
    fd24.print(consolas_24,
               fd24.height()/2 + 1 * consolas_24.height(),
               fd24.width()/2 - consolas_24.width(s) / 2,
               Pixel::black, Pixel::white, s);

} // loop
