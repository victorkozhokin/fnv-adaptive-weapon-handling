// Adaptive Weapon Handling
//
// Tilts the first person weapon by camera pitch: a resting pose that varies
// smoothly with where you look, and holds still when you do. On top of that
// sits a second, fully relaxed pose -- one fixed tilt regardless of where the
// camera is pointing -- reached by the hotkey or by standing still long enough,
// and left again the moment the player does anything.
//
// The engine has no setting for this. Its first person weapon lag reacts to how
// fast the camera moves and decays to nothing once it stops, so no combination
// of fFirstPersonHandFollowMult and fFirstPersonHandChaseSeconds can express a
// pose that stays put. What it does have is a single angle, computed in
// PlayerCharacter's first person hand update at 0x952290, that ends up as a
// rotation of the skeleton -- and biasing that angle is exactly a pose.
//
// The angle reaches the skeleton twice, with opposite signs: "Bip01 Looking"
// gets +angle and the arms hang off it, while "Camera1st", a child of that
// node, gets -angle so the view stays put while the arms trail. Both sites are
// hooked, or the cancellation stops matching and the whole view tilts instead.
//
// Motion-dependent inertia is deliberately not implemented here. B42 Inertia
// does that job, and the vanilla lag is neutralised so the two do not stack.

#include "config.h"
#include "curve.h"
#include "game/game.h"
#include "game/tiles.h"
#include "nvse_api.h"
#include "util/log.h"
#include "util/paths.h"
#include "util/patch.h"

namespace {

struct State {
	// Tilt of the first person node, radians, as finally handed to the hooks.
	float pitchBias = 0.f;

	// The curve's output, eased so it tracks the camera without stepping.
	float trackedPose = 0.f;

	// How much of the pose is showing at all, 0 to 1. Falls to 0 whenever the
	// weapon is busy. Eased separately from, and more slowly than, the tracking
	// above -- splitting the two is what lets the weapon keep following your aim
	// closely while still taking a moment to be raised or relaxed.
	float poseWeight = 0.f;

	// Blend from the curve towards the fully relaxed pose, 0 to 1.
	float relaxWeight = 0.f;

	// Held by the hotkey until it is pressed again, or until something the
	// player does takes the weapon out of the relaxed pose.
	bool  relaxLatched = false;

	// Previous state of the hotkey, for edge detection.
	bool  toggleWasDown = false;

	// Seconds spent doing nothing in particular, for the automatic relax.
	float idleSeconds = 0.f;

	// Frames until the next look at the ini's timestamp.
	UInt32 reloadCountdown = 0;

	// Last weapon animation type seen, so the log records changes and not one
	// line per frame.
	UInt32 lastWeaponType = 0xFFFFFFFF;

