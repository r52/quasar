Creating a Widget
======================

The Basics
------------

.. note::

    See :doc:`wdef` for the complete Widget Definition file reference.

Widgets can be anything that can be loaded in Chromium. This includes, but is not limited to, webpages and webapps, files, URLs, media files such as videos or music etc. At its most basic form, a widget is a simple HTML webpage. To create a Quasar Widget, we must first start with a Widget Definition file.

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

The following example is the Widget Definition file of the `sample widget datetime <https://github.com/r52/quasar/tree/master/widgets/datetime>`_ installed with Quasar:

.. code-block:: json

    {
        "name": "DateTime",
        "width": 470,
        "height": 220,
        "startFile": "index.html",
        "transparentBg": true
    }


The ``height`` and ``width`` parameters determine the size of the viewport of the widget window, while the ``startFile`` parameter provides the entry point for the widget. In this case it refers to the file ``index.html``. The `contents of index.html <https://github.com/r52/quasar/blob/master/widgets/datetime/index.html>`_ in the case of the sample widget ``datetime`` is a simple webpage that queries the current date and time and displays it (refer to its `source code <https://github.com/r52/quasar/tree/master/widgets/datetime>`_ for more information).

As mentioned above, the contents of a widget can be as simple as a text page, or as complex as a WebGL app. Quasar is only limited by the contents that the Chromium engine is capable of displaying. Unfortunately, a tutorial for frontend development is beyond the scope of this guide, but the web is full of resources that will help design your widget, no matter how complex it may be.

Communicating with the Data Server
------------------------------------

.. note::

    See :doc:`wcp` for the complete Widget Client Protocol reference.

.. note::

    The following code samples are adapted from the sample widget ``simple_perf``. Refer to its `source code <https://github.com/r52/quasar/tree/master/widgets/simple_perf>`_ for more information.

.. note::

    The following JavaScript code samples assume `jQuery <https://jquery.com/>`_ is being utilized.

In order to communicate with the Data Server and fetch data from installed Data Server plugins, we first need to open up a WebSocket connection with Quasar.

We can do this with the following JavaScript:

.. code-block:: javascript

    var websocket = new WebSocket(qWsServerUrl);

``qWsServerUrl`` is a globally defined variable only available to widgets loaded in Quasar. This variable contains the URL for Quasar's Data Server.

Once the connection is established, we can start fetching data by subscribing to a Data Source. We can do so first by implementing a handler on the WebSocket connection:

.. code-block:: javascript

    websocket.onopen = function(evt) {
        subscribe();
    };

Where the function ``subscribe()`` can be something like:

.. code-block:: javascript

    function subscribe() {
        var reg = {
            widget: qWidgetName,
            type: "subscribe",
            plugin: "win_simple_perf",
            source: "cpu"
        };

        websocket.send(JSON.stringify(reg));
    }

The above example subscribes the widget to Data Source ``cpu`` provided by the plugin `win_simple_perf <https://github.com/r52/quasar/tree/master/plugins/win_simple_perf>`_. The variable ``qWidgetName`` is also a globally defined variable available to widgets loaded in Quasar. It contains the name of your widget as defined in the Widget Definition file.

How that we have subscribed to a Data Source, we can begin receiving data from the source. To do that, we start by implementing another handler on the WebSocket connection:

.. code-block:: javascript

    websocket.onmessage = function(evt) {
        parseMsg(evt.data);
    };

We can then implement the function ``parseMsg()`` to process the incoming data. Refer to the :doc:`wcp` for the full message format:

.. code-block:: javascript

    function parseMsg(msg) {
        var data = JSON.parse(msg);

        switch (data["type"]) {
            case "data":
                if (data["plugin"] == "win_simple_perf" && data["source"] == "cpu") {
                    var val = data["data"];
                    $('#cpu').text(val + "%");
                }
                break;
            default:
                console.log("Unsupported message type " + data["type"]);
                break;
        }
    }

We start by parsing the JSON message, then examining the ``type`` attribute of the message. Since we've only subscribed to a single Data Source that outputs very simple data, our function here is only expected to receive messages with ``type: "data"``. If the message that receive is indeed ``type: "data"``, we proceed by checking whether the ``plugin`` and ``source`` attributes matches up with what we've subscribed to, namely ``win_simple_perf`` and ``cpu`` respectively. If everything matches, we finally process the payload under the attribute ``data``. Since we know that the ``cpu`` Data Source only outputs a single integer containing the current CPU load on your desktop, we simply output that to the HTML element with the id ``cpu`` using jQuery.

Putting everything together, your widget's script may end up looking something like this:

.. code-block:: javascript

    var websocket = null;

    function subscribe() {
        var reg = {
            widget: qWidgetName,
            type: "subscribe",
            plugin: "win_simple_perf",
            source: "cpu"
        };

        websocket.send(JSON.stringify(reg));
    }

    function parseMsg(msg) {
        var data = JSON.parse(msg);

        switch (data["type"]) {
            case "data":
                if (data["plugin"] == "win_simple_perf" && data["source"] == "cpu") {
                    var val = data["data"];
                    $('#cpu').text(val + "%");
                }
                break;
            default:
                console.log("Unsupported message type " + data["type"]);
                break;
        }
    }

    $(document).ready(function() {
        try {
            if (websocket && websocket.readyState == 1)
                websocket.close();
            websocket = new WebSocket(qWsServerUrl);
            websocket.onopen = function(evt) {
                subscribe();
            };
            websocket.onmessage = function(evt) {
                parseMsg(evt.data);
            };
            websocket.onerror = function(evt) {
                console.log('ERROR: ' + evt.data);
            };
        } catch (exception) {
            console.log('Exception: ' + exception);
        }
    });
