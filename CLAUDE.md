# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Raspberry Pi Pico project for driving LCD displays, some with touchscreens. It demonstrates the ST7796, ST7789Vi, and ST7789V LCD drivers, optionally paired with GT911/FT6336U touchscreen controllers, using the Pico SDK.

**Hardware Target:** the whole Raspberry Pi Pico family - everything here must build and run on both RP2040 (Pico, Pico W) and RP235x (Pico 2, Pico 2 W) boards. Don't assume RP2040-specific details (register layouts, address maps, peripheral quirks) hold on RP235x, or vice versa; prefer SDK-provided platform macros/functions over hand-rolled ones. See "Targeting a specific board" and "Platform Portability" below - a bug from exactly this kind of assumption (a hardcoded RP2040 XIP address-aliasing formula, wrong on RP235x) took a full debugging session to track down.

**Key Components:**
- **Display:** ST7796 SPI LCD controller (480x320, 16-bit color, touchscreen-capable wiring), ST7789Vi SPI LCD controller (Newhaven NHD-2.4-240320 family, 240x320, 16-bit color, no touchscreen), or ST7789V SPI LCD controller (4D Systems 4DLCD-24320240-IPS, older batch, 240x320, 16-bit color, no touchscreen) - see "4D Systems 4DLCD-24320240-IPS" below for why this isn't just another `St7789vi::Model`
- **Touchscreen:** GT911 or FT6336U I2C touch controllers (used with ST7796 boards only)
- **Communication:** SPI for display, I2C for touchscreen

## Build System

This project uses CMake with the Pico SDK build system.

### Building

```bash
# Initial setup (first time only)
mkdir -p build
cd build
cmake ..

# Build all targets
cd build
ninja

# Or use the VS Code task
# "Compile Project" task runs: ninja -C build
```

### Build Outputs

Test/demo executables are built under `build/libraries/<lib>/test/`, e.g. `build/libraries/framebuffer/test/`, `build/libraries/gui/test/`, `build/libraries/touchscreen/test/`, `build/libraries/misc/test/`. Each executable generates:
- `.elf` - Executable with debug symbols
- `.uf2` - Firmware file for drag-and-drop programming
- `.bin`, `.hex` - Alternative firmware formats

### Flashing to Hardware

Via VS Code tasks (configured in `.vscode/tasks.json`):
- **"Run Project"** - Uses picotool to load firmware via USB
- **"Flash"** - Uses OpenOCD with CMSIS-DAP debug probe (adapter speed: 5000 kHz)

## Architecture

### Library Structure

The project is organized into four git submodules under `libraries/`:

1. **framebuffer** (`libraries/framebuffer/`)
   - Abstract base class `Framebuffer` providing graphics primitives
   - `St77xx` (`include/st77xx.h`, `src/st77xx.cpp`) - shared base for the Sitronix ST77xx-family SPI drivers below. Owns all the generic SPI/GPIO/DMA setup, the async fill/copy operation queue, and every drawing/text primitive, built on the handful of MIPI DCS opcodes (CASET/RASET/RAMWR/MADCTL) common to the whole family. A concrete subclass supplies only `init()` (its vendor power-on register sequence) and `madctl()` (its rotation-to-MADCTL-byte mapping).
   - `St7796` (`St77xx` subclass) for the ST7796 LCD controller (480x320, runtime width/height, touchscreen-capable boards)
   - `St7789vi` (`St77xx` subclass) for the ST7789Vi controller used by the Newhaven NHD-2.4-240320 family (240x320, no touchscreen) - see "Newhaven ST7789Vi Displays" below
   - `St7789v` (`St77xx` subclass) for the ST7789V controller used by the (older-batch) 4D Systems 4DLCD-24320240-IPS (240x320, no touchscreen) - see "4D Systems 4DLCD-24320240-IPS" below. Despite the similar name, ST7789V is not ST7789Vi: its vendor init sequence uses different power/gamma tuning and two extra registers, so it's a separate class rather than another `St7789vi::Model`.
   - Graphics primitives: pixels, lines, rectangles, circles (including antialiased)
   - Text rendering with font support
   - DMA-accelerated SPI transfers with async operation queue
   - Color system using 16-bit RGB565 format (`Pixel565`, `Color`)

2. **gui** (`libraries/gui/`)
   - UI widget framework built on `Framebuffer`
   - `gui_widget.h` - base widget
   - `gui_button.h`, `gui_label.h`, `gui_slider.h`, `gui_number.h`, `gui_page.h` - concrete widgets
   - `gui_macros.h` - supporting macros

3. **touchscreen** (`libraries/touchscreen/`)
   - Abstract base class `Touchscreen`
   - Drivers for `Gt911` and `Ft6336u` I2C touchscreen controllers
   - Multi-touch support (up to 5 touch points)
   - Rotation/orientation handling
   - Only relevant to ST7796-based boards; the Newhaven ST7789Vi displays have no touchscreen

