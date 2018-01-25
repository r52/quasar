#include "dataservices.h"

#include "applauncher.h"
#include "dataserver.h"
#include "widgetregistry.h"

/*
* Another quasi singleton to prevent this class from being created more than once
* without using the crappy singleton pattern
*/

namespace
{
    DataServices* s_service = nullptr;
}

DataServices::DataServices(QObject* parent)
    : QObject(parent), server(new DataServer(this)), reg(new WidgetRegistry(server, this)), launcher(new AppLauncher(server, reg, this))
{
    if (nullptr != s_service)
    {
        throw std::runtime_error("Another instance already created");
    }
    s_service = this;
}
