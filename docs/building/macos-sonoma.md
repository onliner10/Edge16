# Build Instructions using macOS 14.1 (Sonoma)

### Install Xcode command line tools:

- Open `Terminal`
- Run this command:
```
xcode-select —install
```

### Install "brew" (https://brew.sh):

- Run command in `Terminal`:
```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### Install newer Python version:

- Run command in `Terminal`:
```
brew install python
```

### Install ARM toolchain

Download and install ARM GCC from here (installs in `/Applications/ARM/`):
- https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-darwin-x86_64-arm-none-eabi.pkg

Please note that this installation takes care of allowing the binaries to be run and "un-quarantines" them. If you choose to install the `bz2` archive to another location, you will have to take care of that yourself (see https://disable-gatekeeper.github.io/ for more details).

### Install various dependencies

- With `brew` in a `Terminal`:
```
brew install sdl fox cmake
```

If you plan to run the standalone simulator for debuging
```
brew install --cask quartz
```

- Install Python dependencies:
```
pip3 install Pillow clang lz4 jinja2
```

### Compile EdgeTX

- Checkout code
```
git clone --recursive https://github.com/EdgeTX/edgetx.git
```

- Switch into the source directory:
```
cd edgetx
```

- Create build directory and configure build using `cmake`:
```
mkdir -p build

cd build

cmake -DPCB=X10 -DPCBREV=TX16S \
   -DARM_TOOLCHAIN_DIR=/Applications/ArmGNUToolchain/13.2.Rel1/arm-none-eabi/bin/ \
   -DPYTHON_EXECUTABLE=$(brew --prefix)/bin/python3 ..
```

Please note that the variables `ARM_TOOLCHAIN_DIR` and `PYTHON_EXECUTABLE` must be specified additionally to what is described in the other compilation HowTos:
- `ARM_TOOLCHAIN_DIR`: this must point to where ARM GCC has been installed (and MUST contain `/` at the end).
- `PYTHON_EXECUTABLE`: this allows overriding Python installed as part of MacOS.

- Then build as usual (`-j4` to use 4 CPU cores):
```
make -j4 firmware
```
