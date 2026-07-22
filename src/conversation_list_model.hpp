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

#include <QAbstractListModel>
#include <QList>
#include <QString>

namespace ChatsBrowser
{
    /*!
        List of conversations from the SQLite store, newest first.

        An optional search filter matches conversation names and, via the FTS5 index,
        full message text. Tombstones (deleted conversations without content) are
        included and rendered greyed-out by the view.
    */
    class ConversationListModel : public QAbstractListModel {
        Q_OBJECT

    public:
        enum Roles {
            UuidRole = Qt::UserRole + 1,
            HasContentRole,
            MessageCountRole,
            CreatedAtRole,
            UpdatedAtRole,
        };

        explicit ConversationListModel(QObject* parent = nullptr);

        //! Reloads all rows from the database, keeping the current search filter.
        void refresh();

        //! Sets the search filter (empty shows everything) and reloads.
        void setSearchFilter(const QString& filter);

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

    private:
        struct Row {
            QString uuid;
            QString name;
            QString created_at;
            QString updated_at;
            int message_count{0};
            bool has_content{false};
        };

        void reload();

        //! Turns free-typed search text into an FTS5 phrase-prefix query ("term term"*).
        [[nodiscard]] static QString toFtsQuery(const QString& filter);

        QList<Row> m_rows;
        QString m_filter;
    };
}
