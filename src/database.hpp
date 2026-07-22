/*
    Copyright (C) 2026 Matej Gomboc https://github.com/MatejGomboc/claude-chats-browser

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
*/

#pragma once

#include <QSqlDatabase>
#include <QString>

namespace ChatsBrowser
{
    //! Opens and initialises the application's local SQLite store (schema + FTS5 index).
    class Database {
    public:
        Database() = delete;

        //! Returns the default database file path inside the per-user application data directory.
        [[nodiscard]] static QString defaultPath();

        /*!
            Opens (or reuses) a named connection to the database and ensures the schema exists.

            QSqlDatabase connections are thread-bound, so each thread must open its own
            connection under a unique name.

            \param connection_name Unique connection name, e.g. "main" or "import".
            \param database_path Path to the SQLite file; its directory is created if missing.
            \param error_message Set to a human-readable message when opening fails.
            \return An open database, or an invalid one on failure.
        */
        [[nodiscard]] static QSqlDatabase open(const QString& connection_name, const QString& database_path, QString* error_message);

    private:
        [[nodiscard]] static bool ensureSchema(QSqlDatabase& database, QString* error_message);
    };
}
