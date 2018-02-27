Creating a Plugin Quickstart
=============================

.. contents::
    :local:

.. note::

    See :doc:`api`, :doc:`wcp`, and the sample plugin `win_simple_perf <https://github.com/r52/quasar/tree/master/plugins/win_simple_perf>`_ for more details.

.. _plugqs_start:

Getting Started
----------------

Quasar implements a WebSocket-based Data Server that facilitates the communication of various data that isn't available in a web-only context to widgets. The Data Server can be extended by **plugins** that provide additional Data Sources that can be used by widgets. The :doc:`Plugin API <api>` is provided as a pure-C interface, so plugins can be written in languages that support implementing C interfaces and produces a native library. This guide assumes that the language of choice is C++.

To begin, your plugins must include :ref:`plugin_api_h` (which includes :ref:`plugin_types_h`) as well as :ref:`plugin_support_h`.

At minimum, a plugin needs to implement 3 functions in additional to :cpp:func:`quasar_plugin_load()` and :cpp:func:`quasar_plugin_destroy()`.

These are:

* ``bool init(quasar_plugin_handle handle)``

* ``bool shutdown(quasar_plugin_handle handle)``

* ``bool get_data(size_t uid, quasar_data_handle dataHandle)``

Within the :cpp:class:`quasar_plugin_info_t` structure.

For each Data Dource provided by the plugin, a :cpp:class:`quasar_data_source_t` entry needs to be created that contains the Data Source's codename and refresh rate in milliseconds or polling style. :cpp:member:`quasar_data_source_t::uid` should be initialized to ``0``.

The entries should be propagated in :cpp:member:`quasar_plugin_info_t::numDataSources` and :cpp:member:`quasar_plugin_info_t::dataSources`.

The rest of the static data fields in :cpp:class:`quasar_plugin_info_t` such as :cpp:member:`quasar_plugin_info_t::name`, :cpp:member:`quasar_plugin_info_t::code`, and :cpp:member:`quasar_plugin_info_t::version` should be filled in with the plugin's basic information.

Example
~~~~~~~~~

Adapted from the sample plugin `win_simple_perf <https://github.com/r52/quasar/tree/master/plugins/win_simple_perf>`_:

.. code-block:: cpp

    quasar_data_source_t sources[2] =
        {
            { "cpu", 5000, 0 },
            { "ram", 5000, 0 }
        };

    quasar_plugin_info_t info =
        {
            QUASAR_API_VERSION,                                     // int api_version, should always be QUASAR_API_VERSION
            "Simple Performance Query",                             // char name[64]
            "win_simple_perf",                                      // char code[16]
            "v1",                                                   // char version[64]
            "me",                                                   // char author[64]
            "Sample plugin that queries basic performance numbers", // char description[256]

            std::size(sources),                                     // size_t numDataSources
            sources,                                                // quasar_data_source_t* dataSources

            simple_perf_init,                                       // bool init(quasar_plugin_handle handle)
            simple_perf_shutdown,                                   // bool shutdown(quasar_plugin_handle handle)
            simple_perf_get_data,                                   // bool get_data(size_t uid, quasar_data_handle dataHandle)
            nullptr,                                                // quasar_settings_t* create_settings()
            nullptr                                                 // void update(quasar_settings_t* settings)
        };

In this example, 2 Data Sources are defined, ``cpu`` and ``ram``, each with a refresh time of 5000ms. The functions ``simple_perf_init()``, ``simple_perf_shutdown()``, and ``simple_perf_get_data()`` are the implementations of ``init()``, ``shutdown()``, and ``get_data()`` respectively. Note that ``create_settings()`` and ``update()`` are not implemented by this plugin. These functions are optional, and only needs to be implemented if the plugin provides custom settings. See :ref:`plugqs_custom` for more information.

quasar_plugin_load()
~~~~~~~~~~~~~~~~~~~~~

This function should return a pointer to a populated :cpp:class:`quasar_plugin_info_t` structure.

Following previous example:

.. code-block:: cpp

    quasar_plugin_info_t* quasar_plugin_load(void)
    {
        return &info;
    }

Since the ``quasar_plugin_info_t info`` structure is defined statically in the previous example, it is suffice for ``quasar_plugin_load()`` to simply return the pointer to it.

init()
~~~~~~~~

If the plugin was loaded successfully, each Data Source entry's :cpp:member:`quasar_data_source_t::uid` is filled with a unique identifier. These are used in the ``get_data()`` function call to identify the Data Source being requested. It is up to the plugin to remember these during ``init()`` as they will be referred to by future ``get_data()`` calls from Quasar.

This function should also allocate or initialize any other resources needed, as well as remember the plugin handle if necessary.

.. code-block:: cpp

    bool simple_perf_init(quasar_plugin_handle handle)
    {
        plugHandle = handle;

        // Process uid entries.
        if (sources[0].uid == 0)
        {
            // "cpu" Data Source didn't get a uid
            return false;
        }

        if (sources[1].uid == 0)
        {
            // "ram" Data Source didn't get a uid
            return false;
        }

        return true;
    }

shutdown()
~~~~~~~~~~~~

This function should deallocate and clean up any resources allocated in ``init()``, including waiting on any threads spawned. Since we have no allocations in our sample ``init()`` function, our ``shutdown()`` can simply return.

.. code-block:: cpp

    bool simple_perf_shutdown(quasar_plugin_handle handle)
    {
        return true;
    }

get_data()
~~~~~~~~~~~

