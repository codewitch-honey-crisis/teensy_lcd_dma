#include "st7789_t4.hpp"

#include <stdint.h>

#define ST7789_XSTART 0
#define ST7789_YSTART 0

#define ST_CMD_DELAY 0x80  // Delay signifier for command lists

#define ST7789_NOP 0x00
#define ST7789_SWRESET 0x01
#define ST7789_RDDID 0x04
#define ST7789_RDDST 0x09

#define ST7789_SLPIN 0x10
#define ST7789_SLPOUT 0x11
#define ST7789_PTLON 0x12
#define ST7789_NORON 0x13

#define ST7789_INVOFF 0x20
#define ST7789_INVON 0x21
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON 0x29
#define ST7789_CASET 0x2A
#define ST7789_RASET 0x2B
#define ST7789_RAMWR 0x2C
#define ST7789_RAMRD 0x2E

#define ST7789_PTLAR 0x30
#define ST7789_COLMOD 0x3A
#define ST7789_MADCTL 0x36

#define ST7789_RDID1 0xDA
#define ST7789_RDID2 0xDB
#define ST7789_RDID3 0xDC

#define TFT_MADCTL 0x36
#define TFT_COLMOD 0x3A

// Flags for TFT_MADCTL
#define TFT_MAD_MY 0x80
#define TFT_MAD_MX 0x40
#define TFT_MAD_MV 0x20
#define TFT_MAD_ML 0x10
#define TFT_MAD_RGB 0x00
#define TFT_MAD_BGR 0x08
#define TFT_MAD_MH 0x04
#define TFT_MAD_SS 0x02
#define TFT_MAD_GS 0x01

#define TFT_MAD_COLOR_ORDER TFT_MAD_RGB

#define DELAY 0x80
static const uint8_t PROGMEM
  initList[] = {                  // Initialization commands for 7735B screens
    9,                       // 9 commands in list:
    ST7789_SWRESET,   DELAY,  //  1: Software reset, no args, w/delay
      150,                     //    150 ms delay
    ST7789_SLPOUT ,   DELAY,  //  2: Out of sleep mode, no args, w/delay
      255,                    //     255 = 500 ms delay
    ST7789_COLMOD , 1+DELAY,  //  3: Set color mode, 1 arg + delay:
      0x55,                   //     16-bit color
      10,                     //     10 ms delay
    ST7789_MADCTL , 1      ,  //  4: Memory access ctrl (directions), 1 arg:
      0x08,                   //     Row addr/col addr, bottom to top refresh
    ST7789_CASET  , 4      ,  //  5: Column addr set, 4 args, no delay:
      0x00, 
      0x00,                   //     XSTART = 0
      0x00, 
      240,                    //      XEND = 240
    ST7789_RASET  , 4      ,  // 6: Row addr set, 4 args, no delay:
      0x00, 
      0x00,                   //     YSTART = 0
      320>>8, 
      320 & 0xFF,             //      YEND = 320
    ST7789_INVON ,   DELAY,   // 7: hack
      10,
    ST7789_NORON  ,   DELAY,  // 8: Normal display on, no args, w/delay
      10,                     //     10 ms delay
    ST7789_DISPON ,   DELAY,  // 9: Main screen turn on, no args, w/delay
    255 };                  //     255 = 500 ms delay


