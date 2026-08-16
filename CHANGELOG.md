# Changelog

## 1.0 — first plugin release

Rewritten from the ground up as an NVSE plugin. The script version drove two
engine settings; this hooks the first person hand angle itself, which is the
difference between nudging how the weapon lags and telling it where to be.

- **The weapon can now be held.** `fFirstPersonHandFollowMult` and
  `fFirstPersonHandChaseSeconds` only ever shaped a reaction that decays to
  nothing once the camera stops, so no setting could hold a pose. The plugin
  biases the angle instead, and a bias is a pose.
- **Tilt follows a curve over camera pitch**, written in the ini. Level at the
  horizon and lowering as the view leaves it by default; any shape is allowed.
- **Relaxed pose** — one fixed angle held whatever the camera does. Hotkey `Z`,
  or on its own after ten seconds of standing around.
- Leaves the pose the moment you do anything: firing, reloading, drawing, menus,
  or a script taking your controls. Looking around deliberately does not count.
- **Heavy weapons tilt less** — animation types 8 and 9, Handle and Launcher.
- **Stands down when another mod has your hands.** kNVSE's `ActivateAnim` runs
  outside the animation system entirely, so nothing can detect it; mods announce
  themselves instead through an esp-less script layer. Realtime Pip-Boy and
  B42 Interact are handled out of the box.
- **MCM menu**, with optional Russian text.
- Vanilla weapon lag pinned to neutral by default, so this and B42 Inertia stack
  rather than fight.

Requires FalloutNV.exe 1.4.0.525. Every hook address is hardcoded and the
plugin refuses to load on any other build rather than crash later.

**Remove the script version before installing.** Both adjust the same settings
and will fight each other.
