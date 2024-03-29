Logging
=================

Quasar provides logging for debugging purposes, where all log messages sent to Quasar are outputted.

The log window can be accessed by right-clicking the Quasar icon in the notification bar, and clicking **Log**. Here, the log messages as well as its time, severity, and in some cases, the location of its source code can be inspected.

If the **Log to file?** setting is enabled in :doc:`settings`, then all log messages will be written to a file as well (typically in ``%AppData%\quasar\quasar.log`` on Windows). Similarily, the **Log Level** setting controls the minimum severity of log messages that gets outputted.

Quasar log will output log messages from the main Quasar application, extensions, as well as widgets. For widgets, Quasar hooks into the JavaScript console ``console.log()``. For extensions, the function :cpp:func:`quasar_log()` is provided in the :doc:`Extension API <api>`.
