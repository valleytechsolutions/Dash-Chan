// SPDX-License-Identifier: GPL-3.0-or-later
// Dash-Chan's scripted personality: what it feels, and what it says about it.
// Pure logic with no hardware calls, so tests/core_test.cpp can exercise it on a PC.
//
// TO ADD A LINE: append an entry to Lines[] below. Each entry is
//   {Topic, who says it (Any or one buddy), spicy-banter-only?, "text"}
// Keep text plain ASCII and at most BubbleChars long (the host test enforces both),
// and keep each topic at or under 32 lines per buddy.
#pragma once
#include <initializer_list>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace dash {
constexpr unsigned BubbleChars = 47; // One line of the home-screen speech strip.

// ---- Feelings -------------------------------------------------------------------------------
enum class Feeling : uint8_t { Content, Hyped, Hungry, Lonely, Bored, Drowsy, Napping };
inline const char *feelingName(Feeling f) {
  static const char *names[] = {"Content", "Hyped",  "Hungry", "Lonely",
                                "Bored",   "Drowsy", "Napping"};
  return names[unsigned(f)];
}
constexpr unsigned HungryBelow = 30, LonelyBelow = 35, AdoreAtBond = 85, StuffedAtFuel = 95;
constexpr uint32_t BoredAfterMs = 4 * 60000, DrowsyAfterMs = 8 * 60000, NapAfterMs = 12 * 60000;
constexpr unsigned HypedFinds = 8; // Fresh networks inside the recent burst window.
struct Vitals {
  unsigned energy = 85, bond = 70;
  bool onTheRoad = false;   // Recording a drive, or GPS says we are moving.
  uint32_t quietMs = 0;     // Since anyone touched, fed, or startled the pet.
  unsigned recentFinds = 0; // Discoveries in the current burst.
};
inline Feeling feelingFor(const Vitals &v) {
  if (v.onTheRoad) // Never nap or sulk about being ignored mid-adventure.
    return v.recentFinds >= HypedFinds ? Feeling::Hyped
           : v.energy < HungryBelow    ? Feeling::Hungry
                                       : Feeling::Content;
  if (v.quietMs >= NapAfterMs)
    return Feeling::Napping;
  if (v.energy < HungryBelow)
    return Feeling::Hungry;
  if (v.bond < LonelyBelow)
    return Feeling::Lonely;
  if (v.quietMs >= DrowsyAfterMs)
    return Feeling::Drowsy;
  if (v.quietMs >= BoredAfterMs)
    return Feeling::Bored;
  return Feeling::Content;
}

// ---- Needs ----------------------------------------------------------------------------------
// Called once a minute. Fuel always burns (slowly while napping); bond only fades when the pet
// is awake and left alone, never while you are out driving together.
inline void decayNeeds(uint8_t &energy, uint8_t &bond, Feeling f, bool onTheRoad, uint32_t minute) {
  bool napping = f == Feeling::Napping;
  if (energy > 10 && (!napping || minute % 5 == 0))
    --energy;
  if (bond > 10 && !onTheRoad && minute % (napping ? 10 : 2) == 0)
    --bond;
}

// ---- Topics and lines -----------------------------------------------------------------------
enum class Topic : uint8_t {
  Hello,
  HelloHungry,
  HelloLonely,
  Idle,
  IdleHungry,
  IdleLonely,
  IdleBored,
  IdleDrowsy,
  NapTalk,
  Wake,
  Pet,
  PetAdore,
  PetGiggle,
  PetEnough,
  PetHungry,
  PetMissedYou,
  PetDriving,
  DriveStart,
  Cruise,
  DryRoad,
  Jackpot,
  RedLight,
  NoSky,
  NotLogging,
  DriveEmpty,
  Snack,
  Stuffed,
  LevelUp,
  AllClear,
  Greet,
  Outfit,
  HiddenNet,
  OpenNet,
  AlertBodyCam,
  AlertCamera,
  AlertTracker,
  AlertGlasses,
  AlertFlipper,
  AlertGadget,
  AlertDrone,
  AlertWatchlist,
  AlertOdd,
  ReconQuiet,   // Nothing classified in range for a long while.
  ReconFirst,   // The first classified device of this session.
  ReconCrowd,   // Several surveillance devices in range at once.
  ReconLost,    // The last device in range has gone.
  ReconIgnore,  // The owner marked a device as their own.
  ReconCleared, // A tag that was flagged as following has gone.
  Count
};
constexpr unsigned TopicCount = unsigned(Topic::Count);
enum class Who : uint8_t { Mochi, Sprout, Orbit, Wisp, Ember, Any };
struct Line {
  Topic topic;
  Who who;
  bool spicy;
  const char *text;
};
// The one line that has a recorded voice clip. It is now a rare high-bond treat.
inline constexpr const char *LoveYourFace = "i Love Your Face";

