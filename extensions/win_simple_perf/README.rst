win_simple_perf
====================

A sample extension for Quasar on Windows that provides basic performance metrics.

Data Sources
-------------

win_simple_perf provides two Data Sources, ``cpu``, and ``ram``, both of which can be polled by clients.

cpu
~~~~~

Outputs the current total CPU load percentage.

This source outputs a single integer that is the current total CPU load percentage.

ram
~~~~

Outputs the total and currently used memory on the system.

This source outputs the total (``total``) and currently used (``used``) memory as a JSON object, in bytes.

Sample Output
~~~~~~~~~~~~~

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
