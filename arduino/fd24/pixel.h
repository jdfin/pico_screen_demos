#pragma once

#include <Arduino.h>


// An RGB pixel is sent to the display in two bytes: 5 bits red, 6 bits green,
// 5 bits blue, packed as follows. The packing assumes a uint16_t is stored
// little-endian, so SPI sends the lower byte first. The LCD expects to
// receive 5 bits red, 6 bits green, then 5 bits blue, each MSB first.
//
// Packed pixel in memory:
//     | 15 14 13 12 11 10  9  8 |  7  6  5  4  3  2  1  0 |
//     | g4 g3 g2 b7 b6 b5 b4 b3 | r7 r6 r5 r4 r3 g7 g6 g5 |
//
// Pixel as SPIed to display module:
//       little end first,         then big end
//     | r7 r6 r5 r4 r3 g7 g6 g5 | g4 g3 g2 b7 b6 b5 b4 b3 |

class Pixel {

    public:

        Pixel() : _pixel(0) {}

        Pixel(uint8_t r, uint8_t g, uint8_t b) :
            _pixel((((uint16_t)g << 11) & 0xe000) |
                   (((uint16_t)b << 5) & 0x1f00) |
                   (((uint16_t)r << 0) & 0x00f8) |
                   (((uint16_t)g >> 5) & 0x0007))
        {
        }

        void rgb(uint8_t& r, uint8_t& g, uint8_t& b) const
        {
            r = (uint8_t)(_pixel & 0x00f8);
            g = (uint8_t)(((_pixel & 0x0007) << 5) | ((_pixel & 0xe000) >> 11));
            b = (uint8_t)((_pixel & 0x1f00) >> 5);
        }

        uint16_t raw() const
        {
            return _pixel;
        }

        static const Pixel black;
        static const Pixel white;
        static const Pixel red;
        static const Pixel green;
        static const Pixel blue;

    private:

        uint16_t _pixel;
};
