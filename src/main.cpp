/*********************************************************************
Programa para controlar la yogurtera
El programa, básicamente, mantiene la temperatura constante (aprox. 42ºC)
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
      rápidamente a 42ºC
    * Poner el inóculo (2 cucharadas de yogurt o inóculo comprado) a la leche
      templada (~42ºC)
    * Seleccionar tiempo y temperatura, y pulsar comenzar
    * refrigerar los yogures cuando termine el tiempo de fermentación

*********************************************************************/
#include <main.h>


//### Reset function
void(* resetFunc) (void) = 0; //declare reset function @ address 0



//##############################################################################
//                            C L A S E S
//##############################################################################
class MyRenderer : public MenuComponentRenderer {
public:
    void render(Menu const& menu) const {
      lcd.clearDisplay();
      lcd.setCursor(0, 0);
      lcd.setTextSize(1);

      for (int i = 0; i < menu.get_num_components(); ++i) {
        MenuComponent const* cp_m_comp = menu.get_menu_component(i);

        if (cp_m_comp->is_current()){
          lcd.setTextColor(WHITE, BLACK);

        } else {
          lcd.setTextColor(BLACK);
        }

        cp_m_comp->render(*this);
        lcd.println();
      }
      lcd.display();
    }

    void render_menu_item(MenuItem const& menu_item) const {
        lcd.print(menu_item.get_name());
    }

    void render_back_menu_item(BackMenuItem const& menu_item) const {
        lcd.print(menu_item.get_name());
    }

    void render_numeric_menu_item(NumericMenuItem const& menu_item) const {
        lcd.print(menu_item.get_name());
        lcd.print(": ");
        lcd.print(menu_item.get_value(), 0);
    }

    void render_menu(Menu const& menu) const {
        lcd.println(menu.get_name());
    }
};

MyRenderer my_renderer;
MenuSystem      ms (my_renderer);
MenuItem        mm_mi1("Empezar"      , &on_start_selected);
NumericMenuItem mm_mi2("Tiempo"       , &on_setTime_selected, DEFAULT_TIME, 1, 24, 1);
//NumericMenuItem mm_mi3("Temp. "       , &on_setTemp_selected, DEFAULT_TEMP, 20, 50, 1);
//MenuItem        mm_mi4("Ajuste PID"   , &on_autoPID_selected);





//##############################################################################
//                                   S E T U P
//##############################################################################
void setup() {
  Serial.begin(115200);
  Serial.println("init...");

  lcd.begin();
  lcd.setContrast(50);
  lcd.display();

  // set PIN modes
  pinMode(BUTTON_ENTER, INPUT_PULLUP);
  pinMode(BUTTON_DOWN,  INPUT_PULLUP);
  pinMode(PIN_THM,      INPUT);
  pinMode(PIN_PWM,      OUTPUT);

  // Si la EEPROM tiene una versión compatible, se cargan los valores
  if (EEPROM.read(ADDR_FIRM_VERSION) == FIRM_VERSION){

    Serial.println("restoring EEPROM values");
    for( unsigned int i=0 ; i<sizeof(PIDDATA) ; i++  ){
      pidData.b[i] = EEPROM.read( ADDR_PID_DATA+i );
    }

  // si la versión no es compatible, se cargan los valores por defecto
  } else {
    Serial.println("setting default PID values");
    pidData.datos.pidKp       = DEFAULT_KP;
    pidData.datos.pidKi       = DEFAULT_KI;
    pidData.datos.pidKd       = DEFAULT_KD;
    pidData.datos.pidSetPoint = DEFAULT_TEMP;
    fermData.timeSet          = DEFAULT_TIME;
  }

  Serial.print("pidSetPoint: ");Serial.print(pidData.datos.pidSetPoint);Serial.print(" Kp: ");Serial.print(pidData.datos.pidKp);Serial.print(" pidKi: ");Serial.print(pidData.datos.pidKi);Serial.print(" Kd: ");Serial.println(pidData.datos.pidKd);

  myPID.SetTunings(pidData.datos.pidKp,pidData.datos.pidKi,pidData.datos.pidKd);
  myPID.SetMode(AUTOMATIC);    //set mode automatic (on) manual (off)

  //construye menu
  ms.get_root_menu().add_item(&mm_mi1);
  ms.get_root_menu().add_item(&mm_mi2);
  //ms.get_root_menu().add_item(&mm_mi3);
  //ms.get_root_menu().add_item(&mm_mi4);

  aTune.SetOutputStep(10);
  aTune.SetControlType(1);
  aTune.SetNoiseBand(0.8);
  aTune.SetLookbackSec(20);

  fermentation = false;
  autoTunning  = false;
  tempTime     = 0;
  lcdTime      = 0;
  serialTime   = 0;
  pidTime      = 0;
}





