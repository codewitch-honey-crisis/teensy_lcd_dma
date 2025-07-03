// BUG: When async flushes are on there will be a crash after a bit when the FPS label overlaps with the clock
// tested with GFX instead of the label and no crash
// suspect the bug is in uix::screen_ex.update_impl()
//#define USE_DTCM

#include <Arduino.h>

#include "gfx.h"
#include "ssd1351_t4.hpp"
//#include "st7789_t4.hpp"
//#include "ili9341_t4.hpp"
#include "uix.h"
#define VGA_8X8_IMPLEMENTATION
#include "assets/vga_8x8.h"

using namespace gfx;
using namespace uix;

// Screen dimension
const uint16_t SCREEN_WIDTH = 128;
const uint16_t SCREEN_HEIGHT = 128;
static bool use_async_flush = true;

using px_t = pixel<channel_traits<channel_name::R, 5>, channel_traits<channel_name::G, 6>, channel_traits<channel_name::B, 5>>;

using screen_t = uix::screen<px_t>;
using color_t = color<screen_t::pixel_type>;
using uix_color_t = color<rgba_pixel<32>>;

static const_buffer_stream fps_font_stream(vga_8x8, sizeof(vga_8x8));
// construct the font with the stream we just made
static win_font fps_font(fps_font_stream);

static char fps_buf[128];

// Pins
const byte CS_PIN = 10;  // for CS1: 38
const byte DC_PIN = 8;
const byte RST_PIN = 9;
const byte DIN_PIN = 11;  // for MOSI1: 26
const byte CLK_PIN = 13;  // for SCK1: 27
const byte BKL_PIN = 7;
ssd1351_t4 lcd(CS_PIN,DC_PIN,RST_PIN);
//st7789_t4 lcd(st7789_t4_res_t::ST7789_240x320, CS_PIN, DC_PIN, RST_PIN, 7);
//ili9341_t4 lcd(CS_PIN,DC_PIN,RST_PIN,BKL_PIN);
static constexpr const size_t lcd_transfer_buffer_size = math::min_((SCREEN_WIDTH * (SCREEN_HEIGHT/10) * 2), 32 * 1024);
#ifdef USE_DTCM
static uint8_t lcd_transfer_buffer1[lcd_transfer_buffer_size];
static uint8_t lcd_transfer_buffer2[lcd_transfer_buffer_size];
#else
static uint8_t* lcd_transfer_buffer1 = nullptr;
static uint8_t* lcd_transfer_buffer2 = nullptr;
#endif
static uix::display lcd_display;

static void uix_on_flush(const rect16& bounds, const void* bitmap_data, void* state) {
#ifdef USE_DTCM
    static const constexpr bool flush_cache = false;
#else
    static const constexpr bool flush_cache = true;
#endif
    if(use_async_flush) {
        lcd.flush_async(bounds.x1, bounds.y1, bounds.x2, bounds.y2, bitmap_data, flush_cache);
    } else {
        lcd.flush(bounds.x1, bounds.y1, bounds.x2, bounds.y2, bitmap_data);
    }
}
#ifdef FASTRUN
    FASTRUN
#endif
static void lcd_on_flush_complete(void* state) {
    lcd_display.flush_complete();
}

template <typename ControlSurfaceType>
class vclock : public uix::canvas_control<ControlSurfaceType> {
    using base_type = uix::canvas_control<ControlSurfaceType>;

   public:
    using type = vclock;
    using control_surface_type = ControlSurfaceType;

