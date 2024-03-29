#pragma once

#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "api/extension_types.h"
#include "common/config.h"
#include "common/settings.h"
#include "common/timer.h"

#include <jsoncons/json.hpp>

class Server;

using SettingsVariantVector = std::vector<Settings::SettingsVariant>;

/*! Holds the mutex and accompanying conditional variable for
    asynchronous or extension signaled Data Sources.
*/
struct DataLock
{
    std::mutex              mutex;              //!< Mutex
    std::condition_variable cv;                 //!< Conditional varaible
    bool                    processed = false;  //!< Bool value to trigger conditional variable notification
};

//! Struct containing cached data for a Data Source
struct DataCache
{
    jsoncons::json data;  //!< Cached data for polled data with a validity duration
                          //!< \sa quasar_data_source_t.rate, quasar_data_source_t.validtime, quasar_polling_type_t
    std::chrono::system_clock::time_point
        expiry;  //!< Expiry time of cached data \sa quasar_data_source_t.rate, quasar_data_source_t.validtime, quasar_polling_type_t
};

//! Struct containing internal resources for a Data Source
struct DataSource
{
    // basic data source fields
    Settings::DataSourceSettings settings;  //!< Contains the enabled flag and refresh rate flag for this Data Source
                                            //!< \sa quasar_data_source_t.rate, quasar_polling_type_t

    std::string topic;      //!< Topic identifier (used by WebSocket server)
    size_t      uid;        //!< Data Source uid
    uint64_t    validtime;  //!< Data validity duration for \ref QUASAR_POLLING_CLIENT. \sa quasar_data_source_t.rate, quasar_data_source_t.validtime,
                            //!< quasar_polling_type_t

    // subscription type source fields
    std::unique_ptr<Timer> timer;        //!< Timer for timer based subscription sources
    int                    subscribers;  //!< Number of subscribers currently subscribed to this source

    // poll type
    std::unordered_set<void*> pollqueue;  //!< Queue of widgets (i.e. its WebSocket instance) waiting for polled data
    DataCache                 cache;      //!< Cached data for polled data with a validity duration

    mutable std::shared_mutex mutex;  //!< Data Source level lock

    std::string               buffer;

    // signaled type source fields
    std::unique_ptr<DataLock> locks;  //!< Mutex/cv for asynchronous or extension signaled sources \sa DataLock
};

class Extension
{
    //! Defines valid return values for getDataFromSource()
    enum DataSourceReturnState : int8_t
    {
        GET_DATA_FAILED  = -1,  //!< get_data failed
        GET_DATA_DELAYED = 0,   //!< get_data delayed
        GET_DATA_SUCCESS = 1    //!< data successfully retrieved
    };

    //! Shorthand for datasources type
    using DataSourceMapType = std::unordered_map<std::string, DataSource>;

public:
    //! Shorthand type for quasar_extension_load()
    using extension_load = std::add_pointer_t<quasar_ext_info_t*(void)>;

    //! Shorthand type for quasar_extension_destroy()
    using extension_destroy                 = std::add_pointer_t<void(quasar_ext_info_t*)>;

    Extension(const Extension&)             = delete;
    Extension& operator= (const Extension&) = delete;

    ~Extension();

    //! Data Source uid counter
    static size_t _uid;

    //! Load an extension
    /*!
        \param[in]  libpath Path to library file
        \return Pointer to a Extension instance if successful, nullptr otherwise
    */
    static Extension* Load(const std::string& libpath, std::shared_ptr<Config> cfg, Server* srv);

    //! Load an internal extension
    /*!
        \param[in]  name        Name of the internal extension
        \param[in]  loadFunc    Load function
        \param[in]  DestroyFunc Destroy function
        \return Pointer to a Extension instance if successful, nullptr otherwise
    */
    static Extension* LoadInternal(std::string_view name, extension_load loadFunc, extension_destroy destroyFunc, std::shared_ptr<Config> cfg, Server* srv);

    /*! Initializes the extension
        Throws an exception if failed.
    */
    void Initialize();

    //! Polls the extension for data to be sent to the requesting client
    /*! Called when the extension receives a widget "poll" request
        \param[in,out]  json        JSON data
        \param[in]      topics      Topics
        \param[in]      args        Any arguments passed to the Data Source, if accepted
        \param[in]      client      Requesting widget's websocket connection instance
        \param[in]      widgetName  Widget name
    */
    void PollDataForSending(jsoncons::json& json, const std::vector<std::string>& topics, const std::string& args, void* client);

    /*! Gets extension identifier
    \return extension identifier
    */
    const std::string& GetName() const { return name; };

    /*! Checks to see whether the Extension is an internal Extension
        \return Extension is an internal Extension
    */
    bool IsInternal() const { return internal; };

    /*! Checks to see whether a Topic exists
        \param[in]  topic   Topic identifier
        \return Topic exists
    */
    bool TopicExists(const std::string& topic) const;

    /*! Checks to see whether a Topic accepts subscribers
        \param[in]  topic   Topic identifier
        \return Topic accepts subscribers
    */
    bool TopicAcceptsSubscribers(const std::string& topic);

