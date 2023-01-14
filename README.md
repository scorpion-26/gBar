# gBar 
My personal blazingly fast and efficient status bar + widgets, in case anyone finds a use for it.

## Prerequisites 
*If you don't have the optional dependencies, some features are not available and you must manually disable them.*
- wayland
- Hyprland(Optional -> For workspaces widget)
- nvidia-utils(Optional -> For GPU status)
- bluez(Optional -> For Bluetooth status)
- GTK 3.0
- gtk-layer-shell
- PulseAudio server (PipeWire works too!)
- meson, gcc/clang, ninja

## Building and installation
1. Configure with meson
    
    *All optional dependencies enabled*
    ```
    meson build --buildtype=release
    ```
    *All optional dependencies are disabled*
    ```
    meson build --buildtype=release -DHasHyprland=false -DHasNvidia=false -DHasBlueZ=false
    ```
4. Build and install
    ```
    ninja -C build && sudo ninja -C build install
    ```
5. Copy css styling into your config directory($XDG_CONFIG_HOME). This will most likely be ~/.config
    ```
    mkdir ~/.config/gBar && cp css/* ~/.config/gBar/
    ```

## Running gBar
*Open bar on monitor 0*
```
gBar bar 0
```
*Open audio flyin (either on current monitor or on the specified monitor)*
```
gBar audio [monitor]
```
*Open bluetooth widget*
```
gBar bluetooth [monitor]
```

## Gallery
![The bar with default css](/assets/bar.png)

*Bar with default css*

![The audio flyin with default css](/assets/audioflyin.png)

*Audio widget with default css*

![The bluetooth widget with default css](/assets/bt.png)

*Bluetooth widget with default css*

## Features / Widgets
Bar: 
- Workspaces (Hyprland only)
- Time
- Bluetooth (BlueZ only)
- Audio control
- Power control
   - Shutdown
   - Restart
   - Suspend
   - Lock (Requires manual setup, see FAQ)
   - Exit/Logout (Hyprland only)
- CPU stats: Utilisation, temperature (Temperature requires manual setup, see FAQ)
- RAM: Utilisation
- GPU stats (Nvidia only): Utilisation, temperature, VRAM
- Disk: Free/Total

Audio Flyin: 
- Audio control


## FAQ
### There are already many GTK bars out there, why not use them?
- Waybar: 
Great performance, though limited styling(Almost no dynamic sliders, revealers, ...) and (at least for me) buggy css.
- eww: 
Really solid project with many great customization options. There is one problem though: Performance
Due to the way eww configuration is set up, for each dynamic variable (the number of them quickly grows) you need a shell command which opens a process. 
This became quickly a bottleneck, where the bar took up 10% of the CPU-time due to the creation of many processes all the time (without even considering the workspace widget).
gBar implements all of the information gathering(CPU, RAM, GPU, Disk, ...) in native C++ code, which is WAY faster. In fact, gBar was meant to be a fast replacement/alternative for eww for me.

And lastly: Implementing it myself is fun and a great excuse to learn something new!

### What scheme are you using?
The colors are from the Dracula theme: https://draculatheme.com

### I want to customize the colors
If you have SASS installed: Edit ~/.config/gBar/style.scss and regenerate style.css with it

Else: Edit ~/.config/gBar/style.css directly!

### I want to modify the widgets behaviour/Add my own widgets
Unfortunately, you need to implement it yourself in C++. For inspiration look into src/Bar.cpp or src/AudioFlyin.cpp, or open an issue(Maybe I'll implement it for you).

### The Audio widget doesn't open
Delete /tmp/gBar__audio.
This happens, when you kill the widget before it closes automatically after a few seconds.

### CPU Temperature is wrong/Lock doesn't work
*This is caused by the way my system and/or Linux is setup.*

Temperature: Edit the variable ```tempFilePath``` in ```src/System.cpp``` to the correct thermal zone file and recompile. The one for my system is *very* likely wrong.

Lock: There is no generic way to lock a system. So please, implement it to suit your needs (Replace XXX by a shell command in ```src/System.cpp```)

### The icons are not showing!
Please install a Nerd Font from https://www.nerdfonts.com (I use Caskaydia Cove NF), and change style.css/style.scss accordingly (Refer to 'I want to customize the colors' for that)

