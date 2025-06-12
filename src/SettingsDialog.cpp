#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

#include <QFileDialog>
#include <QTableWidget>

SettingsDialog::SettingsDialog(QWidget* parent) :
    QDialog(parent), ui(new Ui::SettingsDialog) {
    ui->setupUi(this);
    ui->cameraTable->setColumnCount(2);
    ui->cameraTable->setHorizontalHeaderLabels({tr("Camera Key"), tr("Unique Camera Model")});
    ui->cameraTable->horizontalHeader()->setStretchLastSection(true);
    connect(ui->browseCacheBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseCache);
    connect(ui->saveBtn, &QPushButton::clicked, this, &SettingsDialog::accept);
    connect(ui->cancelBtn, &QPushButton::clicked, this, &SettingsDialog::reject);
    connect(ui->matrixCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onMatrixSetChanged);
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

QString SettingsDialog::cachePath() const {
    return ui->cacheEdit->text();
}

void SettingsDialog::setCachePath(const QString& path) {
    ui->cacheEdit->setText(path);
}

QMap<QString, QString> SettingsDialog::cameraNames() const {
    QMap<QString, QString> names;
    for(int row = 0; row < ui->cameraTable->rowCount(); ++row) {
        auto keyItem = ui->cameraTable->item(row, 0);
        auto valItem = ui->cameraTable->item(row, 1);
        if(keyItem)
            names.insert(keyItem->text(), valItem ? valItem->text() : QString());
    }
    return names;
}

void SettingsDialog::setCameraNames(const QMap<QString, QString>& names) {
    mCameraNames = names;
    ui->cameraTable->setRowCount(names.size());
    int row = 0;
    for(auto it = names.begin(); it != names.end(); ++it, ++row) {
        auto keyItem = new QTableWidgetItem(it.key());
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        ui->cameraTable->setItem(row, 0, keyItem);
        ui->cameraTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    }
    if(ui->cameraTable->rowCount() > 0)
        ui->cameraTable->selectRow(0);
}

QString SettingsDialog::currentCameraKey() const {
    int row = ui->cameraTable->currentRow();
    if(row < 0)
        row = 0;
    if(row >= ui->cameraTable->rowCount())
        return {};
    auto item = ui->cameraTable->item(row, 0);
    return item ? item->text() : QString();
}

void SettingsDialog::setCurrentCameraKey(const QString& key) {
    for(int row = 0; row < ui->cameraTable->rowCount(); ++row) {
        if(auto item = ui->cameraTable->item(row,0); item && item->text() == key) {
            ui->cameraTable->selectRow(row);
            return;
        }
    }
    if(ui->cameraTable->rowCount() > 0)
        ui->cameraTable->selectRow(0);
}

QMap<QString, MatrixProfile> SettingsDialog::matrixProfiles() const {
    auto profiles = mMatrixProfiles;
    auto key = ui->matrixCombo->currentText();
    if(profiles.contains(key)) {
        auto& p = profiles[key];
        p.colorMatrix1 = ui->colorMatrix1Edit->text();
        p.colorMatrix2 = ui->colorMatrix2Edit->text();
        p.forwardMatrix1 = ui->forwardMatrix1Edit->text();
        p.forwardMatrix2 = ui->forwardMatrix2Edit->text();
        p.calibrationMatrix1 = ui->calibrationMatrix1Edit->text();
        p.calibrationMatrix2 = ui->calibrationMatrix2Edit->text();
        p.illuminant1 = ui->illuminant1Edit->text();
        p.illuminant2 = ui->illuminant2Edit->text();
        p.uniqueCameraModel = mMatrixProfiles[key].uniqueCameraModel;
    }
    return profiles;
}

void SettingsDialog::setMatrixProfiles(const QMap<QString, MatrixProfile>& profiles) {
    mMatrixProfiles = profiles;
    ui->matrixCombo->clear();
    for(auto it = profiles.begin(); it != profiles.end(); ++it)
        ui->matrixCombo->addItem(it.key());
    if(!profiles.isEmpty())
        onMatrixSetChanged(0);
}

QString SettingsDialog::currentMatrixKey() const {
    return ui->matrixCombo->currentText();
}

void SettingsDialog::setCurrentMatrixKey(const QString& key) {
    int index = ui->matrixCombo->findText(key);
    if(index >= 0)
        ui->matrixCombo->setCurrentIndex(index);
}

void SettingsDialog::onMatrixSetChanged(int index) {
    Q_UNUSED(index);
    auto key = ui->matrixCombo->currentText();
    if(!mMatrixProfiles.contains(key))
        return;
    const auto& p = mMatrixProfiles[key];
    ui->colorMatrix1Edit->setText(p.colorMatrix1);
    ui->colorMatrix2Edit->setText(p.colorMatrix2);
    ui->forwardMatrix1Edit->setText(p.forwardMatrix1);
    ui->forwardMatrix2Edit->setText(p.forwardMatrix2);
    ui->calibrationMatrix1Edit->setText(p.calibrationMatrix1);
    ui->calibrationMatrix2Edit->setText(p.calibrationMatrix2);
    ui->illuminant1Edit->setText(p.illuminant1);
    ui->illuminant2Edit->setText(p.illuminant2);
}


void SettingsDialog::onBrowseCache() {
    auto folder = QFileDialog::getExistingDirectory(this, tr("Select Cache Folder"));
    if(!folder.isEmpty())
        ui->cacheEdit->setText(folder);
}