    //! Adds a subscriber to a Data Source
    /*!
        \param[in]  subscriber  Subscriber's websocket connection instance
        \param[in]  topic       Topic
        \param[in]  count       Current subscriber count
        \param[in]  widgetName  Widget name
        \return true if successful, false otherwise
    */
    bool AddSubscriber(void* subscriber, const std::string& topic, int count);

    //! Removes a subscriber from a Data Sources
    /*! Invoked when a widget is closed or disconnects
        \param[in]  subscriber  Subscriber's websocket connection instance
        \param[in]  topic       Topic
        \param[in]  count       Current subscriber count
    */
    void                   RemoveSubscriber(void* subscriber, const std::string& topic, int count);

    SettingsVariantVector& GetSettings() { return settings; };

    //! Gets all of this extension's metadata and settings as a JSON object
    /*!
        \param[in,out]  json        JSON data
        \param[in]  settings_only   Retrieve only the settings if true, otherwise retrieves extension's full metadata
    */
    void GetMetadataJSON(jsoncons::json& json, bool settings_only);

    /*! Handles a data ready signal sent by the extension (by async-polled and signaled sources)
        \param[in]  source  Data Source identifier
        \sa quasar_polling_type_t, quasar_signal_data_ready()
    */
    void HandleDataReady(std::string_view source);

    /*! Waits for a set of data to be sent to clients before processing the next set
        \param[in]  source  Data Source identifier
        \sa quasar_signal_wait_processed()
    */
    void WaitForDataProcessed(std::string_view source);

    //! Retrieves non-settings data stored as a part of this extension's config
    /*! \param[in]  label   The stored data's label
        \return The stored data
    */
    template<typename T>
    [[nodiscard]] T ReadStorage(const std::string& label) const
    {
        return config.lock()->ReadGenericStorage<T>(name, label);
    }

    //! Writes non-settings data stored as a part of this extension's config
    /*! \param[in]  label   The stored data's label
        \param[in]  val   The stored data's value
    */
    template<typename T>
    void WriteStorage(const std::string& label, const T& val)
    {
        config.lock()->WriteGenericStorage(name, label, val);
    }

    //! Writes extension settings to config file
    void WriteExtensionSettings();

    //! Writes datasource settings to config file
    void WriteDataSourceSettings();

    /*! Propagates custom setting value changes to the extension
        as well as all unique subscribers after a settings change
        \sa quasar_ext_info_t.update
    */
    void UpdateExtensionSettings();

    //! Returns pointer to Server
    Server* GetServer() { return server; }

private:
    //! Extension constructor
    /*! Extension::load() should be used to load and create a Extension instance
        \param[in]  info        Extension info struct
        \param[in]  destroyfunc Function pointer to the extension destroy function
        \param[in]  path        Library path
        \sa quasar_ext_info_t, quasar_extension_destroy()
    */
    Extension(quasar_ext_info_t* info, extension_destroy destroyfunc, std::string_view path, Server* srv, std::shared_ptr<Config> cfg, bool isInternal = false);

    /*! Retrieves data from a data source and saves it to the supplied JSON object as JSON data
        \param[in]  msg     Reference to the JSON object to save data to
        \param[in]  src     Reference to the Data Source object
        \param[in]  args    Arguments, if any
        \return DataSourceReturnState value determining state of data retrieval
        \sa DataSourceReturnState
    */
    DataSourceReturnState getDataFromSource(jsoncons::json& msg, DataSource& src, std::string args = {});

    //! Retrieves data from the extension and sends it to all subscribers
    /*! Called when extension data is ready to be sent (by both timer and signal)
        \param[in]  src     Data Source
    */
    void sendDataToSubscribers(DataSource& src);

    /*! Creates and initializes the timer for a timer-based source (if it does not exist)
        \param[in,out]  src     Reference to the Data Source object
        \sa DataSource.timer
    */
    void createTimer(DataSource& src);

    /*! Crafts the custom settings message to be sent to subscribers
        \return The settings message
        \sa UpdateExtensionSettings()
    */
    const std::string craftSettingsMessage();

    /*! Helper function that refreshes all data sources
        \sa UpdateExtensionSettings()
    */
    void refreshDataSources();

    // Members
    quasar_ext_info_t*    extensionInfo{};  //!< Extension info data \sa quasar_ext_info_t
    extension_destroy     destroyFunc{};    //!< Extension destroy function \sa quasar_ext_destroy()

    const std::string     libpath{};  //!< Path to library file

    std::string           name{};         //!< Extension identifier
    std::string           fullname{};     //!< Extension full name
    std::string           description{};  //!< Extension description
    std::string           author{};       //!< Extension author
    std::string           version{};      //!< Extension version string
    std::string           url{};          //!< Extension url, if any

    DataSourceMapType     datasources;  //!< Map of Topics in this extension

    bool                  initialized{};  //!< Extension successfully initialized

    const bool            internal{};  //!< Extension is an internal extension

    SettingsVariantVector settings{};  //!< Collection of extension settings

    Server*               server{};
    std::weak_ptr<Config> config{};

    // Metadata keys
    struct
    {
        std::string metadata{};
        std::string settings{};
    } metakeys;
};
