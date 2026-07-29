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
#include <QList>
#include <QString>

namespace ChatsBrowser
{
    //! Conversation metadata for an export, as stored in the conversations table.
    struct ConversationInfo {
        QString uuid;
        QString name;
        QString summary;
        QString created_at;
        QString updated_at;
    };

    /*!
        Turns one conversation into a shareable file.

        Two formats with different promises:
        - Markdown: the *displayed* branch path, made for sharing and reading —
          sender and timestamp headers, fenced code preserved, attachments noted
          by name, and a one-line trace per tool call so an answer that was looked
          up does not read as one that was simply known. Thinking, tool inputs and
          tool results are omitted: the first is private reasoning, the others are
          unbounded payloads (the JSON export keeps them).
        - JSON: lossless — every message of every branch as its original export
          object, in a conversation envelope matching the claude.ai export shape,
          so the file can be re-imported by this app.

        Pure functions over message JSON, so both formats are unit-testable.
    */
    class ConversationExporter {
    public:
        //! Renders the given (already branch-selected, ordered) messages as markdown.
        [[nodiscard]] static QString toMarkdown(const ConversationInfo& info, const QList<QJsonObject>& path_messages);

        //! Wraps all messages (every branch, ordered) in an export-shaped envelope.
        [[nodiscard]] static QJsonObject toExportJson(const ConversationInfo& info, const QList<QJsonObject>& all_messages);

        //! A filesystem-safe file name (no extension) derived from the conversation name.
        [[nodiscard]] static QString suggestedBaseName(const ConversationInfo& info);

        //! The message's visible prose: its text blocks joined (legacy text fallback).
        [[nodiscard]] static QString visibleText(const QJsonObject& message);

        //! One markdown line naming a tool call (and its URL, when it has one).
        //! Never includes the tool's input beyond that URL.
        [[nodiscard]] static QString toolTrace(const QJsonObject& tool_use_block);
    };
}
