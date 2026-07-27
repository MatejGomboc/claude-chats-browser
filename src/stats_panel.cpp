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

#include "stats_panel.hpp"
#include "bar_chart.hpp"
#include <QDate>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QUuid>
#include <QVariant>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace ChatsBrowser
{
    namespace
    {
        //! Walks the whole archive on a private connection (runs on a worker thread).
        ArchiveStats collectStats(const QString& database_path)
        {
            ArchiveStats stats;

            // A unique, short-lived connection: worker threads must never touch
            // the UI thread's "main" connection (same pattern as the search).
            const QString connection_name = QStringLiteral("stats-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
            {
                QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connection_name);
                database.setDatabaseName(database_path);
                if (database.open()) {
                    QSqlQuery totals(database);
                    if (totals.exec("SELECT COUNT(*), SUM(has_content) FROM conversations") && totals.next()) {
                        stats.conversations = totals.value(0).toInt();
                        stats.readable_conversations = totals.value(1).toInt();
                    }

                    // parent uuid → child count, per conversation, for fork counting.
                    QHash<QString, int> children;

                    QSqlQuery query(database);
                    query.setForwardOnly(true);
                    if (query.exec("SELECT conversation_uuid, parent_uuid, raw_json FROM messages")) {
                        while (query.next()) {
                            const QString parent_key = query.value(0).toString() + "/" + query.value(1).toString();
                            children[parent_key]++;

                            const QJsonDocument document = QJsonDocument::fromJson(query.value(2).toString().toUtf8());
                            if (document.isObject()) {
                                StatsCollector::accumulate(document.object(), stats);
                            }
                        }
                    }

                    for (auto it = children.constBegin(); it != children.constEnd(); ++it) {
                        if (it.value() > 1) {
                            stats.forks++;
                        }
                    }
                }
            }
            QSqlDatabase::removeDatabase(connection_name);
            return stats;
        }

        //! "yyyy-MM" months from first to last inclusive, zero-filling gaps.
        QList<BarChart::Bar> monthlyBars(const ArchiveStats& stats)
        {
            QList<BarChart::Bar> bars;
            if (stats.messages_per_month.isEmpty()) {
                return bars;
            }
            QDate month = QDate::fromString(stats.messages_per_month.firstKey() + "-01", Qt::ISODate);
            const QDate last = QDate::fromString(stats.messages_per_month.lastKey() + "-01", Qt::ISODate);
            while (month.isValid() && (month <= last)) {
                const QString key = month.toString("yyyy-MM");
                bars.append({key, stats.messages_per_month.value(key, 0)});
                month = month.addMonths(1);
            }
            return bars;
        }
    }

    StatsPanel::StatsPanel(QWidget* parent) :
        QDialog(parent)
    {
        setWindowTitle("Statistics");
        setObjectName("statsPanel");
        resize(860, 640);
        setAttribute(Qt::WA_DeleteOnClose);

        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(16, 16, 16, 16);
        m_layout->setSpacing(10);

        m_busy = new QLabel("Crunching the archive…", this);
        m_busy->setObjectName("statsBusy");
        m_busy->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(m_busy, 1);

        connect(&m_watcher, &QFutureWatcher<ArchiveStats>::finished, this, [this]() {
            populate(m_watcher.result());
        });
        m_watcher.setFuture(QtConcurrent::run(collectStats, QSqlDatabase::database("main").databaseName()));
    }

    QWidget* StatsPanel::makeTile(const QString& value, const QString& caption, QWidget* parent)
    {
        QWidget* tile = new QWidget(parent);
        tile->setObjectName("statTile");
        QVBoxLayout* tile_layout = new QVBoxLayout(tile);
        tile_layout->setContentsMargins(12, 8, 12, 8);
        tile_layout->setSpacing(2);

        QLabel* value_label = new QLabel(value, tile);
        value_label->setObjectName("statValue");
        QLabel* caption_label = new QLabel(caption, tile);
        caption_label->setObjectName("statCaption");

        tile_layout->addWidget(value_label);
        tile_layout->addWidget(caption_label);
        return tile;
    }

    void StatsPanel::populate(const ArchiveStats& stats)
    {
        delete m_busy;
        m_busy = nullptr;

        const QLocale locale;
        const auto number = [&locale](int value) {
            return locale.toString(value);
        };

        // Headline tiles, two rows of four.
        QGridLayout* tiles = new QGridLayout();
        tiles->setSpacing(8);
        const QString span = (stats.first_message_at.isEmpty())
            ? QString("—")
            : QString("%1 → %2").arg(stats.first_message_at.left(10), stats.last_message_at.left(10));
        const QList<QPair<QString, QString>> tile_data = {
            {QString("%1 / %2").arg(number(stats.readable_conversations), number(stats.conversations)), "readable / total conversations"},
            {number(stats.messages), "messages"},
            {QString("%1 · %2").arg(number(stats.human_messages), number(stats.assistant_messages)), "you · Claude"},
            {span, "first → last message"},
            {number(stats.thinking_blocks), "thinking blocks"},
            {number(stats.tool_calls), "tool calls"},
            {number(stats.artifact_ops), "artifact operations"},
            {number(stats.forks), "branch points"},
        };
        for (int i = 0; i < tile_data.size(); ++i) {
            tiles->addWidget(makeTile(tile_data.at(i).first, tile_data.at(i).second, this), i / 4, i % 4);
        }
        m_layout->addLayout(tiles);

        // Activity over time.
        QLabel* activity_header = new QLabel("Messages per month", this);
        activity_header->setObjectName("statsHeader");
        m_layout->addWidget(activity_header);

        BarChart* chart = new BarChart(this);
        chart->setBars(monthlyBars(stats));
        m_layout->addWidget(chart, 2);

        // Tool usage, ranked.
        QLabel* tools_header = new QLabel("Tool usage", this);
        tools_header->setObjectName("statsHeader");
        m_layout->addWidget(tools_header);

        QList<QPair<QString, int>> tools;
        for (auto it = stats.tool_usage.constBegin(); it != stats.tool_usage.constEnd(); ++it) {
            tools.append({it.key(), it.value()});
        }
        std::sort(tools.begin(), tools.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        QTableWidget* table = new QTableWidget(static_cast<int>(tools.size()), 2, this);
        table->setObjectName("statsTable");
        table->setHorizontalHeaderLabels({"Tool", "Calls"});
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->verticalHeader()->hide();
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        for (int row = 0; row < tools.size(); ++row) {
            table->setItem(row, 0, new QTableWidgetItem(tools.at(row).first));
            QTableWidgetItem* count = new QTableWidgetItem(number(tools.at(row).second));
            count->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(row, 1, count);
        }
        m_layout->addWidget(table, 3);
    }
}
