#include "optionsdialog.h"
#include "ui_optionsdialog.h"

OptionsDialog::OptionsDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::OptionsDialog)
{
    ui->setupUi(this);
}

OptionsDialog::~OptionsDialog() {
    delete ui;
}

void OptionsDialog::setCacheFolder(const QString& path) {
    ui->cacheFolderLabel->setText(path);
}

QString OptionsDialog::cacheFolder() const {
    return ui->cacheFolderLabel->text();
}

void OptionsDialog::setScaleRaw(bool enabled) {
    ui->scaleRawCheck->setChecked(enabled);
}

bool OptionsDialog::scaleRaw() const {
    return ui->scaleRawCheck->isChecked();
}

void OptionsDialog::setCameraModel(const QString& model) {
    int idx = ui->cameraModelCombo->findText(model);
    if(idx >= 0)
        ui->cameraModelCombo->setCurrentIndex(idx);
}

QString OptionsDialog::cameraModel() const {
    return ui->cameraModelCombo->currentText();
}

void OptionsDialog::setCalibrationProfile(const QString& profile) {
    int idx = ui->profileCombo->findText(profile);
    if(idx >= 0)
        ui->profileCombo->setCurrentIndex(idx);
}

QString OptionsDialog::calibrationProfile() const {
    return ui->profileCombo->currentText();
}

