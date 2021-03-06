Settings
===========

Right-click the Quasar icon in your notification bar, and click **Settings** to access the settings menu. Here, users can change advanced options for Quasar, as well as any custom options provided by extensions.

General Settings
-----------------

Secure WebSocket
    Sets whether the Data Server uses the secure (``wss``) or insecure (``ws``) WebSocket protocol. Since Quasar does not actually have a valid certificate (since it runs locally), external clients may require this setting to be turned **off** in order to successfully connect to the Data Server without triggering certificate errors. *(default On)*

Data Server port
    The port the WebSocket Data Server runs on. *(default 13337)*

Log Verbosity
    Severity of log messages that are logged. *(default Warning)*

cookies.txt
    A Netscape format cookies.txt can be pasted here. These cookies will be loaded by all loaded widgets.

Save log to file
    Sets whether log messages are written to a file.

Launch Quasar when system starts
    Launches Quasar when system starts *(Windows only)*


App Launcher Settings
----------------------

Commands available to App Launcher widgets can be configured under the **Launcher** page. See :doc:`Setting up the App Launcher <launcher>` for more information.

User Keys
-----------

External applications and widgets not loaded through Quasar can still connect to the Quasar Data Server through the use of **User Keys**. See the :doc:`userkeys` document for more details.
