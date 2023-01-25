Widget Client Protocol
======================

The Widget Client Protocol is the protocol that is used by Quasar and Quasar widgets to communicate with each other. The messages are JSON-encapsulated data that is sent over WebSocket. This document defines the message format and accepted fields, as well as sample usages in JavaScript.

.. contents::
   :local:

Globals Functions
------------------

These global JavaScript functions are defined for all Quasar loaded widgets:

``quasar_create_websocket()``
    Creates a WebSocket object connecting to Quasar's Data Server.

``quasar_authenticate(socket)``
    Authenticates this widget with the Quasar Data Server.

Sample Usage
~~~~~~~~~~~~~

.. code-block:: javascript

    websocket = quasar_create_websocket();
    websocket.onopen = function(evt) {
        quasar_authenticate(websocket);
    };

Data Protocol
--------------

Client to Server
~~~~~~~~~~~~~~~~~

The following is the basic message format used by a client widget to send messages to the Quasar Data Server.

Basic Message Format
####################

::

    {
        method: <method>,
        params: {
            topics: [<targets>],
            params: <list of target params>
            args: <param args>
        }
    }


Field Descriptions
####################

``method``
    The method/function to be invoked by this message.
    For client widgets, supported values are: ``subscribe``, and ``query``.
    ``subscribe`` is used to subscribe to timer-based or extension signaled Data Sources, while ``query`` is used for client polled Data Sources as well as any other commands.
    For Quasar loaded widgets, ``auth`` is also supported for authenication purposes.

``params``
    The parameters sent to the method.
    This field should be a JSON object that is typically comprised of at least the field ``topics``.

``topics``
    The intended targets of the message.
    Typically, this is an extension's identifier plus the Data Source identifier separated by a forward slash.

``target params``
    List of parameters sent to all targets.
    Typically, this field is unused.

``param args``
    Optional arguments sent to the target.
    Only supported by queried/client polled sources, if arguments are supported by the source.

Sample Usages
#################

.. code-block:: javascript

    function subscribe() {
        const msg = {
            method: "subscribe",
            params: {
                topics: ["win_audio_viz/band"]
            }
        }

        websocket.send(JSON.stringify(msg));
    }

    function poll() {
        const msg = {
            method: "query",
            params: {
                topics: ["win_simple_perf/sysinfo_polled"]
            }
        }

        websocket.send(JSON.stringify(msg));
    }

    function get_launcher_list() {
        const msg = {
            method: "query",
            params: {
                topics: ["applauncher/list"]
            }
        }

        websocket.send(JSON.stringify(msg));
    }

    function launcher_cmd(cmd, arg) {
        let msg = {
            method: "query",
            params: {
                topics: [`applauncher/${cmd}`],
            },
        };

        if (arg) {
            msg.params["args"] = arg;
        }

        websocket.send(JSON.stringify(msg));
    }

    function authenticate() {
        const msg = {
            method: "auth",
            params: {
                code: "6EFBBE6542D52FDD294337343147B033"
            }
        }

        websocket.send(JSON.stringify(msg));
    }

Refer to the source code of `sample widgets <https://github.com/r52/quasar/tree/master/widgets>`_ for concrete examples of client to server communications, or the source code of `sample extensions <https://github.com/r52/quasar/tree/master/extensions>`_ for examples of specific targets.


Server to Client
~~~~~~~~~~~~~~~~~~

The following is the basic message format used by the Data Server to send data and messages to client widgets.

Basic Message Format
#####################

::

    {
        <target>: {
            <target data>
        },
        ...<target>: {
            <target data>
        },
        errors: <errors>
    }

Field Descriptions
###################

The top level ``target`` fields holds all the data sent with the message.

``target`` and ``target data``
    Typically specifies the Data Source identifier and the data payload sent by the extension.

``errors``
    Any errors that occurred while retrieving the data.

Sample Messages
##################

Sample messages sent by various sources, including `sample extensions <https://github.com/r52/quasar/tree/master/extensions>`_ and extension settings, and App Launcher command list:

.. code-block:: json

    {
        "win_simple_perf/sysinfo": {
            "cpu": 15,
            "ram": {
                "total": 34324512768,
                "used": 10252300288
            }
        }
    }

    {
        "win_audio_viz/settings": [
            {
                "def": 256,
                "desc": "FFTSize",
                "max": 8192,
                "min": 0,
                "name": "FFTSize",
                "step": 2,
                "type": "int",
                "val": 1024
            },
            {
                "def": 16,
                "desc": "Number of Bands",
                "max": 1024,
                "min": 0,
                "name": "Bands",
                "step": 1,
                "type": "int",
                "val": 32
            }
        ]
    }

Sample Usage
#############

This following sample is taken from the :doc:`widgetqs` documentation, and defines functions which processes incoming data sent by the `win_simple_perf sample extension <https://github.com/r52/quasar/tree/master/extensions/win_simple_perf>`_.

.. code-block:: javascript

    function parseMsg(msg) {
        const data = JSON.parse(msg);

        if ("win_simple_perf/sysinfo_polled" in data) {
            const vals = data["win_simple_perf/sysinfo_polled"]
            setData(document.getElementById("cpu"), vals["cpu"]);
            setData(
                document.getElementById("ram"),
                Math.round((vals["ram"]["used"] / vals["ram"]["total"]) * 100),
            );
        }
    }

.. _app-launcher-protocol:

App Launcher
--------------

The App Launcher follows the basic message formats as described above.

For example, sending the following message:

.. code-block:: json

    {
        method: "query",
        params: {
            topics: ["applauncher/list"]
        }
    }

Will see Quasar respond with the following sample reply:

.. code-block:: json

    {
        "applauncher/list": [{
            "command": "chrome",
            "icon": "data:image/png;base64,..."
        }, {
            "command": "spotify",
            "icon": "data:image/png;base64..."
        }, {
            "command": "steam",
            "icon": "data:image/png;base64..."
        }]
    }

Where ``chrome``, ``spotify``, and ``steam`` are commands preconfigured in the :doc:`App Launcher Settings <launcher>`. Subsequently, an App Launcher widget may then send:

.. code-block:: json

    {
        method: "query",
        params: {
            topics: ["applauncher/launch"],
            args: "chrome"
        }
    }

At which point the command/application registered with the App Launcher command ``chrome`` will then execute.

See :doc:`launcher` for details on setting up the App Launcher.
