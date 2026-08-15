#include "config.h"
#include "util/log.h"
#include "util/paths.h"
#include "util/win32.h"

namespace config {

Settings g_settings;

namespace {

constexpr auto kSection = "Settings";

// Minimal decimal parser -- enough for the forms an ini file holds, without
// dragging in strtod.
// Returns the first character after the number, or null if there was none.
const char *ParseFloat(const char *text, float *out)
{
	while (*text == ' ' || *text == '\t')
		text++;

	auto sign = 1.f;

	if (*text == '-' || *text == '+')
		sign = *text++ == '-' ? -1.f : 1.f;

	auto seenDigit = false;
	auto value = 0.f;

	for (; *text >= '0' && *text <= '9'; text++) {
		value = value * 10.f + float(*text - '0');
		seenDigit = true;
	}

	if (*text == '.') {
		text++;

		for (auto scale = .1f; *text >= '0' && *text <= '9'; text++, scale *= .1f) {
			value += float(*text - '0') * scale;
			seenDigit = true;
		}
	}

	if (!seenDigit)
		return nullptr;

	*out = sign * value;
	return text;
}

float ReadFloat(const char *key, float fallback)
{
	char buffer[64];

	if (GetPrivateProfileStringA(kSection, key, "", buffer, sizeof(buffer), paths::Ini()) == 0)
		return fallback;

	float value;

	if (ParseFloat(buffer, &value) == nullptr) {
		log::Print("  bad value for %s, using the default", key);
		return fallback;
	}

	return value;
}

bool ReadBool(const char *key, bool fallback)
{
	char buffer[16];

	if (GetPrivateProfileStringA(kSection, key, "", buffer, sizeof(buffer), paths::Ini()) == 0)
		return fallback;

	return buffer[0] == '1' || buffer[0] == 't' || buffer[0] == 'T';
}

// Parses "pitch:degrees, pitch:degrees, ...". Entries must ascend by pitch;
// anything malformed drops the whole curve so the scalars stay in charge.
UInt32 ParsePoseCurve(const char *text, curve::Node *out, UInt32 capacity)
{
	UInt32 count = 0;

	while (*text != '\0') {
		while (*text == ' ' || *text == '\t' || *text == ',')
			text++;

		if (*text == '\0')
			break;

		if (count == capacity) {
			log::Print("  PoseCurve: more than %u points, ignoring the curve", capacity);
			return 0;
		}

		float pitch;
		const auto *afterPitch = ParseFloat(text, &pitch);

		if (afterPitch == nullptr || *afterPitch != ':') {
			log::Print("  PoseCurve: expected 'pitch:degrees', ignoring the curve");
			return 0;
		}

		float degrees;
		const auto *afterDegrees = ParseFloat(afterPitch + 1, &degrees);

		if (afterDegrees == nullptr) {
			log::Print("  PoseCurve: missing the degrees of a point, ignoring the curve");
			return 0;
		}

		if (count > 0 && pitch <= out[count - 1].pitch) {
			log::Print("  PoseCurve: points must ascend by pitch, ignoring the curve");
			return 0;
		}

		out[count++] = {pitch, degrees};
		text = afterDegrees;
	}

	if (count == 1) {
		log::Print("  PoseCurve: a single point is not a curve, ignoring it");
		return 0;
	}

	return count;
}

// Parses "8, 9" into a bit per listed number. Anything malformed, or out of the
// 0..31 a mask can hold, leaves the existing mask alone rather than silently
// dropping weapon types out of it.
UInt32 ParseTypeMask(const char *text, UInt32 fallback)
{
	UInt32 mask = 0;

	while (*text != '\0') {
		while (*text == ' ' || *text == '\t' || *text == ',')
			text++;

		if (*text == '\0')
			break;

		float value;
		const auto *after = ParseFloat(text, &value);

		if (after == nullptr || value < 0.f || value > 31.f) {
			log::Print("  HeavyWeaponTypes: expected numbers 0-31, keeping the default");
			return fallback;
		}

		mask |= 1u << UInt32(value);
		text = after;
	}

	return mask;
}

// Last-write time of the ini as of the last successful read, so the poll below
// can tell a real edit from a file that simply exists.
UInt32 g_stampLow  = 0;
UInt32 g_stampHigh = 0;

void RecordStamp()
{
	FileAttributeData info;

	if (GetFileAttributesExA(paths::Ini(), kGetFileExInfoStandard, &info) == 0)
		return;

	g_stampLow  = info.lastWriteLow;
	g_stampHigh = info.lastWriteHigh;
}

} // namespace

void Load()
{
	auto &s = g_settings;

	s.enabled          = ReadBool ("Enabled",          s.enabled);
	s.disableInCombat  = ReadBool ("DisableInCombat",  s.disableInCombat);
	s.requireWeaponOut = ReadBool ("RequireWeaponOut", s.requireWeaponOut);
	s.smoothingSeconds = ReadFloat("SmoothingSeconds", s.smoothingSeconds);
	s.poseBlendSeconds = ReadFloat("PoseBlendSeconds", s.poseBlendSeconds);
	s.poseDropSeconds  = ReadFloat("PoseDropSeconds",  s.poseDropSeconds);

	s.suppressWhenHandsBusy = ReadBool("SuppressWhenHandsBusy", s.suppressWhenHandsBusy);
	s.suppressInMenus       = ReadBool("SuppressInMenus",       s.suppressInMenus);

	s.suppressWhenControlsDisabled =
		ReadBool("SuppressWhenControlsDisabled", s.suppressWhenControlsDisabled);

	if (const auto mask = ReadFloat("ControlSuppressMask", float(s.controlSuppressMask));
	    mask >= 0.f && mask <= 255.f)
		s.controlSuppressMask = UInt32(mask);

	GetPrivateProfileStringA(kSection, "SuppressUITrait", s.suppressUITrait,
	                         s.suppressUITrait, Settings::kMaxTraitPath, paths::Ini());

	s.neutraliseVanillaLag = ReadBool ("NeutraliseVanillaLag", s.neutraliseVanillaLag);

	s.fallbackTiltDegrees     = ReadFloat("FallbackTiltDegrees",     s.fallbackTiltDegrees);
	s.fallbackLeanUpDegrees   = ReadFloat("FallbackLeanUpDegrees",   s.fallbackLeanUpDegrees);
	s.fallbackLeanDownDegrees = ReadFloat("FallbackLeanDownDegrees", s.fallbackLeanDownDegrees);

	s.poseCurveScale     = ReadFloat("PoseCurveScale",     s.poseCurveScale);
	s.relaxedPoseDegrees = ReadFloat("RelaxedPoseDegrees", s.relaxedPoseDegrees);
	s.heavyWeaponScale   = ReadFloat("HeavyWeaponScale",   s.heavyWeaponScale);

	{
		char buffer[64];

		if (GetPrivateProfileStringA(kSection, "HeavyWeaponTypes", "", buffer,
		                             sizeof(buffer), paths::Ini()) != 0)
			s.heavyWeaponTypes = ParseTypeMask(buffer, s.heavyWeaponTypes);
	}

	if (const auto key = ReadFloat("RelaxedPoseToggleKey", float(s.poseToggleKey));
	    key >= 0.f && key <= 255.f)
		s.poseToggleKey = UInt32(key);

	s.keepRelaxedOutOfCombat =
		ReadBool("KeepRelaxedOutOfCombat", s.keepRelaxedOutOfCombat);

	s.idleRelaxSeconds = ReadFloat("IdleRelaxSeconds", s.idleRelaxSeconds);

	{
		char buffer[256];
		GetPrivateProfileStringA(kSection, "PoseCurve", "", buffer, sizeof(buffer),
		                         paths::Ini());
		s.poseCurveCount =
			ParsePoseCurve(buffer, s.poseCurve, Settings::kMaxPoseNodes);

		if (s.poseCurveCount != 0)
			log::Print("  PoseCurve: %u points, overriding the Fallback* settings",
			           s.poseCurveCount);
	}

	if (s.smoothingSeconds < 0.f)
		s.smoothingSeconds = 0.f;

	if (s.poseBlendSeconds < 0.f)
		s.poseBlendSeconds = 0.f;

	if (s.poseDropSeconds < 0.f)
		s.poseDropSeconds = 0.f;

	if (s.idleRelaxSeconds < 0.f)
		s.idleRelaxSeconds = 0.f;

	RecordStamp();
}

bool ReloadIfChanged()
{
	FileAttributeData info;

	if (GetFileAttributesExA(paths::Ini(), kGetFileExInfoStandard, &info) == 0)
		return false;

	if (info.lastWriteLow == g_stampLow && info.lastWriteHigh == g_stampHigh)
		return false;

	Load();
	return true;
}

} // namespace config
