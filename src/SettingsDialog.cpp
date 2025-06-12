#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget* parent) :
    QDialog(parent), ui(new Ui::SettingsDialog) {
    ui->setupUi(this);
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

QMap<QString, QString> SettingsDialog::uniqueNames() const {
    QMap<QString, QString> result;
    for(int r=0; r<ui->uniqueTable->rowCount(); ++r) {
        result.insert(ui->uniqueTable->item(r,0)->text(), ui->uniqueTable->item(r,1)->text());
    }
    return result;
}

void SettingsDialog::setUniqueNames(const QMap<QString, QString>& names) {
    ui->uniqueTable->setColumnCount(2);
    ui->uniqueTable->setRowCount(names.size());
    int row=0;
    for(auto it = names.begin(); it != names.end(); ++it, ++row) {
        ui->uniqueTable->setItem(row,0,new QTableWidgetItem(it.key()));
        ui->uniqueTable->setItem(row,1,new QTableWidgetItem(it.value()));
    }
    QStringList headers;
    headers<<"Camera Key"<<"Unique Camera Model";
    ui->uniqueTable->setHorizontalHeaderLabels(headers);
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
