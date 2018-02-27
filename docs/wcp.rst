Widget Client Protocol
======================

The Widget Client Protocol is the protocol that is used by Quasar and Quasar widgets to communicate with each other. The messages are JSON-encapsulated data that is sent over WebSocket. This document defines the message format and accepted fields, as well as sample usages in JavaScript.

.. contents::
   :local:

Globals Variables
------------------

These global JavaScript variables are defined for all Quasar loaded widgets:

``qWidgetName``
    Contains the name of this widget (as defined by its :doc:`Widget Definition file <wdef>`).

``qWsServerUrl``
    Contains the URL of the Quasar Data Server.

Sample Usage
~~~~~~~~~~~~~

.. code-block:: javascript

    websocket = new WebSocket(qWsServerUrl);

Data Protocol
--------------

Client to Server
~~~~~~~~~~~~~~~~~

The following is the message format used by a client widget to send messages to the Quasar Data Server.

Message Format
###############

::

    {
        widget: qWidgetName,
        type: <type>,
        plugin: <pluginCode>,
        source: <dataSrc>
    }


Field Descriptions
####################

``widget``
    This widget's name as defined by its :doc:`Widget Definition file <wdef>`. Use global variable ``qWidgetName``.

``type``
    The type of this message.
    Currently supported values are: ``subscribe``, ``poll``, and ``launcher``.
    ``subscribe`` is used to subscribe to timer-based or plugin signaled Data Sources, while ``poll`` is used for client polled Data Sources.
    ``launcher`` is used to send App Launcher commands. See :ref:`app-launcher-protocol` for more details.

``plugin``
    The target plugin's codename.

``source``
    For ``subscribe`` type: A comma delimited list of Data Source codenames to subscribe to.
    For ``poll`` type: A single data source codename to poll data from.

Sample Usage
#################

.. code-block:: javascript

    function subscribe() {
        var reg = {
            widget: qWidgetName,
            type: "subscribe",
            plugin: "win_simple_perf",
            source: "cpu,ram"
        };

        websocket.send(JSON.stringify(reg));
    }

This function is taken from the `sample widget simple_perf <https://github.com/r52/quasar/tree/master/widgets/simple_perf>`_, which subscribes the widget to Data Sources ``cpu`` and ``ram`` provided by the plugin `win_simple_perf <https://github.com/r52/quasar/tree/master/plugins/win_simple_perf>`_.


Server to Client
~~~~~~~~~~~~~~~~~~

The following is the message format used by the Data Server to send data and messages to client widgets.

Message Format
###############

::

    {
        type: "data",
        plugin: <pluginCode>,
        source: <dataSrc>,
        data: <data from plugin source>
    }

Field Descriptions
###################

``type``
    The type of this message.
    Currently supported values are: ``data``, ``settings``.

``plugin``
    The sender plugin's codename.

``source``
    The originator Data Source codename of this message.

``data``
    The data payload. The client is responsible for parsing this payload. Must be a valid JSON supported data type.
    If the ``type`` is ``settings``, then this field contains a JSON object which contains name-value pairs of the plugin's configuration, and the ``source`` field is omitted.

Sample Message
###############

This is a sample message sent by the `win_simple_perf plugin <https://github.com/r52/quasar/tree/master/plugins/win_simple_perf>`_:

.. code-block:: json

    {
        "type": "data",
        "plugin": "win_simple_perf",
        "source": "cpu",
        "data": "10"
    }

Sample Usage
#############

This following sample is taken from the `sample widget simple_perf <https://github.com/r52/quasar/tree/master/widgets/simple_perf>`_, and defines functions which processes incoming data sent by the `win_simple_perf plugin <https://github.com/r52/quasar/tree/master/plugins/win_simple_perf>`_.

.. code-block:: javascript

    function processData(data) {
        var val = data["data"];
        var $elm = null;

        switch (data["source"]) {
            case "cpu":
                $elm = $('#cpu');
                break;
            case "ram":
                val = Math.round((val["used"] / val["total"]) * 100);
                $elm = $('#ram');
                break;
            default:
                console.log("Unknown source type " + data["source"]);
                break;
        }

        if ($elm != null) {
            $elm.attr("aria-valuenow", val).text(val + "%").width(val + "%");
            $elm.removeClass("bg-success bg-info bg-warning bg-danger");

            if (val >= 80) {
                $elm.addClass("bg-danger");
            } else if (val >= 60) {
                $elm.addClass("bg-warning");
            } else {
                $elm.addClass("bg-success");
            }
        }
    }

    function parseMsg(msg) {
        var data = JSON.parse(msg);

        switch (data["type"]) {
            case "data":
                processData(data);
                break;
            default:
                console.log("Unsupported message type " + data["type"]);
                break;
        }
    }

.. _app-launcher-protocol:

App Launcher
--------------

The following is the message format used by a client widget to send App Launcher commands to Quasar.

Message Format
~~~~~~~~~~~~~~~

::

    {
        widget: qWidgetName,
        type: "launcher",
        app: <appcmd>
    }

Sample Usage
~~~~~~~~~~~~

Assuming ``websocket`` is connected:

.. code-block:: javascript

    var msg = {
        widget: qWidgetName,
        type: "launcher"
    };

    function launch(app)
    {
        msg["app"] = app;
        websocket.send(JSON.stringify(msg));
    }

Where ``app`` is a :doc:`registered command in Quasar Settings <launcher>`.