void st7789_t4::set_dimensions(st7789_t4_res_t resolution) {
    switch(resolution) {
        case st7789_t4_res_t::ST7789_135x240:
            _native_width = 135;
            _native_height = 240;
            break;
        case st7789_t4_res_t::ST7789_170x240:
            _native_width = 170;
            _native_height = 240;
            break;
        case st7789_t4_res_t::ST7789_172x240:
            _native_width = 172;
            _native_height = 240;
            break;
        case st7789_t4_res_t::ST7789_240x240:
            _native_width = 240;
            _native_height = 240;
            break;
        case st7789_t4_res_t::ST7789_240x320:
            _native_width = 240;
            _native_height = 320;
            break;
        case st7789_t4_res_t::ST7789_280x240:
            _native_width = 280;
            _native_height = 240;
            break;
        
    }
}
st7789_t4::st7789_t4(st7789_t4_res_t resolution, uint8_t CS, uint8_t RS, uint8_t SID, uint8_t SCLK, uint8_t RST, uint8_t BKL) : lcd_spi_driver_t4(70 * 1000 * 1000, CS, RS, SID, SCLK, RST) {
    _rotation = 0;
    _offset_x = 0;
    _offset_y = 0;
    _bkl = BKL;
    set_dimensions(resolution);
}
st7789_t4::st7789_t4(st7789_t4_res_t resolution, uint8_t CS, uint8_t RS, uint8_t RST, uint8_t BKL) : lcd_spi_driver_t4(70 * 1000 * 1000, CS, RS, RST) {
    _rotation = 0;
    _offset_x = 0;
    _offset_y = 0;
    _bkl = BKL;
    set_dimensions(resolution);
}
uint16_t st7789_t4::width() const { return (_rotation&1)?_native_height:_native_width; }
uint16_t st7789_t4::height() const { return (_rotation&1)?_native_width:_native_height; }
void st7789_t4::initialize(void) {
    uint8_t  numCommands, numArgs;
	uint16_t ms;
    const uint8_t* addr = initList;
	begin_transaction();
	numCommands = pgm_read_byte(addr++);		// Number of commands to follow
	//Serial.printf("CommandList: numCmds:%d\n", numCommands); Serial.flush();
	while(numCommands--) {				// For each command...
		write_command_last(pgm_read_byte(addr++));	//   Read, issue command
		numArgs  = pgm_read_byte(addr++);	//   Number of args to follow
		ms       = numArgs & DELAY;		//   If hibit set, delay follows args
		numArgs &= ~DELAY;			//   Mask out delay bit
		while(numArgs > 1) {			//   For each argument...
			write_data(pgm_read_byte(addr++)); //   Read, issue argument
			numArgs--;
		}

		if (numArgs) write_data_last(pgm_read_byte(addr++)); //   Read, issue argument - wait until this one completes
		if(ms) {
			ms = pgm_read_byte(addr++);	// Read post-command delay time (ms)
			if(ms == 255) ms = 500;		// If 255, delay for 500 ms
			//Serial.printf("delay %d\n", ms); Serial.flush();
			end_transaction();
			delay(ms);
			begin_transaction();
		}
	}
	end_transaction();

    if(_bkl!=0xFF) {
        pinMode(_bkl,OUTPUT);
        digitalWrite(_bkl, HIGH);
    }
    rotation(0);
}
void st7789_t4::write_address_window(int x1, int y1, int x2, int y2) {
    x1+=_offset_x;x2+=_offset_x;
    y1+=_offset_y;y2+=_offset_y;
    write_command_last(ST7789_CASET);  // Column addr set
    write_data16(x1);
    write_data16_last(x2);
    write_command_last(ST7789_RASET);  // Row addr set
    write_data16(y1);
    write_data16_last(y2);
    write_command_last(ST7789_RAMWR);
}
void st7789_t4::set_rotation(int value) {
    _rotation = value;
    begin_transaction();
    write_command_last(ST7789_MADCTL);
    switch (value & 3) {
        case 0:  // Portrait

            if (_native_width == 135) {
                _offset_x = 52;
                _offset_y = 40;
            } else if (_native_height == 280) {
                _offset_x = 0;
                _offset_y = 20;
            } else if (_native_width == 172) {
                _offset_x = 34;
                _offset_y = 0;
            } else if (_native_width == 170) {
                _offset_x = 35;
                _offset_y = 0;
            } else {
                _offset_x = 0;
                _offset_y = 0;
            }

            write_data_last(TFT_MAD_COLOR_ORDER);

            break;

        case 1:  // Landscape (Portrait + 90)

            if (_native_width == 135) {
                _offset_x = 40;
                _offset_y = 53;
            } else if (_native_height == 280) {
                _offset_x = 20;
                _offset_y = 0;
            } else if (_native_width == 172) {
                _offset_x = 0;
                _offset_y = 34;
            } else if (_native_width == 170) {
                _offset_x = 0;
                _offset_y = 35;
            } else {
                _offset_x = 0;
                _offset_y = 0;
            }

            write_data_last(TFT_MAD_MX | TFT_MAD_MV | TFT_MAD_COLOR_ORDER);

            break;

        case 2:  // Inverter portrait

            if (_native_width == 135) {
                _offset_x = 53;
                _offset_y = 40;
            } else if (_native_height == 280) {
                _offset_x = 0;
                _offset_y = 20;
            } else if (_native_width == 172) {
                _offset_x = 34;
                _offset_y = 0;
            } else if (_native_width == 170) {
                _offset_x = 35;
                _offset_y = 0;
            } else {
                _offset_x = 0;
                _offset_y = 80;
            }

            write_data_last(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_COLOR_ORDER);

            break;
        case 3:  // Inverted landscape

            if (_native_width == 135) {
                _offset_x = 40;
                _offset_y = 52;
            } else if (_native_height == 280) {
                _offset_x = 20;
                _offset_y = 0;
            } else if (_native_width == 172) {
                _offset_x = 0;
                _offset_y = 34;
            } else if (_native_width == 170) {
                _offset_x = 0;
                _offset_y = 35;
            } else {
                _offset_x = 80;
                _offset_y = 0;
            }

            write_data_last(TFT_MAD_MV | TFT_MAD_MY | TFT_MAD_COLOR_ORDER);

            break;
    }
    end_transaction();
}