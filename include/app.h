#pragma once
#include "avatar_limits.h"
#include "config.h"
#include "core.h"
#include "gps_support.h"
#include "nutrition.h"
#include "personality.h"
#include "progression.h"
#include "recon.h"
#include "recon_view.h"
#include "themes.h"
#include "voice.h"
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
// Values double as face expressions in avatars.h; Cheer is the discovery bounce.
enum class Mood : uint8_t { Happy, Curious, Sleepy, Love, Alarm, Cheer, Pout };
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
  VoiceMode voiceMode = VoiceMode::Off; // Speech stays opt-in: MENU > Sound & Voice.
  bool repeatAlerts = false;
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
  // Recon alert policy (recon.h). Everything classified is listed; these only shape alerts.
  uint32_t typeFilter = TypeFilter::Defaults; // Families tracked at all (TypeFilter mask).
  bool quietRoad = true;                      // Tags and Ring stay list-only while driving.
  uint8_t autoSnooze = 5; // Alerts per device before it must come closer: 0/5/10.
  // Avatar Studio slots added with DCAV3. Counts live in avatar_limits.h.
  uint8_t customMark = 0, customBack = 0, customHeld = 0, customAura = 0;
  uint8_t customPack = 0, customPersona = 0;
};
// Lifetime memories, saved with the settings so the pet remembers you across restarts.
struct Life {
  uint32_t pets = 0, drives = 0, snacks = 0, bestDrive = 0, boots = 0;
  uint32_t lastSeen = 0; // Seconds since 2000 (UTC) when we last knew the time; 0 if never.
  bool clockSet = false; // GPS has set the board clock at least once, so it can be believed.
};
struct Contact {
  bool used = false, ble = false;
  uint8_t addressType = 0, mac[6]{};
  char name[64]{};
  char label[LabelChars]{}; // Product named by the signature behind `match`; empty if none.
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
  uint8_t kind = 0;                        // AlertKind: what sort of news this is.
  char label[LabelChars]{};                // Product label of the row that alerted, if any.
  uint8_t addr[6]{};                       // Its address, to find the row again from the card.
  uint32_t followMs = 0, followMeters = 0; // AlertFollowing: the company so far.
};
// A device, a burst of new tracker addresses, or a tracker that has travelled with us.
enum AlertKind : uint8_t { AlertDevice, AlertFlood, AlertFollowing };
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
  Feeling feeling = Feeling::Content;
  Life life;
  float gaze = 0;
  Alert active;
  LedgerState ledger = LedgerState::NoCard; // Whether /events/recon.csv is taking rows.
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
void buddyPoke();
void buddyGreet();
void buddySay(Topic);
void buddyDriveStart(bool fix);
void buddyDriveEnd(uint32_t finds);
void buddyDiscovery(const uint8_t mac[6], const char *ssid = nullptr, bool open = false);
void buddyAlert(const Alert &, bool demo = false);
void buddyReconPeek(Family top); // The Recon page was opened; `top` is the top live threat or None.
void buddyReconIgnored();        // The owner put a device on the ignore list.
void buddyDismiss();
void buddyMotion(bool);
void buddySoundPreview();
void buddyDemo();
const char *buddyVoiceTest();
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
// Recon engine (src/radio.cpp). The table holds one row per classified device, whatever the
// alert threshold.
//
// Threading: the table, the policy and the counts are only ever touched from the Arduino loop
// task. The Wi-Fi promiscuous callback and the NimBLE scan callback run on other tasks, but all
// they do is copy the packet into a FreeRTOS queue; radioTick() drains that queue on the loop
// task and is the only writer of live rows. The ledger (storageTick), the buddy, the serial
// console and the UI run from the same loop(), and the upload worker task never reads the table.
// So there is no lock, and none of these functions may be called from a callback or another
// task. The screens still draw from ReconRow copies (recon_view.h) rather than from the table:
// that keeps the rows under a finger steady between refreshes, not for thread safety.
ReconTable &reconTable();         // Rows, sorted views, lifetime counters. See recon.h.
AlertPolicy &reconPolicy();       // The ignore list and the alert timers.
const ReconCounts &reconCounts(); // Live and total rows per tier and family; fresh to 0.5 s.
AlertContext reconContext();      // The settings and road state the policy is deciding with.
uint32_t reconClock();            // Seconds since 2000 UTC, or 0 until GPS or the RTC tells us.
// Put a device on, or take it off, the ignore list; saved at once. False if the list is full.
bool reconIgnore(ReconDevice &, bool ignore);
void reconClearIgnores(); // Empty the ignore list; saved at once.
// The live row a snapshot was copied from, or null once it has left the table.
ReconDevice *reconFind(const ReconRow &);
void reconRefresh(); // Recount now, e.g. after changing a filter.
void reconSave();    // Persist the ignore list and the lifetime per-family counters (NVS).
} // namespace dash
