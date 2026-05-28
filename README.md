# EmuZ-1500 Solaris Source

[日本語版はこちら](README.ja.md)

This repository contains a Solaris-focused EmuZ-1500 source distribution based
on Takeda Toshiya's retro PC emulator common source code.  The original source
can be downloaded from https://takeda-toshiya.my.coocan.jp.  The original
project documentation is kept in `readme.txt`.

Only the Solaris EmuZ-1500 host is wired up and tested here.  The shared source
tree may still contain code for other machines, but this repository should be
treated as an EmuZ-1500 Solaris build.

## Contents

- `src/` - common emulator code, virtual machine devices, and host code needed
  by the Solaris EmuZ-1500 build.
- `src/solaris/` - Solaris direct host using SDL2 and GTK2.
- `src/vm/mz700/` - MZ-700/MZ-1500 family VM implementation used by this build.
  It is mostly unchanged from the original source, except for performance
  improvements in `memory.cpp`.
- `license/` - GPL and third-party license texts.

## Build

The root `Makefile` builds the Solaris EmuZ-1500 host with GCC and GNU make:

```sh
gmake
```

The Solaris build expects:

- GCC with C++11 support
- GNU make (`gmake`)
- SDL2 headers and libraries
- GTK2 headers and libraries

The generated executable is:

```sh
./mz1500
```

Optional image arguments:

```sh
./mz1500 tape-file
./mz1500 --cmt tape-file
./mz1500 --qd quick-disk-file
```

## Solaris Control UI

The Solaris host cannot use the same window layout as the Windows version, so
it provides a separate GTK control window.  It includes a subset of menu items
that follows the Windows operations.

Console command input is disabled in release builds.  It is still available for
diagnostics when the source is built with `-DDEBUG`:

```sh
env CXXFLAGS=-DDEBUG gmake
```

In a debug build, stdin accepts commands such as `help`, `status`, `cmt`,
`cmtrec`, `cmtplay`, `cmtstop`, `cmteject`, `cmtff`, `cmtrew`, `qd`,
`qdeject`, `option`, `reset`, and `exit`.

## License

The source code is available under the GNU General Public License Version 2 or
later.  See `license/COPYING.txt` for the license text and the files under
`license/` for bundled third-party notices.

## Notes for GitHub Uploads

- Keep `readme.txt` because it contains the original project description and
  acknowledgements.
- Keep the complete `license/` directory when publishing the repository.
- Make clear that this repository publishes the Solaris EmuZ-1500 build, not a
  full upstream mirror of all original platforms.
- Do not publish generated binaries or local object files unless a release
  package intentionally includes them.