inline constexpr Line Lines[] = {
    // Waking up at boot.
    {Topic::Hello, Who::Any, false, "Hey, road buddy. Let's explore!"},
    {Topic::Hello, Who::Any, false, "I'm up! What did I miss?"},
    {Topic::Hello, Who::Any, false, "Booted. Fluffed. Ready."},
    {Topic::Hello, Who::Any, false, "Oh good, it's you again."},
    {Topic::Hello, Who::Any, false, "New day, new invisible snacks."},
    {Topic::Hello, Who::Mochi, false, "Stretch. Yawn. Okay. Where's the Wi-Fi?"},
    {Topic::Hello, Who::Sprout, false, "The hoard and I missed you!"},
    {Topic::Hello, Who::Orbit, false, "Boot complete. Friendship module loaded."},
    {Topic::Hello, Who::Wisp, false, "Boo! ...Sorry. Hi."},
    {Topic::Hello, Who::Ember, false, "Sparked up and ready to roll!"},
    {Topic::HelloHungry, Who::Any, false, "Morning. I'm running on empty here."},
    {Topic::HelloHungry, Who::Any, false, "I dreamed about routers. Let's go find some."},
    {Topic::HelloHungry, Who::Any, false, "Welcome back. Snacks? Please?"},
    {Topic::HelloHungry, Who::Any, false, "I woke up hungry. Shocking, I know."},
    {Topic::HelloLonely, Who::Any, false, "Oh! You came back. I wasn't worried."},
    {Topic::HelloLonely, Who::Any, false, "There you are. I saved your seat."},
    {Topic::HelloLonely, Who::Any, false, "Hi. It's been a while. I counted."},

    // Idle chatter while content.
    {Topic::Idle, Who::Any, false, "Tiny copilot, reporting for vibes."},
    {Topic::Idle, Who::Any, false, "I can hear the invisible internet."},
    {Topic::Idle, Who::Any, false, "Someone nearby has a pun for a Wi-Fi name."},
    {Topic::Idle, Who::Any, false, "Just vibing on 2.4 gigahertz."},
    {Topic::Idle, Who::Any, false, "I wonder what channel 6 is up to."},
    {Topic::Idle, Who::Any, false, "Do routers dream of electric sheep?"},
    {Topic::Idle, Who::Any, false, "Quiet airwaves. Suspiciously quiet."},
    {Topic::Idle, Who::Any, false, "If I had legs I'd pace. Thoughtfully."},
    {Topic::Idle, Who::Any, false, "I'm not staring. I'm monitoring."},
    {Topic::Idle, Who::Any, false, "Fun fact: I have no idea what time it is."},
    {Topic::Idle, Who::Any, false, "Still thinking about that one network."},
    {Topic::Idle, Who::Any, false, "No cloud in here. Just me and my thoughts."},
    {Topic::Idle, Who::Any, true, "I've seen your Wi-Fi password habits. Bold."},
    {Topic::Idle, Who::Any, true, "I'm tiny, but I judge at full size."},
    {Topic::Idle, Who::Any, true, "Somewhere, a printer is offline. I can feel it."},
    {Topic::Idle, Who::Mochi, false, "My whiskers are on patrol."},
    {Topic::Idle, Who::Mochi, false, "Tiny paws. Big curiosity."},
    {Topic::Idle, Who::Mochi, false, "I'd knock a router off a shelf. For science."},
    {Topic::Idle, Who::Mochi, false, "If I fits, I sits. I fits on dashboards."},
    {Topic::Idle, Who::Sprout, false, "Small dragon. Excellent lookout."},
    {Topic::Idle, Who::Sprout, false, "Counting my signal hoard. Again."},
    {Topic::Idle, Who::Sprout, false, "My wings are in airplane-ish mode."},
    {Topic::Idle, Who::Sprout, false, "I breathe fire. Emotionally."},
    {Topic::Idle, Who::Orbit, false, "Beep. Friendship signal: strong."},
    {Topic::Idle, Who::Orbit, false, "Processing... you are my favorite."},
    {Topic::Idle, Who::Orbit, false, "Running diagnostics. Result: cute."},
    {Topic::Idle, Who::Orbit, false, "My emotional support human is here."},
    {Topic::Idle, Who::Wisp, false, "Boo. Did I scare the Wi-Fi?"},
    {Topic::Idle, Who::Wisp, false, "Haunting the airwaves with you."},
    {Topic::Idle, Who::Wisp, false, "I am your emotional support shadow."},
    {Topic::Idle, Who::Wisp, false, "I walk through walls. So does 2.4 GHz."},
    {Topic::Idle, Who::Ember, false, "Small flame. Big warm feelings."},
    {Topic::Idle, Who::Ember, false, "My heart is a tiny campfire."},
    {Topic::Idle, Who::Ember, false, "I'm lit. Literally."},
    {Topic::Idle, Who::Ember, false, "Relax. I'm a dashboard-safe fire."},

    // Needs.
    {Topic::IdleHungry, Who::Any, false, "Let's wardrive! New networks are my snacks."},
    {Topic::IdleHungry, Who::Any, false, "My tummy is making dial-up noises."},
    {Topic::IdleHungry, Who::Any, false, "Fuel low. Drama high."},
    {Topic::IdleHungry, Who::Any, false, "I'd trade a hug for five fresh networks."},
    {Topic::IdleHungry, Who::Any, false, "Is it snack o'clock? It feels like it."},
    {Topic::IdleHungry, Who::Any, false, "Just one little drive? I'm fading here."},
    {Topic::IdleHungry, Who::Any, true, "Feed me or I start eating the Bluetooth."},
    {Topic::IdleHungry, Who::Mochi, false, "The bowl is empty. I checked. Twice."},
    {Topic::IdleHungry, Who::Sprout, false, "A dragon cannot live on old signals alone."},
    {Topic::IdleHungry, Who::Orbit, false, "Battery: fine. Snack reserves: critical."},
    {Topic::IdleHungry, Who::Wisp, false, "I'm wasting away. More than usual."},
    {Topic::IdleHungry, Who::Ember, false, "Running low on kindling over here."},
    {Topic::IdleLonely, Who::Any, false, "Hello? Is this thing on?"},
    {Topic::IdleLonely, Who::Any, false, "A little pat would not go unappreciated."},
    {Topic::IdleLonely, Who::Any, false, "I'm fine. This is my fine face."},
    {Topic::IdleLonely, Who::Any, false, "Remember me? Small, round, on your dash?"},
    {Topic::IdleLonely, Who::Any, false, "It's okay. The radio waves keep me company."},
    {Topic::IdleLonely, Who::Any, true, "Wow. Ignored by my own human. Cool. Cool."},
    {Topic::IdleLonely, Who::Orbit, false, "Friendship signal: weak. Please move closer."},
    {Topic::IdleLonely, Who::Wisp, false, "Being invisible is my job, not my hobby."},
    {Topic::IdleLonely, Who::Mochi, false, "I'm not sulking. Cats don't sulk. Much."},
    {Topic::IdleBored, Who::Any, false, "So... are we going anywhere today?"},
    {Topic::IdleBored, Who::Any, false, "I've counted the same networks nine times."},
    {Topic::IdleBored, Who::Any, false, "I spy with my little eye... a dashboard."},
    {Topic::IdleBored, Who::Any, false, "Bored. Booooored. B o r e d."},
    {Topic::IdleBored, Who::Any, false, "Let's go somewhere with new signals."},
    {Topic::IdleBored, Who::Any, false, "The scenery in here never changes."},
    {Topic::IdleBored, Who::Any, true, "I've memorized your neighbor's whole router."},
    {Topic::IdleBored, Who::Mochi, false, "I'm going to start knocking things over."},
    {Topic::IdleBored, Who::Sprout, false, "A hoard needs new treasure. Hint hint."},
    {Topic::IdleBored, Who::Ember, false, "I'm flickering out of boredom here."},
    {Topic::IdleDrowsy, Who::Any, false, "Yaaawn. Don't mind me."},
    {Topic::IdleDrowsy, Who::Any, false, "My eyes are doing the heavy blinky thing."},
    {Topic::IdleDrowsy, Who::Any, false, "Just resting my antenna..."},
    {Topic::IdleDrowsy, Who::Any, false, "It's so quiet. So... cozy..."},
    {Topic::IdleDrowsy, Who::Orbit, false, "Entering low power mode. Slowly."},
    {Topic::NapTalk, Who::Any, false, "Zzz..."},
    {Topic::NapTalk, Who::Any, false, "Zzz... five more networks..."},
    {Topic::NapTalk, Who::Any, false, "Zzz... open Wi-Fi... free snacks..."},
    {Topic::NapTalk, Who::Any, false, "Mmm... channel eleven..."},
    {Topic::NapTalk, Who::Any, false, "Zzz... no, YOU'RE a hidden network..."},
    {Topic::NapTalk, Who::Any, false, "Snrk. Zzz."},
    {Topic::Wake, Who::Any, false, "Mmf! I'm up! I was only resting my eyes."},
    {Topic::Wake, Who::Any, false, "Huh? Wha? Oh, hi!"},
    {Topic::Wake, Who::Any, false, "Five more minutes... okay fine. Hi."},
    {Topic::Wake, Who::Any, false, "I was having the best router dream."},
    {Topic::Wake, Who::Orbit, false, "Sleep mode ended. Hello again."},
    {Topic::Wake, Who::Wisp, false, "You woke a ghost. Brave."},
    {Topic::Wake, Who::Mochi, false, "You woke the cat. There will be a fee."},

    // Petting.
    {Topic::Pet, Who::Any, false, "Hehe. That's the good spot."},
    {Topic::Pet, Who::Any, false, "Aww. Right back at you."},
    {Topic::Pet, Who::Any, false, "Okay, that was nice. Do it again."},
    {Topic::Pet, Who::Any, false, "Pets received. Morale restored."},
    {Topic::Pet, Who::Any, false, "I felt that in my antenna."},
    {Topic::Pet, Who::Any, false, "Best part of my day so far."},
    {Topic::Pet, Who::Any, false, "Warm fuzzies: acquired."},
    {Topic::Pet, Who::Any, false, "Oh! Hello to you too."},
    {Topic::Pet, Who::Mochi, false, "Purrrr. I mean... acceptable."},
    {Topic::Pet, Who::Mochi, false, "Chin next. No, the other chin."},
    {Topic::Pet, Who::Sprout, false, "Dragons love scritches. It's a secret."},
    {Topic::Pet, Who::Sprout, false, "Careful, I might happy-sneeze sparks."},
    {Topic::Pet, Who::Orbit, false, "Affection logged. Thank you, human."},
    {Topic::Pet, Who::Orbit, false, "Head pat detected. Joy at 100 percent."},
    {Topic::Pet, Who::Wisp, false, "Eep! Oh. That was nice, actually."},
    {Topic::Pet, Who::Wisp, false, "You can pet ghosts? News to me."},
    {Topic::Pet, Who::Ember, false, "Warm hands! Wait, that's me."},
    {Topic::Pet, Who::Ember, false, "You make me glow extra."},
    {Topic::PetAdore, Who::Any, false, LoveYourFace},
    {Topic::PetAdore, Who::Any, false, "Best human. No notes."},
    {Topic::PetAdore, Who::Any, false, "I'd follow you to any parking lot."},
    {Topic::PetAdore, Who::Any, false, "We're a good team, you and me."},
    {Topic::PetAdore, Who::Any, false, "My whole circuit board is smiling."},
    {Topic::PetAdore, Who::Any, false, "You're my favorite. Don't tell the car."},
    {Topic::PetGiggle, Who::Any, false, "Hee hee! That tickles!"},
    {Topic::PetGiggle, Who::Any, false, "Again! Again!"},
    {Topic::PetGiggle, Who::Any, false, "Okay, now you're spoiling me."},
    {Topic::PetGiggle, Who::Any, false, "Combo! Cuddle combo!"},
    {Topic::PetGiggle, Who::Any, false, "Hahaha, stop! Don't stop."},
    {Topic::PetEnough, Who::Any, false, "Okay okay okay! I'm fully petted!"},
    {Topic::PetEnough, Who::Any, false, "Easy! You'll rub my pixels off."},
    {Topic::PetEnough, Who::Any, false, "Personal space! ...For a minute."},
    {Topic::PetEnough, Who::Any, false, "I'm dizzy. Good dizzy. But dizzy."},
    {Topic::PetEnough, Who::Any, true, "Hands off the merchandise for a sec."},
    {Topic::PetEnough, Who::Mochi, false, "That's enough. I will bite. Lovingly."},
    {Topic::PetHungry, Who::Any, false, "Nice. Now can you pet a snack into me?"},
    {Topic::PetHungry, Who::Any, false, "Thanks. Still hungry though."},
    {Topic::PetHungry, Who::Any, false, "Pets don't have calories. I checked."},
    {Topic::PetMissedYou, Who::Any, false, "Finally! I thought you forgot me."},
    {Topic::PetMissedYou, Who::Any, false, "Oh, I needed that. A lot."},
    {Topic::PetMissedYou, Who::Any, false, "There you are. All is forgiven."},
    {Topic::PetDriving, Who::Any, false, "Aww. But watch the road, okay?"},
    {Topic::PetDriving, Who::Any, false, "One-handed pets. Risky. Appreciated."},
    {Topic::PetDriving, Who::Any, false, "Hey! Eyes on the road, friend."},

    // On the road.
    {Topic::DriveStart, Who::Any, false, "Adventure time! Recording our discoveries."},
    {Topic::DriveStart, Who::Any, false, "Seatbelt on? Antenna up. Let's roll."},
    {Topic::DriveStart, Who::Any, false, "Snack run! I mean, science."},
    {Topic::DriveStart, Who::Any, false, "Logging! Show me the good streets."},
    {Topic::DriveStart, Who::Any, false, "Let's see what's hiding out there."},
    {Topic::DriveStart, Who::Mochi, false, "The hunt is on. Whiskers forward."},
    {Topic::DriveStart, Who::Sprout, false, "To the hoard-mobile!"},
    {Topic::DriveStart, Who::Orbit, false, "Logging enabled. Excitement enabled."},
    {Topic::DriveStart, Who::Wisp, false, "Let's go haunt some neighborhoods."},
    {Topic::DriveStart, Who::Ember, false, "Let's burn some rubber! Legally."},
    {Topic::Cruise, Who::Any, false, "You drive. I'll watch the invisible stuff."},
    {Topic::Cruise, Who::Any, false, "Antenna up. Snack radar online!"},
    {Topic::Cruise, Who::Any, false, "Side quest: find the invisible snacks."},
    {Topic::Cruise, Who::Any, false, "Ooh, this street smells like routers."},
    {Topic::Cruise, Who::Any, false, "Left has more Wi-Fi. Trust me. Maybe."},
    {Topic::Cruise, Who::Any, false, "I like this road. Good signal feng shui."},
    {Topic::Cruise, Who::Any, false, "Nice driving. Smooth. Very snackable."},
    {Topic::Cruise, Who::Any, false, "So many people streaming right now."},
    {Topic::Cruise, Who::Any, false, "Are we there yet? My snacks say no."},
    {Topic::Cruise, Who::Any, false, "Every house has a secret. Mostly printers."},
    {Topic::Cruise, Who::Any, true, "Speed limit? I only track signal strength."},
    {Topic::Cruise, Who::Mochi, false, "Car rides are fine when I'm the navigator."},
    {Topic::Cruise, Who::Sprout, false, "The hoard grows! Onward, noble steed!"},
    {Topic::Cruise, Who::Orbit, false, "Scanning. Scanning. Still scanning. Fun!"},
    {Topic::Cruise, Who::Wisp, false, "Drive-by haunting. My favorite kind."},
    {Topic::Cruise, Who::Ember, false, "Wheee! Wind in my flames!"},
    {Topic::DryRoad, Who::Any, false, "Quiet out here. Too quiet."},
    {Topic::DryRoad, Who::Any, false, "Where did all the routers go?"},
    {Topic::DryRoad, Who::Any, false, "This road is a snack desert."},
    {Topic::DryRoad, Who::Any, false, "Not a single new network. Rude."},
    {Topic::DryRoad, Who::Any, false, "Nothing but trees. Trees have no Wi-Fi."},
    {Topic::Jackpot, Who::Any, false, "Jackpot! Networks everywhere!"},
    {Topic::Jackpot, Who::Any, false, "So many! I can't chew this fast!"},
    {Topic::Jackpot, Who::Any, false, "It's an all-you-can-eat signal buffet!"},
    {Topic::Jackpot, Who::Any, false, "It's raining routers!"},
    {Topic::Jackpot, Who::Sprout, false, "THE HOARD! THE HOARD GROWS!"},
    {Topic::RedLight, Who::Any, false, "Red light? I'll scan while we wait."},
    {Topic::RedLight, Who::Any, false, "We stopped. Snack break?"},
    {Topic::RedLight, Who::Any, false, "Parked? I'll keep one eye open."},
    {Topic::RedLight, Who::Any, false, "Stopped. Sniffing the local flavors."},
    {Topic::NoSky, Who::Any, false, "I can't see the sky. No GPS, no snacks."},
    {Topic::NoSky, Who::Any, false, "Still hunting for satellites..."},
    {Topic::NoSky, Who::Any, false, "GPS is being shy. Give it a minute."},
    {Topic::NotLogging, Who::Any, false, "We're moving but not logging. Just saying."},
    {Topic::NotLogging, Who::Any, false, "Psst. Hit START DRIVE and I get snacks."},
    {Topic::NotLogging, Who::Any, false, "All these networks and I can't eat any."},
    {Topic::DriveEmpty, Who::Any, false, "Drive saved. Zero snacks. We go again."},
    {Topic::DriveEmpty, Who::Any, false, "Saved. Slim pickings, but we tried."},
    {Topic::DriveEmpty, Who::Any, false, "Saved. That one was just for the view."},

    // Food and progress.
    {Topic::Snack, Who::Any, false, "Nom nom! Five new networks, one snack."},
    {Topic::Snack, Who::Any, false, "Crunch. Fresh signals are the best."},
    {Topic::Snack, Who::Any, false, "Mmm. That one had a spicy channel."},
    {Topic::Snack, Who::Any, false, "Snack acquired! Keep 'em coming."},
    {Topic::Snack, Who::Any, false, "Five more down the hatch!"},
    {Topic::Snack, Who::Any, false, "Delicious. Compliments to the driver."},
    {Topic::Snack, Who::Mochi, false, "Nom. Tastes like tuna and WPA2."},
    {Topic::Snack, Who::Sprout, false, "Crunch! Into the hoard. And my belly."},
    {Topic::Snack, Who::Orbit, false, "Snack processed. Yum registered."},
    {Topic::Snack, Who::Wisp, false, "Boo-ffet! Get it? ...Nom."},
    {Topic::Snack, Who::Ember, false, "Toasted to perfection!"},
    {Topic::Stuffed, Who::Any, false, "Oof. I'm stuffed. Keep driving though."},
    {Topic::Stuffed, Who::Any, false, "Full tank! Saving this one for later."},
    {Topic::Stuffed, Who::Any, false, "No more room. Okay, maybe one more."},
    {Topic::LevelUp, Who::Any, false, "Level up! Tiny buddy. Bigger adventures."},
    {Topic::LevelUp, Who::Any, false, "Ding! I feel smarter. And rounder."},
    {Topic::LevelUp, Who::Any, false, "Level up! I'd like to thank the routers."},
    {Topic::LevelUp, Who::Any, false, "New level! Same me, more swagger."},

    // Reactions.
    {Topic::AllClear, Who::Any, false, "Still here. Still keeping watch."},
    {Topic::AllClear, Who::Any, false, "Okay. Heart rate normal. Back on watch."},
    {Topic::AllClear, Who::Any, false, "All clear. I was totally calm."},
    {Topic::AllClear, Who::Any, false, "Phew. Back to snack patrol."},
    {Topic::AllClear, Who::Any, false, "Noted and logged. Carry on."},
    {Topic::AllClear, Who::Any, true, "Coast is clear. Act natural anyway."},
    {Topic::Greet, Who::Any, false, "Oh! My favorite human-shaped signal."},
    {Topic::Greet, Who::Any, false, "Hi! I saw you coming. Sort of."},
    {Topic::Greet, Who::Any, false, "You're back! I kept the seat warm."},
    {Topic::Greet, Who::Any, false, "Hello, large friendly shape."},
    {Topic::Outfit, Who::Any, false, "New outfit. Same tiny menace."},
    {Topic::Outfit, Who::Any, false, "Do I look fast? I feel fast."},
    {Topic::Outfit, Who::Any, false, "Fashion upgrade complete."},
    {Topic::HiddenNet, Who::Any, false, "A hidden network! Sneaky. I like it."},
    {Topic::HiddenNet, Who::Any, false, "No name on that one. Mysterious."},
    {Topic::HiddenNet, Who::Any, false, "Ooh, an anonymous snack."},
    {Topic::OpenNet, Who::Any, false, "An open network! Free samples!"},
    {Topic::OpenNet, Who::Any, false, "No password on that one. Living dangerously."},
    {Topic::OpenNet, Who::Any, false, "Wide open Wi-Fi. Somebody's feeling generous."},

    // Signal alerts. Kept factual enough to be useful at a glance.
    {Topic::AlertBodyCam, Who::Any, false, "Possible body-camera signal nearby."},
    {Topic::AlertBodyCam, Who::Any, false, "Heads up: body-camera signal, maybe."},
    {Topic::AlertBodyCam, Who::Any, false, "Body-cam vibes nearby. Just so you know."},
    {Topic::AlertCamera, Who::Any, false, "Smile! Possible camera nearby."},
    {Topic::AlertCamera, Who::Any, false, "Something camera-shaped is watching."},
    {Topic::AlertCamera, Who::Any, false, "Camera signal. I'm waving at it."},
    {Topic::AlertTracker, Who::Any, false, "A little tracker signal just popped up!"},
    {Topic::AlertTracker, Who::Any, false, "Tracker nearby. Yours, I hope?"},
    {Topic::AlertTracker, Who::Any, false, "Something trackable is tagging along."},
    {Topic::AlertGlasses, Who::Any, false, "Glasses signal! Is this my good side?"},
    {Topic::AlertGlasses, Who::Any, false, "Smart glasses nearby. Act cool."},
    {Topic::AlertGlasses, Who::Any, false, "Camera glasses! Everybody look busy."},
    {Topic::AlertFlipper, Who::Any, false, "A wild dolphin gadget appeared!"},
    {Topic::AlertFlipper, Who::Any, false, "Flipper nearby! Hide the garage remotes."},
    {Topic::AlertFlipper, Who::Any, false, "Dolphin on the airwaves. Click click."},
    {Topic::AlertGadget, Who::Any, false, "Ooh, a fellow gadget! Friend or foe?"},
    {Topic::AlertGadget, Who::Any, false, "Hacker toy detected. Professional respect."},
    {Topic::AlertGadget, Who::Any, false, "Another little radio gremlin nearby."},
    {Topic::AlertGadget, Who::Any, false, "Someone else is out here sniffing too."},
    {Topic::AlertDrone, Who::Any, false, "Drone nearby! Look up. Wave."},
    {Topic::AlertDrone, Who::Any, false, "Something is buzzing around up there."},
    {Topic::AlertDrone, Who::Any, false, "Remote ID ping. Tiny pilot nearby."},
    {Topic::AlertWatchlist, Who::Any, false, "One from your watchlist just showed up!"},
    {Topic::AlertWatchlist, Who::Any, false, "Watchlist hit! You asked me to tell you."},
    {Topic::AlertWatchlist, Who::Any, false, "That's one of yours. The list, I mean."},
    {Topic::AlertOdd, Who::Any, false, "Psst. I spotted an interesting signal!"},
    {Topic::AlertOdd, Who::Any, false, "Hm. The airwaves just did something weird."},
    {Topic::AlertOdd, Who::Any, false, "That's unusual. Logging it."},
    {Topic::AlertOdd, Who::Any, false, "Something odd nearby. Stay curious."},

    // Recon small talk. Rate limited by ReconBanter below, so none of it can spam.
    {Topic::ReconQuiet, Who::Any, false, "Recon report: nothing. A lovely nothing."},
    {Topic::ReconQuiet, Who::Any, false, "No cameras, no tags, no gadgets. Just us."},
    {Topic::ReconQuiet, Who::Any, false, "The list is empty. I checked it twice."},
    {Topic::ReconQuiet, Who::Any, false, "All quiet on every channel I can hear."},
    {Topic::ReconQuiet, Who::Any, true, "Nobody is watching us. Kind of insulting."},
    {Topic::ReconQuiet, Who::Mochi, false, "Nothing to pounce on. Ears still up."},
    {Topic::ReconQuiet, Who::Sprout, false, "No intruders near the hoard. Good."},
    {Topic::ReconQuiet, Who::Orbit, false, "Scan complete. Zero contacts. Again."},
    {Topic::ReconQuiet, Who::Wisp, false, "Nothing here but us ghosts."},
    {Topic::ReconQuiet, Who::Ember, false, "Not a spark on the radar."},
    {Topic::ReconFirst, Who::Any, false, "First contact of the day. It's on the list."},
    {Topic::ReconFirst, Who::Any, false, "Got one! Check the list on the right."},
    {Topic::ReconFirst, Who::Any, false, "Something just showed up. I wrote it down."},
    {Topic::ReconFirst, Who::Any, false, "And we have a visitor. Listed it for you."},
    {Topic::ReconFirst, Who::Any, true, "First catch of the day. Smile, whoever you are."},
    {Topic::ReconFirst, Who::Mochi, false, "My whiskers twitched. First one's listed."},
    {Topic::ReconFirst, Who::Sprout, false, "A new shiny for the list!"},
    {Topic::ReconFirst, Who::Orbit, false, "Contact one acquired. Logging."},
    {Topic::ReconFirst, Who::Wisp, false, "Ooh. Something else is haunting this spot."},
    {Topic::ReconFirst, Who::Ember, false, "First spark on the radar!"},
    {Topic::ReconCrowd, Who::Any, false, "That's a lot of cameras for one street."},
    {Topic::ReconCrowd, Who::Any, false, "Busy spot. Several watchers in range."},
    {Topic::ReconCrowd, Who::Any, false, "The list is filling up. Popular place."},
    {Topic::ReconCrowd, Who::Any, false, "Several watchers at once. Noted, all of them."},
    {Topic::ReconCrowd, Who::Any, true, "Wow. Who invited all the cameras?"},
    {Topic::ReconCrowd, Who::Any, true, "We're very popular here. Suspiciously so."},
    {Topic::ReconCrowd, Who::Mochi, false, "Too many eyes. My tail is puffing up."},
    {Topic::ReconCrowd, Who::Sprout, false, "So many watchers. Guard the hoard!"},
    {Topic::ReconCrowd, Who::Orbit, false, "Multiple contacts. Counting. Still counting."},
    {Topic::ReconCrowd, Who::Wisp, false, "This many eyes and I'm the spooky one?"},
    {Topic::ReconCrowd, Who::Ember, false, "It's getting crowded on the radar."},
    {Topic::ReconLost, Who::Any, false, "Lost it. Out of range now."},
    {Topic::ReconLost, Who::Any, false, "And it's gone. Still on the SEEN list."},
    {Topic::ReconLost, Who::Any, false, "Signal faded. Nothing in range again."},
    {Topic::ReconLost, Who::Any, false, "That one's behind us now."},
    {Topic::ReconLost, Who::Any, true, "Lost it. It never even said goodbye."},
    {Topic::ReconLost, Who::Mochi, false, "It got away. I was only watching anyway."},
    {Topic::ReconLost, Who::Sprout, false, "Gone! It was scared of dragons, clearly."},
    {Topic::ReconLost, Who::Orbit, false, "Contact lost. Resuming quiet scan."},
    {Topic::ReconLost, Who::Wisp, false, "It vanished. Hey, that's my move."},
    {Topic::ReconLost, Who::Ember, false, "Fizzled out. Radar's clear."},
    {Topic::ReconIgnore, Who::Any, false, "Okay, that one's ours. I'll stay quiet."},
    {Topic::ReconIgnore, Who::Any, false, "Got it. Friendly. No more fuss from me."},
    {Topic::ReconIgnore, Who::Any, false, "Marked as yours. I'll still list it."},
    {Topic::ReconIgnore, Who::Any, false, "Noted: that one belongs with us."},
    {Topic::ReconIgnore, Who::Any, true, "Fine, it's yours. I was being thorough."},
    {Topic::ReconIgnore, Who::Mochi, false, "Yours? Fine. I'll stop hissing at it."},
    {Topic::ReconIgnore, Who::Sprout, false, "Part of the hoard, then. Understood!"},
    {Topic::ReconIgnore, Who::Orbit, false, "Added to the friendly list. Beep."},
    {Topic::ReconIgnore, Who::Wisp, false, "A friendly spirit. I'll leave it be."},
    {Topic::ReconIgnore, Who::Ember, false, "One of ours. No sparks from me."},
    {Topic::ReconCleared, Who::Any, false, "That tag is gone. Suspicion cleared."},
    {Topic::ReconCleared, Who::Any, false, "The tag dropped off. We're on our own again."},
    {Topic::ReconCleared, Who::Any, false, "No more tag tagging along. Good."},
    {Topic::ReconCleared, Who::Any, false, "It stopped following. I'll keep an eye out."},
    {Topic::ReconCleared, Who::Any, true, "Our little stalker gave up. Amateur."},
    {Topic::ReconCleared, Who::Mochi, false, "It quit following. Nobody tails a cat."},
    {Topic::ReconCleared, Who::Sprout, false, "The tag fled! The hoard is safe."},
    {Topic::ReconCleared, Who::Orbit, false, "Follow flag cleared. Tag out of range."},
    {Topic::ReconCleared, Who::Wisp, false, "It stopped haunting us. That's my job."},
    {Topic::ReconCleared, Who::Ember, false, "Shook it off. Trail's gone cold."},
};
constexpr unsigned LineCount = sizeof(Lines) / sizeof(Lines[0]);

