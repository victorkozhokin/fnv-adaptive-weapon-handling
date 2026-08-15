#pragma once

// Game structures and addresses for FalloutNV.exe 1.4.0.525 (Steam and GOG).
//
// Only what this plugin touches is declared. Offsets were verified against the
// retail binary; the static_asserts keep the padding honest.

#include "game/types.h"

namespace address {

inline constexpr UInt32 kPlayerCharacter = 0x11DEA3C; // PlayerCharacter **
inline constexpr UInt32 kTimeGlobal      = 0x11F6394; // TimeGlobal *
inline constexpr UInt32 kOSInputGlobals  = 0x11F35CC; // OSInputGlobals **
inline constexpr UInt32 kInterfaceManager = 0x11D8A80; // InterfaceManager **

// float *__thiscall Setting::GetDataPtr()
//   Returns &this->data, or a pointer to a zeroed static for a null setting.
inline constexpr UInt32 kSetting_GetDataPtr = 0x403E20;

// The three settings that drive first person weapon lag, and the only place in
// the game that reads them at runtime -- PlayerCharacter's first person hand
// update at 0x952290.
inline constexpr UInt32 kSetting_HandFollowMult        = 0x11CDCB0;
inline constexpr UInt32 kSetting_HandChaseSeconds      = 0x11CD620;
inline constexpr UInt32 kSetting_HandChaseSecondsAttack = 0x11CDE38;

// void __thiscall NiMatrix33::SetRotationX(float radians)
//   Builds a rotation about X from sin/cos of the angle; its first row is
//   (1, 0, 0).
inline constexpr UInt32 kSetRotationX = 0x524AC0;

// The lag angle reaches the skeleton through two call sites, with opposite
// signs, and both must be biased together.
//
//   0x952602 rotates "Bip01 Looking" by +angle. The arms hang off it.
//   0x94B1A4 rotates "Camera1st" by -angle. That node is a child of
//            "Bip01 Looking", so the negative cancels the positive and the view
//            stays put while the arms trail behind.
//
// Biasing only the first tilts the camera along with the arms, because the
// cancellation is then computed from the unbiased angle.
inline constexpr UInt32 kCall_TiltLookingNode = 0x952602;
inline constexpr UInt32 kCall_TiltCameraNode  = 0x94B1A4;

inline constexpr UInt32 kCall_ReadFollowMult        = 0x9522A6;
inline constexpr UInt32 kCall_ReadChaseSeconds      = 0x95236A;
inline constexpr UInt32 kCall_ReadChaseSecondsAttack = 0x95238C;

// The engine's own "the weapon is busy" tests, performed right after it reads
// the follow multiplier and each forcing it to 1.0 (0x9522FD..0x952348).
//
//   int  __thiscall Actor::GetCurrentAction()        -- -1 with no process
//   bool __thiscall PlayerCharacter::Unk_967AE0()    -- inspects the first
//                                                       person animation set
//   bool __thiscall Actor::Unk_8BBC10()              -- a process virtual
//
// Their individual meanings are not needed: what matters is that together they
// are the engine's verdict on when the hands must not be displaced.
inline constexpr UInt32 kGetCurrentAction   = 0x8A7570;
inline constexpr UInt32 kHandsBusyAnim      = 0x967AE0;
inline constexpr UInt32 kHandsBusyProcess   = 0x8BBC10;

// AnimData *__thiscall PlayerCharacter::GetAnimData(bool), the overload the
// hand update itself uses.
inline constexpr UInt32 kGetHandAnimData    = 0x950A60;

// The action value that exempts a weapon animation from the first two tests.
inline constexpr int    kActionExempt       = 4;

// Base anim group range the engine treats as "weapon busy": attacks, reloads,
// equips and the rest.
inline constexpr UInt32 kBusyGroupFirst     = 0x18;
inline constexpr UInt32 kBusyGroupLast      = 0xA8;

// TESForm::typeID for TESObjectWEAP.
inline constexpr UInt8  kFormType_Weapon    = 0x28;

} // namespace address

// No weapon equipped, or nothing that can be classified. Outside the 0..13 the
// game itself uses, so it never matches a configured heavy type.
inline constexpr UInt32 kWeaponType_None = 0xFF;

struct AnimData {
	enum SequenceTypes {
		kSequence_Idle = 0,
		kSequence_Movement,
		kSequence_LeftArm,
		kSequence_LeftHand,
		kSequence_Weapon,
		kSequence_WeaponUp,
		kSequence_WeaponDown,
		kSequence_SpecialIdle,
	};

	// The engine stores 0xFF, not 0xFFFF, in an unused slot.
	enum { kAnimGroup_None = 0xFF };

