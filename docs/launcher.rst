Setting up the App Launcher
============================

App Launcher widgets require additional setup in order to function properly. The installation instructions for an App Launcher widget may ask you to setup several App Launcher commands (or give you the option of setting up custom commands) so that apps can be launched from Quasar. App Launcher commands can be configured in the **Settings** menu under the **Launcher** page.

Example:

.. image:: https://i.imgur.com/sUPGVaM.png

In the image above, Quasar is configured to launch Chrome, Windows Explorer, Spotify, and Steam using the App Launcher commands ``chrome``, ``explorer``, ``spotify``, and ``steam`` respectively. A properly configured App Launcher widget will then be able to send these commands to Quasar in order to launch the configured applications.

For example, the ``sample_app_launcher`` that comes installed with Quasar requires the commands ``chrome``, ``spotify``, and ``steam`` to be configured in order to work properly. Of course, what the commands themselves execute can be completely arbitrary and is up to user discretion, so be sure to set them up properly!