//##############################################################################
//                                   L O O P
//##############################################################################
void loop() {
  if(digitalRead(BUTTON_ENTER)==LOW){
    Serial.println("Select button pressed");
    ms.select();
    ms.display();
    delay(100);
  }

  if(digitalRead(BUTTON_DOWN)==LOW){
    Serial.println("Down button pressed");
    ms.next(true);
    delay(100);
  }

  //lee el termistor
  if(millis() > tempTime){
    temp.add(analogRead(PIN_THM));
    tempTime = millis() + TIME_UPDATE_TEMP;
  }

  //calcula pidOutput a partir de pidInput y ajusta PWM en concordancia
  if(millis() > pidTime){
    pidInput = adc2temp(temp.getMedian(), VCC, SERIESRESISTOR, true);
    pidTime = millis() + TIME_UPDATE_PID;
  }

  if((fermentation || autoTunning) && millis() > pidTime){
    if (!autoTunning){
      myPID.Compute();
    }
    analogWrite(PIN_PWM, pidOutput);
    pidTime = millis() + TIME_UPDATE_PID;
  }

  //update LCD if needed
  if(millis() > lcdTime) {
    if (fermentation){
      lcd.setTextSize(2);
      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.println(pidInput, 2);

      lcd.print("Tiempo: ");
      unsigned long t = (fermData.endTime - millis())/1000;
      int h,m,s;
      h = floor(t/3600);
      m = floor((t - h * 3600) / 60);
      s = floor(t - (h * 3600 + m * 60));

      lcd.print(h);
      lcd.print(":");
      lcd.print(m);
      lcd.print(":");
      lcd.print(s);

      lcd.display();

    } else if (autoTunning){
      lcd.setCursor(0,0);

      char buf[20];
      sprintf (buf, "Kp: %2.1f", pidData.datos.pidKp);
      lcd.println(buf);

      sprintf (buf, "Ki: %2.1f", pidData.datos.pidKi);
      lcd.println(buf);

      sprintf (buf, "Kd: %2.1f", pidData.datos.pidKd);
      lcd.println(buf);

      sprintf (buf, "Set: %2.1f", pidData.datos.pidSetPoint);
      lcd.println(buf);

      sprintf (buf, "Temp: %2.1f",  adc2temp(temp.getMedian(), VCC, SERIESRESISTOR));
      lcd.println(buf);

      lcd.display();

    } else {
      ms.display();

    }

    lcdTime = millis() + TIME_REFRESH_LCD;
  }

  // Serial print
  if(millis() > serialTime) {
    Serial.print("pidSetPoint: ");Serial.print(pidData.datos.pidSetPoint); Serial.print(" ");
    Serial.print("pidInput: ");Serial.print(pidInput); Serial.print(" ");
    Serial.print("pidOutput: ");Serial.println(pidOutput);
    serialTime = millis() + TIME_REFRESH_SERIAL;
  }

  if(fermentation && millis() > fermData.endTime){
    Serial.print("Fermentación terminada!");
    fermentation = false;
    analogWrite(PIN_PWM, 0);

    while (digitalRead(BUTTON_ENTER)==HIGH && digitalRead(BUTTON_DOWN)==HIGH) {
      if(millis() > lcdTime) {
        lcd.setCursor(0, 0);
        lcd.setTextSize(3);
        lcd.print("END");
        lcd.display();
        lcdTime = millis() + TIME_REFRESH_LCD;
      }
    }
    delay(100);
  }

  if (autoTunning && aTune.Runtime() != 0){
    Serial.println("Autotunning terminado");
    autoTunning = false;

    pidData.datos.pidKp = aTune.GetKp();
    pidData.datos.pidKi = aTune.GetKi();
    pidData.datos.pidKd = aTune.GetKd();
    for( unsigned int i=0 ; i<sizeof(PIDDATA) ; i++  ) {
      EEPROM.write( ADDR_PID_DATA+i , pidData.b[i] );
    }
    EEPROM.write(ADDR_FIRM_VERSION, FIRM_VERSION);
    myPID.SetTunings(pidData.datos.pidKp,pidData.datos.pidKi,pidData.datos.pidKd);
    Serial.print(" Kp: "); Serial.print(pidData.datos.pidKp); Serial.print(" pidKi: "); Serial.print(pidData.datos.pidKi); Serial.print(" Kd: "); Serial.println(pidData.datos.pidKd);
  }
}



