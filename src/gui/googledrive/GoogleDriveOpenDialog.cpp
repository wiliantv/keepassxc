/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "GoogleDriveOpenDialog.h"
#include "ui_GoogleDriveOpenDialog.h"
#include "googledrive/GoogleDriveClient.h"
#include "googledrive/GoogleDriveOAuth.h"
#include "googledrive/GoogleDriveSettings.h"
#include "gui/FileDialog.h"

#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTreeWidgetItem>

GoogleDriveOpenDialog::GoogleDriveOpenDialog(QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::GoogleDriveOpenDialog())
{
    m_ui->setupUi(this);

    m_ui->filesTreeWidget->setHeaderLabels({tr("Name"), tr("Size"), tr("Last Modified")});
    m_ui->filesTreeWidget->setColumnWidth(0, 300);
    m_ui->filesTreeWidget->setColumnWidth(1, 100);

    connect(m_ui->authButton, &QPushButton::clicked, this, &GoogleDriveOpenDialog::onAuthButtonClicked);
    connect(m_ui->refreshButton, &QPushButton::clicked, this, &GoogleDriveOpenDialog::onRefreshClicked);
    connect(m_ui->openButton, &QPushButton::clicked, this, &GoogleDriveOpenDialog::onOpenClicked);
    connect(m_ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_ui->searchLineEdit, &QLineEdit::textChanged, this, &GoogleDriveOpenDialog::onSearchTextChanged);
    connect(m_ui->filesTreeWidget, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem*, int) {
        onOpenClicked();
    });

    connect(GoogleDriveOAuth::instance(), &GoogleDriveOAuth::authorizationFinished, this, [this](bool success, const QString& msg) {
        m_ui->progressBar->setVisible(false);
        updateAccountStatus();
        if (success) {
            onRefreshClicked();
        } else {
            m_ui->statusLabel->setText(tr("Authorization error: %1").arg(msg));
        }
    });

    updateAccountStatus();
    if (GoogleDriveSettings::instance()->isAuthenticated()) {
        onRefreshClicked();
    }
}

GoogleDriveOpenDialog::~GoogleDriveOpenDialog() = default;

QString GoogleDriveOpenDialog::downloadedFilePath() const
{
    return m_downloadedPath;
}

QString GoogleDriveOpenDialog::selectedFileId() const
{
    return m_selectedFileId;
}

void GoogleDriveOpenDialog::updateAccountStatus()
{
    auto settings = GoogleDriveSettings::instance();
    if (settings->isAuthenticated()) {
        QString email = settings->accountEmail();
        m_ui->accountStatusLabel->setText(email.isEmpty() ? tr("Connected to Google Drive")
                                                          : tr("Connected: %1").arg(email));
        m_ui->authButton->setText(tr("Disconnect"));
    } else {
        m_ui->accountStatusLabel->setText(tr("Not connected to Google Drive"));
        m_ui->authButton->setText(tr("Connect Google Account"));
        m_ui->filesTreeWidget->clear();
        m_cachedFiles.clear();
    }
}

void GoogleDriveOpenDialog::onAuthButtonClicked()
{
    auto settings = GoogleDriveSettings::instance();
    if (settings->isAuthenticated()) {
        GoogleDriveOAuth::instance()->signOut();
        updateAccountStatus();
        m_ui->statusLabel->setText(tr("Disconnected from Google Drive."));
    } else {
        m_ui->progressBar->setVisible(true);
        m_ui->progressBar->setRange(0, 0); // busy indicator
        m_ui->statusLabel->setText(tr("Authorizing in browser..."));
        GoogleDriveOAuth::instance()->startAuthorization();
    }
}

void GoogleDriveOpenDialog::onRefreshClicked()
{
    if (!GoogleDriveSettings::instance()->isAuthenticated()) {
        m_ui->statusLabel->setText(tr("Please connect your Google account first."));
        return;
    }

    m_ui->progressBar->setVisible(true);
    m_ui->progressBar->setRange(0, 0);
    m_ui->statusLabel->setText(tr("Loading database files from Google Drive..."));

    GoogleDriveClient::instance()->listFiles("", [this](bool success, const QList<DriveFile>& files, const QString& error) {
        m_ui->progressBar->setVisible(false);
        if (!success) {
            m_ui->statusLabel->setText(tr("Error loading files: %1").arg(error));
            return;
        }

        m_cachedFiles = files;
        populateFileList(files);
        m_ui->statusLabel->setText(tr("Found %1 database file(s).").arg(files.size()));
    });
}

void GoogleDriveOpenDialog::populateFileList(const QList<DriveFile>& files)
{
    m_ui->filesTreeWidget->clear();
    QString filter = m_ui->searchLineEdit->text().trimmed();

    for (const auto& f : files) {
        if (f.isFolder) {
            continue;
        }
        if (!filter.isEmpty() && !f.name.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        auto* item = new QTreeWidgetItem(m_ui->filesTreeWidget);
        item->setText(0, f.name);
        item->setText(1, formatFileSize(f.size));
        item->setText(2, f.modifiedTime.isValid() ? f.modifiedTime.toString("yyyy-MM-dd HH:mm") : tr("Unknown"));
        item->setData(0, Qt::UserRole, f.id);
        item->setData(0, Qt::UserRole + 1, f.name);
    }

    if (m_ui->filesTreeWidget->topLevelItemCount() > 0) {
        m_ui->filesTreeWidget->setCurrentItem(m_ui->filesTreeWidget->topLevelItem(0));
    }
}

void GoogleDriveOpenDialog::onSearchTextChanged(const QString&)
{
    populateFileList(m_cachedFiles);
}

QString GoogleDriveOpenDialog::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    }
    return QString("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 2));
}

void GoogleDriveOpenDialog::onOpenClicked()
{
    auto* item = m_ui->filesTreeWidget->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("Select File"), tr("Please select a database file from the list."));
        return;
    }

    QString fileId = item->data(0, Qt::UserRole).toString();
    QString fileName = item->data(0, Qt::UserRole + 1).toString();

    // Ask user where to save the local synchronized copy
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString destination = FileDialog::getSaveFileName(this,
                                                      tr("Choose Local Location for Database"),
                                                      defaultDir + "/" + fileName,
                                                      QString("%1 (*.kdbx)").arg(tr("KeePass 2 Database")));

    if (destination.isEmpty()) {
        return;
    }

    m_ui->progressBar->setVisible(true);
    m_ui->progressBar->setRange(0, 0);
    m_ui->statusLabel->setText(tr("Downloading database from Google Drive..."));
    m_ui->openButton->setEnabled(false);

    GoogleDriveClient::instance()->downloadFile(fileId, destination, [this, fileId, destination](bool success, const QString& error) {
        m_ui->progressBar->setVisible(false);
        m_ui->openButton->setEnabled(true);

        if (!success) {
            QMessageBox::critical(this, tr("Download Failed"), tr("Could not download database: %1").arg(error));
            m_ui->statusLabel->setText(tr("Download failed."));
            return;
        }

        // Link local copy to Google Drive fileId for future syncing
        GoogleDriveSettings::instance()->setFileIdForDatabase(destination, fileId);

        m_downloadedPath = destination;
        m_selectedFileId = fileId;
        accept();
    });
}
