
#include <Arduino.h>
#include "ili9341_t4.hpp"
#include "gfx.h"
#include "uix.h"
#define VGA_8X8_IMPLEMENTATION
#include "assets/vga_8x8.h"

using namespace gfx;
using namespace uix;

// Screen dimension
const uint16_t SCREEN_WIDTH = 240;
const uint16_t SCREEN_HEIGHT = 320;

using px_t = pixel<channel_traits<channel_name::R,5>,channel_traits<channel_name::G,6>,channel_traits<channel_name::B,5>>;

using screen_t = uix::screen<px_t>;
using color_t = color<screen_t::pixel_type>;
using uix_color_t = color<rgba_pixel<32>>;

#define USE_SPANS
// fire stuff
#define V_WIDTH (240 / 4)
#define V_HEIGHT (320 / 4)
#define BUF_WIDTH (240 / 4)
#define BUF_HEIGHT ((320 / 4) + 6)
#define PALETTE_SIZE (256 * 3)
#define INT_SIZE 2
#ifdef USE_SPANS
#define PAL_TYPE uint16_t
// store preswapped uint16_ts for performance
// these are computed by the compiler, and
// resolve to literal uint16_t values
#define RGB(r,g,b) (rgb_pixel<16>(r,g,b))
#else
// store rgb_pixel<16> instances
// these are computed by the compiler, and
// resolve to literal uint16_t values
#define PAL_TYPE rgb_pixel<16>
#define RGB(r,g,b) rgb_pixel<16>(r,g,b)
#endif

