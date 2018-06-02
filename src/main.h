#include <Arduino.h>
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>
#include <RunningMedian.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include <LCDMenuLib2.h>    // see https://github.com/Jomelo/LCDMenuLib2

// U8g2lib
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
  #include <SPI.h>
#endif
  #ifdef U8X8_HAVE_HW_I2C
  #include <Wire.h>
#endif


//##############################################################################
//                              C O N S T A N T S
//##############################################################################
// firmware version
#define FIRM_VERSION  1

// PID default settings
#define DEFAULT_TEMP  42  //Recomendado entre 37 y 46ºC
#define DEFAULT_TIME  10  //Recomendado entre 5 y 10h
#define DEFAULT_KP    2
#define DEFAULT_KI    0.5
#define DEFAULT_KD    2

// update time (refresh, read ADC, ...)
#define TIME_REFRESH_LCD      200
#define TIME_REFRESH_SERIAL   1000
#define TIME_UPDATE_PID       50

// thermistor constants
#define THERMISTORNOMINAL   100000  // resistance at 25 degrees C
#define TEMPERATURENOMINAL  25      // temp. for nominal resistance (almost always 25 C)
#define BCOEFFICIENT        3950    // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR      98700   // the value of the 'other' resistor
#define VCC                 5

//pin out
#define PIN_LCD_RST     3
#define PIN_LCD_CS      4
#define PIN_LCD_DC      5
#define PIN_LCD_DIN     6
#define PIN_LCD_SCLK    7
#define BUTTON_RIGHT    _LCDML_CONTROL_digital_enter
#define BUTTON_UP       _LCDML_CONTROL_digital_up
#define BUTTON_DOWN     _LCDML_CONTROL_digital_down
#define PIN_PWM         12
#define PIN_THM         A0

#define _LCDML_CONTROL_digital_enter           8
#define _LCDML_CONTROL_digital_up              9
#define _LCDML_CONTROL_digital_down            10
#define _LCDML_CONTROL_digital_quit            11

//EEPROM
#define ADDR_FIRM_VERSION 1
#define ADDR_PID_DATA     5



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


// settings for lcd
#define _LCDML_DISP_cols  20
#define _LCDML_DISP_rows  4

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

// menu element count - last element id
// this value must be the same as the last menu element
#define _LCDML_DISP_cnt    8


//##############################################################################
//                            F U N C T I O N S
//##############################################################################
double adc2temp(int adc, float Vin, float sr);
double adc2temp(int adc, float Vin, float sr, boolean vccTherm);

void setupLCD();
void lcdml_menu_display();
void lcdml_menu_clear();
void lcdml_menu_control();
void mDyn_time(uint8_t line);
void mDyn_temp(uint8_t line);
void mFunc_start(uint8_t param);
void mFunc_setPID(uint8_t param);
void mDyn_Kp(uint8_t line);
void mDyn_Ki(uint8_t line);
void mDyn_Kd(uint8_t line);


//##############################################################################
//                          D A T A   T Y P E
//##############################################################################
struct PIDDATA{
  double pidSetPoint;
  double pidKp;
  double pidKi;
  double pidKd;
};

struct FERMDATA{
  int timeSet;
  unsigned long startTime;
  unsigned long endTime;
};


//##############################################################################
//                    G L O B A L   V A R I A B L E S
//##############################################################################
//Pid data
union pidData_t{
  PIDDATA datos;
  byte b[sizeof(PIDDATA)];
};
pidData_t pidData;

FERMDATA fermData;
double pidInput, pidOutput;

// Verbose variables
unsigned long serialTime, lcdTime, pidTime;

// control del menú
boolean showMenu;

//extern Adafruit_PCD8544                display;
PID           myPID (&pidInput, &pidOutput, &pidData.datos.pidSetPoint, DEFAULT_KP, DEFAULT_KI, DEFAULT_KD, DIRECT);//REVERSE;
PID_ATune     aTune (&pidInput, &pidOutput);
RunningMedian temp  (100);
//extern U8G2_PCD8544_84X48_1_4W_SW_SPI  u8g2;

LCDMenuLib2_menu                LCDML_0 (255, 0, 0, NULL, NULL); // root menu element (do not change)
LCDMenuLib2                     LCDML   (LCDML_0, _LCDML_DISP_rows, _LCDML_DISP_cols, lcdml_menu_display, lcdml_menu_clear, lcdml_menu_control);
U8G2_PCD8544_84X48_1_4W_SW_SPI  u8g2    (U8G2_R0, PIN_LCD_SCLK, PIN_LCD_DIN, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST); // The complete list is available here: https://github.com/olikraus/u8g2/wiki/u8g2setupcpp

unsigned long g_LCDML_DISP_press_time = 0;