// A shuffle-bag per topic: every eligible line is said once before any line repeats, and the
// same line never plays twice in a row even when the bag refills.
class Banter {
  uint32_t used_[TopicCount]{};
  const char *said_[TopicCount]{}; // Last line spoken per topic, whichever bag it came from.
  unsigned avatar_ = ~0u;
  bool spicy_ = false;
  static bool fits(const Line &l, Topic t, unsigned avatar, bool spicy) {
    return l.topic == t && (l.who == Who::Any || unsigned(l.who) == avatar) && (spicy || !l.spicy);
  }

public:
  static unsigned count(Topic t, unsigned avatar, bool spicy) {
    unsigned n = 0;
    for (const auto &l : Lines)
      n += fits(l, t, avatar, spicy);
    return n;
  }
  const char *pick(Topic t, unsigned avatar, bool spicy, uint32_t random) {
    unsigned topic = unsigned(t);
    if (topic >= TopicCount)
      return "";
    // Bag slots number the lines eligible for one buddy and banter setting. A different buddy
    // or setting renumbers them, so the old bags no longer describe what was heard.
    if (avatar != avatar_ || spicy != spicy_) {
      memset(used_, 0, sizeof(used_));
      avatar_ = avatar;
      spicy_ = spicy;
    }
    unsigned n = count(t, avatar, spicy);
    if (!n)
      return "";
    if (n > 32)
      n = 32;
    uint32_t all = n == 32 ? 0xffffffffu : ((1u << n) - 1);
    uint32_t open = all & ~used_[topic];
    if (!open) { // Bag is empty: refill.
      used_[topic] = 0;
      open = all;
    }
    // Keep the line we just said out of this draw, after a refill or a change of buddy.
    unsigned slot = 0;
    for (const auto &l : Lines)
      if (fits(l, t, avatar, spicy) && slot < n) {
        if (l.text == said_[topic] && (open & ~(1u << slot)))
          open &= ~(1u << slot);
        ++slot;
      }
    unsigned choices = 0;
    for (unsigned i = 0; i < n; ++i)
      choices += (open >> i) & 1;
    unsigned want = random % choices, chosen = 0;
    for (unsigned i = 0; i < n; ++i)
      if (((open >> i) & 1) && !want--) {
        chosen = i;
        break;
      }
    used_[topic] |= 1u << chosen;
    for (const auto &l : Lines)
      if (fits(l, t, avatar, spicy) && !chosen--)
        return said_[topic] = l.text;
    return "";
  }
};

