#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "IFuseFileSystem.h"

#include <QMainWindow>
#include <QList>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void mountFile(const QString& filePath);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onRenderSettingsChanged(const Qt::CheckState &state);
    void onSetCacheFolder(bool checked);

    void playFile(const QString& path);
    void removeFile(QWidget* fileWidget);

private:
    Ui::MainWindow *ui;
    std::unique_ptr<motioncam::IFuseFileSystem> mFuseFilesystem;
    QList<motioncam::MountId> mMountedFiles;
    QString mCacheRootFolder;
};

#endif // MAINWINDOW_H
