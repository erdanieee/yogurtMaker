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

*********************************************************************/




#include <Arduino.h>
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <EEPROMEx.h>

#define DEFAULT_TEMP  37  //ºC
#define DEFAULT_TIME  10  //horas
#define DEFAULT_KP    2
#define DEFAULT_KI    0.5
#define DEFAULT_KD    2



#define VERSION               1.0
#define TIME_BUTTON_SHORT     500
#define TIME_BUTTON_LONG      1000
#define TIME_BUTTON_VERYLONG  8000
#define TIME_REFRESH_LCD      200
#define TIME_REFRESH_SERIAL   1000

#define THERMISTORNOMINAL     100000  // resistance at 25 degrees C
#define TEMPERATURENOMINAL    25      // temp. for nominal resistance (almost always 25 C)
#define BCOEFFICIENT          3950    // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR        98700   // the value of the 'other' resistor

//pin out
#define PIN_LCD_RST     3
#define PIN_LCD_CS      4
#define PIN_LCD_DC      5
#define PIN_LCD_DIN     6
#define PIN_LCD_SCLK    7
#define BUTTON_NEXT     8
#define BUTTON_OK       9
#define PIN_THM         A0
#define PIN_PWM         11

//EEPROM
#define ADDR_FLOAT_VERSION    0
#define ADDR_DOUBLE_KP        5
#define ADDR_DOUBLE_KI        10
#define ADDR_DOUBLE_KD        15
#define ADDR_DOUBLE_SETPOINT  20
#define ADDR_DOUBLE_TIME      25
#define ADDR_FLOAT_TEMP       30


double  input,
        output,
        setpoint,
        kp,
        ki,
        kd;
boolean tuning = false;

unsigned long serialTime,
              lcdTime,
              buttonUpTime,
              buttonDownTime,
              startTime;


Adafruit_PCD8544  display (PIN_LCD_SCLK, PIN_LCD_DIN, PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_RST);
PID               myPID   (&input, &output, &setpoint, DEFAULT_KP, DEFAULT_KI, DEFAULT_KD, DIRECT);//REVERSE;
PID_ATune         aTune   (&input, &output);
RunningMedian     temp    (100);




void setup() {
  Serial.begin(9600);
  Serial.println("init...");

  display.begin();

  // set PIN modes
  pinMode(BUTTON_NEXT,  INPUT_PULLUP);
  pinMode(BUTTON_OK,    INPUT_PULLUP);

  // Si la EEPROM tiene una versión compatible, se cargan los valores PID
  if (EEPROM.readFloat(ADDR_FLOAT_VERSION) >= VERSION){
    Serial.println("restoring EEPROM values");
    setpoint = EEPROM.readDouble(ADDR_DOUBLE_SETPOINT);
    kp       = EEPROM.readDouble(ADDR_DOUBLE_KP);
    ki       = EEPROM.readDouble(ADDR_DOUBLE_KI);
    kd       = EEPROM.readDouble(ADDR_DOUBLE_KD);

  // si la versión no es compatible, se cargan los valores por defecto
  } else {
    Serial.println("setting default PID values");
    setpoint = DEFAULT_TEMP;
    kp       = DEFAULT_KP;
    ki       = DEFAULT_KI;
    kd       = DEFAULT_KD;
  }


//TODO: si están los 2 botones pulsados al iniciarse pasa al modo autotunning
if ( digitalRead(BUTTON_NEXT)==LOW && digitalRead(BUTTON_OK)==LOW){
  Serial.println("autotuning...");

  aTune.SetOutputStep(10);
  aTune.SetControlType(1);  //TODO: CHECK!
  aTune.SetNoiseBand(0.8);
  aTune.SetLookbackSec(20);

  tuning = true;
}

  Serial.print("setpoint: ");Serial.print(setpoint);Serial.print(" Kp: ");Serial.print(kp);Serial.print(" ki: ");Serial.print(ki);Serial.print(" Kd: ");Serial.println(kd);

  myPID.SetTunings(kp,ki,kd);
  myPID.SetMode(AUTOMATIC);    //set mode automatic (on) manual (off)

  serialTime      = 0;
  lcdTime         = 0;
  startTime       = 0;
  buttonUpTime    = 0;
  buttonDownTime  = 0;
}






