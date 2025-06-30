/***************************************************
  This is a library for the Adafruit 1.8" SPI display.
  This library works with the Adafruit 1.8" TFT Breakout w/SD card
  ----> http://www.adafruit.com/products/358
  as well as Adafruit raw 1.8" TFT display
  ----> http://www.adafruit.com/products/618

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#pragma once
#include <SPI.h>
typedef void (*lcd_spi_on_flush_complete_callback_t)(void *state);
#define LCD_SPI_DMA_TCR_MASK (LPSPI_TCR_PCS(3) | LPSPI_TCR_FRAMESZ(31) | LPSPI_TCR_CONT | LPSPI_TCR_RXMSK)
#define LCD_SPI_DMA_BUFFER_SIZE 512
typedef struct {
    DMASetting _dmasettings[2];
    DMAChannel _dmatx;
} lcd_spi_dma_data_t;

class lcd_spi_driver_t4 {
    enum { LCD_SPI_DMA_INIT = 0x01,
           LCD_SPI_DMA_CONT = 0x02,
           LCD_SPI_DMA_FINISH = 0x04,
           LCD_SPI_DMA_ACTIVE = 0x80 };
    SPIClass *_pspi = nullptr;
    uint8_t _spi_num = 0;  // Which bus is this spi on?
    IMXRT_LPSPI_t *_pimxrt_spi = nullptr;
    SPIClass::SPI_Hardware_t *_spi_hardware;
    uint8_t _pending_rx_count = 0;
    uint32_t _spi_tcr_current = 0;
    SPISettings _spiSettings;
    volatile uint8_t _dma_state;  // DMA status
    uint32_t _count_words;
    uint32_t _color_width;
    const uint16_t *_buffer;
    bool hwSPI;
    uint8_t _cs, _rs, _rst, _sid, _sclk;
    uint32_t _freq;
    uint32_t _cspinmask;
    volatile uint32_t *_csport;
    uint32_t _dcpinmask;
    volatile uint32_t *_dcport;
    uint32_t _mosipinmask;
    volatile uint32_t *_mosiport;
    uint32_t _sckpinmask;
    volatile uint32_t *_sckport;
    uint8_t _rotation;
    bool _color_swap_bytes;
    static lcd_spi_driver_t4 *_dmaActiveDisplay[3];  // Use pointer to this as a way to get back to object...
    static lcd_spi_dma_data_t _dma_data[3];          // one structure for each SPI bus...
    // try work around DMA memory cached.  So have a couple of buffers we copy frame buffer into
    // as to move it out of the memory that is cached...
    uint32_t _spi_fcr_save;  // save away previous FCR register value
    lcd_spi_on_flush_complete_callback_t _on_transfer_complete = nullptr;
    void *_on_transfer_complete_state = nullptr;

    inline void DIRECT_WRITE_LOW(volatile uint32_t *base, uint32_t mask) __attribute__((always_inline)) {
        *(base + 34) = mask;
    }
    inline void DIRECT_WRITE_HIGH(volatile uint32_t *base, uint32_t mask) __attribute__((always_inline)) {
        *(base + 33) = mask;
    }
    void spi_write(uint8_t value);
    void spi_write16(uint16_t value);
    void wait_transmit_complete(void);
    void wait_fifo_not_full(void);
    void maybe_update_tcr(uint32_t requested_tcr_state);
    static void dma_interrupt(void);
    static void dma_interrupt1(void);
    static void dma_interrupt2(void);
    bool init_dma_settings(void);
    void process_dma_interrupt(void);
    bool flush16_async(int x1, int y1, int x2, int y2, const void *bitmap, bool flush_cache);
    bool flush8_async(int x1, int y1, int x2, int y2, const void *bitmap, bool flush_cache);
    bool flush16(int x1, int y1, int x2, int y2, const void *bitmap);
    bool flush8(int x1, int y1, int x2, int y2, const void *bitmap);

   protected:
    inline void begin_transaction(void) __attribute__((always_inline)) {
        if (hwSPI) _pspi->beginTransaction(_spiSettings);
        if (_csport) DIRECT_WRITE_LOW(_csport, _cspinmask);
    }
    inline void end_transaction(void) __attribute__((always_inline)) {
        if (_csport) DIRECT_WRITE_HIGH(_csport, _cspinmask);
        if (hwSPI) _pspi->endTransaction();
    }
    inline void write_command(uint8_t cmd) __attribute__((always_inline)) {
        if (hwSPI) {
            maybe_update_tcr(LPSPI_TCR_PCS(0) | LPSPI_TCR_FRAMESZ(7));
            _pimxrt_spi->TDR = cmd;
            _pending_rx_count++;  //
            wait_fifo_not_full();
        } else {
            DIRECT_WRITE_LOW(_dcport, _dcpinmask);
            spi_write(cmd);
        }
    }
    inline void write_command_last(uint8_t cmd) __attribute__((always_inline)) {
        if (hwSPI) {
            maybe_update_tcr(LPSPI_TCR_PCS(0) | LPSPI_TCR_FRAMESZ(7));
            _pimxrt_spi->TDR = cmd;
            _pending_rx_count++;  //
            wait_transmit_complete();
        } else {
            DIRECT_WRITE_LOW(_dcport, _dcpinmask);
            spi_write(cmd);
        }
    }
    inline void write_data(uint8_t data) __attribute__((always_inline)) {
        if (hwSPI) {
            maybe_update_tcr(LPSPI_TCR_PCS(1) | LPSPI_TCR_FRAMESZ(7));
            _pimxrt_spi->TDR = data;
            _pending_rx_count++;  //
            wait_fifo_not_full();
        } else {
            DIRECT_WRITE_HIGH(_dcport, _dcpinmask);
            spi_write(data);
        }
    }
    inline void write_data_last(uint8_t data) __attribute__((always_inline)) {
        if (hwSPI) {
            maybe_update_tcr(LPSPI_TCR_PCS(1) | LPSPI_TCR_FRAMESZ(7));
            _pimxrt_spi->TDR = data;
            _pending_rx_count++;  //
            wait_transmit_complete();
        } else {
            DIRECT_WRITE_HIGH(_dcport, _dcpinmask);
            spi_write(data);
        }
    }
    inline void write_data16(uint16_t data) __attribute__((always_inline)) {
        if (hwSPI) {
            maybe_update_tcr(LPSPI_TCR_PCS(1) | LPSPI_TCR_FRAMESZ(15) | LPSPI_TCR_CONT);
            _pimxrt_spi->TDR = data;
            _pending_rx_count++;  //
            wait_fifo_not_full();
        } else {
            DIRECT_WRITE_HIGH(_dcport, _dcpinmask);
            spi_write16(data);
        }
    }
    inline void write_data16_last(uint16_t data) __attribute__((always_inline)) {
        if (hwSPI) {
            maybe_update_tcr(LPSPI_TCR_PCS(1) | LPSPI_TCR_FRAMESZ(15));
            _pimxrt_spi->TDR = data;
            _pending_rx_count++;  //
            wait_transmit_complete();
        } else {
            DIRECT_WRITE_HIGH(_dcport, _dcpinmask);
            spi_write16(data);
        }
    }
    lcd_spi_driver_t4(uint8_t color_width, bool swap_color_bytes, uint32_t frequency, uint8_t CS, uint8_t RS, uint8_t SID, uint8_t SCLK, uint8_t RST = -1);
    lcd_spi_driver_t4(uint8_t color_width, bool swap_color_bytes, uint32_t frequency, uint8_t CS, uint8_t RS, uint8_t RST = -1);

    virtual void initialize(void) = 0;
    virtual void write_address_window(int x1, int y1, int x2, int y2) = 0;
    virtual void set_rotation(int rotation) = 0;

   public:
    void begin(void);
    void rotation(int value);
    void on_flush_complete_callback(lcd_spi_on_flush_complete_callback_t callback, void *state = nullptr);
    inline bool flush_async(int x1, int y1, int x2, int y2, const void *bitmap, bool flush_cache) __attribute__((always_inline)) {
        if (!_color_swap_bytes || _color_width != 2) {
            return flush8_async(x1, y1, x2, y2, bitmap, flush_cache);
        }
        return flush16_async(x1, y1, x2, y2, bitmap, flush_cache);
    }
    inline bool flush(int x1, int y1, int x2, int y2, const void *bitmap) __attribute__((always_inline)) {
        if (!_color_swap_bytes || _color_width != 2) {
            return flush8(x1, y1, x2, y2, bitmap);
        }
        return flush16(x1, y1, x2, y2, bitmap);
    }
};