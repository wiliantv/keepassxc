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

#include "GoogleDriveOAuth.h"
#include "GoogleDriveSettings.h"
#include "networking/NetworkManager.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

GoogleDriveOAuth* GoogleDriveOAuth::s_instance = nullptr;

GoogleDriveOAuth* GoogleDriveOAuth::instance()
{
    if (!s_instance) {
        s_instance = new GoogleDriveOAuth(QCoreApplication::instance());
    }
    return s_instance;
}

GoogleDriveOAuth::GoogleDriveOAuth(QObject* parent)
    : QObject(parent)
    , m_tcpServer(new QTcpServer(this))
{
    connect(m_tcpServer, &QTcpServer::newConnection, this, &GoogleDriveOAuth::onNewConnection);
}

GoogleDriveOAuth::~GoogleDriveOAuth()
{
    cancelAuthorization();
}

void GoogleDriveOAuth::startAuthorization()
{
    cancelAuthorization();

    auto settings = GoogleDriveSettings::instance();
    QString clientId = settings->clientId().trimmed();
    if (clientId.isEmpty()) {
        emit authorizationFinished(false, tr("Google Client ID is not configured. Please enter your Client ID in Settings > Google Drive."));
        return;
    }

    if (!m_tcpServer->listen(QHostAddress::LocalHost, 0)) {
        emit authorizationFinished(false, tr("Failed to start local loopback listener: %1").arg(m_tcpServer->errorString()));
        return;
    }

    quint16 port = m_tcpServer->serverPort();
    m_redirectUri = QString("http://127.0.0.1:%1/oauth2callback").arg(port);
    m_oauthState = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QUrl authUrl("https://accounts.google.com/o/oauth2/v2/auth");
    QUrlQuery query;
    query.addQueryItem("client_id", clientId);
    query.addQueryItem("redirect_uri", m_redirectUri);
    query.addQueryItem("response_type", "code");
    query.addQueryItem("scope", "https://www.googleapis.com/auth/drive.file https://www.googleapis.com/auth/userinfo.email");
    query.addQueryItem("access_type", "offline");
    query.addQueryItem("prompt", "consent");
    query.addQueryItem("state", m_oauthState);
    authUrl.setQuery(query);

    QDesktopServices::openUrl(authUrl);
}

void GoogleDriveOAuth::cancelAuthorization()
{
    if (m_tcpServer && m_tcpServer->isListening()) {
        m_tcpServer->close();
    }
    m_oauthState.clear();
    m_redirectUri.clear();
}

void GoogleDriveOAuth::signOut()
{
    cancelAuthorization();
    m_accessToken.clear();
    m_tokenExpiry = QDateTime();
    GoogleDriveSettings::instance()->clearAuth();
}

void GoogleDriveOAuth::onNewConnection()
{
    QTcpSocket* socket = m_tcpServer->nextPendingConnection();
    if (!socket) {
        return;
    }

    connect(socket, &QTcpSocket::readyRead, [this, socket]() {
        QByteArray requestData = socket->readAll();
        QString requestStr = QString::fromUtf8(requestData);

        // Parse HTTP GET line: GET /oauth2callback?code=...&state=... HTTP/1.1
        int firstLineEnd = requestStr.indexOf("\r\n");
        if (firstLineEnd == -1) {
            firstLineEnd = requestStr.indexOf("\n");
        }
        QString firstLine = requestStr.left(firstLineEnd);
        QStringList parts = firstLine.split(' ');

        QString code;
        QString receivedState;
        QString error;

        if (parts.size() >= 2 && parts[0] == "GET") {
            QUrl url(parts[1]);
            QUrlQuery query(url.query());
            code = query.queryItemValue("code");
            receivedState = query.queryItemValue("state");
            error = query.queryItemValue("error");
        }

        bool success = !code.isEmpty() && (receivedState == m_oauthState) && error.isEmpty();

        QString html = QString(
            "<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<title>KeePassXC - Google Drive</title>"
            "<style>"
            "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; "
            "background: #1e1e24; color: #f0f0f0; display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; }"
            ".card { background: #2b2b36; border-radius: 12px; padding: 40px; text-align: center; box-shadow: 0 10px 25px rgba(0,0,0,0.4); max-width: 420px; }"
            "h1 { color: %1; margin-bottom: 12px; font-size: 22px; }"
            "p { color: #b0b0b8; line-height: 1.5; font-size: 15px; }"
            "</style></head><body>"
            "<div class='card'>"
            "<h1>%2</h1>"
            "<p>%3</p>"
            "</div></body></html>")
            .arg(success ? "#4caf50" : "#f44336",
                 success ? tr("Authorization Successful") : tr("Authorization Failed"),
                 success ? tr("You have successfully connected KeePassXC to Google Drive. You can now close this tab and return to the application.")
                         : tr("Failed to complete Google Drive authorization. You can close this tab and try again in KeePassXC."));

        QByteArray response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/html; charset=utf-8\r\n"
                              "Connection: close\r\n"
                              "Content-Length: " + QByteArray::number(html.toUtf8().size()) + "\r\n\r\n" +
                              html.toUtf8();

        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();

        QString redirectUriCopy = m_redirectUri;
        cancelAuthorization();

        if (success) {
            exchangeCodeForTokens(code, redirectUriCopy);
        } else {
            emit authorizationFinished(false, error.isEmpty() ? tr("State mismatch or missing authorization code.") : error);
        }
    });
}

