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

#include <QStringList>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE

namespace ChatsBrowser
{
    /*!
        Renders a (possibly huge) markdown text as a stack of labels, streaming
        in paragraph-sized chunks through the event loop so layout never blocks
        the UI.

        The stream is visibility-aware: the first chunk is built immediately (so
        content appears under the click that revealed the widget) and further
        chunks render only while the widget is shown. Hiding it (collapsing the
        section) stops the stream AND discards the rendered labels — collapsed
        sections hold no laid-out widgets in memory — and showing it again
        restarts the stream from the top.
    */
    class StreamedMarkdown : public QWidget {
        Q_OBJECT

    public:
        explicit StreamedMarkdown(const QString& markdown, QWidget* parent = nullptr);

    protected:
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        //! Builds the label for the next chunk (no scheduling).
        void renderNext();

        //! Renders the next chunk and reschedules while visible and unfinished.
        void step();

        //! Splits markdown into whole-paragraph chunks, never inside a code fence.
        [[nodiscard]] static QStringList splitChunks(const QString& text);

        QVBoxLayout* m_layout{nullptr};
        QStringList m_chunks;
        qsizetype m_next{0}; //!< Index of the next chunk to render.
        bool m_step_queued{false}; //!< A step is already scheduled on the event loop.
    };
}
