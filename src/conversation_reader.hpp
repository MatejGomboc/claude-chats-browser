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

#include <QString>
#include <QTextBrowser>

namespace ChatsBrowser
{
    //! Renders a single conversation's messages, newest turn last, as formatted markdown.
    class ConversationReader : public QTextBrowser {
        Q_OBJECT

    public:
        explicit ConversationReader(QWidget* parent = nullptr);

        //! Loads and displays the conversation with the given UUID; empty clears the view.
        void showConversation(const QString& conversation_uuid);

        //! Clears the view back to its placeholder prompt.
        void clearConversation();

    private:
        //! Builds the markdown document for one conversation from its messages.
        [[nodiscard]] static QString buildMarkdown(const QString& conversation_uuid, bool* found_any_content);

        //! A human-readable label for a message sender ("You", "Claude", or the raw value).
        [[nodiscard]] static QString senderLabel(const QString& sender);

        QString m_conversation_uuid;
    };
}
