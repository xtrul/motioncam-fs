#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPushButton>
#include <QFileInfo>
#include <QProcess>
#include <QMessageBox>

#ifdef _WIN32
#include "win/FuseFileSystemImpl_Win.h"
#endif

namespace {
    motioncam::FileRenderOptions getRenderOptions(Ui::MainWindow& ui) {
        if(ui.draftModeCheckBox->checkState() == Qt::CheckState::Checked)
            return motioncam::RENDER_OPT_DRAFT;

        return motioncam::RENDER_OPT_NONE;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Enable drag and drop on the scroll area
    ui->dragAndDropScrollArea->setAcceptDrops(true);
    ui->dragAndDropScrollArea->installEventFilter(this);

#ifdef _WIN32
    mFuseFilesystem = std::make_unique<motioncam::FuseFileSystemImpl_Win>();
#endif
}

MainWindow::~MainWindow() {
    delete ui;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->dragAndDropScrollArea) {
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent *dragEvent = static_cast<QDragEnterEvent*>(event);
            if (dragEvent->mimeData()->hasUrls()) {
                // Check if at least one file has the extension we want
                for (const QUrl &url : dragEvent->mimeData()->urls()) {
                    QString filePath = url.toLocalFile();

                    // Replace ".txt" with your desired file extension
                    if (filePath.endsWith(".mcraw", Qt::CaseInsensitive)) {
                        dragEvent->acceptProposedAction();
                        return true;
                    }
                }
            }

            return true;
        }
        else if (event->type() == QEvent::Drop) {
            QDropEvent *dropEvent = static_cast<QDropEvent*>(event);

            if (dropEvent->mimeData()->hasUrls()) {
                for (const QUrl &url : dropEvent->mimeData()->urls()) {
                    QString filePath = url.toLocalFile();
                    if (filePath.endsWith(".mcraw", Qt::CaseInsensitive)) {
                        mountFile(filePath);
                    }
                }

                dropEvent->acceptProposedAction();
            }

            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::mountFile(const QString& filePath) {
    // Extract just the filename from the path
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QString dstPath = fileInfo.path() + "/" + fileInfo.baseName();
    motioncam::MountId mountId;

    try {
        mountId = mFuseFilesystem->mount(
            getRenderOptions(*ui), filePath.toStdString(), dstPath.toStdString());
    }
    catch(std::runtime_error& e) {
        // log error
        return;
    }

    if(mountId == motioncam::InvalidMountId) {
        // todo: log error
        return;
    }

    // Get the scroll area's content widget and its layout
    QWidget* scrollContent = ui->dragAndDropScrollArea->widget();
    QVBoxLayout* scrollLayout = qobject_cast<QVBoxLayout*>(scrollContent->layout());

    // Create a widget to hold a filename label and remove button
    QWidget* fileWidget = new QWidget(scrollContent);

    fileWidget->setFixedHeight(40);
    fileWidget->setProperty("filePath", filePath);
    fileWidget->setProperty("mountId", mountId);

    QHBoxLayout* fileLayout = new QHBoxLayout(fileWidget);
    fileLayout->setContentsMargins(5, 5, 5, 5);

    // Create and add the filename label
    QLabel* fileLabel = new QLabel(fileName, fileWidget);
    fileLabel->setToolTip(filePath); // Show full path on hover
    fileLayout->addWidget(fileLabel);

    // Add a spacer to push the button to the right
    fileLayout->addStretch();

    // Create and add the remove button
    QPushButton* playButton = new QPushButton("Play", fileWidget);
    playButton->setMaximumWidth(80);
    fileLayout->addWidget(playButton);

    // Create and add the remove button
    QPushButton* removeButton = new QPushButton("Remove", fileWidget);
    removeButton->setMaximumWidth(80);
    fileLayout->addWidget(removeButton);

    // Add the file widget to the scroll area
    scrollLayout->insertWidget(0, fileWidget);

    // Hide the drag-drop label since we now have content
    ui->dragAndDropLabel->hide();

    // Connect buttons
    connect(playButton, &QPushButton::clicked, this, [this, filePath] {
        playFile(filePath);
    });

    connect(removeButton, &QPushButton::clicked, this, [this, fileWidget] {
        removeFile(fileWidget);
    });

    mMountedFiles.append(mountId);
}

void MainWindow::playFile(const QString& path) {
    QStringList arguments;
    arguments << path;

    bool success = QProcess::startDetached("MCRAW_Player.exe", arguments);
    if (!success)
        QMessageBox::warning(this, "Error", QString("Failed to launch player with file: %1").arg(path));
}

void MainWindow::removeFile(QWidget* fileWidget) {
    QWidget* scrollContent = ui->dragAndDropScrollArea->widget();
    QVBoxLayout* scrollLayout = qobject_cast<QVBoxLayout*>(scrollContent->layout());

    scrollLayout->removeWidget(fileWidget);
    fileWidget->deleteLater();

    // If all files are removed, show the drag-drop label again
    if (scrollLayout->count() == 0) {
        ui->dragAndDropLabel->show();
    }

    // Unmount the file
    bool ok = false;
    motioncam::MountId mountId = fileWidget->property("mountId").toInt(&ok);
    if(ok)
        mFuseFilesystem->unmount(mountId);
}

void MainWindow::onDraftModeCheckBoxChanged(const Qt::CheckState &checkState) {
    auto it = mMountedFiles.begin();
    auto renderOptions = getRenderOptions(*ui);

    while(it != mMountedFiles.end()) {
        mFuseFilesystem->updateOptions(*it, renderOptions);
        ++it;
    }
}
