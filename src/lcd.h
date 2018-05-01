#include <LCDMenuLib2.h>    // see https://github.com/Jomelo/LCDMenuLib2

// U8g2lib
#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
  #include <SPI.h>
#endif
  #ifdef U8X8_HAVE_HW_I2C
  #include <Wire.h>
#endif



// LCD pin out
#define PIN_LCD_RST     3
#define PIN_LCD_CS      4
#define PIN_LCD_DC      5
#define PIN_LCD_DIN     6
#define PIN_LCD_SCLK    7

// configuración de los botones de control
#define _LCDML_CONTROL_digital_low_active      0    // 0 = high active (pulldown) button, 1 = low active (pullup)
#define _LCDML_CONTROL_digital_enable_quit     0
#define _LCDML_CONTROL_digital_enable_lr       0
#define _LCDML_CONTROL_digital_enter           8
#define _LCDML_CONTROL_digital_up              9
#define _LCDML_CONTROL_digital_down            10
#define _LCDML_CONTROL_digital_quit            11
#define _LCDML_CONTROL_digital_left            12
#define _LCDML_CONTROL_digital_right           13


// settings for u8g lib and lcd
#define _LCDML_DISP_w                 84             // lcd width
#define _LCDML_DISP_h                 48             // lcd height
// font settings
#define _LCDML_DISP_font              u8g_font_6x13  // u8glib font (more fonts under u8g.h line 1520 ...)
#define _LCDML_DISP_font_w            6              // font width
#define _LCDML_DISP_font_h            13             // font heigt
// cursor settings
#define _LCDML_DISP_cursor_char       "X"            // cursor char
#define _LCDML_DISP_cur_space_before  2              // cursor space between
#define _LCDML_DISP_cur_space_behind  4              // cursor space between
// menu position and size
#define _LCDML_DISP_box_x0            0              // start point (x0, y0)
#define _LCDML_DISP_box_y0            0              // start point (x0, y0)
#define _LCDML_DISP_box_x1            84             // width x  (x0 + width)
#define _LCDML_DISP_box_y1            48             // hight y  (y0 + height)
#define _LCDML_DISP_draw_frame        1              // draw a box around the menu
 // scrollbar width
#define _LCDML_DISP_scrollbar_w       6  // scrollbar width (if this value is < 3, the scrollbar is disabled)
// nothing change here
#define _LCDML_DISP_cols_max          ((_LCDML_DISP_box_x1-_LCDML_DISP_box_x0)/_LCDML_DISP_font_w)
#define _LCDML_DISP_rows_max          ((_LCDML_DISP_box_y1-_LCDML_DISP_box_y0-((_LCDML_DISP_box_y1-_LCDML_DISP_box_y0)/_LCDML_DISP_font_h))/_LCDML_DISP_font_h)
// rows and cols
// when you use more rows or cols as allowed change in LCDMenuLib.h the define "_LCDML_DISP_cfg_max_rows" and "_LCDML_DISP_cfg_max_string_length"
// the program needs more ram with this changes
#define _LCDML_DISP_rows              _LCDML_DISP_rows_max  // max rows
#define _LCDML_DISP_cols              _LCDML_DISP_cols_max   // max cols



void lcdml_menu_display();
void lcdml_menu_clear();
void lcdml_menu_control();

void mFunc_information(uint8_t param);
void mFunc_timer_info(uint8_t param);
void mFunc_p2(uint8_t param);
void mDyn_para(uint8_t line);
