#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_quasar.h"

class Quasar : public QMainWindow
{
    Q_OBJECT

public:
    Quasar(QWidget *parent = Q_NULLPTR);

private:
    Ui::QuasarClass ui;
};
