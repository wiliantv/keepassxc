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

#include "GoogleDriveClient.h"
#include "GoogleDriveOAuth.h"
#include "networking/NetworkManager.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

GoogleDriveClient* GoogleDriveClient::s_instance = nullptr;

GoogleDriveClient* GoogleDriveClient::instance()
{
    if (!s_instance) {
        s_instance = new GoogleDriveClient(QCoreApplication::instance());
    }
    return s_instance;
}

GoogleDriveClient::GoogleDriveClient(QObject* parent)
    : QObject(parent)
{
}

void GoogleDriveClient::listFiles(const QString& customQuery,
                                 std::function<void(bool success, const QList<DriveFile>& files, const QString& error)> callback)
{
    GoogleDriveOAuth::instance()->getAccessToken([this, customQuery, callback](const QString& token, const QString& err) {
        if (!err.isEmpty()) {
            callback(false, {}, err);
            return;
        }

        QUrl url("https://www.googleapis.com/drive/v3/files");
        QUrlQuery query;
        QString q = customQuery;
        if (q.isEmpty()) {
            q = "trashed = false and (name contains '.kdbx' or mimeType = 'application/vnd.google-apps.folder')";
        }
        query.addQueryItem("q", q);
        query.addQueryItem("fields", "files(id, name, mimeType, size, modifiedTime, md5Checksum)");
        query.addQueryItem("pageSize", "100");
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

        QNetworkReply* reply = getNetMgr()->get(request);
        connect(reply, &QNetworkReply::finished, [reply, callback]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                callback(false, {}, tr("Failed to list files from Google Drive: %1").arg(reply->errorString()));
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isObject()) {
                callback(false, {}, tr("Invalid JSON response from Google Drive."));
                return;
            }

            QList<DriveFile> fileList;
            QJsonArray files = doc.object().value("files").toArray();
            for (const auto& itemVal : files) {
                QJsonObject item = itemVal.toObject();
                DriveFile f;
                f.id = item.value("id").toString();
                f.name = item.value("name").toString();
                f.mimeType = item.value("mimeType").toString();
                f.size = item.value("size").toString().toLongLong();
                f.modifiedTime = QDateTime::fromString(item.value("modifiedTime").toString(), Qt::ISODate);
                f.md5Checksum = item.value("md5Checksum").toString();
                f.isFolder = (f.mimeType == "application/vnd.google-apps.folder");
                fileList.append(f);
            }

            callback(true, fileList, QString());
        });
    });
}

void GoogleDriveClient::downloadFile(const QString& fileId,
                                     const QString& destinationPath,
                                     std::function<void(bool success, const QString& error)> callback)
{
    GoogleDriveOAuth::instance()->getAccessToken([this, fileId, destinationPath, callback](const QString& token, const QString& err) {
        if (!err.isEmpty()) {
            callback(false, err);
            return;
        }

        QUrl url(QString("https://www.googleapis.com/drive/v3/files/%1").arg(fileId));
        QUrlQuery query;
        query.addQueryItem("alt", "media");
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

        QNetworkReply* reply = getNetMgr()->get(request);
        connect(reply, &QNetworkReply::finished, [reply, destinationPath, callback]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                callback(false, tr("Failed to download file from Google Drive: %1").arg(reply->errorString()));
                return;
            }

            QFile file(destinationPath);
            if (!file.open(QIODevice::WriteOnly)) {
                callback(false, tr("Failed to open local destination file for writing: %1").arg(file.errorString()));
                return;
            }

            file.write(reply->readAll());
            file.close();
            callback(true, QString());
        });
    });
}

