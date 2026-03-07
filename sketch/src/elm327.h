#pragma once
#include <Arduino.h>

// ---  FOR TESTING PURPOSE ------
// #define DEBUG         //show log data in terminal tab
// #define TEST_ACTIVATE //Test activation normally false
// #define TEST_DTC    //for testing reading DTCs


//----------------------------------------------------------------------------
struct PIDs {
  const char *label; // Pid label show on meter
  const char *unit;  // unit for pid
  const char *pid;   // string pid 0104
  uint8_t formula;   // formula no to calculate
  int32_t min;       // lowest value
  int32_t max;       // highest value
  uint8_t skip;      // no of skip reading
  bool digit;        // want to show digit or no
  uint32_t warn;     // default warning value
};
extern PIDs pidConfig[];
extern const char *custom_pid_file;

#define TARGET_BAUD 0 // int baud[MAX_RATES] = {115200, 57600, 38400, 19200};

extern const char *version;
extern const String compile_date;
extern const uint8_t elm327InitCount;
extern const String elm327Init[];
extern uint8_t initIndex;

extern uint8_t page;
extern uint8_t startPidIndex[];
extern uint8_t endPidIndex[];
extern int8_t pidIndex;
extern const uint8_t maxPid;
// extern uint8_t AA, BB;

extern bool initializing;
extern bool prompt;
extern bool skip;
extern String elm_message;
extern uint8_t pidRead;

extern const char *pidLabel[];
extern const char *pidUnit[];
extern int32_t pidMin[];
extern int32_t pidMax[];
extern String pidList[];
extern uint8_t pidReadSkip[];
extern uint8_t pidCurrentSkip[];
extern int32_t pidWarn[];
extern float batt_volt_threshold;

void resetData();
void setELMbaudrate();
void updateGaugeDisplay(uint8_t pidNo, String response);
void readDTCs();
void clearDTC();
void readVINNumber();
void nonBlockDelaySec(const unsigned timer);
void updateDTCdescription();
void live_chart_init();
