#pragma once

#include <QtWebEngineCore/QWebEngineUrlSchemeHandler>

class WebUiHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT
public:
    explicit WebUiHandler(QObject* parent = nullptr);

    void requestStarted(QWebEngineUrlRequestJob* job) override;

    static void registerUrlScheme();

    const static QByteArray schemeName;
    const static QUrl       settingsUrl;
};
