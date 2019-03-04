win_audio_viz
=====================

A sample plugin for Quasar on Windows that captures the default audio playback device and outputs frequency band data.

Data Source
-------------

win_audio_viz provides a single Data Source ``viz``.

viz
~~~~~

Outputs the frequency band data of the system's default audio playback device. Refreshes every ~10 audio buffers (approx. ~100ms).

Data Format
#############

This source outputs frequency band data as an array of floats that represent the amplitude of the band. The number of bands is configurable through Quasar Settings.

Sample message:

.. code-block:: json

    {
        "type": "data",
        "plugin": "win_audio_viz",
        "source": "viz",
        "data": [0.123123, 0.237892, 0.83792, 0.37855, 0.382793, 0.38927, 0.72893, 0.83792, 0.8327492, 0.3827938]
    }

Settings
----------

This plugin provides several user configurable settings:

FFT Size (default: 256)
    Frequency resolution of the output data. The value should be a power of 2.

    **Note:** Higher values will incur higher CPU usage.

Sensitivity (default: 50)
    Sensitivity of the data. Higher values mean the data is more responsive to quieter sounds.

Band Frequency Min (Hz) (default: 20.0)
    Minimum frequency band.

Band Frequency Max (Hz) (default: 20000.0)
    Maximum frequency band.

Number of Bands (default: 32)
    Number of frequency bands to generate.

