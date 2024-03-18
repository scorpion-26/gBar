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
- pamixer
- libsass
- meson, gcc/clang, ninja

## Building and installation (Manually)
1. Clone gBar
    ```
    git clone https://github.com/scorpion-26/gBar
    ```
2. Configure with meson
    
    *All optional dependencies enabled*
    ```
    meson setup build
    ```
3. Build and install
    ```
    ninja -C build && sudo ninja -C build install
    ```

## Building and installation (AUR)
For Arch systems, gBar can be found on the AUR.
You can install it e.g.: with yay
```yay -S gbar-git```

## Building and installation (Nix)
If you choose the Nix/NixOS installation there are a couple of ways to do it but they all require you to have flakes enabled.
- Building it seperately and just running the binary, run nix build in the directory and use the binary from ./result/bin
- Import the flake to inputs and then add `gBar.defaultPackage.x86_64-linux` to either environment.systemPackages or home.packages.
- Use the home manager module. This is done by, as in the previous way, importing the flake and then adding `gBar.homeManagerModules.x86_64-linux.default` into your home-manager imorts section. This exposes the option programs.gBar to home-manager, use it like below.
```nix
# Inputs section
inputs.gBar.url = "github:scorpion-26/gBar";
...
# Inside home config
home-manager.users.user = {
    ...
    imports = [ inputs.gBar.homeManagerModules.x86_64-linux.default ];
    ...
    programs.gBar = {
        enable = true;
        config = {
            Location = "L";
            EnableSNI = true;
            SNIIconSize = {
                Discord = 26;
                OBS = 23;
            };
            WorkspaceSymbols = [ " " " " ];
        };
    };
};
```

## Running gBar
*Open bar on monitor "DP-1"*
```
gBar bar DP-1
```
*Open bar on monitor 0 (Legacy way of specifying the monitor)*
```
gBar bar 0
```
*Open audio flyin (either on current monitor or on the specified monitor)*
```
gBar audio [monitor]
```
*Open microphone flyin, this is equivalent to the audio flyin*
```
gBar mic [monitor]
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
- Workspaces (Hyprland only. Technically works on all compositors implementing ext_workspace when ```UseHyprlandIPC``` is false, though workspace control relies on Hyprland)
- Time
- Title of the focused Window
- Bluetooth (BlueZ only)
- Audio control
- Microphone control
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
- Update checking (Non-Arch systems need to be configured manually)
- Tray icons

Bluetooth:
 - Scanning of nearby bluetooth devices
 - Pairing and connecting

Audio Flyin: 
- Audio control
- Microphone control

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

For more examples on how to use the gBar API, you can have a look at the built-in widgets (AudioFlyin.cpp, BluetoothDevices.cpp, Bar.cpp) as they use the same API.

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

### Can you implement feature XYZ? / I've found a bug. Can you fix it?
This project is meant to be for my personal use, though I want it to be easily used by others without bugs or a complicated setup. This means the following:
 -  If you found a bug, please [open an issue](https://github.com/scorpion-26/gBar/issues/new/choose) and I'll try to fix it as quickly as I can.
 -  If you're missing a particular feature, please [open an issue](https://github.com/scorpion-26/gBar/issues/new/choose) as well and I'll see what I can do, although I can't guarantee anything. Small requests or features I'll find useful too will probably be implemented in a timely fashion though.


### What scheme are you using?
The colors are from the Dracula theme: https://draculatheme.com

### I want to customize the colors
First, find where the data is located for gBar. Possible locations: 
 - In a 'gBar' directory found in any of the directories listed by `echo $XDG_DATA_DIRS`
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

### The margins of the bar are inconsistent/messed up / Custom CSS broke.
If you have a custom style.[s]css, make sure that the margins/names/... are the same as the ones found in ```style/style.[s]css```.\
*As of commit f78758c margins are no longer used in the default css. If you didn't play around with margins, you can safely remove them from your css*\
If you've checked the css against upstream gBar and the issue persists, please [open an issue](https://github.com/scorpion-26/gBar/issues/new/choose).

### The Audio/Bluetooth widget doesn't open
Delete ```/tmp/gBar__audio```/```/tmp/gBar__bluetooth```.
This happens, when you kill the widget before it closes properly (Automatically after a few seconds for the audio widget, or the close button for the bluetooth widget). Ctrl-C in the terminal (SIGINT) is fine though.

### CPU Temperature is wrong / Lock doesn't work / Exiting WM does not work
See *Configuration for your system*

### The icons are not showing!
Please install a Nerd Font from https://www.nerdfonts.com (I use Caskaydia Cove NF), and change style.css/style.scss accordingly (Refer to 'I want to customize the colors' for that). You _will_ need a Nerd Font with version 2.3.0 or newer (For more details see [this comment](https://github.com/scorpion-26/gBar/issues/5#issuecomment-1442037005))

### The tray doesn't show
Some apps sometimes don't actively query for tray applications. A fix for this is to start gBar before the tray app
If it still doesn't show, please open an issue with your application.
The tray icons are confirmed to work with Discord, Telegram, OBS, Steam and KeePassXC

### Clicking on the tray opens a glitchy transparent menu
~This is semi-intentional and a known bug (See https://github.com/scorpion-26/gBar/pull/12#issuecomment-1529143790 for an explanation). You can make it opaque by setting the background-color property of .popup in style.css/style.scss~\
As of [bc0281c](https://github.com/scorpion-26/gBar/commit/bc0281ca5321cb6e72ab6d295c790ae10d7eec7e) this is now fixed! For things to look properly you may want to update your css (Specifically the selectors ```.popup``` and ```menu```)
