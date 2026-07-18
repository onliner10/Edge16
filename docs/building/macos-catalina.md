# Build Instructions using macOS 10.15 (Catalina)

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
- https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-mac.pkg

Please note that this installation takes care of allowing the binaries to be run and "un-quarantines" them. If you choose to install the `bz2` archive to another location, you will have to take care of that yourself (see https://disable-gatekeeper.github.io/ for more details).

### Install various dependencies

- With `brew` in a `Terminal`:
```
brew install sdl fox cmake
```

- Install Python dependencies:
```
pip3 install Pillow clang lz4 jinja2
```

- If you face issues from pip3 command, try to fix your CommandLineTools installation
From the terminal run:
```
softwareupdate --list
```
which produces a list of available updates. Wait a bit for a list to display (won't take very long). And look for the "* Label:" under Software Update found the following new or updated software:

It should say something like: * Label: `Command Line Tools for Xcode-13.2`

Then simply run:
```
softwareupdate -i "Command Line Tools for Xcode-13.2"
```
and replace the text in the brackets with the Label from the previous output. This will then install the updates and the fix for python3.

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
   -DARM_TOOLCHAIN_DIR=/Applications/ARM/bin/ \
   -DPYTHON_EXECUTABLE=$(brew --prefix)/bin/python3 ..
```

Please note that the variables `ARM_TOOLCHAIN_DIR` and `PYTHON_EXECUTABLE` must be specified additionally to what is described in the other compilation HowTos:
- `ARM_TOOLCHAIN_DIR`: this must point to where ARM GCC has been installed (and MUST contain `/` at the end).
- `PYTHON_EXECUTABLE`: this allows overriding Python installed as part of MacOS.

- Then build as usual (`-j4` to use 4 CPU cores):
```
make -j4 firmware
```
