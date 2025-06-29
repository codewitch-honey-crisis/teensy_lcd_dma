#pragma once
#include <stdint.h>
#include "lcd_spi_driver_t4.hpp"
typedef enum {
    ST7789_240x320=0,
    ST7789_240x240=1,
    ST7789_135x240=2,
    ST7789_280x240=3,
    ST7789_170x240=4,
    ST7789_172x240=5
} st7789_t4_res_t;
class st7789_t4 : public lcd_spi_driver_t4 {
    uint16_t _native_width;
    uint16_t _native_height;
    uint16_t _offset_x;
    uint16_t _offset_y;
    uint8_t _rotation;
    uint8_t _bkl;
    void set_dimensions(st7789_t4_res_t resolution);
public:
    st7789_t4(st7789_t4_res_t resolution, uint8_t CS, uint8_t RS, uint8_t SID, uint8_t SCLK, uint8_t RST , uint8_t BKL = -1);
    st7789_t4(st7789_t4_res_t resolution, uint8_t CS, uint8_t RS, uint8_t RST ,uint8_t BKL = -1);
    uint16_t width() const;
    uint16_t height() const;
protected:
    virtual void initialize(void) override;
    virtual void write_address_window(int x1, int y1, int x2, int y2) override;
    virtual void set_rotation(int rotation) override;
};