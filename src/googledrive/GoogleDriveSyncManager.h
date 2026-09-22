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

#ifndef KEEPASSXC_GOOGLEDRIVESYNCMANAGER_H
#define KEEPASSXC_GOOGLEDRIVESYNCMANAGER_H

#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <functional>

class Database;

class GoogleDriveSyncManager : public QObject
{
    Q_OBJECT

public:
    static GoogleDriveSyncManager* instance();

    bool isDatabaseLinked(const QSharedPointer<Database>& db) const;
    QString getLinkedFileId(const QSharedPointer<Database>& db) const;

    void linkDatabaseToFile(const QSharedPointer<Database>& db, const QString& fileId);
    void unlinkDatabase(const QSharedPointer<Database>& db);

    void syncDatabase(const QSharedPointer<Database>& db,
                      std::function<void(bool success, const QString& message)> callback = nullptr);

    void uploadAndLinkDatabase(const QSharedPointer<Database>& db,
                               const QString& remoteFileName,
                               const QString& parentFolderId,
                               std::function<void(bool success, const QString& message)> callback = nullptr);

signals:
    void syncStarted(const QString& databasePath);
    void syncProgress(int percentage, const QString& statusMessage);
    void syncFinished(const QString& databasePath, bool success, const QString& message);

private:
    explicit GoogleDriveSyncManager(QObject* parent = nullptr);
    static GoogleDriveSyncManager* s_instance;
};

#endif // KEEPASSXC_GOOGLEDRIVESYNCMANAGER_H
