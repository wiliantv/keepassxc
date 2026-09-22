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

#ifndef KEEPASSXC_GOOGLEDRIVEOAUTH_H
#define KEEPASSXC_GOOGLEDRIVEOAUTH_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <functional>

class QTcpServer;
class QNetworkReply;

class GoogleDriveOAuth : public QObject
{
    Q_OBJECT

public:
    static GoogleDriveOAuth* instance();

    void startAuthorization();
    void cancelAuthorization();
    void signOut();

    void getAccessToken(std::function<void(const QString& token, const QString& error)> callback);

signals:
    void authorizationFinished(bool success, const QString& message);
    void authorizationCancelled();

private slots:
    void onNewConnection();

private:
    explicit GoogleDriveOAuth(QObject* parent = nullptr);
    ~GoogleDriveOAuth() override;

    void exchangeCodeForTokens(const QString& code, const QString& redirectUri);
    void fetchUserProfile(const QString& accessToken);
    void refreshAccessToken(std::function<void(const QString& token, const QString& error)> callback);

    QTcpServer* m_tcpServer = nullptr;
    QString m_oauthState;
    QString m_redirectUri;
    QString m_accessToken;
    QDateTime m_tokenExpiry;

    static GoogleDriveOAuth* s_instance;
};

#endif // KEEPASSXC_GOOGLEDRIVEOAUTH_H
