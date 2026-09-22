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

#include "GoogleDriveSettingsWidget.h"
#include "ui_GoogleDriveSettingsWidget.h"
#include "googledrive/GoogleDriveOAuth.h"
#include "googledrive/GoogleDriveSettings.h"

#include <QMessageBox>

GoogleDriveSettingsWidget::GoogleDriveSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::GoogleDriveSettingsWidget())
{
    m_ui->setupUi(this);

    connect(m_ui->authButton, &QPushButton::clicked, this, &GoogleDriveSettingsWidget::onAuthButtonClicked);
    connect(GoogleDriveOAuth::instance(), &GoogleDriveOAuth::authorizationFinished, this, [this](bool success, const QString& msg) {
        updateAccountStatus();
        if (!success) {
            QMessageBox::warning(this, tr("Google Drive Authorization"), tr("Authorization failed: %1").arg(msg));
        } else {
            QMessageBox::information(this, tr("Google Drive Authorization"), tr("Successfully connected to Google Drive!"));
        }
    });

    loadSettings();
}

GoogleDriveSettingsWidget::~GoogleDriveSettingsWidget() = default;

void GoogleDriveSettingsWidget::loadSettings()
{
    auto settings = GoogleDriveSettings::instance();
    m_ui->clientIdLineEdit->setText(settings->clientId());
    m_ui->clientSecretLineEdit->setText(settings->clientSecret());
    m_ui->autoSyncCheckBox->setChecked(settings->isAutoSyncOnSave());
    updateAccountStatus();
}

void GoogleDriveSettingsWidget::saveSettings()
{
    auto settings = GoogleDriveSettings::instance();
    settings->setClientId(m_ui->clientIdLineEdit->text().trimmed());
    settings->setClientSecret(m_ui->clientSecretLineEdit->text().trimmed());
    settings->setAutoSyncOnSave(m_ui->autoSyncCheckBox->isChecked());
}

void GoogleDriveSettingsWidget::updateAccountStatus()
{
    auto settings = GoogleDriveSettings::instance();
    if (settings->isAuthenticated()) {
        QString email = settings->accountEmail();
        m_ui->accountStatusLabel->setText(email.isEmpty() ? tr("Connected to Google Drive")
                                                          : tr("Connected: %1").arg(email));
        m_ui->authButton->setText(tr("Disconnect Account"));
    } else {
        m_ui->accountStatusLabel->setText(tr("Not connected to Google Drive"));
        m_ui->authButton->setText(tr("Connect Account"));
    }
}

void GoogleDriveSettingsWidget::onAuthButtonClicked()
{
    auto settings = GoogleDriveSettings::instance();
    if (settings->isAuthenticated()) {
        GoogleDriveOAuth::instance()->signOut();
        updateAccountStatus();
    } else {
        // Save current client ID / secret before attempting to authorize
        saveSettings();
        GoogleDriveOAuth::instance()->startAuthorization();
    }
}
