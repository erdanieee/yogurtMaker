/*********************************************************************
Programa para controlar la yogurtera
El programa, básicamente, mantiene la temperatura constante (aprox. 37ºC)
durante el tiempo establecido (aprox 10h) mediante un control PID. Para ello,
utiliza un termistor y una resistencia calefactora. El programa incluye la
opción de ajustar los parámetros del control de temperatura (PID), la
temperatura y el tiempo de fermentación.

Flujo del programa.
Setup:
  * inicializa serial, pantalla, pines, filtros, variables, ...
  * carga datos de la EEPROM
  * muestra menu para ajuste de valores de fermentación (tiempo/temperatura),
    ajuste de constantes PID (autotunning) o para comenzar la fermentación.

Loop:
  * fermentación:
    - lee la temperatura
    - ajusta salida PWM del calefactor para ajustar la temperatura actual
      a la establecida
    - cuando se alcanza el tiempo establecido se apaga.
  * autotunning
    - ajusta las constantes del PID mediante saltos de temperatura

  Cómo hacer yogurt:
    * (opcional) Calentar leche de vaca a 82ºC durante 30' y enfriar
      rápidamente a 45ºC
    * Poner el inóculo (2 cucharadas de yogurt o inóculo comprado) a la leche
      templada (~45ºC)
    * Seleccionar tiempo y temperatura, y pulsar comenzar
    * refrigerar los yogures cuando termine el tiempo de fermentación

*********************************************************************/




#include <Arduino.h>
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <EEPROMEx.h>


//##############################################################################
//                              C O N S T A N T S
//##############################################################################
// firmware version
#define VERSION     1.0

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
#define ADDR_FLOAT_VERSION    0
#define ADDR_DOUBLE_KP        5
#define ADDR_DOUBLE_KI        10
#define ADDR_DOUBLE_KD        15
#define ADDR_DOUBLE_SETPOINT  20
#define ADDR_INT_TIME         25

// settings for lcd
#define _LCDML_DISP_cols  20
#define _LCDML_DISP_rows  4




//##############################################################################
//                    G L O B A L   V A R I A B L E S
//##############################################################################
// PID variables
double  pidInput, pidOutput, pidSetPoint, pidKp, pidKi, pidKd;

// Fermentation variables
int timeSet;  //tempSet==pidSetPoint
unsigned long startTime, endTime;

// Verbose variables
unsigned long serialTime, lcdTime, pidTime;

Adafruit_PCD8544                display (PIN_LCD_SCLK, PIN_LCD_DIN, PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_RST);
PID                             myPID   (&pidInput, &pidOutput, &pidSetPoint, DEFAULT_KP, DEFAULT_KI, DEFAULT_KD, DIRECT);//REVERSE;
PID_ATune                       aTune   (&pidInput, &pidOutput);
RunningMedian                   temp    (100);
U8G2_PCD8544_84X48_1_4W_SW_SPI  u8g2    (U8G2_R0, PIN_LCD_SCLK, PIN_LCD_DIN, PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST) // The complete list is available here: https://github.com/olikraus/u8g2/wiki/u8g2setupcpp


//### Reset function
void(* resetFunc) (void) = 0; //declare reset function @ address 0


//##############################################################################
//                                   S E T U P
//##############################################################################
void setup() {
  Serial.begin(115200);
  Serial.println("init...");

  u8g2.begin();
  //display.begin();

  // set PIN modes
  //pinMode(BUTTON_UP,    INPUT_PULLUP);
  //pinMode(BUTTON_DOWN,  INPUT_PULLUP);
  //pinMode(BUTTON_LEFT,  INPUT_PULLUP);
  //pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(PIN_THM,      INPUT);
  pinMode(PIN_PWM,      OUTPUT);

  // Si la EEPROM tiene una versión compatible, se cargan los valores
  if (EEPROM.readFloat(ADDR_FLOAT_VERSION) >= VERSION){
    Serial.println("restoring EEPROM values");
    pidKp       = EEPROM.readDouble(ADDR_DOUBLE_KP);
    pidKi       = EEPROM.readDouble(ADDR_DOUBLE_KI);
    pidKd       = EEPROM.readDouble(ADDR_DOUBLE_KD);
    pidSetPoint = EEPROM.readDouble(ADDR_DOUBLE_SETPOINT);
    timeSet     = EEPROM.readInt(ADDR_INT_TIME)

  // si la versión no es compatible, se cargan los valores por defecto
  } else {
    Serial.println("setting default PID values");
    pidKp       = DEFAULT_KP;
    pidKi       = DEFAULT_KI;
    pidKd       = DEFAULT_KD;
    pidSetPoint = DEFAULT_TEMP;
    timeSet     = DEFAULT_TIME;
  }

  Serial.print("pidSetPoint: ");Serial.print(pidSetPoint);Serial.print(" Kp: ");Serial.print(pidKp);Serial.print(" pidKi: ");Serial.print(pidKi);Serial.print(" Kd: ");Serial.println(pidKd);

  myPID.SetTunings(pidKp,pidKi,pidKd);
  myPID.SetMode(AUTOMATIC);    //set mode automatic (on) manual (off)



//TODO: mostrar menú para empezar, ajustar valores o iniciar el autotunning
  /*while(menu){
    temp.add(analogRead(PIN_THM));    //lee la temperatura mientras se muestra menu
    LCDML.loop();
  }*/

  serialTime  = 0;
  lcdTime     = 0;
  pidTime     = 0;
  startTime   = millis();                       //TODO: mover a la función de inicio
  endTime     = startTime + (timeSet*3600*1000) //TODO: mover a la función de inicio
}





