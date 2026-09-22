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

#include "GoogleDriveSettings.h"
#include "core/Config.h"

#include <QCoreApplication>
#include <QSettings>

GoogleDriveSettings* GoogleDriveSettings::s_instance = nullptr;

GoogleDriveSettings* GoogleDriveSettings::instance()
{
    if (!s_instance) {
        s_instance = new GoogleDriveSettings(QCoreApplication::instance());
    }
    return s_instance;
}

GoogleDriveSettings::GoogleDriveSettings(QObject* parent)
    : QObject(parent)
{
}

QString GoogleDriveSettings::clientId() const
{
    QSettings settings;
    return settings.value("GoogleDrive/ClientId", "").toString();
}

void GoogleDriveSettings::setClientId(const QString& clientId)
{
    QSettings settings;
    settings.setValue("GoogleDrive/ClientId", clientId);
    emit settingsChanged();
}

QString GoogleDriveSettings::clientSecret() const
{
    QSettings settings;
    return settings.value("GoogleDrive/ClientSecret", "").toString();
}

void GoogleDriveSettings::setClientSecret(const QString& clientSecret)
{
    QSettings settings;
    settings.setValue("GoogleDrive/ClientSecret", clientSecret);
    emit settingsChanged();
}

QString GoogleDriveSettings::refreshToken() const
{
    QSettings settings;
    return settings.value("GoogleDrive/RefreshToken", "").toString();
}

void GoogleDriveSettings::setRefreshToken(const QString& token)
{
    QSettings settings;
    if (token.isEmpty()) {
        settings.remove("GoogleDrive/RefreshToken");
    } else {
        settings.setValue("GoogleDrive/RefreshToken", token);
    }
    emit authChanged(!token.isEmpty());
    emit settingsChanged();
}

QString GoogleDriveSettings::accountEmail() const
{
    QSettings settings;
    return settings.value("GoogleDrive/AccountEmail", "").toString();
}

void GoogleDriveSettings::setAccountEmail(const QString& email)
{
    QSettings settings;
    if (email.isEmpty()) {
        settings.remove("GoogleDrive/AccountEmail");
    } else {
        settings.setValue("GoogleDrive/AccountEmail", email);
    }
    emit settingsChanged();
}

bool GoogleDriveSettings::isAutoSyncOnSave() const
{
    QSettings settings;
    return settings.value("GoogleDrive/AutoSyncOnSave", true).toBool();
}

void GoogleDriveSettings::setAutoSyncOnSave(bool enable)
{
    QSettings settings;
    settings.setValue("GoogleDrive/AutoSyncOnSave", enable);
    emit settingsChanged();
}

bool GoogleDriveSettings::isAuthenticated() const
{
    return !refreshToken().trimmed().isEmpty();
}

void GoogleDriveSettings::clearAuth()
{
    setRefreshToken("");
    setAccountEmail("");
}

QString GoogleDriveSettings::fileIdForDatabase(const QString& localFilePath) const
{
    if (localFilePath.isEmpty()) {
        return {};
    }
    QSettings settings;
    settings.beginGroup("GoogleDrive/FileMappings");
    QString id = settings.value(localFilePath, "").toString();
    settings.endGroup();
    return id;
}

void GoogleDriveSettings::setFileIdForDatabase(const QString& localFilePath, const QString& fileId)
{
    if (localFilePath.isEmpty()) {
        return;
    }
    QSettings settings;
    settings.beginGroup("GoogleDrive/FileMappings");
    if (fileId.isEmpty()) {
        settings.remove(localFilePath);
    } else {
        settings.setValue(localFilePath, fileId);
    }
    settings.endGroup();
}

void GoogleDriveSettings::removeFileIdForDatabase(const QString& localFilePath)
{
    setFileIdForDatabase(localFilePath, "");
}
