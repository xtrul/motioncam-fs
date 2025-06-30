#pragma once
#include <QDialog>
namespace Ui { class AdvancedOptionsDialog; }

class AdvancedOptionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit AdvancedOptionsDialog(QWidget* parent = nullptr);
    ~AdvancedOptionsDialog();

    void setUniqueCameraModel(const QString& model);
    QString uniqueCameraModel() const;

private slots:

private:
    Ui::AdvancedOptionsDialog* ui;
};

