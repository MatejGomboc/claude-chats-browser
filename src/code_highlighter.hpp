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

#include <QList>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

namespace ChatsBrowser
{
    /*!
        A small, dependency-free syntax highlighter for fenced code blocks.

        It is deliberately language-agnostic: a broad union of keywords across the common
        languages, plus rules for strings, numbers and comments. That gives a readable,
        VS Code-flavoured result for any snippet without bundling per-language grammars.
    */
    class CodeHighlighter : public QSyntaxHighlighter {
        Q_OBJECT

    public:
        explicit CodeHighlighter(QTextDocument* document);

    protected:
        void highlightBlock(const QString& text) override;

    private:
        struct Rule {
            QRegularExpression pattern;
            QTextCharFormat format;
        };

        QList<Rule> m_rules;
        QRegularExpression m_block_comment_start;
        QRegularExpression m_block_comment_end;
        QTextCharFormat m_comment_format;
    };
}