   private:
    constexpr static const uint16_t default_face_border_width = 2;
    constexpr static const gfx::vector_pixel default_face_border_color = gfx::color<gfx::vector_pixel>::black;
    constexpr static const gfx::vector_pixel default_face_color = gfx::color<gfx::vector_pixel>::white;
    constexpr static const gfx::vector_pixel default_tick_border_color = gfx::color<gfx::vector_pixel>::gray;
    constexpr static const gfx::vector_pixel default_tick_color = gfx::color<gfx::vector_pixel>::gray;
    constexpr static const uint16_t default_tick_border_width = 2;
    constexpr static const gfx::vector_pixel default_minute_color = gfx::color<gfx::vector_pixel>::black;
    constexpr static const gfx::vector_pixel default_minute_border_color = gfx::color<gfx::vector_pixel>::gray;
    constexpr static const uint16_t default_minute_border_width = 2;
    constexpr static const gfx::vector_pixel default_hour_color = gfx::color<gfx::vector_pixel>::black;
    constexpr static const gfx::vector_pixel default_hour_border_color = gfx::color<gfx::vector_pixel>::gray;
    constexpr static const uint16_t default_hour_border_width = 2;
    constexpr static const gfx::vector_pixel default_second_color = gfx::color<gfx::vector_pixel>::red;
    constexpr static const gfx::vector_pixel default_second_border_color = gfx::color<gfx::vector_pixel>::red;
    constexpr static const uint16_t default_second_border_width = 2;
    using fb_t = gfx::bitmap<typename control_surface_type::pixel_type, typename control_surface_type::palette_type>;
    uint16_t m_face_border_width;
    gfx::vector_pixel m_face_border_color;
    gfx::vector_pixel m_face_color;
    gfx::vector_pixel m_tick_border_color;
    gfx::vector_pixel m_tick_color;
    uint16_t m_tick_border_width;
    gfx::vector_pixel m_minute_color;
    gfx::vector_pixel m_minute_border_color;
    uint16_t m_minute_border_width;
    gfx::vector_pixel m_hour_color;
    gfx::vector_pixel m_hour_border_color;
    uint16_t m_hour_border_width;
    gfx::vector_pixel m_second_color;
    gfx::vector_pixel m_second_border_color;
    uint16_t m_second_border_width;
    time_t m_time;
    bool m_face_dirty;
    bool m_buffer_face;
    fb_t m_face_buffer;
    // compute thetas for a rotation
    static void update_transform(float rotation, float& ctheta, float& stheta) {
        float rads = gfx::math::deg2rad(rotation);
        ctheta = cosf(rads);
        stheta = sinf(rads);
    }
    // transform a point given some thetas, a center and an offset
    static gfx::pointf transform_point(float ctheta, float stheta, gfx::pointf center, gfx::pointf offset, float x, float y) {
        float rx = (ctheta * (x - (float)center.x) - stheta * (y - (float)center.y) + (float)center.x) + offset.x;
        float ry = (stheta * (x - (float)center.x) + ctheta * (y - (float)center.y) + (float)center.y) + offset.y;
        return {(float)rx, (float)ry};
    }
    gfx::gfx_result draw_clock_face(gfx::canvas& clock_canvas) {
        constexpr static const float rot_step = 360.0f / 12.0f;
        gfx::pointf offset(0, 0);
        gfx::pointf center(0, 0);

        float rotation(0);
        float ctheta, stheta;
        gfx::ssize16 size = (gfx::ssize16)clock_canvas.dimensions();
        gfx::rectf b = gfx::sizef(size.width, size.height).bounds();
        b.inflate_inplace(-m_face_border_width - 1, -m_face_border_width - 1);
        float w = b.width();
        float h = b.height();
        if (w > h) w = h;
        gfx::rectf sr(0, w / 30, w / 30, w / 5);
        sr.center_horizontal_inplace(b);
        center = gfx::pointf(w * 0.5f + m_face_border_width + 1, w * 0.5f + m_face_border_width + 1);
        clock_canvas.fill_color(m_face_color);
        clock_canvas.stroke_color(m_face_border_color);
        clock_canvas.stroke_width(m_face_border_width);
        clock_canvas.circle(center, center.x - 1);
        gfx::gfx_result res = clock_canvas.render();
        if (res != gfx::gfx_result::success) {
            return res;
        }
        bool toggle = false;
        clock_canvas.stroke_color(m_tick_border_color);
        clock_canvas.fill_color(m_tick_color);
        clock_canvas.stroke_width(m_tick_border_width);

        for (float rot = 0; rot < 360.0f; rot += rot_step) {
            rotation = rot;
            update_transform(rotation, ctheta, stheta);
            toggle = !toggle;
            if (toggle) {
                clock_canvas.move_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y1));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y1));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y2));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y2));
                clock_canvas.close_path();
            } else {
                clock_canvas.move_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y1));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y1));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y2 - sr.height() * 0.5f));
                clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y2 - sr.height() * 0.5f));
                clock_canvas.close_path();
            }
            res = clock_canvas.render();
            if (res != gfx::gfx_result::success) {
                return res;
            }
        }
        return gfx::gfx_result::success;
    }
    void draw_clock_time(gfx::canvas& clock_canvas) {
        gfx::pointf offset(0, 0);
        gfx::pointf center(0, 0);
        float rotation(0);
        float ctheta, stheta;
        time_t time = m_time;
        gfx::ssize16 size = (gfx::ssize16)clock_canvas.dimensions();
        gfx::rectf b = gfx::sizef(size.width, size.height).bounds();
        b.inflate_inplace(-m_face_border_width - 1, -m_face_border_width - 1);
        float w = b.width();
        float h = b.height();
        if (w > h) w = h;
        gfx::rectf sr(0, w / 30, w / 30, w / 5);
        sr.center_horizontal_inplace(b);
        center = gfx::pointf(w * 0.5f + m_face_border_width + 1, w * 0.5f + m_face_border_width + 1);
        sr = gfx::rectf(0, w / 40, w / 16, w / 2);
        sr.center_horizontal_inplace(b);
        // create a path for the minute hand:
        rotation = (fmodf(time / 60.0f, 60) / 60.0f) * 360.0f;
        update_transform(rotation, ctheta, stheta);
        clock_canvas.move_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y1));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y2));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y2 + (w / 20)));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y2));
        clock_canvas.close_path();
        clock_canvas.fill_color(m_minute_color);
        clock_canvas.stroke_color(m_minute_border_color);
        clock_canvas.stroke_width(m_minute_border_width);
        clock_canvas.render();  // render the path
        // create a path for the hour hand
        sr.y1 += w / 8;
        rotation = (fmodf(time / (3600.0f), 12.0f) / (12.0f)) * 360.0f;
        update_transform(rotation, ctheta, stheta);
        clock_canvas.move_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y1));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y2));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y2 + (w / 20)));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y2));
        clock_canvas.close_path();
        clock_canvas.fill_color(m_hour_color);
        clock_canvas.stroke_color(m_hour_border_color);
        clock_canvas.stroke_width(m_hour_border_width);
        clock_canvas.render();  // render the path
        // create a path for the second hand
        sr.y1 -= w / 8;
        rotation = ((time % 60) / 60.0f) * 360.0f;
        update_transform(rotation, ctheta, stheta);
        clock_canvas.move_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y1));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x2, sr.y2));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y2 + (w / 20)));
        clock_canvas.line_to(transform_point(ctheta, stheta, center, offset, sr.x1, sr.y2));
        clock_canvas.close_path();
        clock_canvas.fill_color(m_second_color);
        clock_canvas.stroke_color(m_second_border_color);
        clock_canvas.stroke_width(m_second_border_width);
        clock_canvas.render();
    }

   public:
    vclock() : base_type(),
               m_face_border_width(default_face_border_width),
               m_face_border_color(default_face_border_color),
               m_face_color(default_face_color),
               m_tick_color(default_tick_color),
               m_tick_border_width(default_tick_border_width),
               m_minute_color(default_minute_color),
               m_minute_border_color(default_minute_border_color),
               m_minute_border_width(default_minute_border_width),
               m_hour_color(default_hour_color),
               m_hour_border_color(default_hour_border_color),
               m_hour_border_width(default_hour_border_width),
               m_second_color(default_second_color),
               m_second_border_color(default_second_border_color),
               m_second_border_width(default_second_border_width),
               m_time(0),
               m_face_dirty(true),
               m_buffer_face(true) {
    }
    vclock(const vclock& rhs) {
        *this = rhs;
    }
    vclock(vclock&& rhs) {
        *this = rhs;
    }
    vclock& operator=(const vclock& rhs) {
        *this = rhs;
    }
    vclock& operator=(vclock&& rhs) {
        *this = rhs;
    }
    virtual ~vclock() {
        if (m_face_buffer.begin()) {
            free(m_face_buffer.begin());
        }
    }

   protected:
    virtual void on_paint(control_surface_type& destination, const uix::srect16& clip) override {
        if (m_buffer_face && m_face_dirty) {
            int16_t w = this->dimensions().width;
            int16_t h = this->dimensions().height;
            if (w > h) w = h;
            fb_t bmp = gfx::create_bitmap<typename fb_t::pixel_type, typename fb_t::palette_type>(gfx::size16(w, w));
            if (bmp.begin()) {
                bmp.clear(bmp.bounds());
                gfx::canvas cvs(gfx::size16(w, w));
                if (gfx::gfx_result::success == cvs.initialize()) {
                    gfx::draw::canvas(bmp, cvs, gfx::point16::zero());
                    if (gfx::gfx_result::success == draw_clock_face(cvs)) {
                        if (m_face_buffer.begin() != nullptr) {
                            free(m_face_buffer.begin());
                        }
                        m_face_buffer = bmp;
                        m_face_dirty = false;
                    }
                }
            }
        }
        if (m_buffer_face && !m_face_dirty) {
            // we have a valid bitmap, no need to draw the face here.
            gfx::draw::bitmap(destination, m_face_buffer.bounds(), m_face_buffer, m_face_buffer.bounds());
        }
        base_type::on_paint(destination, clip);
    }
    virtual void on_paint(gfx::canvas& destination, const uix::srect16& clip) override {
        if (!m_buffer_face || m_face_dirty) {
            draw_clock_face(destination);
        }
        draw_clock_time(destination);
    }

   public:
    void refresh() {
        m_face_dirty = true;
        this->invalidate();
    }
    uint16_t face_border_width() const {
        return m_face_border_width;
    }
    void face_border_width(uint16_t value) {
        m_face_border_width = value;
        m_face_dirty = true;
        this->invalidate();
    }
    gfx::rgba_pixel<32> face_border_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_face_border_color, &result);
        return result;
    }
    void face_border_color(gfx::rgba_pixel<32> value) {
        convert(value, &m_face_border_color);
        m_face_dirty = true;
        this->invalidate();
    }
    gfx::rgba_pixel<32> face_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_face_color, &result);
        return result;
    }
    void face_color(gfx::rgba_pixel<32> value) {
        convert(value, &m_face_color);
        m_face_dirty = true;
        this->invalidate();
    }
    gfx::rgba_pixel<32> tick_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_tick_color, &result);
        return result;
    }
    void tick_color(gfx::rgba_pixel<32> value) {
        convert(value, &m_tick_color);
        m_face_dirty = true;
        this->invalidate();
    }
    uint16_t tick_border_width() const {
        return m_tick_border_width;
    }
    void tick_border_width(uint16_t value) {
        m_tick_border_width = value;
        m_face_dirty = true;
        this->invalidate();
    }
    gfx::rgba_pixel<32> minute_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_minute_color, &result);
        return result;
    }
    void minute_color(gfx::rgba_pixel<32> value) {
        convert(value, &m_minute_color);
        this->invalidate();
    }
    gfx::rgba_pixel<32> minute_border_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_minute_border_color, &result);
        return result;
    }
    void minute_border_color(gfx::rgba_pixel<32> value) {
        convert(value, &m_minute_border_color);
        this->invalidate();
    }
    uint16_t minute_border_width() const {
        return m_minute_border_width;
    }
    void minute_border_width(uint16_t value) {
        m_minute_border_width = value;
        this->invalidate();
    }
    gfx::rgba_pixel<32> hour_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_hour_color, &result);
        return result;
    }
    void hour_color(gfx::rgba_pixel<32> value) {
        convert(value, &m_hour_color);
        this->invalidate();
    }
    gfx::rgba_pixel<32> hour_border_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_hour_border_color, &result);
        return result;
    }
    void hour_border_color(gfx::rgba_pixel<32> value) {
        convert(value, &m_hour_border_color);
        this->invalidate();
    }
    uint16_t hour_border_width() const {
        return m_hour_border_width;
    }
    void hour_border_width(uint16_t value) {
        m_hour_border_width = value;
        this->invalidate();
    }
    gfx::rgba_pixel<32> second_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_second_color, &result);
        return result;
    }
    void second_color(gfx::rgba_pixel<32> value) {
        convert(value, &m_second_color);
        this->invalidate();
    }
    gfx::rgba_pixel<32> second_border_color() const {
        gfx::rgba_pixel<32> result;
        convert(m_second_border_color, &result);
        return result;
    }
    void second_border_color(gfx::rgba_pixel<32> value) {
        convert(value, &m_second_border_color);
        this->invalidate();
    }
    uint16_t second_border_width() const {
        return m_second_border_width;
    }
    void second_border_width(uint16_t value) {
        m_second_border_width = value;
        this->invalidate();
    }
    time_t time() const {
        return m_time;
    }
    void time(time_t value) {
        m_time = value;
        this->invalidate();
    }
    bool buffer_face() const {
        return m_buffer_face;
    }
    void buffer_face(bool value) {
        if(value!=m_buffer_face) {
            if (value == false) {
                if (m_buffer_face) {
                    if (m_face_buffer.begin()) {
                        free(m_face_buffer.begin());
                    }
                }
            } else {
                m_face_dirty = true;
            }
            m_buffer_face = value;
        }
    }
};

