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

#include "GoogleDriveSyncManager.h"
#include "GoogleDriveClient.h"
#include "GoogleDriveSettings.h"
#include "core/Database.h"
#include "core/Merger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

GoogleDriveSyncManager* GoogleDriveSyncManager::s_instance = nullptr;

GoogleDriveSyncManager* GoogleDriveSyncManager::instance()
{
    if (!s_instance) {
        s_instance = new GoogleDriveSyncManager(QCoreApplication::instance());
    }
    return s_instance;
}

GoogleDriveSyncManager::GoogleDriveSyncManager(QObject* parent)
    : QObject(parent)
{
}

bool GoogleDriveSyncManager::isDatabaseLinked(const QSharedPointer<Database>& db) const
{
    if (!db || db->filePath().isEmpty()) {
        return false;
    }
    return !getLinkedFileId(db).isEmpty();
}

QString GoogleDriveSyncManager::getLinkedFileId(const QSharedPointer<Database>& db) const
{
    if (!db) {
        return {};
    }
    return GoogleDriveSettings::instance()->fileIdForDatabase(db->filePath());
}

void GoogleDriveSyncManager::linkDatabaseToFile(const QSharedPointer<Database>& db, const QString& fileId)
{
    if (!db || db->filePath().isEmpty()) {
        return;
    }
    GoogleDriveSettings::instance()->setFileIdForDatabase(db->filePath(), fileId);
}

void GoogleDriveSyncManager::unlinkDatabase(const QSharedPointer<Database>& db)
{
    if (!db || db->filePath().isEmpty()) {
        return;
    }
    GoogleDriveSettings::instance()->removeFileIdForDatabase(db->filePath());
}

void GoogleDriveSyncManager::syncDatabase(const QSharedPointer<Database>& db,
                                         std::function<void(bool success, const QString& message)> callback)
{
    if (!db || db->filePath().isEmpty()) {
        QString msg = tr("No active database to sync.");
        emit syncFinished(QString(), false, msg);
        if (callback) {
            callback(false, msg);
        }
        return;
    }

    if (!GoogleDriveSettings::instance()->isAuthenticated()) {
        QString msg = tr("Not logged in to Google Drive. Please authenticate first.");
        emit syncFinished(db->filePath(), false, msg);
        if (callback) {
            callback(false, msg);
        }
        return;
    }

    QString fileId = getLinkedFileId(db);
    if (fileId.isEmpty()) {
        QString msg = tr("Database '%1' is not linked to any file on Google Drive.").arg(QFileInfo(db->filePath()).fileName());
        emit syncFinished(db->filePath(), false, msg);
        if (callback) {
            callback(false, msg);
        }
        return;
    }

    emit syncStarted(db->filePath());
    emit syncProgress(10, tr("Downloading remote database from Google Drive..."));

    QString tempFilePath = QDir::tempPath() + QString("/kpxc_gdrive_%1.kdbx").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    GoogleDriveClient::instance()->downloadFile(fileId, tempFilePath, [this, db, fileId, tempFilePath, callback](bool downloadSuccess, const QString& downloadError) {
        if (!downloadSuccess) {
            QFile::remove(tempFilePath);
            QString msg = tr("Download failed: %1").arg(downloadError);
            emit syncFinished(db->filePath(), false, msg);
            if (callback) {
                callback(false, msg);
            }
            return;
        }

        emit syncProgress(50, tr("Merging local and remote changes..."));

        QString openError;
        QSharedPointer<Database> remoteDb = QSharedPointer<Database>::create();
        remoteDb->markAsTemporaryDatabase();
        if (!remoteDb->open(tempFilePath, db->key(), &openError)) {
            QFile::remove(tempFilePath);
            QString msg = tr("Failed to unlock downloaded database with the current key: %1").arg(openError);
            emit syncFinished(db->filePath(), false, msg);
            if (callback) {
                callback(false, msg);
            }
            return;
        }

        Merger firstMerge(db.data(), remoteDb.data());
        Merger secondMerge(remoteDb.data(), db.data());
        auto changeList = firstMerge.merge() + secondMerge.merge();

        if (!changeList.isEmpty()) {
            QString saveError;
            if (!db->save(Database::Atomic, {}, &saveError)) {
                QFile::remove(tempFilePath);
                QString msg = tr("Failed to save local merged database: %1").arg(saveError);
                emit syncFinished(db->filePath(), false, msg);
                if (callback) {
                    callback(false, msg);
                }
                return;
            }
        }

        emit syncProgress(75, tr("Uploading updated database to Google Drive..."));

        GoogleDriveClient::instance()->updateExistingFile(fileId, db->filePath(), [this, db, tempFilePath, callback](bool updateSuccess, const DriveFile&, const QString& updateError) {
            QFile::remove(tempFilePath);
            if (!updateSuccess) {
                QString msg = tr("Failed to update Google Drive file: %1").arg(updateError);
                emit syncFinished(db->filePath(), false, msg);
                if (callback) {
                    callback(false, msg);
                }
                return;
            }

            emit syncProgress(100, tr("Sync complete."));
            QString msg = tr("Google Drive sync completed successfully.");
            emit syncFinished(db->filePath(), true, msg);
            if (callback) {
                callback(true, msg);
            }
        });
    });
}

void GoogleDriveSyncManager::uploadAndLinkDatabase(const QSharedPointer<Database>& db,
                                                  const QString& remoteFileName,
                                                  const QString& parentFolderId,
                                                  std::function<void(bool success, const QString& message)> callback)
{
    if (!db || db->filePath().isEmpty()) {
        QString msg = tr("No active database to upload.");
        if (callback) {
            callback(false, msg);
        }
        return;
    }

    if (!GoogleDriveSettings::instance()->isAuthenticated()) {
        QString msg = tr("Not logged in to Google Drive. Please log in first.");
        if (callback) {
            callback(false, msg);
        }
        return;
    }

    emit syncStarted(db->filePath());
    emit syncProgress(20, tr("Uploading database to Google Drive..."));

    QString targetName = remoteFileName.trimmed();
    if (targetName.isEmpty()) {
        targetName = QFileInfo(db->filePath()).fileName();
    }
    if (!targetName.endsWith(".kdbx", Qt::CaseInsensitive)) {
        targetName += ".kdbx";
    }

    GoogleDriveClient::instance()->uploadNewFile(db->filePath(), targetName, parentFolderId, [this, db, callback](bool success, const DriveFile& file, const QString& error) {
        if (!success) {
            QString msg = tr("Upload to Google Drive failed: %1").arg(error);
            emit syncFinished(db->filePath(), false, msg);
            if (callback) {
                callback(false, msg);
            }
            return;
        }

        linkDatabaseToFile(db, file.id);
        emit syncProgress(100, tr("Database uploaded and linked to Google Drive."));
        QString msg = tr("Database successfully uploaded to Google Drive and linked for synchronization.");
        emit syncFinished(db->filePath(), true, msg);
        if (callback) {
            callback(true, msg);
        }
    });
}
