# EmuZ-1500 Solaris Port

[日本語版はこちら](README.md)

This repository contains a Solaris-focused EmuZ-1500 source distribution based
on the Common Source Code Project by TAKEDA, Toshiya. The original source can be
downloaded from https://takeda-toshiya.my.coocan.jp.

Only EmuZ-1500 is tested. The shared source tree contains code for other
machines, but they are not supported in this repository.

## Contents

- `src/` - common emulator code, virtual machine devices, and host code needed
  to build the Solaris EmuZ-1500 version.
- `src/solaris/` - Solaris direct host using SDL2 and GTK2.
- `src/vm/mz700/` - MZ-700/MZ-1500 family VM implementation used by this build.
  It is mostly unchanged from the original source, except for performance
  improvements in `memory.cpp`.
- `g++/` - build files for GCC and GNU make.
- `license/` - GPL and bundled third-party license documents.

## Build

`g++/Makefile.mz1500` builds the Solaris EmuZ-1500 host. GCC and GNU make
(`gmake`) are expected.

```sh
gmake -f g++/Makefile.mz1500
```

In addition to standard OS functionality, the build requires:

- GCC with C++11 support
- GNU make (`gmake`)
- SDL2 headers and libraries
- GTK2 headers and libraries

The generated executable is:

```sh
./mz1500
```

Examples:

```sh
./mz1500 tape-file
./mz1500 --cmt tape-file.mzt
./mz1500 --qd quick-disk-file.mzt
```

## Solaris Control UI

Because the Solaris version could not use the same window layout as the Windows
version, it provides a separate control window. It includes a subset of menu
items based on the Windows operations.

## License

Source code derived from the Common Source Code Project is available under the
GNU General Public License Version 2 or later. Copyright belongs to the
respective authors of each source file.

See `license/COPYING.txt` for the GPL text. See `readme.txt` and
`readme_by_*.txt` for notes from the original project and authors. Bundled
third-party license documents are under `license/`.