// color palette for flames
static PAL_TYPE fire_palette[] = {
    RGB(0, 0, 0),    RGB(0, 0, 3),    RGB(0, 0, 3),
    RGB(0, 0, 3),    RGB(0, 0, 4),    RGB(0, 0, 4),
    RGB(0, 0, 4),    RGB(0, 0, 5),    RGB(1, 0, 5),
    RGB(2, 0, 4),    RGB(3, 0, 4),    RGB(4, 0, 4),
    RGB(5, 0, 3),    RGB(6, 0, 3),    RGB(7, 0, 3),
    RGB(8, 0, 2),    RGB(9, 0, 2),    RGB(10, 0, 2),
    RGB(11, 0, 2),   RGB(12, 0, 1),   RGB(13, 0, 1),
    RGB(14, 0, 1),   RGB(15, 0, 0),   RGB(16, 0, 0),
    RGB(16, 0, 0),   RGB(16, 0, 0),   RGB(17, 0, 0),
    RGB(17, 0, 0),   RGB(18, 0, 0),   RGB(18, 0, 0),
    RGB(18, 0, 0),   RGB(19, 0, 0),   RGB(19, 0, 0),
    RGB(20, 0, 0),   RGB(20, 0, 0),   RGB(20, 0, 0),
    RGB(21, 0, 0),   RGB(21, 0, 0),   RGB(22, 0, 0),
    RGB(22, 0, 0),   RGB(23, 1, 0),   RGB(23, 1, 0),
    RGB(24, 2, 0),   RGB(24, 2, 0),   RGB(25, 3, 0),
    RGB(25, 3, 0),   RGB(26, 4, 0),   RGB(26, 4, 0),
    RGB(27, 5, 0),   RGB(27, 5, 0),   RGB(28, 6, 0),
    RGB(28, 6, 0),   RGB(29, 7, 0),   RGB(29, 7, 0),
    RGB(30, 8, 0),   RGB(30, 8, 0),   RGB(31, 9, 0),
    RGB(31, 9, 0),   RGB(31, 10, 0),  RGB(31, 10, 0),
    RGB(31, 11, 0),  RGB(31, 11, 0),  RGB(31, 12, 0),
    RGB(31, 12, 0),  RGB(31, 13, 0),  RGB(31, 13, 0),
    RGB(31, 14, 0),  RGB(31, 14, 0),  RGB(31, 15, 0),
    RGB(31, 15, 0),  RGB(31, 16, 0),  RGB(31, 16, 0),
    RGB(31, 17, 0),  RGB(31, 17, 0),  RGB(31, 18, 0),
    RGB(31, 18, 0),  RGB(31, 19, 0),  RGB(31, 19, 0),
    RGB(31, 20, 0),  RGB(31, 20, 0),  RGB(31, 21, 0),
    RGB(31, 21, 0),  RGB(31, 22, 0),  RGB(31, 22, 0),
    RGB(31, 23, 0),  RGB(31, 24, 0),  RGB(31, 24, 0),
    RGB(31, 25, 0),  RGB(31, 25, 0),  RGB(31, 26, 0),
    RGB(31, 26, 0),  RGB(31, 27, 0),  RGB(31, 27, 0),
    RGB(31, 28, 0),  RGB(31, 28, 0),  RGB(31, 29, 0),
    RGB(31, 29, 0),  RGB(31, 30, 0),  RGB(31, 30, 0),
    RGB(31, 31, 0),  RGB(31, 31, 0),  RGB(31, 32, 0),
    RGB(31, 32, 0),  RGB(31, 33, 0),  RGB(31, 33, 0),
    RGB(31, 34, 0),  RGB(31, 34, 0),  RGB(31, 35, 0),
    RGB(31, 35, 0),  RGB(31, 36, 0),  RGB(31, 36, 0),
    RGB(31, 37, 0),  RGB(31, 38, 0),  RGB(31, 38, 0),
    RGB(31, 39, 0),  RGB(31, 39, 0),  RGB(31, 40, 0),
    RGB(31, 40, 0),  RGB(31, 41, 0),  RGB(31, 41, 0),
    RGB(31, 42, 0),  RGB(31, 42, 0),  RGB(31, 43, 0),
    RGB(31, 43, 0),  RGB(31, 44, 0),  RGB(31, 44, 0),
    RGB(31, 45, 0),  RGB(31, 45, 0),  RGB(31, 46, 0),
    RGB(31, 46, 0),  RGB(31, 47, 0),  RGB(31, 47, 0),
    RGB(31, 48, 0),  RGB(31, 48, 0),  RGB(31, 49, 0),
    RGB(31, 49, 0),  RGB(31, 50, 0),  RGB(31, 50, 0),
    RGB(31, 51, 0),  RGB(31, 52, 0),  RGB(31, 52, 0),
    RGB(31, 52, 0),  RGB(31, 52, 0),  RGB(31, 52, 0),
    RGB(31, 53, 0),  RGB(31, 53, 0),  RGB(31, 53, 0),
    RGB(31, 53, 0),  RGB(31, 54, 0),  RGB(31, 54, 0),
    RGB(31, 54, 0),  RGB(31, 54, 0),  RGB(31, 54, 0),
    RGB(31, 55, 0),  RGB(31, 55, 0),  RGB(31, 55, 0),
    RGB(31, 55, 0),  RGB(31, 56, 0),  RGB(31, 56, 0),
    RGB(31, 56, 0),  RGB(31, 56, 0),  RGB(31, 57, 0),
    RGB(31, 57, 0),  RGB(31, 57, 0),  RGB(31, 57, 0),
    RGB(31, 57, 0),  RGB(31, 58, 0),  RGB(31, 58, 0),
    RGB(31, 58, 0),  RGB(31, 58, 0),  RGB(31, 59, 0),
    RGB(31, 59, 0),  RGB(31, 59, 0),  RGB(31, 59, 0),
    RGB(31, 60, 0),  RGB(31, 60, 0),  RGB(31, 60, 0),
    RGB(31, 60, 0),  RGB(31, 60, 0),  RGB(31, 61, 0),
    RGB(31, 61, 0),  RGB(31, 61, 0),  RGB(31, 61, 0),
    RGB(31, 62, 0),  RGB(31, 62, 0),  RGB(31, 62, 0),
    RGB(31, 62, 0),  RGB(31, 63, 0),  RGB(31, 63, 0),
    RGB(31, 63, 1),  RGB(31, 63, 1),  RGB(31, 63, 2),
    RGB(31, 63, 2),  RGB(31, 63, 3),  RGB(31, 63, 3),
    RGB(31, 63, 4),  RGB(31, 63, 4),  RGB(31, 63, 5),
    RGB(31, 63, 5),  RGB(31, 63, 5),  RGB(31, 63, 6),
    RGB(31, 63, 6),  RGB(31, 63, 7),  RGB(31, 63, 7),
    RGB(31, 63, 8),  RGB(31, 63, 8),  RGB(31, 63, 9),
    RGB(31, 63, 9),  RGB(31, 63, 10), RGB(31, 63, 10),
    RGB(31, 63, 10), RGB(31, 63, 11), RGB(31, 63, 11),
    RGB(31, 63, 12), RGB(31, 63, 12), RGB(31, 63, 13),
    RGB(31, 63, 13), RGB(31, 63, 14), RGB(31, 63, 14),
    RGB(31, 63, 15), RGB(31, 63, 15), RGB(31, 63, 15),
    RGB(31, 63, 16), RGB(31, 63, 16), RGB(31, 63, 17),
    RGB(31, 63, 17), RGB(31, 63, 18), RGB(31, 63, 18),
    RGB(31, 63, 19), RGB(31, 63, 19), RGB(31, 63, 20),
    RGB(31, 63, 20), RGB(31, 63, 21), RGB(31, 63, 21),
    RGB(31, 63, 21), RGB(31, 63, 22), RGB(31, 63, 22),
    RGB(31, 63, 23), RGB(31, 63, 23), RGB(31, 63, 24),
    RGB(31, 63, 24), RGB(31, 63, 25), RGB(31, 63, 25),
    RGB(31, 63, 26), RGB(31, 63, 26), RGB(31, 63, 26),
    RGB(31, 63, 27), RGB(31, 63, 27), RGB(31, 63, 28),
    RGB(31, 63, 28), RGB(31, 63, 29), RGB(31, 63, 29),
    RGB(31, 63, 30), RGB(31, 63, 30), RGB(31, 63, 31),
    RGB(31, 63, 31)};

