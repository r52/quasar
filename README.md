# Quasar

[![License](https://img.shields.io/github/license/r52/quasar.svg)](https://github.com/r52/quasar/blob/master/LICENSE.txt)
[![Build Quasar](https://github.com/r52/quasar/actions/workflows/build.yml/badge.svg)](https://github.com/r52/quasar/actions/workflows/build.yml)
[![Documentation Status](https://readthedocs.org/projects/quasardoc/badge/?version=latest)](https://quasardoc.readthedocs.io/en/latest/?badge=latest)

**Looking for stuff to use with Quasar? [Click here](https://github.com/r52/things-for-quasar)**

![](https://i.imgur.com/NX2SNUD.png)

**Quasar** is a cross-platform desktop application that displays web-based widgets on your desktop. Quasar leverages the Chromium engine to serve desktop widgets in a platform agnostic manner. Widgets can be as simple as a single webpage with a couple of lines of HTML, a complex fully dynamic web app, or even a WebGL app.

Quasar provides a WebSocket-based Data Server that is extensible using extensions. The Data Server is capable of processing and sending data to client widgets that would otherwise not be available in a purely web-based context, for example, your PC's resource information such as CPU and memory usage, or your [Spotify client's now-playing information](https://github.com/r52/quasar-spotify-api).

Refer to the [Quasar documentation](https://quasardoc.readthedocs.io) for more information on how to build a widget or a Data Server extension.

Quasar is licensed under GPL-3.0. All sample widgets are licensed under the MIT license.

## System Requirements

An OS and computer capable of running Chrome, preferably with Hardware Acceleration capabilities. Only 64-bit OSes are supported. On Windows, only Windows 10 and above are supported. On Linux, only X11 desktop environments with system tray support are supported.

While Quasar is reasonably fast and lightweight, do not expect Quasar to be power efficient, especially when running heavier widgets like the [WebGL visualizer](widgets/visualizer_webgl).

#### Note Regarding Wayland

Quasar does not work properly on Wayland compositors due to Wayland not supporting functions like global cursor position or explicitly setting window positions for top-level windows [[1]](https://bugreports.qt.io/browse/QTBUG-110119)[[2]](https://bugreports.qt.io/browse/QTBUG-86780), which completely defeats Quasar's core functionality. Until a workaround or solution is added to Qt itself, Quasar cannot support Wayland.

#### Qt on Linux

Quasar has been built and tested on the latest pre-built binaries supplied by Qt, which at the time of writing is Qt 6.4.3 built against OpenSSL 1.1.1. The Qt version offered by Ubuntu 22.04's package repository is Qt 6.2.4 built against OpenSSL 3.0. As of Ubuntu 22.04, OpenSSL 3 is the default and version 1.1.1 is no longer offered in the package repository. Building Quasar on versions of Qt older than 6.4 may work but is not supported. Ensure that the version of the OpenSSL libraries installed (i.e `libcrypto.so` and `libssl.so`) matches the version your Qt installation is built against or SSL functionality will fail.

## Getting Started

[Download the latest portable release here](https://github.com/r52/quasar/releases) for Windows x64.

Simply extract Quasar and run the application.

The Quasar icon will then show up in your desktop's notification bar. Right-click the icon, load your desired widgets, and enjoy! See the [documentation](https://quasardoc.readthedocs.io) for more details.

## Creating Widgets/Extensions

For developers who wishes to build a Data Server extension, or those who wants to create their own widget, please refer to the [Quasar documentation](https://quasardoc.readthedocs.io). The documentation contains examples and other resources on how to build your first extension or widget.

## Building Requirements

Source code is [available on GitHub](https://github.com/r52/quasar).

### All Platforms

- [CMake 3.23 or later](https://cmake.org/)
- [Qt 6.4 or later](http://www.qt.io/), with at least the following additional libraries:
  - WebEngine (qtwebengine)
  - Positioning (qtpositioning)
  - WebChannel (qtwebchannel)
  - Network Authorization (qtnetworkauth)
  - Serial Port (qtserialport)
- The `Qt6_DIR` environment variable defined for your Qt installation
  - Windows example: `C:\Qt\6.4.3\msvc2019_64`
  - Linux example: `$HOME/Qt/6.4.3/gcc_64/`
  - On Linux, additional dependencies may be needed for Qt such as the packages `libgl1-mesa-dev libglvnd-dev`

### Windows

- [Visual Studio 2022 or later](https://www.visualstudio.com/) is required
  - The Clang MSVC toolkit is also required if you wish to build the `win_audio_viz` sample extension

### Linux

- gcc/g++ 11 or later, or Clang 16 or later
  - Tested on Ubuntu 22.04 using both g++ 11 and 12
  - Clang 15 and earlier fails to compile gcc's implementation of the C++20 ranges library, which is used in Quasar
  - Clang is required if you wish to build the `pulse_viz` sample extension
- [vcpkg](https://github.com/microsoft/vcpkg) dependencies, for example including but not limited to the following Debian-based packages:
  - `build-essential tar curl zip unzip pkg-config`

### macOS

Quasar is written in cross-platform C++ and should build on Mac with minimal changes. However, it is currently untested and unsupported.

### Cloning

```bash
git clone --recurse-submodules https://github.com/r52/quasar.git
cd quasar
```

### Configuring the Project

#### Windows

The following example configures the project to build using the `clang-cl` toolkit with Visual Studio Community 2022 (plus the Clang MSVC toolkit) installed:

```pwsh
cmake --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE "-DCMAKE_C_COMPILER:FILEPATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe" "-DCMAKE_CXX_COMPILER:FILEPATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe" -S./ -B./build -G "Visual Studio 17 2022" -T ClangCL,host=x64 -A x64
```

#### Linux

The following example configures the project to build using `g++-12`, assuming g++ 12 is installed, and configures Quasar to be installed to `$HOME/.local/quasar/`:

```bash
export CC=gcc-12
export CXX=g++-12
cmake --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE --install-prefix $HOME/.local/ -S./ -B./build -G "Unix Makefiles"
```

`-G Ninja` can also be used provided that Ninja is installed.

### Building the Project

```bash
cmake --build ./build --config Release --
```

### Installing from Build (optional)

```bash
cmake --install ./build
```

## Acknowledgements

- The `win_audio_viz` and `pulse_viz` extensions are ported from [Rainmeter's AudioLevel plugin](https://github.com/rainmeter/rainmeter/blob/master/Plugins/PluginAudioLevel/)
  - [Rainmeter](https://github.com/rainmeter/rainmeter) is licensed under GPLv2
- The `win_visualizer_webgl` widget is adapted from the [three.js webaudio visualizer demo](https://threejs.org/examples/webaudio_visualizer.html)
  - [three.js](https://github.com/mrdoob/three.js) is licensed under the MIT license
- Various other code snippets obtained from sources such as StackOverflow, used under compatible licensing, credited within the source code
