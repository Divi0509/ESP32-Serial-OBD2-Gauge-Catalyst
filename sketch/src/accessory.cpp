// Helper functions

#include "accessory.h"
#include "elm327.h"
#include "ui/ui.h"
#include <LittleFS.h>
#include <Preferences.h>
Preferences pref;
#include "esp32-hal.h"
#include "mbedtls/md.h" //for encoding activation with secret-key
#include <WiFi.h>

//------------------------------------
// log & debug information in terminal textarea with line limit
#define MAX_LOG_LINES 30// maximum line in log textarea

void log(String text) {
  lv_obj_t *terminal = ui_ScreenConfig_Textarea_Terminal;
  const char *current_text = lv_textarea_get_text(terminal);
  // Count the number of lines
  int line_count = 0;
  for (const char *p = current_text; *p; p++) {
    if (*p == '\n') {
      line_count++;
    }
  }

  // If the line count is at or over the limit, remove the first line
  if (line_count >= MAX_LOG_LINES) {
    const char *first_newline = strchr(current_text, '\n');
    if (first_newline != NULL) {
      // Set the text to start from the character after the first newline
      lv_textarea_set_text(terminal, first_newline + 1);
    }
  }
  String log = text + "\n";
  lv_textarea_add_text(ui_ScreenConfig_Textarea_Terminal, log.c_str());
}

// log information in textarea
void debug(String text) {
  #ifdef DEBUG
  log(text);
  #endif
}
//----------  Security  -------------
const String secret_key = "Java&Jacob"; // a secret key for encryption
String serialNo = "";
bool systemInfo = false;
bool autoPowerOff = true;
bool activated = false;

// a function to generate activation code from key & serialNo
String computeHMAC(const String &secret, const String &serial) {
  unsigned char out[32];
  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(md_info, (const unsigned char *)secret.c_str(), secret.length(), (const unsigned char *)serial.c_str(), serial.length(), out);
  // take first 8 bytes (64 bits) -> 16 hex chars
  String code;
  for (int i = 0; i < 8; i++) {
    char buf[3];
    sprintf(buf, "%02X", out[i]);
    code += buf;
  }
  return code; // 16 hex chars (64-bit)
}

//------------ SOUND EFFECT ----------
#define SPEAKER_PIN 26
#define SPEAKER_CH 2
uint8_t volume = 80;

void initSpeaker() {
  int freq = 2000; // Hz
  int resolution = 10; // bits (0–1023)
  ledcSetup(SPEAKER_CH, freq, resolution);
  ledcAttachPin(SPEAKER_PIN, SPEAKER_CH);
}

// playToneWithVolume(2000, 20, 200);  // 20% volume, 2000 Hz, 200ms
void playToneWithVolume(int freq, int volumePercent, int duration) {
  // clamp volume between 0–100
  volumePercent = constrain(volumePercent, 0, 100);
  // convert percent to duty cycle
  int maxDuty = (1 << 10) - 1; // for 10-bit = 1023
  int duty = (maxDuty * volumePercent) / 100;
  ledcWriteTone(SPEAKER_CH, freq); // set frequency
  ledcWrite(SPEAKER_CH, duty); // apply duty cycle (volume)
  delay(duration);
  ledcWrite(SPEAKER_CH, 0); // stop
}

void beepbeep() { // 2 shot beep
  initSpeaker();
  playToneWithVolume(2000, volume, 100);
  delay(50);
  playToneWithVolume(2000, volume, 100);
}
void beep() {
  initSpeaker();
  playToneWithVolume(2000, volume, 500);
}
void clickSound() {
  initSpeaker();
  playToneWithVolume(5000, volume, 5);
}
void offSound() {
  initSpeaker();
  playToneWithVolume(3136, volume, 100);
  playToneWithVolume(2637, volume, 100);
  playToneWithVolume(2093, volume, 100);
}

//-----------  auto dim backlight function ------------
#define LDR_PIN 34 // LDR sensor

uint8_t dim_brightness = 10;
uint8_t brightness = 200;

