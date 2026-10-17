#pragma once
#include "config.h"
#include "core.h"
#include "nutrition.h"
#include <Arduino.h>
#include <atomic>
namespace dash {
enum class Avatar : uint8_t { Mochi, Sprout, Orbit };
enum class Mood : uint8_t { Happy, Curious, Sleepy, Love, Alarm };
struct Settings {
  Avatar avatar = Avatar::Mochi;
  bool sound = true, motion = false, spicy = true;
  Confidence threshold = Confidence::Medium;
  uint32_t gpsBaud = GpsBaud;
  uint8_t gpsPort = 0;
  uint32_t xp = 0;
};
struct Contact {
  bool used = false, ble = false;
  uint8_t addressType = 0, mac[6]{};
  char name[64]{};
  int rssi = 0;
  uint8_t channel = 0;
  Match match;
  uint32_t seen = 0, logGeneration = 0;
  Cooldown logged, alerted;
};
struct Alert {
  Match match;
  char name[64]{}, mac[18]{};
  int rssi = 0;
  uint32_t at = 0;
};
struct State {
  Settings settings;
  Session session;
  WardriveFood food;
  Fix fix;
  bool sdReady = false, radioReady = false, blePhase = false, alertOpen = false;
  uint8_t channel = 1;
  uint32_t wifiSeen = 0, bleSeen = 0, rows = 0, skippedFix = 0, malformed = 0, drops = 0,
           alerts = 0, sessionAt = 0, gpsChars = 0, gpsGood = 0, gpsBad = 0;
  uint8_t bond = 70, energy = 85;
  Mood mood = Mood::Happy;
  float gaze = 0;
  Alert active, history[HistoryCapacity];
  uint8_t historyCount = 0;
  char message[96] = "Hey, road buddy. Let's explore!";
  char storageError[80]{};
  char logPath[96]{};
  uint32_t messageAt = 0, alertAt = 0;
  bool demo = false;
};
extern State app;
extern Contact contacts[ContactCapacity];
extern std::atomic<uint32_t> captureGeneration;
void saveSettings();
void loadSettings();
void gpsBegin();
void gpsTick();
void storageBegin();
void storageTick();
uint64_t storageCardSize();
bool storageFormat(uint64_t expectedBytes);
void storageCheckLog();
bool startDrive();
void stopDrive();
void logObservation(Contact &, const char *auth, uint32_t captureGen, uint32_t at);
void logAlert(const Alert &);
void radioBegin();
void radioTick();
void buddyBegin();
void buddyTick();
void buddyPet();
void buddyDiscovery(const uint8_t mac[6]);
void buddyAlert(const Alert &, bool demo = false);
void buddyDismiss();
void buddyMotion(bool);
void uiBegin();
void uiTick();
void say(const char *);
void uiTap(int x, int y);
void uiScreenshot();
void serialTick();
void printStatus();
} // namespace dash
