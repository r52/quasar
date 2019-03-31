Setting up the App Launcher
============================

App Launcher widgets require additional setup in order to function properly. Specific App Launcher commands needs to be created and configured so that apps can be launched from Quasar. App Launcher commands can be configured in the **Settings** menu under the **Launcher** page.

Example:

.. image:: https://i.imgur.com/yEh9Cu9.png

In the image above, Quasar is configured to launch Chrome, Spotify, and Steam using the App Launcher commands ``chrome``, ``spotify``, and ``steam`` respectively. A properly configured App Launcher widget will receive the list of configured commands and will able to send these commands to Quasar in order to launch the configured applications.

.. image:: https://i.imgur.com/aTjp7gm.png

Furthermore, you can also specify your own custom icon for each of these commands. In the previous example, icons have been added for all three commands. The sample widget ``sample_app_launcher`` that comes installed with Quasar is preconfigured to use custom icons. Of course, what the commands themselves execute can be completely arbitrary and is up to user discretion, so be sure to set them up properly!
