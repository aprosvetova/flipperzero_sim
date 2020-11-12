# Flipper UI Simulator

This is a very early prototype of a simulator to compile and run Flipper apps or games as native Windows or Linux applications.

Note that the `Flipper API` currently used does not yet match the API of the real firmware.

# Examples
Snake

<img src="./gif/flipperzero-snake.gif" alt="snake" width="300"/> 

Tetris

<img src="./gif/flipperzero-tetris.gif" alt="snake" width="150"/>

# Compiling
## Linux 

Debian/Ubuntu:
```bash
apt install libsdl2-dev libsdl2-image-dev
mkdir build
cd build
cmake ..
make
```

## Windows

First, install vcpkg. See ```https://github.com/microsoft/vcpkg```

Use vcpkg to install SDL2 and SDL2-image libraries (x64):
```
vcpkg.exe install sdl2:x64-windows sdl2-image:x64-windows
```

Set `%VCPKG_ROOT%` to point to the vcpkg directory.

Generate Visual Studio project with CMake:
```
mkdir build
cd build
cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ..
```

Or, generate Ninja project with CMake:
```
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ..
```    