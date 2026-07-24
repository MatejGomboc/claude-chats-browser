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
    //! One sidebar row. Built on a worker thread, so it is a plain value type.
    struct ConversationRow {
        QString uuid;
        QString name;
        QString created_at;
        QString updated_at;
        int message_count{0};
        bool has_content{false};
    };

    /*!
        List of conversations from the SQLite store, newest first.

        An optional search filter matches conversation names and, via the FTS5 index,
        full message text. The query runs on a worker thread — a broad prefix like "th"
        matches almost everything and can take over a second, which would otherwise freeze
        the UI on every keystroke. Tombstones are included and rendered greyed-out.
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

    signals:
        //! Emitted when a (possibly slow) query is dispatched to the worker thread.
        void searchStarted();

        //! Emitted when the latest query's results have been applied.
        void searchFinished();

    private:
        //! Dispatches the query for the current filter to a worker thread.
        void reload();

        QList<ConversationRow> m_rows;
        QString m_filter;
        quint64 m_query_generation{0}; //!< Bumped per query; stale results are discarded.
    };
}
