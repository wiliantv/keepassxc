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

#ifndef KEEPASSXC_GOOGLEDRIVESAVEDIALOG_H
#define KEEPASSXC_GOOGLEDRIVESAVEDIALOG_H

#include <QDialog>
#include <QScopedPointer>
#include <QSharedPointer>

class Database;

namespace Ui
{
    class GoogleDriveSaveDialog;
}

class GoogleDriveSaveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GoogleDriveSaveDialog(const QSharedPointer<Database>& db, QWidget* parent = nullptr);
    ~GoogleDriveSaveDialog() override;

private slots:
    void onSaveClicked();

private:
    QScopedPointer<Ui::GoogleDriveSaveDialog> m_ui;
    QSharedPointer<Database> m_db;
};

#endif // KEEPASSXC_GOOGLEDRIVESAVEDIALOG_H