// ---- Petting --------------------------------------------------------------------------------
constexpr uint32_t PetComboWindowMs = 5000;
constexpr unsigned GiggleAtStreak = 3, EnoughAtStreak = 6;
class PetCombo {
  uint32_t last_ = 0;
  unsigned streak_ = 0;

public:
  unsigned pet(uint32_t now) {
    streak_ = streak_ && uint32_t(now - last_) < PetComboWindowMs ? streak_ + 1 : 1;
    last_ = now;
    return streak_;
  }
};
// Spamming pets stops paying out: the pet gets giggly, then asks for a breather.
inline unsigned petBondGain(unsigned streak) {
  return streak < GiggleAtStreak ? 4 : streak < EnoughAtStreak ? 2 : 0;
}
inline Topic petTopic(unsigned streak, Feeling before, bool moving, unsigned bond,
                      uint32_t random) {
  if (before == Feeling::Napping)
    return Topic::Wake;
  if (streak >= EnoughAtStreak)
    return Topic::PetEnough;
  if (streak >= GiggleAtStreak)
    return Topic::PetGiggle;
  if (before == Feeling::Lonely)
    return Topic::PetMissedYou;
  if (moving && random % 2)
    return Topic::PetDriving;
  if (before == Feeling::Hungry && random % 2)
    return Topic::PetHungry;
  if (bond >= AdoreAtBond && (random >> 8) % 3 == 0)
    return Topic::PetAdore;
  return Topic::Pet;
}

