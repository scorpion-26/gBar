---
name: Bug report
about: Found an issue with gBar?
title: ''
labels: bug
assignees: ''

---

*Please fill out this form and delete the defaults(everything not in bold font), so I can help you better*
**Describe the bug**

What doesn't work? Please be as precise as possible.
e.g.: "gBar crashes on hovering the RAM sensor module. No other modules are affected."

**Steps to Reproduce**

What did you do, that triggered the issue? 
e.g.:
1. Open the bar
2. Hover over the RAM sensor module
3. gBar crashes

**Expected behavior**

What do you expect should happen?
e.g.: gBar shouldn't crash, but stay open

**Screenshots/Error logs**

Please include as much information about the crash/bug as possible.
 - If it is a visual glitch, please include a screenshot/video as well as any modifications to the css (if any).
 - If it is a *crash* (e.g. ```Segmentation fault (Core dumped)``` in the console), please include the log (```/tmp/gBar-<pid>.log``` or from the console) and a crash stack trace. They can e.g. be found via journalctl (```journalctl --boot=0 --reverse```) if systemd-coredump is used.
 - If it is an *assert failed* (Last line in the log reads ```[Exiting due to assert failed]```) please include the log (```/tmp/gBar-<pid>.log``` or from the console).
 - The config (```~/.config/gBar/config```) used is always valuable to diagnose an issue, so please include it.

**Information about your system and gBar**
 - OS: [e.g. Arch, Ubuntu, ...]
 - Desktop environment [e.g. Hyprland, Sway, ...]
 - commit sha256 if possible [e.g. 16e7d8aa50ddebeffb2509d7c0428b38cad565f8]
