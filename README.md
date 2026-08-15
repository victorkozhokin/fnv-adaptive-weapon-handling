# fnv-adaptive-weapon-handling

NVSE plugin that retunes first person weapon lag from the camera pitch. It is a
reimplementation of the quest-script version of Adaptive Weapon Handling.

Requires **FalloutNV.exe 1.4.0.525** (Steam or GOG) and xNVSE. The plugin refuses
to load on any other build, because every hook address is hardcoded.

## Installing

`AdaptiveWeaponHandling.dll` goes into `Fallout New Vegas/Data/NVSE/Plugins/`.
`AWHConfig.ini` goes into `Fallout New Vegas/Data/Config/AWHConfig/`, which the
plugin creates on first run if missing. `AWHConfig.log` is written next to it.

**Remove the script version first.** Both adjust the same two settings and will
fight each other.

## What it does

Tilts the first person weapon by camera pitch. The shape is a curve in the ini,
so the weapon can rest level at the horizon and lower progressively as the view
leaves it, or anything else that is a function of pitch. It holds still when the
camera does. This is the default, and it is always on.

On top of that sits a **relaxed pose**: one fixed tilt held whatever the camera
is doing, replacing the curve rather than adding to it. It comes on with a
hotkey (`Z` by default) or on its own after `IdleRelaxSeconds` of standing
around, and comes back off the moment the player does anything -- firing,
reloading, drawing, opening a menu, or having controls taken by a script.
Looking around does not count, deliberately: the pose is built to follow the
camera, so cancelling it on camera movement would defeat the point. Settling
into it is slow (`PoseBlendSeconds`); leaving it is faster (`PoseDropSeconds`),
because raising a weapon is a reaction.

Motion-dependent inertia is deliberately absent -- B42 Inertia and friends do
that job. The engine's own weapon lag is pinned to neutral by default so the two
do not stack.

## How it works

The engine has no setting for a pose. Its first person weapon lag reacts to how
fast the camera moves and decays to nothing once it stops, so no combination of
`fFirstPersonHandFollowMult` and `fFirstPersonHandChaseSeconds` can hold the
weapon anywhere. What it does have is a single angle, computed in
`PlayerCharacter`'s first person hand update at `0x952290`, that becomes a
rotation of the skeleton. Biasing that angle is exactly a pose.

The angle reaches the skeleton twice, with opposite signs:

- `0x952602` rotates **Bip01 Looking** by `+angle`. The arms hang off it.
- `0x94B1A4` rotates **Camera1st** by `-angle`. That node is a child of
  Bip01 Looking, so the negative cancels the positive and the view stays put
  while the arms trail behind.

Both are hooked. Biasing only the first tilts the whole view along with the
arms, because the cancellation is then computed from the unbiased angle.

A third hook sits on the follow multiplier's only runtime read, at `0x9522A6`.
It drives the once-per-frame update and, when `NeutraliseVanillaLag` is set,
substitutes 1.0 -- the engine's own no-lag value, since hand rotation is scaled
by `multiplier - 1` (`0x9525A8`). The setting itself is never written, so
everything else in the game still sees its real value.

## History

This replaces a quest script that rewrote `fFirstPersonHandFollowMult` and
`fFirstPersonHandChaseSeconds` globally, ~31 times a second, from a 22-branch
piecewise curve over the camera pitch. That approach had a few problems worth
recording:

- The settings are global. Other consumers and mods saw the rewritten values,
  and whatever was written last stayed behind.
- `fFirstPersonHandChaseSecondsAttack` exists. The engine substitutes it while
  attacking and the script never touched it, so the tuning silently stopped
  applying whenever you fired.
- Polling on a fixed 31 Hz timer stair-steps against the framerate.
- The ini was read once and the "already initialised" flag lived in a quest
  variable, which persists in the save -- so editing the ini did nothing on an
  existing game.

The lag curve itself was eventually dropped rather than ported forward: it
described how much the weapon should trail *while moving*, which is a different
quantity from where it should rest, and its sharp peak just above the horizon
read as a jump rather than as weight.

## Building

```sh
brew install lld       # brings llvm, for lld-link and llvm-dlltool
./scripts/build.sh     # any clang will do for compiling, Apple's included
```

The plugin links without a C runtime and without the Windows SDK; the kernel32
imports are declared by hand in `src/util/win32.h`.

## License

GPL-3.0. See `LICENSE`.
