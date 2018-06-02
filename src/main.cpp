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


// *********************************************************************
// LCDML MENU/DISP
// *********************************************************************
// LCDML_0        => layer 0
// LCDML_0_X      => layer 1
// LCDML_0_X_X    => layer 2
// LCDML_0_X_X_X  => layer 3
LCDML_add         (0  , LCDML_0       , 1 , "Empezar"            , mFunc_start); // LCDML_add(id, prev_layer, new_num, lang_char_array, callback_function)
LCDML_addAdvanced (1  , LCDML_0       , 2 , NULL, "" , mDyn_time, 0, _LCDML_TYPE_dynParam); // LCDMenuLib_add(id, prev_layer,     new_num, condetion,   lang_char_array, callback_function, parameter (0-255), menu function type  )
LCDML_addAdvanced (2  , LCDML_0       , 3 , NULL, "" , mDyn_temp, 0, _LCDML_TYPE_dynParam); // LCDMenuLib_add(id, prev_layer,     new_num, condetion,   lang_char_array, callback_function, parameter (0-255), menu function type  )
LCDML_add         (3  , LCDML_0       , 4 , "Ajuste PID"         , NULL); // LCDML_add(id, prev_layer, new_num, lang_char_array, callback_function)
LCDML_add         (4  , LCDML_0_4     , 1 , "Automático"         , mFunc_setPID); // LCDML_add(id, prev_layer, new_num, lang_char_array, callback_function)
LCDML_add         (5  , LCDML_0_4     , 2 , "Manual"             , NULL); // LCDML_add(id, prev_layer, new_num, lang_char_array, callback_function)
LCDML_addAdvanced (6  , LCDML_0_4_2       , 1 , NULL, "" , mDyn_Kp, 0, _LCDML_TYPE_dynParam); // LCDMenuLib_add(id, prev_layer,     new_num, condetion,   lang_char_array, callback_function, parameter (0-255), menu function type  )
LCDML_addAdvanced (7  , LCDML_0_4_2       , 2 , NULL, "" , mDyn_Ki, 0, _LCDML_TYPE_dynParam); // LCDMenuLib_add(id, prev_layer,     new_num, condetion,   lang_char_array, callback_function, parameter (0-255), menu function type  )
LCDML_addAdvanced (8  , LCDML_0_4_2       , 3 , NULL, "" , mDyn_Kd, 0, _LCDML_TYPE_dynParam); // LCDMenuLib_add(id, prev_layer,     new_num, condetion,   lang_char_array, callback_function, parameter (0-255), menu function type  )
LCDML_createMenu(_LCDML_DISP_cnt);






