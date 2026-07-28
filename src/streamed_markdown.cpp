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

#include "streamed_markdown.hpp"
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

namespace ChatsBrowser
{
    namespace
    {
        //! Target size of one streamed chunk; small enough that a single chunk's
        //! markdown layout never noticeably blocks the UI thread.
        constexpr qsizetype CHUNK_CHARS = 6000;
    }

    StreamedMarkdown::StreamedMarkdown(const QString& markdown, QWidget* parent) :
        QWidget(parent),
        m_chunks(splitChunks(markdown))
    {
        // Paint our own background so the themed border/background render.
        setAttribute(Qt::WA_StyledBackground, true);

        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(10, 6, 10, 6);
        m_layout->setSpacing(6);

        // First chunk immediately: content must appear under the click that
        // revealed this widget, before it is even shown.
        if (!m_chunks.isEmpty()) {
            renderNext();
        }
    }

    void StreamedMarkdown::renderNext()
    {
        QLabel* label = new QLabel(this);
        label->setObjectName("thinkingBodyText");
        label->setTextFormat(Qt::MarkdownText);
        label->setText(m_chunks.at(m_next));
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_layout->addWidget(label);
        m_next++;
    }

    void StreamedMarkdown::step()
    {
        m_step_queued = false;
        // A step queued before the widget was hidden still fires once; do
        // nothing — hideEvent has already discarded the content, and the next
        // showEvent restarts the stream.
        if ((!isVisible()) || (m_next >= m_chunks.size())) {
            return;
        }

        renderNext();

        if (m_next < m_chunks.size()) {
            m_step_queued = true;
            QTimer::singleShot(0, this, &StreamedMarkdown::step);
        }
    }

    void StreamedMarkdown::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        if ((m_next < m_chunks.size()) && (!m_step_queued)) {
            m_step_queued = true;
            QTimer::singleShot(0, this, &StreamedMarkdown::step);
        }
    }

    void StreamedMarkdown::hideEvent(QHideEvent* event)
    {
        QWidget::hideEvent(event);

        // Collapsing throws the rendered labels away rather than keeping them
        // parked: many expanded-then-collapsed sections would otherwise pile up
        // laid-out widgets in memory. Re-expanding restarts the stream.
        while (QLayoutItem* item = m_layout->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        m_next = 0;
    }

    QStringList StreamedMarkdown::splitChunks(const QString& text)
    {
        QStringList chunks;
        QString current;
        bool in_fence = false;

        for (const QString& paragraph : text.split("\n\n")) {
            if (!current.isEmpty()) {
                current += "\n\n";
            }
            current += paragraph;

            for (const QString& line : paragraph.split('\n')) {
                if (line.trimmed().startsWith("```")) {
                    in_fence = !in_fence;
                }
            }
            if ((!in_fence) && (current.size() >= CHUNK_CHARS)) {
                chunks.append(current);
                current.clear();
            }
        }
        if (!current.isEmpty()) {
            chunks.append(current);
        }
        return chunks;
    }
}
