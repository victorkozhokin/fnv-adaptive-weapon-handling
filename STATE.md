# Adaptive Weapon Handling — where things stand

Written so the state survives a fresh session. The README explains the hooks in
detail; this is what a newcomer needs that the README does not say.

Latest build on the share as `AdaptiveWeaponHandling-v11.zip`. Ready for
release; `CHANGELOG.md` is the 1.0 copy.

## The idea in one paragraph

The engine has no setting for a weapon pose. Its first person weapon lag reacts
to how fast the camera moves and decays to nothing once it stops, so no
combination of `fFirstPersonHandFollowMult` and `fFirstPersonHandChaseSeconds`
can hold a weapon anywhere — which is the ceiling the old quest-script version
hit. What the engine does have is a single angle, computed in the first person
hand update, that becomes a rotation of the skeleton. Biasing that angle is
exactly a pose.

## The thing that took longest

The angle reaches the skeleton **twice, with opposite signs**:

- `0x952602` rotates **Bip01 Looking** by `+angle` — the arms hang off it
- `0x94B1A4` rotates **Camera1st** by `-angle` — a child of Bip01 Looking, so
  the negative cancels the positive and the view stays put while the arms trail

Both must be hooked. Biasing only the first tilts the whole view along with the
arms, because the cancellation is then computed from the unbiased angle.

## Weapon type

Heavy weapons scale their tilt down, and the pair is animation types **8 and 9**
— Handle and Launcher, so miniguns and gatling lasers on one side, missile
launchers and bazookas on the other. Confirmed in play.

Getting there was not obvious: reading the type from the animation data does not
work, because the anim groups those mods play are not what carries it. It is
read from the equipped weapon form instead — vtable entry `0x52`
(`GetWeaponInfo`), then the form pointer at `+0x08`, then `eWeaponType` at
`+0xF4`, with a form-type check in between.

## Suppression

The pose stands down when something else owns the hands. The engine's own tests
cover firing, reloading, melee and idles and are asked directly.

What they cannot see is a mod driving the arms **outside** the animation system:
kNVSE's `ActivateAnim` runs a sequence straight on the controller manager and no
anim group ever learns of it. Being told is the only reliable answer, which is
what `nvse/Plugins/Scripts/ln_AdaptiveWeaponHandling.txt` is for — Realtime
Pip-Boy and B42 Interact announce themselves there.

Calls are counted, not flagged, so two mods overlapping cannot cancel each other
out. Anything still raised after thirty seconds is released anyway.

The script file is optional: delete it and the plugin falls back to
`SuppressUITrait` and the engine tests.

## Also worth knowing

- Motion-dependent inertia is **deliberately absent**. B42 Inertia does that
  job. The engine's own weapon lag is pinned to neutral by default so the two
  do not stack.
- The relaxed pose is not cancelled by looking around, on purpose. The pose
  exists to follow the camera, so cancelling it on camera movement would defeat
  the point. Every other action does cancel it.
- Requires **FalloutNV.exe 1.4.0.525**; every hook address is hardcoded and the
  plugin refuses to load elsewhere rather than crash later.

## Open

- Nothing outstanding. The last fix was reading the weapon type from the
  equipped form rather than from animation data.
