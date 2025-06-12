#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget* parent) :
    QDialog(parent), ui(new Ui::SettingsDialog) {
    ui->setupUi(this);
    connect(ui->browseCacheBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseCache);
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

void SettingsDialog::onBrowseCache() {
    auto folder = QFileDialog::getExistingDirectory(this, tr("Select Cache Folder"));
    if(!folder.isEmpty())
        ui->cacheEdit->setText(folder);
}