//##############################################################################
//                                   L O O P
//##############################################################################
void loop() {
  //lee el termistor
  temp.add(analogRead(PIN_THM));

  //calcula pidOutput a partir de pidInput y ajuta PWM en concordancia
  if(millis() > pidTime){
      pidInput = adc2temp(temp.getMedian(), VCC, SERIESRESISTOR);
      myPID.Compute();
      analogWrite(PIN_PWM, pidOutput);
      pidTime = millis() + TIME_UPDATE_PID;
  }

  //update LCD if needed
  if(millis() > lcdTime) {
    lcdTime = millis() + TIME_REFRESH_LCD;
  }

  // Serial print
  if(millis() > serialTime) {
    Serial.print("pidSetPoint: ");Serial.print(pidSetPoint); Serial.print(" ");
    Serial.print("pidInput: ");Serial.print(pidInput); Serial.print(" ");
    Serial.print("pidOutput: ");Serial.println(pidOutput);
    serialTime = millis() + TIME_REFRESH_SERIAL;
  }
}



void autoTune(){
  Serial.println("autotuning...");

  aTune.SetOutputStep(10);
  aTune.SetControlType(1);
  aTune.SetNoiseBand(0.8);
  aTune.SetLookbackSec(20);

  while ( aTune.Runtime() == 0 ){
    analogWrite(PIN_PWM, pidOutput);
  }

  pidKp = aTune.GetKp();
  pidKi = aTune.GetKi();
  pidKd = aTune.GetKd();
  EEPROM.writeDouble(ADDR_DOUBLE_KP, pidKp);
  EEPROM.writeDouble(ADDR_DOUBLE_KI, pidKi);
  EEPROM.writeDouble(ADDR_DOUBLE_KD, pidKd);
  EEPROM.writeFloat(ADDR_FLOAT_VERSION,VERSION);
  myPID.SetTunings(pidKp,pidKi,pidKd);

  Serial.println("Autotunning terminado")
  Serial.print(" Kp: "); Serial.print(pidKp); Serial.print(" pidKi: "); Serial.print(pidKi); Serial.print(" Kd: "); Serial.println(pidKd);

  resetFunc();  //reboot!!
}



// Función pensada para transformar valores leídos con entrada analógica
// a ºC.
//    - adc: valor obtenido con analogRead
//    - Vin: tensión de alimentación del divisor de tensión
//    - sr: resistencia serie del divisor de tensión
//    - vccTherm: thermistor a VCC (true) o a GND (false)
double adc2temp(int adc, float Vin, float sr){
  adc2temp(int adc, float Vin, float sr, true);
}
double adc2temp(int adc, float Vin, float sr, boolean vccTherm){
  float steinhart, vadc, radc, rinf;

  //convert ADC value to resistance
  vadc = Vin * adc/1023.0;
  if (vccTherm){
      radc = sr*vadc/(Vin-vadc);  //sr*(Vin-vadc)/vadc
  } else {
      radc = sr*(Vin-vadc)/vadc
  }

  rinf = THERMISTORNOMINAL*exp(-BCOEFFICIENT/TEMPERATURENOMINAL);               //R0*e^(-B/T0)
  steinhart = BCOEFFICIENT/log(radc/rinf);                                      //T = B/log(Rout/Rinf)
  steinhart -= 273.15;                                                          // convert to C

  return steinhart;
}
