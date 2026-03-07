#include "elm327.h"
#include "accessory.h"
#include <sys/_stdint.h>

#include "ui.h"
#include "ui_events.h"
#include <LittleFS.h>
#include <WiFi.h>

//---------- Variable --------------------------

#define array_length(x) (sizeof(x) / sizeof(x[0])) // macro to calculate array length

const String compile_date = __DATE__ " - " __TIME__; // get built date and time
const char *version = "1.0.0";

float batt_volt_threshold = 0.3; // battery voltage threshold detect engine start

// page 0 = Gauge,  1 = monitor 2= performance
uint8_t page = 0; // default gauge page
uint8_t startPidIndex[5] = {0, 11, 21, 0, 0};
uint8_t endPidIndex[5] = {11, 21, 26, 0, 0};
int8_t pidIndex = 0;
uint8_t AA, BB = 0; // Hold AA BB C D E

// const uint8_t elm327InitCount = 10;
// const String elm327Init[elm327InitCount] = { "ATZ", "AT E0", "AT L0", "AT H0", "AT SP A0", "AT ST 0A", "AT AT 2", "AT
// SH 7E0", "0100", "AT DP" };  // agressive wait response time from ECU
const uint8_t elm327InitCount = 9;
const String elm327Init[elm327InitCount] = {"ATZ", "AT E0", "AT L0", "AT H0", "AT SP A0", "AT ST 1F", "AT AT 2", "0100", "AT DP"}; // agressive wait response time from ECU

uint8_t initIndex = 0;
uint8_t ABP = 14.7; // absolute baromatic pressure
String elm_message = ""; // elm message buffer
bool initializing = true; // initializing mode or running mode
bool prompt = false; // bt elm ready flag
bool skip = false; // pid skip flag
bool showsystem = true; // show fps and temp EEPROM 0x01
uint8_t pidRead = 0; // to check FPS
uint8_t engine_off_count = 0; // counter to indicate engine is off then sleep

#include "pid_define.h"
const char *custom_pid_file = "/pid_custom.csv";

// String warningValue[8] = { "99", "99", "99", "99", "99", "99", "99", "99" };
const uint8_t maxPid = array_length(pidConfig);
const char *pidLabel[maxPid] = {}; // label
const char *pidUnit[maxPid] = {}; // unit
String pidList[maxPid] = {}; //{"0104","0105","010C","010B","010C","0142","015C"}
int32_t pidMin[maxPid] = {};
int32_t pidMax[maxPid] = {};
uint8_t pidReadSkip[maxPid] = {}; // skip reading 0 - 3; 3 = max delay read
uint8_t pidCurrentSkip[maxPid] = {}; // current skip counter
int32_t pidWarn[maxPid] = {};
float old_data[maxPid] = {}; // keep old data for each gauge to improve speed for pidIndex 0-6

// chart
//  Define pointers to the chart series objects
lv_chart_series_t *series1left;
lv_chart_series_t *series1right;
lv_chart_series_t *series2left;
lv_chart_series_t *series2right;

//---------------------------------------------
// reset data
void resetData() {
  engine_off_count = 0;
  elm_message = "";
  skip = false;
  pidIndex = 0; // will be +1 to be 0
  prompt = true; // to trig reading elm again
  for (int i = 0; i < maxPid; i++) {

    pidLabel[i] = pidConfig[i].label;
    pidUnit[i] = pidConfig[i].unit;
    pidList[i] = pidConfig[i].pid;
    pidMin[i] = pidConfig[i].min;
    pidMax[i] = pidConfig[i].max;
    pidReadSkip[i] = pidConfig[i].skip;
    pidCurrentSkip[i] = pidReadSkip[i]; // skip for skip reading pid
    pidWarn[i] = pidConfig[i].warn;
    old_data[i] = 0; // clear old data
  }
}
//-------------------------------------
// Auto config ELM327 baud rate
#define MAX_RATES 4
uint32_t baud[MAX_RATES] = {115200, 57600, 38400, 19200};
String baudrateCommand[MAX_RATES] = {"AT PP 0C SV 23", "AT PP 0C SV 45", "AT PP 0C SV 68", "AT PP 0C SV D0"};

