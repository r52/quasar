User Keys
============

.. image:: https://i.imgur.com/lzTUHus.png

**User Keys** are generated authentication codes that will allow external applications or widgets to connect to Quasar. Each User Key only allows a single simultaneous connection to Quasar.

External clients must authenticate to Quasar upon establishing a WebSocket connection by sending an ``auth`` method request along with a User Key generated here in order to authenticate to Quasar. See :doc:`widgetqs` for samples on how to establish a connection to Quasar, or :doc:`wcp` for the complete Widget Client Protocol reference.
