
Quasar
==================================

.. image:: https://img.shields.io/github/license/r52/quasar.svg
    :alt: GitHub license
    :target: https://github.com/r52/quasar/blob/master/LICENSE.txt

.. image:: https://github.com/r52/quasar/actions/workflows/build.yml/badge.svg
    :target: https://github.com/r52/quasar/actions/workflows/build.yml

.. image:: https://readthedocs.org/projects/quasardoc/badge/?version=latest
    :target: http://quasardoc.readthedocs.io/en/latest/?badge=latest

.. image:: https://i.imgur.com/NX2SNUD.png

**Quasar** is a cross-platform desktop application that displays web-based widgets on your desktop. Quasar leverages the Chromium engine to serve desktop widgets in a platform agnostic manner. Widgets can be as simple as a single webpage with a couple of lines of HTML, a complex fully dynamic web app, or even a WebGL app.

Quasar provides a WebSocket-based Data Server that is extensible by custom extensions. The Data Server is capable of processing and sending data to client widgets that would otherwise not be available in a purely web-based context, for example, your PC's resource information such as CPU and memory usage, or your `Spotify client's now-playing information <https://github.com/r52/quasar-spotify-api>`_.

See :doc:`extqs` and :doc:`api` for more information on how to create a Data Server extension, and :doc:`widgetqs` for building widgets.

Quasar is licensed under GPL-3.0. All sample widgets are licensed under the MIT license.

System Requirements
-------------------

An OS and computer capable of running Chrome. Only 64-bit OSes are supported. On Windows, only Windows 10 and above are supported. On Linux, only desktop environments with system tray support are supported.

Getting Started
---------------

`Download the latest portable release here <https://github.com/r52/quasar/releases>`_, for Windows x64.

Simply extract Quasar and run the application.

The Quasar icon will then show up in your desktop's notification bar. Right-click the icon, load your desired widgets, and enjoy! See :doc:`usage` for more details.

Creating Widgets/Extensions
-----------------------------

See :doc:`extqs` and :doc:`api` for more information on how to create a Data Server extension, and :doc:`widgetqs` for building widgets.

Building Requirements
-------------------------

Source code is `available on GitHub <https://github.com/r52/quasar>`_.

All Platforms
~~~~~~~~~~~~~~~~

* `CMake 3.23 or later <https://cmake.org/>`_
* `Qt 6.4 or later <http://www.qt.io/>`_, with at least the following additional libraries:

  * WebEngine (qtwebengine)
  * Positioning (qtpositioning)
  * WebChannel (qtwebchannel)
  * Network Authorization (qtnetworkauth)
  * Serial Port (qtserialport)

* The ``Qt6_DIR`` environment variable defined for your Qt installation

  * Windows example: ``C:\Qt\6.4.2\msvc2019_64``
  * Linux example: ``$HOME/Qt/6.4.2/gcc_64/``
  * On Linux, additional dependencies may be needed for Qt such as the packages ``libgl1-mesa-dev libglvnd-dev``

Windows
~~~~~~~~~~~~~~~~~~~

* `Visual Studio 2022 or later <https://www.visualstudio.com/>`_ is required

  * The Clang MSVC toolkit is also required if you wish to build the ``win_audio_viz`` sample extension


Linux
~~~~~~~~~~~~~~~~~

* gcc/g++ 11 or later, or Clang 15 or later

  * Tested on Ubuntu 22.04 using both g++ 11 and 12

* `vcpkg <https://github.com/microsoft/vcpkg>`_ dependencies, for example including but not limited to the following Debian-based packages:

  * ``build-essential tar curl zip unzip pkg-config``


macOS
~~~~~~~~~~~~~~~

Quasar is written in cross-platform C++ and should build on Mac with minimal changes. However, it is currently untested and unsupported.

Cloning
~~~~~~~~~~~~

.. code-block:: bash

    git clone --recurse-submodules https://github.com/r52/quasar.git
    cd quasar


Configuring the Project
~~~~~~~~~~~~~~~~~~~~~~~~

Windows
^^^^^^^^^^^^^

The following example configures the project to build using the ``clang-cl`` toolkit with Visual Studio Communitry 2022 (plus the Clang MSVC toolkit) installed:

.. code-block:: bash

    cmake --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE "-DCMAKE_C_COMPILER:FILEPATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe" "-DCMAKE_CXX_COMPILER:FILEPATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe" -S./ -B./build -G "Visual Studio 17 2022" -T ClangCL,host=x64 -A x64


Linux
^^^^^^^^^^^

The following example configures the project to build using ``g++-12``, assuming g++ 12 is installed, and configures Quasar to be installed to ``$HOME/.local/quasar/``:

.. code-block:: bash

    export CC=gcc-12
    export CXX=g++-12
    cmake --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE --install-prefix $HOME/.local/ -S./ -B./build -G "Unix Makefiles"


Building the Project
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    cmake --build ./build --config Release --

Installing from Build (optional)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash
  
    cmake --install ./build


Resources
-------------------

.. toctree::
   :maxdepth: 1
   :caption: Main

   usage
   extensions
   settings
   launcher

.. toctree::
   :maxdepth: 1
   :caption: Guides

   widgetqs
   extqs

.. toctree::
  :maxdepth: 1
  :caption: Sample Extensions

  win_simple_perf
  win_audio_viz

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
