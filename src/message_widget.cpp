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
#include "code_highlighter.hpp"
#include "collapsible_section.hpp"
#include "icon_util.hpp"
#include "streamed_markdown.hpp"
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSet>
#include <QStringList>
#include <QToolButton>
#include <QVBoxLayout>

namespace ChatsBrowser
{
    MessageWidget::MessageWidget(const QJsonObject& message, int branch_index, int branch_count, QWidget* parent) :
        QWidget(parent)
    {
        QString sender = message.value("sender").toString();

        // Paint our own background so a find highlight (a class-selector background rule)
        // is actually rendered; without this the widget stays transparent.
        setAttribute(Qt::WA_StyledBackground, true);

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(6);

        addHeader(layout, sender, message.value("created_at").toString(), branch_index, branch_count);
        addAttachments(layout, message);

        // Render content blocks in their original order. Consecutive text blocks are
        // merged into one markdown label; thinking and tool blocks each become a
        // collapsed disclosure section.
        QString pending_text;

        const auto flush_text = [&]() {
            if (!pending_text.trimmed().isEmpty()) {
                addRichText(layout, pending_text);
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
            m_message_text = pending_text;
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
                    if (!m_message_text.isEmpty()) {
                        m_message_text += "\n\n";
                    }
                    m_message_text += block_text;
                }
            } else if (type == "thinking") {
                flush_text();
                QString thinking = block.value("thinking").toString();
                if (!thinking.trimmed().isEmpty()) {
                    addCollapsible(
                        layout, "Thinking",
                        [thinking]() {
                            return thinking;
                        },
                        false);
                    m_has_content = true;
                }
            } else if (type == "tool_use") {
                flush_text();
                QString name = block.value("name").toString();
                // Capture self-owned bytes, not the QJsonValue: the value references the
                // caller's QJsonDocument, which is gone by the time the section expands.
                QByteArray input_bytes = wrapValue(block.value("input"));
                addCollapsible(
                    layout, QString("Tool call · %1").arg(name),
                    [input_bytes]() {
                        return prettyJson(unwrapValue(input_bytes));
                    },
                    true);
                m_has_content = true;
            } else if (type == "tool_result") {
                flush_text();
                QByteArray content_bytes = wrapValue(block.value("content"));
                addCollapsible(
                    layout, "Tool result",
                    [content_bytes]() {
                        return prettyJson(unwrapValue(content_bytes));
                    },
                    true);
                m_has_content = true;
            }
        }

        flush_text();

