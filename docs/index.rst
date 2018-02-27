
Quasar
==================================

.. image:: https://img.shields.io/github/license/r52/quasar.svg
    :alt: GitHub license
    :target: https://github.com/r52/quasar/blob/master/LICENSE.txt

.. image:: https://ci.appveyor.com/api/projects/status/yd5l7u53ufo4mur1?svg=true
    :target: https://ci.appveyor.com/project/r52/quasar

.. image:: https://readthedocs.org/projects/quasardoc/badge/?version=latest
    :target: http://quasardoc.readthedocs.io/en/latest/?badge=latest

.. image:: https://i.imgur.com/NX2SNUD.png

**Quasar** is a cross-platform desktop application that displays web-based widgets on your desktop. Quasar takes full advantage of the Chromium engine to allow even for complex, fully dynamic web apps.

Quasar provides a WebSocket-based Data Server that is extensible using plugins. The Data Server is capable of processing and sending data to client widgets that would otherwise not be available in a purely web-based context, for example, your PC's resource information such as CPU and memory usage, or your `Spotify client's now-playing information <https://github.com/r52/quasar-spotify>`_.

See :doc:`plugqs` and :doc:`api` for more information on how to create a Data Server extension plugin, and :doc:`widgetqs` for building widgets.

Quasar is licensed under GPL-3.0.

System Requirements
-------------------

An OS and computer capable of running Chrome.

`Microsoft Visual C++ Redistributable for Visual Studio 2017 <https://go.microsoft.com/fwlink/?LinkId=746572>`_ is required on Windows.

Getting Started
---------------

Both an installer and a portable package is provided for Windows x64 under `GitHub Releases <https://github.com/r52/quasar/releases>`_.

Simply install or extract Quasar and run the application. The Quasar icon will then show up in your desktop's notification bar. Right-click the icon, load your desired widgets, and enjoy! See :doc:`usage` for more details.

For developers who wishes to build a Data Server extension plugin, or those who wants to create their own web widget, please refer to Quasar's documentation at https://quasardoc.readthedocs.io. The documentation contains examples and other resources on how to build your first plugin or widget.

Building
-------------------------

Source code is `available on GitHub <https://github.com/r52/quasar>`_.


Building on Windows
~~~~~~~~~~~~~~~~~~~

`Visual Studio 2017 or later <https://www.visualstudio.com/>`_ is required.

`Qt 5.9 or later <http://www.qt.io/>`_ is required.

Build using the Visual Studio Solution file.

Building on Linux
~~~~~~~~~~~~~~~~~

Qt 5.9 or later, GCC 7+ or Clang 4+, and CMake 3.9+ is required. Quasar has been tested to build and work successfully under Ubuntu 17.10 using packages available in the official repository.

Build using the supplied CMake project file.

Building on Mac
~~~~~~~~~~~~~~~

Quasar is written in cross-platform C++ and should build on Mac with minimal changes. However, it is currently untested and unsupported.

Resources
-------------------

.. toctree::
   :maxdepth: 1
   :caption: Main

   usage
   settings
   launcher

.. toctree::
   :maxdepth: 1
   :caption: Guides

   widgetqs
   plugqs

.. toctree::
   :maxdepth: 1
   :caption: Developer References

   api
   wcp
   wdef
   log


Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
