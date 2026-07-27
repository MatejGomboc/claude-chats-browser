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
#include <QMap>
#include <QString>

namespace ChatsBrowser
{
    //! Aggregated statistics over an imported archive.
    struct ArchiveStats {
        int conversations{0}; //!< All conversations, tombstones included.
        int readable_conversations{0}; //!< Conversations with content.
        int messages{0}; //!< All messages.
        int human_messages{0};
        int assistant_messages{0};
        int thinking_blocks{0};
        int tool_calls{0}; //!< tool_use blocks across all messages.
        int artifact_ops{0}; //!< tool_use blocks of the artifacts tool.
        int attachments{0}; //!< Pasted-text attachments carried by messages.
        int forks{0}; //!< Messages that have more than one child (edit/retry points).
        QString first_message_at; //!< ISO timestamp of the earliest message (empty if none).
        QString last_message_at; //!< ISO timestamp of the latest message (empty if none).
        QMap<QString, int> messages_per_month; //!< "yyyy-MM" → message count.
        QMap<QString, int> tool_usage; //!< tool name → tool_use count.
    };

    /*!
        Pure accumulation of archive statistics from raw message JSON.

        The database walk lives in the caller (the stats panel runs it off the UI
        thread); this class only turns messages into numbers, so the aggregation
        logic is unit-testable without a database.
    */
    class StatsCollector {
    public:
        //! Folds one message (its raw export JSON) into the running totals.
        //! Fork counting is the caller's job: it needs the parent→children map.
        static void accumulate(const QJsonObject& message, ArchiveStats& stats);
    };
}
