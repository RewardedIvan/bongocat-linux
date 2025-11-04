# bongocat linux

## recommended hyprland config

```ini
windowrulev2 = noblur,title:^(bongocatl)$
windowrulev2 = noborder,title:^(bongocatl)$
windowrulev2 = noshadow,title:^(bongocatl)$
windowrulev2 = nodim,title:^(bongocatl)$
windowrulev2 = noanim,title:^(bongocatl)$
windowrulev2 = pin,title:^(bongocatl)$
exec-once = bongocat
```

## my bongocat config
```ini
dark_mode = 1
alt_mouth = 0
flipped = 1
rotation = 15.000000
scale = 0.50000
paw_hold_ns = 50000000
window_width = 550
window_height = 540
```

## config

the config file is at `~/.config/bongocatl/cat.conf`  
it will get created if it doesn't exist.  
please don't change the order because it will break and rewrite the entire file.  
you can edit it while bongocat is open and it will change immediately.

## build

requirements:
- hyprland headers
- cmake
- a c and c++ (23) compiler

```bash
git clone --recursive https://github.com/rewardedivan/bongocat-linux.git
cd bongocat-linux
mkdir build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
your binary should be in `build/bongocat`  

install:

```bash
sudo cmake --install build
```

you can manually un/load the plugin with

```bash
hyprctl plugin load $PWD/build/libhypr_bongocat.so
hyprctl plugin unload $PWD/build/libhypr_bongocat.so
```

but if you're going to be using the release build

```bash
hyprpm add https://github.com/rewardedivan/bongocat-linux
hyprpm enable hypr_bongocat
```

hyprland wiki page on plugin dev: https://wiki.hypr.land/Plugins/Development/Getting-Started/