	UInt8  pad00[0x4C];
	UInt16 animGroupIDs[8]; // 4C
};

static_assert(__builtin_offsetof(AnimData, animGroupIDs) == 0x4C);

// Setting is { vtbl, Info data, const char *name }; GetDataPtr hands out &data.
struct Setting {
	void *vtbl;    // 00
	float value;   // 04
	const char *name; // 08
};

static_assert(sizeof(Setting) == 0x0C);

struct BaseProcess {
	UInt8 pad00[0x28];
	UInt8 processLevel; // 28
	UInt8 pad29[0x10C];
	UInt8 isWeaponOut;  // 135 (MiddleHighProcess and above only)
};

static_assert(__builtin_offsetof(BaseProcess, processLevel) == 0x28);
static_assert(__builtin_offsetof(BaseProcess, isWeaponOut)  == 0x135);

struct PlayerCharacter {
	// pcControlFlags mirrors DisablePlayerControls' arguments, one bit each,
	// and is what any script takes away controls through -- including the
	// NVSE-plugin variants mods use.
	enum ControlFlags : UInt8 {
		kControlFlag_Movement = 0x01,
		kControlFlag_Any      = 0xFF,
	};

	UInt8        pad000[0x24];
	NiVector3    rotation;       // 024  radians
	UInt8        pad030[0x38];
	BaseProcess *baseProcess;    // 068
	UInt8        pad06C[0x614];
	UInt8        pcControlFlags; // 680
	UInt8        pad681[0x0F];
	AnimData    *firstPersonAnim; // 690
	UInt8        pad694[0x75C];
	UInt8        pcInCombat;     // DF0

	static PlayerCharacter *GetSingleton()
	{
		return *(PlayerCharacter**)address::kPlayerCharacter;
	}

	// Degrees, matching what the GetAngle script function reports. The engine
	// stores the value in radians.
	float GetPitch() const { return rotation.x * 57.29578018f; }

	AnimData *GetHandAnimData() const
	{
		return ThisCall<AnimData*>(address::kGetHandAnimData, (void*)this, 0);
	}

	// TESObjectWEAP::eWeaponType of whatever is in the player's hands -- the
	// "Animation Type" field in the GECK:
	//
	//    0 hand to hand    1 melee 1H     2 melee 2H     3 pistol
	//    4 pistol energy   5 rifle        6 automatic    7 rifle energy
	//    8 handle          9 launcher    10 grenade     11 mine
	//   12 lunchbox mine  13 thrown
	//
	// Read from the weapon form rather than from an animation group. The groups
	// looked like the cheaper route -- the engine does keep the class in their
	// high byte -- but only while a weapon animation is actually loaded, which
	// is never during the plain aiming this pose exists for.
	//
	// Returns kWeaponType_None when nothing usable is equipped.
	UInt32 WeaponAnimType() const
	{
		const auto *process = baseProcess;

		if (process == nullptr)
			return kWeaponType_None;

		// BaseProcess::GetWeaponInfo, virtual 0x52, returning the equipped
		// weapon's inventory entry.
		using func_t = void *(__thiscall*)(const BaseProcess*);
		const auto *vtable = *(func_t* const*)process;
		const auto *entry = vtable[0x52](process);

		if (UInt32(entry) < 0x10000)
			return kWeaponType_None;

		// ExtraContainerChanges::EntryData: extendData, countDelta, then type.
		const auto *form = *(const UInt8* const*)((const UInt8*)entry + 0x08);

		// TESForm::typeID. Checked rather than trusted: if the virtual above is
		// ever not the one meant, this is what stops a stray pointer from being
		// read as a weapon.
		if (UInt32(form) < 0x10000 || form[0x04] != address::kFormType_Weapon)
			return kWeaponType_None;

		return form[0xF4];
	}

	// Virtual call into PlayerCharacter::GetAnimData (0x950A10). Returns null
	// while the player has no 3D loaded, so callers must check.
	AnimData *GetAnimData() const
	{
		using func_t = AnimData *(__thiscall*)(const PlayerCharacter*);
		const auto *vtable = *(func_t**)this;
		return vtable[0x1E4 / 4](this);
	}

	// The third person set, via BaseProcess::GetAnimData (vtable 0x1B8), the
	// way Actor::GetAnimData (0x8B70D0) reaches it. GetAnimData above returns
	// only whichever set is currently in charge, which depends on the camera.
	AnimData *GetThirdPersonAnimData() const
	{
		auto *process = baseProcess;

		if (process == nullptr)
			return nullptr;

		using func_t = AnimData *(__thiscall*)(BaseProcess*);
		const auto *vtable = *(func_t**)process;
		return vtable[0x1B8 / 4](process);
	}

