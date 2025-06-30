#include "AdvancedOptionsDialog.h"
#include "ui_advancedoptionsdialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStringList>
#include <nlohmann/json.hpp>
#include <fstream>

AdvancedOptionsDialog::AdvancedOptionsDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::AdvancedOptionsDialog)
{
    ui->setupUi(this);

    connect(ui->browseModelBtn, &QPushButton::clicked, this, &AdvancedOptionsDialog::onBrowseModel);
    connect(ui->browseCalibrationBtn, &QPushButton::clicked, this, &AdvancedOptionsDialog::onBrowseCalibration);
    connect(ui->profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdvancedOptionsDialog::onProfileChanged);
}

AdvancedOptionsDialog::~AdvancedOptionsDialog() { delete ui; }

void AdvancedOptionsDialog::setUniqueCameraModel(const QString& model) {
    ui->uniqueCameraModelEdit->setText(model);
}

QString AdvancedOptionsDialog::uniqueCameraModel() const { return ui->uniqueCameraModelEdit->text(); }

void AdvancedOptionsDialog::setCalibrationFile(const QString& path) {
    mCalibrationPath = path;
    ui->calibrationFileEdit->setText(path);
}

QString AdvancedOptionsDialog::calibrationFile() const { return mCalibrationPath; }

void AdvancedOptionsDialog::setCalibrationProfiles(const std::map<std::string, motioncam::CalibrationProfile>& profiles) {
    mProfiles = profiles;
    ui->profileCombo->clear();
    for(const auto& kv : mProfiles)
        ui->profileCombo->addItem(QString::fromStdString(kv.first));
}

const std::map<std::string, motioncam::CalibrationProfile>& AdvancedOptionsDialog::calibrationProfiles() const {
    return mProfiles;
}

void AdvancedOptionsDialog::setSelectedProfile(const QString& profile) {
    int idx = ui->profileCombo->findText(profile);
    if(idx >= 0)
        ui->profileCombo->setCurrentIndex(idx);
}

QString AdvancedOptionsDialog::selectedProfile() const { return ui->profileCombo->currentText(); }

void AdvancedOptionsDialog::onBrowseModel() {
    auto file = QFileDialog::getOpenFileName(this, tr("Load uniqueCameraModel.json"), QString(), tr("JSON Files (*.json)"));
    if(file.isEmpty())
        return;
    loadModelFile(file);
}

void AdvancedOptionsDialog::loadModelFile(const QString& path) {
    std::ifstream f(path.toStdString());
    if(!f.is_open()) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to open file"));
        return;
    }
    try {
        nlohmann::json j; f >> j;
        auto model = QString::fromStdString(j.value("uniqueCameraModel", ""));
        ui->uniqueCameraModelEdit->setText(model);
    } catch(std::exception& e) {
        QMessageBox::warning(this, tr("Error"), e.what());
    }
}

void AdvancedOptionsDialog::onBrowseCalibration() {
    auto file = QFileDialog::getOpenFileName(this, tr("Load Calibration"), QString(), tr("JSON Files (*.json)"));
    if(file.isEmpty())
        return;
    loadCalibrationFile(file);
}

void AdvancedOptionsDialog::loadCalibrationFile(const QString& path) {
    try {
        mProfiles = motioncam::loadCalibrationProfiles(path.toStdString());
        mCalibrationPath = path;
        ui->calibrationFileEdit->setText(path);
        ui->profileCombo->clear();
        for(const auto& kv : mProfiles)
            ui->profileCombo->addItem(QString::fromStdString(kv.first));
        if(!mProfiles.empty())
            ui->profileCombo->setCurrentIndex(0);
    } catch(std::exception& e) {
        QMessageBox::warning(this, tr("Error"), e.what());
    }
}

void AdvancedOptionsDialog::onProfileChanged(int index) {
    if(index < 0) return;
    auto it = mProfiles.find(ui->profileCombo->currentText().toStdString());
    if(it != mProfiles.end())
        updateProfileDetails(it->second);
}

static QString arrayToString(const std::array<float,9>& arr) {
    QStringList list;
    for(float v : arr) list << QString::number(v);
    return list.join(", ");
}

void AdvancedOptionsDialog::updateProfileDetails(const motioncam::CalibrationProfile& p) {
    QString details;
    if(!p.uniqueCameraModel.empty())
        details += QStringLiteral("uniqueCameraModel: %1\n").arg(QString::fromStdString(p.uniqueCameraModel));
    details += QStringLiteral("colorMatrix1: %1\n").arg(arrayToString(p.colorMatrix1));
    details += QStringLiteral("colorMatrix2: %1\n").arg(arrayToString(p.colorMatrix2));
    details += QStringLiteral("forwardMatrix1: %1\n").arg(arrayToString(p.forwardMatrix1));
    details += QStringLiteral("forwardMatrix2: %1\n").arg(arrayToString(p.forwardMatrix2));
    details += QStringLiteral("calibrationMatrix1: %1\n").arg(arrayToString(p.calibrationMatrix1));
    details += QStringLiteral("calibrationMatrix2: %1\n").arg(arrayToString(p.calibrationMatrix2));
    details += QStringLiteral("colorIlluminant1: %1\n").arg(QString::fromStdString(p.colorIlluminant1));
    details += QStringLiteral("colorIlluminant2: %1\n").arg(QString::fromStdString(p.colorIlluminant2));
    if(p.baselineExposure != 0.f)
        details += QStringLiteral("baselineExposure: %1\n").arg(p.baselineExposure);
    ui->profileDetails->setPlainText(details);
}

