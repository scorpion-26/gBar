# gBar 
My personal blazingly fast and efficient status bar + widgets, in case anyone finds a use for it.

*gBar: **G**TK **Bar***

## Prerequisites 
*If you don't have the optional dependencies, some features are not available.*
- wayland
- Hyprland(Optional -> For workspaces widget)
- nvidia-utils(Optional -> For Nvidia GPU status)
- bluez(Optional -> For Bluetooth status)
- GTK 3.0
- gtk-layer-shell
- PulseAudio server (PipeWire works too!)
- meson, gcc/clang, ninja

## Building and installation (Manually)
1. Configure with meson
    
    *All optional dependencies enabled*
    ```
    meson build --buildtype=release
    ```
    *All optional dependencies are disabled*
    ```
    meson build --buildtype=release -DWithHyprland=false -DWithNvidia=false -DWithBlueZ=false
    ```
4. Build and install
    ```
    ninja -C build && sudo ninja -C build install
    ```

## Building and installation (AUR)
For Arch systems, gBar can be found on the AUR.
You can install it e.g.: with yay
```yay -S gbar-git```

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
- Battery: Capacity
- CPU stats: Utilisation, temperature (Temperature requires manual setup, see FAQ)
- RAM: Utilisation
- GPU stats (Nvidia/AMD only): Utilisation, temperature, VRAM
- Disk: Free/Total
- Network: Current upload and download speed

Bluetooth:
 - Scanning of nearby bluetooth devices
 - Pairing and connecting

Audio Flyin: 
- Audio control

## Configuration for your system
Copy the example config (found under data/config) into ~/.config/gBar/config and modify it to your needs.

## Plugins
gBar utilizes a plugin system for custom widgets anyone can create without modifying the source code.
Plugins are native shared-libraries, which need to be placed inside ```~/.local/lib/gBar```, ```/usr/lib/gBar``` or ```/usr/local/lib/gBar```.
Inside example/ there is an example plugin setup. To build and run it, run the following commands inside the example directory:

```
meson setup build -Dprefix=~/.local
```
for the local user
OR 
```
meson setup build
``` 
for all users

```
ninja -C build install
gBar gBarHelloWorld
```
The second argument is the name of the shared library (without 'lib' and '.so').


## FAQ
### There are already many GTK bars out there, why not use them?
- Waybar: 
Great performance, though limited styling(Almost no dynamic sliders, revealers, ...) and (at least for me) buggy css.
- eww: 
Really solid project with many great customization options. There is one problem though: Performance.\
Due to the way eww configuration is set up, for each dynamic variable (the number of them quickly grows) you need a shell command which opens a process. 
This became quickly a bottleneck, where the bar took up 10% of the CPU-time due to the creation of many processes all the time (without even considering the workspace widget).
gBar implements all of the information gathering(CPU, RAM, GPU, Disk, ...) in native C++ code, which is WAY faster. In fact, gBar was meant to be a fast replacement/alternative for eww for me.

And lastly: Implementing it myself is fun and a great excuse to learn something new!

### What scheme are you using?
The colors are from the Dracula theme: https://draculatheme.com

### I want to customize the colors
First, find where the data is located for gBar. Possible locations: 
 - /usr/share/gBar
 - /usr/local/share/gBar
 - ~/.local/share/gBar
 - If you cloned this repository locally: Inside css/

 Copy the scss and css files from within the data direction into ~/.config/gBar. e.g.:
 ```
 mkdir ~/.config/gBar/
 cp /usr/local/share/gBar/* ~/.config/gBar/
 ```
 This will override the default behaviour. If you have sass installed, you can modify the scss file and then regenerate the css file accordingly. Else modify the css file directly.


### The Audio widget doesn't open
Delete /tmp/gBar__audio.
This happens, when you kill the widget before it closes automatically after a few seconds.

### CPU Temperature is wrong / Lock doesn't work / Exiting WM does not work
See *Configuration for your system*

### The icons are not showing!
Please install a Nerd Font from https://www.nerdfonts.com (I use Caskaydia Cove NF), and change style.css/style.scss accordingly (Refer to 'I want to customize the colors' for that)

