#pragma once

// Reading a value out of the game's UI, which is the only place some mods put
// their state.
//
// Realtime Pip-Boy Map is the case this exists for. It raises the pip-boy with
// kNVSE's ActivateAnim, straight onto the controller manager, so no animation
// group is ever set and nothing in AnimData knows it happened. Its own state
// lives in a JIP auxiliary variable, which is another plugin's private storage.
// What it does leave in reach is the HUD: it drives several tile traits while
// the map is up, and the menu tile tree is engine memory like any other.
//
// Layouts and the one address below are from xNVSE's GameTiles.h / GameUI.cpp.

#include "game/types.h"

namespace ui {

// TileMenu ***, indexed by menu type minus kMenuTypeMin.
inline constexpr UInt32 kTileMenuArray = 0x11F350C;
inline constexpr UInt32 kMenuTypeMin   = 0x3E9;
inline constexpr UInt32 kMenu_HUDMain  = 1004;

// UInt32 __cdecl TraitNameToID(const char *traitName)
//   Resolves a trait name, including the custom ones an xml declares, to the id
//   the tile stores it under.
inline constexpr UInt32 kTraitNameToID = 0xA01860;

struct Tile;

struct TileValue {
	UInt32 id;      // 00
	Tile  *parent;  // 04
	float  num;     // 08
};

struct ChildNode {
	ChildNode *next;  // 00
	ChildNode *prev;  // 04
	Tile      *child; // 08
};

// One link of the tList the children hang off; its head sits inside Tile.
struct ChildList {
	ChildNode *data; // 00
	ChildList *next; // 04
};

struct Tile {
	void      *vtbl;        // 00
	ChildList  childList;   // 04
	UInt32     unk0C;       // 0C
	void      *valuesVtbl;  // 10
	TileValue **values;     // 14
	UInt32     valueCount;  // 18
	UInt32     valueAlloc;  // 1C
	const char *name;       // 20  String::m_data
};

static_assert(__builtin_offsetof(Tile, values)     == 0x14);
static_assert(__builtin_offsetof(Tile, valueCount) == 0x18);
static_assert(__builtin_offsetof(Tile, name)       == 0x20);

inline bool NameMatches(const char *name, const char *text, UInt32 length)
{
	if (name == nullptr)
		return false;

	for (UInt32 i = 0; i < length; i++)
		if (name[i] != text[i] || name[i] == '\0')
			return false;

	return name[length] == '\0';
}

inline Tile *MenuTile(UInt32 menuType)
{
	auto **array = *(Tile***)kTileMenuArray;

	if (array == nullptr || menuType < kMenuTypeMin)
		return nullptr;

	return array[menuType - kMenuTypeMin];
}

// `length` rather than a terminator so a path can be walked in place.
inline Tile *Child(const Tile *tile, const char *name, UInt32 length)
{
	if (tile == nullptr)
		return nullptr;

	for (const auto *node = &tile->childList; node != nullptr; node = node->next) {
		const auto *entry = node->data;

		if (entry != nullptr && entry->child != nullptr &&
		    NameMatches(entry->child->name, name, length))
			return entry->child;
	}

	return nullptr;
}

// The tile's values are kept sorted by id, and the engine binary searches them.
inline const TileValue *Trait(const Tile *tile, const char *traitName)
{
	if (tile == nullptr || tile->values == nullptr)
		return nullptr;

	const auto id = CdeclCall<UInt32>(kTraitNameToID, traitName);

	for (UInt32 low = 0, high = tile->valueCount; low < high; ) {
		const auto middle = (low + high) / 2;
		const auto *value = tile->values[middle];

		if (value == nullptr)
			return nullptr;

		if (value->id == id)
			return value;

		if (value->id < id)
			low = middle + 1;
		else
			high = middle;
	}

	return nullptr;
}

// Reads "Child\Child\_traitName" under HUDMainMenu, returning `fallback` when
// any part of the path is missing.
//
// Missing has to read as "not there" rather than as zero: a mod whose menu is
// not installed must not be able to hold a suppression on for ever.
inline float HUDTrait(const char *path, float fallback)
{
	if (path == nullptr || *path == '\0')
		return fallback;

	auto *tile = MenuTile(kMenu_HUDMain);

	const char *segment = path;

	for (const char *p = path; ; p++) {
		if (*p != '\\' && *p != '/' && *p != '\0')
			continue;

		if (*p == '\0') {
			// The last segment is the trait, and it is already terminated.
			const auto *value = Trait(tile, segment);
			return value != nullptr ? value->num : fallback;
		}

		tile = Child(tile, segment, UInt32(p - segment));

		if (tile == nullptr)
			return fallback;

		segment = p + 1;
	}
}

} // namespace ui
