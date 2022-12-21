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
#include "extension_exports.h"

#include <jsoncons/json.hpp>

class Config;
class Server;

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
    bool        enabled;    //!< Whether this data source is enabled
    std::string name;       //!< Data Source identifier
    size_t      uid;        //!< Data Source uid
    int64_t     rate;       //!< Data Source refresh rate \sa quasar_data_source_t.rate, quasar_polling_type_t
    uint64_t    validtime;  //!< Data validity duration for \ref QUASAR_POLLING_CLIENT. \sa quasar_data_source_t.rate, quasar_data_source_t.validtime,
                            //!< quasar_polling_type_t

    // subscription type source fields
    // std::unique_ptr<QTimer> timer;        //!< QTimer for timer based subscription sources
    std::set<void*> subscribers;  //!< Set of widgets (i.e. its WebSocket instance) subscribed to this source

    // poll type
    std::unordered_set<void*> pollqueue;  //!< Queue of widgets (i.e. its WebSocket instance) waiting for polled data
    DataCache                 cache;      //!< Cached data for polled data with a validity duration

    mutable std::shared_mutex mutex;  //!< Data Source level lock

    // signaled type source fields
    std::unique_ptr<DataLock> locks;  //!< Mutex/cv for asynchronous or extension signaled sources \sa DataLock
};

class extension_API Extension
{
    //! Defines valid return values for getDataFromSource()
    enum DataSourceReturnState : int8_t
    {
        GET_DATA_FAILED  = -1,  //!< get_data failed
        GET_DATA_DELAYED = 0,   //!< get_data delayed
        GET_DATA_SUCCESS = 1    //!< data successfully retrieved
    };

    //! Shorthand type for quasar_extension_load()
    using extension_load = std::add_pointer_t<quasar_ext_info_t*(void)>;

    //! Shorthand type for quasar_extension_destroy()
    using extension_destroy = std::add_pointer_t<void(quasar_ext_info_t*)>;

    //! Shorthand for datasources type
    using DataSourceMapType = std::unordered_map<std::string, DataSource>;

public:
    Extension(Extension const&)             = delete;
    Extension& operator= (Extension const&) = delete;

    ~Extension();

    //! Data Source uid counter
    static uintmax_t _uid;

    //! Load an extension
    /*!
        \param[in]  libpath Path to library file
        \return Pointer to a Extension instance if successful, nullptr otherwise
    */
    static Extension* Load(const std::string& libpath, std::shared_ptr<Config> cfg, std::shared_ptr<Server> srv);

    /*! Initializes the extension
        Throws an exception if failed.
    */
    void Initialize();

    //! Polls the extension for data to be sent to the requesting client
    /*! Called when the extension receives a widget "poll" request
        \param[in]  sources     Data Source identifiers
        \param[in]  args        Any arguments passed to the Data Source, if accepted
        \param[in]  client      Requesting widget's websocket connection instance
        \param[in]  widgetName  Widget name
    */
    std::string PollDataForSending(const std::vector<std::string>& sources, const std::string& args, void* client);

    /*! Gets extension identifier
    \return extension identifier
    */
    const std::string& GetName() const { return name; };

private:
    //! Extension constructor
    /*! Extension::load() should be used to load and create a Extension instance
        \param[in]  p           extension info struct
        \param[in]  destroyfunc Function pointer to the extension destroy function
        \param[in]  path        Library path
        \sa quasar_ext_info_t, quasar_extension_destroy()
    */
    Extension(quasar_ext_info_t* p, extension_destroy destroyfunc, const std::string& path, std::shared_ptr<Server> srv, std::shared_ptr<Config> cfg);

    /*! Retrieves data from a data source and saves it to the supplied JSON object as JSON data
        \param[in]  msg     Reference to the JSON object to save data to
        \param[in]  src     Reference to the Data Source object
        \param[in]  args    Arguments, if any
        \return DataSourceReturnState value determining state of data retrieval
        \sa DataSourceReturnState
    */
    DataSourceReturnState getDataFromSource(jsoncons::json& msg, DataSource& src, std::string args = {});

    // Members
    quasar_ext_info_t*    extensionInfo;  //!< Extension info data \sa quasar_ext_info_t
    extension_destroy     destroyFunc;    //!< Extension destroy function \sa quasar_ext_destroy()

    std::string           libpath;      //!< Path to library file
    std::string           name;         //!< Extension identifier
    std::string           fullname;     //!< Extension full name
    std::string           description;  //!< Extension description
    std::string           author;       //!< Extension author
    std::string           version;      //!< Extension version string
    std::string           url;          //!< Extension url, if any

    DataSourceMapType     datasources;  //!< Map of Data Sources provided by this extension

    bool                  initialized;  //!< Extension successfully initialized;

    std::weak_ptr<Server> server{};
    std::weak_ptr<Config> config{};
};
