/*! \file
    \brief Defines the DataExtension class and supporting data structures
*/

#pragma once

#include <qstring_hash_impl.h>

#include <extension_types.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

#include <QObject>
#include <QTimer>

//! Internal setting prefix for Data Source enabled UI toggle
#define QUASAR_DP_ENABLED "/enabled"

//! Internal setting prefix for Data Source refresh rate UI
#define QUASAR_DP_RATE_PREFIX "/rate"

#ifdef PLUGINAPI_LIB
#    define PAPI_EXPORT Q_DECL_EXPORT
#else
#    define PAPI_EXPORT Q_DECL_IMPORT
#endif // PLUGINAPI_LIB

QT_FORWARD_DECLARE_CLASS(QWebSocket)

/*! Holds the mutex and accompanying conditional variable for
    asynchronous or extension signaled Data Sources.
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
    QString                   name;        //!< Data Source codename
    size_t                    uid;         //!< Data Source uid
    int64_t                   refreshmsec; //!< Data Source refresh rate \sa quasar_data_source_t.refreshMsec
    std::unique_ptr<QTimer>   timer;       //!< QTimer for timer based Data Sources
    std::set<QWebSocket*>     subscribers; //!< Set of widgets (its WebSocket connection instance) subscribed to this Data Source
    std::unique_ptr<DataLock> locks;       //!< Mutex/cv for asynchronous or extension signaled Data Sources \sa DataLock
};

//! Shorthand for m_datasources type
using DataSourceMapType = std::unordered_map<QString, DataSource>;

//! Class that encapsulates a Quasar extension library (dll/so)
class PAPI_EXPORT DataExtension : public QObject
{
    Q_OBJECT;

public:
    //! Shorthand type for quasar_extension_load()
    using extension_load = std::add_pointer_t<quasar_ext_info_t*(void)>;

    //! Shorthand type for quasar_extension_destroy()
    using extension_destroy = std::add_pointer_t<void(quasar_ext_info_t*)>;

    //! DataExtension destructor
    ~DataExtension();

    //! Data Source uid counter
    static uintmax_t _uid;

    //! Load an extension
    /*!
        \param[in]  libpath Path to library file
        \param[in]  parent  Parent element in Qt object tree
        \return Pointer to a DataExtension instance if successful, nullptr otherwise
    */
    static DataExtension* load(QString libpath, QObject* parent = nullptr);

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

    //! Polls the extension for data and sends it to the requesting widget
    /*! Called when the extension receives a widget "poll" request
        \param[in]  source      Data Source codename
        \param[in]  subscriber  Requesting widget's websocket connection instance
        \param[in]  widgetName  Widget name
    */
    void pollAndSendData(QString source, QWebSocket* subscriber, QString widgetName);

    //! Retrieves data from the extension and sends it to all subscribers
    /*! Called when extension data is ready to be sent (by both timer and signal)
        \param[in]  source  Data Source
    */
    void sendDataToSubscribers(DataSource& source);

    /*! Gets path to library file
        \return path to library file
    */
    QString getLibPath() { return m_libpath; };

    /*! Gets extension identifier
        \return extension identifier
    */
    QString getName() { return m_name; };

    /*! Gets extension full name
        \return extension full name
    */
    QString getFullName() { return m_fullname; };

    /*! Gets extension description
        \return extension description
    */
    QString getDesc() { return m_desc; };

    /*! Gets extension author
        \return extension author
    */
    QString getAuthor() { return m_author; };

    /*! Gets extension version
        \return extension version
    */
    QString getVersion() { return m_version; };

    /*! Gets extension url
        \return extension url
    */
    QString getUrl() { return m_url; };

    /*! Gets extension settings prefix for use with internal Quasar settings store
        \return extension settings prefix
    */
    QString getSettingsKey(QString name) { return getName() + "/" + name; };

    //! Gets all of this extension's metadata and settings as a JSON object
    /*!
        \param[in]  settings_only   Retrieve only the settings if true, otherwise retrieves extension's full metadata
        \return JSON object with the extension's metadata
    */
    QJsonObject getMetadataJSON(bool settings_only);

    /*! Gets extension custom settings
        \sa quasar_settings_t
        \return pointer to extension custom settings
    */
    quasar_settings_t* getSettings() { return m_settings.get(); };

    /*! Gets extension Data Sources
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

    /*! Sets all settings returned by the settings dialog
        This function is only called by the relevant GUI can should not be used otherwise.

        \param[in]  settings    JSON object containing extension settings being changed

        \sa quasar_settings_t, DataSource.refreshmsec, DataSource.timer
    */
    void setAllSettings(const QJsonObject& setjs);

    /*! Propagates custom setting value changes to the extension
        as well as all unique subscribers

        \sa quasar_ext_info_t.update
    */
    void updateExtensionSettings();

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
    //! Retrieves data from the extension and sends it to all subscribers by name
    /*! Called when extension data is ready to be sent (by async and signaled sources)
        \param[in]  source  Data Source codename
        \sa sendDataToSubscribers()
    */
    void sendDataToSubscribersByName(QString source);

private:
    //! DataExtension constructor
    /*! DataExtension::load() should be used to load and create a DataExtension instance
        \param[in]  p           extension info struct
        \param[in]  destroyfunc Function pointer to the extension destroy function
        \param[in]  path        Library path
        \param[in]  parent      Qt parent object
        \sa quasar_ext_info_t, quasar_extension_destroy()
    */
    DataExtension(quasar_ext_info_t* p, extension_destroy destroyfunc, QString path, QObject* parent = nullptr);

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
        \sa updateExtensionSettings()
    */
    QString craftSettingsMessage();

    /*! Helper function that propagates custom settings message to all unique subscribers
        \sa updateExtensionSettings()
    */
    void propagateSettingsToAllUniqueSubscribers();

    quasar_ext_info_t* m_extension;   //!< Extension info data \sa quasar_ext_info_t
    extension_destroy  m_destroyfunc; //!< Extension destroy function \sa quasar_ext_destroy()

    std::unique_ptr<quasar_settings_t> m_settings; //!< Extension settings \sa quasar_settings_t

    QString m_libpath;  //!< Path to library file
    QString m_name;     //!< Extension identifiter
    QString m_fullname; //!< Extension full name
    QString m_desc;     //!< Extension description
    QString m_author;   //!< Extension author
    QString m_version;  //!< Extension version string
    QString m_url;      //!< Extension url, if any

    DataSourceMapType m_datasources; //!< Map of Data Sources provided by this extension
};