// the control template instantiation aliases
using ana_clock_t = vclock<screen_t::control_surface_type>;
;
using label_t = label<screen_t::control_surface_type>;

// the FPS label which displays statistics
static label_t fps_label;

static screen_t main_screen;

static ana_clock_t ana_clock;
uint8_t* alloc32(size_t size) {
    uint8_t* mem = (uint8_t*)malloc(size+31);
    if(!mem) return nullptr;
    return (uint8_t*)(((unsigned long)(uintptr_t)mem+31) & ~((unsigned long) (uintptr_t)31));
}
void setup() {
    // SCB_DisableCache();
    Serial.begin(115200);
    //delay(5000);
    Serial.println("Demo Startup...!");
#ifndef USE_DTCM
    lcd_transfer_buffer1 = (uint8_t*)malloc(lcd_transfer_buffer_size);
    if (lcd_transfer_buffer1 == nullptr) {
        Serial.println("Out of memory allocating transfer buffer. Choose a smaller size");
        while (1);
    }
    lcd_transfer_buffer2 = (uint8_t*)malloc(lcd_transfer_buffer_size);
    if (lcd_transfer_buffer2 == nullptr) {
        Serial.println("Out of memory allocating transfer buffer. Choose a smaller size");
        while (1);
    }
#endif
    SPI.begin();
    lcd.begin();
    lcd.rotation(0);
    lcd_display.buffer_size(lcd_transfer_buffer_size);
    lcd_display.buffer1(lcd_transfer_buffer1);
    lcd_display.buffer2(lcd_transfer_buffer2);
    lcd.on_flush_complete_callback(lcd_on_flush_complete, nullptr);
    lcd_display.on_flush_callback(uix_on_flush);
    main_screen.dimensions({SCREEN_WIDTH, SCREEN_HEIGHT});
    // initialize the font
    fps_font.initialize();
    
    uint16_t extent = gfx::math::min_(main_screen.dimensions().width, main_screen.dimensions().height);
    if(main_screen.dimensions().aspect_ratio()==1.f) {
        extent -= 16;
    }
    ana_clock.bounds(srect16(spoint16::zero(), ssize16(extent, extent)).center_horizontal(main_screen.bounds()).offset(0,17));
    //ana_clock.buffer_face(false);
    main_screen.register_control(ana_clock);
    // set the label color
    fps_label.color(uix_color_t::blue);
    // set the bounds for the label (near the bottom)
    fps_label.bounds(srect16(0, 0, SCREEN_WIDTH - 1, 16).offset(0,0/*ana_clock.bounds().y2+1)*/));
    // want to align the text to the right
    fps_label.text_justify(uix_justify::top_right);
    // set the font to use (from above)
    fps_label.font(fps_font);
    // we don't want any padding around the text
    fps_label.padding({0, 0});
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
static int seconds = 0;
static uint32_t sync_async_ts = 0;
void loop() {
    uint32_t start_ms = millis();

    // update the display - causes part of the frame to be
    // flushed to the display.
    lcd_display.update(false);
    uint32_t end_ms = millis();
    total_ms += (end_ms - start_ms);
    // when we have a flush pending, that means htcw_uix returned
    // immediately due to the DMA being already tied up with a previous
    // transaction. It will continue rendering once it is complete.
    // However, we don't want to count a pending as one of the frames
    // for calculating the FPS, since rendering was effectively skipped,
    // or rather, delayed - no paint actually happened, yet.
    if (!lcd_display.flush_pending() && !lcd_display.dirty()) {
        // if we're not flushing see if we should switch between sync/async 
        // render modes:
        if(!lcd_display.flushing()) {
            if(millis() > sync_async_ts+1000) {
                sync_async_ts = millis();
                if(0==(seconds%5)) { // every 5 seconds
                    use_async_flush = !use_async_flush;
                    if(!use_async_flush) {
                        fps_label.color(uix_color_t::red);
                    } else {
                        fps_label.color(uix_color_t::blue);
                    }
                }
            }
        }
        ana_clock.time(ana_clock.time() + 1);
        // compute the statistics
        ++frames;

    }
    if (end_ms > ts_ms + 1000) {
        ++seconds;
        ts_ms = end_ms;
        // make sure we don't div by zero
        if (frames > 0) {
            //sprintf(fps_buf,"FPS: %d", frames);
            sprintf(fps_buf, "%s FPS: %d\navg ms: %0.2f", (use_async_flush) ? "async" : "sync", frames,
                    (float)total_ms / (float)frames);
        } else {
            //strcpy(fps_buf,"FPS: <1");
            sprintf(fps_buf, "%s FPS: < 1\ntotal ms: %d", (use_async_flush) ? "async" : "sync", (int)total_ms);
        }
        // update the label (redraw is "automatic" (happens during lcd_display.update()))
        fps_label.text(fps_buf);
        Serial.println(fps_buf);
        total_ms = 0;
        frames = 0;
    }
}