// it's common in htxw_uix to write small controls in order to do demand draws when
// you need something a bit more involved that what the painter<> control allows for.
template <typename ControlSurfaceType>
class fire_box : public control<ControlSurfaceType> {
    using base_type = control<ControlSurfaceType>;
    int draw_state = 0;
    uint8_t p1[BUF_HEIGHT][BUF_WIDTH];  // VGA buffer, quarter resolution w/extra lines
    unsigned int i, j, k, l, delta;     // looping variables, counters, and data
    char ch;
   public:
    // not necessarly strictly required, but may be expected in larger 
    // apps further down the chain, so they are strongly recommended
    // when making generalized controls. Implemented here mainly
    // for example
    using control_surface_type = ControlSurfaceType;
    using pixel_type = typename base_type::pixel_type;
    using palette_type = typename base_type::palette_type;
    // this constructor not strictly necessary but recommended for generalized controls
    fire_box(uix::invalidation_tracker& parent, const palette_type* palette = nullptr)
        : base_type(parent, palette) {
    }
    // required
    fire_box()
        : base_type() {
    }
    // not strictly necessary but again recommended for generalized controls
    fire_box(fire_box&& rhs) {
        do_move_control(rhs);
        draw_state = 0;
    }
    // not strictly necessary but recommended for generalized controls
    fire_box& operator=(fire_box&& rhs) {
        do_move_control(rhs);
        draw_state = 0;
        return *this;
    }
    // not strictly necessary but recommended for generalized controls
    fire_box(const fire_box& rhs) {
        do_copy_control(rhs);
        draw_state = 0;
    }
    // not strictly necessary but recommended for generalized controls
    fire_box& operator=(const fire_box& rhs) {
        do_copy_control(rhs);
        draw_state = 0;
        return *this;
    }
protected:
    // we require this to compute the frame, as this gets called ONCE
    // per screen draw, whereas on_paint gets called potentially several
    // times
    virtual void on_before_paint() override {
        switch (draw_state) {
            case 0: // first draw
                // Initialize the buffer to 0s
                for (i = 0; i < BUF_HEIGHT; i++) {
                    for (j = 0; j < BUF_WIDTH; j++) {
                        p1[i][j] = 0;
                    }
                }
                draw_state = 1;
                // fall through
            case 1: // first draw and subsequent draws
                // Transform current buffer
                for (i = 1; i < BUF_HEIGHT; ++i) {
                    for (j = 0; j < BUF_WIDTH; ++j) {
                        if (j == 0)
                            p1[i - 1][j] = (p1[i][j] +
                                            p1[i - 1][BUF_WIDTH - 1] +
                                            p1[i][j + 1] +
                                            p1[i + 1][j]) >>
                                           2;
                        else if (j == 79)
                            p1[i - 1][j] = (p1[i][j] +
                                            p1[i][j - 1] +
                                            p1[i + 1][0] +
                                            p1[i + 1][j]) >>
                                           2;
                        else
                            p1[i - 1][j] = (p1[i][j] +
                                            p1[i][j - 1] +
                                            p1[i][j + 1] +
                                            p1[i + 1][j]) >>
                                           2;

                        if (p1[i][j] > 11)
                            p1[i][j] = p1[i][j] - 12;
                        else if (p1[i][j] > 3)
                            p1[i][j] = p1[i][j] - 4;
                        else {
                            if (p1[i][j] > 0) p1[i][j]--;
                            if (p1[i][j] > 0) p1[i][j]--;
                            if (p1[i][j] > 0) p1[i][j]--;
                        }
                    }
                }
                delta = 0;
                // make more 
                for (j = 0; j < BUF_WIDTH; j++) {
                    if (rand() % 10 < 5) {
                        delta = (rand() & 1) * 255;
                    }
                    p1[BUF_HEIGHT - 2][j] = delta;
                    p1[BUF_HEIGHT - 1][j] = delta;
                }
        }
    }
    // here we implement the demand draw. The destination is an htcw_gfx draw target. The clip
    // is the current rectangle within the destination that is actually being drawn, as this
    // routine may be called multiple times to render the entire destination. 
    // Note that the destination and clip use local coordinates, not screen coordinates.
    virtual void on_paint(control_surface_type& destination, const srect16& clip) override {
#ifdef USE_SPANS
        static_assert(gfx::helpers::is_same<rgb_pixel<16>,typename screen_t::pixel_type>::value,"USE_SPANS only works with RGB565");
        for (int y = clip.y1; y <= clip.y2; y+=2) {
            // must use rgb_pixel<16>
            // get the spans for the current partial rows (starting at clip.x1)
            // note that we're getting two, because we draw 2x2 squares
            // of all the same color.
            gfx_span row = destination.span(point16(clip.x1,y));
            gfx_span row2 = destination.span(point16(clip.x1,y+1));
            // get the pointers to the partial row data
            uint16_t *prow = (uint16_t*)row.data;
            uint16_t *prow2 = (uint16_t*)row2.data;
            for (int x = clip.x1; x <= clip.x2; x+=2) {
                int i = y >> 2;
                int j = x >> 2;
                PAL_TYPE px = fire_palette[this->p1[i][j]];
                // set the pixels
                *(prow++)=px;
                // if the clip x ends on an odd value, we need to not set the pointer
                // so check here
                if(x-clip.x1+1<(int)row.length) {
                    *(prow++)=px;
                }
                // the clip y ends on an odd value prow2 will be null
                if(prow2!=nullptr) {
                    *(prow2++)=px;
                    // another check for x if clip ends on an odd value
                    if(x-clip.x1+1<(int)row2.length) {
                        *(prow2++)=px;
                    }
                }                
            }
        }
#else 
        for (int y = clip.y1; y <= clip.y2; ++y) {
            for (int x = clip.x1; x <= clip.x2; ++x) {
                int i = y >> 2;
                int j = x >> 2;
                PAL_TYPE px = fire_palette[this->p1[i][j]];
                // set the pixel
                destination.point(point16(x,y),px);
            }
        }
#endif               
    }
    
};
// Pins
const byte CS_PIN = 10; // for CS1: 38
const byte DC_PIN = 8;
const byte RST_PIN = 9;
const byte DIN_PIN = 11; // for MOSI1: 26
const byte CLK_PIN = 13; // for SCK1: 27
const byte BKL_PIN = 7;
ili9341_t4 lcd(CS_PIN,DC_PIN,RST_PIN,7);
//ST7789_t3 lcd( CS_PIN,DC_PIN,RST_PIN);
static constexpr const size_t lcd_transfer_buffer_size = (SCREEN_WIDTH*(SCREEN_HEIGHT/4)*2);
static uint8_t lcd_transfer_buffer1[lcd_transfer_buffer_size];
static uint8_t lcd_transfer_buffer2[lcd_transfer_buffer_size];
static uix::display lcd_display;
static void uix_on_flush(const rect16& bounds, const void* bitmap, void* state){
    lcd.flush(bounds.x1,bounds.y1,bounds.x2,bounds.y2,bitmap);
}
static void lcd_on_flush_complete(void* state) {
  lcd_display.flush_complete();
}
// the control template instantiation aliases
using fire_t = fire_box<screen_t::control_surface_type>;;
using label_t = label<screen_t::control_surface_type>;

