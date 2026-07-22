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

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace ChatsBrowser
{
    /*!
        Imports a claude.ai export directory into the SQLite store on a worker thread.

        Imports merge by conversation UUID and are never destructive: a conversation that
        already has content in the database is not overwritten by a content-less skeleton
        (tombstone) from a later export.
    */
    class ImportWorker : public QObject {
        Q_OBJECT

    public:
        explicit ImportWorker(const QString& database_path, QObject* parent = nullptr);

    public slots:
        //! Runs the import of \a export_dir (a directory containing conversations.json).
        void importExport(const QString& export_dir);

    signals:
        //! Emitted periodically while conversations are being written.
        void progressChanged(int done_conversations, int total_conversations);

        //! Emitted on success with the import statistics.
        void finished(int imported_conversations, int skipped_conversations, int imported_messages);

        //! Emitted when the import aborts; no partial data is committed.
        void failed(const QString& error_message);

    private:
        //! Joins the top-level text field and all text content blocks into one searchable string.
        [[nodiscard]] static QString flattenMessageText(const QJsonObject& message);

        //! True if any message in the conversation carries text or content blocks.
        [[nodiscard]] static bool conversationHasContent(const QJsonObject& conversation);

        QString m_database_path;
    };
}