4. **misc** (`libraries/misc/`)
   - General utilities used across the project, headers under the namespaced `misc/*.h` include path (e.g. `#include "misc/argv.h"`)
   - `sys_led.h` - System LED control
   - `dma_extra.h` - DMA interrupt multiplexer (allows multiple DMA channels to share IRQ handlers)
   - `spi_extra.h`, `i2c_extra.h`, `i2c_dev.h`, `pwm_extra.h`, `gpio_extra.h` - thin extensions over the corresponding `hardware_*` SDK libraries
   - Other utilities: `buf_log`, `dump`, `argv`, `args`, `str_ops`, `pretty_io`, `dbg_gpio`, `ibus`, `ring`, `rgb`, `timer`, `timer_extra`, `stdio_extra`, `util` (see "Platform Portability" below)

### Pin Configuration

Each test/demo target has its own small, checked-in GPIO config header (e.g. `fb_gpio_cfg.h`, `ts_gpio_cfg.h`, `st7789vi_gpio_cfg.h` under that target's `test/` directory) declaring `constexpr int` pin numbers and the SPI/I2C instance to use. Edit that header directly to match your wiring before building - there's no runtime pin configuration.

Typical ST7796 wiring (`libraries/framebuffer/test/fb_gpio_cfg.h`, `libraries/gui/test/fb_gpio_cfg.h`):

**SPI (Display):**
- MISO: GPIO 16
- CS: GPIO 17
- SCK: GPIO 18
- MOSI: GPIO 19
- LCD CD (Command/Data): GPIO 20
- LCD RST: GPIO 21
- LCD LED (Backlight): GPIO 22
- Baud rate: 15 MHz

**I2C (Touchscreen, `libraries/gui/test/ts_gpio_cfg.h`):**
- SDA: GPIO 4 (I2C0)
- SCL: GPIO 5 (I2C0)
- TP RST: GPIO 6
- TP INT: GPIO 7
- Frequency: 400 kHz
- Address: 0x14 or 0x5d (GT911)

The ST7789Vi test (`libraries/framebuffer/test/st7789vi_gpio_cfg.h`) only needs the SPI display pins; there's no touchscreen config to match.

### Display Coordinate System

All three display drivers (and `Gt911`, on touchscreen-capable boards) use the same rotation system, defined by `Framebuffer::Rotation`/`Touchscreen::Rotation` (`portrait`, `landscape`, `portrait2`, `landscape2`):
- The constructor's width/height (or, for `St7789vi`/`St7789v`, their fixed compile-time size) is the panel's physical/silicon shape, which for every panel here is portrait (width <= height) - this is the default/no-rotation state.
- `set_rotation()` updates `width()`/`height()` and reprograms the controller's MADCTL register. Landscape use (the common case) is a rotation away: call `set_rotation(Rotation::landscape)` (or `landscape2`) after `init()`.
- Typical usage: landscape mode with connector to the left
- Origin (0,0) placement varies with rotation
- Touchscreen rotation must match display rotation (ST7796 boards only)

**St7796:** physical panel is portrait (320w x 480h) silicon, constructed as `St7796(..., 320, 480, ...)` matching that shape directly - width and height are runtime constructor arguments.

**St7789vi:** physical panel is portrait (240w x 320h) silicon for all three supported Newhaven parts, so width/height are fixed at compile time (see below) rather than passed to the constructor.

**St7789v:** physical panel is also portrait (240w x 320h) silicon (4D Systems 4DLCD-24320240-IPS), width/height likewise fixed at compile time - see "4D Systems 4DLCD-24320240-IPS" below.

### Newhaven ST7789Vi Displays

`St7789vi` (`libraries/framebuffer/include/st7789vi.h`, `src/st7789vi.cpp`) supports three Newhaven NHD-2.4-240320 parts, all SPI, all 240x320, none with a touchscreen:
- NHD-2.4-240320CF-BSXV#-F
- NHD-2.4-240320AF-CTXP
- NHD-2.4-240320AF-CSXP

All three parts share the same physical size and differ only in one confirmed hardware difference - the MADCTL RGB/BGR color-order bit - so the panel choice is a runtime constructor argument (`St7789vi::Model`), not a compile-time switch: any single build of the driver works for all three parts, just by passing a different `Model` value. `St7789vi`'s constructor still doesn't take width/height like `St7796`'s does, since the physical size (240x320) really is fixed across the whole family.

The three `st7789vi_*_test` executables (`libraries/framebuffer/test/st7789vi_bsxv_f_test.cpp`, `st7789vi_af_ctxp_test.cpp`, `st7789vi_af_csxp_test.cpp`) each just fix a different `Model` value at construction - no source editing needed to target a specific part, build and flash the matching `.uf2`. They're otherwise identical and, like `st7796_test`, share `libraries/framebuffer/test/fb_tests.cpp` for the actual test/demo suite.

`libraries/framebuffer/include/st7789vi_cmd.h` holds the ST7789Vi command opcodes. Despite sharing some opcode values with `st7796_cmd.h`, several vendor-specific (Table 2) registers mean different things on the two controllers (e.g. `0xb7` is `GCTRL` on ST7789Vi but `EM` on ST7796) - don't reuse one command table for the other chip.

`St7789vi::init()` is transcribed, table-driven, from Newhaven's own sample init code in `newhaven/NHD-2.4-240320CF-Cxxx.ino` and `NHD-2.4-240320CF-BSXV.ino` (also in the repo root, along with the `ST7789VI.pdf` / `ST7796S.pdf` datasheets for reference). Only `Rotation::portrait`'s MADCTL byte is confirmed against that sample; the other three rotations use the standard MX/MV/MY combinations but haven't been verified against real hardware - if a non-default rotation comes out mirrored or flipped, adjust the bits in `St7789vi::madctl()`.

### 4D Systems 4DLCD-24320240-IPS

`St7789v` (`libraries/framebuffer/include/st7789v.h`, `src/st7789v.cpp`) drives the 4D Systems 4DLCD-24320240-IPS, a 2.4" 240x320 SPI IPS panel, no touchscreen. This product shipped with two different driver chips depending on manufacturing batch - **only the older ST7789V batch is supported**; a newer ILI9341V batch also exists and would need its own driver (its init code is also on 4D Systems' datasheet page, unused here).

ST7789V is a related but distinct chip from ST7789Vi (the Newhaven parts' controller, above) - not another `St7789vi::Model`. Its own vendor sample init sequence (from 4D Systems' datasheet page, resources.4dsystems.com.au) uses different power/gamma tuning throughout, needs `INVON` (Display Inversion On) where the Newhaven parts don't, and writes two registers (`RAMCTRL`, `RGBCTRL`) that don't appear in Newhaven's sample at all - so it gets its own command table, `libraries/framebuffer/include/st7789v_cmd.h`. `St7789v::init()` is transcribed from that vendor sequence, including its apparent redundancy (several registers get written once with default values, then again with final tuning) rather than collapsing it to just the final values, since this display is untested by this project beyond that transcription. As with `St7789vi`, only `Rotation::portrait`'s MADCTL byte (`0x00`, matching the vendor sample's own default) is confirmed; the other three rotations use the standard MX/MV/MY combinations, unverified.

`st7789v_test` (`libraries/framebuffer/test/st7789v_test.cpp`) follows the same pattern as `st7796_test`/`st7789vi_*_test`, sharing `fb_tests.cpp` for the test/demo suite.

### DMA Architecture

All three display drivers (`St7796`, `St7789vi`, `St7789v`) share this via `St77xx`, their common base class:
- Single DMA channel allocated per driver instance
- Custom DMA IRQ multiplexer (`misc/dma_extra.c`) allows multiple handlers per IRQ
- Asynchronous operation queue (64 slots) for fill/copy operations
- Operations execute in background; `wait_idle()` for synchronization
- Copy operations DMA pixel data directly from flash (XIP) when the source is there, using `misc/util.h`'s `xip_nocache()` to bypass the cache - see "Platform Portability" below for a bug this hit

### Code Style

The project uses a custom clang-format configuration (`.clang-format`):
- Based on Google style
- 4-space indentation
- No short blocks/functions/loops on single lines (enforced in recent commits)
- Braces after classes and functions on new lines
- Pointer alignment to the right (`int *ptr`)

## Common Development Tasks

### Running Tests

Test executables are built automatically:
- `build/libraries/framebuffer/test/st7796_test.uf2` - ST7796 (Waveshare) display driver test
- `build/libraries/framebuffer/test/st7789vi_bsxv_f_test.uf2`, `st7789vi_af_ctxp_test.uf2`, `st7789vi_af_csxp_test.uf2` - ST7789Vi (Newhaven) display driver test, one executable per supported part, each with `St7789vi::Model` fixed - no source editing needed to switch parts, just flash the right `.uf2`. All three share `fb_tests.cpp` with `st7796_test` (see below).
- `build/libraries/framebuffer/test/st7789v_test.uf2` - ST7789V (4D Systems 4DLCD-24320240-IPS, older batch) display driver test; also shares `fb_tests.cpp`.
- `build/libraries/gui/test/gui_test.uf2` - GUI widget test (ST7796 + touchscreen)
- `build/libraries/touchscreen/test/gt911_test.uf2`, `ft6336u_test.uf2` - touchscreen driver tests
- `build/libraries/misc/test/*.uf2` - utility library tests (`argv_test`, `args_test`, `i2c_dev_test`, `ibus_test`, `misc_test`)

Flash the desired `.uf2` file to the Pico (drag-and-drop or use VS Code tasks).

### Working with Submodules

The four libraries are git submodules:

```bash
# Initialize submodules (first time)
git submodule update --init --recursive

# Update all submodules to latest
git submodule update --remote

# Update a specific submodule
cd libraries/framebuffer
git pull origin master
cd ../..
git add libraries/framebuffer
git commit -m "update framebuffer submodule"
```

### Adding New Graphics Features

When extending the framebuffer library:
1. Add virtual methods to `Framebuffer` base class if needed by multiple drivers
2. Implement the generic logic once in `St77xx` (shared by `St7796`/`St7789vi`/`St7789v`) rather than duplicating it per driver; only chip-specific pieces (`init()`, `madctl()`, `spi_cpol()`/`spi_cpha()`) belong in a concrete subclass
3. Consider DMA implications - large transfers should use async operations
4. Update `fb_tests.cpp` (shared by every display's test executable) to demonstrate new features

### Adding a New Display Variant

- If a new panel uses a controller chip this project already drives (`St7796`'s ST7796, `St7789vi`'s ST7789Vi, `St7789v`'s ST7789V) **and** shares that driver's own vendor init sequence, differing only in something narrow like a MADCTL color-order bit (as the three Newhaven parts do for `St7789vi`), add it as a new value of that driver's `Model`-style runtime constructor argument - no new class needed, and no source editing needed to switch parts, per `St7789vi::Model`.
- If a new panel needs a genuinely different init sequence (different register values, different opcodes, different SPI mode - as `St7789v` does versus `St7789vi`, despite the similar chip name), write a new `St77xx` subclass instead: its own `*_cmd.h` command table, `init()`, and `madctl()`, following `St7789v` as the template. Don't try to shoehorn a different init table into an existing driver's `Model` switch.
- Either way, prefer one executable per board/model over compile-time editing (see the three `st7789vi_*_test` executables and `st7789v_test`) so nothing needs source changes to target a specific display.

### Debug Output

All executables use USB serial for stdio:
- `pico_enable_stdio_uart(target 0)` - UART disabled
- `pico_enable_stdio_usb(target 1)` - USB enabled
- `main()` waits for `stdio_usb_connected()` before proceeding
- Use `printf()` for debug output

### Compiler Flags

All targets compile with strict warnings:
```cmake
add_compile_options(-Wall -Wextra -Werror)
```

Warnings are treated as errors. Fix all warnings before committing.

## SDK Configuration

- **Pico SDK Version:** 2.2.0 (set in CMakeLists.txt)
- **Toolchain:** arm-none-eabi-gcc 14_2_Rel1
- **Board:** `PICO_BOARD` is a plain CMake cache variable set in `CMakeLists.txt` (currently `pico2_w`). `PICO_PLATFORM` (`rp2040` vs `rp2350-arm-s`) is derived automatically from it by the Pico SDK.
- **Standards:** C11, C++17

### Targeting a specific board

Everything here is expected to build for any RP2040 or RP235x board (see "Hardware Target" above). To switch targets, either edit the `set(PICO_BOARD ...)` line in `CMakeLists.txt` or pass `-DPICO_BOARD=<board>` when configuring (e.g. `pico`, `pico_w`, `pico2`, `pico2_w`), then reconfigure (`rm -rf build && mkdir build && cd build && cmake -G Ninja ..` - a stale `build/` from a different `PICO_BOARD` won't pick up the change on its own) and rebuild.

### Platform Portability

RP2040 and RP235x differ in low-level details (register layouts, address maps, peripheral quirks) that aren't always obvious from the code. A concrete example hit in this project: `misc/include/misc/util.h`'s `xip_nocache()` computed the DMA-safe (cache-bypassing) XIP address with a hardcoded bit-pattern trick (`(addr & ~0x0f000000) | 0x03000000`) that's only correct on RP2040, where `XIP_NOCACHE_NOALLOC_BASE` happens to be `XIP_BASE + 0x03000000`. On RP235x that offset is `0x04000000`, so the same formula pointed every flash-sourced DMA transfer at an unrelated address - silently reading garbage (which rendered as black, indistinguishable from an already-cleared screen) or hanging outright, on both `St7796` and `St7789v` identically, since the bug was in shared code, not either display driver. Fixed by computing the offset from `XIP_BASE` and reapplying it to the SDK's own `XIP_NOCACHE_NOALLOC_BASE`, correct on both platforms.

When touching low-level/DMA/XIP/memory-map code, prefer the SDK's own named constants and functions (`XIP_BASE`, `XIP_NOCACHE_NOALLOC_BASE`, etc.) over reconstructing addresses or bit patterns by hand, and if a fix or workaround is only verified on one platform, say so in a comment.
