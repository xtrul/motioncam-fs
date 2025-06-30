#pragma once
#include <QDialog>
#include <map>
#include "CalibrationProfile.h"
namespace Ui { class AdvancedOptionsDialog; }

class AdvancedOptionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit AdvancedOptionsDialog(QWidget* parent = nullptr);
    ~AdvancedOptionsDialog();

    void setUniqueCameraModel(const QString& model);
    QString uniqueCameraModel() const;
    void setCalibrationFile(const QString& file);
    QString calibrationFile() const;
    QString selectedProfile() const;
    void setProfiles(const std::map<std::string, motioncam::CalibrationProfile>& profiles);

private slots:
    void onBrowse();

private:
    Ui::AdvancedOptionsDialog* ui;
    QString mCalibrationFile;
    std::map<std::string, motioncam::CalibrationProfile> mProfiles;
};

