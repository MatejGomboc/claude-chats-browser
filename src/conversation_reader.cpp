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
#include "message_widget.hpp"
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QtLogging>

namespace ChatsBrowser
{
    namespace
    {
        //! First chunk: just enough to fill the viewport as fast as possible.
        constexpr int FIRST_CHUNK_MESSAGES = 8;

        //! Messages built per subsequent event-loop step; keeps the UI responsive.
        constexpr int RENDER_CHUNK_MESSAGES = 32;

        //! Minimum ms between immediate height recomputations during resize storms.
        constexpr int HEIGHT_THROTTLE_MS = 50;
    }

    ConversationReader::ConversationReader(QWidget* parent) :
        QScrollArea(parent)
    {
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);

        m_height_timer = new QTimer(this);
        m_height_timer->setSingleShot(true);
        m_height_timer->setInterval(HEIGHT_THROTTLE_MS + 10);
        connect(m_height_timer, &QTimer::timeout, this, &ConversationReader::updateContentHeight);

        m_container = new QWidget(this);
        m_container->setObjectName("readerContainer");

        m_layout = new QVBoxLayout(m_container);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(0);

        m_placeholder = new QLabel(m_container);
        m_placeholder->setObjectName("readerPlaceholder");
        m_placeholder->setAlignment(Qt::AlignCenter);
        m_placeholder->setWordWrap(true);
        m_layout->addWidget(m_placeholder);

        m_layout->addStretch();

