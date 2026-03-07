#pragma once
#include "LGFX_CYD.h"
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18
#define SD_CS 5

extern LGFX tft; /* LGFX instance */
extern SPIClass sd_spi;

extern String current_version;
extern bool firmware_checked;
extern String firmware_manifest;
extern bool mpu_install;
extern float cpuTemp_c;
extern float cpuTemp_f;
extern uint8_t volume;
extern uint8_t dim_brightness;
extern uint8_t brightness;
extern bool systemInfo;
extern bool autoPowerOff;

extern String serialNo;
extern const String secret_key;

#define LED_RED 4
#define LED_GREEN 16
#define LED_BLUE 17

#define RED_ON digitalWrite(LED_RED, LOW);
#define RED_OFF digitalWrite(LED_RED, HIGH);
#define GREEN_ON digitalWrite(LED_GREEN, LOW);
#define GREEN_OFF digitalWrite(LED_GREEN, HIGH);
#define BLUE_ON digitalWrite(LED_BLUE, LOW);
#define BLUE_OFF digitalWrite(LED_BLUE, HIGH);

//------------ SOUND EFFECT ---------
void initSpeaker();
void beepbeep();
void beep();
void clickSound();
void offSound();
//----------- SD CARD------------
void sd_spi_begin();
void sd_spi_end();
//------- Screen -------------------
void autoDim();
/*-----------------------------*/
// void setVersion(const String ver, String manifest);
// bool newFirmwareAvailable();
/*-----------------------------*/
void initMPU(byte sda, byte scl);
void checkCPUTemp();
void initCPUTemp();
void initFileSystem();
void initSDCard();
String removeColons(String mac);
void initWidget();
void log(String text); // log information in terminal
void debug(String text); // debug
String computeHMAC(const String &secret, const String &serial);
void loadSetting();
void notifySuccess(const char *title, const char *text, const char *button, bool closeBtn);
void notifyError(const char *title, const char *text, const char *button, bool closeBtn);
void updateCustomPID();