win_simple_perf
====================

A sample extension for Quasar on Windows that provides basic performance metrics.

Data Sources
-------------

win_simple_perf provides two Data Sources: the subscription based ``sysinfo``, and client polled ``sysinfo_polled``. Both provide the same information.

``sysinfo`` refreshes at a default rate of 5000ms.

cpu field
~~~~~~~~~

Contains a single integer that is the current total CPU load percentage.

ram field
~~~~~~~~~

Contains the total (``total``) and currently used (``used``) memory as a JSON object, in bytes.

Sample Output
~~~~~~~~~~~~~

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
