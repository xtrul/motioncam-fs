#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class OptionsDialog; }
QT_END_NAMESPACE

class OptionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit OptionsDialog(QWidget* parent = nullptr);
    ~OptionsDialog();

    void setCacheFolder(const QString& path);
    QString cacheFolder() const;

    void setScaleRaw(bool enabled);
    bool scaleRaw() const;

    void setCameraModel(const QString& model);
    QString cameraModel() const;

    void setCalibrationProfile(const QString& profile);
    QString calibrationProfile() const;

private:
    Ui::OptionsDialog* ui;
};

