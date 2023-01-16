#include "extensionpage.h"
#include "ui_extensionpage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

#include <spdlog/spdlog.h>

ExtensionPage::ExtensionPage(const std::string& extname,
    const Settings::ExtensionInfo&              info,
    std::vector<Settings::DataSourceSettings*>& src,
    std::vector<Settings::SettingsVariant>*     settings,
    QWidget*                                    parent) :
    QWidget(parent),
    name{extname},
    ui(new Ui::ExtensionPage)
{
    ui->setupUi(this);

    // Info
    ui->nameLabel->setText(QString::fromStdString(info.fullname));
    ui->versionLabel->setText(QString::fromStdString(info.version));
    ui->authorLabel->setText(QString::fromStdString(info.author));
    ui->descLabel->setText(QString::fromStdString(info.description));
    ui->urlLabel->setText(QString::fromStdString(info.url));

    // Sources
    int row = ui->sourcesLayout->rowCount();
    for (auto data : src)
    {
        savedSrc.insert({data->name, *data});

        auto& savedDat    = savedSrc[data->name];

        auto  enableCheck = new QCheckBox(this);
        auto  name        = QString::fromStdString(data->name);
        enableCheck->setObjectName(name + "/enabled");
        enableCheck->setText(name);
        enableCheck->setChecked(data->enabled);

        ui->sourcesLayout->setWidget(row, QFormLayout::LabelRole, enableCheck);

        connect(enableCheck, &QCheckBox::toggled, [&](bool state) {
            savedDat.enabled = state;
        });

        if (data->rate > 0)
        {
            QSpinBox* upSpin = new QSpinBox(this);
            upSpin->setObjectName(name + "/rate");
            upSpin->setMinimum(1);
            upSpin->setMaximum(INT_MAX);
            upSpin->setSingleStep(1);
            upSpin->setValue(data->rate);
            upSpin->setSuffix("ms");
            upSpin->setEnabled(data->enabled);

            connect(enableCheck, &QCheckBox::toggled, [=](bool state) {
                upSpin->setEnabled(state);
            });

            connect(upSpin, &QSpinBox::valueChanged, [&](int value) {
                savedDat.rate = value;
            });

            ui->sourcesLayout->setWidget(row, QFormLayout::FieldRole, upSpin);
        }

        row++;
    }

    // Settings
    row = ui->settingsLayout->rowCount();
    for (auto& def : *settings)
    {
        std::visit(
            [&](auto&& arg) {
                using T   = std::decay_t<decltype(arg)>;

                auto name = arg.GetLabel();

                savedSettings.insert({name, arg.GetValue()});

                auto argLabel = new QLabel(this);
                argLabel->setText(QString::fromStdString(arg.GetDescription()));

                ui->settingsLayout->setWidget(row, QFormLayout::LabelRole, argLabel);

                if constexpr (std::is_same_v<T, Settings::Setting<int>>)
                {
                    auto [min, max, step] = arg.GetMinMaxStep();

                    QSpinBox* upSpin      = new QSpinBox(this);
                    upSpin->setObjectName(QString::fromStdString(name));
                    upSpin->setMinimum(min);
                    upSpin->setMaximum(max);
                    upSpin->setSingleStep(step);
                    upSpin->setValue(arg.GetValue());

                    ui->settingsLayout->setWidget(row, QFormLayout::FieldRole, upSpin);

                    connect(upSpin, &QSpinBox::valueChanged, [&, name](int value) {
                        savedSettings[name] = value;
                    });
                }
                else if constexpr (std::is_same_v<T, Settings::Setting<double>>)
                {
                    auto [min, max, step]  = arg.GetMinMaxStep();

                    QDoubleSpinBox* upSpin = new QDoubleSpinBox(this);
                    upSpin->setObjectName(QString::fromStdString(name));
                    upSpin->setMinimum(min);
                    upSpin->setMaximum(max);
                    upSpin->setSingleStep(step);
                    upSpin->setValue(arg.GetValue());

                    ui->settingsLayout->setWidget(row, QFormLayout::FieldRole, upSpin);

                    connect(upSpin, &QDoubleSpinBox::valueChanged, [&, name](double value) {
                        savedSettings[name] = value;
                    });
                }
                else if constexpr (std::is_same_v<T, Settings::Setting<bool>>)
                {
                    auto enableCheck = new QCheckBox(this);
                    enableCheck->setObjectName(QString::fromStdString(name));
                    enableCheck->setChecked(arg.GetValue());

                    ui->sourcesLayout->setWidget(row, QFormLayout::FieldRole, enableCheck);

                    connect(enableCheck, &QCheckBox::toggled, [&, name](bool state) {
                        savedSettings[name] = state;
                    });
                }
                else if constexpr (std::is_same_v<T, Settings::Setting<std::string>>)
                {
                    QLineEdit* edit = new QLineEdit(this);
                    edit->setObjectName(QString::fromStdString(name));
                    edit->setText(QString::fromStdString(arg.GetValue()));
                    if (arg.GetIsPassword())
                    {
                        edit->setEchoMode(QLineEdit::Password);
                    }

                    ui->settingsLayout->setWidget(row, QFormLayout::FieldRole, edit);

                    connect(edit, &QLineEdit::textEdited, [&, name](const QString& text) {
                        savedSettings[name] = text.toStdString();
                    });
                }
                else if constexpr (std::is_same_v<T, Settings::SelectionSetting<std::string>>)
                {
                    auto& c     = arg.GetOptions();

                    auto  combo = new QComboBox(this);
                    combo->setObjectName(QString::fromStdString(name));

                    for (auto& [value, name] : c)
                    {
                        combo->addItem(QString::fromStdString(name), QString::fromStdString(value));
                    }

                    combo->setCurrentIndex(combo->findData(QString::fromStdString(arg.GetValue())));

                    ui->settingsLayout->setWidget(row, QFormLayout::FieldRole, combo);

                    connect(combo, &QComboBox::currentIndexChanged, [&, name](int index) {
                        savedSettings[name] = combo->itemData(index).toString().toStdString();
                    });
                }
                else
                {
                    SPDLOG_WARN("Non-exhaustive visitor!");
                }
            },
            def);

        row++;
    }
}

ExtensionPage::~ExtensionPage()
{
    delete ui;
}