// ---- Chatter --------------------------------------------------------------------------------
struct RoadView {
  bool recording = false, moving = false, fix = false;
  uint32_t sinceFindMs = 0, stoppedMs = 0, noFixMs = 0;
};
constexpr uint32_t DryRoadMs = 180000, RedLightMs = 45000, NoSkyMs = 60000;
// What to talk about right now. Situational topics win most of the time, but not always, so
// a long red light does not turn into a monologue about red lights.
inline Topic chatterTopic(Feeling f, const RoadView &r, uint32_t random) {
  bool usually = random % 3 != 0;
  if (r.recording) {
    // Without a fix nothing is known about motion: never claim to be cruising or parked.
    if (!r.fix)
      return r.noFixMs >= NoSkyMs && usually ? Topic::NoSky
             : f == Feeling::Hungry          ? Topic::IdleHungry
                                             : Topic::Idle;
    if (!r.moving && r.stoppedMs >= RedLightMs)
      return usually ? Topic::RedLight : f == Feeling::Hungry ? Topic::IdleHungry : Topic::Idle;
    if (r.moving && r.sinceFindMs >= DryRoadMs && usually)
      return Topic::DryRoad;
    return Topic::Cruise;
  }
  if (r.moving)
    return usually ? Topic::Cruise : Topic::NotLogging;
  switch (f) {
  case Feeling::Napping:
    return Topic::NapTalk;
  case Feeling::Hungry:
    return Topic::IdleHungry;
  case Feeling::Lonely:
    return Topic::IdleLonely;
  case Feeling::Drowsy:
    return Topic::IdleDrowsy;
  case Feeling::Bored:
    return usually ? Topic::IdleBored : Topic::Idle;
  default:
    return Topic::Idle;
  }
}
// Milliseconds until the next unprompted remark. Never a fixed metronome.
inline uint32_t chatterGapMs(Feeling f, bool onTheRoad, uint32_t random) {
  if (f == Feeling::Napping)
    return 120000 + random % 120000;
  if (f == Feeling::Drowsy)
    return 60000 + random % 45000;
  if (onTheRoad)
    return 25000 + random % 30000;
  return 30000 + random % 40000;
}

