
#include "lcd_spi_driver_t4.hpp"

lcd_spi_driver_t4* lcd_spi_driver_t4::_dmaActiveDisplay[3] = {0, 0, 0};
lcd_spi_dma_data_t lcd_spi_driver_t4::_dma_data[3];  // one structure for each SPI bus

lcd_spi_driver_t4::lcd_spi_driver_t4(uint8_t color_width, bool swap_color_bytes, uint32_t frequency, uint8_t cs, uint8_t rs, uint8_t sid, uint8_t sclk, uint8_t rst) : hwSPI(false),
                                                                                                                                                                       _cs(cs),
                                                                                                                                                                       _rs(rs),
                                                                                                                                                                       _rst(rst),
                                                                                                                                                                       _sid(sid),
                                                                                                                                                                       _sclk(sclk) {
    _color_width = color_width;
    _color_swap_bytes = swap_color_bytes;
    _freq = frequency;
    _rotation = 0;
    _on_transfer_complete = nullptr;
    _buffer = NULL;
    _dma_state = 0;
}

// Constructor when using hardware SPI.  Faster, but must use SPI pins
// specific to each board type (e.g. 11,13 for Uno, 51,52 for Mega, etc.)
lcd_spi_driver_t4::lcd_spi_driver_t4(uint8_t color_width, bool swap_color_bytes, uint32_t frequency, uint8_t cs, uint8_t rs, uint8_t rst) : hwSPI(true), _cs(cs), _rs(rs), _rst(rst), _sid((uint8_t)-1), _sclk((uint8_t)-1) {
    _color_width = color_width;
    _color_swap_bytes = swap_color_bytes;
    _freq = frequency;
    _rotation = 0;
    _on_transfer_complete = nullptr;
    _buffer = NULL;
    _dma_state = 0;
}

void lcd_spi_driver_t4::dma_interrupt(void) {
    if (_dmaActiveDisplay[0]) {
        _dmaActiveDisplay[0]->process_dma_interrupt();
    }
}
void lcd_spi_driver_t4::dma_interrupt1(void) {
    if (_dmaActiveDisplay[1]) {
        _dmaActiveDisplay[1]->process_dma_interrupt();
    }
}
void lcd_spi_driver_t4::dma_interrupt2(void) {
    if (_dmaActiveDisplay[2]) {
        _dmaActiveDisplay[2]->process_dma_interrupt();
    }
}
void lcd_spi_driver_t4::process_dma_interrupt(void) {
    // See if we are logically done

    // We are in single refresh mode or the user has called cancel so
    // Lets try to release the CS pin
    // Lets wait until FIFO is not empty
    while (_pimxrt_spi->FSR & 0x1f);  // wait until this one is complete

    while (_pimxrt_spi->SR & LPSPI_SR_MBF);  // wait until this one is complete

    _dma_data[_spi_num]._dmatx.clearComplete();
    _pimxrt_spi->FCR = _spi_fcr_save;  // LPSPI_FCR_TXWATER(15); // restore the FSR status...
    _pimxrt_spi->DER = 0;              // DMA no longer doing TX (or RX)

    _pimxrt_spi->CR = LPSPI_CR_MEN | LPSPI_CR_RRF | LPSPI_CR_RTF;  // actually clear both...
    _pimxrt_spi->SR = 0x3f00;                                      // clear out all of the other status...
    _pending_rx_count = 0;                                         // Make sure count is zero
    end_transaction();
    _dma_state &= ~(LCD_SPI_DMA_ACTIVE | LCD_SPI_DMA_FINISH);
    
    if (_on_transfer_complete != nullptr) {
        _on_transfer_complete(_on_transfer_complete_state);
    }

    _dma_data[_spi_num]._dmatx.clearInterrupt();
    _dma_data[_spi_num]._dmatx.clearComplete();
    asm("dsb");
}