	// Same for the watched HUD trait, so it can be seen firing -- or not.
	float lastTraitValue = -1.f;
	bool  haveTraitValue = false;
};

constinit State g_state;

// Handed to the engine in place of fFirstPersonHandFollowMult. Hand rotation is
// scaled by (multiplier - 1), so 1.0 is the engine's own "no lag" value.
constinit float g_neutralFollowMult = 1.f;

// Raised by the script layer while another mod owns the player's hands. A
// count, not a flag, so two mods overlapping cannot cancel each other out.
constinit UInt32 g_scriptSuppress = 0;
constinit bool   g_scriptLayerSeen = false;
constinit float  g_suppressSeconds = 0.f;

// A suppression whose owner never releases it would hold the pose off for the
// rest of the session, so it lapses well after any real one.
constexpr auto kSuppressWatchdogSeconds = 30.f;

constinit void *g_origGetDataPtr   = nullptr;
constinit void *g_origSetRotationX = nullptr;

// Fraction of the way to the target to move this frame, for an exponential
// ease with the configured time constant.
float SmoothingAlpha(float deltaTime, float timeConstant)
{
	if (timeConstant <= 0.f || deltaTime <= 0.f)
		return 1.f;

	// 1 - e^(-dt/tau), via exp2 to stay clear of libm.
	constexpr auto kInvLn2 = 1.442695041f;
	return 1.f - Exp2(-deltaTime * kInvLn2 / timeConstant);
}

bool ShouldApply(const PlayerCharacter *player)
{
	const auto &settings = config::g_settings;

	if (!settings.enabled || player == nullptr)
		return false;

	if (settings.requireWeaponOut && !player->IsWeaponOut())
		return false;

	if (settings.disableInCombat && player->pcInCombat != 0)
		return false;

	if (settings.suppressInMenus && IsMenuMode())
		return false;

	// Catches what the animation tests below cannot see: mods that drive the
	// first person arms with kNVSE's ActivateAnim, straight on the controller
	// manager, leaving AnimData none the wiser. They give themselves away by
	// taking player controls for the duration.
	if (settings.suppressWhenControlsDisabled &&
	    player->ControlsDisabled(UInt8(settings.controlSuppressMask)))
		return false;

	// Raised by the script layer, which is told by the mods themselves rather
	// than having to work it out from the animation system.
	if (g_scriptSuppress != 0)
		return false;

	// A HUD trait some other mod drives while it owns the player's hands. The
	// fallback for an install without the script, and for mods that announce
	// nothing at all.
	if (ui::HUDTrait(settings.suppressUITrait, 0.f) != 0.f)
		return false;

	// The engine's own verdict, reused rather than reimplemented. Note this is
	// genuinely needed here: those tests neutralise the *multiplier*, and the
	// pose is added further downstream where no multiplier can reach it.
	if (settings.suppressWhenHandsBusy && player->HandsBusy())
		return false;

	return true;
}

// Scale for whatever pose is being produced, given what is in the player's
// hands. Something braced against the shoulder does not hang as low as a
// pistol, so the whole pose shrinks rather than being redesigned per weapon.
float WeaponScale(const PlayerCharacter *player)
{
	const auto &settings = config::g_settings;

	if (player == nullptr || settings.heavyWeaponTypes == 0)
		return 1.f;

	const auto type = player->WeaponAnimType();

	// Logged on change only, so swapping weapons prints the number to put in
	// HeavyWeaponTypes rather than leaving it to be guessed at. 255 means
	// nothing is equipped.
	if (type != g_state.lastWeaponType) {
		g_state.lastWeaponType = type;
		log::Print("Weapon animation type %u", type);
	}

	if (type > 31 || (settings.heavyWeaponTypes & (1u << type)) == 0)
		return 1.f;

	return settings.heavyWeaponScale;
}

// The pose, in radians, for a camera pitch in degrees.
//
// Pitch is negative looking up and positive looking down; the returned tilt is
// positive downwards. The two run opposite ways round, which is worth keeping
// in mind when reading a curve out of the ini.
float PitchBias(float pitchDegrees)
{
	const auto &settings = config::g_settings;
	constexpr auto kDegToRad = 0.01745329238f;

	if (settings.poseCurveCount != 0)
		return curve::Interpolate(settings.poseCurve, settings.poseCurveCount,
		                          pitchDegrees) * settings.poseCurveScale * kDegToRad;

	const auto lean = pitchDegrees >= 0.f
		? settings.fallbackLeanDownDegrees * (pitchDegrees / 89.f)
		: settings.fallbackLeanUpDegrees * (-pitchDegrees / 89.f);

	return (settings.fallbackTiltDegrees + lean) * kDegToRad;
}

// True on the frame the hotkey goes down.
bool PollPoseToggle()
{
	const auto key = config::g_settings.poseToggleKey;

	if (key == 0)
		return false;

	// Ignore the key while a menu or the console has focus, so binding it to a
	// letter does not fire while typing.
	const auto down = !IsMenuMode() && IsKeyDown(key);
	const auto pressed = down && !g_state.toggleWasDown;

	g_state.toggleWasDown = down;
	return pressed;
}

// Runs once per frame, driven from the engine's read of the follow multiplier.
// That read sits at the top of the hand update, ahead of both rotation sites,
// so the bias they use is always this frame's.
void Update()
{
	// Pick up MCM edits, and hand edits made with the game running, without a
	// restart. About once a second, and it only stats a file until one lands.
	// The suppression lapses if whoever raised it never comes back.
	if (g_scriptSuppress != 0) {
		g_suppressSeconds += TimeGlobal::Get()->secondsPassed;

		if (g_suppressSeconds > kSuppressWatchdogSeconds) {
			g_scriptSuppress = 0;
			log::Print("Suppression timed out; releasing the pose.");
		}
	}

	if (g_state.reloadCountdown-- == 0) {
		g_state.reloadCountdown = 60;

		if (config::ReloadIfChanged())
			log::Print("Reloaded %s", paths::Ini());
	}

	const auto &settings = config::g_settings;
	const auto *player = PlayerCharacter::GetSingleton();
	const auto deltaTime = TimeGlobal::Get()->secondsPassed;

	// Polled before the checks below so the hotkey stays responsive with the
	// weapon holstered or in combat.
	const auto togglePressed = PollPoseToggle();
	const auto applying = ShouldApply(player);

	// Keep tracking the curve even while suppressed, so the pose fades back in
	// at the angle you are actually looking rather than the one you left.
	// Reported on change so the watched trait can be confirmed rather than
	// assumed. If nothing is ever printed, the path in the ini does not resolve.
	if (const auto trait = ui::HUDTrait(settings.suppressUITrait, 0.f);
	    !g_state.haveTraitValue || trait != g_state.lastTraitValue) {
		g_state.lastTraitValue = trait;
		g_state.haveTraitValue = true;
		log::Print("UI trait %s = %u", settings.suppressUITrait, UInt32(trait));
	}

	const auto weaponScale = WeaponScale(player);

	if (player != nullptr) {
		const auto trackAlpha = SmoothingAlpha(deltaTime, settings.smoothingSeconds);
		g_state.trackedPose +=
			(PitchBias(player->GetPitch()) * weaponScale - g_state.trackedPose) * trackAlpha;
	}

	if (togglePressed)
		g_state.relaxLatched = !g_state.relaxLatched;

	// Out of combat the latch survives a suppression: the pose still drops for
	// the shot and eases back down afterwards, but the weapon returns to being
	// relaxed rather than back onto the curve. In a fight it is dropped, so the
	// weapon stays up until asked otherwise.
	const auto inCombat = player != nullptr && player->pcInCombat != 0;
	const auto keepLatch = settings.keepRelaxedOutOfCombat && !inCombat;

	// Only the suppressions restart the clock -- firing, reloading, drawing a
	// weapon, a menu, a script taking controls. Looking around deliberately
	// does not: this pose is meant to follow the camera, so cancelling it on
	// camera movement would defeat the point of the mod.
	if (!applying || togglePressed) {
		g_state.idleSeconds = 0.f;

		if (!applying && !keepLatch)
			g_state.relaxLatched = false;
	} else {
		g_state.idleSeconds += deltaTime;
	}

	const auto relaxing = g_state.relaxLatched ||
		(settings.idleRelaxSeconds > 0.f && g_state.idleSeconds >= settings.idleRelaxSeconds);

	// Settling into the relaxed pose is slow and deliberate; coming back out of
	// it, or losing the pose altogether, is a reaction and runs faster.
	const auto slow = SmoothingAlpha(deltaTime, settings.poseBlendSeconds);
	const auto fast = SmoothingAlpha(deltaTime, settings.poseDropSeconds);

	if (applying)
		g_state.poseWeight += (1.f - g_state.poseWeight) * slow;
	else if (settings.keepRelaxedOutOfCombat)
		// Instant: with the latch surviving the shot, the weapon has to be up
		// the moment you pull the trigger, not a fifth of a second later.
		// Coming back down stays as slow as ever.
		g_state.poseWeight = 0.f;
	else
		g_state.poseWeight -= g_state.poseWeight * fast;

	g_state.relaxWeight += ((relaxing ? 1.f : 0.f) - g_state.relaxWeight) * (relaxing ? slow : fast);

	constexpr auto kDegToRad = 0.01745329238f;
	const auto relaxed = settings.relaxedPoseDegrees * kDegToRad * weaponScale;

	// The relaxed pose replaces the curve rather than adding to it, so the tilt
	// is the same however far up or down the camera is pointing.
	const auto pose =
		g_state.trackedPose + (relaxed - g_state.trackedPose) * g_state.relaxWeight;

	g_state.pitchBias = pose * g_state.poseWeight;
}

} // namespace