static uint8_t low_count = 0;
static uint8_t high_count = 0;
static bool dim = false; // back light dim flag
static const uint16_t LIGHT_LEVEL = 300; // backlight control by light level Higher = darker light
long dimTimer = 0;

void autoDim() {
  if (millis() - dimTimer > 100) {
    int light = analogRead(LDR_PIN); // read light
    // Serial.printf("LDR: %d\n",light);
    if (light > LIGHT_LEVEL) { // low light
      low_count++;
      if (low_count > 20) {
        low_count = 20; // low amb light 10 times count
        if (!dim) {
          tft.setBrightness(dim_brightness); // dim light
          dim = true;
        } // if !dim
      } // if low_count
      high_count = 0;
    } else {
      high_count++;
      if (high_count > 20) {
        high_count = 20; // high amb light 10 times count
        if (dim) {
          tft.setBrightness(brightness);
          dim = false;
        } // if dim
      } // if high_count
      low_count = 0;
    } // if light > lightlevel
    dimTimer = millis();
  }
} // autodim
//-------------------------------
// ESP32 overtemp protection
float cpuTemp_c = 0, cpuTemp_f = 0;
uint8_t max_cpuTemp_c = 70;
uint8_t max_cpuTemp_f = 158;
long cpuTempTimer = 0;
bool warned = false;
extern "C" uint8_t temprature_sens_read();

