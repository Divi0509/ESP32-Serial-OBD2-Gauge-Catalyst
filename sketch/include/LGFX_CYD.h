#pragma once

/*
  LovyanGFX configuration for the "Cheap Yellow Display" (CYD)
  This board is an ESP32-2432S028R.
*/

#define LGFX_USE_V1

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  // Provide panel parameters
  lgfx::Panel_ILI9341 _panel_instance;

  // Provide bus parameters
  lgfx::Bus_SPI _bus_instance;

  // Provide touch screen parameters
  lgfx::Touch_XPT2046 _touch_instance;

  // Provide backlight control parameters
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = HSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_dc = 2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 15;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1; // ILI9341 does not have a busy pin
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false; // Set to true for BGR order
      cfg.dlen_16bit = false; // must be false unless display not show
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }

    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 300;
      cfg.x_max = 3600;
      cfg.y_min = 3600;
      cfg.y_max = 300;     
      cfg.bus_shared = true; // Touch uses different pins on the same SPI bus
      cfg.offset_rotation = 0;
      cfg.spi_host = VSPI_HOST; // Touch controller is on the HSPI bus
      cfg.pin_int = 36;
      cfg.pin_sclk = 25;
      cfg.pin_mosi = 32;
      cfg.pin_miso = 39;
      cfg.pin_cs = 33;
      cfg.freq = 2500000; // Set to 2.5MHz as per your old config
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 21;
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 0;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};
