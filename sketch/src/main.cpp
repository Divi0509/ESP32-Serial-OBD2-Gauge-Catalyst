/* OBD-II Gauge Catalyst
 * @file main.cpp
 * @author Va&Cob
 * @date 2025-12-01
 * @Copyright (c) 2025 Va&Cob
 *
 * Hardware: CYD 2.8" ESP32 Dev Board
 * IDE: PlatformIO
 * Partition scheme: Miminal SPIFFS(1.9 MB App with OTA/ 190KB SPIFFS)
*/

// Library
#include "LGFX_CYD.h"
#include "ui/ui.h"
#include <Arduino.h>
#include <lvgl.h>

#include "accessory.h"
#include "elm327.h"


// Pin configuration
#define CALIB_PIN 0
#define SELECTOR_PIN 27
#define LDR_PIN 34

#define SDA_PIN 27
#define SCL_PIN 22

#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18
#define SD_CS 5

#define ROTATION 3
// Define screen dimensions for LVGL buffer allocation.
// These are the dimensions *after* rotation.
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

long runtime = 0;
long elm_timeout = 0;

#define ELM_CONN_TIMEOUT 2000  // timer for communicate with ELM
#define ELM_INIT_TIMEOUT 15000 // ELM init command timeut
#define ELM_RESP_TIMEOUT 500   // set elm response timeout

LGFX tft; /* LGFX instance */

//-----------------------------------------

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  lv_disp_flush_ready(disp_drv);
}

// touchpad callback
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  uint16_t touchX, touchY;
  bool touched = tft.getTouch(&touchX, &touchY);

  if (touched) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touchX;
    data->point.y = touchY;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ################### SETUP ############################
void setup() {
  // init communication
  Serial.begin(115200);
  delay(500);
  Serial.print(F("\n<< OBD-II Gauge Fusion >> by Va&Cob\nVersion "));
  Serial.print(version);
  Serial.print(F(" | "));
  Serial.println(compile_date);

  // pin configuration
  analogSetAttenuation(ADC_0db);       // 0dB(1.0 ครั้ง) 0~800mV   for LDR
  pinMode(LDR_PIN, ANALOG);            // ldr analog input read brightness
  pinMode(SELECTOR_PIN, INPUT_PULLUP); // button
  pinMode(CALIB_PIN, INPUT_PULLUP);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);

  // init TFT
  tft.begin();
  tft.setSwapBytes(true);
  tft.setRotation(ROTATION);
  tft.setBrightness(brightness);

  // screen calibration
  if (digitalRead(CALIB_PIN) == LOW || digitalRead(SELECTOR_PIN) == LOW) {
    uint16_t calData[8];
    tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 15);
  }
  uint16_t screenWidth = tft.width();
  uint16_t screenHeight = tft.height();

  lv_init();
  // LVGL display buffers
  static lv_disp_draw_buf_t draw_buf;
  // Allocate buffer for 1/10 of the screen size
  static lv_color_t buf1[TFT_WIDTH * TFT_HEIGHT / 10];
  static lv_color_t buf2[TFT_WIDTH * TFT_HEIGHT / 10];
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, TFT_WIDTH * TFT_HEIGHT / 10);

  // Register display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Touch input driver
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  resetData(); // prepare variable

  // App start here
  ui_init();                 // start ui and display
  initFileSystem();          // LittleFS
  initWidget();              // init widgets
  live_chart_init();         // add chart series to chart widget
  loadSetting();             // load configuration
  initSpeaker();             // Initialize speaker once
  initMPU(SDA_PIN, SCL_PIN); // init MPU6050

  beepbeep();
  elm_timeout = millis() - ELM_INIT_TIMEOUT;

} // setup

