# Changelog

## 1.0 — first plugin release

* Rewritten as an NVSE plugin — hooks the first person hand angle directly instead of driving two engine settings
* The weapon can be held at an angle now; the old settings only shaped a reaction that decayed to nothing once the camera stopped
* Tilt follows a curve over camera pitch, editable in the ini
* Relaxed pose on a hotkey (Z), or on its own after ten seconds of standing around
* Pose drops instantly on firing, reloading, drawing, menus, or a script taking your controls
* Looking around does not cancel it — deliberately, since the pose exists to follow the camera
* Heavy weapons tilt less — animation types 8 and 9, Handle and Launcher
* Stands down when another mod takes your hands, through an ESPless script layer; Realtime Pip-Boy and B42 Interact work out of the box
* Vanilla weapon lag pinned to neutral, so this and B42 Inertia stack rather than fight
* MCM menu, with optional Russian text
* Requires FalloutNV.exe 1.4.0.525 — refuses to load on any other build rather than crash later
* Remove the script version before installing; both adjust the same settings and will fight each other
