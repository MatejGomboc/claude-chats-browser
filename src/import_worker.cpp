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

#include "import_worker.hpp"
#include "database.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace ChatsBrowser
{
    namespace
    {
        constexpr int PROGRESS_INTERVAL_CONVERSATIONS = 25;
    }

    ImportWorker::ImportWorker(const QString& database_path, QObject* parent) :
        QObject(parent),
        m_database_path(database_path)
    {
    }

    QString ImportWorker::flattenMessageText(const QJsonObject& message)
    {
        // Index the visible text only. Content blocks are authoritative when present; the
        // top-level "text" field (which concatenates blocks, thinking included) is used
        // only as a fallback for older messages that carry no content blocks.
        QJsonArray content_blocks = message.value("content").toArray();
        if (content_blocks.isEmpty()) {
            return message.value("text").toString();
        }

        QStringList parts;
        for (const QJsonValue& block_value : content_blocks) {
            QJsonObject block = block_value.toObject();
            if (block.value("type").toString() == "text") {
                QString block_text = block.value("text").toString();
                if (!block_text.isEmpty()) {
                    parts.append(block_text);
                }
            }
        }

        return parts.join("\n\n");
    }

    bool ImportWorker::conversationHasContent(const QJsonObject& conversation)
    {
        QJsonArray messages = conversation.value("chat_messages").toArray();
        for (const QJsonValue& message_value : messages) {
            QJsonObject message = message_value.toObject();
            if (!message.value("text").toString().isEmpty()) {
                return true;
            }
            if (!message.value("content").toArray().isEmpty()) {
                return true;
            }
        }
        return false;
    }

    void ImportWorker::importExport(const QString& export_dir)
    {
        QString conversations_path = export_dir + "/conversations.json";
        QFile conversations_file(conversations_path);
        if (!conversations_file.exists()) {
            emit failed(QString("No conversations.json found in %1").arg(export_dir));
            return;
        }
        if (!conversations_file.open(QIODevice::ReadOnly)) {
            emit failed(QString("Cannot read %1: %2").arg(conversations_path, conversations_file.errorString()));
            return;
        }

        // The export is one big JSON array; parsing it whole is a one-off cost per import,
        // paid on this worker thread. Replace with a streaming parser only if real exports
        // outgrow available memory.
        QByteArray raw_json = conversations_file.readAll();
        conversations_file.close();

        QJsonParseError parse_error{};
        QJsonDocument document = QJsonDocument::fromJson(raw_json, &parse_error);
        raw_json.clear();
        if (document.isNull()) {
            emit failed(QString("Invalid JSON in %1: %2").arg(conversations_path, parse_error.errorString()));
            return;
        }
        if (!document.isArray()) {
            emit failed(QString("Unexpected format in %1: top-level value is not an array").arg(conversations_path));
            return;
        }
        QJsonArray conversations = document.array();

        QString open_error;
        QSqlDatabase database = Database::open("import", m_database_path, &open_error);
        if (!database.isValid()) {
            emit failed(open_error);
            return;
        }

        if (!database.transaction()) {
            emit failed(QString("Cannot start import transaction: %1").arg(database.lastError().text()));
            return;
        }

        QSqlQuery existing_query(database);
        existing_query.prepare("SELECT has_content FROM conversations WHERE uuid = ?");

        QSqlQuery conversation_query(database);
        conversation_query.prepare("INSERT OR REPLACE INTO conversations (uuid, name, summary, created_at, updated_at, message_count, has_content)"
                                   " VALUES (?, ?, ?, ?, ?, ?, ?)");

        QSqlQuery delete_messages_query(database);
        delete_messages_query.prepare("DELETE FROM messages WHERE conversation_uuid = ?");

        QSqlQuery message_query(database);
        message_query.prepare("INSERT OR REPLACE INTO messages (uuid, conversation_uuid, parent_uuid, sender, created_at, text, raw_json)"
                              " VALUES (?, ?, ?, ?, ?, ?, ?)");

        int total_conversations = static_cast<int>(conversations.size());
        int done_conversations = 0;
        int imported_conversations = 0;
        int skipped_conversations = 0;
        int imported_messages = 0;

        for (const QJsonValue& conversation_value : conversations) {
            QJsonObject conversation = conversation_value.toObject();
            done_conversations++;

            QString conversation_uuid = conversation.value("uuid").toString();
            if (conversation_uuid.isEmpty()) {
                skipped_conversations++;
                continue;
            }

            bool has_content = conversationHasContent(conversation);
            if (!has_content) {
                // Merge rule: never let a tombstone overwrite a conversation we already
                // have real content for.
                existing_query.addBindValue(conversation_uuid);
                if (!existing_query.exec()) {
                    database.rollback();
                    emit failed(QString("Lookup failed for %1: %2").arg(conversation_uuid, existing_query.lastError().text()));
                    return;
                }
                bool existing_has_content = existing_query.next() && existing_query.value(0).toBool();
                existing_query.finish();
                if (existing_has_content) {
                    skipped_conversations++;
                    continue;
                }
            }

            QJsonArray messages = conversation.value("chat_messages").toArray();

            conversation_query.addBindValue(conversation_uuid);
            conversation_query.addBindValue(conversation.value("name").toString());
            conversation_query.addBindValue(conversation.value("summary").toString());
            conversation_query.addBindValue(conversation.value("created_at").toString());
            conversation_query.addBindValue(conversation.value("updated_at").toString());
            conversation_query.addBindValue(static_cast<int>(messages.size()));
            conversation_query.addBindValue(has_content ? 1 : 0);
            if (!conversation_query.exec()) {
                database.rollback();
                emit failed(QString("Insert failed for conversation %1: %2").arg(conversation_uuid, conversation_query.lastError().text()));
                return;
            }

            delete_messages_query.addBindValue(conversation_uuid);
            if (!delete_messages_query.exec()) {
                database.rollback();
                emit failed(QString("Cleanup failed for conversation %1: %2").arg(conversation_uuid, delete_messages_query.lastError().text()));
                return;
            }

            for (const QJsonValue& message_value : messages) {
                QJsonObject message = message_value.toObject();
                QString message_uuid = message.value("uuid").toString();
                if (message_uuid.isEmpty()) {
                    continue;
                }

                message_query.addBindValue(message_uuid);
                message_query.addBindValue(conversation_uuid);
                message_query.addBindValue(message.value("parent_message_uuid").toString());
                message_query.addBindValue(message.value("sender").toString());
                message_query.addBindValue(message.value("created_at").toString());
                message_query.addBindValue(flattenMessageText(message));
                message_query.addBindValue(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
                if (!message_query.exec()) {
                    database.rollback();
                    emit failed(QString("Insert failed for message %1: %2").arg(message_uuid, message_query.lastError().text()));
                    return;
                }
                imported_messages++;
            }

            imported_conversations++;
            if ((done_conversations % PROGRESS_INTERVAL_CONVERSATIONS) == 0) {
                emit progressChanged(done_conversations, total_conversations);
            }
        }

        if (!database.commit()) {
            emit failed(QString("Cannot commit import: %1").arg(database.lastError().text()));
            return;
        }

        emit progressChanged(total_conversations, total_conversations);
        emit finished(imported_conversations, skipped_conversations, imported_messages);
    }
}
