win_simple_perf
====================

A sample plugin for Quasar on Windows that provides basic performance metrics.

Data Sources
-------------

win_simple_perf provides two Data Sources, ``cpu``, and ``ram``.

cpu
~~~~~

Outputs the current total CPU load percentage. Refreshes every 5000ms (5 seconds).

Data Format
############

This source outputs a single integer that is the current total CPU load percentage.

Sample message:

.. code-block:: json

    {
        "type": "data",
        "plugin": "win_simple_perf",
        "source": "cpu",
        "data": 46
    }

ram
~~~~

Outputs the total and currently used memory on the system. Refreshes every 5000ms (5 seconds).

Data Format
############

This source outputs the total (``total``) and currently used (``used``) memory as a JSON object, in bytes.

Sample message:

.. code-block:: json

    {
        "type": "data",
        "plugin": "win_simple_perf",
        "source": "ram",
        "data": {
            "total": 34324574208,
            "used": 8168804352
        }
    }
