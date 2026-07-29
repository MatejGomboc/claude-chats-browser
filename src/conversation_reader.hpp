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

#include "conversation_tree.hpp"
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QScrollArea>
#include <QString>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QLabel;
class QVBoxLayout;
class QWidget;
QT_END_NAMESPACE

namespace ChatsBrowser
{
    class MessageWidget;

    /*!
        Scrollable reader for a single conversation.

        A conversation is a tree: editing a prompt or retrying a reply creates sibling
        messages under a shared parent. The reader renders one linear path through that
        tree (each message a MessageWidget) and shows "< k / n >" branch controls at every
        fork, so alternate versions stay reachable — like claude.ai.
    */
    class ConversationReader : public QScrollArea {
        Q_OBJECT

    public:
        explicit ConversationReader(QWidget* parent = nullptr);

        //! Loads and displays the conversation with the given UUID; empty clears the view.
        void showConversation(const QString& conversation_uuid);

        //! Clears the view back to its placeholder prompt.
        void clearConversation();

        //! Highlights every message whose text contains \a term and jumps to the first
        //! match. Case-insensitive; an empty term clears the find. Returns the match count.
        int findText(const QString& term);

        //! Moves to the next / previous match, wrapping around. No-op without matches.
        void findNext();
        void findPrev();

        //! Clears any active find: removes highlights and forgets the matches.
        void clearFind();

        //! The UUID of the conversation being shown (empty when none).
        [[nodiscard]] QString conversationUuid() const;

        //! The currently displayed branch path as parsed message objects, in order.
        //! What the user sees is what this returns — branch selection included.
        [[nodiscard]] QList<QJsonObject> currentPathMessages() const;

    signals:
        //! Progress of a chunked render (emitted only for conversations that need chunking).
        void renderProgressChanged(int done_messages, int total_messages);

        //! Emitted when a chunked render completes or is superseded.
        void renderFinished();

        //! Reports the state of the current find: the 1-based current match (0 if none)
        //! out of \a total matches. Drives the find bar's "k / n" counter.
        void findResultsChanged(int current, int total);

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        //! Loads the conversation's messages and builds the parent→children tree.
        void buildTree(const QString& conversation_uuid);

        /*!
            Starts rendering the currently-selected path through the tree.

            The first chunk of message widgets is built synchronously so the viewport
            fills immediately; the rest stream in through queued zero-delay timer steps,
            keeping the UI responsive on large conversations. A generation counter
            cancels pending chunks when a new render starts.
        */
        void renderPath(bool reset_scroll);

        //! Builds the next chunk of pending message widgets; reschedules itself while more remain.
        void appendPendingChunk(int generation);

        //! Forces the scrolled content tall enough for word-wrapped messages at the current width.
        void updateContentHeight();

        //! Throttled updateContentHeight for resize storms.
        void scheduleHeightUpdate();

        //! Removes all message widgets, leaving the trailing stretch in place.
        void clearMessages();

        //! Scrolls the current match into view and moves the Current highlight onto it.
        void focusCurrentMatch();

        //! Shows a single centred placeholder message instead of a conversation.
        void showPlaceholder(const QString& text);

        QWidget* m_container{nullptr}; //!< Scrolled content widget.
        QVBoxLayout* m_layout{nullptr}; //!< Vertical stack of message widgets, with a trailing stretch.
        QLabel* m_placeholder{nullptr}; //!< Shown when there is nothing to read.
        QString m_conversation_uuid;

        //! uuid → raw message JSON. Parsed lazily per rendered chunk: parsing hundreds of
        //! large blobs up front was a measurable part of the open-conversation hang.
        QHash<QString, QByteArray> m_raw_messages;
        ConversationTree m_tree; //!< Reply tree + branch selection for the current conversation.

        QList<PathNode> m_pending_nodes; //!< Remaining path nodes of the render in progress.
        int m_pending_index{0}; //!< Next node to build in m_pending_nodes.
        int m_render_generation{0}; //!< Bumped per render; stale queued chunks bail out.
        bool m_any_content{false}; //!< Whether the render in progress produced a widget yet.
        QTimer* m_height_timer{nullptr}; //!< Trailing height update after resize storms.
        QElapsedTimer m_height_throttle; //!< Rate-limits immediate height updates on resize.

        QString m_find_term; //!< Active find term (empty when no find is running).
        QList<MessageWidget*> m_find_matches; //!< Matching message widgets, in display order.
        int m_find_current{-1}; //!< Index into m_find_matches of the focused match (-1 = none).
    };
}
