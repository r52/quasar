# Quasar
[![License](https://img.shields.io/github/license/r52/quasar.svg)](https://github.com/r52/quasar/blob/master/LICENSE.txt)
[![Build status](https://ci.appveyor.com/api/projects/status/yd5l7u53ufo4mur1?svg=true)](https://ci.appveyor.com/project/r52/quasar)
[![Documentation Status](https://readthedocs.org/projects/quasardoc/badge/?version=latest)](https://quasardoc.readthedocs.io/en/latest/?badge=latest)

![](https://i.imgur.com/NX2SNUD.png)

**Quasar** is a cross-platform desktop application that displays web-based widgets on your desktop. Quasar takes full advantage of the Chromium engine to allow even for complex, fully dynamic web apps.

Quasar provides a WebSocket-based Data Server that is extensible using extensions. The Data Server is capable of processing and sending data to client widgets that would otherwise not be available in a purely web-based context, for example, your PC's resource information such as CPU and memory usage, or your [Spotify client's now-playing information](https://github.com/r52/quasar-spotify).

Refer to the [Quasar documentation](https://quasardoc.readthedocs.io) for more information on how to build a widget or a Data Server extension.

Quasar is licensed under GPL-3.0.

## System Requirements

An OS and computer capable of running Chrome.

[Microsoft Visual C++ Redistributable for Visual Studio 2017](https://go.microsoft.com/fwlink/?LinkId=746572) is required on Windows.


## Getting Started

Both an installer and a portable package is provided for Windows x64 under [GitHub Releases](https://github.com/r52/quasar/releases>).

Simply install or extract Quasar and run the application. The Quasar icon will then show up in your desktop's notification bar. Right-click the icon, load your desired widgets, and enjoy! See the [documentation](https://quasardoc.readthedocs.io) for more details.

For developers who wishes to build a Data Server extension, or those who wants to create their own widget, please refer to the [Quasar documentation](https://quasardoc.readthedocs.io). The documentation contains examples and other resources on how to build your first extension or widget.


## Data Server

Quasar propagates data to widgets over the WebSocket protocol using a local WebSocket Data Server that is extensible using extensions, and it is not just limited to Quasar widgets. External widgets or applications can connect to Quasar Data Server as well, allowing Quasar to be used in various other scenarios, such as an HTML stream overlay. Refer to the [Quasar documentation](https://quasardoc.readthedocs.io) for more information on how to build a widget or a Data Server extension.

## Building

Source code is [available on GitHub](https://github.com/r52/quasar).

### Building on Windows

[Visual Studio 2017 or later](https://www.visualstudio.com/) is required.

[Qt 5.12 or later](http://www.qt.io/) is required.

Build using the Visual Studio Solution file.

### Building on Linux

Qt 5.12 or later, GCC 7+ or Clang 4+, and CMake 3.9+ is required.

Build using the supplied CMake project file.

### Building on Mac

Quasar is written in cross-platform C++ and should build on Mac with minimal changes. However, it is currently untested and unsupported.