// Replaces Setting::GetDataPtr at the follow multiplier's only runtime read.
//
// Its sole job beyond driving Update is neutralising the engine's own weapon
// lag, which would otherwise stack with whatever mod is providing inertia. The
// setting itself is left untouched, so everything else in the game still sees
// its real value.
extern "C" float * __fastcall hook_GetSettingValue(Setting *setting, int)
{
	auto *original = ((float *(__thiscall*)(Setting*))g_origGetDataPtr)(setting);

	if ((UInt32)setting != address::kSetting_HandFollowMult)
		return original;

	Update();

	return config::g_settings.neutraliseVanillaLag ? &g_neutralFollowMult : original;
}

// Replace NiMatrix33::SetRotationX at the two sites that rotate the skeleton by
// the lag angle. The signs mirror the engine's own, so the camera keeps
// cancelling what the arms are given.
extern "C" void __fastcall hook_TiltLookingNode(void *matrix, int, float radians)
{
	((void(__thiscall*)(void*, float))g_origSetRotationX)(matrix, radians + g_state.pitchBias);
}

extern "C" void __fastcall hook_TiltCameraNode(void *matrix, int, float radians)
{
	((void(__thiscall*)(void*, float))g_origSetRotationX)(matrix, radians - g_state.pitchBias);
}

