# A virtual pet with a recon notebook

Dash-Chan keeps its rounded, colorful virtual-pet style. Every look is available immediately. There are no locked cosmetics, subscriptions or AI accounts.

## Home views

Tap **FACE / RECON** on the bottom bar, or select **Display + Fun → Home**.

- **Recon + Buddy:** your pet occupies the left side; three recent clues stay beside it. Tap an entry for confidence, signal evidence, address and age. Old clues fade and say OLD after five minutes; presence in the list is not proof the device is still nearby.
- **Full Face:** the expression fills the screen like a stock companion, with saved Wi-Fi/BLE counts on top and the same recording/menu controls below. The magnified primitive artwork deliberately keeps a small LCD/toy appearance.

The pet remains on screen during alerts. Tap the alert strip to dismiss the reaction, or tap an unalarmed face for affection. Backgrounds include **Signal City, Sunset Run, Star Patrol and Pixel Garden**, with **Dark, Light, Matrix and BWM** palettes. A nearby clue is evidence from radio metadata, not authenticated identity or a directional bearing.

Counters marked `~` are approximate distinct addresses successfully saved with usable GPS during the current drive. Separate bounded Bloom filters track Wi-Fi and BLE; collisions can undercount, and rotating BLE addresses can represent one physical device more than once. They reset when a new drive starts. Watching while stopped updates clues but does not earn collection counts, food or XP.

The four worlds have original neon rooftops, striped sunsets, ringed planets and pixel flowers. Movement from valid GPS scrolls the ground and city scenery. This is a playful travel cue, not a map. Each saved Wi-Fi discovery gives a brief happy bounce and colored sparks; level-ups add a five-second celebration. Active clues take priority over celebration effects. The effects do not change scanning or sound settings.

## Leveling and care

Level = `1 + floor(lifetime XP / 25)`. The progress bar shows XP toward the next level. A new Wi-Fi BSSID actually written with usable GPS earns one XP; every five such discoveries in the drive earn +12 fuel. BLE, demos, outfit changes, touch and background watch earn neither food nor XP. A new drive begins a new approximate discovery filter; this is simple exploration progression, not a global lifetime unique-network claim. XP saturates instead of wrapping at the 32-bit limit.

Touch builds affection. Speech is off by default and respects your saved mute settings. The scripts, mood eyes, bobbing, quiet glances, field kit and captions provide personality without cloud processing. In **Personality**, choose Off/Gentle/Lively head motion. Camera and microphone features remain unused.

## Separate hardware switches

Open **Display + Fun → Lights + Sensor Reactions**:

| Setting | Behavior |
|---|---|
| Mood lights | Slow, low-brightness side-LED pulses: mint exploring, lilac watching, pink affection, amber clue, gold level-up. No fast strobe. |
| Tilt + gaze | Low-pass IMU input shifts eyes and the small pet pose. It does not send sudden servo movements from bumps. |
| Head swipe | A detected forward/backward swipe switches the saved home view. It never starts a drive. Regular head petting remains quiet affection. |
| Wave hello | Optional proximity greeting when a close object enters range; a one-minute cooldown and release threshold prevent continuous greetings. Disabled by default. It detects proximity, not people. |
| Auto brightness | Optional ambient-sensor brightness adjustment, smoothly bounded between 70 and 190. Disabled by default. The sensor reading is used as a brightness cue, not a calibrated lux measurement. |

Sensor thresholds are an initial K151 tuning, not a claim of reliable person recognition or a field-calibrated ambient response. If mounting or lighting causes unwanted reactions, turn that interaction off. Wave hello is suppressed during recording and active alerts.

## NFC outfit cards

The official K151 includes an **ST25R3916 NFC reader**, not a general-purpose 125 kHz RFID reader. Dash-Chan uses ISO14443A tag UIDs only, for local outfit selection. It does not read tag contents, clone credentials, emulate access cards or write tags.

1. Choose the buddy/outfit, theme and background you want.
2. Stop any drive or upload. Open **Display + Fun → NFC outfit cards**.
3. Enable NFC, tap **Link next card to this outfit**, and hold one of your own compatible tags near the base within 30 seconds.
4. Remove and present it again to recall the outfit. You can link up to eight tags; a new ninth tag replaces the oldest slot. Re-linking a known tag replaces its look.

NFC is off by default. Polling is suspended during a drive or upload to protect recording responsiveness. No XP or food is granted. NFC recalls appearance only: audio, motion, GPS and detection settings remain yours. A matching UID is an outfit shortcut, not authentication, and not every NFC technology is supported. No physical tag read or enrollment has yet been verified on this bench; the reader initializes successfully on the connected K151.

Hardware facts and original BSP examples: [M5Stack StackChan specifications](https://docs.m5stack.com/en/StackChan), [StackChan-BSP](https://github.com/m5stack/StackChan-BSP), and [M5Unit-NFC](https://github.com/m5stack/M5Unit-NFC). These features do not enable the camera.
