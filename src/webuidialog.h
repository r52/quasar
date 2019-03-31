#pragma once

#include "dataserver.h"

#include <QWidget>

class WebUiDialog : public QWidget
{
public:
    WebUiDialog(DataServer* server, QString title, QUrl url, ClientAccessLevel lvl = CAL_SETTINGS, QSize size = {1280, 720}, QWidget* parent = nullptr);

private:
    Q_DISABLE_COPY(WebUiDialog);
};
