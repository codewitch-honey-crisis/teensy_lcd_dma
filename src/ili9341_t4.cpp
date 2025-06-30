#include "ili9341_t4.hpp"

#include <stdint.h>

#define ILI9341_TFTWIDTH 240
#define ILI9341_TFTHEIGHT 320

#define ILI9341_NOP 0x00
#define ILI9341_SWRESET 0x01
#define ILI9341_RDDID 0x04
#define ILI9341_RDDST 0x09

#define ILI9341_SLPIN 0x10
#define ILI9341_SLPOUT 0x11
#define ILI9341_PTLON 0x12
#define ILI9341_NORON 0x13

#define ILI9341_RDMODE 0x0A
#define ILI9341_RDMADCTL 0x0B
#define ILI9341_RDPIXFMT 0x0C
#define ILI9341_RDIMGFMT 0x0D
#define ILI9341_RDSELFDIAG 0x0F

#define ILI9341_INVOFF 0x20
#define ILI9341_INVON 0x21
#define ILI9341_GAMMASET 0x26
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON 0x29

#define ILI9341_CASET 0x2A
#define ILI9341_PASET 0x2B
#define ILI9341_RAMWR 0x2C
#define ILI9341_RAMRD 0x2E

#define ILI9341_PTLAR 0x30
#define ILI9341_MADCTL 0x36
#define ILI9341_VSCRSADD 0x37
#define ILI9341_PIXFMT 0x3A

#define ILI9341_FRMCTR1 0xB1
#define ILI9341_FRMCTR2 0xB2
#define ILI9341_FRMCTR3 0xB3
#define ILI9341_INVCTR 0xB4
#define ILI9341_DFUNCTR 0xB6

#define ILI9341_PWCTR1 0xC0
#define ILI9341_PWCTR2 0xC1
#define ILI9341_PWCTR3 0xC2
#define ILI9341_PWCTR4 0xC3
#define ILI9341_PWCTR5 0xC4
#define ILI9341_VMCTR1 0xC5
#define ILI9341_VMCTR2 0xC7

#define ILI9341_RDID1 0xDA
#define ILI9341_RDID2 0xDB
#define ILI9341_RDID3 0xDC
#define ILI9341_RDID4 0xDD

#define ILI9341_GMCTRP1 0xE0
#define ILI9341_GMCTRN1 0xE1

#define MADCTL_MY 0x80
#define MADCTL_MX 0x40
#define MADCTL_MV 0x20
#define MADCTL_ML 0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH 0x04

static const uint8_t initList[] = {
    4, 0xEF, 0x03, 0x80, 0x02,
    4, 0xCF, 0x00, 0XC1, 0X30,
    5, 0xED, 0x64, 0x03, 0X12, 0X81,
    4, 0xE8, 0x85, 0x00, 0x78,
    6, 0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02,
    2, 0xF7, 0x20,
    3, 0xEA, 0x00, 0x00,
    2, ILI9341_PWCTR1, 0x23,        // Power control
    2, ILI9341_PWCTR2, 0x10,        // Power control
    3, ILI9341_VMCTR1, 0x3e, 0x28,  // VCM control
    2, ILI9341_VMCTR2, 0x86,        // VCM control2
    2, ILI9341_MADCTL, 0x48,        // Memory Access Control
    2, ILI9341_PIXFMT, 0x55,
    3, ILI9341_FRMCTR1, 0x00, 0x18,
    4, ILI9341_DFUNCTR, 0x08, 0x82, 0x27,  // Display Function Control
    2, 0xF2, 0x00,                         // Gamma Function Disable
    2, ILI9341_GAMMASET, 0x01,             // Gamma curve selected
    16, ILI9341_GMCTRP1, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
    0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,  // Set Gamma
    16, ILI9341_GMCTRN1, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
    0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,  // Set Gamma
    3, 0xb1, 0x00, 0x10,                                   // FrameRate Control 119Hz
    0};

ili9341_t4::ili9341_t4(uint8_t CS, uint8_t RS, uint8_t SID, uint8_t SCLK, uint8_t RST, uint8_t BKL) : lcd_spi_driver_t4(2, true, 50 * 1000 * 1000, CS, RS, SID, SCLK, RST) {
    _rotation = 0;
    _bkl = BKL;
}
ili9341_t4::ili9341_t4(uint8_t CS, uint8_t RS, uint8_t RST, uint8_t BKL) : lcd_spi_driver_t4(2, true, 50 * 1000 * 1000, CS, RS, RST) {
    _rotation = 0;
    _bkl = BKL;
}
uint16_t ili9341_t4::width() const { return (_rotation & 1) ? 320 : 240; }
uint16_t ili9341_t4::height() const { return (_rotation & 1) ? 240 : 320; }
void ili9341_t4::initialize(void) {
    begin_transaction();
    const uint8_t *addr = initList;
    while (1) {
        uint8_t count = *addr++;
        if (count-- == 0) break;
        write_command(*addr++);
        while (count-- > 0) {
            write_data(*addr++);
        }
    }
    write_command_last(ILI9341_SLPOUT);  // Exit Sleep
    end_transaction();

    delay(120);
    begin_transaction();
    write_command_last(ILI9341_DISPON);  // Display on
    end_transaction();
    rotation(0);
    if (_bkl != 0xFF) {
        pinMode(_bkl, OUTPUT);
        digitalWrite(_bkl, HIGH);
    }
}
void ili9341_t4::write_address_window(int x1, int y1, int x2, int y2) {
    write_command(ILI9341_CASET);  // Column addr set
    write_data16(x1);
    write_data16(x2);
    write_command(ILI9341_PASET);  // Row addr set
    write_data16(y1);
    write_data16(y2);
    write_command_last(ILI9341_RAMWR);
}
void ili9341_t4::set_rotation(int value) {
    _rotation = value & 3;
    begin_transaction();
    write_command_last(ILI9341_MADCTL);
    switch (_rotation) {
        case 0:
            write_data_last(MADCTL_MX | MADCTL_BGR);
            break;
        case 1:
            write_data_last(MADCTL_MV | MADCTL_BGR);
            break;
        case 2:
            write_data_last(MADCTL_MY | MADCTL_BGR);
            break;
        case 3:
            write_data_last(MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR);
            break;
    }
    end_transaction();
}