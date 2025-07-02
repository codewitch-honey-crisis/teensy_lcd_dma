#include "ssd1351_t4.hpp"

#include <stdint.h>
#define SSD1351_SPICLOCK 18000000
// These #defines are DEPRECATED but present for older code compatibility:
#define SSD1351WIDTH 128   ///< DEPRECATED screen width
#define SSD1351HEIGHT 128  ///< DEPRECATED screen height, set to 96 for 1.27"
// (NEW CODE SHOULD IGNORE THIS, USE THE CONSTRUCTORS THAT ACCEPT WIDTH
// AND HEIGHT ARGUMENTS).

#define SSD1351_CMD_NOP 0x00
#define SSD1351_CMD_SETCOLUMN 0x15       ///< See datasheet
#define SSD1351_CMD_SETROW 0x75          ///< See datasheet
#define SSD1351_CMD_WRITERAM 0x5C        ///< See datasheet
#define SSD1351_CMD_READRAM 0x5D         ///< Not currently used
#define SSD1351_CMD_SETREMAP 0xA0        ///< See datasheet
#define SSD1351_CMD_STARTLINE 0xA1       ///< See datasheet
#define SSD1351_CMD_DISPLAYOFFSET 0xA2   ///< See datasheet
#define SSD1351_CMD_DISPLAYALLOFF 0xA4   ///< Not currently used
#define SSD1351_CMD_DISPLAYALLON 0xA5    ///< Not currently used
#define SSD1351_CMD_NORMALDISPLAY 0xA6   ///< See datasheet
#define SSD1351_CMD_INVERTDISPLAY 0xA7   ///< See datasheet
#define SSD1351_CMD_FUNCTIONSELECT 0xAB  ///< See datasheet
#define SSD1351_CMD_DISPLAYOFF 0xAE      ///< See datasheet
#define SSD1351_CMD_DISPLAYON 0xAF       ///< See datasheet
#define SSD1351_CMD_PRECHARGE 0xB1       ///< See datasheet
#define SSD1351_CMD_DISPLAYENHANCE 0xB2  ///< Not currently used
#define SSD1351_CMD_CLOCKDIV 0xB3        ///< See datasheet
#define SSD1351_CMD_SETVSL 0xB4          ///< See datasheet
#define SSD1351_CMD_SETGPIO 0xB5         ///< See datasheet
#define SSD1351_CMD_PRECHARGE2 0xB6      ///< See datasheet
#define SSD1351_CMD_SETGRAY 0xB8         ///< Not currently used
#define SSD1351_CMD_USELUT 0xB9          ///< Not currently used
#define SSD1351_CMD_PRECHARGELEVEL 0xBB  ///< Not currently used
#define SSD1351_CMD_VCOMH 0xBE           ///< See datasheet
#define SSD1351_CMD_CONTRASTABC 0xC1     ///< See datasheet
#define SSD1351_CMD_CONTRASTMASTER 0xC7  ///< See datasheet
#define SSD1351_CMD_MUXRATIO 0xCA        ///< See datasheet
#define SSD1351_CMD_COMMANDLOCK 0xFD     ///< See datasheet
#define SSD1351_CMD_HORIZSCROLL 0x96     ///< Not currently used
#define SSD1351_CMD_STOPSCROLL 0x9E      ///< Not currently used
#define SSD1351_CMD_STARTSCROLL 0x9F     ///< Not currently used

static const uint8_t PROGMEM initList[] = {
    SSD1351_CMD_COMMANDLOCK, 1,  // Set command lock, 1 arg
    0x12,
    SSD1351_CMD_COMMANDLOCK, 1,  // Set command lock, 1 arg
    0xB1,
    SSD1351_CMD_DISPLAYOFF, 0,  // Display off, no args
    SSD1351_CMD_CLOCKDIV, 1,
    0xF1,  // 7:4 = Oscillator Freq, 3:0 = CLK Div Ratio (A[3:0]+1 = 1..16)
    SSD1351_CMD_MUXRATIO, 1,
    127,
    SSD1351_CMD_DISPLAYOFFSET, 1,
    0x0,
    SSD1351_CMD_SETGPIO, 1,
    0x00,
    SSD1351_CMD_FUNCTIONSELECT, 1,
    0x01,  // internal (diode drop)
    SSD1351_CMD_PRECHARGE, 1,
    0x32,
    SSD1351_CMD_VCOMH, 1,
    0x05,
    SSD1351_CMD_NORMALDISPLAY, 0,
    SSD1351_CMD_CONTRASTABC, 3,
    0xC8, 0x80, 0xC8,
    SSD1351_CMD_CONTRASTMASTER, 1,
    0x0F,
    SSD1351_CMD_SETVSL, 3,
    0xA0, 0xB5, 0x55,
    SSD1351_CMD_PRECHARGE2, 1,
    0x01,
    SSD1351_CMD_DISPLAYON, 0,  // Main screen turn on
    0};                        // END OF COMMAND LIST

ssd1351_t4::ssd1351_t4(uint8_t CS, uint8_t RS, uint8_t SID, uint8_t SCLK, uint8_t RST) : lcd_spi_driver_t4(2, true, 20 * 1000 * 1000, CS, RS, SID, SCLK, RST) {
}
ssd1351_t4::ssd1351_t4(uint8_t CS, uint8_t RS, uint8_t RST) : lcd_spi_driver_t4(2, true, 20 * 1000 * 1000, CS, RS, RST) {
}

void ssd1351_t4::initialize(void) {
    uint8_t numArgs;
    const uint8_t* addr = initList;
    begin_transaction();
    while (*addr) {                   // For each command...
        write_command_last(*addr++);  //   Read, issue command
        numArgs = *addr++;            //   Number of args to follow
        while (numArgs > 1) {         //   For each argument...
            write_data(*addr++);      //   Read, issue argument
            numArgs--;
        }

        if (numArgs) write_data_last(*addr++);  //   Read, issue argument - wait until this one completes
    }
    end_transaction();
    rotation(0);
}
void ssd1351_t4::write_address_window(int x1, int y1, int x2, int y2) {
    write_command(SSD1351_CMD_SETCOLUMN);  // Column addr set
    write_data(x1);                             // XSTART
    write_data(x2);                        // XEND
    write_command(SSD1351_CMD_SETROW);     // Row addr set
    write_data(y1);                             // YSTART
    write_data(y2);                        // YEND
    write_command_last(SSD1351_CMD_WRITERAM);
}
void ssd1351_t4::set_rotation(int value) {
    begin_transaction();
    uint8_t madctl = 0b01100100;  // 0b01100100; // 64K, enable split, CBA
    int r = value & 3;            // Clip input to valid range

    switch (r) {
        case 0:
            madctl |= 0x10;  // Scan bottom-up
            break;
        case 1:                    // doesn't work!
            madctl |= 0b00010011;  // Scan bottom-up, column remap 127-0, vertical
            break;
        case 2:
            madctl |= 0x00;  // Column remap 127-0
            break;
        case 3:                    // doesn't work!
            madctl |= 0b00000001;  // Vertical
            break;
    }

    write_command_last(SSD1351_CMD_SETREMAP);
    write_data_last(madctl);
    uint8_t startline = (r < 2) ? SSD1351HEIGHT : 0;
    write_command_last(SSD1351_CMD_STARTLINE);
    write_data_last(startline);
    end_transaction();
}