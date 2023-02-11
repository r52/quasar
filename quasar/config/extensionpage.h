#ifndef EXTENSIONPAGE_H
#define EXTENSIONPAGE_H

#include "common/settings.h"

#include <QWidget>

namespace Ui
{
    class ExtensionPage;
}

class ExtensionPage : public QWidget
{
    Q_OBJECT

public:
    explicit ExtensionPage(const std::string& extname, const Settings::ExtensionInfo& info, QWidget* parent = nullptr);
    ~ExtensionPage();

    const std::string                                                   name{};
    std::map<std::string, Settings::DataSourceSettings>                 savedSrc;
    std::map<std::string, std::variant<int, double, bool, std::string>> savedSettings;

private:
    Ui::ExtensionPage* ui;
};

#endif  // EXTENSIONPAGE_H
