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

#include "stats_collector.hpp"
#include <QDialog>
#include <QFutureWatcher>
#include <QString>

QT_BEGIN_NAMESPACE
class QLabel;
class QVBoxLayout;
QT_END_NAMESPACE

namespace ChatsBrowser
{
    class BarChart;

    /*!
        A non-modal statistics window for the imported archive: headline
        numbers, a messages-per-month activity chart, and tool usage.

        The archive walk (parsing every message's JSON) runs on a worker thread
        with its own database connection — the dialog shows a busy placeholder
        until the numbers arrive. Model usage is deliberately absent: the
        offline export carries no model field.
    */
    class StatsPanel : public QDialog {
        Q_OBJECT

    public:
        explicit StatsPanel(QWidget* parent = nullptr);

    private:
        //! Builds the widgets once the worker delivers the numbers.
        void populate(const ArchiveStats& stats);

        //! Adds one headline tile (value + caption) to the tiles row.
        static QWidget* makeTile(const QString& value, const QString& caption, QWidget* parent);

        QVBoxLayout* m_layout{nullptr};
        QLabel* m_busy{nullptr};
        QFutureWatcher<ArchiveStats> m_watcher;
    };
}
