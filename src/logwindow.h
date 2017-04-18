#pragma once

#include <QObject>

QT_FORWARD_DECLARE_CLASS(QTextEdit)

class LogWindow : public QObject
{
    Q_OBJECT

    friend class Quasar;

public:
    ~LogWindow();

private:
    LogWindow(QObject* parent = nullptr);

    QTextEdit* release();

    bool m_released = false;
};
