win_audio_viz
=====================

A sample extension for Quasar on Windows that captures audio device output and provides various audio data.

This extension is adapted from the `Rainmeter AudioLevel plugin <https://github.com/rainmeter/rainmeter/blob/master/Plugins/PluginAudioLevel/>`_ for Quasar usage.

Rainmeter is licensed under GPLv2.

Usage
-------------

win_audio_viz provides all functionality available to the `Rainmeter AudioLevel plugin <https://docs.rainmeter.net/manual/plugins/audiolevel/>`_, except not at a per channel/band level. Instead, win_audio_viz will typically provide the entire spectrum of values from a source as an array.

See the `Rainmeter AudioLevel documentation <https://docs.rainmeter.net/manual/plugins/audiolevel/>`_ for more details.

Data Sources
~~~~~~~~~~~~

win_audio_viz provides all options available in `Rainmeter AudioLevel's <https://docs.rainmeter.net/manual/plugins/audiolevel/>`_ ``Type`` setting as a Data Source. These are:

- ``rms`` : The current RMS level (0.0 to 1.0) for all channels. Subscription, default 16.67ms refresh.
- ``peak`` : The current Peak level (0.0 to 1.0) for all channels. Subscription, default 16.67ms refresh.
- ``fft`` : The current FFT level (0.0 to 1.0) for all FFT bins. Subscription, default 16.67ms refresh.
- ``fftfreq`` : The frequency in Hz for each FFT bin. Client polled.
- ``band`` : The current FFT level (0.0 to 1.0) for all bands. Subscription, default 16.67ms refresh.
- ``bandfreq`` : The frequency in Hz for all bands. Client polled.
- ``format`` : A string describing the audio format of the device connected to. Client polled.
- ``dev_status`` : Status (bool - true/false) of the device connected to. Client polled.
- ``dev_name`` : A string with the name of the device connected to. Client polled.
- ``dev_id`` : A string with the Windows ID of the device connected to. Client polled.
- ``dev_list`` : A string with a list of all available device IDs. Client polled.

Sample Output
#############

.. code-block:: json

    {
        "win_audio_viz/band": [
            0.123123,
            0.237892,
            0.83792,
            0.37855,
            0.382793,
            0.38927,
            0.72893,
            0.83792,
            0.8327492,
            0.3827938,
            0.84641651,
            0.62826286,
            0.6654456,
            0.4864866,
            0.1691962,
            0.8641233
        ]
    }

Settings
----------

win_audio_viz provides the same set of settings available to the `Rainmeter AudioLevel plugin <https://docs.rainmeter.net/manual/plugins/audiolevel/>`_, with the exception of parameters which define specific data retrieval settings such as ``Channel``, ``FFTIdx``, and ``BandIdx``. The parameter ``Port`` is not supported in win_audio_viz.

See the `Rainmeter AudioLevel documentation <https://docs.rainmeter.net/manual/plugins/audiolevel/>`_ for more details.
