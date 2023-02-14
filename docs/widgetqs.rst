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

The following example is the Widget Definition file of the `sample widget datetime <https://github.com/r52/quasar/tree/master/widgets/datetime>`_:

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

In order for widgets to be able to communicate with the Quasar Data Server and fetch data from installed Data Server extensions, its definition file must have the parameter ``dataserver`` defined with its value set to ``true``. This tells Quasar to load the connection and authentication scripts into the widget (see :doc:`wdef`).

Once this is done, we first need to open up a WebSocket connection with Quasar.

**Quasar widgets** can simply execute the following JavaScript:

.. code-block:: javascript

    let websocket = quasar_create_websocket();

``quasar_create_websocket()`` is a globally defined function only available to widgets loaded in Quasar when the ``dataserver`` parameter is set to ``true``. This function creates a WebSocket object connecting to Quasar's Data Server.

However, **external clients** must manually establish the connection to Quasar:

.. code-block:: javascript

    let websocket = new WebSocket("ws://127.0.0.1:<port>");

Where ``<port>`` is the port that the Data Server is running on, as set in :doc:`settings`.

Once the connection is established, we then need to authenticate with the Data Server to establish our widget's identity.

Similar to the above, **Quasar widgets** can achieve this simply by calling the (similarity defined) global function ``quasar_authenticate()`` in the WebSocket's ``onopen`` handler, supplying our ``websocket`` connection object as an argument:

.. code-block:: javascript

    websocket.onopen = function(evt) {
        quasar_authenticate(websocket);
    };

If the ``Allow only Quasar widgets to connect to the WebSocket server?`` setting is enabled, **external clients** will be unable to connect.

Once our widget is authenticated, we can start fetching data from a Data Source by placing a call to a data request function in the handler. For example:

.. code-block:: javascript

    websocket.onopen = function(evt) {
        quasar_authenticate(websocket);
        setInterval(poll, 5000);
    };

Where the function ``poll()`` can be something like:

.. code-block:: javascript

    function poll() {
        let msg = {
            method: "query",
            params: {
                topics: ["win_simple_perf/sysinfo_polled"]
            }
        }

        websocket.send(JSON.stringify(msg));
    }

The above example polls the Data Source ``sysinfo_polled`` provided by the sample extension `win_simple_perf <https://github.com/r52/quasar/tree/master/extensions/win_simple_perf>`_ every 5000ms.

How that we have configured the Data Sources we want to receive data from, we must now setup our data processing for the data we will receive. We start by implementing another handler on the WebSocket connection. For example:

.. code-block:: javascript

    websocket.onmessage = function(evt) {
        parseMsg(evt.data);
    };

We can then implement a function ``parseMsg()`` to process the incoming data. Refer to the :doc:`wcp` for the full message format:

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

We start by parsing the JSON message, then examining the object's fields to ensure that we have received what we wanted, namely the ``data["win_simple_perf/sysinfo_polled"]`` field, which is what we requested in the previous code examples. If everything matches, we finally process the payload. The ``cpu`` field in the data outputs a single integer containing the current CPU load percentage on your desktop, while the ``ram`` field contains the ``total`` and ``used`` RAM in bytes. We convert these numbers to a percentage if they are not already one, and output them to the HTML elements defined in the widget's ``index.html`` with the IDs ``cpu`` and ``ram`` respectively.

Putting everything together, your widget's script may end up looking something like this:

.. code-block:: javascript

    let websocket = null;

    function poll() {
        let msg = {
            method: "query",
            params: {
                topics: ["win_simple_perf/sysinfo_polled"]
            }
        }

        websocket.send(JSON.stringify(msg));
    }

    function setData(elm, value) {
        if (elm != null) {
            elm.setAttribute("aria-valuenow", value);
            elm.textContent = value + "%";
            elm.style.width = value + "%";
            elm.classList.remove("bg-success", "bg-info", "bg-warning", "bg-danger");

            if (value >= 80) {
                elm.classList.add("bg-danger");
            } else if (value >= 60) {
                elm.classList.add("bg-warning");
            } else {
                elm.classList.add("bg-success");
            }
        }
    }

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

    function ready(fn) {
        if (document.readyState !== "loading") {
            fn();
        } else {
            document.addEventListener("DOMContentLoaded", fn);
        }
    }


    ready(function() {
        try {
            if (websocket && websocket.readyState == 1)
                websocket.close();
            websocket = quasar_create_websocket();
            websocket.onopen = function(evt) {
                quasar_authenticate(websocket);
                setInterval(poll, 5000);
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
