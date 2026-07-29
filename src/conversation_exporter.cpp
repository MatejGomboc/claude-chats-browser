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

#include "conversation_exporter.hpp"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>

namespace ChatsBrowser
{
    namespace
    {
        [[nodiscard]] QString senderLabel(const QString& sender)
        {
            if (sender == "human") {
                return "You";
            }
            if (sender == "assistant") {
                return "Claude";
            }
            return sender.isEmpty() ? QString("Unknown") : sender;
        }

        //! "2026-07-18T15:08:39.970934Z" → "2026-07-18 15:08" local time (empty if invalid).
        [[nodiscard]] QString formatTimestamp(const QString& iso_timestamp)
        {
            const QDateTime moment = QDateTime::fromString(iso_timestamp, Qt::ISODateWithMs);
            return moment.isValid() ? moment.toLocalTime().toString("yyyy-MM-dd HH:mm") : QString();
        }
    }

    QString ConversationExporter::visibleText(const QJsonObject& message)
    {
        const QJsonArray content_blocks = message.value("content").toArray();

        // Content blocks are authoritative when present; the top-level "text" field
        // is the legacy fallback for old messages (it concatenates thinking too, so
        // it is only used when there are no blocks at all).
        if (content_blocks.isEmpty()) {
            return message.value("text").toString();
        }

        QString text;
        for (const QJsonValue& block_value : content_blocks) {
            const QJsonObject block = block_value.toObject();
            if (block.value("type").toString() == "text") {
                const QString block_text = block.value("text").toString();
                if (!block_text.isEmpty()) {
                    if (!text.isEmpty()) {
                        text += "\n\n";
                    }
                    text += block_text;
                }
            }
        }
        return text;
    }

    QString ConversationExporter::toMarkdown(const ConversationInfo& info, const QList<QJsonObject>& path_messages)
    {
        QString out;
        out += QString("# %1\n\n").arg(info.name.isEmpty() ? QString("Untitled conversation") : info.name);

        const QString created = formatTimestamp(info.created_at);
        const QString updated = formatTimestamp(info.updated_at);
        out += QString("*Exported from Claude Chats Browser — conversation of %1, last updated %2.*\n")
                   .arg(created.isEmpty() ? QString("unknown date") : created, updated.isEmpty() ? QString("unknown date") : updated);

        for (const QJsonObject& message : path_messages) {
            const QString text = visibleText(message);
            QStringList attachment_names;
            for (const QJsonValue& value : message.value("attachments").toArray()) {
                const QString name = value.toObject().value("file_name").toString();
                if (!name.isEmpty()) {
                    attachment_names.append(name);
                }
            }
            if (text.trimmed().isEmpty() && attachment_names.isEmpty()) {
                continue; // tool/thinking-only message: nothing shareable
            }

            out += "\n---\n\n";
            const QString timestamp = formatTimestamp(message.value("created_at").toString());
            out += QString("## %1%2\n\n").arg(senderLabel(message.value("sender").toString()), timestamp.isEmpty() ? QString() : QString(" — %1").arg(timestamp));

            for (const QString& name : attachment_names) {
                out += QString("> 📎 Attachment: %1\n").arg(name);
            }
            if (!attachment_names.isEmpty()) {
                out += "\n";
            }

            if (!text.trimmed().isEmpty()) {
                out += text.trimmed();
                out += "\n";
            }
        }

        return out;
    }

    QJsonObject ConversationExporter::toExportJson(const ConversationInfo& info, const QList<QJsonObject>& all_messages)
    {
        QJsonArray chat_messages;
        for (const QJsonObject& message : all_messages) {
            chat_messages.append(message);
        }

        // The same envelope shape as claude.ai's conversations.json entries, so the
        // file round-trips through this app's own (schema-tolerant) importer.
        QJsonObject conversation;
        conversation.insert("uuid", info.uuid);
        conversation.insert("name", info.name);
        conversation.insert("summary", info.summary);
        conversation.insert("created_at", info.created_at);
        conversation.insert("updated_at", info.updated_at);
        conversation.insert("chat_messages", chat_messages);
        return conversation;
    }

    QString ConversationExporter::suggestedBaseName(const ConversationInfo& info)
    {
        QString base = info.name.isEmpty() ? info.uuid : info.name;
        base.replace(QRegularExpression("[^A-Za-z0-9._ -]"), "_");
        base = base.simplified().left(80);
        return base.isEmpty() ? QString("conversation") : base;
    }
}
