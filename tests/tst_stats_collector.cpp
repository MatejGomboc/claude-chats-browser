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
#include <QJsonObject>
#include <QtTest>

using ChatsBrowser::ArchiveStats;
using ChatsBrowser::StatsCollector;

namespace
{
    QJsonObject message(const QString& sender, const QString& created_at, const QJsonArray& content = {}, const QJsonArray& attachments = {})
    {
        QJsonObject m;
        m.insert("sender", sender);
        m.insert("created_at", created_at);
        m.insert("content", content);
        m.insert("attachments", attachments);
        return m;
    }
}

class TestStatsCollector : public QObject {
    Q_OBJECT

private slots:
    //! Senders, months and the first/last span accumulate correctly.
    void countsSendersAndMonths()
    {
        ArchiveStats stats;
        StatsCollector::accumulate(message("human", "2025-03-01T10:00:00Z"), stats);
        StatsCollector::accumulate(message("assistant", "2025-03-02T10:00:00Z"), stats);
        StatsCollector::accumulate(message("assistant", "2025-05-01T10:00:00Z"), stats);

        QCOMPARE(stats.messages, 3);
        QCOMPARE(stats.human_messages, 1);
        QCOMPARE(stats.assistant_messages, 2);
        QCOMPARE(stats.messages_per_month.value("2025-03"), 2);
        QCOMPARE(stats.messages_per_month.value("2025-05"), 1);
        QCOMPARE(stats.first_message_at, QString("2025-03-01T10:00:00Z"));
        QCOMPARE(stats.last_message_at, QString("2025-05-01T10:00:00Z"));
    }

    //! Thinking and tool_use blocks are tallied; artifacts ops counted separately.
    void countsContentBlocks()
    {
        const QJsonArray content = {
            QJsonObject{{"type", "thinking"}, {"thinking", "hmm"}},
            QJsonObject{{"type", "tool_use"}, {"name", "artifacts"}},
            QJsonObject{{"type", "tool_use"}, {"name", "web_search"}},
            QJsonObject{{"type", "tool_use"}, {"name", "web_search"}},
            QJsonObject{{"type", "text"}, {"text", "hello"}},
        };

        ArchiveStats stats;
        StatsCollector::accumulate(message("assistant", "2025-01-01T00:00:00Z", content), stats);

        QCOMPARE(stats.thinking_blocks, 1);
        QCOMPARE(stats.tool_calls, 3);
        QCOMPARE(stats.artifact_ops, 1);
        QCOMPARE(stats.tool_usage.value("web_search"), 2);
        QCOMPARE(stats.tool_usage.value("artifacts"), 1);
    }

    //! Attachments count; a message with no usable timestamp joins no month.
    void countsAttachmentsAndSkipsBadTimestamps()
    {
        const QJsonArray attachments = {QJsonObject{{"file_name", "a.txt"}}, QJsonObject{{"file_name", "b.txt"}}};

        ArchiveStats stats;
        StatsCollector::accumulate(message("human", "", {}, attachments), stats);

        QCOMPARE(stats.attachments, 2);
        QCOMPARE(stats.messages, 1);
        QVERIFY(stats.messages_per_month.isEmpty());
        QVERIFY(stats.first_message_at.isEmpty());
    }

    //! Unnamed tools are grouped rather than dropped.
    void unnamedToolsAreGrouped()
    {
        const QJsonArray content = {QJsonObject{{"type", "tool_use"}}};

        ArchiveStats stats;
        StatsCollector::accumulate(message("assistant", "2025-01-01T00:00:00Z", content), stats);

        QCOMPARE(stats.tool_usage.value("(unnamed)"), 1);
    }
};

QTEST_MAIN(TestStatsCollector)
#include "tst_stats_collector.moc"