void GoogleDriveClient::uploadNewFile(const QString& localFilePath,
                                     const QString& remoteFileName,
                                     const QString& parentFolderId,
                                     std::function<void(bool success, const DriveFile& file, const QString& error)> callback)
{
    GoogleDriveOAuth::instance()->getAccessToken([this, localFilePath, remoteFileName, parentFolderId, callback](const QString& token, const QString& err) {
        if (!err.isEmpty()) {
            callback(false, {}, err);
            return;
        }

        QFile localFile(localFilePath);
        if (!localFile.open(QIODevice::ReadOnly)) {
            callback(false, {}, tr("Cannot read database file: %1").arg(localFile.errorString()));
            return;
        }
        QByteArray fileData = localFile.readAll();
        localFile.close();

        QUrl url("https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart");
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

        QString boundary = "kpxc_gdrive_boundary_42";
        request.setHeader(QNetworkRequest::ContentTypeHeader, QString("multipart/related; boundary=%1").arg(boundary).toUtf8());

        QJsonObject metaObj;
        metaObj.insert("name", remoteFileName.isEmpty() ? QFileInfo(localFilePath).fileName() : remoteFileName);
        if (!parentFolderId.isEmpty()) {
            QJsonArray parents;
            parents.append(parentFolderId);
            metaObj.insert("parents", parents);
        }

        QByteArray metaData = QJsonDocument(metaObj).toJson(QJsonDocument::Compact);

        QByteArray multipartData;
        multipartData.append("--" + boundary.toUtf8() + "\r\n");
        multipartData.append("Content-Type: application/json; charset=UTF-8\r\n\r\n");
        multipartData.append(metaData);
        multipartData.append("\r\n--" + boundary.toUtf8() + "\r\n");
        multipartData.append("Content-Type: application/octet-stream\r\n\r\n");
        multipartData.append(fileData);
        multipartData.append("\r\n--" + boundary.toUtf8() + "--\r\n");

        QNetworkReply* reply = getNetMgr()->post(request, multipartData);
        connect(reply, &QNetworkReply::finished, [reply, callback]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                callback(false, {}, tr("Upload to Google Drive failed: %1").arg(reply->errorString()));
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isObject()) {
                callback(false, {}, tr("Invalid JSON response from Google Drive upload."));
                return;
            }

            QJsonObject obj = doc.object();
            DriveFile df;
            df.id = obj.value("id").toString();
            df.name = obj.value("name").toString();
            df.mimeType = obj.value("mimeType").toString();
            callback(true, df, QString());
        });
    });
}

void GoogleDriveClient::updateExistingFile(const QString& fileId,
                                          const QString& localFilePath,
                                          std::function<void(bool success, const DriveFile& file, const QString& error)> callback)
{
    GoogleDriveOAuth::instance()->getAccessToken([this, fileId, localFilePath, callback](const QString& token, const QString& err) {
        if (!err.isEmpty()) {
            callback(false, {}, err);
            return;
        }

        QFile localFile(localFilePath);
        if (!localFile.open(QIODevice::ReadOnly)) {
            callback(false, {}, tr("Cannot read database file for upload: %1").arg(localFile.errorString()));
            return;
        }
        QByteArray fileData = localFile.readAll();
        localFile.close();

        QUrl url(QString("https://www.googleapis.com/upload/drive/v3/files/%1?uploadType=media").arg(fileId));
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

        QNetworkReply* reply = getNetMgr()->sendCustomRequest(request, "PATCH", fileData);
        connect(reply, &QNetworkReply::finished, [reply, callback]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                callback(false, {}, tr("Failed to update database in Google Drive: %1").arg(reply->errorString()));
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isObject()) {
                callback(false, {}, tr("Invalid response after updating Google Drive file."));
                return;
            }

            QJsonObject obj = doc.object();
            DriveFile df;
            df.id = obj.value("id").toString();
            df.name = obj.value("name").toString();
            callback(true, df, QString());
        });
    });
}

void GoogleDriveClient::getFileMetadata(const QString& fileId,
                                       std::function<void(bool success, const DriveFile& file, const QString& error)> callback)
{
    GoogleDriveOAuth::instance()->getAccessToken([this, fileId, callback](const QString& token, const QString& err) {
        if (!err.isEmpty()) {
            callback(false, {}, err);
            return;
        }

        QUrl url(QString("https://www.googleapis.com/drive/v3/files/%1").arg(fileId));
        QUrlQuery query;
        query.addQueryItem("fields", "id, name, mimeType, size, modifiedTime, md5Checksum");
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

        QNetworkReply* reply = getNetMgr()->get(request);
        connect(reply, &QNetworkReply::finished, [reply, callback]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                callback(false, {}, tr("Failed to retrieve file metadata from Google Drive: %1").arg(reply->errorString()));
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isObject()) {
                callback(false, {}, tr("Invalid metadata response from Google Drive."));
                return;
            }

            QJsonObject obj = doc.object();
            DriveFile df;
            df.id = obj.value("id").toString();
            df.name = obj.value("name").toString();
            df.mimeType = obj.value("mimeType").toString();
            df.size = obj.value("size").toString().toLongLong();
            df.modifiedTime = QDateTime::fromString(obj.value("modifiedTime").toString(), Qt::ISODate);
            df.md5Checksum = obj.value("md5Checksum").toString();
            df.isFolder = (df.mimeType == "application/vnd.google-apps.folder");

            callback(true, df, QString());
        });
    });
}