This function is responsible for retrieving the data requested by the ``uid`` argument and populating it into the ``quasar_data_handle`` handle using functions from :ref:`plugin_support_h`.

.. note::

    This function needs to be both re-entrant and thread-safe!


.. code-block:: cpp

    bool getCPUData(quasar_data_handle hData)
    {
        double cpu = GetCPULoad() * 100.0;

        quasar_set_data_string(hData, std::to_string((int) cpu).c_str());

        return true;
    }

    bool getRAMData(quasar_data_handle hData)
    {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
        DWORDLONG physMemUsed  = memInfo.ullTotalPhys - memInfo.ullAvailPhys;

        std::stringstream ss;
        ss << "{ \"total\": " << totalPhysMem << ", \"used\": " << physMemUsed << " }";

        quasar_set_data_json(hData, ss.str().c_str());

        return true;
    }

    bool simple_perf_get_data(size_t uid, quasar_data_handle hData)
    {
        // the "cpu" source
        if (uid == sources[0].uid)
        {
            return getCPUData(hData);
        }
        // the "ram" source
        else if (uid == sources[1].uid)
        {
            return getRAMData(hData);
        }

        return false;
    }

See :ref:`plugin_support_h` for all supported data types.

.. _plugqs_models:

Data Models
--------------

Quasar supports three different types of data models for Data Sources.

By default, Data Sources in Quasar operate on a timer-based subscription model.

This can be changed by initializing :cpp:member:`quasar_data_source_t::refreshMsec` of a Data Source entry to different values. A positive value means the default timer-based subscription. A value of ``0`` means the client widget is responsible for polling the plugin for new data. A value of ``-1`` means the plugin will signal when new data becomes available (i.e. from a thread) and automatically send the new data to all subscribed widgets.

See :doc:`wcp` for details on client message formats.

Timer-based Subscription
~~~~~~~~~~~~~~~~~~~~~~~~~

Enabled by initializing :cpp:member:`quasar_data_source_t::refreshMsec` of a Data Source entry to a positive value.

Multiple client widgets may subscribe to a single data source, which is polled for new data every :cpp:member:`quasar_data_source_t::refreshMsec` milliseconds. This new data is then propagated to every subscribed widget.

The sample code in the above sections are based on this model.

Signal-based Subscription
~~~~~~~~~~~~~~~~~~~~~~~~~~

Enabled by initializing :cpp:member:`quasar_data_source_t::refreshMsec` to ``-1``.

This model supports Data Sources which require inconsistent timing, as well as Data Sources which require background processing, such as a producer-consumer thread.

To use this model, utilize the functions :cpp:func:`quasar_signal_data_ready()` and :cpp:func:`quasar_signal_wait_processed()` in :ref:`plugin_support_h`.

For example:

.. code-block:: cpp

    quasar_data_source_t sources[2] =
        {
          { "some_thread_source", -1, 0 },
          { "some_timer_source", 5000, 0 }
        };

    quasar_plugin_handle pluginHandle = nullptr;
    std::atomic_bool running = true;

    void workerThread()
    {
        while (running)
        {
            // do the work
            ...

            // signal that data is ready
            quasar_signal_data_ready(pluginHandle, "some_thread_source");

            // call this function if the thread needs to wait for the data to be consumed
            // before processing new data
            quasar_signal_wait_processed(pluginHandle, "some_thread_source");
        }
    }

    bool init_func(quasar_plugin_handle handle)
    {
        pluginHandle = handle;

        std::thread work(workerThread);
        work.detach();

        return true;
    }

    bool shutdown_func(quasar_plugin_handle handle)
    {
        running = false;

        // Sleep for a while to ensure thread exits
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        return true;
    }

Client Polling
~~~~~~~~~~~~~~~

Enabled by initializing :cpp:member:`quasar_data_source_t::refreshMsec` to ``0``.

This data model transfers the responsibility of polling for new data to the client widget. The data source will no longer accept subscribers.

Example:

.. code-block:: cpp

    quasar_data_source_t sources[2] =
        {
          { "some_polled_source", 0, 0 },
          { "some_timer_source", 5000, 0 }
        };


From the client:

.. code-block:: javascript

    function poll() {
        var reg = {
            widget: qWidgetName,
            type: "poll",
            plugin: "some_plugin",
            source: "some_polled_source"
        };

        websocket.send(JSON.stringify(reg));
    }


This model also allows the plugin to signal data ready using :cpp:func:`quasar_signal_data_ready()` for an asynchronous poll request/response timing.

.. _plugqs_custom:

Custom Settings
-------------------

By default, users can enable or disable a Data Source as well as change its refresh rate from the :doc:`settings` dialog.

However, a plugin can provide further custom settings by utilizing the :ref:`plugin_support_h` API and implementing the ``create_settings()`` and ``update()`` functions in :cpp:class:`quasar_plugin_info_t`. These custom settings will appear under the Settings dialog.

Sample code:

.. code-block:: cpp

    quasar_settings_t* create_custom_settings()
    {
        quasar_settings_t* settings = quasar_create_settings();
        quasar_add_bool(settings, "s_levelenabled", "Process Level:", true);
        quasar_add_int(settings, "s_level", "Level:", 1, 30, 1, 1);

        return settings;
    }

    void custom_settings_update(quasar_settings_t* settings)
    {
        g_levelenabled = quasar_get_bool(settings, "s_levelenabled");
        g_level = quasar_get_int(settings, "s_level");
    }

