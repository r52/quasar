/*! \file
    \brief Defines the DataPlugin class and supporting data structures
*/

#pragma once

#include <qstring_hash_impl.h>

#include <plugin_types.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

#include <QObject>
#include <QTimer>

//! Internal setting prefix for Data Source enabled UI toggle
#define QUASAR_DP_ENABLED_PREFIX "enabled_"

//! Internal setting prefix for Data Source refresh rate UI
#define QUASAR_DP_REFRESH_PREFIX "refresh_"

//! Internal setting prefix for custom plugin defined settings
#define QUASAR_DP_CUSTOM_PREFIX "custom_"

#ifdef PLUGINAPI_LIB
#    define PAPI_EXPORT Q_DECL_EXPORT
#else
#    define PAPI_EXPORT Q_DECL_IMPORT
#endif // PLUGINAPI_LIB

QT_FORWARD_DECLARE_CLASS(QWebSocket)

/*! Holds the mutex and accompanying conditional variable for
    asynchronous or plugin signaled Data Sources.
*/
struct DataLock
{
    std::mutex              mutex;             //!< Mutex
    std::condition_variable cv;                //!< Conditional varaible
    bool                    processed = false; //!< Bool value to trigger conditional variable notification
};

//! Struct containing internal resources for a Data Source
struct DataSource
{
    bool                      enabled;     //!< Whether this data source is enabled
    QString                   key;         //!< Data Source codename
    size_t                    uid;         //!< Data Source uid
    int64_t                   refreshmsec; //!< Data Source refresh rate \sa quasar_data_source_t.refreshMsec
    std::unique_ptr<QTimer>   timer;       //!< QTimer for timer based Data Sources
    std::set<QWebSocket*>     subscribers; //!< Set of widgets (its WebSocket connection instance) subscribed to this Data Source
    std::unique_ptr<DataLock> locks;       //!< Mutex/cv for asynchronous or plugin signaled Data Sources \sa DataLock
};

//! Shorthand for m_datasources type
using DataSourceMapType = std::unordered_map<QString, DataSource>;

//! Class that encapsulates a Quasar plugin library (dll/so)
class PAPI_EXPORT DataPlugin : public QObject
{
    Q_OBJECT;

public:
    //! Shorthand type for quasar_plugin_load()
    using plugin_load = std::add_pointer_t<quasar_plugin_info_t*(void)>;

    //! Shorthand type for quasar_plugin_destroy()
    using plugin_destroy = std::add_pointer_t<void(quasar_plugin_info_t*)>;

    //! DataPlugin destructor
    ~DataPlugin();

    //! Data Source uid counter
    static uintmax_t _uid;

    //! Load a plugin
    /*!
        \param[in]  libpath Path to library file
        \param[in]  parent  Parent element in Qt object tree
        \return Pointer to a DataPlugin instance if successful, nullptr otherwise
    */
    static DataPlugin* load(QString libpath, QObject* parent = Q_NULLPTR);

    //! Adds a subscriber to a Data Source
    /*!
        \param[in]  source      Data Source codename
        \param[in]  subscriber  Subscriber widget's websocket connection instance
        \param[in]  widgetName  Widget name
        \return true if successful, false otherwise
    */
    bool addSubscriber(QString source, QWebSocket* subscriber, QString widgetName);

    //! Removes a subscriber from all Data Sources
    /*! Invoked when a widget is closed or disconnects
        \param[in]  subscriber  Subscriber widget's websocket connection instance
    */
    void removeSubscriber(QWebSocket* subscriber);

    //! Polls the plugin for data and sends it to the requesting widget
    /*! Called when the plugin receives a widget "poll" request
        \param[in]  source      Data Source codename
        \param[in]  subscriber  Requesting widget's websocket connection instance
        \param[in]  widgetName  Widget name
    */
    void pollAndSendData(QString source, QWebSocket* subscriber, QString widgetName);

    //! Retrieves data from the plugin and sends it to all subscribers
    /*! Called when plugin data is ready to be sent (by both timer and signal)
        \param[in]  source  Data Source
    */
    void sendDataToSubscribers(DataSource& source);

    /*! Gets path to library file
        \return path to library file
    */
    QString getLibPath() { return m_libpath; };

    /*! Gets plugin name
        \return plugin name
    */
    QString getName() { return m_name; };

    /*! Gets plugin codename
        \return plugin codename
    */
    QString getCode() { return m_code; };

    /*! Gets plugin description
        \return plugin description
    */
    QString getDesc() { return m_desc; };

    /*! Gets plugin author
        \return plugin author
    */
    QString getAuthor() { return m_author; };

    /*! Gets plugin version
        \return plugin version
    */
    QString getVersion() { return m_version; };