	// True while any of the player's animation sets has a special idle loaded.
	//
	// All four are checked because they disagree: which set GetAnimData hands
	// back depends on the camera, and an idle injected for one is not visible
	// in the others. Asking only the first person set -- the one the hand
	// update itself uses -- misses interaction animations outright.
	bool SpecialIdlePlaying() const
	{
		const auto has = [](const AnimData *animData) {
			return animData != nullptr &&
			       animData->animGroupIDs[AnimData::kSequence_SpecialIdle] !=
			           AnimData::kAnimGroup_None;
		};

		return has(GetHandAnimData()) || has(GetAnimData()) ||
		       has(firstPersonAnim)   || has(GetThirdPersonAnimData());
	}

	int GetCurrentAction() const
	{
		return ThisCall<int>(address::kGetCurrentAction, (void*)this);
	}

	// Reproduces the engine's own decision, in its own order, that the hands
	// must not be displaced this frame.
	bool HandsBusy() const
	{
		const auto *animData = GetHandAnimData();

		if (animData == nullptr)
			return true;

		const auto action = GetCurrentAction();

		// GetBaseAnimGroupID is a plain mask; see 0x5F2440.
		const auto group =
			UInt32(animData->animGroupIDs[AnimData::kSequence_Weapon]) & 0xFF;

		if (action != address::kActionExempt &&
		    group >= address::kBusyGroupFirst && group <= address::kBusyGroupLast)
			return true;

		if (action != address::kActionExempt &&
		    ThisCall<bool>(address::kHandsBusyAnim, (void*)this))
			return true;

		if (ThisCall<bool>(address::kHandsBusyProcess, (void*)this))
			return true;

		// The script version also stood down for any idle on the player, which
		// is how interaction animations drive it.
		return SpecialIdlePlaying();
	}

	// Whether a script has taken any of the masked controls away.
	//
	// This is the one signal that catches mods which drive the first person
	// arms outside the animation system. Realtime Pip-Boy Map is the case in
	// point: it raises the pip-boy with kNVSE's ActivateAnim, which plays a
	// NiControllerSequence straight on the controller manager, so AnimData
	// never learns of it and HandsBusy below cannot see it. What it does do is
	// disable player controls for the duration, which lands here.
	bool ControlsDisabled(UInt8 mask) const
	{
		return (pcControlFlags & mask) != 0;
	}

	bool IsWeaponOut() const
	{
		const auto *process = baseProcess;

		if (process == nullptr || process->processLevel > 1)
			return false;

		return process->isWeaponOut != 0;
	}
};

static_assert(__builtin_offsetof(PlayerCharacter, rotation)        == 0x024);
static_assert(__builtin_offsetof(PlayerCharacter, baseProcess)     == 0x068);
static_assert(__builtin_offsetof(PlayerCharacter, pcControlFlags)  == 0x680);
static_assert(__builtin_offsetof(PlayerCharacter, firstPersonAnim) == 0x690);
static_assert(__builtin_offsetof(PlayerCharacter, pcInCombat)      == 0xDF0);

// Raw DirectInput key state, indexed by scancode.
struct OSInputGlobals {
	UInt8 pad0000[0x18F8];
	UInt8 currKeyStates[256]; // 18F8

	static OSInputGlobals *Get() { return *(OSInputGlobals**)address::kOSInputGlobals; }
};

static_assert(__builtin_offsetof(OSInputGlobals, currKeyStates) == 0x18F8);

struct InterfaceManager {
	UInt8  pad000[0x0C];
	UInt32 currentMode; // 00C  1 = game mode, higher = a menu is up

	static InterfaceManager *Get() { return *(InterfaceManager**)address::kInterfaceManager; }
};

static_assert(__builtin_offsetof(InterfaceManager, currentMode) == 0x0C);

// True while any menu, including the console, has focus.
inline bool IsMenuMode()
{
	const auto *manager = InterfaceManager::Get();
	return manager == nullptr || manager->currentMode > 1;
}

// scancode is a DirectInput code (DIK_*), e.g. 0x2C for Z.
inline bool IsKeyDown(UInt32 scancode)
{
	if (scancode == 0 || scancode > 255)
		return false;

	const auto *input = OSInputGlobals::Get();
	return input != nullptr && input->currKeyStates[scancode] != 0;
}

// Frame timing. secondsPassed is the current frame's delta.
struct TimeGlobal {
	UInt8 pad00[0x0C];
	float secondsPassed; // 0C

	static TimeGlobal *Get() { return (TimeGlobal*)address::kTimeGlobal; }
};

static_assert(__builtin_offsetof(TimeGlobal, secondsPassed) == 0x0C);
