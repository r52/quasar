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
            target: <target>,
            params: <target params>
        }
    }


Field Descriptions
####################

``method``
    The method/function to be invoked by this message.
    For client widgets, supported values are: ``subscribe``, and ``query``.
    ``subscribe`` is used to subscribe to timer-based or extension signaled Data Sources, while ``query`` is used for client polled Data Sources as well as any other commands.
    For external widgets and applications, ``auth`` is also supported for authenication purposes.

``params``
    The parameters sent to the method.
    This field should be a JSON object that is typically comprised of two fields: ``target`` and ``params``. If the method is ``auth``, then it should be comprised of a single field ``code`` that is set to the :doc:`User Key <userkeys>` being used.

``target``
    The intended target of the message.
    Typically, this is an extension's identifier.
    For App Launcher widgets, use target name ``launcher``.
    See :ref:`app-launcher-protocol` for more details.

``target params``
    Parameters sent to the target.
    Typically, these are Data Source identifiers.
    For App Launcher widgets, these are App Launcher commands, or ``get`` to retrieve the command list.

Sample Usages
#################

.. code-block:: javascript

    function subscribe() {
        var msg = {
            "method": "subscribe",
            "params": {
                "target": "win_audio_viz",
                "params": "band"
            }
        }

        websocket.send(JSON.stringify(msg));
    }

    function poll() {
        var msg = {
            "method": "query",
            "params": {
                "target": "win_simple_perf",
                "params": "cpu,ram"
            }
        }

        websocket.send(JSON.stringify(msg));
    }

    function get_launcher_list() {
        var msg = {
            "method": "query",
            "params": {
                "target": "launcher",
                "params": "get"
            }
        }

        websocket.send(JSON.stringify(msg));
    }

    function launcher_cmd(cmd) {
        var msg = {
            "method": "query",
            "params": {
                "target": "launcher",
                "params": cmd
            }
        }

        websocket.send(JSON.stringify(msg));
    }

    function authenticate() {
        var msg = {
            "method": "auth",
            "params": {
                "code": "6EFBBE6542D52FDD294337343147B033"
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
        data: {
            <target>: <target data>
        },
        errors: <errors>
    }

Field Descriptions
###################

The top level data field holds all the data sent with the message.

``target`` and ``target data``
    Typically specifies the extension identifier and the data payload sent by the extension.

``errors``
    Any errors that occurred while retrieving the data.

Sample Message
###############

Sample messages sent by various sources, including `sample extensions <https://github.com/r52/quasar/tree/master/extensions>`_ and extension settings, and App Launcher command list:

.. code-block:: json

    {
        "data": {
            "win_simple_perf": {
                "cpu": 15,
                "ram": {
                    "total": 34324512768,
                    "used": 10252300288
                }
            }
        }
    }

    {
        "data": {
            "win_simple_perf": {
                "cpu": 36
            }
        },
        "errors": ["Unknown data source band requested in extension win_simple_perf"]
    }

    {
        "data": {
            "settings": {
                "win_audio_viz": {
                    "rates": [{
                        "enabled": true,
                        "name": "fft",
                        "rate": 100
                    }, {
                        "enabled": true,
                        "name": "band",
                        "rate": 100
                    }],
                    "settings": [{
                        "def": 256,
                        "desc": "FFTSize",
                        "max": 8192,
                        "min": 0,
                        "name": "FFTSize",
                        "step": 2,
                        "type": "int",
                        "val": 1024
                    }, {
                        "def": 16,
                        "desc": "Number of Bands",
                        "max": 1024,
                        "min": 0,
                        "name": "Bands",
                        "step": 1,
                        "type": "int",
                        "val": 32
                    }]
                }
            }
        }
    }

Sample Usage
#############

This following sample is taken from the :doc:`widgetqs` documentation, and defines functions which processes incoming data sent by the `win_simple_perf sample extension <https://github.com/r52/quasar/tree/master/extensions/win_simple_perf>`_.

.. code-block:: javascript

    function parseMsg(msg) {
        var data = JSON.parse(msg);

        if ("data" in data && "win_simple_perf" in data["data"] && "cpu" in data["data"]["win_simple_perf"]) {
            var val = data["data"]["win_simple_perf"]["cpu"]
            $('#cpu').text(val + "%");
        }
    }

.. _app-launcher-protocol:

App Launcher
--------------

The App Launcher follows the basic message formats as described above.

For example, sending the following message:

.. code-block:: json

    {
        "method": "query",
        "params": {
            "target": "launcher",
            "params": "get"
        }
    }

Will see Quasar respond with the following sample reply:

.. code-block:: json

    {
        "data": {
            "launcher": [{
                "command": "chrome",
                "icon": "data:image/svg+xml;base64,..."
            }, {
                "command": "spotify",
                "icon": "data:image/svg+xml;base64..."
            }, {
                "command": "steam",
                "icon": "data:image/svg+xml;base64..."
            }]
        }
    }

Where ``chrome``, ``spotify``, and ``steam`` are commands preconfigured in the :doc:`App Launcher Settings <launcher>`. Subsequently, an App Launcher widget may then send:

.. code-block:: json

    {
        "method": "query",
        "params": {
            "target": "launcher",
            "params": "chrome"
        }
    }

At which point the command/application registered with the App Launcher command ``chrome`` will then execute.

See :doc:`launcher` for details on setting up the App Launcher.