// Función pensada para transformar valores leídos con entrada analógica
// a ºC.
//    - adc: valor obtenido con analogRead
//    - Vin: tensión de alimentación del divisor de tensión
//    - sr: resistencia serie del divisor de tensión
//    - vccTherm: thermistor a VCC (true) o a GND (false)
double adc2temp(int adc, float Vin, float sr){
  return adc2temp(adc, Vin, sr, true);
}
double adc2temp(int adc, float Vin, float sr, boolean vccTherm){
  float steinhart, vadc, radc, rinf;

  //convert ADC value to resistance
  vadc = Vin * adc/1023.0;
  if (vccTherm){
      radc = sr*vadc/(Vin-vadc);  //sr*(Vin-vadc)/vadc
  } else {
      radc = sr*(Vin-vadc)/vadc;
  }


  steinhart = radc / THERMISTORNOMINAL;        // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C

  /*
  rinf = THERMISTORNOMINAL*exp(-BCOEFFICIENT/TEMPERATURENOMINAL);               //R0*e^(-B/T0)
  steinhart = BCOEFFICIENT/log(radc/rinf);                                      //T = B/log(Rout/Rinf)
  steinhart -= 273.15;                                                          // convert to C
  */

  return steinhart;
}
/*#define THERMISTORNOMINAL   100000.0  // resistance at 25 degrees C
#define TEMPERATURENOMINAL  25.0      // temp. for nominal resistance (almost always 25 C)
#define BCOEFFICIENT        3950.0    // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR      98700.0   // the value of the 'other' resistor
#define VCC                 5.0*/



void on_start_selected(MenuComponent* p_menu_component){
  Serial.println("Start fermentation");
  fermData.startTime = millis();
  fermData.endTime   = fermData.startTime + (fermData.timeSet*3600*1000);
  fermentation = true;
  //TODO: grabar en eeprom si ha habido modificación de parámetros
}

void on_setTime_selected(NumericMenuItem* p_menu_component){
  fermData.timeSet = p_menu_component->get_value();
  Serial.print("timeSet: ");Serial.println(fermData.timeSet);
}

void on_setTemp_selected(NumericMenuItem* p_menu_component){
  pidData.datos.pidSetPoint = p_menu_component->get_value();
  Serial.print("pidSetPoint: ");Serial.print(pidData.datos.pidSetPoint); Serial.println(" ");
}

void on_autoPID_selected(MenuItem* p_menu_component){
  autoTunning = true;
  Serial.println("autotuning...");
}