        setWidget(m_container);
        showPlaceholder("Select a conversation to read it here.");
    }

    void ConversationReader::clearMessages()
    {
        // Cancel any chunked render still streaming in, and let listeners (the progress
        // bar) know it is over.
        m_render_generation++;
        m_pending_nodes.clear();
        m_pending_index = 0;

        // These message widgets are about to be deleted; drop the find's references to them
        // so no stale pointer survives. The find bar (in the main window) re-runs after the
        // next render finishes if it is still open.
        m_find_matches.clear();
        m_find_current = -1;

        emit renderFinished();

        // Remove every item except the trailing stretch (always the last item).
        while (m_layout->count() > 1) {
            QLayoutItem* item = m_layout->takeAt(0);
            if (item->widget() != nullptr) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        m_placeholder = nullptr;
    }

    void ConversationReader::showPlaceholder(const QString& text)
    {
        clearMessages();
        m_placeholder = new QLabel(m_container);
        m_placeholder->setObjectName("readerPlaceholder");
        m_placeholder->setAlignment(Qt::AlignCenter);
        m_placeholder->setWordWrap(true);
        m_placeholder->setText(text);
        m_layout->insertWidget(0, m_placeholder);
    }

    void ConversationReader::showConversation(const QString& conversation_uuid)
    {
        m_conversation_uuid = conversation_uuid;

        if (conversation_uuid.isEmpty()) {
            clearConversation();
            return;
        }

        buildTree(conversation_uuid);
        if (m_raw_messages.isEmpty()) {
            showPlaceholder("Could not load this conversation.");
            return;
        }

        renderPath(true);
    }

    void ConversationReader::buildTree(const QString& conversation_uuid)
    {
        m_raw_messages.clear();
        m_tree.clear();

        QSqlQuery query(QSqlDatabase::database("main"));
        query.prepare("SELECT uuid, parent_uuid, raw_json FROM messages WHERE conversation_uuid = ? ORDER BY created_at, rowid");
        query.addBindValue(conversation_uuid);
        if (!query.exec()) {
            qWarning() << "Message query failed:" << query.lastError().text();
            return;
        }

        QList<QPair<QString, QString>> ordered;
        while (query.next()) {
            QString uuid = query.value(0).toString();
            if (uuid.isEmpty()) {
                continue;
            }
            ordered.append({uuid, query.value(1).toString()});
            m_raw_messages.insert(uuid, query.value(2).toString().toUtf8());
        }

        m_tree.build(ordered);
    }

    void ConversationReader::renderPath(bool reset_scroll)
    {
        clearMessages(); // bumps the render generation, cancelling any streaming render

        m_pending_nodes = m_tree.currentPath();
        m_pending_index = 0;
        m_any_content = false;

        appendPendingChunk(m_render_generation);
        if (reset_scroll) {
            verticalScrollBar()->setValue(0);
        }
    }

    void ConversationReader::appendPendingChunk(int generation)
    {
        if (generation != m_render_generation) {
            return; // a newer render superseded this one
        }

        const int total = static_cast<int>(m_pending_nodes.size());
        const int chunk_size = (m_pending_index == 0) ? FIRST_CHUNK_MESSAGES : RENDER_CHUNK_MESSAGES;
        const int chunk_end = qMin(m_pending_index + chunk_size, total);

        m_container->setUpdatesEnabled(false);
        while (m_pending_index < chunk_end) {
            const PathNode node = m_pending_nodes.at(m_pending_index);
            m_pending_index++;

            QJsonDocument document = QJsonDocument::fromJson(m_raw_messages.value(node.uuid));
            if (!document.isObject()) {
                continue;
            }

            MessageWidget* widget = new MessageWidget(document.object(), node.branch_index, node.branch_count, m_container);
            if (!widget->hasRenderedContent()) {
                widget->deleteLater();
                continue;
            }

            if (node.branch_count > 1) {
                const QString fork_key = node.fork_key;
                const int branch_index = node.branch_index;
                connect(widget, &MessageWidget::branchPrevRequested, this, [this, fork_key, branch_index]() {
                    m_tree.selectBranch(fork_key, branch_index - 1);
                    renderPath(false);
                });
                connect(widget, &MessageWidget::branchNextRequested, this, [this, fork_key, branch_index]() {
                    m_tree.selectBranch(fork_key, branch_index + 1);
                    renderPath(false);
                });
            }

            if (m_any_content) {
                QFrame* divider = new QFrame(m_container);
                divider->setObjectName("messageDivider");
                divider->setFrameShape(QFrame::HLine);
                m_layout->insertWidget(m_layout->count() - 1, divider);
            }

            m_layout->insertWidget(m_layout->count() - 1, widget);
            m_any_content = true;
        }
        m_container->setUpdatesEnabled(true);

        if (m_pending_index < total) {
            // Recomputing heightForWidth over every accumulated label is O(n); doing it per
            // chunk would be O(n^2). Throttle it while streaming — an approximate height is
            // fine mid-load; the final pass below settles it exactly.
            scheduleHeightUpdate();
            emit renderProgressChanged(m_pending_index, total);
            // Zero-delay single shot: yields to the event loop between chunks so input,
            // painting and scrolling stay live while the rest of the conversation loads.
            const int current_generation = generation;
            QTimer::singleShot(0, this, [this, current_generation]() {
                appendPendingChunk(current_generation);
            });
            return;
        }

        updateContentHeight();
        emit renderFinished();
        if (!m_any_content) {
            showPlaceholder("This conversation was deleted; its content is no longer available.");
        }
    }

    void ConversationReader::clearConversation()
    {
        m_conversation_uuid.clear();
        showPlaceholder("Select a conversation to read it here.");
    }

    QString ConversationReader::conversationUuid() const
    {
        return m_conversation_uuid;
    }

    QList<QJsonObject> ConversationReader::currentPathMessages() const
    {
        QList<QJsonObject> messages;
        for (const PathNode& node : m_tree.currentPath()) {
            const QJsonDocument document = QJsonDocument::fromJson(m_raw_messages.value(node.uuid));
            if (document.isObject()) {
                messages.append(document.object());
            }
        }
        return messages;
    }

    int ConversationReader::findText(const QString& term)
    {
        // Drop the previous run's highlights before recomputing.
        for (MessageWidget* widget : m_find_matches) {
            widget->setSearchHighlight(MessageWidget::SearchHighlight::None);
        }
        m_find_matches.clear();
        m_find_current = -1;
        m_find_term = term;

        if (term.isEmpty()) {
            emit findResultsChanged(0, 0);
            return 0;
        }

        // Collect matching message widgets in display order (skip dividers and placeholder).
        for (int i = 0; i < m_layout->count(); ++i) {
            MessageWidget* message = qobject_cast<MessageWidget*>(m_layout->itemAt(i)->widget());
            if (message == nullptr) {
                continue;
            }
            if (message->searchableText().contains(term, Qt::CaseInsensitive)) {
                message->setSearchHighlight(MessageWidget::SearchHighlight::Match);
                m_find_matches.append(message);
            }
        }

        if (!m_find_matches.isEmpty()) {
            m_find_current = 0;
            focusCurrentMatch();
        }
        emit findResultsChanged(m_find_current + 1, static_cast<int>(m_find_matches.size()));
        return static_cast<int>(m_find_matches.size());
    }

    void ConversationReader::findNext()
    {
        if (m_find_matches.isEmpty()) {
            return;
        }
        m_find_current = (m_find_current + 1) % static_cast<int>(m_find_matches.size());
        focusCurrentMatch();
        emit findResultsChanged(m_find_current + 1, static_cast<int>(m_find_matches.size()));
    }

    void ConversationReader::findPrev()
    {
        if (m_find_matches.isEmpty()) {
            return;
        }
        const int count = static_cast<int>(m_find_matches.size());
        m_find_current = ((m_find_current - 1) + count) % count;
        focusCurrentMatch();
        emit findResultsChanged(m_find_current + 1, count);
    }

    void ConversationReader::clearFind()
    {
        for (MessageWidget* widget : m_find_matches) {
            widget->setSearchHighlight(MessageWidget::SearchHighlight::None);
        }
        m_find_matches.clear();
        m_find_current = -1;
        m_find_term.clear();
        emit findResultsChanged(0, 0);
    }

    void ConversationReader::focusCurrentMatch()
    {
        if ((m_find_current < 0) || (m_find_current >= static_cast<int>(m_find_matches.size()))) {
            return;
        }
        for (int i = 0; i < m_find_matches.size(); ++i) {
            m_find_matches.at(i)->setSearchHighlight(i == m_find_current ? MessageWidget::SearchHighlight::Current : MessageWidget::SearchHighlight::Match);
        }
        ensureWidgetVisible(m_find_matches.at(m_find_current), 50, 80);
    }

    void ConversationReader::resizeEvent(QResizeEvent* event)
    {
        QScrollArea::resizeEvent(event);
        scheduleHeightUpdate();
    }

    void ConversationReader::scheduleHeightUpdate()
    {
        // Recomputing heightForWidth over hundreds of word-wrapped labels is too slow to
        // run per resize event; throttle to one immediate pass per interval, with a
        // trailing pass to settle on the final size.
        if ((!m_height_throttle.isValid()) || (m_height_throttle.elapsed() >= HEIGHT_THROTTLE_MS)) {
            m_height_throttle.restart();
            updateContentHeight();
        } else {
            m_height_timer->start(); // trailing pass to settle the final size
        }
    }

    void ConversationReader::updateContentHeight()
    {
        // widgetResizable sizes the content to its unconstrained sizeHint, which is too short
        // for word-wrapped messages. Pin a minimum height computed at the real (viewport)
        // width so heightForWidth is honoured and messages never overlap.
        QLayout* content_layout = m_container->layout();
        if ((content_layout == nullptr) || (!content_layout->hasHeightForWidth())) {
            return;
        }
        m_container->setMinimumHeight(content_layout->heightForWidth(viewport()->width()));
    }
}