    /*! Gets plugin settings prefix for use with internal Quasar settings store
        \return plugin settings prefix
    */
    QString getSettingsCode(QString key) { return "plugin_" + getCode() + "/" + key; };

    /*! Gets plugin custom settings
        \sa quasar_settings_t
        \return pointer to plugin custom settings
    */
    quasar_settings_t* getSettings() { return m_settings.get(); };

    /*! Gets plugin Data Sources
        \sa DataSourceMapType, DataSource
        \return reference to map of Data Sources
    */
    DataSourceMapType& getDataSources() { return m_datasources; };

    /*! Set Data Source enabled/disabled
        \param[in]  source  Data Source codename
        \param[in]  enabled Enabled
        \sa DataSource.enabled
    */
    void setDataSourceEnabled(QString source, bool enabled);

    /*! Set Data Source refresh rate (for timed Data Sources)
        \param[in]  source  Data Source codename
        \param[in]  msec    Refresh rate (ms)
        \sa DataSource.refreshmsec, DataSource.timer
    */
    void setDataSourceRefresh(QString source, int64_t msec);

    /*! Sets an integer custom plugin setting.
        This function is only called by the relevant GUI can should not be used otherwise.

        \param[in]  name    Custom setting name
        \param[in]  val     New value

        \sa quasar_settings_t
    */
    void setCustomSetting(QString name, int val);

    /*! Sets a double custom plugin setting.
        This function is only called by the relevant GUI can should not be used otherwise.

        \param[in]  name    Custom setting name
        \param[in]  val     New value

        \sa quasar_settings_t
    */
    void setCustomSetting(QString name, double val);

    /*! Sets a bool custom plugin setting.
        This function is only called by the relevant GUI can should not be used otherwise.

        \param[in]  name    Custom setting name
        \param[in]  val     New value

        \sa quasar_settings_t
    */
    void setCustomSetting(QString name, bool val);

    /*! Propagates custom setting value changes to the plugin
        as well as all unique subscribers

        \sa quasar_plugin_info_t.update
    */
    void updatePluginSettings();

    /*! Emits the data ready signal

        \param[in]  source  Data Source codename
        \sa quasar_signal_data_ready()
    */
    void emitDataReady(QString source);

    /*! Waits for a set of data to be sent to clients before processing the next set

        \param[in]  source  Data Source codename
        \sa quasar_signal_wait_processed()
    */
    void waitDataProcessed(QString source);

signals:
    /*! Qt data ready signal
        \param[in]  source  Data Source codename
        \sa emitDataReady(), quasar_signal_data_ready()
    */
    void dataReady(QString source);

private slots:
    //! Retrieves data from the plugin and sends it to all subscribers by name
    /*! Called when plugin data is ready to be sent (by async and signaled sources)
        \param[in]  source  Data Source codename
        \sa sendDataToSubscribers()
    */
    void sendDataToSubscribersByName(QString source);

private:
    //! DataPlugin constructor
    /*! DataPlugin::load() should be used to load and create a DataPlugin instance
        \param[in]  p           Plugin info struct
        \param[in]  destroyfunc Function pointer to the plugin destroy function
        \param[in]  path        Library path
        \param[in]  parent      Qt parent object
        \sa quasar_plugin_info_t, quasar_plugin_destroy()
    */
    DataPlugin(quasar_plugin_info_t* p, plugin_destroy destroyfunc, QString path, QObject* parent = Q_NULLPTR);

    /*! Creates and initializes the timer for a timer-based source (if it does not exist)
        \param[in,out]  data    Reference to the Data Source object
        \sa DataSource.timer
    */
    void createTimer(DataSource& data);

    /*! Crafts the data message to be sent to subscribers
        \param[in]  data    Reference to the Data Source object
        \return The data message
    */
    QString craftDataMessage(const DataSource& data);

    /*! Crafts the custom settings message to be sent to subscribers
        \return The settings message
        \sa updatePluginSettings()
    */
    QString craftSettingsMessage();

    /*! Helper function that propagates custom settings message to all unique subscribers
        \sa updatePluginSettings()
    */
    void propagateSettingsToAllUniqueSubscribers();

    quasar_plugin_info_t* m_plugin;      //!< Plugin info data \sa quasar_plugin_info_t
    plugin_destroy        m_destroyfunc; //!< Plugin destroy function \sa quasar_plugin_destroy()

    std::unique_ptr<quasar_settings_t> m_settings; //!< Plugin settings \sa quasar_settings_t

    QString m_libpath; //!< Path to library file
    QString m_name;    //!< Plugin name
    QString m_code;    //!< Plugin codename
    QString m_desc;    //!< Plugin description
    QString m_author;  //!< Plugin author
    QString m_version; //!< Plugin version string

    DataSourceMapType m_datasources; //!< Map of Data Sources provided by this plugin
};