bool lcd_spi_driver_t4::init_dma_settings(void) {
    if (_dma_state) {  // should test for init, but...
        return false;  // we already init this.
    }
    uint8_t dmaTXevent = _spi_hardware->tx_dma_channel;
    if (_color_swap_bytes == true && _color_width == 2) {
        _dma_data[_spi_num]._dmasettings[0].sourceBuffer((uint16_t*)_buffer, _count_words * _color_width);
    } else {
        _dma_data[_spi_num]._dmasettings[0].sourceBuffer((uint8_t*)_buffer, _count_words * _color_width);
    }
    _dma_data[_spi_num]._dmasettings[0].destination(_pimxrt_spi->TDR);
    _dma_data[_spi_num]._dmasettings[0].TCD->ATTR_DST = (_color_swap_bytes && _color_width == 2) ? 1 : 0;
    _dma_data[_spi_num]._dmasettings[0].replaceSettingsOnCompletion(_dma_data[_spi_num]._dmasettings[1]);
    _dma_data[_spi_num]._dmasettings[0].interruptAtCompletion();
    _dma_data[_spi_num]._dmatx.begin(true);
    _dma_data[_spi_num]._dmatx.triggerAtHardwareEvent(dmaTXevent);
    _dma_data[_spi_num]._dmatx = _dma_data[_spi_num]._dmasettings[0];

    if (_spi_num == 0)
        _dma_data[_spi_num]._dmatx.attachInterrupt(dma_interrupt);
    else if (_spi_num == 1)
        _dma_data[_spi_num]._dmatx.attachInterrupt(dma_interrupt1);
    else
        _dma_data[_spi_num]._dmatx.attachInterrupt(dma_interrupt2);

    _dma_state = LCD_SPI_DMA_INIT;
    return true;
}

void lcd_spi_driver_t4::wait_transmit_complete(void) {
    uint32_t tmp __attribute__((unused));
    while (_pending_rx_count) {
        if ((_pimxrt_spi->RSR & LPSPI_RSR_RXEMPTY) == 0) {
            tmp = _pimxrt_spi->RDR;  // Read any pending RX bytes in
             _pending_rx_count--;  // decrement count of bytes still levt
        }
    }
    _pimxrt_spi->CR = LPSPI_CR_MEN | LPSPI_CR_RRF;  // Clear RX FIFO
}

#define TCR_MASK (LPSPI_TCR_PCS(3) | LPSPI_TCR_FRAMESZ(31) | LPSPI_TCR_CONT | LPSPI_TCR_RXMSK | LPSPI_TCR_LSBF)

void lcd_spi_driver_t4::maybe_update_tcr(uint32_t requested_tcr_state) {
    if ((_spi_tcr_current & TCR_MASK) != requested_tcr_state) {
        bool dc_state_change = (_spi_tcr_current & LPSPI_TCR_PCS(3)) != (requested_tcr_state & LPSPI_TCR_PCS(3));
        _spi_tcr_current = (_spi_tcr_current & ~TCR_MASK) | requested_tcr_state;
        // only output when Transfer queue is empty.
        if (!dc_state_change || !_dcpinmask) {
            while ((_pimxrt_spi->FSR & 0x1f));
            _pimxrt_spi->TCR = _spi_tcr_current;  // update the TCR

        } else {
            wait_transmit_complete();
            if (requested_tcr_state & LPSPI_TCR_PCS(3))
                DIRECT_WRITE_HIGH(_dcport, _dcpinmask);
            else
                DIRECT_WRITE_LOW(_dcport, _dcpinmask);
            _pimxrt_spi->TCR = _spi_tcr_current & ~(LPSPI_TCR_PCS(3) | LPSPI_TCR_CONT);  // go ahead and update TCR anyway?
        }
    }
}
void lcd_spi_driver_t4::wait_fifo_not_full(void) {
    uint32_t tmp __attribute__((unused));
    do {
        if ((_pimxrt_spi->RSR & LPSPI_RSR_RXEMPTY) == 0) {
            tmp = _pimxrt_spi->RDR;                      // Read any pending RX bytes in
            if (_pending_rx_count) _pending_rx_count--;  // decrement count of bytes still levt
        }
    } while ((_pimxrt_spi->SR & LPSPI_SR_TDF) == 0);
}
void lcd_spi_driver_t4::spi_write(uint8_t c) {
    if (_pspi) {
        _pspi->transfer(c);
    } else {
        // Fast SPI bitbang swiped from LPD8806 library
        for (uint8_t bit = 0x80; bit; bit >>= 1) {
            if (c & bit)
                DIRECT_WRITE_HIGH(_mosiport, _mosipinmask);
            else
                DIRECT_WRITE_LOW(_mosiport, _mosipinmask);
            DIRECT_WRITE_HIGH(_sckport, _sckpinmask);
            asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
            DIRECT_WRITE_LOW(_sckport, _sckpinmask);
        }
    }
}
void lcd_spi_driver_t4::spi_write16(uint16_t c) {
    if (_pspi) {
        _pspi->transfer16(c);
    } else {
        // Fast SPI bitbang swiped from LPD8806 library
        for (uint16_t bit = 0x8000; bit; bit >>= 1) {
            if (c & bit)
                DIRECT_WRITE_HIGH(_mosiport, _mosipinmask);
            else
                DIRECT_WRITE_LOW(_mosiport, _mosipinmask);
            DIRECT_WRITE_HIGH(_sckport, _sckpinmask);
            asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
            DIRECT_WRITE_LOW(_sckport, _sckpinmask);
        }
    }
}

