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

#include "database.hpp"
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QVariant>

namespace ChatsBrowser
{
    namespace
    {
        constexpr int SCHEMA_VERSION = 1;

        //! Statements that create the version-1 schema. Executed in order inside a transaction.
        const char* const SCHEMA_STATEMENTS[] = {
            "CREATE TABLE conversations ("
            "    uuid TEXT PRIMARY KEY,"
            "    name TEXT NOT NULL DEFAULT '',"
            "    summary TEXT NOT NULL DEFAULT '',"
            "    created_at TEXT NOT NULL DEFAULT '',"
            "    updated_at TEXT NOT NULL DEFAULT '',"
            "    message_count INTEGER NOT NULL DEFAULT 0,"
            "    has_content INTEGER NOT NULL DEFAULT 0"
            ")",

            "CREATE TABLE messages ("
            "    uuid TEXT PRIMARY KEY,"
            "    conversation_uuid TEXT NOT NULL,"
            "    parent_uuid TEXT NOT NULL DEFAULT '',"
            "    sender TEXT NOT NULL DEFAULT '',"
            "    created_at TEXT NOT NULL DEFAULT '',"
            "    text TEXT NOT NULL DEFAULT '',"
            "    raw_json TEXT NOT NULL DEFAULT '{}'"
            ")",

            "CREATE INDEX idx_messages_conversation ON messages(conversation_uuid)",

            "CREATE VIRTUAL TABLE messages_fts USING fts5(text, content='messages', content_rowid='rowid')",

            "CREATE TRIGGER messages_after_insert AFTER INSERT ON messages BEGIN"
            "    INSERT INTO messages_fts(rowid, text) VALUES (new.rowid, new.text);"
            "END",

            "CREATE TRIGGER messages_after_delete AFTER DELETE ON messages BEGIN"
            "    INSERT INTO messages_fts(messages_fts, rowid, text) VALUES ('delete', old.rowid, old.text);"
            "END",

            "CREATE TRIGGER messages_after_update AFTER UPDATE ON messages BEGIN"
            "    INSERT INTO messages_fts(messages_fts, rowid, text) VALUES ('delete', old.rowid, old.text);"
            "    INSERT INTO messages_fts(rowid, text) VALUES (new.rowid, new.text);"
            "END",
        };
    }

    QString Database::defaultPath()
    {
        QString data_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        return data_dir + "/chats.db";
    }

    QSqlDatabase Database::open(const QString& connection_name, const QString& database_path, QString* error_message)
    {
        if (QSqlDatabase::contains(connection_name)) {
            return QSqlDatabase::database(connection_name);
        }

        QDir database_dir = QFileInfo(database_path).absoluteDir();
        if ((!database_dir.exists()) && (!database_dir.mkpath("."))) {
            *error_message = QString("Cannot create directory %1").arg(database_dir.absolutePath());
            return QSqlDatabase();
        }

        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connection_name);
        database.setDatabaseName(database_path);
        if (!database.open()) {
            *error_message = QString("Cannot open %1: %2").arg(database_path, database.lastError().text());
            QSqlDatabase::removeDatabase(connection_name);
            return QSqlDatabase();
        }

        QSqlQuery pragma_query(database);
        if (!pragma_query.exec("PRAGMA journal_mode = WAL")) {
            *error_message = QString("Cannot enable WAL mode: %1").arg(pragma_query.lastError().text());
            return QSqlDatabase();
        }
        // PRAGMA journal_mode returns a row ("wal"); release the cursor so it does not
        // block the schema transaction's COMMIT.
        pragma_query.finish();

        if (!ensureSchema(database, error_message)) {
            return QSqlDatabase();
        }

        return database;
    }

    bool Database::ensureSchema(QSqlDatabase& database, QString* error_message)
    {
        QSqlQuery version_query(database);
        if (!version_query.exec("PRAGMA user_version")) {
            *error_message = QString("Cannot read schema version: %1").arg(version_query.lastError().text());
            return false;
        }
        int current_version = 0;
        if (version_query.next()) {
            current_version = version_query.value(0).toInt();
        }
        // Release the read cursor: an active statement would block the schema COMMIT below.
        version_query.finish();

        if (current_version == SCHEMA_VERSION) {
            return true;
        }
        if (current_version > SCHEMA_VERSION) {
            *error_message = QString("Database schema version %1 is newer than this app supports (%2)").arg(current_version).arg(SCHEMA_VERSION);
            return false;
        }
        if (current_version != 0) {
            *error_message = QString("Unknown database schema version %1").arg(current_version);
            return false;
        }

        if (!database.transaction()) {
            *error_message = QString("Cannot start schema transaction: %1").arg(database.lastError().text());
            return false;
        }

        for (const char* const statement : SCHEMA_STATEMENTS) {
            QSqlQuery schema_query(database);
            if (!schema_query.exec(statement)) {
                *error_message = QString("Schema statement failed (%1): %2").arg(statement, schema_query.lastError().text());
                database.rollback();
                return false;
            }
        }

        QSqlQuery set_version_query(database);
        if (!set_version_query.exec(QString("PRAGMA user_version = %1").arg(SCHEMA_VERSION))) {
            *error_message = QString("Cannot set schema version: %1").arg(set_version_query.lastError().text());
            database.rollback();
            return false;
        }

        if (!database.commit()) {
            *error_message = QString("Cannot commit schema: %1").arg(database.lastError().text());
            return false;
        }

        return true;
    }
}