//##############################################################################
//                                   S E T U P
//##############################################################################
void setup() {
  Serial.begin(115200);
  Serial.println("init...");

  setupLCD();
  //u8g2.begin();
  //display.begin();

  // set PIN modes
  //pinMode(BUTTON_UP,    INPUT_PULLUP);
  //pinMode(BUTTON_DOWN,  INPUT_PULLUP);
  //pinMode(BUTTON_LEFT,  INPUT_PULLUP);
  //pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(PIN_THM,      INPUT);
  pinMode(PIN_PWM,      OUTPUT);

  // Si la EEPROM tiene una versión compatible, se cargan los valores
  if (EEPROM.read(ADDR_FIRM_VERSION) >= FIRM_VERSION){
    Serial.println("restoring EEPROM values");
    for( unsigned int i=0 ; i<sizeof(PIDDATA) ; i++  ){
      pidData.b[i] = EEPROM.read( ADDR_PID_DATA+i );
    }

  // si la versión no es compatible, se cargan los valores por defecto
  } else {
    Serial.println("setting default PID values");
    pidData.datos.pidKp = DEFAULT_KP;
    pidData.datos.pidKi       = DEFAULT_KI;
    pidData.datos.pidKd       = DEFAULT_KD;
    pidData.datos.pidSetPoint = DEFAULT_TEMP;
    fermData.timeSet          = DEFAULT_TIME;
  }

  Serial.print("pidSetPoint: ");Serial.print(pidData.datos.pidSetPoint);Serial.print(" Kp: ");Serial.print(pidData.datos.pidKp);Serial.print(" pidKi: ");Serial.print(pidData.datos.pidKi);Serial.print(" Kd: ");Serial.println(pidData.datos.pidKd);

  myPID.SetTunings(pidData.datos.pidKp,pidData.datos.pidKi,pidData.datos.pidKd);
  myPID.SetMode(AUTOMATIC);    //set mode automatic (on) manual (off)


showMenu=true;
//TODO: mostrar menú para empezar, ajustar valores o iniciar el autotunning
  /*while(menu){
    temp.add(analogRead(PIN_THM));    //lee la temperatura mientras se muestra menu
    LCDML.loop();
  }*/

  serialTime  = 0;
  lcdTime     = 0;
  pidTime     = 0;
  fermData.startTime    = millis();                       //TODO: mover a la función de inicio
  fermData.endTime = fermData.startTime + (fermData.timeSet*3600*1000); //TODO: mover a la función de inicio
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
    Serial.print("pidSetPoint: ");Serial.print(pidData.datos.pidSetPoint); Serial.print(" ");
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

  pidData.datos.pidKp = aTune.GetKp();
  pidData.datos.pidKi = aTune.GetKi();
  pidData.datos.pidKd = aTune.GetKd();
  for( unsigned int i=0 ; i<sizeof(PIDDATA) ; i++  ) {
    EEPROM.write( ADDR_PID_DATA+i , pidData.b[i] );
  }
  EEPROM.write(ADDR_FIRM_VERSION, FIRM_VERSION);
  myPID.SetTunings(pidData.datos.pidKp,pidData.datos.pidKi,pidData.datos.pidKd);

  Serial.println("Autotunning terminado");
  Serial.print(" Kp: "); Serial.print(pidData.datos.pidKp); Serial.print(" pidKi: "); Serial.print(pidData.datos.pidKi); Serial.print(" Kd: "); Serial.println(pidData.datos.pidKd);

  resetFunc();  //reboot!!
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

  rinf = THERMISTORNOMINAL*exp(-BCOEFFICIENT/TEMPERATURENOMINAL);               //R0*e^(-B/T0)
  steinhart = BCOEFFICIENT/log(radc/rinf);                                      //T = B/log(Rout/Rinf)
  steinhart -= 273.15;                                                          // convert to C

  return steinhart;
}