void GoogleDriveOAuth::exchangeCodeForTokens(const QString& code, const QString& redirectUri)
{
    auto settings = GoogleDriveSettings::instance();
    QUrl tokenUrl("https://oauth2.googleapis.com/token");
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("code", code);
    postData.addQueryItem("client_id", settings->clientId().trimmed());
    postData.addQueryItem("client_secret", settings->clientSecret().trimmed());
    postData.addQueryItem("redirect_uri", redirectUri);
    postData.addQueryItem("grant_type", "authorization_code");

    QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply* reply = getNetMgr()->post(request, body);

    connect(reply, &QNetworkReply::finished, [this, reply, settings]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit authorizationFinished(false, tr("Token request failed: %1").arg(reply->errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit authorizationFinished(false, tr("Invalid JSON response from Google OAuth."));
            return;
        }

        QJsonObject obj = doc.object();
        m_accessToken = obj.value("access_token").toString();
        QString refreshToken = obj.value("refresh_token").toString();
        int expiresIn = obj.value("expires_in").toInt(3600);

        m_tokenExpiry = QDateTime::currentDateTime().addSecs(expiresIn - 60);

        if (!refreshToken.isEmpty()) {
            settings->setRefreshToken(refreshToken);
        }

        fetchUserProfile(m_accessToken);
    });
}

void GoogleDriveOAuth::fetchUserProfile(const QString& accessToken)
{
    QUrl userinfoUrl("https://www.googleapis.com/oauth2/v2/userinfo");
    QNetworkRequest request(userinfoUrl);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken).toUtf8());

    QNetworkReply* reply = getNetMgr()->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (doc.isObject()) {
                QString email = doc.object().value("email").toString();
                GoogleDriveSettings::instance()->setAccountEmail(email);
            }
        }
        emit authorizationFinished(true, tr("Successfully connected to Google Drive."));
    });
}

void GoogleDriveOAuth::getAccessToken(std::function<void(const QString& token, const QString& error)> callback)
{
    if (!m_accessToken.isEmpty() && m_tokenExpiry.isValid() && QDateTime::currentDateTime() < m_tokenExpiry) {
        callback(m_accessToken, QString());
        return;
    }

    refreshAccessToken(callback);
}

void GoogleDriveOAuth::refreshAccessToken(std::function<void(const QString& token, const QString& error)> callback)
{
    auto settings = GoogleDriveSettings::instance();
    QString refreshToken = settings->refreshToken().trimmed();
    if (refreshToken.isEmpty()) {
        callback(QString(), tr("Not logged into Google Drive. Please log in first."));
        return;
    }

    QUrl tokenUrl("https://oauth2.googleapis.com/token");
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("client_id", settings->clientId().trimmed());
    postData.addQueryItem("client_secret", settings->clientSecret().trimmed());
    postData.addQueryItem("refresh_token", refreshToken);
    postData.addQueryItem("grant_type", "refresh_token");

    QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply* reply = getNetMgr()->post(request, body);

    connect(reply, &QNetworkReply::finished, [this, reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            callback(QString(), tr("Failed to refresh access token: %1").arg(reply->errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            callback(QString(), tr("Invalid token response from Google."));
            return;
        }

        QJsonObject obj = doc.object();
        m_accessToken = obj.value("access_token").toString();
        int expiresIn = obj.value("expires_in").toInt(3600);
        m_tokenExpiry = QDateTime::currentDateTime().addSecs(expiresIn - 60);

        callback(m_accessToken, QString());
    });
}
