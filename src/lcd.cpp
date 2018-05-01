#include <lcd.h>



// *********************************************************************
// Objects
// *********************************************************************
LCDMenuLib2_menu  LCDML_0 (255, 0, 0, NULL, NULL); // root menu element (do not change)
LCDMenuLib2       LCDML   (LCDML_0, _LCDML_DISP_rows, _LCDML_DISP_cols, lcdml_menu_display, lcdml_menu_clear, lcdml_menu_control);

unsigned long g_LCDML_DISP_press_time = 0;


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
  LCDML_add         (4  , LCDML_0_3     , 1 , "Automático"         , mFunc_setPID); // LCDML_add(id, prev_layer, new_num, lang_char_array, callback_function)
  LCDML_add         (5  , LCDML_0_3     , 2 , "Manual"             , NULL); // LCDML_add(id, prev_layer, new_num, lang_char_array, callback_function)

  // menu element count - last element id
  // this value must be the same as the last menu element
  #define _LCDML_DISP_cnt    5

  // create menu
  LCDML_createMenu(_LCDML_DISP_cnt);

  // LCDMenuLib Setup
  LCDML_setup(_LCDML_DISP_cnt);




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
          timeSet++;
        }

        // This check have only an effekt when MENU_disScroll is set
        if(LCDML.BT_checkDown()) {
          timeSet--;
        }
      }
    }

    char buf[20];
    sprintf (buf, "Tiempo: %d h", g_dynParam);

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
          pidSetPoint++;
        }

        // This check have only an effekt when MENU_disScroll is set
        if(LCDML.BT_checkDown()) {
          pidSetPoint--;
        }
      }
    }

    char buf[20];
    sprintf (buf, "Temperatura: %d C", g_dynParam);

    // setup function
    u8g2.drawStr( _LCDML_DISP_box_x0+_LCDML_DISP_font_w + _LCDML_DISP_cur_space_behind,  (_LCDML_DISP_font_h * (1+line)), buf);     // the value can be changed with left or right
  }
















    /* ===================================================================== *
     *                                                                       *
     * Menu Callback Function                                                *
     *                                                                       *
     * ===================================================================== *
     *
     * EXAMPLE CODE:
    // *********************************************************************
    void your_function_name(uint8_t param)
    // *********************************************************************
    {
      if(LCDML.FUNC_setup())          // ****** SETUP *********
      {
        // setup
        // is called only if it is started
        // starts a trigger event for the loop function every 100 millisecounds
        LCDML.FUNC_setLoopInterval(100);
      }
      if(LCDML.FUNC_loop())           // ****** LOOP *********
      {
        // loop
        // is called when it is triggert
        // - with LCDML_DISP_triggerMenu( millisecounds )
        // - with every button status change
        // check if any button is presed (enter, up, down, left, right)
        if(LCDML.BT_checkAny()) {
          LCDML.FUNC_goBackToMenu();
        }
      }
      if(LCDML.FUNC_close())      // ****** STABLE END *********
      {
        // loop end
        // you can here reset some global vars or delete it
      }
    }

     * ===================================================================== *
     */
    // *********************************************************************
    uint8_t g_button_value = 0; // button value counter (global variable)
    void mFunc_start(uint8_t param) {
      if(LCDML.FUNC_setup())          // ****** SETUP *********
      {
        // setup function
        // print lcd content
        char buf[17];
        sprintf (buf, "count: %d of 3", 0);

        u8g2.setFont(_LCDML_DISP_font);
        u8g2.firstPage();
        do {
          u8g2.drawStr( 0, (_LCDML_DISP_font_h * 1), "press a or w button");
          u8g2.drawStr( 0, (_LCDML_DISP_font_h * 2), buf);
        } while( u8g2.nextPage() );

        // Reset Button Value
        g_button_value = 0;

        // Disable the screensaver for this function until it is closed
        LCDML.FUNC_disableScreensaver();
      }

      if(LCDML.FUNC_loop())           // ****** LOOP *********
      {
        // loop function, can be run in a loop when LCDML_DISP_triggerMenu(xx) is set
        // the quit button works in every DISP function without any checks; it starts the loop_end function

        // the quit button works in every DISP function without any checks; it starts the loop_end function
        if (LCDML.BT_checkAny()) // check if any button is pressed (enter, up, down, left, right)
        {
          if (LCDML.BT_checkLeft() || LCDML.BT_checkUp()) // check if button left is pressed
          {
            LCDML.BT_resetLeft(); // reset the left button
            LCDML.BT_resetUp(); // reset the left button
            g_button_value++;

            // update lcd content
            char buf[20];
            sprintf (buf, "count: %d of 3", g_button_value);

            u8g2.setFont(_LCDML_DISP_font);
            u8g2.firstPage();
            do {
              u8g2.drawStr( 0, (_LCDML_DISP_font_h * 1), "press a or w button");
              u8g2.drawStr( 0, (_LCDML_DISP_font_h * 2), buf);
            } while( u8g2.nextPage() );
          }
        }

       // check if button count is three
        if (g_button_value >= 3) {
          LCDML.FUNC_goBackToMenu();      // leave this function
        }
      }

      if(LCDML.FUNC_close())     // ****** STABLE END *********
      {
        // you can here reset some global vars or do nothing
      }
    }
