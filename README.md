# Quasar

HTML widgets for your desktop, powered by Qt WebEngine. Licensed under GPLv3.

## Usage

After running the application, right click the application icon in the notification bar to load widgets.

## Widgets

Widgets are just webpages. They can be as simple or as complex as you want, perfect for those with web skills that wants to make their desktop a little fancier. Refer to the wiki for details on how to create a widget definition.

## Data Server

Quasar propagates data to widgets over the WebSocket protocol using a local data server, and it is not just limited to Quasar widgets. The data server can be used in various other scenarios, such as an HTML stream overlay. Refer to the wiki for details on the WebSocket API.

## Plugins

Need more data? Data available to widgets can be extended through plugins. Refer to the wiki for details on the Plugin API.

## System Requirements

An OS and computer capable of running Chrome.

[Microsoft Visual C++ Redistributable for Visual Studio 2017](https://go.microsoft.com/fwlink/?LinkId=746572) may be needed on Windows.

## Build Dependencies

[![Build status](https://ci.appveyor.com/api/projects/status/yd5l7u53ufo4mur1?svg=true)](https://ci.appveyor.com/project/r52/quasar)

[Qt 5.9 or higher](http://www.qt.io/) and Visual Studio 2017 is required. Should be compile-able on Linux and Mac as well, untested.
