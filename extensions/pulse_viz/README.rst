pulse_viz
=====================

A sample extension for Quasar on Linux that captures audio output from a PulseAudio server and provides various audio data.

This extension is adapted from the ``win_audio_viz`` sample extension, which itself is adapted from the `Rainmeter AudioLevel plugin <https://github.com/rainmeter/rainmeter/blob/master/Plugins/PluginAudioLevel/>`_ for Quasar usage.

Rainmeter is licensed under GPLv2.

Usage
-------------

pulse_viz provides only a subset of the functionality available in ``win_audio_viz`` for simplicity's sake.

See the `win_audio_viz extension <https://github.com/r52/quasar/tree/master/extensions/win_audio_viz>`_ for more details.

.. image:: https://i.imgur.com/YFQkZls.png

Use ``pavucontrol`` to set pulse_viz's monitoring device to that of your primary desktop audio device.

Data Sources
~~~~~~~~~~~~~~

- ``fft`` : The current FFT level (0.0 to 1.0) for all FFT bins. Subscription, default 16.67ms refresh.
- ``band`` : The current FFT level (0.0 to 1.0) for all bands. Subscription, default 16.67ms refresh.

Sample Output
###############

.. code-block:: json

    {
        "pulse_viz/band": [
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

pulse_viz provides most of settings available to the `Rainmeter AudioLevel plugin <https://docs.rainmeter.net/manual/plugins/audiolevel/>`_, with the exception of parameters which define specific data retrieval settings such as ``Channel``, ``FFTIdx``, and ``BandIdx``. The parameter ``Port`` is not supported in pulse_viz.

See the `Rainmeter AudioLevel documentation <https://docs.rainmeter.net/manual/plugins/audiolevel/>`_ for more details.
