#pragma once

#include <QDialog>
#include <QMap>

namespace Ui {
class SettingsDialog;
}

struct MatrixProfile {
    QString colorMatrix1;
    QString colorMatrix2;
    QString forwardMatrix1;
    QString forwardMatrix2;
    QString calibrationMatrix1;
    QString calibrationMatrix2;
    QString illuminant1;
    QString illuminant2;
    QString uniqueCameraModel;
};

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
    void onCameraKeyChanged(int index);

private:
    Ui::SettingsDialog* ui;
    QMap<QString, MatrixProfile> mMatrixProfiles;
    QMap<QString, QString> mCameraNames;
};