bool waitForPromptResponse(int timeoutMs = 1000) {
  unsigned long start = millis();
  String response = "";
  while (millis() - start < timeoutMs) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      if (c == '>') { // if response > , correct baud
        return true;
      }
    } // while
  } // while
  return false;
}
//-------------------------------------
// set ELM327 baudrate to target baud (115200)
void setELMbaudrate() {
  lv_textarea_add_text(ui_ScreenConfig_Textarea_Terminal, "Connecting to ELM327 -> ");
  for (int i = 0; i < MAX_RATES; i++) {
    int rate = baud[i];
    Serial.end();
    delay(200);
    Serial.begin(rate);
    delay(200);
    Serial.print("AT\r");
    if (waitForPromptResponse(200)) {
      if (rate != baud[TARGET_BAUD]) {
        String msg = "\nSet baudrate to " + String(baud[TARGET_BAUD]);
        log(msg.c_str());
        // Serial.println("ATPP 0C SV 22");
        Serial.println(baudrateCommand[TARGET_BAUD]);

        delay(100);
        Serial.println("ATPP 0C ON");
        delay(100);
        Serial.println("ATWR");
        delay(100);
        Serial.println("ATZ");
        delay(1000);
        ESP.restart();
      } else {
        String msg = String(baud[TARGET_BAUD]);
        log(msg.c_str());
        return;
      }
    }
    lv_task_handler();
  }
  log("FAIL");
  //----- show notification ELM cannot connect ---------------
  lv_obj_clear_flag(ui_ScreenGauge_Label_info1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_color(ui_ScreenGauge_Label_info1, lv_color_hex(0xFF9D00), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(ui_ScreenGauge_Label_info1, "Unable to connect\nwith the adapter!");
}
//----------------------------------
/*
Parsing elm327  get A B C D E F G H I from response
  * Short frame
  41 0D 33 22 00\r
  62 1E 1C 3A 9B\r
  * long/multi-line frame
  00D\r
  0:41 6D 07 0A 24 0A\r
  1:1D 54 00 00 00 00 00\r
*/
struct PID_DATA {
  uint8_t byte[32];
  bool valid;
};

PID_DATA parseDataFrame(uint8_t pidNo, const String &elm_rsp) {
  PID_DATA result{};
  result.valid = false;

  // 1. NO DATA case
  // if (strcmp(elm_rsp.c_str(), "NO DATA\r\r") == 0) {
  //   return result;
  //  }
  if (strstr(elm_rsp.c_str(), "NO") != NULL) {
    return result;
  }

  // 2. Clean: remove CR, LF, space
  String cleaned = elm_rsp;
  cleaned.replace("\r", "");
  cleaned.replace("\n", "");
  cleaned.replace(" ", "");

  // 3. Remove segment labels like "0:" / "1:"
  for (int i = 0; i < cleaned.length();) {
    if (isHexadecimalDigit(cleaned[i]) && (i + 1 < cleaned.length()) && cleaned[i + 1] == ':') {
      cleaned.remove(i, 2);
    } else {
      i++;
    }
  }

  // 4. Find position of response header
  int pos41 = cleaned.indexOf("41");
  int pos62 = cleaned.indexOf("62");

  if (pos41 == -1 && pos62 == -1) { // not found 41 or 62
    return result; //// invalid data
  }

  const char *hex = cleaned.c_str();

  // 5. Mode 01 (41 xx …)
  if (pos41 != -1) {
    hex += pos41; // start parsing from "41..."
    char mode01pid[5] = {'0', '1', hex[2], hex[3], 0};
    if (strcmp(mode01pid, pidConfig[pidNo].pid) != 0) {
      return result; // invalid data
    }

    int len = (strlen(hex) - 4) / 2;
    for (int i = 0; i < len && i < 32; i++) {
      char buf[3] = {hex[4 + i * 2], hex[5 + i * 2], 0};
      result.byte[i] = strtol(buf, nullptr, 16);
    }
    result.valid = true;
    return result; // valid data
  }

  // 6. Mode 22 (62 xxxx …)
  if (pos62 != -1) {
    hex += pos62; // start parsing from "62..."
    char mode22pid[7] = {'2', '2', hex[2], hex[3], hex[4], hex[5], 0};
    if (strcmp(mode22pid, pidConfig[pidNo].pid) != 0) {
      return result; // invalid data
    }

    int len = (strlen(hex) - 6) / 2;
    for (int i = 0; i < len && i < 32; i++) {
      char buf[3] = {hex[6 + i * 2], hex[7 + i * 2], 0};
      result.byte[i] = strtol(buf, nullptr, 16);
    }
    result.valid = true;
    return result; // valid data
  }

  return result; // invalid data
}

//-------------------------------------
// check engine status if off then turn off gauge
#define LOG_ATRV // log voltage reading for ATRV
void engine_off_detect(float data, uint8_t pid) {
  if (strcmp(pidList[pid].c_str(), "010C") == 0) { // engine speed RPM

    if (data <= 0 || strcmp(elm_message.c_str(), "CAN ERROR\r\r") == 0)
      engine_off_count++;
    else
      engine_off_count = 0; // reset
                            // engine is stopped
    if (engine_off_count > 20) {
      offSound();
#ifdef TEST_ATRV
#else // not turn off screen in debug mode
      for (int i = 255; i > 0; i--) { // fading effect
        tft.setBrightness(i);
        delay(5);
      }
      tft.setBrightness(0);
      tft.fillScreen(TFT_BLACK);
      tft.sleep();
#endif
      float last_volt = 0.0;
      Serial.flush(); // wait for all TX bytes
      Serial.end();
      delay(10);
      setCpuFrequencyMhz(80);
      delay(10);
      Serial.begin(baud[TARGET_BAUD]);

      //--- loging voltage to SD card --------------
#ifdef LOG_ATRV
      File file;
      sd_spi_begin(); // Acquire VSPI
      if (SD.begin(SD_CS, sd_spi)) {
        // --- Open file
        if (SD.exists("/volt_log.txt"))
          file = SD.open("/volt_log.txt", FILE_APPEND);
        else
          file = SD.open("/volt_log.txt", FILE_WRITE);

        if (!file) sd_spi_end();

      } else {
        sd_spi_end(); // Release VSPI
      }
#endif

      // loop during sleeping state
      while (true) {

        delay(5000);
        elm_message = "";
        Serial.print("ATRV\r");

        unsigned long start_millis = millis();
        bool prompt_received = false;

        // Wait for ELM327 response or timeout after 1s
        while (millis() - start_millis < 2000) {
          while (Serial.available() > 0) {
            char c = Serial.read();
            if (c == '>')
              prompt_received = true;
            else
              elm_message += c;
          } // while serial
          if (prompt_received) break; // receive > exit timeout loop
          delay(1);
        } // exit loop if millis - start >1000 and prompt_received = true

        if (prompt_received) {
          elm_message.trim(); // remove \r or \n
          elm_message.replace("V", ""); // remove unit
          float read_volt = elm_message.toFloat(); // safely convert
#ifdef LOG_ATRV
          // log to SD card
          if (file) {
            file.print(read_volt, 1);
            file.print(",");
            file.flush();
          }
#endif
          if (read_volt > 15.0 || read_volt < 10.5) {
            continue; // invalid voltage read
          }
          // check voltage read
          if (last_volt == 0.0) { // read sleep voltage
            last_volt = read_volt;
          } else { // reading voltage after sleep

            // check if voltage spike -> retart the guage
            if (read_volt - last_volt > batt_volt_threshold) {
              float diff = read_volt - last_volt;
#ifdef LOG_ATRV
              // log to SD card
              if (file) {
                file.print("diff=");
                file.print(diff, 1);
                file.print(",batt_volt_thereshold=");
                file.print(batt_volt_threshold, 1);
                file.print(",RESTART\n");
                file.close();
                sd_spi_end();
              }
#endif
              beep();
              ESP.restart();
            } else {
              last_volt = read_volt; // keep tracking current voltage
            }

          } // last_volt == 0.0
        } // if (prompt_received)
      } // sleeping loop
      //-------------------------
    } // if engine_off_count>20
  } // if PID = "010C"
}

//------------------------------------
// Update gauge display
void updateGaugeDisplay(uint8_t pidNo, String response) {
  // update parameter on screen
  // Array data -> { Label , unit, pid, fomula, min, max, ,skip, digit }
  PID_DATA dataSet = parseDataFrame(pidNo, response); // get PID_DATA
  if (!dataSet.valid) return; // error data
  float data = 0.0;
  // A = dataSet.byte[0];
  // I = dataSet.byte[9];
  switch (pidConfig[pidNo].formula) { // calculate by formula
  case 0:
    data = dataSet.byte[0] * 0.145038; // A *1.8/32
    break;
  case 1:
    data = (dataSet.byte[0] - 40); // A-40
    break;
  case 2:
    data = (dataSet.byte[0] * 256 + dataSet.byte[1]) / 4000.0; // RPM 1.5 (from 1500)
    break;
  case 3:
    data = dataSet.byte[0] * 100 / 255;
    break;
  case 4:
    data = dataSet.byte[1] * 100 / 255;
    break;
  case 5:
    data = dataSet.byte[1] - 40;
    break;
  case 6:
    data = (dataSet.byte[1] * 256 + dataSet.byte[2]) * 0.00453125 - ABP;
    break;
  case 7:
    data = (dataSet.byte[3] * 256 + dataSet.byte[4]) * 1.45; //(256*D + E)x10 x  0.145
    break;
  case 8:
    data = dataSet.byte[0];
    break;
  case 9:
    data = (dataSet.byte[0] * 256 + dataSet.byte[1]) / 16;
    break;
  case 10:
    data = dataSet.byte[2] * 100 / 255;
    break;
  case 11:
    data = dataSet.byte[5] * 100 / 255;
    break;
  case 12:
    data = (dataSet.byte[1] * 256 + dataSet.byte[2]) * 1.45;
    break;
  case 13:
    data = ((uint32_t)dataSet.byte[1] << 24) | ((uint32_t)dataSet.byte[2] << 16) | ((uint32_t)dataSet.byte[3] << 8) | ((uint32_t)dataSet.byte[4]);
    data = data / 3600.0f; // convert to hr
    break;
  case 14:
    data = ((uint32_t)dataSet.byte[5] << 24) | ((uint32_t)dataSet.byte[6] << 16) | ((uint32_t)dataSet.byte[7] << 8) | ((uint32_t)dataSet.byte[8]);
    data = data / 3600.0f; // convert to hr
    break;
  case 15:
    data = (dataSet.byte[1] * 256 + dataSet.byte[2]) * 10;
    break;
  case 16:
    data = (dataSet.byte[0] * 256 + dataSet.byte[1]) / 128.0 - 210;
    break;
  case 17:
    data = (dataSet.byte[0] * 256 + dataSet.byte[1]) / 100.0;
    break;
  case 18:
    data = (dataSet.byte[0] * 256 + dataSet.byte[1]) / 400; // RPM 15 from (1500)
    break;
  case 19:
    data = dataSet.byte[0] * 5; // A * 5
    break;
  case 20:
    data = dataSet.byte[1] * 256 + dataSet.byte[2];
    break;
  case 21:
    data = ((dataSet.byte[0] * 256 + dataSet.byte[1]) / 10) - 40;
    break;
  case 22: // runtime after engine start
    data = dataSet.byte[0] * 256 + dataSet.byte[1];
    break;
  case 23:
    data = (dataSet.byte[0] * 256 + dataSet.byte[1]) / 64;
    break;
  case 24: // odometer km
    data = (((uint32_t)dataSet.byte[0] << 24) | ((uint32_t)dataSet.byte[1] << 16) | ((uint32_t)dataSet.byte[2] << 8) | ((uint32_t)dataSet.byte[3])) / 10;
    break;
  case 25: // ford T5 TFT
    data = (dataSet.byte[0] * 256 + dataSet.byte[1]) * 5 / 72 - 18;
    break;
    // more formula

  } // switch fomula

  if (autoPowerOff) // check engine off
    engine_off_detect(data, pidIndex);

  if (data > pidConfig[pidNo].max || data < pidConfig[pidNo].min) // check if data is outbound use old data
    data = old_data[pidNo];

  if (data != old_data[pidNo]) { // skip if data not changed
    old_data[pidNo] = data;
    char buf[10]; // adjust size as needed

    if (pidConfig[pidNo].digit) // show digit or not
      sprintf(buf, "%.1f", data);
    else
      sprintf(buf, "%d", (int)(data + 0.5f)); // for float

    switch (pidNo) { // update live data on the widget
    // Gauge screen
    case 0:
      ABP = data;
      break;
    case 1: // speed
      lv_label_set_text(ui_ScreenGauge_Label_speed, buf);
      break;
    case 2: // RPM data = 0.0 - 8.0
      lv_arc_set_value(ui_ScreenGauge_Arc_rpm, data * 10);
      break;
    case 3: // add a function to translate number into gear position
      if (data == 50) {
        lv_label_set_text(ui_ScreenGauge_Label_gear, "N");
      } else if (data == 60) {
        lv_label_set_text(ui_ScreenGauge_Label_gear, "R");
      } else if (data == 70) {
        lv_label_set_text(ui_ScreenGauge_Label_gear, "P");
      } else if (data > 0 && data < 11) {
        lv_label_set_text(ui_ScreenGauge_Label_gear, buf);
      }
      break;
    case 4:
      lv_label_set_text(ui_ScreenGauge_Label_number1, buf);
      break;
    case 5:
      lv_label_set_text(ui_ScreenGauge_Label_number2, buf);
      break;
    case 6:
      lv_label_set_text(ui_ScreenGauge_Label_number3, buf);
      break;
    case 7:
      lv_label_set_text(ui_ScreenGauge_Label_number4, buf);
      break;
    case 8:
      lv_label_set_text(ui_ScreenGauge_Label_number5, buf);
      break;
    case 9:
      lv_label_set_text(ui_ScreenGauge_Label_number6, buf);
      break;
    case 10:
      lv_label_set_text(ui_ScreenGauge_Label_number7, buf);
      break;

      // for monitor screen
    case 11: // RPM data = 0.0 - 8.0
      // lv_arc_set_value(ui_ScreenGauge_Arc_rpm, data * 10);
      break;
    case 12:
      lv_label_set_text(ui_ScreenMonitor_Label_tileNumber1, buf);
      break;
    case 13:
      lv_label_set_text(ui_ScreenMonitor_Label_tileNumber2, buf);
      break;
    case 14: { // convert sec to hh:mm
      char timeBuffer[9];
      uint32_t totalSeconds = (uint32_t)data;
      uint16_t hours = totalSeconds / 3600;
      uint16_t minutes = (totalSeconds % 3600) / 60;
      //  uint16_t seconds = (totalSeconds % 3600) % 60;
      // snprintf(timeBuffer, sizeof(timeBuffer), "%02u:%02u:%02u",(unsigned)hours, (unsigned)minutes,
      // (unsigned)seconds);
      snprintf(timeBuffer, sizeof(timeBuffer), "%01d:%02d", hours, minutes); // 0:00
      lv_label_set_text(ui_ScreenMonitor_Label_tileNumber3, timeBuffer);
      break;
    }
    case 15:
      lv_label_set_text(ui_ScreenMonitor_Label_tileNumber4, buf);
      break;
    case 16:
      lv_label_set_text(ui_ScreenMonitor_Label_tileNumber5, buf);
      break;
    case 17:
      lv_label_set_text(ui_ScreenMonitor_Label_tileNumber6, buf);
      break;
    case 18:
      lv_label_set_text(ui_ScreenMonitor_Label_tileNumber7, buf);
      break;
    case 19:
      lv_label_set_text(ui_ScreenMonitor_Label_tileNumber8, buf);
      break;
    case 20:
      lv_label_set_text(ui_ScreenMonitor_Label_tileNumber9, buf);
      break;

      // Chart screen
    case 21: // RPM data = 0.0 - 8.0
             // lv_arc_set_value(ui_ScreenGauge_Arc_rpm, data * 10);
      break;
    case 22: // Boost
      lv_chart_set_next_value(ui_ScreenPerformance_Chart_top, series1left, data);
      break;
    case 23: // MAF
      lv_chart_set_next_value(ui_ScreenPerformance_Chart_top, series1right, data);
      break;
    case 24: // FRP
      lv_chart_set_next_value(ui_ScreenPerformance_Chart_bottom, series2left, data);
      break;
    case 25: // EGT
      lv_chart_set_next_value(ui_ScreenPerformance_Chart_bottom, series2right, data);
      break;

    } // switch
  } // if data
} // updatemeter
//----------------------------------------------
// Copy dtc.txt from sd card to littleFS
void updateDTCdescription() {
  sd_spi_begin(); // Acquire VSPI for SD card
  if (!SD.begin(SD_CS, sd_spi)) {
    beep();
    lv_label_set_text(ui_ScreenService_Label_pleasewait, LV_SYMBOL_SD_CARD " SD card not mounted");
    sd_spi_end(); // Release VSPI
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    beep();
    lv_label_set_text(ui_ScreenService_Label_pleasewait, LV_SYMBOL_WARNING " SD card error");
    SD.end();
    sd_spi_end(); // Release VSPI
    return;
  }

  // Open source file on SD
  File sdFile = SD.open("/dtc.txt", FILE_READ);
  if (!sdFile) {
    beep();
    lv_label_set_text(ui_ScreenService_Label_pleasewait, LV_SYMBOL_FILE " 'dtc.txt' not found on SD card");
    SD.end();
    sd_spi_end(); // Release VSPI
    return;
  }

  // Open destination file on LittleFS
  File lfsFile = LittleFS.open("/dtc.txt", FILE_WRITE);
  if (!lfsFile) {
    beep();
    lv_label_set_text(ui_ScreenService_Label_pleasewait, LV_SYMBOL_CLOSE " Unable to write to internal storage");
    sdFile.close();
    SD.end();
    sd_spi_end(); // Release VSPI
    return;
  }

  lv_label_set_text(ui_ScreenService_Label_pleasewait, LV_SYMBOL_COPY " Copying file, please wait...");

  // Copy content
  size_t total = 0;
  while (sdFile.available()) {
    char buf[64];
    size_t len = sdFile.readBytes(buf, sizeof(buf));
    lfsFile.write((const uint8_t *)buf, len);
    total += len;
  }
  sdFile.close();
  lfsFile.close();
  SD.end();
  sd_spi_end(); // Release VSPI
  lv_label_set_text(ui_ScreenService_Label_pleasewait, LV_SYMBOL_OK " File copied successfully");
  beepbeep();
}

//------------------------------------------------------

// Helper function for DTC reading
String getPID(String pid) { // fucntion getpid response from elm327
  debug(pid);
  while (Serial.available() > 0) { // clear rx buffer
    Serial.read();
  }
  String elm_response = "";
  Serial.print(pid + "\r");

  unsigned long startTime = millis();
  while (millis() - startTime < 1000) { // 1 second timeout
    if (Serial.available() > 0) { // reading Bluetooth
      char incomingChar = Serial.read();
      if (incomingChar == '>') {
        break; // End of response
      }
      elm_response += incomingChar;
    }
  }
  elm_response.trim();
  debug(elm_response);
  return elm_response;
}
//-----------------------------

// get AA BB from response return in global AA BB variable
void getAB2(String elm_rsp, String mode, String para) { // 41 05 2A 3C 01>
  elm_rsp.trim();
  String hexcode[16];
  uint8_t StringCount = 0;
  while (elm_rsp.length() > 0) { // keep reading each char
    int index = elm_rsp.indexOf(' '); // check space
    if (index == -1) // No space found
    { // only data without space is now in hexcode {41,05,aa,bb}
      hexcode[StringCount++] = elm_rsp; // last byte
      // for (index=0;index<StringCount;index++) Serial.println(hexcode[index]);
      if ((hexcode[0] == mode) && (hexcode[1] == para)) { // check correct response
        AA = strtol(hexcode[2].c_str(), NULL, 16); // byte 3 save to AA
        BB = strtol(hexcode[3].c_str(), NULL, 16); // byte 4 save to BB
      } else
        AA = 0xFF; // error
      break;
    } else { // found space
      hexcode[StringCount++] = elm_rsp.substring(0, index); // copy strings from 0 to space to hexcode
      elm_rsp = elm_rsp.substring(index + 1); // copy the rest behind space to elm_rsp
    }
  } // while
} // getAB
//------------------------------

// Read DTC descriptoin from dtc.txt
String getDTCDescription(const String &code) {
  File file = LittleFS.open("/dtc.txt", "r");
  if (!file) {
    debug("DTCs description file not found!");
    return "";
  }
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() >= 6 && line.startsWith(code)) {
      int tabIndex = line.indexOf('\t');
      if (tabIndex > 0) {
        String description = line.substring(tabIndex + 1);
        file.close();
        return " - " + description;
      }
    }
  }
  file.close();
  return " - Description not found";
}
//--------------------------------------

