Logging
=================

Quasar provides logging for debugging purposes, where all log messages sent to Quasar are outputted.

The log window can be accessed by right-clicking the Quasar icon in the notification bar, and clicking **Log**. Here, the log messages as well as its time, severity, and in some cases, the location of its source code can be inspected.

If the **Save log to file** setting is enabled in :doc:`settings`, then all log messages will be written to a file as well (typically in ``%AppData%\Quasar\Quasar.log`` on Windows). Similarily, the **Log Verbosity** setting controls the minimum severity of log messages that gets outputted.

Quasar log will output log messages from the main Quasar application, extension plugins, as well as widgets. For widgets, Quasar hooks into the JavaScript console ``console.log()``. For plugins, the function :cpp:func:`quasar_log()` is provided in the :doc:`Plugin API <api>`.
