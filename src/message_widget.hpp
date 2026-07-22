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
#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE

namespace ChatsBrowser
{
    /*!
        Renders one message: a sender header followed by its content blocks in order.

        Plain text renders as markdown; thinking blocks and tool calls/results render as
        collapsed disclosure sections so a turn reads cleanly but stays inspectable.
    */
    class MessageWidget : public QWidget {
        Q_OBJECT

    public:
        /*!
            \param message The message object as stored in the export (its raw JSON).
            \param branch_index Zero-based position of this message among its siblings.
            \param branch_count Number of sibling branches at this point (1 = no fork).
        */
        explicit MessageWidget(const QJsonObject& message, int branch_index = 0, int branch_count = 1, QWidget* parent = nullptr);

        //! True if the message produced any visible content (text, thinking, or a tool block).
        [[nodiscard]] bool hasRenderedContent() const;

    signals:
        //! Emitted when the user asks for the previous sibling branch at this fork.
        void branchPrevRequested();

        //! Emitted when the user asks for the next sibling branch at this fork.
        void branchNextRequested();

    private:
        void addHeader(QVBoxLayout* layout, const QString& sender, int branch_index, int branch_count);
        void addMarkdownLabel(QVBoxLayout* layout, const QString& markdown, const char* object_name);
        void addCollapsible(QVBoxLayout* layout, const QString& title, const QString& body, bool monospace);

        [[nodiscard]] static QString senderLabel(const QString& sender);
        [[nodiscard]] static QString prettyJson(const QJsonValue& value);

        bool m_has_content{false};
    };
}
