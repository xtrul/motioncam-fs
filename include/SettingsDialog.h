#pragma once

#include <QDialog>
#include <QMap>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();

    QString cachePath() const;
    void setCachePath(const QString& path);
    QMap<QString, QString> uniqueNames() const;
    void setUniqueNames(const QMap<QString, QString>& names);

private slots:
    void onBrowseCache();

private:
    Ui::SettingsDialog* ui;
};
