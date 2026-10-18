#pragma once
#include "config.h"
#include "core.h"
#include "gps_support.h"
#include "nutrition.h"
#include "progression.h"
#include "themes.h"
#include "watchlist.h"
#include <Arduino.h>
#include <atomic>
namespace dash {
// Preserve saved value 3 for Custom across upgrades.
enum class Avatar : uint8_t { Mochi, Sprout, Orbit, Custom, Wisp, Ember };
inline unsigned avatarBase(Avatar a, unsigned custom) {
  return a == Avatar::Custom  ? custom
         : a == Avatar::Wisp  ? 3
         : a == Avatar::Ember ? 4
                              : unsigned(a);
}
enum class Mood : uint8_t { Happy, Curious, Sleepy, Love, Alarm };
enum class AlertTone : uint8_t { Personality, Siren, Arcade, Chime, Silent };
struct Settings {
  Avatar avatar = Avatar::Mochi;
  bool sound = true, motion = false, spicy = true;
  Confidence threshold = Confidence::Medium;
  uint32_t gpsBaud = GpsBaud;
  uint8_t gpsPort = 0;
  uint8_t gpsProfile = 0, gpsRx = 18;
  uint32_t xp = 0;
  Theme theme = Theme::Dark;
  AlertTone tone = AlertTone::Personality;
  uint8_t volume = 245, motionStyle = 2;
  bool voice = false, repeatAlerts = false;
  uint32_t familyQuietMs = 60000;
  uint8_t customBase = 0, customAccessory = 0;
  uint16_t customBody = 0xff7a, customAccent = 0xfbb2;
  uint8_t customEyes = 0, customHeadwear = 0, customEyewear = 0, customNeckwear = 0;
  uint16_t customEye = 0x1085;
  uint8_t fieldKit = 0;           // Auto / Always / Off; cosmetic, never controls the radios.
  uint8_t view = 0, backdrop = 0; // Recon / Face; Radar / Sunset / Orbit / Tiles.
  bool rgb = true, tilt = true, gestures = true, proximity = false, autoDim = false, nfc = false;
  uint8_t ledBrightness = 32;
  char customName[17] = "My buddy";
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
  uint32_t firstAt = 0, sightings = 1;
  bool ble = false;
  uint8_t addressType = 0;
};
struct State {
  Settings settings;
  Session session;
  WardriveFood food;
  WardriveFood bleCollection;
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
  uint32_t pets = 0;
  uint32_t suppressed = 0;
  bool moving = false;
  double speedKph = -1;
  uint32_t wifiRows = 0, bleRows = 0, levelUpAt = 0, greetAt = 0, discoveryAt = 0;
  float tiltX = 0, tiltY = 0;
  bool nfcReady = false, proximityReady = false;
  char nfcMessage[64] = "NFC cards are off";
  bool gpsSearching = false;
  uint32_t gpsActiveBaud = GpsBaud;
  char gpsMessage[64] = "Waiting for NMEA";
};
extern State app;
extern Contact contacts[ContactCapacity];
extern Watchlist watchlist;
extern std::atomic<uint32_t> captureGeneration;
void saveSettings();
void loadSettings();
void gpsBegin();
void gpsTick();
GpsConfig gpsConfig();
bool gpsApply(const GpsConfig &);
bool gpsFindBaud();
void gpsCancelSearch();
void storageBegin();
void storageTick();
uint64_t storageCardSize();
bool storageFormat(uint64_t expectedBytes);
void storageCheckLog();
void storageLoadAvatar();
bool startDrive();
void stopDrive();
void logObservation(Contact &, const char *auth, uint32_t captureGen, uint32_t at);
void logAlert(const Alert &);
void radioBegin();
void radioTick();
void buddyBegin();
void buddyTick();
void buddyPet(bool allowVoice = true);
void buddyDiscovery(const uint8_t mac[6]);
void buddyAlert(const Alert &, bool demo = false);
void buddyDismiss();
void buddyMotion(bool);
void buddySoundPreview();
void buddyDemo();
void uiBegin();
void uiBoot();
void uiTick();
void say(const char *);
void uiTap(int x, int y);
void uiScreenshot();
void uiSwitchView();
void hardwareBegin();
void hardwareTick();
void nfcRememberOutfit();
void nfcCancelEnrollment();
void radioPause(bool);
void serialTick();
void printStatus();
bool applyAvatarConfig(const char *line);
void printAvatarConfig();
} // namespace dash
