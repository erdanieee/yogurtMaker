#include <Arduino.h>
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>
#include <RunningMedian.h>
#include <EEPROM.h>
#include <MenuSystem.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>


//##############################################################################
//                              C O N S T A N T S
//##############################################################################
// firmware version
#define FIRM_VERSION  2

// PID default settings
#define DEFAULT_TEMP  42  //Recomendado entre 37 y 46ºC
#define DEFAULT_TIME  10  //Recomendado entre 5 y 10h
#define DEFAULT_KP    2
#define DEFAULT_KI    0.5
#define DEFAULT_KD    2

// update time (refresh, read ADC, ...)
#define TIME_REFRESH_LCD      500
#define TIME_REFRESH_SERIAL   5000
#define TIME_UPDATE_PID       1000
#define TIME_UPDATE_TEMP      100

// thermistor constants
#define THERMISTORNOMINAL   100000.0  // resistance at 25 degrees C
#define TEMPERATURENOMINAL  25.0      // temp. for nominal resistance (almost always 25 C)
#define BCOEFFICIENT        3950.0    // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR      98700.0   // the value of the 'other' resistor
#define VCC                 5.0

//pin out
#define PIN_LCD_RST     5
#define PIN_LCD_CS      6
#define PIN_LCD_DC      7
#define PIN_LCD_DIN     8
#define PIN_LCD_SCLK    9
#define BUTTON_ENTER    10
#define BUTTON_DOWN     11
#define PIN_PWM         4
#define PIN_THM         A0

//EEPROM
#define ADDR_FIRM_VERSION 1
#define ADDR_PID_DATA     5




//##############################################################################
//                            F U N C T I O N S
//##############################################################################
double adc2temp(int adc, float Vin, float sr);
double adc2temp(int adc, float Vin, float sr, boolean vccTherm);
void on_start_selected  (MenuComponent* p_menu_component);
void on_autoPID_selected(MenuItem* p_menu_component);
void on_setTime_selected(NumericMenuItem* p_menu_component);
void on_setTemp_selected(NumericMenuItem* p_menu_component);



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
unsigned long serialTime, lcdTime, pidTime, tempTime;

// control del menú
boolean fermentation, saveToEeprom, autoTunning; //TODO: meter fermetation y autoTunning en un struct para hacer estados

PID               myPID (&pidInput, &pidOutput, &pidData.datos.pidSetPoint, DEFAULT_KP, DEFAULT_KI, DEFAULT_KD, DIRECT);//REVERSE;
PID_ATune         aTune (&pidInput, &pidOutput);
RunningMedian     temp  (100);
Adafruit_PCD8544  lcd   (PIN_LCD_SCLK, PIN_LCD_DIN, PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_RST);
//Adafruit_PCD8544 lcd = Adafruit_PCD8544(9, 8, 7, 6, 5);
