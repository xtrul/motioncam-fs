#pragma once

#include <QDialog>
#include <QMap>
#include "MatrixProfile.h"

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
    QMap<QString, QString> cameraNames() const;
    void setCameraNames(const QMap<QString, QString>& names);
    QString currentCameraKey() const;
    void setCurrentCameraKey(const QString& key);
    QMap<QString, MatrixProfile> matrixProfiles() const;
    void setMatrixProfiles(const QMap<QString, MatrixProfile>& profiles);
    QString currentMatrixKey() const;
    void setCurrentMatrixKey(const QString& key);

private slots:
    void onBrowseCache();
    void onMatrixSetChanged(int index);


private:
    Ui::SettingsDialog* ui;
    QMap<QString, MatrixProfile> mMatrixProfiles;
    QMap<QString, QString> mCameraNames;
};