void lcd_spi_driver_t4::begin(void) {
    if (_sid == (uint8_t)-1) _sid = 11;
    if (_sclk == (uint8_t)-1) _sclk = 13;
    if (SPI.pinIsMOSI(_sid) && SPI.pinIsSCK(_sclk)) {
        Serial.println("LCD SPI Driver - Hardware SPI");
        _pspi = &SPI;
        _spi_num = 0;                   // Which bus is this spi on?
        _pimxrt_spi = &IMXRT_LPSPI4_S;  // Could hack our way to grab this from SPI object, but...

    } else if (SPI1.pinIsMOSI(_sid) && SPI1.pinIsSCK(_sclk)) {
        Serial.println("LCD SPI Driver - Hardware SPI1");
        _pspi = &SPI1;
        _spi_num = 1;  // Which buss is this spi on?
        _pimxrt_spi = &IMXRT_LPSPI3_S;
    } else if (SPI2.pinIsMOSI(_sid) && SPI2.pinIsSCK(_sclk)) {
        Serial.println("LCD SPI Driver - Hardware SPI2");
        _pspi = &SPI2;
        _spi_num = 2;  // Which buss is this spi on?
        _pimxrt_spi = &IMXRT_LPSPI1_S;
    } else {
        _pspi = nullptr;
    }
    _spiSettings = SPISettings(_freq, MSBFIRST, SPI_MODE0);
    if (_pspi) {
        hwSPI = true;
        _pspi->begin();
        _pending_rx_count = 0;
        _pspi->beginTransaction(_spiSettings);  // Should have our settings.
        _pspi->transfer(0);                     // hack to see if it will actually change then...
        _pspi->endTransaction();
        _spi_tcr_current = _pimxrt_spi->TCR;  // get the current TCR value
        uint32_t* pa = (uint32_t*)((void*)_pspi);
        _spi_hardware = (SPIClass::SPI_Hardware_t*)(void*)pa[1];

    } else {
        hwSPI = false;
        _sckport = portOutputRegister(_sclk);
        _sckpinmask = digitalPinToBitMask(_sclk);
        pinMode(_sclk, OUTPUT);
        DIRECT_WRITE_LOW(_sckport, _sckpinmask);

        _mosiport = portOutputRegister(_sid);
        _mosipinmask = digitalPinToBitMask(_sid);
        pinMode(_sid, OUTPUT);
        DIRECT_WRITE_LOW(_mosiport, _mosipinmask);
    }
    if (_cs != 0xff) {
        _csport = portOutputRegister(_cs);
        _cspinmask = digitalPinToBitMask(_cs);
        pinMode(_cs, OUTPUT);
        DIRECT_WRITE_HIGH(_csport, _cspinmask);
    } else {
        _csport = 0;
    }

    if (_pspi && _pspi->pinIsChipSelect(_rs)) {
        uint8_t dc_cs_index = _pspi->setCS(_rs);
        _dcport = 0;
        _dcpinmask = 0;
        dc_cs_index--;	// convert to 0 based
		_tcr_dc_assert = LPSPI_TCR_PCS(dc_cs_index);
    	_tcr_dc_not_assert = LPSPI_TCR_PCS(3);
    } else {
        // Serial.println("Error not DC is not valid hardware CS pin");
        _dcport = portOutputRegister(_rs);
        _dcpinmask = digitalPinToBitMask(_rs);
        pinMode(_rs, OUTPUT);
        DIRECT_WRITE_HIGH(_dcport, _dcpinmask);
        _tcr_dc_assert = LPSPI_TCR_PCS(0);
    	_tcr_dc_not_assert = LPSPI_TCR_PCS(1);
    }
    maybe_update_tcr(_tcr_dc_not_assert | LPSPI_TCR_FRAMESZ(7));
    if (_rst != 0xff) {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, HIGH);
        delay(5);//delay(100);
        digitalWrite(_rst, LOW);
        delay(20);//delay(100);
        digitalWrite(_rst, HIGH);
        delay(150);//delay(200);
    }
    initialize();
}
void lcd_spi_driver_t4::on_flush_complete_callback(lcd_spi_on_flush_complete_callback_t callback, void* state) {
    this->_on_transfer_complete = callback;
    _on_transfer_complete_state = state;
}
bool lcd_spi_driver_t4::flush16_async(int x1, int y1, int x2, int y2, const void* bitmap, bool flush_cache) {
    // Don't start one if already active.
    if (_dma_state & LCD_SPI_DMA_ACTIVE) {
        Serial.println("DMA IN PROGRESS");
        return false;
    }
    _buffer = (uint16_t*)bitmap;
    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;
    _count_words = w * h;
    size_t bytes = _count_words * 2;
    if (flush_cache) {
        arm_dcache_flush((void*)_buffer, bytes);
    }
    if (!init_dma_settings()) {
        _dma_data[_spi_num]._dmasettings[0].sourceBuffer((const uint16_t*)_buffer, bytes);
    }
       

    // Start off remove disable on completion...
    // it will be the ISR that disables it...
    _dma_data[_spi_num]._dmasettings[0].TCD->CSR &= ~(DMA_TCD_CSR_DREQ);
    _dma_data[_spi_num]._dmasettings[1].TCD->CSR &= ~(DMA_TCD_CSR_DREQ);
    begin_transaction();
    write_address_window(x1, y1, x2, y2);
    // Update TCR to 16 bit mode.
    _spi_fcr_save = _pimxrt_spi->FCR;  // remember the FCR
    _pimxrt_spi->FCR = 0;              // clear water marks...
    maybe_update_tcr(_tcr_dc_not_assert | LPSPI_TCR_FRAMESZ(15) | LPSPI_TCR_RXMSK /*| LPSPI_TCR_CONT*/);
    _pimxrt_spi->DER = LPSPI_DER_TDDE;
    _pimxrt_spi->SR = 0x3f00;  // clear out all of the other status...
    _dma_data[_spi_num]._dmatx = _dma_data[_spi_num]._dmasettings[0];
    _dma_data[_spi_num]._dmatx.triggerAtHardwareEvent(_spi_hardware->tx_dma_channel);

    _dmaActiveDisplay[_spi_num] = this;    
    
    _dma_data[_spi_num]._dmatx.begin(false);
    _dma_data[_spi_num]._dmatx.disableOnCompletion();
    _dma_data[_spi_num]._dmatx.enable();
    _dma_state &= ~LCD_SPI_DMA_CONT;
    _dma_state |= LCD_SPI_DMA_ACTIVE;
    return true;
}
bool lcd_spi_driver_t4::flush8_async(int x1, int y1, int x2, int y2, const void* bitmap, bool flush_cache) {
    // Don't start one if already active.
    if (_dma_state & LCD_SPI_DMA_ACTIVE) {
        Serial.println("DMA IN PROGRESS");
        return false;
    }
    _buffer = (uint16_t*)bitmap;
    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;
    _count_words = w * h;
    // count pixels * 2 at 16-bit color
    size_t bytes = _count_words * _color_width;
    if (flush_cache) {
        arm_dcache_flush((void*)_buffer, bytes);
    }
    init_dma_settings();

    _dma_data[_spi_num]._dmasettings[0].sourceBuffer((const uint8_t*)_buffer, bytes);
    // Start off remove disable on completion from both...
    // it will be the ISR that disables it...
    _dma_data[_spi_num]._dmasettings[0].TCD->CSR &= ~(DMA_TCD_CSR_DREQ);
    _dma_data[_spi_num]._dmasettings[1].TCD->CSR &= ~(DMA_TCD_CSR_DREQ);
    begin_transaction();
    write_address_window(x1, y1, x2, y2);
    _spi_fcr_save = _pimxrt_spi->FCR;  // remember the FCR
    _pimxrt_spi->FCR = 0;              // clear water marks...
    // Update TCR to 8 bit mode.
    maybe_update_tcr(_tcr_dc_not_assert | LPSPI_TCR_PCS(1) | LPSPI_TCR_FRAMESZ(7) | LPSPI_TCR_RXMSK /*| LPSPI_TCR_CONT*/);
    _pimxrt_spi->DER = LPSPI_DER_TDDE;
    _pimxrt_spi->SR = 0x3f00;  // clear out all of the other status...
    _dma_data[_spi_num]._dmatx.triggerAtHardwareEvent(_spi_hardware->tx_dma_channel);

    _dma_data[_spi_num]._dmatx = _dma_data[_spi_num]._dmasettings[0];

    _dma_data[_spi_num]._dmatx.begin(false);
    _dma_data[_spi_num]._dmatx.enable();

    _dmaActiveDisplay[_spi_num] = this;
    _dma_state &= ~LCD_SPI_DMA_CONT;
    _dma_state |= LCD_SPI_DMA_ACTIVE;
    return true;
}
bool lcd_spi_driver_t4::flush16(int x1, int y1, int x2, int y2, const void* bitmap) {
    if (_dma_state & LCD_SPI_DMA_ACTIVE) {
        Serial.println("DMA IN PROGRESS");
        return false;
    }
    size_t size = (x2 - x1 + 1) * (y2 - y1 + 1);
    const uint16_t* p = (const uint16_t*)bitmap;
    begin_transaction();
    write_address_window(x1, y1, x2, y2);
    while (size > 1) {
        write_data16(*(p++));
        --size;
    }
    if (size) {
        write_data16_last(*p);
    }
    end_transaction();
    if (_on_transfer_complete != nullptr) {
        _on_transfer_complete(_on_transfer_complete_state);
    }
    return true;
}
bool lcd_spi_driver_t4::flush8(int x1, int y1, int x2, int y2, const void* bitmap) {
    if (_dma_state & LCD_SPI_DMA_ACTIVE) {
        Serial.println("DMA IN PROGRESS");
        return false;
    }
    size_t size = (x2 - x1 + 1) * (y2 - y1 + 1) * 2;
    const uint16_t* p = (const uint16_t*)bitmap;
    begin_transaction();
    write_address_window(x1, y1, x2, y2);
    while (size > 1) {
        write_data(*(p++));
        --size;
    }
    if (size) {
        write_data_last(*p);
    }
    end_transaction();
    if (_on_transfer_complete != nullptr) {
        _on_transfer_complete(_on_transfer_complete_state);
    }
    return true;
}

void lcd_spi_driver_t4::rotation(int value) {
    _rotation = value;
    set_rotation(value);
}