//----------------------------------------------------------------------------
// Script interface
//----------------------------------------------------------------------------
//
// Two commands, so a mod that takes over the player's hands can say so instead
// of leaving this plugin to infer it.
//
// Inference was the old approach and it kept running out of road. The engine's
// own "hands busy" tests miss anything driven outside the animation system --
// Realtime Pip-Boy Map raises the pip-boy with kNVSE straight onto the
// controller manager, and no anim group ever learns of it. Reading its state
// meant reading another plugin's private storage, or watching a HUD tile and
// hoping the mod keeps driving it. Being told is neither.
//
// Balanced calls, counted rather than flagged, so two mods overlapping do not
// cancel each other out.

// Called once as the script loads, not when a suppression starts. Without it
// there is no way to tell a script that failed to compile from one that
// compiled and is simply waiting for an event that never comes.
extern "C" bool __cdecl Cmd_AWHScriptLayerReady_Execute(void*, void*, void*, void*,
                                                        void*, void*, double *result,
                                                        UInt32*)
{
	*result = 1.0;

	if (!g_scriptLayerSeen) {
		g_scriptLayerSeen = true;
		log::Print("Script layer connected.");
	}

	return true;
}

extern "C" bool __cdecl Cmd_AWHBeginSuppress_Execute(void*, void*, void*, void*,
                                                     void*, void*, double *result,
                                                     UInt32*)
{
	*result = 1.0;
	log::Print("Suppress raised (%u active).", g_scriptSuppress + 1);
	g_scriptSuppress++;
	g_suppressSeconds = 0.f;
	return true;
}

extern "C" bool __cdecl Cmd_AWHEndSuppress_Execute(void*, void*, void*, void*,
                                                   void*, void*, double *result,
                                                   UInt32*)
{
	*result = 1.0;

	if (g_scriptSuppress != 0)
		g_scriptSuppress--;

	log::Print("Suppress released (%u active).", g_scriptSuppress);
	return true;
}

// Plugin opcode space runs from 0x2000 to 0x8000. This range is not one handed
// out by the NVSE team, it is a sparse corner picked to make a collision
// unlikely, kept clear of the one Player Physics uses. Two slots are claimed.
inline constexpr UInt32 kOpcodeBase = 0x6A10;

// Not const: NVSE writes the assigned opcode into these during registration.
CommandInfo g_cmdScriptLayerReady = {
	"AWHScriptLayerReady", "", 0,
	"tells Adaptive Weapon Handling the script layer is installed",
	0, 0, nullptr, AsCodePtr(Cmd_AWHScriptLayerReady_Execute), nullptr, nullptr, 0
};

