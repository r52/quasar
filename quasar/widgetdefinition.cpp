#include "widgetdefinition.h"
#include "widget_json.h"

WidgetDefinition parse_definition(std::string_view json_document)
{
    return daw::json::from_json<WidgetDefinition>(json_document, daw::json::options::parse_flags<daw::json::options::PolicyCommentTypes::cpp>);
}
