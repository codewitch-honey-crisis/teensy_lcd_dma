#pragma once
#include <stdint.h>
#include "lcd_spi_driver_t4.hpp"
class ili9341_t4 : public lcd_spi_driver_t4 {
    uint8_t _bkl;    
public:
    ili9341_t4(uint8_t CS, uint8_t RS, uint8_t SID, uint8_t SCLK, uint8_t RST , uint8_t BKL = -1);
    ili9341_t4(uint8_t CS, uint8_t RS, uint8_t RST , uint8_t BKL = -1);
    uint16_t width() const;
    uint16_t height() const;
protected:
    virtual void initialize(void) override;
    virtual void write_address_window(int x1, int y1, int x2, int y2) override;
    virtual void set_rotation(int rotation) override;
};