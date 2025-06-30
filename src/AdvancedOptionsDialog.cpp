#include "AdvancedOptionsDialog.h"
#include "ui_advancedoptionsdialog.h"
#include <QDialogButtonBox>

AdvancedOptionsDialog::AdvancedOptionsDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::AdvancedOptionsDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
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