void checkCPUTemp() {
  if (millis() - cpuTempTimer > 5000) {
    // Convert raw sensor value to Celsius
    cpuTemp_f = temprature_sens_read();
    cpuTemp_c = (cpuTemp_f - 32) / 1.8;
    cpuTempTimer = millis();
    if (cpuTemp_c >= max_cpuTemp_c || cpuTemp_f >= max_cpuTemp_f) { // over temperature
      if (!warned) {
        warned = true;
        systemInfo = false; // force hide info label
        lv_obj_clear_flag(ui_ScreenGauge_Label_info1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(ui_ScreenGauge_Label_info1, lv_color_hex(0xD32F2F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_ScreenGauge_Label_info1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_ScreenGauge_Label_info1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(ui_ScreenMonitor_Label_info2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(ui_ScreenMonitor_Label_info2, lv_color_hex(0xD32F2F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_ScreenMonitor_Label_info2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_ScreenMonitor_Label_info2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(ui_ScreenPerformance_Label_info3, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(ui_ScreenPerformance_Label_info3, lv_color_hex(0xD32F2F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_ScreenPerformance_Label_info3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_ScreenPerformance_Label_info3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        String warn = String(LV_SYMBOL_WARNING) + " WARNING!\nCPU overheat " + String(cpuTemp_c, 0) + " °C";
        lv_label_set_text(ui_ScreenGauge_Label_info1, warn.c_str());
        lv_label_set_text(ui_ScreenMonitor_Label_info2, warn.c_str());
        lv_label_set_text(ui_ScreenPerformance_Label_info3, warn.c_str());
        beep();
      }
    } // over temp
  } // 5 sec passed
} // check cpu temp

//======================== MPU6050 =================================
bool print_started = false;
bool print_swing_now = false;
static const float maxAcc = 4.0; // Max acceleration in g (±8g)
static const int maxAngle = 350;
static float devider = 8192.0; // for 4g accelometer
bool mpu_install = false;
#include <MPU6050.h>
#include <Wire.h>
MPU6050 mpu;
//-------------------------------
void initMPU(byte sda, byte scl) {
  Wire.begin(sda, scl);
  Wire.setClock(400000); // Set I2C clock speed to 100kHz
  mpu.initialize();
  delay(100); // Let it stabilize
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_4);
  uint8_t id = mpu.getDeviceID();
  if (id != 0) {
    log("Connecting to Gyro Sensor -> OK");
    mpu_install = true;
  } else {
    log("Connecting to Gyro Sensor -> FAIL");
    mpu_install = false;
  }
  lv_timer_handler();
}
//---------------------------------
// read acceleration
int16_t mpu_read() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  // float acx = ax / devider;
  float acy = ay / devider;
  // float acz = az / devider;
  return acy * -100;
}
//---------------------------------
// load custom pidconfig from csv file in littleFS to pidConfig array
bool loadPIDConfigFromCSV(const char *filename) {
  char error[128];
  snprintf(error, sizeof(error), LV_SYMBOL_FILE " File %s is corrupted", custom_pid_file);
  if (!LittleFS.exists(filename)) {
    // Serial.printf("❌ File not found: %s\n", filename);
    return false;
  }

  File file = LittleFS.open(filename, "r");
  if (!file || !file.available()) {
    log(error);
    // Serial.printf("❌ Failed to open or empty file: %s\n", filename);
    return false;
  }

  String line;
  bool firstLine = true;
  size_t pidCount = 0;
  size_t lineNum = 0;

  while (file.available()) {
    line = file.readStringUntil('\n');
    line.trim();
    lineNum++;
    if (line.length() == 0) continue;

    // Skip header
    if (firstLine) {
      firstLine = false;
      continue;
    }

    if (pidCount >= maxPid) break;

    String parts[9];
    int idx = 0, start = 0;
    for (int i = 0; i < line.length(); i++) {
      if (line[i] == ',' || i == line.length() - 1) {
        if (i == line.length() - 1) i++;
        parts[idx++] = line.substring(start, i);
        start = i + 1;
        if (idx >= 9) break;
      }
    }

    if (idx < 9) {
      log(error);
      //  Serial.printf("⚠️ [Line %d] Skipped: insufficient columns (%d found)\n", lineNum, idx);
      continue;
    }

    // Trim all fields
    for (int i = 0; i < 9; i++) parts[i].trim();

    // Validate numeric fields
    bool valid = true;

    auto isInteger = [](const String &s) {
      if (s.length() == 0) return false;
      for (uint8_t i = 0; i < s.length(); i++) {
        if (!isDigit(s[i]) && !(i == 0 && s[i] == '-')) return false;
      }
      return true;
    };

    if (!isInteger(parts[3]) || !isInteger(parts[4]) || !isInteger(parts[5]) || !isInteger(parts[6]) || !isInteger(parts[8])) {
      log(error);
      // Serial.printf("⚠️ [Line %d] Invalid numeric field(s)\n", lineNum);
      valid = false;
    }

    // Validate PID (must be hex string)
    for (uint8_t i = 0; i < parts[2].length(); i++) {
      char c = toupper(parts[2][i]);
      if (!isxdigit(c)) {
        log(error);
        //   Serial.printf("⚠️ [Line %d] Invalid PID: %s\n", lineNum, parts[2].c_str());
        valid = false;
        break;
      }
    }

    if (!valid) continue; // skip invalid line

    // Parse boolean safely
    String d = parts[7];
    d.toUpperCase();
    bool digitValue = (d == "TRUE" || d == "1" || d == "YES");

    // Assign values
    pidConfig[pidCount].label = strdup(parts[0].c_str());
    pidConfig[pidCount].unit = strdup(parts[1].c_str());
    pidConfig[pidCount].pid = strdup(parts[2].c_str());
    pidConfig[pidCount].formula = parts[3].toInt();
    pidConfig[pidCount].min = parts[4].toInt();
    pidConfig[pidCount].max = parts[5].toInt();
    pidConfig[pidCount].skip = parts[6].toInt();
    pidConfig[pidCount].digit = digitValue;
    pidConfig[pidCount].warn = parts[8].toInt();

    pidCount++;
  }
  file.close();
  // Serial.printf("✅ Successfully loaded %u PIDs from %s\n", pidCount, filename);
  return pidCount > 0;
}

//---------------------------------
// init LittleFS
void initFileSystem() {
  if (!LittleFS.begin()) {
    log("Checking file system\nNo filesystem, try formatting filesystem.");
    LittleFS.format();
    delay(100); // Optional delay after formatting
    if (!LittleFS.begin()) {
      log("Checking file system\nFailed to mount filesystem after formatting.");
    }
  } else {
    log("Checking file system -> OK");

    // loadPIDConfigFromCSV if not found skip (use default)
    if (loadPIDConfigFromCSV(custom_pid_file)) {
      log("Load PIDs config -> CUSTOM");
    } else {
      log("Load PIDs config -> DEFAULT");
    }
  }
}
// Init SD CARD
SPIClass sd_spi(VSPI); // Use VSPI for the SD card
// Switch VSPI bus to be used by the SD card
void sd_spi_begin() {
  // The touch controller also uses VSPI, so we need to re-init the bus
  // with the SD card's pins.
  sd_spi.begin(SD_SCK, SD_MISO, SD_MOSI, -1); // Initialize without CS
}

// Release the VSPI bus from the SD card and restore it for the touch controller
void sd_spi_end() {
  sd_spi.end(); // De-initialize the SPI bus for the SD card.
  // Re-initialize only the touch controller's bus (VSPI).
  // This restores the SPI configuration for the touch screen without
  // resetting the main display panel on HSPI.
  tft.touch()->init();
  lv_timer_handler(); // It's good practice to call this to handle any pending
                      // UI tasks.
}

//---------------------------------
// "AA:BB:CC:DD:EE:FF" and returns "AABBCCDDEEFF"
String removeColons(String mac) {
  String result = "";
  for (int i = 0; i < mac.length(); i++) {
    if (mac.charAt(i) != ':') {
      result += mac.charAt(i);
    }
  }
  return result;
}

//---------------------------------
// load pidConfig to display on each widget
void loadDisplay() {
  // Gauge screen 1-10
  lv_label_set_text(ui_ScreenGauge_Label_kmh, pidConfig[1].unit);// speed
  lv_label_set_text(ui_ScreenGauge_Label_pid1, pidConfig[4].label);//top left
  lv_label_set_text(ui_ScreenGauge_Label_unit1, pidConfig[4].unit);
  lv_label_set_text(ui_ScreenGauge_Label_pid2, pidConfig[5].label);//middle left
  lv_label_set_text(ui_ScreenGauge_Label_unit2, pidConfig[5].unit);
  lv_label_set_text(ui_ScreenGauge_Label_pid3, pidConfig[6].label);//bottom left
  lv_label_set_text(ui_ScreenGauge_Label_unit3, pidConfig[6].unit);
  lv_label_set_text(ui_ScreenGauge_Label_pid4, pidConfig[7].label);//top right
  lv_label_set_text(ui_ScreenGauge_Label_unit4, pidConfig[7].unit);
  lv_label_set_text(ui_ScreenGauge_Label_pid5, pidConfig[8].label);//middle right
  lv_label_set_text(ui_ScreenGauge_Label_unit5, pidConfig[8].unit);
  lv_label_set_text(ui_ScreenGauge_Label_pid6, pidConfig[9].label);//bottom right
  lv_label_set_text(ui_ScreenGauge_Label_unit6, pidConfig[9].unit);
  lv_label_set_text(ui_ScreenGauge_Label_pid7, pidConfig[10].label);//middle bottom
  lv_label_set_text(ui_ScreenGauge_Label_unit7, pidConfig[10].unit);
  // Monitor screen 12-20
  lv_label_set_text(ui_ScreenMonitor_Label_tilePid1, pidConfig[12].label);//top left
  lv_label_set_text(ui_ScreenMonitor_Label_tileUnit1, pidConfig[12].unit);
  lv_label_set_text(ui_ScreenMonitor_Label_tilePid2, pidConfig[13].label);//top middle
  lv_label_set_text(ui_ScreenMonitor_Label_tileUnit2, pidConfig[13].unit);
  lv_label_set_text(ui_ScreenMonitor_Label_tilePid3, pidConfig[14].label);//top right
  lv_label_set_text(ui_ScreenMonitor_Label_tileUnit3, pidConfig[14].unit);
  lv_label_set_text(ui_ScreenMonitor_Label_tilePid4, pidConfig[15].label);//middle left
  lv_label_set_text(ui_ScreenMonitor_Label_tileUnit4, pidConfig[15].unit);
  lv_label_set_text(ui_ScreenMonitor_Label_tilePid5, pidConfig[16].label);//center
  lv_label_set_text(ui_ScreenMonitor_Label_tileUnit5, pidConfig[16].unit);
  lv_label_set_text(ui_ScreenMonitor_Label_tilePid6, pidConfig[17].label);//middle right
  lv_label_set_text(ui_ScreenMonitor_Label_tileUnit6, pidConfig[17].unit);
  lv_label_set_text(ui_ScreenMonitor_Label_tilePid7, pidConfig[18].label);//bottom left
  lv_label_set_text(ui_ScreenMonitor_Label_tileUnit7, pidConfig[18].unit);
  lv_label_set_text(ui_ScreenMonitor_Label_tilePid8, pidConfig[19].label);//bottom middle
  lv_label_set_text(ui_ScreenMonitor_Label_tileUnit8, pidConfig[19].unit);
  lv_label_set_text(ui_ScreenMonitor_Label_tilePid9, pidConfig[20].label);//bottom right
  lv_label_set_text(ui_ScreenMonitor_Label_tileUnit9, pidConfig[20].unit);
  // Chart screen 22-25
  char chart_label_buffer[32]; // Buffer for chart labels
  snprintf(chart_label_buffer, sizeof(chart_label_buffer), "%s %s", pidConfig[22].label, pidConfig[22].unit);//top chart left
  lv_label_set_text(ui_ScreenPerformance_Label_series1left, chart_label_buffer);
  snprintf(chart_label_buffer, sizeof(chart_label_buffer), "%s %s", pidConfig[23].label, pidConfig[23].unit);//top chart right
  lv_label_set_text(ui_ScreenPerformance_Label_series1right, chart_label_buffer);
  snprintf(chart_label_buffer, sizeof(chart_label_buffer), "%s %s", pidConfig[24].label, pidConfig[24].unit);//bottom chart left
  lv_label_set_text(ui_ScreenPerformance_Label_series2left, chart_label_buffer);
  snprintf(chart_label_buffer, sizeof(chart_label_buffer), "%s %s", pidConfig[25].label, pidConfig[25].unit);//bottom chart right
  lv_label_set_text(ui_ScreenPerformance_Label_series2right, chart_label_buffer);
  // chart ranges
  lv_chart_set_range(ui_ScreenPerformance_Chart_top, LV_CHART_AXIS_PRIMARY_Y, pidConfig[22].min, pidConfig[22].max);
  lv_chart_set_range(ui_ScreenPerformance_Chart_top, LV_CHART_AXIS_SECONDARY_Y, pidConfig[23].min, pidConfig[23].max);
  lv_chart_set_range(ui_ScreenPerformance_Chart_bottom, LV_CHART_AXIS_PRIMARY_Y, pidConfig[24].min, pidConfig[24].max);
  lv_chart_set_range(ui_ScreenPerformance_Chart_bottom, LV_CHART_AXIS_SECONDARY_Y, pidConfig[25].min, pidConfig[25].max);
    // After setting ranges, it's good practice to refresh the chart to apply changes
  lv_chart_refresh(ui_ScreenPerformance_Chart_top);
  lv_chart_refresh(ui_ScreenPerformance_Chart_bottom);
}
//---------------------------------
// HEX input keyboard
void initWidget() {
  // load pidconfig display
  loadDisplay();

  // disable cursor move around by clicking on Terminal
  lv_textarea_set_cursor_click_pos(ui_ScreenConfig_Textarea_Terminal, false);

  // initialize symbol
  lv_label_set_text(ui_ScreenConfig_Label_down, LV_SYMBOL_DOWN);
  lv_label_set_text(ui_ScreenService_Label_upup, LV_SYMBOL_UP);
  lv_label_set_text(ui_ScreenMonitor_Label_left, LV_SYMBOL_LEFT);
  lv_label_set_text(ui_ScreenPerformance_Label_right, LV_SYMBOL_RIGHT);
  lv_label_set_text(ui_ScreenConfig_Label_Save, LV_SYMBOL_SAVE " SAVE");
  lv_label_set_text(ui_ScreenConfig_Label_custom, LV_SYMBOL_LIST " Custom PIDs");
  lv_label_set_text(ui_ScreenConfig_Label_activateStatus, LV_SYMBOL_WARNING " Device is not activated");

  // initialize keypad
  static const char *hex_keymap[] = {"0", "1", "2", "3", "4", "\n", "5", "6", "7", "8", "9", "\n", "A", "B", "C", "D", "E", "\n", "F", LV_SYMBOL_BACKSPACE, LV_SYMBOL_OK, ""};
  static const lv_btnmatrix_ctrl_t hex_kb_ctrl[] = {
      1, 1, 1, 1, 1, // Row 1
      1, 1, 1, 1, 1, // Row 2
      1, 1, 1, 1, 1, // Row 3
      1, 2, 2 // Row 4 (OK and Backspace bigger, optional)
  };
  // This function will only be called after ui_init() runs, so it's safe to use
  // the object.
  lv_keyboard_set_map(ui_ScreenConfig_Keyboard_keypad, LV_KEYBOARD_MODE_USER_1, hex_keymap, hex_kb_ctrl);

// initialize warning list roller widget
#define MAX_OPTIONS_BUFFER_SIZE 512
  // Static buffer to build the full list of options.
  static char options_buffer[MAX_OPTIONS_BUFFER_SIZE];

  // Temporary buffer to hold the combined "Label + Unit" string for each
  // iteration. Assuming the longest label + unit + space is less than 32
  // characters.
  char temp_combined_str[32];

  int buffer_pos = 0;
  options_buffer[0] = '\0'; // Initialize buffer

  for (uint8_t i = 1; i < maxPid; i++) {

    // --- Error Check ---
    // Ensure both parts of the string are valid before combining.
    if (pidLabel[i] == NULL || pidUnit[i] == NULL) {
      const char *option_list = "Error Loading";
      lv_roller_set_options(ui_ScreenService_Roller_warnList, option_list, LV_ROLLER_MODE_NORMAL);
      // Use return to exit the function, ensuring the error message is NOT
      // overwritten.
      return;
    }

    // --- Safe Combination (Replaces the unsafe combinedString function) ---
    // Format: "LABEL UNIT" (e.g., "Engine RPM rpm")
    // Use snprintf on the small local buffer (temp_combined_str)
    snprintf(temp_combined_str, sizeof(temp_combined_str), "%s (%s)", pidLabel[i], pidUnit[i]);

    // --- Concatenate into the large options_buffer ---
    int remaining_size = MAX_OPTIONS_BUFFER_SIZE - buffer_pos;
    if (remaining_size <= 1) {
      break; // Buffer is full
    }

    // snprintf: appends the combined string ("Label Unit") followed by a
    // newline (\n)
    int written_chars = snprintf(options_buffer + buffer_pos, remaining_size, "%s\n",
                                 temp_combined_str // Use the safely combined temporary string
    );

    if (written_chars > 0) {
      buffer_pos += written_chars;
    } else {
      // Error writing (buffer full or label too long)
      break;
    }
  }
  // CRITICAL FIX: Use the full options_buffer, NOT the dangling pointer 'list'
  lv_roller_set_options(ui_ScreenService_Roller_warnList, options_buffer, LV_ROLLER_MODE_NORMAL);
}
//------------------------------------
// Load Setting
void loadSetting() {
  pref.begin("config", true);

  // screen brightness
  dim_brightness = pref.getUChar("dim_brightness", 50);
  brightness = pref.getUChar("brightness", 200); // brightness
  if (dim_brightness < 10) dim_brightness = 10; // prevent too dark to see
  lv_slider_set_left_value(ui_ScreenConfig_Slider_Brightness, dim_brightness, LV_ANIM_OFF);
  lv_slider_set_value(ui_ScreenConfig_Slider_Brightness, brightness, LV_ANIM_OFF);

  // set volume
  volume = pref.getUChar("volume", 80);
  lv_slider_set_value(ui_ScreenConfig_Slider_Volume, volume, LV_ANIM_OFF);

  // show system info
  systemInfo = pref.getBool("systemInfo", false);
  if (systemInfo) {
    lv_obj_add_state(ui_ScreenConfig_Switch_Info, LV_STATE_CHECKED);
    lv_obj_clear_flag(ui_ScreenGauge_Label_info1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ScreenMonitor_Label_info2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ScreenPerformance_Label_info3, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_state(ui_ScreenConfig_Switch_Info, LV_STATE_CHECKED);
    lv_obj_add_flag(ui_ScreenGauge_Label_info1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ScreenMonitor_Label_info2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ScreenPerformance_Label_info3, LV_OBJ_FLAG_HIDDEN);
  }

  // auto power off
  autoPowerOff = pref.getBool("autoPowerOff", true);
  if (autoPowerOff) {
    lv_obj_add_state(ui_ScreenConfig_Switch_Sleep, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(ui_ScreenConfig_Switch_Sleep, LV_STATE_CHECKED);
  }
  // wake up battery voltage threshold
  batt_volt_threshold = pref.getFloat("battThres", 0.3);
  uint8_t sel = 0;
  if (batt_volt_threshold == 0.3) {
    sel = 0;
  } else if (batt_volt_threshold == 0.5) {
    sel = 1;
  } else if (batt_volt_threshold == 1.0) {
    sel = 2;
  } else sel = 0;
  lv_dropdown_set_selected(ui_ScreenConfig_Dropdown_threshold, sel);
  // set font style
  uint8_t fontOption = pref.getUChar("fontOption", 0);
  const lv_font_t *font_array[2] = {&ui_font_oswald_36, &ui_font_segment_40}; // font array
  lv_dropdown_set_selected(ui_ScreenConfig_Dropdown_fontStyle, fontOption);
  lv_obj_set_style_text_font(ui_Screen_ScreenGauge, font_array[fontOption], LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Screen_ScreenMonitor, font_array[fontOption], LV_PART_MAIN | LV_STATE_DEFAULT);

  // device activation
  serialNo = removeColons(WiFi.macAddress());
  const String expected_code = computeHMAC(secret_key, serialNo);

  String activateCode = pref.getString("activateCode", "0123456789ABCDEF");
  lv_textarea_set_text(ui_ScreenConfig_Textarea_activationCode, activateCode.c_str()); // show activation code

#ifdef TEST_ACTIVATE
  activateCode = expect_code; // for testing activation
#endif
  if (activateCode.equals(expected_code)) {
    debug("Device is activated");
    // change status label
    lv_obj_set_style_bg_color(ui_ScreenConfig_Label_activateStatus, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ScreenConfig_Label_activateStatus, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_ScreenConfig_Label_activateStatus, LV_SYMBOL_OK " Device is activated");
    lv_textarea_set_text(ui_ScreenConfig_Textarea_activationCode, activateCode.c_str());
    lv_obj_clear_flag(ui_ScreenConfig_Textarea_activationCode, LV_OBJ_FLAG_CLICKABLE); // disable input textbox
    lv_obj_add_flag(ui_ScreenConfig_Button_activate, LV_OBJ_FLAG_HIDDEN); // hide activate button
  } else {
    debug("Device is not activated");
    lv_obj_clear_flag(ui_ScreenService_Button_readDTC, LV_OBJ_FLAG_CLICKABLE); // disable read DTC button
    lv_obj_clear_flag(ui_ScreenService_Button_updateDescription, LV_OBJ_FLAG_CLICKABLE); // disble update desc button
    lv_label_set_text(ui_ScreenService_Label_pleasewait, LV_SYMBOL_EYE_CLOSE " Activation required");
  }
  pref.end();
  lv_task_handler(); // force update screen
}
// Notification popup ------------------------------------------------------
void notifySuccess(const char *title, const char *text, const char *button, bool closeBtn) {
  //------  notify success with message box pop up
  static const char *btns[] = {button, NULL}; // LVGL uses the array for the button matrix, NOT a single
  // string like "cancel".
  lv_obj_t *mbox = lv_msgbox_create(NULL, // Parent: NULL or lv_layer_top() for a modal, full-screen overlay
                                    title, // Title
                                    text, // Text
                                    btns, // Array of button texts (must be const char** type)
                                    closeBtn // Don't add a close button (optional, but good practice for a simple error)
  );

  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_bg_color(&style, lv_color_make(0xDD, 0xFF, 0xDD));
  lv_style_set_bg_opa(&style, LV_OPA_COVER);
  lv_style_set_radius(&style, 10); // Rounded corners
  lv_style_set_border_width(&style, 3);
  lv_style_set_border_color(&style, lv_color_make(0x00, 0xCC, 0x00));
  lv_style_set_shadow_width(&style, 8);
  lv_style_set_shadow_color(&style, lv_color_make(0x00, 0xAA, 0x00));
  lv_obj_add_style(mbox, &style, LV_PART_MAIN);
  lv_obj_center(mbox);
}

void notifyError(const char *title, const char *text, const char *button, bool closeBtn) {
  static const char *btns[] = {button, NULL}; // LVGL uses the array for the button matrix, NOT a single
// string like "cancel".
  lv_obj_t *mbox = lv_msgbox_create(NULL, // Parent: NULL or lv_layer_top() for a modal, full-screen overlay
                                    title, // Title
                                    text, // Text
                                    btns, // Array of button texts (must be const char** type)
                                    closeBtn // Don't add a close button (optional, but good practice for a simple error)
  );
  lv_obj_t *closeButton = lv_msgbox_get_close_btn(mbox);
  // 1. Initialize a new style
  static lv_style_t style;
  lv_style_init(&style);
  // 2. Set the desired properties
  lv_style_set_bg_color(&style, lv_color_make(0xFF, 0xDD, 0xDD)); // Light Red/Pink background
  lv_style_set_bg_opa(&style, LV_OPA_COVER);
  lv_style_set_radius(&style, 10); // Rounded corners
  lv_style_set_border_width(&style, 3);
  lv_style_set_border_color(&style, lv_color_make(0xCC, 0x00, 0x00)); // Dark red border
  lv_style_set_shadow_width(&style, 8);
  lv_style_set_shadow_color(&style, lv_color_make(0xAA, 0x00, 0x00));
  lv_obj_set_size(closeButton, 36, 36);

  // 3. Add the style to the message box's main part
  lv_obj_add_style(mbox, &style, LV_PART_MAIN);
  lv_obj_center(mbox); // show
}
//-----------------------------------------------------------------------------------
// Copy pid_custom.csv from sdcard to littleFS
void updateCustomPID() {
  sd_spi_begin(); // Acquire VSPI for SD card
  if (!SD.begin(SD_CS, sd_spi)) {
    beep();
    notifyError(LV_SYMBOL_LIST " Load Custom PIDs", LV_SYMBOL_SD_CARD " SD card not mounted", "", true);
    sd_spi_end(); // Release VSPI
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    beep();
    notifyError(LV_SYMBOL_LIST " Load Custom PIDs", LV_SYMBOL_SD_CARD " SD card error", "", true);
    SD.end();
    sd_spi_end(); // Release VSPI
    return;
  }

  // Open source file on SD
  File sdFile = SD.open(custom_pid_file, FILE_READ);
  if (!sdFile) {
    beep();
    char txt[128];
    snprintf(txt, sizeof(txt), LV_SYMBOL_FILE " File %s not found on SD card", custom_pid_file);
    notifyError(LV_SYMBOL_LIST " Load Custom PIDs", txt, "", true);
    SD.end();
    sd_spi_end(); // Release VSPI
    return;
  }

  // Open destination file on LittleFS
  File lfsFile = LittleFS.open(custom_pid_file, FILE_WRITE);
  if (!lfsFile) {
    beep();
    notifyError(LV_SYMBOL_LIST " Load Custom PIDs", LV_SYMBOL_CLOSE " Unable to write to internal storage", "", true);
    sdFile.close();
    SD.end();
    sd_spi_end(); // Release VSPI
    return;
  }

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
  notifySuccess(LV_SYMBOL_LIST " Load Custom PIDs", LV_SYMBOL_OK " File copied successfully\nRestarting...", "", true);
  beepbeep();

  // Let LVGL render for 2 seconds before restart
  lv_timer_create([](lv_timer_t *timer) { ESP.restart(); }, 2000, NULL);
}