void setupLCD() {
    // create menu
  LCDML_setup(_LCDML_DISP_cnt);
}





  // *********************************************************************
  void lcdml_menu_control(void){
    // If something must init, put in in the setup condition
    if(LCDML.BT_setup()) {
      // runs only once
      // init buttons
      pinMode(_LCDML_CONTROL_digital_enter      , INPUT_PULLUP);
      pinMode(_LCDML_CONTROL_digital_up         , INPUT_PULLUP);
      pinMode(_LCDML_CONTROL_digital_down       , INPUT_PULLUP);
      # if(_LCDML_CONTROL_digital_enable_quit == 1)
        pinMode(_LCDML_CONTROL_digital_quit     , INPUT_PULLUP);
      # endif
      # if(_LCDML_CONTROL_digital_enable_lr == 1)
        pinMode(_LCDML_CONTROL_digital_left     , INPUT_PULLUP);
        pinMode(_LCDML_CONTROL_digital_right    , INPUT_PULLUP);
      # endif
    }

    #if(_LCDML_CONTROL_digital_low_active == 1)
    #  define _LCDML_CONTROL_digital_a !
    #else
    #  define _LCDML_CONTROL_digital_a
    #endif

    uint8_t but_stat = 0x00;

    bitWrite(but_stat, 0, _LCDML_CONTROL_digital_a(digitalRead(_LCDML_CONTROL_digital_enter)));
    bitWrite(but_stat, 1, _LCDML_CONTROL_digital_a(digitalRead(_LCDML_CONTROL_digital_up)));
    bitWrite(but_stat, 2, _LCDML_CONTROL_digital_a(digitalRead(_LCDML_CONTROL_digital_down)));
    #if(_LCDML_CONTROL_digital_enable_quit == 1)
    bitWrite(but_stat, 3, _LCDML_CONTROL_digital_a(digitalRead(_LCDML_CONTROL_digital_quit)));
    #endif
    #if(_LCDML_CONTROL_digital_enable_lr == 1)
    bitWrite(but_stat, 4, _LCDML_CONTROL_digital_a(digitalRead(_LCDML_CONTROL_digital_left)));
    bitWrite(but_stat, 5, _LCDML_CONTROL_digital_a(digitalRead(_LCDML_CONTROL_digital_right)));
    #endif

    if (but_stat > 0) {
      if((millis() - g_LCDML_DISP_press_time) >= 200) {
        g_LCDML_DISP_press_time = millis(); // reset press time

        if (bitRead(but_stat, 0)) { LCDML.BT_enter(); }
        if (bitRead(but_stat, 1)) { LCDML.BT_up();    }
        if (bitRead(but_stat, 2)) { LCDML.BT_down();  }
        if (bitRead(but_stat, 3)) { LCDML.BT_quit();  }
        if (bitRead(but_stat, 4)) { LCDML.BT_left();  }
        if (bitRead(but_stat, 5)) { LCDML.BT_right(); }
      }
    }
  }






  /* ******************************************************************** */
  void lcdml_menu_clear()
  /* ******************************************************************** */
  {
  }

  /* ******************************************************************** */
  void lcdml_menu_display()
  /* ******************************************************************** */
  {
    // for first test set font here
    u8g2.setFont(_LCDML_DISP_font);

    // decalration of some variables
    // ***************
    // content variable
    char content_text[_LCDML_DISP_cols];  // save the content text of every menu element
    // menu element object
    LCDMenuLib2_menu *tmp;
    // some limit values
    uint8_t i = LCDML.MENU_getScroll();
    uint8_t maxi = _LCDML_DISP_rows + i;
    uint8_t n = 0;

     // init vars
    uint8_t n_max             = (LCDML.MENU_getChilds() >= _LCDML_DISP_rows) ? _LCDML_DISP_rows : (LCDML.MENU_getChilds());

    uint8_t scrollbar_min     = 0;
    uint8_t scrollbar_max     = LCDML.MENU_getChilds();
    uint8_t scrollbar_cur_pos = LCDML.MENU_getCursorPosAbs();
    uint8_t scroll_pos        = ((1.*n_max * _LCDML_DISP_rows) / (scrollbar_max - 1) * scrollbar_cur_pos);

    // generate content
    u8g2.firstPage();
    do {
      n = 0;
      i = LCDML.MENU_getScroll();
      // update content
      // ***************

        // clear menu
        // ***************

      // check if this element has children
      if (tmp = LCDML.MENU_getObj()->getChild(LCDML.MENU_getScroll())) {
        // loop to display lines
        do {
          // check if a menu element has a condetion and if the condetion be true
          if (tmp->checkCondetion()) {
            // check the type off a menu element
            if(tmp->checkType_menu() == true) {
              // display normal content
              LCDML_getContent(content_text, tmp->getID());
              u8g2.drawStr( _LCDML_DISP_box_x0+_LCDML_DISP_font_w + _LCDML_DISP_cur_space_behind, _LCDML_DISP_box_y0 + _LCDML_DISP_font_h * (n + 1), content_text);
            }
            else {
              if(tmp->checkType_dynParam()) {
                tmp->callback(n);
              }
            }
            // increment some values
            i++;
            n++;
          }
        // try to go to the next sibling and check the number of displayed rows
        } while (((tmp = tmp->getSibling(1)) != NULL) && (i < maxi));
      }

      // set cursor
      u8g2.drawStr( _LCDML_DISP_box_x0+_LCDML_DISP_cur_space_before, _LCDML_DISP_box_y0 + _LCDML_DISP_font_h * (LCDML.MENU_getCursorPos() + 1),  _LCDML_DISP_cursor_char);

      if(_LCDML_DISP_draw_frame == 1) {
         u8g2.drawFrame(_LCDML_DISP_box_x0, _LCDML_DISP_box_y0, (_LCDML_DISP_box_x1-_LCDML_DISP_box_x0), (_LCDML_DISP_box_y1-_LCDML_DISP_box_y0));
      }

      // display scrollbar when more content as rows available and with > 2
      if (scrollbar_max > n_max && _LCDML_DISP_scrollbar_w > 2) {
        // set frame for scrollbar
        u8g2.drawFrame(_LCDML_DISP_box_x1 - _LCDML_DISP_scrollbar_w, _LCDML_DISP_box_y0, _LCDML_DISP_scrollbar_w, _LCDML_DISP_box_y1-_LCDML_DISP_box_y0);

        // calculate scrollbar length
        uint8_t scrollbar_block_length = scrollbar_max - n_max;
        scrollbar_block_length = (_LCDML_DISP_box_y1-_LCDML_DISP_box_y0) / (scrollbar_block_length + _LCDML_DISP_rows);

        //set scrollbar
        if (scrollbar_cur_pos == 0) {                                   // top position     (min)
          u8g2.drawBox(_LCDML_DISP_box_x1 - (_LCDML_DISP_scrollbar_w-1), _LCDML_DISP_box_y0 + 1                                                     , (_LCDML_DISP_scrollbar_w-2)  , scrollbar_block_length);
        }
        else if (scrollbar_cur_pos == (scrollbar_max-1)) {            // bottom position  (max)
          u8g2.drawBox(_LCDML_DISP_box_x1 - (_LCDML_DISP_scrollbar_w-1), _LCDML_DISP_box_y1 - scrollbar_block_length                                , (_LCDML_DISP_scrollbar_w-2)  , scrollbar_block_length);
        }
        else {                                                                // between top and bottom
          u8g2.drawBox(_LCDML_DISP_box_x1 - (_LCDML_DISP_scrollbar_w-1), _LCDML_DISP_box_y0 + (scrollbar_block_length * scrollbar_cur_pos + 1),(_LCDML_DISP_scrollbar_w-2)  , scrollbar_block_length);
        }
      }
    } while ( u8g2.nextPage() );
  }







  /* ===================================================================== *
   *                                                                       *
   * Dynamic content                                                       *
   *                                                                       *
   * ===================================================================== *
   */
  void mDyn_time(uint8_t line) {
    // check if this function is active (cursor stands on this line)
    if (line == LCDML.MENU_getCursorPos()) {
      // make only an action when the cursor stands on this menuitem
      //check Button
      if(LCDML.BT_checkAny()) {
        if(LCDML.BT_checkEnter()) {
          // this function checks returns the scroll disable status (0 = menu scrolling enabled, 1 = menu scrolling disabled)
          if(LCDML.MENU_getScrollDisableStatus() == 0) {
            // disable the menu scroll function to catch the cursor on this point
            // now it is possible to work with BT_checkUp and BT_checkDown in this function
            // this function can only be called in a menu, not in a menu function
            LCDML.MENU_disScroll();
          }

          else {
            // enable the normal menu scroll function
            LCDML.MENU_enScroll();
          }
          // dosomething for example save the data or something else
          LCDML.BT_resetEnter();
        }

        // This check have only an effekt when MENU_disScroll is set
        if(LCDML.BT_checkUp()) {
          if (fermData.timeSet <24){
              fermData.timeSet++;

          } else {
            fermData.timeSet = 0;
          }
        }

        // This check have only an effekt when MENU_disScroll is set
        if(LCDML.BT_checkDown()) {
          if (fermData.timeSet >0){
            fermData.timeSet--;

          } else{
            fermData.timeSet=24;
          }
        }
      }
    }

    char buf[20];
    sprintf (buf, "Tiempo: %d h", fermData.timeSet);

    // setup function
    u8g2.drawStr( _LCDML_DISP_box_x0+_LCDML_DISP_font_w + _LCDML_DISP_cur_space_behind,  (_LCDML_DISP_font_h * (1+line)), buf);     // the value can be changed with left or right
  }




  /* ===================================================================== *
   *                                                                       *
   * Dynamic content                                                       *
   *                                                                       *
   * ===================================================================== *
   */
  void mDyn_temp(uint8_t line) {
    // check if this function is active (cursor stands on this line)
    if (line == LCDML.MENU_getCursorPos()) {
      // make only an action when the cursor stands on this menuitem
      //check Button
      if(LCDML.BT_checkAny()) {
        if(LCDML.BT_checkEnter()) {
          // this function checks returns the scroll disable status (0 = menu scrolling enabled, 1 = menu scrolling disabled)
          if(LCDML.MENU_getScrollDisableStatus() == 0) {
            // disable the menu scroll function to catch the cursor on this point
            // now it is possible to work with BT_checkUp and BT_checkDown in this function
            // this function can only be called in a menu, not in a menu function
            LCDML.MENU_disScroll();
          }

          else {
            // enable the normal menu scroll function
            LCDML.MENU_enScroll();
          }
          // dosomething for example save the data or something else
          LCDML.BT_resetEnter();
        }

        // This check have only an effekt when MENU_disScroll is set
        if(LCDML.BT_checkUp()) {
          if (pidData.datos.pidSetPoint<50){
            pidData.datos.pidSetPoint++;

          } else {
            pidData.datos.pidSetPoint=30;
          }
        }

        // This check have only an effekt when MENU_disScroll is set
        if(LCDML.BT_checkDown()) {
          if (pidData.datos.pidSetPoint>30){
            pidData.datos.pidSetPoint--;

          } else {
            pidData.datos.pidSetPoint=50;
          }
        }
      }
    }

    char buf[20];
    sprintf (buf, "Temperatura: %d C", pidData.datos.pidSetPoint);

    // setup function
    u8g2.drawStr( _LCDML_DISP_box_x0+_LCDML_DISP_font_w + _LCDML_DISP_cur_space_behind,  (_LCDML_DISP_font_h * (1+line)), buf);     // the value can be changed with left or right
  }




  /* ===================================================================== *
   *                                                                       *
   * Dynamic content                                                       *
   *                                                                       *
   * ===================================================================== *
   */
  void mDyn_Kp(uint8_t line) {
    // check if this function is active (cursor stands on this line)
    if (line == LCDML.MENU_getCursorPos()) {
      // make only an action when the cursor stands on this menuitem
      //check Button
      if(LCDML.BT_checkAny()) {
        if(LCDML.BT_checkEnter()) {
          // this function checks returns the scroll disable status (0 = menu scrolling enabled, 1 = menu scrolling disabled)
          if(LCDML.MENU_getScrollDisableStatus() == 0) {
            // disable the menu scroll function to catch the cursor on this point
            // now it is possible to work with BT_checkUp and BT_checkDown in this function
            // this function can only be called in a menu, not in a menu function
            LCDML.MENU_disScroll();
          }

          else {
            // enable the normal menu scroll function
            LCDML.MENU_enScroll();
          }
          // dosomething for example save the data or something else
          LCDML.BT_resetEnter();
        }

        // This check have only an effekt when MENU_disScroll is set
        if(LCDML.BT_checkUp()) {
            pidData.datos.pidKp+=0.1;
        }

        // This check have only an effekt when MENU_disScroll is set
        if(LCDML.BT_checkDown()) {
            pidData.datos.pidKp-=0.1;
        }
      }
    }

    char buf[20];
    sprintf (buf, "Pid Kp: %d", pidData.datos.pidKp);

    // setup function
    u8g2.drawStr( _LCDML_DISP_box_x0+_LCDML_DISP_font_w + _LCDML_DISP_cur_space_behind,  (_LCDML_DISP_font_h * (1+line)), buf);     // the value can be changed with left or right
  }






    /* ===================================================================== *
     *                                                                       *
     * Dynamic content                                                       *
     *                                                                       *
     * ===================================================================== *
     */
    void mDyn_Ki(uint8_t line) {
      // check if this function is active (cursor stands on this line)
      if (line == LCDML.MENU_getCursorPos()) {
        // make only an action when the cursor stands on this menuitem
        //check Button
        if(LCDML.BT_checkAny()) {
          if(LCDML.BT_checkEnter()) {
            // this function checks returns the scroll disable status (0 = menu scrolling enabled, 1 = menu scrolling disabled)
            if(LCDML.MENU_getScrollDisableStatus() == 0) {
              // disable the menu scroll function to catch the cursor on this point
              // now it is possible to work with BT_checkUp and BT_checkDown in this function
              // this function can only be called in a menu, not in a menu function
              LCDML.MENU_disScroll();
            }

            else {
              // enable the normal menu scroll function
              LCDML.MENU_enScroll();
            }
            // dosomething for example save the data or something else
            LCDML.BT_resetEnter();
          }

          // This check have only an effekt when MENU_disScroll is set
          if(LCDML.BT_checkUp()) {
              pidData.datos.pidKi+=0.1;
          }

          // This check have only an effekt when MENU_disScroll is set
          if(LCDML.BT_checkDown()) {
              pidData.datos.pidKi-=0.1;
          }
        }
      }

      char buf[20];
      sprintf (buf, "Pid Kp: %d", pidData.datos.pidKi);

      // setup function
      u8g2.drawStr( _LCDML_DISP_box_x0+_LCDML_DISP_font_w + _LCDML_DISP_cur_space_behind,  (_LCDML_DISP_font_h * (1+line)), buf);     // the value can be changed with left or right
    }






      /* ===================================================================== *
       *                                                                       *
       * Dynamic content                                                       *
       *                                                                       *
       * ===================================================================== *
       */
      void mDyn_Kd(uint8_t line) {
        // check if this function is active (cursor stands on this line)
        if (line == LCDML.MENU_getCursorPos()) {
          // make only an action when the cursor stands on this menuitem
          //check Button
          if(LCDML.BT_checkAny()) {
            if(LCDML.BT_checkEnter()) {
              // this function checks returns the scroll disable status (0 = menu scrolling enabled, 1 = menu scrolling disabled)
              if(LCDML.MENU_getScrollDisableStatus() == 0) {
                // disable the menu scroll function to catch the cursor on this point
                // now it is possible to work with BT_checkUp and BT_checkDown in this function
                // this function can only be called in a menu, not in a menu function
                LCDML.MENU_disScroll();
              }

              else {
                // enable the normal menu scroll function
                LCDML.MENU_enScroll();
              }
              // dosomething for example save the data or something else
              LCDML.BT_resetEnter();
            }

            // This check have only an effekt when MENU_disScroll is set
            if(LCDML.BT_checkUp()) {
                pidData.datos.pidKd+=0.1;
            }

            // This check have only an effekt when MENU_disScroll is set
            if(LCDML.BT_checkDown()) {
                pidData.datos.pidKd-=0.1;
            }
          }
        }

        char buf[20];
        sprintf (buf, "Pid Kp: %d", pidData.datos.pidKd);

        // setup function
        u8g2.drawStr( _LCDML_DISP_box_x0+_LCDML_DISP_font_w + _LCDML_DISP_cur_space_behind,  (_LCDML_DISP_font_h * (1+line)), buf);     // the value can be changed with left or right
      }









    /* ===================================================================== *
     *                                                                       *
     * Menu Callback Function                                                *
     *                                                                       *
     * ===================================================================== *
     */
    void mFunc_start(uint8_t param) {
      if(LCDML.FUNC_setup())          // ****** SETUP *********
      {
        char buf[20];

        u8g2.setFont(_LCDML_DISP_font);
        u8g2.firstPage();
        do {
          u8g2.drawStr( 0, (_LCDML_DISP_font_h * 1), "Pulsa para continuar");
          sprintf (buf, "tiempo: %d h", fermData.timeSet);
          u8g2.drawStr( 0, (_LCDML_DISP_font_h * 2), buf);
          sprintf (buf, "temperatura: %d C", pidData.datos.pidSetPoint);
          u8g2.drawStr( 0, (_LCDML_DISP_font_h * 3), buf);
        } while( u8g2.nextPage() );
      }

      if(LCDML.FUNC_loop()) {          // ****** LOOP *********
        if (LCDML.BT_checkAny()) {
          //TODO: es posible que haga falta copiar aquí la parte del setup
          if (LCDML.BT_checkEnter()) {
              LCDML.FUNC_close();

          } else {
            LCDML.BT_resetAll();
            LCDML.FUNC_goBackToMenu();
          }
        }
      }

      if(LCDML.FUNC_close()) {    // ****** STABLE END *********
        showMenu=false;
      }
    }



    /* ===================================================================== *
     *                                                                       *
     * Menu Callback Function                                                *
     *                                                                       *
     * ===================================================================== *
     */
    void mFunc_setPID(uint8_t param) {    //FIXME: TODOOOOOOOOOOOOOOOOOOO!!!!!!!!!!!!!!!!!!!!
      if(LCDML.FUNC_setup())          // ****** SETUP *********
      {
        char buf[20];

        u8g2.setFont(_LCDML_DISP_font);
        u8g2.firstPage();
        do {
          u8g2.drawStr( 0, (_LCDML_DISP_font_h * 1), "Pulsa para continuar");
          sprintf (buf, "tiempo: %d h", fermData.timeSet);
          u8g2.drawStr( 0, (_LCDML_DISP_font_h * 2), buf);
          sprintf (buf, "temperatura: %d C", pidData.datos.pidSetPoint);
          u8g2.drawStr( 0, (_LCDML_DISP_font_h * 3), buf);
        } while( u8g2.nextPage() );
      }

      if(LCDML.FUNC_loop()) {          // ****** LOOP *********
        if (LCDML.BT_checkAny()) {
          //TODO: es posible que haga falta copiar aquí la parte del setup
          if (LCDML.BT_checkEnter()) {
              LCDML.FUNC_close();

          } else {
            LCDML.BT_resetAll();
            LCDML.FUNC_goBackToMenu();
          }
        }
      }

      if(LCDML.FUNC_close()) {    // ****** STABLE END *********
        showMenu=false;
      }
    }
