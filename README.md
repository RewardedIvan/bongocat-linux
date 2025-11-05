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

## config

the config file is at `~/.config/bongocatl/cat.conf`  
it will get created if it doesn't exist.  
please don't change the (line) order because it will break and override the entire file with defaults.  
you can edit it while bongocat is open and it will change immediately.  
you can use `font_path = ` for raylib's default font.
`clicks_color` is a hex formatted 32-bit ABGR color.  
`clicks_horizontal_alignment` is either:
- `0` for left (digits grow to the right)
- `1` for center (digits grow towards the sides)
- `2` for right (digits grow to the left)
- and anything else will have weird behavior.

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
