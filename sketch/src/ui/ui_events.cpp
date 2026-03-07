// custom functions library

#include "accessory.h"
#include "elm327.h"
#include <Preferences.h> //save permanent data
extern Preferences pref;
#include <WiFi.h>

#include "ui.h"

#include "nes_audio.h"
#include "song.h"

void changeScreenGauge(lv_event_t *e) {
  stopMusicPlayback();
  clickSound();
  page = 0;
  pidIndex = startPidIndex[page];
}
void changeScreenMonitor(lv_event_t *e) {
  clickSound();
  page = 1;
  pidIndex = startPidIndex[page];
}
void changeScreenPerformance(lv_event_t *e) {
  clickSound();
  page = 2;
  pidIndex = startPidIndex[page];
}
void changeScreenConfig(lv_event_t *e) {
  clickSound();
  page = 3;
  pidIndex = startPidIndex[page];
}
void changeScreenService(lv_event_t *e) {
  clickSound();
  page = 4;
  pidIndex = startPidIndex[page];
}

void showSystemInfo(lv_event_t *e) {
  clickSound();
  systemInfo = true;
  lv_obj_clear_flag(ui_ScreenGauge_Label_info1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_ScreenMonitor_Label_info2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_ScreenPerformance_Label_info3, LV_OBJ_FLAG_HIDDEN);
}

void hideSystemInfo(lv_event_t *e) {
  clickSound();
  systemInfo = false;
  lv_obj_add_flag(ui_ScreenGauge_Label_info1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ScreenMonitor_Label_info2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ScreenPerformance_Label_info3, LV_OBJ_FLAG_HIDDEN);
}

void autoPowerOffEnable(lv_event_t *e) {
  autoPowerOff = true;
  clickSound();
}
void autoPowerOffDisable(lv_event_t *e) {
  autoPowerOff = false;
  clickSound();
}

static int prev_left = 0;
static int prev_right = 0;
void setBrightness(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target(e); // Get the slider that triggered the event
  int left = lv_slider_get_left_value(slider);
  int right = lv_slider_get_value(slider);

  if (left != prev_left) {
    // Left handle is being moved
    dim_brightness = left;
    tft.setBrightness(dim_brightness);
    prev_left = left;
  }
  if (right != prev_right) {
    // Right handle is being moved
    brightness = right;
    tft.setBrightness(brightness);
    prev_right = right;
  }
}

void setVolume(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target(e); // Get the slider that triggered the event
  int slider_value = lv_slider_get_value(slider); // Extract the slider value (e.g., 0–255)
  volume = slider_value;
  clickSound();
}

void saveConfig(lv_event_t *e) {
  pref.begin("config", false);
  pref.putUChar("dim_brightness", dim_brightness);
  pref.putUChar("brightness", brightness);
  pref.putUChar("volume", volume);
  pref.putBool("systemInfo", systemInfo);
  pref.putBool("autoPowerOff", autoPowerOff);
  char buf[16];
  lv_dropdown_get_selected_str(ui_ScreenConfig_Dropdown_threshold, buf, sizeof(buf));
  batt_volt_threshold = atof(buf); // convert "0.3" → 0.3 float
  pref.putFloat("battThres", batt_volt_threshold);
  uint8_t fontOption = lv_dropdown_get_selected(ui_ScreenConfig_Dropdown_fontStyle);
  pref.putUChar("fontOption", fontOption);
  pref.end();
  log("Configuration saved"); // terminal
  beep();
}

void setupTabChanged(lv_event_t *e) {
  static int last_tab_idx = -1; // last_tab_idx value will be persist in this funciton
  uint8_t tab_idx = lv_tabview_get_tab_act(ui_ScreenConfig_Tabview_Config);
  if (tab_idx == last_tab_idx) return; // ignore duplicates
  last_tab_idx = tab_idx;
  // clickSound();
  switch (tab_idx) {
  case 0: // Setting
    stopMusicPlayback();
    clickSound();
    break;
  case 1: // About
    clickSound();
    startMusicPlayback(music, true, GAIN * volume / 100.0, 25); // play track music, loop or not, timeout in sec
    readVINNumber();

    break;
  case 2: // Activate
    stopMusicPlayback();
    clickSound();

    break;
  case 3: // Terminal
    stopMusicPlayback();
    clickSound();
    break;
  default:

    break;
  }
}