CommandInfo g_cmdBeginSuppress = {
	"AWHBeginSuppress", "", 0,
	"tells Adaptive Weapon Handling another mod has taken the player's hands",
	0, 0, nullptr, AsCodePtr(Cmd_AWHBeginSuppress_Execute), nullptr, nullptr, 0
};

CommandInfo g_cmdEndSuppress = {
	"AWHEndSuppress", "", 0,
	"releases a suppression raised by AWHBeginSuppress",
	0, 0, nullptr, AsCodePtr(Cmd_AWHEndSuppress_Execute), nullptr, nullptr, 0
};

//----------------------------------------------------------------------------
// Installation
//----------------------------------------------------------------------------

static void ExpectSameOriginal(const char *name, void *first, void *other)
{
	if (first != other)
		log::Print("  WARNING %s: call sites disagree (%x vs %x); "
		           "another plugin hooked them inconsistently",
		           name, (UInt32)first, (UInt32)other);
}

static bool InstallHooks()
{
	g_origGetDataPtr = patch::CallRel32(
		"ReadFollowMult", address::kCall_ReadFollowMult,
		address::kSetting_GetDataPtr, AsCodePtr(hook_GetSettingValue));

	g_origSetRotationX = patch::CallRel32(
		"TiltLookingNode", address::kCall_TiltLookingNode,
		address::kSetRotationX, AsCodePtr(hook_TiltLookingNode));

	ExpectSameOriginal("NiMatrix33::SetRotationX", g_origSetRotationX, patch::CallRel32(
		"TiltCameraNode", address::kCall_TiltCameraNode,
		address::kSetRotationX, AsCodePtr(hook_TiltCameraNode)));

	return patch::FailureCount() == 0 &&
	       g_origGetDataPtr != nullptr && g_origSetRotationX != nullptr;
}

//----------------------------------------------------------------------------
// NVSE entry points
//----------------------------------------------------------------------------

extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface *nvse,
                                                       PluginInfo *info)
{
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name        = "Adaptive Weapon Handling";
	info->version     = 3;

	if (nvse->isEditor)
		return false;

	paths::Init(nvse->GetRuntimeDirectory());
	log::Open();

	if (nvse->runtimeVersion != kRuntimeVersion_1_4_0_525 &&
	    nvse->runtimeVersion != kRuntimeVersion_1_4_0_525ng) {
		log::Print("Unsupported runtime version %x, expected %x or %x. Not loading.",
		           nvse->runtimeVersion, kRuntimeVersion_1_4_0_525,
		           kRuntimeVersion_1_4_0_525ng);
		log::Close();
		return false;
	}

	return true;
}

extern "C" __declspec(dllexport) bool NVSEPlugin_Load(NVSEInterface *nvse)
{
	config::Load();

	// Registered before the disabled check and before the hooks: the script
	// that drives them has to compile whatever this plugin decides to do, and a
	// missing command is a compile error rather than a quiet false.
	nvse->SetOpcodeBase(kOpcodeBase);

	if (nvse->RegisterCommand(&g_cmdScriptLayerReady) &&
	    nvse->RegisterCommand(&g_cmdBeginSuppress) &&
	    nvse->RegisterCommand(&g_cmdEndSuppress))
		log::Print("Registered the script commands from opcode %x.", kOpcodeBase);
	else
		log::Print("Could not register the script commands; the UI trait and the "
		           "animation tests are all that is left.");

	log::Print("Config: %s", paths::Ini());
	log::Print("Relaxed pose: %u degrees, scancode %x (0 = no hotkey), auto after %u s",
	           UInt32(config::g_settings.relaxedPoseDegrees),
	           config::g_settings.poseToggleKey,
	           UInt32(config::g_settings.idleRelaxSeconds));

	if (!config::g_settings.enabled) {
		log::Print("Disabled via Enabled=0.");
		log::Close();
		return true;
	}

	if (!InstallHooks())
		log::Print("%u patch(es) were skipped -- check for a conflicting plugin.",
		           patch::FailureCount());
	else
		log::Print("All hooks installed.");

	return true;
}