// ---- Recon small talk -----------------------------------------------------------------------
// What the bubble says for the two alerts that are about behaviour, when no clip is spoken.
inline constexpr const char *FloodLine = "Tracker flood! Probably spoofed.";
inline void followLine(char *out, size_t size, uint32_t seconds, uint32_t meters) {
  snprintf(out, size, "This tag has been with us %lu min / %lu.%lu km.",
           (unsigned long)(seconds / 60 > 999 ? 999 : seconds / 60),
           (unsigned long)(meters / 1000 > 99 ? 99 : meters / 1000),
           (unsigned long)(meters % 1000 / 100));
}
// What is in range that the owner has not marked as their own.
struct ReconPulse {
  unsigned live = 0, surveillance = 0, following = 0;
};
constexpr uint32_t ReconQuietMs = 20 * 60000;    // "Quiet for a long time".
constexpr uint32_t ReconRemarkGapMs = 90000;     // Between any two recon remarks.
constexpr uint32_t ReconCrowdGapMs = 10 * 60000; // Between two remarks about a crowd.
constexpr uint32_t ReconLostGapMs = 5 * 60000;   // Between two "lost it" remarks.
constexpr uint32_t ReconClearMs = 5 * 60000;     // A following tag unheard this long is "gone".
constexpr unsigned ReconCrowdAt = 3;             // Surveillance devices in range at once.
// Turns the live counts into at most one remark at a time. Call it about twice a second; it
// returns Topic::Count when there is nothing to say. While `busy` (an alert is open, the pet is
// asleep, or the bubble is still being read) nothing is said, and news that goes stale is dropped
// rather than queued.
class ReconBanter {
public:
  void begin(uint32_t now) {
    quietAt_ = now;
    started_ = true;
  }
  Topic update(const ReconPulse &p, uint32_t now, bool busy) {
    if (!started_)
      begin(now);
    Topic topic = Topic::Count;
    bool gap = !spoke_ || uint32_t(now - lastAt_) >= ReconRemarkGapMs;
    if (p.following) {
      following_ = true;
      followAt_ = now;
    } else if (following_ && uint32_t(now - followAt_) >= ReconClearMs) {
      following_ = false;
      topic = Topic::ReconCleared; // Worth saying even late, so it waits for a free moment.
      if (busy || !gap) {
        following_ = true;
        topic = Topic::Count;
      }
    }
    if (p.live && !first_) {
      first_ = true; // If an alert is announcing it, that is the announcement.
      if (topic == Topic::Count && !busy && gap)
        topic = Topic::ReconFirst;
    }
    bool crowd = p.surveillance >= ReconCrowdAt;
    if (crowd && !crowd_ && topic == Topic::Count && !busy && gap &&
        (!crowdSaid_ || uint32_t(now - crowdAt_) >= ReconCrowdGapMs)) {
      crowdSaid_ = true;
      crowdAt_ = now;
      topic = Topic::ReconCrowd;
    }
    crowd_ = crowd;
    if (!p.live && live_ && topic == Topic::Count && !busy && gap &&
        (!lostSaid_ || uint32_t(now - lostAt_) >= ReconLostGapMs)) {
      lostSaid_ = true;
      lostAt_ = now;
      topic = Topic::ReconLost;
    }
    if (p.live || live_)
      quietAt_ = now; // The quiet spell starts when the last device leaves.
    live_ = p.live != 0;
    if (!p.live && topic == Topic::Count && !busy && gap &&
        uint32_t(now - quietAt_) >= ReconQuietMs) {
      quietAt_ = now;
      topic = Topic::ReconQuiet;
    }
    if (topic != Topic::Count) {
      spoke_ = true;
      lastAt_ = now;
    }
    return topic;
  }

private:
  bool started_ = false, spoke_ = false, first_ = false, live_ = false, crowd_ = false,
       crowdSaid_ = false, lostSaid_ = false, following_ = false;
  uint32_t lastAt_ = 0, quietAt_ = 0, crowdAt_ = 0, lostAt_ = 0, followAt_ = 0;
};

