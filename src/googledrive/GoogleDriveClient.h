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

#ifndef KEEPASSXC_GOOGLEDRIVECLIENT_H
#define KEEPASSXC_GOOGLEDRIVECLIENT_H

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <functional>

struct DriveFile
{
    QString id;
    QString name;
    QString mimeType;
    qint64 size = 0;
    QDateTime modifiedTime;
    QString md5Checksum;
    bool isFolder = false;
};

class GoogleDriveClient : public QObject
{
    Q_OBJECT

public:
    static GoogleDriveClient* instance();

    void listFiles(const QString& customQuery,
                   std::function<void(bool success, const QList<DriveFile>& files, const QString& error)> callback);

    void downloadFile(const QString& fileId,
                      const QString& destinationPath,
                      std::function<void(bool success, const QString& error)> callback);

    void uploadNewFile(const QString& localFilePath,
                       const QString& remoteFileName,
                       const QString& parentFolderId,
                       std::function<void(bool success, const DriveFile& file, const QString& error)> callback);

    void updateExistingFile(const QString& fileId,
                            const QString& localFilePath,
                            std::function<void(bool success, const DriveFile& file, const QString& error)> callback);

    void getFileMetadata(const QString& fileId,
                         std::function<void(bool success, const DriveFile& file, const QString& error)> callback);

private:
    explicit GoogleDriveClient(QObject* parent = nullptr);
    static GoogleDriveClient* s_instance;
};

#endif // KEEPASSXC_GOOGLEDRIVECLIENT_H