// Read diagnostic trouble code
void readDTCs() { // https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf

  // DTC prefex code mapping
  const String dtcMap[16] = {"P0", "P1", "P2", "P3", "C0", "C1", "C2", "C3", "B0", "B1", "B2", "B3", "U0", "U1", "U2", "U3"};

  // clear screen
  lv_obj_set_style_text_color(ui_ScreenService_Label_MILStatus, lv_color_hex(0xFFFF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_textarea_set_text(ui_ScreenService_Textarea_DTCList, ""); // Clear textarea and reset cursor
  lv_textarea_set_cursor_pos(ui_ScreenService_Textarea_DTCList, 0);

  getPID("ATE0"); // force echo off
  // #1 get number of DTC & MIL status
  // getAB2("41 01 86 0E 88 88 41 01 81 0C 00 00 ","41","01");//(AA = 80+1,80h=MIL ON,no of dtc = AA-80h)
  uint8_t error_cnt = 0;
  debug("MIL Status 0101->");
  AA = 0xFF; // supoose error read AA
  while ((AA == 0xFF) && (error_cnt < 3)) { // if AA == 0xFF repeat reading again (AA must be 0x80+) try 3 times

    // lv_textarea_add_text(ui_ScreenService_Textarea_DTCList, "MIL Status 0101->\n");
    String res = getPID("0101");
    getAB2(res, "41", "01"); // try read MIL status and save in to AA as number of DTCs
    error_cnt++;
    delay(100);
  }
  // String res = "No of DTCs = " + String(AA-128);
  log(String(AA).c_str()); // show real no. of dtc in terminal

#ifdef TEST_DTC
  AA = 0x86;
#endif

  if (AA == 0xFF) { // Error reading MIL status
    lv_obj_set_style_text_color(ui_ScreenService_Label_MILStatus, lv_color_hex(0xFFFF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_ScreenService_Label_MILStatus, LV_SYMBOL_CLOSE " Error reading MIL status!");
    beep();

  } else {
    if (AA == 0x00 || AA <= 0x80) { // 41 01 00 = Mil is OFF No DTC
      lv_obj_set_style_text_color(ui_ScreenService_Label_MILStatus, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_label_set_text(ui_ScreenService_Label_MILStatus, LV_SYMBOL_OK " MIL is OFF - No DTC");
      clickSound();
      // hide CLEAR BUTTON
      lv_obj_add_flag(ui_ScreenService_Button_clearMIL, LV_OBJ_FLAG_HIDDEN); // hide clear MIL button

    } else if (AA >= 0x80) {
      /*  #2 get Raw Diagnostic Troulbe Code from ELM327
      for 1 dtc)43 06 00 7D
      (more than 1 dtc) 00E 0: 43 06 00 7D C6 93 1: 01 08 C6 0F 02 E9 02 e: E0 CC CC CC CC CC CC 43 01 C4 01
        */
      uint8_t error_cnt = 0;
      String dtcRead = "";

      debug("Read DTC 03 ->");
      // Get DTCs
      while ((dtcRead.substring(0, 2) != "43") && (dtcRead.substring(0, 2) != "00") && (error_cnt < 5)) { // check first header 43 or 00
        dtcRead = getPID("03"); // try read DTC code

#ifdef TEST_DTC
        dtcRead = "00E\n0: 43 06 00 7D C6 93\n1: 01 08 C6 0F 02 E9 02\n2: E0 CC CC CC CC CC CC\n43 01 C4 01";

#endif
        error_cnt++;
        delay(100);
      }
      log(dtcRead.c_str());

      if (error_cnt >= 10) { // exist by error counter
        lv_obj_set_style_text_color(ui_ScreenService_Label_MILStatus, lv_color_hex(0xFFFF00), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_ScreenService_Label_MILStatus, LV_SYMBOL_CLOSE " Error reading DTCs!");
      } else {
        // sanitize
        dtcRead.trim();
        dtcRead.replace("\r", " ");
        dtcRead.replace("\n", " ");
        dtcRead.replace(":", "");

        String tokens[128];
        int tokenCount = 0;

        while (dtcRead.length() > 0 && tokenCount < 128) {
          int index = dtcRead.indexOf(' ');
          if (index == -1) index = dtcRead.length();
          String token = dtcRead.substring(0, index);
          dtcRead = dtcRead.substring(index + 1);
          token.trim();

          // Skip labels like "0", "1", "2"
          if (token.length() == 1 && isDigit(token[0])) continue;

          if (token.length() > 0) {
            tokens[tokenCount++] = token;
          }
        } // while dtcRead

        // Parse DTCs
        String dtcList[32];
        byte no_of_dtc = 0;

        for (int i = 0; i < tokenCount - 2; i++) {
          if (tokens[i] == "43") {
            int numDTC = strtol(tokens[i + 1].c_str(), NULL, 16);
            i += 2;

            for (int j = 0; j < numDTC && i + 1 < tokenCount; j++) {
              uint8_t b1 = strtol(tokens[i++].c_str(), NULL, 16);
              uint8_t b2 = strtol(tokens[i++].c_str(), NULL, 16);

              uint8_t firstNibble = (b1 & 0xF0) >> 4;
              uint8_t secondNibble = (b1 & 0x0F);

              String dtcCode = dtcMap[firstNibble];
              dtcCode += String(secondNibble, HEX);
              if (secondNibble < 0x10 && secondNibble < 0xA) dtcCode += "";

              if (b2 < 0x10) dtcCode += "0";
              dtcCode += String(b2, HEX);

              dtcCode.toUpperCase();
              dtcList[no_of_dtc++] = dtcCode; // all DTC saved in dtcList
              String listcode = dtcCode + getDTCDescription(dtcCode) + "\n";
              log(listcode.c_str()); // terminal
              lv_textarea_add_text(ui_ScreenService_Textarea_DTCList, listcode.c_str());
            }
          }
        } // for i
          // #4  List all DTC on screen
        lv_obj_set_style_text_color(ui_ScreenService_Label_MILStatus, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        String txt = LV_SYMBOL_WARNING " MIL is ON - No of DTCs = " + String(no_of_dtc);
        lv_label_set_text(ui_ScreenService_Label_MILStatus, txt.c_str());

        // #5 show clear MIL button
        lv_obj_clear_flag(ui_ScreenService_Button_clearMIL, LV_OBJ_FLAG_HIDDEN); // show clear MIL button
        beep();

      } // else !error_read//
    } // else if AA >0x80
  } // else if error_ent>5
}

/*------------------*/
/* VIN reading
  getPID("ATE0");//force echo off
  txt = "VIN :  " + parseVIN(getPID("0902"));
String resp = "014
0: 49 02 01 4D 50 42
1: 41 4D 46 30 36 30 4E
2: 58 34 33 37 30 39 33 "; */
// convert ascii code to Charactor
const char asciiTable[0x60] = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}`";
char HextoChar(uint8_t asciiCode) {
  if ((asciiCode > 0x20) && (asciiCode < 0x80)) {
    return (asciiTable[asciiCode - 32]);
  } else
    return '\0'; // null-terminator
}
//----------------------------
// function get VIN   Serial.println(parseVIN(getPID(0902)));
String parseVIN(String elm_rsp) {
  String VIN = "";
  uint8_t byteCount = 0;
  elm_rsp.trim();
  while (elm_rsp.length() > 0) { // keep reading each char
    int index = elm_rsp.indexOf(' '); // check space
    String getByte = elm_rsp.substring(0, index); // get first byte
    if (index == -1) { // no space found
      byte ascii = strtol(getByte.c_str(), NULL, 16); // read ascii
      VIN.concat(HextoChar(ascii)); // last char
      // check correct VIN
      if (VIN.length() == byteCount) {
        return VIN.substring(3); // remove first 3 string
      } else {
        return "Unable to read VIN";
      }

    } else { // found space
      if (getByte.indexOf(':') == -1) {
        if (getByte.length() >= 3) {
          byteCount = strtol(getByte.c_str(), NULL, 16); // get byte count
        } else { // skip ':' byte
          byte ascii = strtol(getByte.c_str(), NULL, 16); // read ascii
          VIN.concat(HextoChar(ascii)); // convert to hex charactor
        }
      }
      elm_rsp = elm_rsp.substring(index + 1); // copy the rest behind space to elm_rsp.
    } // else
  } // while
  return "Unable to read VIN";
} // parseVIN
//----------------------------------
// clear DTC event
void clearDTC() {
  Serial.print("04\r"); // clear MIL 04->7F 04 78  7F 04 78
  debug("Clear DTC -> 04");
  delay(500); // wait respond

  String response = "";
  while (Serial.available() > 0) {
    char incomingChar = Serial.read();
    if (incomingChar == '>') { // check prompt
      debug(response);
      lv_textarea_set_text(ui_ScreenService_Textarea_DTCList, "Successfully Cleared");
      readDTCs();

    } else {
      response.concat(incomingChar); // keep elm327 respond
    }
  }
}
//----------------------------
// About page display VIN , mac address, build
void readVINNumber() {
  debug("Read VIN");
  const String vin = parseVIN(getPID("0902")); // read VIN
  const String serial = removeColons(WiFi.macAddress());
  const String ver = version;
  const String aboutText = "VIN        : " + vin + "\nBUILD   : " + ver + " - " + compile_date + "\nSERIAL  : " + serial + "\nSTATUS :";
  lv_label_set_text(ui_ScreenConfig_Label_aboutText, aboutText.c_str());
}
//------------------------------------
// helper function to delay without blocking
void nonBlockDelaySec(const unsigned timer) {
  const unsigned long counterTimer = millis();
  while (millis() - counterTimer < timer * 1000) {
    lv_timer_handler();
    delay(5);
  }
}
//------------------------------------
// add chart series to chart widget
void live_chart_init() {
  // Boost series with a primary Y axis
  series1left = lv_chart_add_series(ui_ScreenPerformance_Chart_top, lv_color_hex(0xF000F0), LV_CHART_AXIS_PRIMARY_Y);
  // MAF series with a secondary Y axis
  series1right = lv_chart_add_series(ui_ScreenPerformance_Chart_top, lv_color_hex(0xF0F000), LV_CHART_AXIS_SECONDARY_Y);
  // FRP series with a primary Y axis
  series2left = lv_chart_add_series(ui_ScreenPerformance_Chart_bottom, lv_color_hex(0x00F0F0), LV_CHART_AXIS_PRIMARY_Y);
  // EGT series with a secondary Y axis
  series2right = lv_chart_add_series(ui_ScreenPerformance_Chart_bottom, lv_color_hex(0xFEA139), LV_CHART_AXIS_SECONDARY_Y);
}
//------------------------------------------------------
