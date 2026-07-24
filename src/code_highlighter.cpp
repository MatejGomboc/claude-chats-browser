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

#include "code_highlighter.hpp"
#include <QColor>

namespace ChatsBrowser
{
    namespace
    {
        // VS Code Dark+ token colours.
        const QColor KEYWORD_COLOUR = QColor("#569CD6");
        const QColor TYPE_COLOUR = QColor("#4EC9B0");
        const QColor NUMBER_COLOUR = QColor("#B5CEA8");
        const QColor STRING_COLOUR = QColor("#CE9178");
        const QColor COMMENT_COLOUR = QColor("#6A9955");

        // A broad union of keywords across the common languages. A snippet's exact language
        // is unknown, and over-matching a few identifiers reads fine.
        const char* const KEYWORDS[] = {
            "abstract",
            "and",
            "as",
            "async",
            "await",
            "break",
            "case",
            "catch",
            "chan",
            "class",
            "const",
            "continue",
            "def",
            "defer",
            "del",
            "delete",
            "do",
            "elif",
            "else",
            "enum",
            "export",
            "extends",
            "extern",
            "final",
            "finally",
            "fn",
            "for",
            "from",
            "func",
            "function",
            "go",
            "goto",
            "if",
            "impl",
            "implements",
            "import",
            "in",
            "include",
            "instanceof",
            "interface",
            "is",
            "lambda",
            "let",
            "loop",
            "match",
            "module",
            "mut",
            "namespace",
            "new",
            "nil",
            "none",
            "not",
            "operator",
            "or",
            "override",
            "package",
            "pass",
            "private",
            "protected",
            "public",
            "raise",
            "return",
            "select",
            "self",
            "static",
            "struct",
            "super",
            "switch",
            "template",
            "this",
            "throw",
            "throws",
            "trait",
            "try",
            "type",
            "typedef",
            "typename",
            "union",
            "unsafe",
            "use",
            "using",
            "var",
            "virtual",
            "void",
            "where",
            "while",
            "with",
            "yield",
        };

        // Common primitive/library type names, coloured like types.
        const char* const TYPES[] = {
            "bool",
            "boolean",
            "byte",
            "char",
            "double",
            "float",
            "int",
            "int8",
            "int16",
            "int32",
            "int64",
            "long",
            "short",
            "signed",
            "size_t",
            "string",
            "str",
            "uint",
            "uint8",
            "uint16",
            "uint32",
            "uint64",
            "unsigned",
            "usize",
            "isize",
            "String",
            "Vec",
            "Option",
            "Result",
            "auto",
        };

        // Language-agnostic literals.
        const char* const LITERALS[] = {
            "true",
            "false",
            "True",
            "False",
            "None",
            "null",
            "NULL",
            "nullptr",
            "undefined",
        };

        QString wordAlternation(const char* const* words, int count)
        {
            QStringList alternatives;
            alternatives.reserve(count);
            for (int i = 0; i < count; ++i) {
                alternatives.append(QRegularExpression::escape(QString::fromLatin1(words[i])));
            }
            return QString("\\b(?:%1)\\b").arg(alternatives.join('|'));
        }
    }

    CodeHighlighter::CodeHighlighter(QTextDocument* document) :
        QSyntaxHighlighter(document)
    {
        const auto add_rule = [this](const QString& pattern, const QColor& colour, bool bold = false) {
            Rule rule;
            rule.pattern = QRegularExpression(pattern);
            rule.format.setForeground(colour);
            if (bold) {
                rule.format.setFontWeight(QFont::DemiBold);
            }
            m_rules.append(rule);
        };

        // Order matters: strings and comments are added last so they override keyword and
        // number colouring for text that falls inside them.
        add_rule(wordAlternation(KEYWORDS, static_cast<int>(std::size(KEYWORDS))), KEYWORD_COLOUR);
        add_rule(wordAlternation(TYPES, static_cast<int>(std::size(TYPES))), TYPE_COLOUR);
        add_rule(wordAlternation(LITERALS, static_cast<int>(std::size(LITERALS))), KEYWORD_COLOUR);
        add_rule("\\b(?:0[xX][0-9a-fA-F]+|\\d+\\.?\\d*(?:[eE][+-]?\\d+)?)\\b", NUMBER_COLOUR);

        // Strings: double, single and back-tick quoted, tolerating escaped quotes.
        add_rule("\"(?:[^\"\\\\]|\\\\.)*\"", STRING_COLOUR);
        add_rule("'(?:[^'\\\\]|\\\\.)*'", STRING_COLOUR);
        add_rule("`(?:[^`\\\\]|\\\\.)*`", STRING_COLOUR);

        // Line comments: //…, #…, --… and ;… to end of line.
        add_rule("(?://|#|--|;).*$", COMMENT_COLOUR);

        m_block_comment_start = QRegularExpression("/\\*");
        m_block_comment_end = QRegularExpression("\\*/");
        m_comment_format.setForeground(COMMENT_COLOUR);
    }

    void CodeHighlighter::highlightBlock(const QString& text)
    {
        for (const Rule& rule : m_rules) {
            QRegularExpressionMatchIterator matches = rule.pattern.globalMatch(text);
            while (matches.hasNext()) {
                const QRegularExpressionMatch match = matches.next();
                setFormat(static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength()), rule.format);
            }
        }

        // Multi-line /* */ comments, tracked across blocks via the block state.
        setCurrentBlockState(0);
        int start = 0;
        if (previousBlockState() != 1) {
            start = static_cast<int>(text.indexOf(m_block_comment_start));
        }
        while (start >= 0) {
            const QRegularExpressionMatch end_match = m_block_comment_end.match(text, start);
            const int end = static_cast<int>(end_match.capturedStart());
            int length = 0;
            if (end < 0) {
                setCurrentBlockState(1);
                length = static_cast<int>(text.length()) - start;
            } else {
                length = end - start + static_cast<int>(end_match.capturedLength());
            }
            setFormat(start, length, m_comment_format);
            start = static_cast<int>(text.indexOf(m_block_comment_start, start + length));
        }
    }
}
