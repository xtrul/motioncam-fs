#include "AdvancedOptionsDialog.h"
#include "ui_advancedoptionsdialog.h"
#include <QDialogButtonBox>
#include <QFileDialog>
#include "CalibrationProfile.h"

AdvancedOptionsDialog::AdvancedOptionsDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::AdvancedOptionsDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    connect(ui->browseButton, &QPushButton::clicked, this, &AdvancedOptionsDialog::onBrowse);
}

AdvancedOptionsDialog::~AdvancedOptionsDialog()
{
    delete ui;
}

void AdvancedOptionsDialog::setUniqueCameraModel(const QString& model)
{
    ui->uniqueCameraModelEdit->setText(model);
}

QString AdvancedOptionsDialog::uniqueCameraModel() const
{
    return ui->uniqueCameraModelEdit->text();
}

void AdvancedOptionsDialog::setCalibrationFile(const QString& file)
{
    mCalibrationFile = file;
    ui->calibrationFileEdit->setText(file);
}

QString AdvancedOptionsDialog::calibrationFile() const
{
    return ui->calibrationFileEdit->text();
}

QString AdvancedOptionsDialog::selectedProfile() const
{
    return ui->profileCombo->currentText();
}

void AdvancedOptionsDialog::setProfiles(const std::map<std::string, motioncam::CalibrationProfile>& profiles)
{
    mProfiles = profiles;
    ui->profileCombo->clear();
    for(const auto& kv : profiles)
        ui->profileCombo->addItem(QString::fromStdString(kv.first));
}

void AdvancedOptionsDialog::onBrowse()
{
    auto path = QFileDialog::getOpenFileName(this, tr("Open Calibration"), QString(), tr("JSON Files (*.json)"));
    if(path.isEmpty())
        return;
    setCalibrationFile(path);
    mProfiles = motioncam::loadCalibrationProfiles(path.toStdString());
    setProfiles(mProfiles);
}