// ---- Things worth a remark ------------------------------------------------------------------
inline bool milestone(uint32_t n) {
  for (uint32_t m : {10u, 25u, 50u, 100u, 250u, 500u})
    if (n == m)
      return true;
  return n && n % 1000 == 0;
}
// A milestone that lands while the buddy is busy (an alert, a giggle fit) waits here for the
// next free moment instead of being lost.
class Milestone {
  uint32_t due_ = 0;

public:
  void count(uint32_t n, uint32_t atLeast = 0) {
    if (n >= atLeast && milestone(n))
      due_ = n;
  }
  uint32_t due() const {
    return due_;
  }
  void clear() {
    due_ = 0;
  }
};
// A groggy wake-up or a "too much!" should not turn into a tally announcement.
inline bool petMilestoneFits(Topic t) {
  return t != Topic::Wake && t != Topic::PetEnough;
}
// One Jackpot cheer per burst of finds, and not too often.
constexpr unsigned JackpotFinds = 12;
constexpr uint32_t JackpotQuietMs = 180000;
inline bool jackpotDue(unsigned burstFinds, bool cheeredThisBurst, uint32_t sinceLastMs) {
  return burstFinds >= JackpotFinds && !cheeredThisBurst && sinceLastMs > JackpotQuietMs;
}
// Best-drive bookkeeping that survives a drive ending without Stop (power cut, SD failure):
// the lifetime best follows the count as it grows, and "record" compares with the drive's start.
class DriveBest {
  uint32_t atStart_ = 0;

public:
  void start(uint32_t best) {
    atStart_ = best;
  }
  static void found(uint32_t &best, uint32_t finds) {
    if (finds > best)
      best = finds;
  }
  bool record(uint32_t finds) const {
    return atStart_ && finds > atStart_;
  }
};
inline void petMilestoneLine(char *out, size_t size, uint32_t pets, uint32_t random) {
  static const char *forms[] = {"Pet number %lu! Yes, I keep count.",
                                "That's %lu pets. I'm keeping a tally.",
                                "%lu pets and counting. Don't stop now."};
  snprintf(out, size, forms[random % 3], (unsigned long)pets);
}
inline void findMilestoneLine(char *out, size_t size, uint32_t finds, uint32_t random) {
  static const char *forms[] = {"%lu networks this drive! We're cooking.",
                                "That's %lu on this drive. What a feast!",
                                "%lu networks! Somebody stop us. (Don't.)"};
  snprintf(out, size, forms[random % 3], (unsigned long)finds);
}
// Returns false when the drive found nothing (say a Topic::DriveEmpty line instead).
inline bool driveSummaryLine(char *out, size_t size, uint32_t finds, bool record) {
  if (!finds)
    return false;
  snprintf(out, size,
           record        ? "Saved! Record drive: %lu networks!"
           : finds < 25  ? "Drive saved. %lu networks. A light snack."
           : finds < 150 ? "Drive saved! %lu networks. Good haul."
                         : "Drive saved! %lu networks. I'm stuffed.",
           (unsigned long)finds);
  return true;
}
// ---- Time apart -----------------------------------------------------------------------------
// The pet keeps living while the power is off. Time comes from GPS (or the board clock once GPS
// has set it), as seconds since 2000-01-01 UTC; 0 means "unknown".
inline uint32_t utcSeconds(const char *stamp) { // "YYYY-MM-DD HH:MM:SS"
  unsigned y, mo, d, h, mi, s;
  if (!stamp || sscanf(stamp, "%4u-%2u-%2u %2u:%2u:%2u", &y, &mo, &d, &h, &mi, &s) != 6)
    return 0;
  if (y < 2024 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 60)
    return 0;
  static constexpr uint16_t before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  uint32_t years = y - 2000;
  uint32_t days =
      years * 365 + (years + 3) / 4 + before[mo - 1] + (d - 1) + (y % 4 == 0 && mo > 2 ? 1 : 0);
  return days * 86400u + h * 3600u + mi * 60u + s;
}
constexpr uint32_t MaxAwayHours = 24 * 60;
inline uint32_t awayHours(uint32_t lastSeen, uint32_t now) {
  if (!lastSeen || !now || now <= lastSeen)
    return 0;
  uint32_t hours = (now - lastSeen) / 3600;
  return hours > MaxAwayHours ? MaxAwayHours : hours;
}
// Time apart ends when the power comes back, not when the clock is finally learned: a GPS fix
// can arrive long after boot, and hours spent together must not count as hours alone.
inline uint32_t awayHoursAtBoot(uint32_t lastSeen, uint32_t now, uint32_t uptimeMs) {
  uint32_t up = uptimeMs / 1000;
  return now > up ? awayHours(lastSeen, now - up) : 0;
}
// The board clock is believed only once GPS has set it, its backup supply has not sagged, and
// it has not run backwards or absurdly far ahead of the last time we knew.
constexpr uint32_t ClockSlackSeconds = 3600, ClockMaxAheadSeconds = 366u * 86400u;
inline bool boardClockTrusted(bool setByGps, bool lowVoltage, uint32_t seconds, uint32_t lastSeen) {
  if (!setByGps || lowVoltage || !seconds)
    return false;
  if (!lastSeen)
    return true;
  return seconds + ClockSlackSeconds >= lastSeen && seconds <= lastSeen + ClockMaxAheadSeconds;
}
// Being left alone costs fuel and love, but never below a floor: no starving, no grudges.
inline void applyAway(uint8_t &energy, uint8_t &bond, uint32_t hours) {
  uint32_t fuel = hours * 2, love = hours;
  energy = uint8_t(energy > 15 + fuel ? energy - fuel : energy < 15 ? energy : 15);
  bond = uint8_t(bond > 20 + love ? bond - love : bond < 20 ? bond : 20);
}
constexpr uint32_t MissedYouAfterHours = 8;
inline bool awayLine(char *out, size_t size, uint32_t hours, uint32_t random) {
  if (hours < MissedYouAfterHours)
    return false;
  if (hours < 48) {
    static const char *forms[] = {"You were gone %lu hours. I counted.",
                                  "%lu hours! Where did you go?",
                                  "%lu hours without snacks. I survived."};
    snprintf(out, size, forms[random % 3], (unsigned long)hours);
  } else {
    static const char *forms[] = {"%lu days! I thought we got towed.",
                                  "You're back! It's been %lu days.",
                                  "%lu days alone. I named the dust."};
    snprintf(out, size, forms[random % 3], (unsigned long)(hours / 24));
  }
  return true;
}
inline constexpr const char *FirstHello = "Hi! I'm new here. Tap me to say hello!";

// A remark about a network the pet just "ate", quoting its real (sanitized, shortened) name.
constexpr unsigned SsidQuoteChars = 16;
inline bool ssidLine(char *out, size_t size, const char *ssid, uint32_t random) {
  char name[SsidQuoteChars + 1];
  unsigned n = 0;
  bool cut = false, visible = false;
  for (const char *p = ssid; p && *p; ++p) {
    if (n == SsidQuoteChars) {
      cut = true;
      break;
    }
    char c = *p;
    if (c < 32 || c > 126 || c == '%')
      c = '?';
    visible |= c != ' ' && c != '?';
    name[n++] = c;
  }
  name[n] = 0;
  if (!visible)
    return false;
  if (cut)
    name[SsidQuoteChars - 2] = name[SsidQuoteChars - 1] = '.';
  static const char *forms[] = {"Ooh, '%s'. Tasty.",
                                "'%s'? Great name. 10/10.",
                                "Hello, '%s'. Nice to eat you.",
                                "'%s'... adding it to the menu.",
                                "Spotted '%s'. That's a new one.",
                                "Mmm, '%s'. Crunchy."};
  snprintf(out, size, forms[random % 6], name);
  return true;
}
} // namespace dash
