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
#include <functional>

QT_BEGIN_NAMESPACE
class QToolButton;
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
        //! How this message is emphasised by an in-conversation find.
        enum class SearchHighlight {
            None, //!< Not a match.
            Match, //!< A match, but not the one currently focused.
            Current //!< The match the find bar is currently sitting on.
        };

        /*!
            \param message The message object as stored in the export (its raw JSON).
            \param branch_index Zero-based position of this message among its siblings.
            \param branch_count Number of sibling branches at this point (1 = no fork).
        */
        explicit MessageWidget(const QJsonObject& message, int branch_index = 0, int branch_count = 1, QWidget* parent = nullptr);

        //! True if the message produced any visible content (text, thinking, or a tool block).
        [[nodiscard]] bool hasRenderedContent() const;

        //! The message's visible text, used to match in-conversation finds.
        [[nodiscard]] QString searchableText() const;

        //! Tints the whole message to mark it as a find match (see SearchHighlight).
        void setSearchHighlight(SearchHighlight state);

    signals:
        //! Emitted when the user asks for the previous sibling branch at this fork.
        void branchPrevRequested();

        //! Emitted when the user asks for the next sibling branch at this fork.
        void branchNextRequested();

    private:
        void addHeader(QVBoxLayout* layout, const QString& sender, const QString& created_at, int branch_index, int branch_count);

        //! Renders the message's attachments (pasted text) and file references (images).
        void addAttachments(QVBoxLayout* layout, const QJsonObject& message);

        //! Renders markdown, breaking fenced code blocks out into highlighted code editors.
        void addRichText(QVBoxLayout* layout, const QString& markdown);

        //! Adds a syntax-highlighted, read-only code block (with a copy button) sized to its content.
        void addCodeBlock(QVBoxLayout* layout, const QString& code);

        void addMarkdownLabel(QVBoxLayout* layout, const QString& markdown, const char* object_name);

        //! Adds a collapsed section whose body text is produced lazily on first expansion.
        void addCollapsible(QVBoxLayout* layout, const QString& title, std::function<QString()> body_provider, bool monospace);

        [[nodiscard]] static QString senderLabel(const QString& sender);
        [[nodiscard]] static QString formatTimestamp(const QString& iso_timestamp);
        [[nodiscard]] static QString prettyJson(const QJsonValue& value);

        //! Serialises a JSON value into self-owned bytes (independent of the source document).
        [[nodiscard]] static QByteArray wrapValue(const QJsonValue& value);

        //! Recovers a value previously stored by wrapValue().
        [[nodiscard]] static QJsonValue unwrapValue(const QByteArray& bytes);

        bool m_has_content{false};
        QString m_message_text; //!< The message's visible text, for the copy-message button.
        QToolButton* m_copy_button{nullptr}; //!< Header copy button; hidden for text-less messages.
        SearchHighlight m_search_highlight{SearchHighlight::None}; //!< Current find emphasis.
    };
}
