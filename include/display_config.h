#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"

class LGFX_RoundTFT : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;
    lgfx::Light_PWM    _light_instance;

public:
    LGFX_RoundTFT(void) {
        { // Configure SPI Bus for ESP32-C3 Super Mini
            auto cfg = _bus_instance.config();
            cfg.spi_host   = SPI2_HOST;     // ESP32-C3 hardware SPI host
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;      // 40 MHz crisp SPI updates
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = true;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = PIN_TFT_SCLK;
            cfg.pin_mosi   = PIN_TFT_MOSI;
            cfg.pin_miso   = -1;            // Not used on GC9A01
            cfg.pin_dc     = PIN_TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        { // Configure GC9A01 240x240 Round Panel
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = PIN_TFT_CS;
            cfg.pin_rst          = PIN_TFT_RST;
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;
            cfg.panel_height     = 240;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;       // Adjust 0-3 if screen orientation is rotated
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = true;    // GC9A01 IPS panels use inverted colors by default
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }

        { // Configure PWM Backlight (if BLK pin is wired to GPIO 1)
            auto cfg = _light_instance.config();
            cfg.pin_bl      = PIN_TFT_BL;
            cfg.invert      = false;
            cfg.freq        = 12000;
            cfg.pwm_channel = 0;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};
