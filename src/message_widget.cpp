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

#include "message_widget.hpp"
#include "collapsible_section.hpp"
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace ChatsBrowser
{
    MessageWidget::MessageWidget(const QJsonObject& message, int branch_index, int branch_count, QWidget* parent) :
        QWidget(parent)
    {
        QString sender = message.value("sender").toString();

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(6);

        addHeader(layout, sender, branch_index, branch_count);

        // Render content blocks in their original order. Consecutive text blocks are
        // merged into one markdown label; thinking and tool blocks each become a
        // collapsed disclosure section.
        QString pending_text;

        const auto flush_text = [&]() {
            if (!pending_text.trimmed().isEmpty()) {
                addMarkdownLabel(layout, pending_text, "messageText");
                m_has_content = true;
            }
            pending_text.clear();
        };

        QJsonArray content_blocks = message.value("content").toArray();

        // Content blocks are authoritative when present. The top-level "text" field is a
        // legacy convenience that concatenates blocks (including thinking), so it is used
        // only as a fallback for older messages that carry no content blocks at all.
        if (content_blocks.isEmpty()) {
            pending_text = message.value("text").toString();
        }

        for (const QJsonValue& block_value : content_blocks) {
            QJsonObject block = block_value.toObject();
            QString type = block.value("type").toString();

            if (type == "text") {
                QString block_text = block.value("text").toString();
                if (!block_text.isEmpty()) {
                    if (!pending_text.isEmpty()) {
                        pending_text += "\n\n";
                    }
                    pending_text += block_text;
                }
            } else if (type == "thinking") {
                flush_text();
                QString thinking = block.value("thinking").toString();
                if (!thinking.trimmed().isEmpty()) {
                    addCollapsible(layout, "Thinking", thinking, false);
                    m_has_content = true;
                }
            } else if (type == "tool_use") {
                flush_text();
                QString name = block.value("name").toString();
                addCollapsible(layout, QString("Tool call · %1").arg(name), prettyJson(block.value("input")), true);
                m_has_content = true;
            } else if (type == "tool_result") {
                flush_text();
                addCollapsible(layout, "Tool result", prettyJson(block.value("content")), true);
                m_has_content = true;
            }
        }

        flush_text();
    }

    bool MessageWidget::hasRenderedContent() const
    {
        return m_has_content;
    }

    void MessageWidget::addHeader(QVBoxLayout* layout, const QString& sender, int branch_index, int branch_count)
    {
        QWidget* header_row = new QWidget(this);
        QHBoxLayout* header_layout = new QHBoxLayout(header_row);
        header_layout->setContentsMargins(0, 0, 0, 0);
        header_layout->setSpacing(6);

        QLabel* sender_label = new QLabel(senderLabel(sender), header_row);
        sender_label->setObjectName(sender == "human" ? "senderHuman" : "senderAssistant");
        header_layout->addWidget(sender_label);

        // A fork: this message is one of several sibling branches (an edit or a retry).
        // Offer claude.ai-style "< k / n >" navigation between them.
        if (branch_count > 1) {
            QToolButton* prev = new QToolButton(header_row);
            prev->setObjectName("branchNav");
            prev->setText("‹");
            prev->setEnabled(branch_index > 0);
            prev->setToolTip("Previous version");
            prev->setCursor(Qt::PointingHandCursor);
            connect(prev, &QToolButton::clicked, this, &MessageWidget::branchPrevRequested);

            QLabel* position = new QLabel(QString("%1 / %2").arg(branch_index + 1).arg(branch_count), header_row);
            position->setObjectName("branchPosition");

            QToolButton* next = new QToolButton(header_row);
            next->setObjectName("branchNav");
            next->setText("›");
            next->setEnabled(branch_index < branch_count - 1);
            next->setToolTip("Next version");
            next->setCursor(Qt::PointingHandCursor);
            connect(next, &QToolButton::clicked, this, &MessageWidget::branchNextRequested);

            header_layout->addWidget(prev);
            header_layout->addWidget(position);
            header_layout->addWidget(next);
        }

        header_layout->addStretch();
        layout->addWidget(header_row);
    }

    void MessageWidget::addMarkdownLabel(QVBoxLayout* layout, const QString& markdown, const char* object_name)
    {
        QLabel* label = new QLabel(this);
        label->setObjectName(QString::fromLatin1(object_name));
        label->setTextFormat(Qt::MarkdownText);
        label->setText(markdown);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        label->setOpenExternalLinks(true);
        layout->addWidget(label);
    }

    void MessageWidget::addCollapsible(QVBoxLayout* layout, const QString& title, const QString& body, bool monospace)
    {
        CollapsibleSection* section = new CollapsibleSection(title, false, this);

        QLabel* body_label = new QLabel(section);
        body_label->setObjectName(monospace ? "toolBody" : "thinkingBody");
        body_label->setTextFormat(monospace ? Qt::PlainText : Qt::MarkdownText);
        body_label->setText(body);
        body_label->setWordWrap(true);
        body_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        section->setContentWidget(body_label);

        layout->addWidget(section);
    }

    QString MessageWidget::senderLabel(const QString& sender)
    {
        if (sender == "human") {
            return "You";
        }
        if (sender == "assistant") {
            return "Claude";
        }
        return sender.isEmpty() ? QString("Unknown") : sender;
    }

    QString MessageWidget::prettyJson(const QJsonValue& value)
    {
        if (value.isString()) {
            return value.toString();
        }
        if (value.isObject()) {
            return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Indented));
        }
        if (value.isArray()) {
            return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented));
        }
        if (value.isNull() || value.isUndefined()) {
            return QString();
        }
        return value.toVariant().toString();
    }
}
