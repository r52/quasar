Widget Definition Reference
===========================

A Widget Definition file is a JSON file that contains at least the following parameters:

``name``
    Name of the widget.

``width``
    Width of the widget.

``height``
    Height of the widget.

``startFile``
    Entry point for the widget. This can be a local file or a URL.

``transparentBg``
    Whether the widget background is transparent.

Example
--------

.. code-block:: json

    {
        "name": "simple_perf",
        "width": 400,
        "height": 120,
        "startFile": "index.html",
        "transparentBg": true,
        "required": ["win_simple_perf"]
    }


Optional Parameters
--------------------

``dataserver``: true/false (default false)
    Defines whether connection and authentication scripts are loaded into the widget. This parameter must be defined and set to ``true`` if the widget requires a connection to the Data Server. If this parameter is not defined, ``false`` is implied, and connection/authentication scripts are not loaded.

``required``: String array
    A string array containing extension identifiers that the widget requires to function. The widget will fail to load if one or more of the extensions listed here are unavailable.

``remoteAccess``: true/false
    By default, the Chrome instances hosting locally defined widgets cannot access remote URLs (i.e. over ajax) due to Cross-Origin restrictions. This parameter can be defined by a widget to allow remote URL accesses. If the parameter is defined, a security warning will be shown when loading the widget to remind the user that the widget should be downloaded from a trusted source when allowing remote URL access.

``clickable``: true/false (default false)
    Defines whether the widget's contents can be interacted with by default (e.g. links, App Launcher widgets). See also :ref:`widget-menu`.
