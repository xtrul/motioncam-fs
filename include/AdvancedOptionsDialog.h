#pragma once
#include <QDialog>
#include "CalibrationProfile.h"
#include <map>

namespace Ui { class AdvancedOptionsDialog; }

class AdvancedOptionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit AdvancedOptionsDialog(QWidget* parent = nullptr);
    ~AdvancedOptionsDialog();

    void setUniqueCameraModel(const QString& model);
    QString uniqueCameraModel() const;

    void setCalibrationFile(const QString& path);
    QString calibrationFile() const;

    void setCalibrationProfiles(const std::map<std::string, motioncam::CalibrationProfile>& profiles);
    const std::map<std::string, motioncam::CalibrationProfile>& calibrationProfiles() const;

    void setSelectedProfile(const QString& profile);
    QString selectedProfile() const;

private slots:
    void onBrowseModel();
    void onBrowseCalibration();
    void onProfileChanged(int index);

private:
    void loadModelFile(const QString& path);
    void loadCalibrationFile(const QString& path);
    void updateProfileDetails(const motioncam::CalibrationProfile& profile);

private:
    Ui::AdvancedOptionsDialog* ui;
    std::map<std::string, motioncam::CalibrationProfile> mProfiles;
    QString mCalibrationPath;
};