        // A message that is only tool calls / thinking has nothing to copy.
        if (m_message_text.trimmed().isEmpty() && (m_copy_button != nullptr)) {
            m_copy_button->hide();
        }
    }

    bool MessageWidget::hasRenderedContent() const
    {
        return m_has_content;
    }

    QString MessageWidget::searchableText() const
    {
        return m_message_text;
    }

    void MessageWidget::setSearchHighlight(SearchHighlight state)
    {
        if (state == m_search_highlight) {
            return;
        }
        m_search_highlight = state;

        // Scope the rule to our own type so the tint lands on the message frame only and
        // never cascades into the child labels/code editors.
        switch (state) {
        case SearchHighlight::None:
            setStyleSheet(QString());
            break;
        case SearchHighlight::Match:
            setStyleSheet("ChatsBrowser--MessageWidget { background-color: #2E2A1E; }");
            break;
        case SearchHighlight::Current:
            setStyleSheet("ChatsBrowser--MessageWidget { background-color: #4A3A1E; }");
            break;
        }
    }

    void MessageWidget::addHeader(QVBoxLayout* layout, const QString& sender, const QString& created_at, int branch_index, int branch_count)
    {
        QWidget* header_row = new QWidget(this);
        QHBoxLayout* header_layout = new QHBoxLayout(header_row);
        header_layout->setContentsMargins(0, 0, 0, 0);
        header_layout->setSpacing(6);

        QLabel* sender_label = new QLabel(senderLabel(sender), header_row);
        sender_label->setObjectName(sender == "human" ? "senderHuman" : "senderAssistant");
        header_layout->addWidget(sender_label);

        const QString timestamp = formatTimestamp(created_at);
        if (!timestamp.isEmpty()) {
            QLabel* time_label = new QLabel(timestamp, header_row);
            time_label->setObjectName("messageTime");
            header_layout->addWidget(time_label);
        }

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

        m_copy_button = new QToolButton(header_row);
        m_copy_button->setObjectName("copyButton");
        m_copy_button->setIcon(IconUtil::tinted(":/icons/copy.svg", QColor("#858585")));
        m_copy_button->setToolTip("Copy message");
        m_copy_button->setCursor(Qt::PointingHandCursor);
        connect(m_copy_button, &QToolButton::clicked, this, [this]() {
            QGuiApplication::clipboard()->setText(m_message_text);
        });
        header_layout->addWidget(m_copy_button);

        layout->addWidget(header_row);
    }

    void MessageWidget::addAttachments(QVBoxLayout* layout, const QJsonObject& message)
    {
        // Pasted-text attachments carry their extracted content inline in the export.
        QSet<QString> attachment_names;
        const QJsonArray attachments = message.value("attachments").toArray();
        for (const QJsonValue& value : attachments) {
            const QJsonObject attachment = value.toObject();
            const QString name = attachment.value("file_name").toString();
            attachment_names.insert(name);
            const QString content = attachment.value("extracted_content").toString();
            if (content.trimmed().isEmpty()) {
                continue;
            }
            addCollapsible(
                layout, QString("Attachment · %1").arg(name.isEmpty() ? QString("file") : name),
                [content]() {
                    return content;
                },
                true);
            m_has_content = true;
        }

        // File references carry no binary in the export — show a placeholder chip. Skip
        // files already shown above as text attachments (an item can appear in both arrays).
        const QJsonArray files = message.value("files").toArray();
        for (const QJsonValue& value : files) {
            const QString name = value.toObject().value("file_name").toString();
            if (attachment_names.contains(name)) {
                continue;
            }
            static const QStringList image_extensions = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".svg"};
            bool is_image = false;
            for (const QString& extension : image_extensions) {
                if (name.endsWith(extension, Qt::CaseInsensitive)) {
                    is_image = true;
                    break;
                }
            }
            const QString kind = is_image ? QString("image") : QString("file");
            QLabel* chip = new QLabel(QString("%1  %2  (%3 not included in the export)").arg(is_image ? "🖼" : "📎", name.isEmpty() ? kind : name, kind), this);
            chip->setObjectName("attachmentFile");
            chip->setWordWrap(true);
            layout->addWidget(chip);
            m_has_content = true;
        }
    }

    QString MessageWidget::formatTimestamp(const QString& iso_timestamp)
    {
        // "2026-07-18T15:08:39.970934Z" -> "2026-07-18 15:08" in local time.
        const QDateTime moment = QDateTime::fromString(iso_timestamp, Qt::ISODateWithMs);
        if (!moment.isValid()) {
            return QString();
        }
        return moment.toLocalTime().toString("yyyy-MM-dd HH:mm");
    }

    void MessageWidget::addRichText(QVBoxLayout* layout, const QString& markdown)
    {
        // Split the text on ``` fences: prose renders as markdown, code renders in a
        // highlighted editor. (QLabel markdown shows code as flat monospace with no colour.)
        const QStringList lines = markdown.split('\n');
        QStringList prose_lines;
        QStringList code_lines;
        bool in_code = false;

        const auto flush_prose = [&]() {
            const QString prose = prose_lines.join('\n');
            if (!prose.trimmed().isEmpty()) {
                addMarkdownLabel(layout, prose, "messageText");
            }
            prose_lines.clear();
        };
        const auto flush_code = [&]() {
            addCodeBlock(layout, code_lines.join('\n'));
            code_lines.clear();
        };

        for (const QString& line : lines) {
            if (line.trimmed().startsWith("```")) {
                if (in_code) {
                    flush_code();
                    in_code = false;
                } else {
                    flush_prose();
                    in_code = true;
                }
            } else if (in_code) {
                code_lines.append(line);
            } else {
                prose_lines.append(line);
            }
        }

        if (in_code) {
            flush_code(); // an unterminated fence: render what we have as code
        } else {
            flush_prose();
        }
    }

    void MessageWidget::addCodeBlock(QVBoxLayout* layout, const QString& code)
    {
        QWidget* container = new QWidget(this);
        QVBoxLayout* container_layout = new QVBoxLayout(container);
        container_layout->setContentsMargins(0, 0, 0, 0);
        container_layout->setSpacing(0);

        // A thin bar with a right-aligned copy button above the code.
        QWidget* bar = new QWidget(container);
        QHBoxLayout* bar_layout = new QHBoxLayout(bar);
        bar_layout->setContentsMargins(0, 0, 0, 0);
        bar_layout->addStretch();
        QToolButton* copy = new QToolButton(bar);
        copy->setObjectName("copyButton");
        copy->setIcon(IconUtil::tinted(":/icons/copy.svg", QColor("#858585")));
        copy->setToolTip("Copy code");
        copy->setCursor(Qt::PointingHandCursor);
        connect(copy, &QToolButton::clicked, this, [code]() {
            QGuiApplication::clipboard()->setText(code);
        });
        bar_layout->addWidget(copy);
        container_layout->addWidget(bar);

        QPlainTextEdit* editor = new QPlainTextEdit(container);
        editor->setObjectName("codeBlock");
        editor->setReadOnly(true);
        editor->setFrameShape(QFrame::NoFrame);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        editor->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        editor->document()->setDocumentMargin(8);

        QFont mono("Cascadia Code");
        mono.setStyleHint(QFont::Monospace);
        mono.setPointSize(9);
        editor->setFont(mono);

        editor->setPlainText(code);
        new CodeHighlighter(editor->document()); // owned by the document

        // Fixed height fitting all lines (no vertical scrollbar). Long lines scroll
        // horizontally rather than wrap, so leave room for that scrollbar.
        const QFontMetrics metrics(mono);
        const int line_count = qMax(1, editor->document()->blockCount());
        editor->setFixedHeight((line_count * metrics.lineSpacing()) + (2 * 8) + 14);
        container_layout->addWidget(editor);

        layout->addWidget(container);
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

    void MessageWidget::addCollapsible(QVBoxLayout* layout, const QString& title, std::function<QString()> body_provider, bool monospace)
    {
        CollapsibleSection* section = new CollapsibleSection(title, false, this);

        // The body widget (and the body text itself, e.g. pretty-printed tool JSON) is
        // only built when the user first expands the section — building thousands of
        // them eagerly is what froze the UI on large conversations.
        //
        // Monospace bodies (tool JSON, attachments) go into a QPlainTextEdit exactly
        // like code blocks: its plain-text layout handles huge documents instantly,
        // and with wrapping off the height is just the line count — expanding a
        // megabyte tool result used to hang for seconds in QLabel's layout engine.
        // Thinking keeps full markdown rendering, but streams in paragraph chunks
        // through the event loop (the reader's own chunked-render trick), so a long
        // block never freezes the UI while it lays out.
        section->setContentFactory([body_provider = std::move(body_provider), monospace](QWidget* parent) -> QWidget* {
            const QString body = body_provider();
            if (monospace) {
                QPlainTextEdit* editor = new QPlainTextEdit(parent);
                editor->setObjectName("toolBody");
                editor->setReadOnly(true);
                editor->setFrameShape(QFrame::NoFrame);
                editor->setLineWrapMode(QPlainTextEdit::NoWrap);
                editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                editor->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
                editor->document()->setDocumentMargin(8);

                QFont mono("Cascadia Code");
                mono.setStyleHint(QFont::Monospace);
                mono.setPointSize(9);
                editor->setFont(mono);

                editor->setPlainText(body);

                const QFontMetrics metrics(mono);
                const int line_count = qMax(1, editor->document()->blockCount());
                editor->setFixedHeight((line_count * metrics.lineSpacing()) + (2 * 8) + 14);
                return editor;
            }

            StreamedMarkdown* streamed = new StreamedMarkdown(body, parent);
            streamed->setObjectName("thinkingBody");
            return streamed;
        });

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

    QByteArray MessageWidget::wrapValue(const QJsonValue& value)
    {
        // Serialise any value (object/array/string/...) into self-owned bytes by wrapping
        // it in a one-element array, so the captured data no longer depends on the source
        // QJsonDocument's lifetime.
        return QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    }

    QJsonValue MessageWidget::unwrapValue(const QByteArray& bytes)
    {
        const QJsonArray wrapper = QJsonDocument::fromJson(bytes).array();
        return wrapper.isEmpty() ? QJsonValue() : wrapper.at(0);
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
