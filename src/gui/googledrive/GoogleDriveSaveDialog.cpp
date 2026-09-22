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

#include "GoogleDriveSaveDialog.h"
#include "ui_GoogleDriveSaveDialog.h"
#include "core/Database.h"
#include "googledrive/GoogleDriveSettings.h"
#include "googledrive/GoogleDriveSyncManager.h"

#include <QFileInfo>
#include <QMessageBox>

GoogleDriveSaveDialog::GoogleDriveSaveDialog(const QSharedPointer<Database>& db, QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::GoogleDriveSaveDialog())
    , m_db(db)
{
    m_ui->setupUi(this);

    if (m_db) {
        QString fileName = QFileInfo(m_db->filePath()).fileName();
        m_ui->fileNameLineEdit->setText(fileName);
    }

    auto settings = GoogleDriveSettings::instance();
    QString email = settings->accountEmail();
    m_ui->accountLabel->setText(email.isEmpty() ? tr("Account: Connected") : tr("Account: %1").arg(email));

    connect(m_ui->saveButton, &QPushButton::clicked, this, &GoogleDriveSaveDialog::onSaveClicked);
    connect(m_ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

GoogleDriveSaveDialog::~GoogleDriveSaveDialog() = default;

void GoogleDriveSaveDialog::onSaveClicked()
{
    if (!m_db) {
        return;
    }

    QString remoteName = m_ui->fileNameLineEdit->text().trimmed();
    if (remoteName.isEmpty()) {
        QMessageBox::warning(this, tr("File Name"), tr("Please enter a valid file name."));
        return;
    }

    m_ui->progressBar->setVisible(true);
    m_ui->progressBar->setRange(0, 0);
    m_ui->statusLabel->setText(tr("Uploading database to Google Drive..."));
    m_ui->saveButton->setEnabled(false);

    GoogleDriveSyncManager::instance()->uploadAndLinkDatabase(m_db, remoteName, "", [this](bool success, const QString& message) {
        m_ui->progressBar->setVisible(false);
        m_ui->saveButton->setEnabled(true);

        if (!success) {
            QMessageBox::critical(this, tr("Upload Failed"), message);
            m_ui->statusLabel->setText(tr("Upload failed."));
            return;
        }

        QMessageBox::information(this, tr("Google Drive"), tr("Database uploaded and linked successfully!"));
        accept();
    });
}
