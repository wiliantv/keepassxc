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

#ifndef KEEPASSXC_GOOGLEDRIVESETTINGS_H
#define KEEPASSXC_GOOGLEDRIVESETTINGS_H

#include <QObject>
#include <QString>

class GoogleDriveSettings : public QObject
{
    Q_OBJECT

public:
    static GoogleDriveSettings* instance();

    QString clientId() const;
    void setClientId(const QString& clientId);

    QString clientSecret() const;
    void setClientSecret(const QString& clientSecret);

    QString refreshToken() const;
    void setRefreshToken(const QString& token);

    QString accountEmail() const;
    void setAccountEmail(const QString& email);

    bool isAutoSyncOnSave() const;
    void setAutoSyncOnSave(bool enable);

    bool isAuthenticated() const;
    void clearAuth();

    QString fileIdForDatabase(const QString& localFilePath) const;
    void setFileIdForDatabase(const QString& localFilePath, const QString& fileId);
    void removeFileIdForDatabase(const QString& localFilePath);

signals:
    void authChanged(bool authenticated);
    void settingsChanged();

private:
    explicit GoogleDriveSettings(QObject* parent = nullptr);
    static GoogleDriveSettings* s_instance;
};

#endif // KEEPASSXC_GOOGLEDRIVESETTINGS_H
