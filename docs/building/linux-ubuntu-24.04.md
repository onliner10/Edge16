# Build Instructions under Ubuntu 24.04

Last tested with Ubuntu 24.04 LTS in March 2026.

The document here is meant to help you develop or test changes to Edge16 on your PC, not to build flight/radio safe version of binaries.

- [Setting up the build environment for Edge16](#setting-up-the-build-environment-for-edge16)
- [Building Edge16 firmware for the radio](#building-edge16-firmware-for-the-radio)
- [Building the simulator and radio simulator module](#building-the-simulator-and-radio-simulator-module)

## Setting up the build environment for Edge16

You can setup Ubuntu 24.04 on bare-metal, inside a virtual machine environment, or using WSL2 under Windows 10/11.

* Download [Ubuntu 24.04](https://ubuntu.com/download/desktop) and install it (using Minimal installation type is sufficient. Allow _Download updates while installing Ubuntu_. 3rd party software is not required, unless you need this for graphics or WiFi adapter on your PC).
* When the installer has finished and the obligatory reboot is done, log in. Install updates using Software Updater (click _Activities_ in top left corner, type in _Software Updater_ and press _Enter_). **Restart** the PC and log in again after reboot.
* To make setting up the build environment as easy as possible, we created a shell script that includes all the necessary commands. In the next steps, we download it, make it executable and run it. Active Internet connection is required for the script to be able to download the required packages for installation. Start, by opening a terminal window (click _Activities_ in the top left corner, type _terminal_ and press _Enter_). Enter the following 3 lines, each line at a time and enter your password (with sudo rights) if asked:
```
wget https://raw.githubusercontent.com/onliner10/Edge16/main/tools/setup_buildenv_ubuntu24.04.sh
```
```
chmod +x setup_buildenv_ubuntu24.04.sh
```
```
./setup_buildenv_ubuntu24.04.sh
```
* If all went smoothly, you should not have seen any errors, and should have been informed that setup was finished.

If you are interested to see what the script does or which functions it calls, you can open it in a text editor and have look at it - it's pretty self-explanatory (_gedit_ for example in Ubuntu is a text editor with syntax highlighting). You can alternatively start the script with _--pause_ argument to stop the script execution after each step to better inspect the output. To achieve this, issue `./setup_buildenv_ubuntu24.04.sh --pause` as the last command in the above list instead.

It's best to reboot the PC before continuing to next steps. This concludes the build setup preparations.

## Building Edge16 firmware for the radio

For tidy files and folder hierarchy, it's best to create a dedicated subfolder in the current user home for Edge16, as a container for various Edge16 flavors and builds. In the terminal window, issue the following commands, one at a time:
```
mkdir ~/edge16
```
```
cd ~/edge16
```

We will next fetch the Edge16 source files from the GitHub main development branch into local subfolder /edge16/edge16_main in current user home, prepare the environment and build output directory. Issue, in the same terminal window as above, the following commands, one at a time:
```
git clone --recursive -b main https://github.com/onliner10/Edge16.git edge16_main
```
```
cd edge16_main && mkdir build-output
```

To build Edge16, we need to minimally specify the radio target, but can further select or de-select a number of build-time options. A full list of available options is documented on the the CMake options reference in source history page. You can also generate a text-file list of all options by running:
```
cmake -LAH -S . > ~/edge16_main-cmake-options.txt
```

You can use, e.g. _gedit_ under Ubuntu to view the file.

As an example, we will build next for RadioMaster TX16S (PCB=X10, PCBREV=TX16S), mode 2 default stick (DEFAULT_MODE=2, will otherwise default to mode 1) and selected the type as a Debug build with debug symbols included (CMAKE_BUILD_TYPE=Debug). The CMake command for this is:
```
cmake --fresh -S . -B build-output -Wno-dev -DPCB=X10 -DPCBREV=TX16S -DDEFAULT_MODE=2 -DCMAKE_BUILD_TYPE=Debug
```
If you do not want to include the debug symbols, use `-DCMAKE_BUILD_TYPE=Release` instead.

To build for other radios, you just need to select another build target by specifying appropriate values for `PCB` and `PCBREV` for your radio. It is best to use a different build folder for each target. As a tip for which values to use, have a look at a Python script according to your radio manufacturer in a file named `build-<radio-manufacturer>.py` under [https://github.com/onliner10/Edge16/tree/main/tools](https://github.com/onliner10/Edge16/tree/main/tools)

It is recommended to set the `CMAKE_BUILD_PARALLEL_LEVEL` environment variable to the number of CPU cores on your system, to speed up all subsequent builds:
```
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)
```

To configure, issue:
```
cmake --build build-output --target arm-none-eabi-configure
```

To build the firmware, issue:
```
cmake --build build-output --target firmware --parallel
```

This process can take some minutes to complete.
If successful, you should find a firmware binary _firmware.bin_ in the `build-output/arm-none-eabi` folder, that you can flash onto your radio.

It's a good idea to rename the binary, so that it is easier later to see the target radio and which options were baked into it. For this, issue e.g.:
```
mv build-output/arm-none-eabi/firmware.bin edge16_tx16s_mode2_debug.bin
```

You will need to prepare a clean microSD card. Edge16 currently uses upstream EdgeTX SD card packs for 480x320 color radios; download them from [EdgeTX SD card releases](https://github.com/EdgeTX/edgetx-sdcard/releases/tag/latest).

You can use [Edge16 release assets](https://github.com/onliner10/Edge16/releases) or [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) to flash the binary to your radio. For further instructions, see:
[https://github.com/onliner10/Edge16/releases](https://github.com/onliner10/Edge16/releases)

## Building the simulator and radio simulator module

You can build the firmware, the native SDL simulator (`simu`) and the radio simulator module (`wasi-module`) in one step:
```
cmake --build build-output --parallel --target firmware --target wasi-module --target simu
```

This will configure and download extra dependencies as needed. Alternately, if you only want to build the simulator module at this point, you can run:
```
cmake --build build-output --parallel --target wasi-module
```

The wasm simulator module is built into `build-output/wasm/wasm-build/`. If you want to build simulator modules for multiple radio targets, the helper script `tools/build-wasm-modules.sh` can build all supported targets in one go:
```
tools/build-wasm-modules.sh . ./wasm-modules/
```
The `.wasm` files are output to `./wasm-modules/`.

The native simulator is built as the `simu` target:
```
cmake --build build-output --parallel --target simu
```

Before running the simulator, copy upstream EdgeTX SD card content for 480x320 color radios and extract it e.g. to `~/edge16/simu_sdcard/horus`.

To launch the simulator, point it at that SD card content with `--storage`:
```
./build-output/native/simu --storage ~/edge16/simu_sdcard/horus
```

[![Edge16 simulator on Linux](../assets/images/build/linux/EdgeTX_simulator_Linux.png)](../assets/images/build/linux/EdgeTX_simulator_Linux.png)