void serviceTabChanged(lv_event_t *e) {
  clickSound();
  uint8_t tab_idx = lv_tabview_get_tab_act(ui_ScreenService_Tabview_Service);
  switch (tab_idx) {
  case 0: // Warning

    break;
  case 1: // Diagnostic

    break;
  default:

    break;
  }
}

//---------------------------------------
void deferred_readDTC(lv_timer_t *timer) { // delay function
  lv_timer_del(timer); // Cleanup timer after use
  readDTCs(); // read DTCs
  lv_obj_clear_flag(ui_ScreenService_Panel_dtc, LV_OBJ_FLAG_HIDDEN); // show panel
}

void readDTC(lv_event_t *e) {
  clickSound();
  lv_label_set_text(ui_ScreenService_Label_pleasewait, LV_SYMBOL_DOWNLOAD " Reading... please wait!");
  // ✅ Defer readDTC so UI can update first
  lv_timer_create(deferred_readDTC, 50, e); // 50 ms delay
}

void confirmClearMIL(lv_event_t *e) {
  clickSound();
  clearDTC();
}

void abortClearMIL(lv_event_t *e) {
  clickSound();
  lv_label_set_text(ui_ScreenService_Label_pleasewait, "Touch Read DTCs button to start");
  lv_obj_add_flag(ui_ScreenService_Panel_dtc, LV_OBJ_FLAG_HIDDEN); // hide logo
}

