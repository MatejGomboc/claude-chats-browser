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

#include "stats_collector.hpp"
#include <QJsonArray>
#include <QJsonValue>

namespace ChatsBrowser
{
    void StatsCollector::accumulate(const QJsonObject& message, ArchiveStats& stats)
    {
        stats.messages++;

        const QString sender = message.value("sender").toString();
        if (sender == "human") {
            stats.human_messages++;
        } else if (sender == "assistant") {
            stats.assistant_messages++;
        }

        const QString created_at = message.value("created_at").toString();
        if (created_at.size() >= 7) {
            stats.messages_per_month[created_at.left(7)]++;
            if (stats.first_message_at.isEmpty() || created_at < stats.first_message_at) {
                stats.first_message_at = created_at;
            }
            if (created_at > stats.last_message_at) {
                stats.last_message_at = created_at;
            }
        }

        for (const QJsonValue& block_value : message.value("content").toArray()) {
            const QJsonObject block = block_value.toObject();
            const QString type = block.value("type").toString();
            if (type == "thinking") {
                stats.thinking_blocks++;
            } else if (type == "tool_use") {
                stats.tool_calls++;
                const QString name = block.value("name").toString();
                stats.tool_usage[name.isEmpty() ? QString("(unnamed)") : name]++;
                if (name == "artifacts") {
                    stats.artifact_ops++;
                }
            }
        }

        for (const QJsonValue& attachment : message.value("attachments").toArray()) {
            Q_UNUSED(attachment);
            stats.attachments++;
        }
    }
}