static const_buffer_stream fps_font_stream(vga_8x8,sizeof(vga_8x8));
// construct the font with the stream we just made
static win_font fps_font(fps_font_stream);
// the FPS label which displays statistics
static label_t fps_label;

static screen_t main_screen;

static fire_t fire;
void setup()
{
    
    Serial.begin(115200);
    Serial.println("Display Startup...!");
    SPI.begin();
    lcd.begin();
    lcd.rotation(0);
    lcd_display.buffer_size(lcd_transfer_buffer_size);
    lcd_display.buffer1(lcd_transfer_buffer1);
    lcd_display.buffer2(lcd_transfer_buffer2);
    lcd.on_flush_complete_callback(lcd_on_flush_complete,nullptr);
    lcd_display.on_flush_callback(uix_on_flush);
    main_screen.dimensions({SCREEN_WIDTH,SCREEN_HEIGHT});
    fire.bounds(main_screen.bounds());
    main_screen.register_control(fire);
    // initialize the font
    fps_font.initialize();
    // set the label color
    fps_label.color(uix_color_t::blue);
    // set the bounds for the label (near the bottom)
    fps_label.bounds({0,0,239,8});
    // want to align the text to the right
    fps_label.text_justify(uix_justify::top_right);
    // set the font to use (from above)
    fps_label.font(fps_font);
    // we don't want any padding around the text
    fps_label.padding({0,0});
    // for now set the text to nothing - to be
    // set later after we compute the FPS
    fps_label.text("");
    // register the label with the screem
    main_screen.register_control(fps_label);
    lcd_display.active_screen(main_screen);
    
}
static int frames = 0;
static uint32_t total_ms = 0;
static uint32_t ts_ms = 0;
static char fps_buf[64];
void loop() {
    uint32_t start_ms = millis();
    
    // update the display - causes the frame to be 
    // flushed to the display.
    lcd_display.update();
    uint32_t end_ms = millis();
    total_ms += (end_ms - start_ms);
    // when we have a flush pending, that means htcw_uix returned
    // immediately due to the DMA being already tied up with a previous
    // transaction. It will continue rendering once it is complete.
    // However, we don't want to count a pending as one of the frames
    // for calculating the FPS, since rendering was effectively skipped,
    // or rather, delayed - no paint actually happened, yet.
    if(!lcd_display.flush_pending()) {
        fire.invalidate();
        // compute the statistics
        ++frames;
    } 
    if (end_ms > ts_ms + 1000) {
        ts_ms = end_ms;
        // make sure we don't div by zero
        if (frames > 0) {
            sprintf(fps_buf, "FPS: %d, avg ms: %0.2f", frames,
                (float)total_ms / (float)frames);
        } else {
            sprintf(fps_buf, "FPS: < 1, total ms: %d", (int)total_ms);
        }
        // update the label (redraw is "automatic" (happens during lcd_display.update()))
        fps_label.text(fps_buf);
        Serial.println(fps_buf);
        total_ms = 0;
        frames = 0;
    }
}