void updateDTC(lv_event_t *e) {
  clickSound();
  updateDTCdescription();
}
// dial gauge sweep animation
void sweepDial(lv_event_t *e) {
  nonBlockDelaySec(1);
  for (uint8_t i = 0; i < 80; i++) {
    lv_arc_set_value(ui_ScreenGauge_Arc_rpm, i);
    lv_timer_handler();
    delay(4);
  }
  for (uint8_t i = 80; i > 0; i--) {
    lv_arc_set_value(ui_ScreenGauge_Arc_rpm, i);
    lv_timer_handler();
    delay(6);
  }
}
//---------------------------------------
void readKeyboard(lv_event_t *e) {
  clickSound();

  lv_obj_t *kb = lv_event_get_target(e);

  // Which button was pressed
  uint16_t btn_id = lv_btnmatrix_get_selected_btn(kb);
  const char *txt = lv_btnmatrix_get_btn_text(kb, btn_id);

  if (txt == NULL) return;

  if (strcmp(txt, LV_SYMBOL_OK) == 0) {
    // ✅ OK pressed → hide keyboard
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
}
//---------------------------------------
// Function to check and activate device
void activate(lv_event_t *e) {

  clickSound();
  const String activateCode = lv_textarea_get_text(ui_ScreenConfig_Textarea_activationCode);
  String expected_code = computeHMAC(secret_key, serialNo);

  // save activateCode
  pref.begin("config", false);
  pref.putString("activateCode", activateCode);
  pref.end();

  if (activateCode.equals(expected_code)) { // correct activation code
    debug("Device is activated");
    lv_obj_set_style_bg_color(ui_ScreenConfig_Label_activateStatus, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ScreenConfig_Label_activateStatus, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_ScreenConfig_Label_activateStatus, LV_SYMBOL_OK " Device is activated");
    lv_textarea_set_text(ui_ScreenConfig_Textarea_activationCode, activateCode.c_str());
    lv_obj_clear_flag(ui_ScreenConfig_Textarea_activationCode, LV_OBJ_FLAG_CLICKABLE); // disable input textbox
    lv_obj_add_flag(ui_ScreenConfig_Button_activate, LV_OBJ_FLAG_HIDDEN); // hide activate button
    //  terminal tab and Diagnostic tab
    lv_obj_add_flag(ui_ScreenService_Button_readDTC, LV_OBJ_FLAG_CLICKABLE); // enable read DTC button
    lv_obj_add_flag(ui_ScreenService_Button_updateDescription, LV_OBJ_FLAG_CLICKABLE); // enable update desc button
    lv_label_set_text(ui_ScreenService_Label_pleasewait, "Touch to start diagnostics (Read DTCs)");
    beepbeep();
    notifySuccess("Device Activation", "The device is now active", "", true); // no buttons

  } else { // wrong activation code

    debug("Activation code mismatch!");
    beep();
    notifyError("Device Activation", "Activation failed.\nDouble-check your code and try again.", "", true); // no buttons
  }
}

//---------------------------------------
// change gauge font
void changeFont(lv_event_t *e) {
  const lv_font_t *font_array[2] = {&ui_font_oswald_36, &ui_font_segment_40}; // font array
  lv_obj_t *dd = lv_event_get_target(e);
  uint8_t fontOption = lv_dropdown_get_selected(dd);
  lv_obj_set_style_text_font(ui_Screen_ScreenGauge, font_array[fontOption], LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Screen_ScreenMonitor, font_array[fontOption], LV_PART_MAIN | LV_STATE_DEFAULT);
}
//---------------------------------------
// Toggles the point count of the second chart between 60 and 30.
uint8_t chart1_point_count = 60;
uint8_t chart2_point_count = 60;
void togglePointChart1(lv_event_t *e) {
  clickSound();
  if (chart1_point_count == 60) {
    chart1_point_count = 30;
    lv_label_set_text(ui_ScreenPerformance_Label_chart1X, "2x");
  } else {
    chart1_point_count = 60;
    lv_label_set_text(ui_ScreenPerformance_Label_chart1X, "1x");
  }
  lv_chart_set_point_count(ui_ScreenPerformance_Chart_top, chart1_point_count);
  lv_chart_refresh(ui_ScreenPerformance_Chart_top);
}

void togglePointChart2(lv_event_t *e) {
  clickSound();
  if (chart2_point_count == 60) {
    chart2_point_count = 30;
    lv_label_set_text(ui_ScreenPerformance_Label_chart2X, "2x");
  } else {
    chart2_point_count = 60;
    lv_label_set_text(ui_ScreenPerformance_Label_chart2X, "1x");
  }
  lv_chart_set_point_count(ui_ScreenPerformance_Chart_bottom, chart2_point_count);
  lv_chart_refresh(ui_ScreenPerformance_Chart_bottom);
}
//------------------------------------
// load Warning Tab
void loadWarning(lv_event_t *e) {

  lv_obj_t *roller = lv_event_get_target(e);
  uint8_t index = lv_roller_get_selected(roller) + 1;
  lv_slider_set_range(ui_ScreenService_Slider_limit, pidMin[index], pidMax[index]);
  lv_slider_set_value(ui_ScreenService_Slider_limit, pidWarn[index], LV_ANIM_ON);
  String min = String(pidMin[index]);
  lv_label_set_text(ui_ScreenService_Label_min, min.c_str());
  String max = String(pidMax[index]);
  lv_label_set_text(ui_ScreenService_Label_max, max.c_str());
  String warn = String(pidWarn[index]);
  lv_label_set_text(ui_ScreenService_Label_warnValue, warn.c_str());
}
//-------------------------------------
// slider change warning limit
void setWarningLimit(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target(e);
  uint8_t index = lv_roller_get_selected(ui_ScreenService_Roller_warnList) + 1;
  pidWarn[index] = lv_slider_get_value(slider);
  String warn = String(pidWarn[index]);
  lv_label_set_text(ui_ScreenService_Label_warnValue, warn.c_str());
}
//-------------------------------------
// to load custom pid (pid_custom.csv) from sc card
void loadCustomPid(lv_event_t *e) {
  updateCustomPID();
}
//-------------------------------------