void loop() {
  //now   = millis();                  //overflows in ~ 50 days!!
  temp.add(analogRead(PIN_THM));
  //tempOut = adc2temp(outSensor.getMedian(), SERIAL_RESISTOR_HOT);

  if(tuning){
    if ( aTune.Runtime() != 0 ){
      kp = aTune.GetKp();
      ki = aTune.GetKi();
      kd = aTune.GetKd();
      EEPROM.writeDouble(ADDR_DOUBLE_KP, kp);
      EEPROM.writeDouble(ADDR_DOUBLE_KI, ki);
      EEPROM.writeDouble(ADDR_DOUBLE_KD, kd);
      EEPROM.writeFloat(ADDR_FLOAT_VERSION,VERSION);
      myPID.SetTunings(kp,ki,kd);

      Serial.println("Autotunning terminado")
      Serial.print(" Kp: "); Serial.print(kp); Serial.print(" ki: "); Serial.print(ki); Serial.print(" Kd: "); Serial.println(kd);

      tuning = false;
    }

  } else {
    myPID.Compute();                   //calculate output
  }

  analogWrite(PIN_PWM,output);       //heater control (default PID output range is between 0-255)

  switch(checkButtons()){
    case UP:
      setpoint++;
      EEPROM.writeDouble(ADDR_DOUBLE_SETPOINT, setpoint);
      break;

    case DOWN:
      setpoint--;
      EEPROM.writeDouble(ADDR_DOUBLE_SETPOINT, setpoint);
      break;
  }


  //update LCD if needed
  if(millis() > lcdTime) {
    if(!tuning){
      /*//lcd.clear();
      lcd.home();
      lcd.print("Temp: "); lcd.print ( input ); lcd.print ( " (" ); lcd.print((int)setpoint); lcd.print ( ") " );
      lcd.setCursor ( 0, 1 );
      lcd.print("Vel: "); lcd.print ( map(output,0,255,0,100) ); lcd.print ( "%    " );*/

    } else {
      lcd.setCursor ( 0, 1 );
      lcd.print("S"); lcd.print((int)setpoint); lcd.print(" T"); lcd.print ( (int)input ); lcd.print(" V"); lcd.print ( output );//map(output,0,255,0,100) );
    }

    //serialTime+=500;
    lcdTime = millis() + TIME_REFRESH_LCD;
  }

  //send with processing if it's time
  if(millis() > serialTime) {
    Serial.print("setpoint: ");Serial.print(setpoint); Serial.print(" ");
    Serial.print("input: ");Serial.print(input); Serial.print(" ");
    Serial.print("output: ");Serial.println(output);
    serialTime = millis() + TIME_REFRESH_SERIAL;
  }
}






byte checkButtons(){
  if (digitalRead(BUTTON_UP) == LOW) {
    Serial.println(buttonUpTime);
    if (buttonUpTime == 0) {
      Serial.println("Button up");
      delay(15);
      buttonUpTime = millis() + TIME_BUTTON_LONG;
      return UP;

    } else if (millis() > buttonUpTime){
      Serial.println("Button up");
      buttonUpTime += TIME_BUTTON_SHORT;
      return UP;
    }

  } else {
    buttonUpTime = 0;
  }

  if (digitalRead(BUTTON_DOWN) == LOW) {
    if (buttonDownTime == 0) {
      Serial.println("Button down");
      delay(15);
      buttonDownTime = millis() + TIME_BUTTON_LONG;
      return DOWN;

    } else if (millis() > buttonDownTime){
      Serial.println("Button down");
      buttonDownTime += TIME_BUTTON_SHORT;
      return DOWN;
    }

  } else {
    buttonDownTime = 0;
  }

  return -1;
}


double adc2temp(int adc, float sr){
  float steinhart, vadc, radc, rinf;

  //convert ADC value to resistance
  vadc = Vin * adc/1023.0;
  radc = sr*vadc/(Vin-vadc);  //sr*(Vin-vadc)/vadc

  rinf = THERMISTORNOMINAL*exp(-BCOEFFICIENT/TEMPERATURENOMINAL);               //R0*e^(-B/T0)
  steinhart = BCOEFFICIENT/log(radc/rinf);                                      //T = B/log(Rout/Rinf)
  steinhart -= 273.15;                                                          // convert to C

  return steinhart;
}
