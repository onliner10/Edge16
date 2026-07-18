# Build Instructions using macOS 15 (Sequoia) and macOS 26 (Tahoe)

# Install [Homebrew](https://brew.sh/)

- Run command in `Terminal`:
```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```
!!! tip
    Installing Brew via the command above will automatically install the [Xcode Command Line Tools](https://mac.install.guide/commandlinetools/). If for some reason you need to do this manually, run `xcode-select —install` via the Terminal app.

# Install ARM toolchain

Download and install the ARM GCC toolchain [from here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (installs in `/Applications/ArmGNUToolchain/`):

- For Intel Mac: https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-x86_64-arm-none-eabi.pkg
- If Mac Silicon (i.e. M1-M5): https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi.pkg

Please note that this installation takes care of allowing the downloaded binaries to be run and prevents them being quarantined. If you choose to install the `tar.xz` archive to another location, you will have to take care of that yourself (see https://disable-gatekeeper.github.io/ for more details).

# Install various dependencies

- With `brew` in a `Terminal`:
```
brew install sdl cmake
```

If you plan to run the standalone simulator for debugging:
```
brew install --cask quartz
```


# Download EdgeTX code

- Checkout code
```
git clone --recursive https://github.com/EdgeTX/edgetx.git
```

- Switch into the source directory:
```
cd edgetx
```

# Install Python dependencies

Since Python 3.11 enabled the "externally managed" flag, it is recommended that you use a virtual environment. [uv](https://docs.astral.sh/uv/getting-started/installation/) is one of the easiest tools to do and manage this, and can be installed with brew. It is recommended you create the virtual environment now rather than earlier in the process, as doing it now will create it in the edgetx directory.

- Install UV:
```
brew install uv
```

- Create the uv-managed environment:
```
uv sync
```

- Build commands should be run through uv so CMake uses the Python dependencies from the project environment:
```
uv run python -c "import jinja2, PIL, lz4, pydantic"
```

# Compile EdgeTX

- Create and enter build directory:
```
mkdir -p build && cd build
```

Configure build flags using `cmake` (in this case, for RadioMaster TX16S, [see here](https://github.com/EdgeTX/edgetx/blob/main/tools/build-common.sh) for other possible handset specific flags).
```
uv run cmake -DPCB=X10 -DPCBREV=TX16S \
   -DARM_TOOLCHAIN_DIR=/Applications/ArmGNUToolchain/14.2.Rel1/arm-none-eabi/bin/ ..
```

!!! note
    Please note that the variable `ARM_TOOLCHAIN_DIR` must be specified additionally to what is described in the other compilation HowTos:

    - `ARM_TOOLCHAIN_DIR`: this must point to where ARM GCC has been installed (and MUST contain `/` at the end).

Configure the compiler for firmware building (parallel limits the number of CPU cores used - you can increase this if your machine can handle more):
```
uv run cmake --build . --target arm-none-eabi-configure --parallel 4
```

Build the firmware!
```
uv run cmake --build . --target firmware
```
