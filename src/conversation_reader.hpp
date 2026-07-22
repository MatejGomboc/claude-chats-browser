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

#include <QScrollArea>
#include <QString>

QT_BEGIN_NAMESPACE
class QLabel;
class QVBoxLayout;
class QWidget;
QT_END_NAMESPACE

namespace ChatsBrowser
{
    /*!
        Scrollable reader for a single conversation.

        Each message becomes a MessageWidget stacked top to bottom, so text, thinking
        blocks and tool calls render structurally rather than as one flat blob.
    */
    class ConversationReader : public QScrollArea {
        Q_OBJECT

    public:
        explicit ConversationReader(QWidget* parent = nullptr);

        //! Loads and displays the conversation with the given UUID; empty clears the view.
        void showConversation(const QString& conversation_uuid);

        //! Clears the view back to its placeholder prompt.
        void clearConversation();

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        //! Forces the scrolled content tall enough for word-wrapped messages at the current width.
        void updateContentHeight();

        //! Removes all message widgets, leaving the trailing stretch in place.
        void clearMessages();

        //! Shows a single centred placeholder message instead of a conversation.
        void showPlaceholder(const QString& text);

        QWidget* m_container{nullptr}; //!< Scrolled content widget.
        QVBoxLayout* m_layout{nullptr}; //!< Vertical stack of message widgets, with a trailing stretch.
        QLabel* m_placeholder{nullptr}; //!< Shown when there is nothing to read.
        QString m_conversation_uuid;
    };
}
