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

#include "conversation_reader.hpp"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <QtLogging>

namespace ChatsBrowser
{
    namespace
    {
        const QString PLACEHOLDER_MARKDOWN = "*Select a conversation to read it here.*";
    }

    ConversationReader::ConversationReader(QWidget* parent) :
        QTextBrowser(parent)
    {
        setOpenExternalLinks(true);
        setMarkdown(PLACEHOLDER_MARKDOWN);
    }

    QString ConversationReader::senderLabel(const QString& sender)
    {
        if (sender == "human") {
            return "You";
        }
        if (sender == "assistant") {
            return "Claude";
        }
        return sender.isEmpty() ? QString("Unknown") : sender;
    }

    QString ConversationReader::buildMarkdown(const QString& conversation_uuid, bool* found_any_content)
    {
        *found_any_content = false;

        QSqlDatabase database = QSqlDatabase::database("main");
        QSqlQuery query(database);
        query.prepare("SELECT sender, created_at, text FROM messages"
                      " WHERE conversation_uuid = ? ORDER BY created_at, rowid");
        query.addBindValue(conversation_uuid);
        if (!query.exec()) {
            qWarning() << "Message query failed:" << query.lastError().text();
            return QString("*Could not load this conversation.*");
        }

        QStringList blocks;
        while (query.next()) {
            QString sender = query.value(0).toString();
            QString created_at = query.value(1).toString();
            QString text = query.value(2).toString();

            if (text.trimmed().isEmpty()) {
                continue;
            }
            *found_any_content = true;

            QString header = QString("**%1**").arg(senderLabel(sender));
            if (!created_at.isEmpty()) {
                header += QString("  ·  *%1*").arg(created_at);
            }
            blocks.append(header + "\n\n" + text);
        }

        // Turns are separated by a horizontal rule; each message's own markdown
        // (code fences, lists, emphasis) is preserved for QTextBrowser to render.
        return blocks.join("\n\n---\n\n");
    }

    void ConversationReader::showConversation(const QString& conversation_uuid)
    {
        m_conversation_uuid = conversation_uuid;

        if (conversation_uuid.isEmpty()) {
            clearConversation();
            return;
        }

        bool found_any_content = false;
        QString markdown = buildMarkdown(conversation_uuid, &found_any_content);
        if (!found_any_content) {
            setMarkdown("*This conversation was deleted; its content is no longer available.*");
            return;
        }

        setMarkdown(markdown);
        moveCursor(QTextCursor::Start);
    }

    void ConversationReader::clearConversation()
    {
        m_conversation_uuid.clear();
        setMarkdown(PLACEHOLDER_MARKDOWN);
    }
}
