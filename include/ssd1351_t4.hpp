#pragma once
#include <stdint.h>
#include "lcd_spi_driver_t4.hpp"
class ssd1351_t4 : public lcd_spi_driver_t4 {
public:
    ssd1351_t4(uint8_t CS, uint8_t RS, uint8_t SID, uint8_t SCLK, uint8_t RST = -1);
    ssd1351_t4(uint8_t CS, uint8_t RS, uint8_t RST = -1);
    
protected:
    virtual void initialize(void) override;
    virtual void write_address_window(int x1, int y1, int x2, int y2) override;
    virtual void set_rotation(int rotation) override;
};