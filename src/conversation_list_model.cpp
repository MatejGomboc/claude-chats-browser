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

#include "conversation_list_model.hpp"
#include "icon_util.hpp"
#include <QBrush>
#include <QColor>
#include <QIcon>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtLogging>

namespace ChatsBrowser
{
    ConversationListModel::ConversationListModel(QObject* parent) :
        QAbstractListModel(parent)
    {
    }

    void ConversationListModel::refresh()
    {
        reload();
    }

    void ConversationListModel::setSearchFilter(const QString& filter)
    {
        m_filter = filter.trimmed();
        reload();
    }

    QString ConversationListModel::toFtsQuery(const QString& filter)
    {
        QString escaped = filter;
        escaped.replace('"', "\"\"");
        return QString("\"%1\"*").arg(escaped);
    }

    void ConversationListModel::reload()
    {
        beginResetModel();
        m_rows.clear();

        QSqlDatabase database = QSqlDatabase::database("main");
        QSqlQuery query(database);

        if (m_filter.isEmpty()) {
            query.prepare("SELECT uuid, name, created_at, updated_at, message_count, has_content"
                          " FROM conversations ORDER BY updated_at DESC");
        } else {
            QString like_pattern = m_filter;
            like_pattern.replace('\\', "\\\\");
            like_pattern.replace('%', "\\%");
            like_pattern.replace('_', "\\_");
            like_pattern = QString("%%%1%%").arg(like_pattern);

            query.prepare("SELECT uuid, name, created_at, updated_at, message_count, has_content"
                          " FROM conversations"
                          " WHERE (name LIKE ? ESCAPE '\\')"
                          "    OR (uuid IN (SELECT conversation_uuid FROM messages WHERE rowid IN"
                          "        (SELECT rowid FROM messages_fts WHERE messages_fts MATCH ?)))"
                          " ORDER BY updated_at DESC");
            query.addBindValue(like_pattern);
            query.addBindValue(toFtsQuery(m_filter));
        }

        if (!query.exec()) {
            qWarning() << "Conversation query failed:" << query.lastError().text();
            endResetModel();
            return;
        }

        while (query.next()) {
            Row row;
            row.uuid = query.value(0).toString();
            row.name = query.value(1).toString();
            row.created_at = query.value(2).toString();
            row.updated_at = query.value(3).toString();
            row.message_count = query.value(4).toInt();
            row.has_content = query.value(5).toBool();
            m_rows.append(row);
        }

        endResetModel();
    }

    int ConversationListModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) {
            return 0;
        }
        return static_cast<int>(m_rows.size());
    }

    QVariant ConversationListModel::data(const QModelIndex& index, int role) const
    {
        if ((!index.isValid()) || (index.row() < 0) || (index.row() >= static_cast<int>(m_rows.size()))) {
            return QVariant();
        }

        const Row& row = m_rows.at(index.row());

        switch (role) {
        case Qt::DisplayRole:
            if (!row.name.isEmpty()) {
                return row.name;
            }
            return row.has_content ? QString("(untitled)") : QString("(deleted conversation)");
        case Qt::DecorationRole: {
            // Cached once: a chat glyph in the default file-icon grey, dimmer for tombstones.
            static const QIcon content_icon = IconUtil::tinted(":/icons/chat.svg", QColor("#C5C5C5"));
            static const QIcon tombstone_icon = IconUtil::tinted(":/icons/chat.svg", QColor("#6A6A6A"));
            return row.has_content ? content_icon : tombstone_icon;
        }
        case Qt::ForegroundRole:
            if (!row.has_content) {
                return QBrush(QColor(0x85, 0x85, 0x85));
            }
            return QVariant();
        case Qt::ToolTipRole:
            return QString("Created %1\nUpdated %2\n%3 messages").arg(row.created_at, row.updated_at).arg(row.message_count);
        case UuidRole:
            return row.uuid;
        case HasContentRole:
            return row.has_content;
        case MessageCountRole:
            return row.message_count;
        case CreatedAtRole:
            return row.created_at;
        case UpdatedAtRole:
            return row.updated_at;
        default:
            return QVariant();
        }
    }
}