// #################### MAIN  #####################
void loop() {

  lv_task_handler();
  checkCPUTemp();
  autoDim();
  //----------------------

  // initialize ELM
  if (initializing && millis() - elm_timeout > ELM_INIT_TIMEOUT) { // elm stop resonse too long during initializng
    elm_timeout = millis();                                        // reset timer
    setELMbaudrate();                                              // set ELM327 baudrate
    initIndex = 0;
    while (Serial.available()) Serial.read();
    RED_ON;
    String command = elm327Init[initIndex] + "\r";
    delay(50);
    Serial.print(command);
    debug(command);
  }
//------------------
  // ELM Read
  if (Serial.available() > 0) {
    char incomingChar = Serial.read();
    if (incomingChar == '>') {         // check prompt
      while (Serial.available() > 0) { // clear garbage
        Serial.read();                 // just read & discard
      }
   
      // initializing mode
      if (initializing) {
        log(elm_message.c_str()); // show response
        RED_OFF;
        if (initIndex == elm327InitCount - 2) { // 0100 check can bus connection first
          if (elm_message.indexOf("41 00") != -1) {

            //-----  //OK connect return with 41 00 xx yy ---------------
            lv_obj_clear_flag(ui_ScreenGauge_Label_info1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(ui_ScreenGauge_Label_info1, lv_color_hex(0xFFFFFF),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(ui_ScreenGauge_Label_info1, "- Catalyst -");
            GREEN_ON;

          } else { // UNABLE TO CONNECT -> try again
            initIndex = elm327InitCount - 3;
            //----- show notification CAN BUS ERROR ---------------
            lv_obj_clear_flag(ui_ScreenGauge_Label_info1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(ui_ScreenGauge_Label_info1, lv_color_hex(0xFF9D00),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(ui_ScreenGauge_Label_info1, "Unable to connect\nCANBUS Error!");
            RED_ON;
          }
        }

        initIndex++; // next AT command

        if (initIndex < elm327InitCount) {
          elm_message = "";
          String command = elm327Init[initIndex] + "\r"; // send next init command
          delay(50);
          Serial.print(command);
          debug(command);
        } else { // end of AT command set
          initializing = false;
          prompt = true;
          pidIndex = -1;
        }

        // running mode
      } else {
        prompt = true; // pid response ready to calculate & display
      }

    } else {                            // collect message response
      elm_message.concat(incomingChar); // keep elm327 respond
    }
  }
//------------------
  // Send another PID to elm327 //> ELM327 ready! -> request next PID
  if (page == 0 || page == 1 || page == 2) { // skip setup and diagnostic page
    if (prompt) {
      prompt = false; // no prompt from ELM327

      // display guage
      if (!skip && pidIndex != -1) {
        updateGaugeDisplay(pidIndex, elm_message); // update meter screen
        pidRead++;                                 // for calculate pid/s
      }
      elm_message = "";
      pidIndex++; // point to next pid

      // 1 round passed
      if (pidIndex >= endPidIndex[page]) {
        pidIndex = startPidIndex[page]; // reset pid index

        if (systemInfo) {
          String status = String(pidRead * 1000.0 / (millis() - runtime), 0) + " p/s  " + String(cpuTemp_c, 0) + " °C";
          // String status = String(startPidIndex[page]) + " - " + String(endPidIndex[page])+", " + String(pidRead *
          // 1000.0 / (millis() - runtime), 0) + " p/s " + String(cpuTemp_c, 0) + " °C";
          lv_label_set_text(ui_ScreenGauge_Label_info1, status.c_str());
          lv_label_set_text(ui_ScreenMonitor_Label_info2, status.c_str());
          lv_label_set_text(ui_ScreenPerformance_Label_info3, status.c_str());
          runtime = millis();
        }
        pidRead = 0; // reset pid read counter
      }

      // next PID
      if (pidCurrentSkip[pidIndex] <= 0) {                // end of skip loop , go next index
        pidCurrentSkip[pidIndex] = pidReadSkip[pidIndex]; // read max delay loop to current
        skip = false;
        Serial.print(pidList[pidIndex] + "\r"); // send PID request
        elm_timeout = millis();
      } else {                      // skip sending request
        pidCurrentSkip[pidIndex]--; // decrese loop count
        prompt = true;
        skip = true;
      } // else

    } else { // not prompt

      // elm stop resonse too long
      if (!initializing && millis() - elm_timeout > ELM_RESP_TIMEOUT) {
        elm_timeout = millis();
        prompt = true;
      }

    } // else if prompt
  } // if (page 0-2)

 
//------------------
} // loop