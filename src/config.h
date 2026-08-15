#pragma once

#include "curve.h"
#include "game/types.h"

// Read once at load from Data\Config\AWHConfig\AWHConfig.ini.

namespace config {

struct Settings {
	bool  enabled          = true;

	// Kept from the script version of this mod, same section and key.
	bool  disableInCombat  = true;

	bool  requireWeaponOut = true;

	// Time constant for tracking the curve as the camera pitches. Fast: this is
	// following your aim, and lagging it reads as sloppiness. 0 applies the
	// curve directly, which is what the script did.
	float smoothingSeconds = 0.06f;

	// Time constant for relaxing: settling into the fully relaxed pose, whether
	// from the hotkey or from going idle. Deliberately slower than the tracking
	// above -- this is a deliberate movement, not a snap.
	float poseBlendSeconds = 0.30f;

	// Time constant for the opposite direction: coming back up out of the
	// relaxed pose, and dropping the pose entirely when the weapon is busy.
	// Faster on purpose, because that transition is a reaction.
	float poseDropSeconds = 0.15f;

	// Drop the pose while the engine considers the weapon busy: firing,
	// reloading, equipping, melee, and any idle playing on the player.
	bool  suppressWhenHandsBusy = true;

	// Drop the pose while a menu or the console is up.
	bool  suppressInMenus = true;

	// Drop the pose while a script has taken player controls away. Catches
	// scripted sequences, and mods that drive the arms outside the animation
	// system where the test above is blind to them.
	bool  suppressWhenControlsDisabled = true;

	// Which of those controls count, one bit per DisablePlayerControls
	// argument: movement, pip-boy, fighting, POV, looking, sneaking, menu,
	// activate. 255 means any of them.
	UInt32 controlSuppressMask = 0xFF;

	// A HUD tile trait to watch, as "Child\\Child\\_trait" under HUDMainMenu.
	// Non-zero drops the pose. Empty switches the check off.
	//
	// This exists for mods whose state is only ever visible in the interface --
	// Realtime Pip-Boy Map drives its minimap tile while the map is up and
	// leaves nothing else a plugin can reach.
	static constexpr UInt32 kMaxTraitPath = 128;
	char suppressUITrait[kMaxTraitPath] = "ModernMinimap\\_ToggleUpdate";

	// Pin fFirstPersonHandFollowMult to 1.0, the engine's "no lag" value, so
	// the vanilla weapon lag does not stack with a dedicated inertia mod. The
	// setting itself is left alone; only this one read is substituted.
	bool  neutraliseVanillaLag = true;

	// The default pose: tilt as a function of camera pitch, always on.
	// Parsed from "pitch:degrees, pitch:degrees, ..." and must be ascending.
	static constexpr UInt32 kMaxPoseNodes = 16;
	curve::Node poseCurve[kMaxPoseNodes]{};
	UInt32      poseCurveCount = 0;

	// Scales the whole curve. The shape is a string and so cannot be edited
	// from a menu; this gives its depth one knob that can be.
	float poseCurveScale = 1.f;

	// Used only when poseCurve is empty: a constant tilt plus a straight blend
	// to these two values at +-89 degrees.
	float fallbackTiltDegrees    = 0.f;
	float fallbackLeanUpDegrees  = 0.f;
	float fallbackLeanDownDegrees = 0.f;

	// The fully relaxed pose: one tilt, held whatever the camera is doing. It
	// replaces the curve rather than adding to it.
	float relaxedPoseDegrees = 13.f;

	// Scales both poses while a heavy weapon is in hand. Something you have to
	// brace does not hang as low as a pistol, so 0.667 -- a third off -- keeps
	// the movement readable without dropping a minigun through the floor.
	float heavyWeaponScale = 0.667f;

	// Which weapon animation types count as heavy, one bit each. The defaults
	// are the two-handed Handle and Launcher sets: miniguns, flamers, missile
	// and grenade launchers.
	UInt32 heavyWeaponTypes = (1u << 8) | (1u << 9);

	// DirectInput scancode toggling the relaxed pose, 0 to disable the hotkey.
	// 0x2C is Z.
	UInt32 poseToggleKey = 0x2C;

	// Keep the relaxed pose latched through a suppression while the player is
	// not in combat, so a shot at a gecko does not put the weapon back on the
	// curve for the next ten seconds. The pose still drops for the shot itself
	// and eases back down afterwards; only the latch survives. In combat the
	// latch is dropped as usual.
	bool  keepRelaxedOutOfCombat = true;

	// Seconds of doing nothing before relaxing on its own. 0 disables it.
	//
	// "Doing nothing" means none of the suppressions above are in force.
	// Looking around deliberately does not count: the pose is meant to follow
	// the camera, so cancelling it on camera movement would defeat the mod.
	float idleRelaxSeconds = 10.f;
};

extern Settings g_settings;

void Load();

// Re-reads the ini if it has been written since the last read, so changes made
// from the MCM menu -- or by hand, with the game running -- take effect without
// a restart. Cheap enough to call on a timer; it stats one file.
//
// Settings applied as code patches at load are deliberately not re-applied.
bool ReloadIfChanged();

} // namespace config
