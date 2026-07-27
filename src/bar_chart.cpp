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

#include "bar_chart.hpp"
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

namespace ChatsBrowser
{
    namespace
    {
        // Palette: one accent hue for magnitude (validated against the #1E1E1E
        // surface), text in the theme's text tokens, recessive baseline.
        const QColor BAR_COLOUR("#CE7B3C");
        const QColor BAR_HOVER_COLOUR("#E0A96D");
        const QColor TEXT_COLOUR("#CCCCCC");
        const QColor MUTED_COLOUR("#858585");
        const QColor BASELINE_COLOUR("#3C3C3C");

        constexpr int PADDING = 8; //!< Inner padding around the plot area.
        constexpr int LABEL_HEIGHT = 18; //!< Reserved strip for category labels.
        constexpr int VALUE_HEIGHT = 14; //!< Reserved strip for value labels.
        constexpr int BAR_GAP = 2; //!< Surface gap between adjacent bars.
        constexpr qreal BAR_RADIUS = 4.0; //!< Rounded data-end radius.
    }

    BarChart::BarChart(QWidget* parent) :
        QWidget(parent)
    {
        setMouseTracking(true);
        setMinimumHeight(160);
    }

    void BarChart::setBars(const QList<Bar>& bars)
    {
        m_bars = bars;
        m_max_value = 0;
        for (const Bar& bar : m_bars) {
            m_max_value = qMax(m_max_value, bar.value);
        }
        m_hover_index = -1;
        update();
    }

    QRectF BarChart::barRect(int index) const
    {
        const int count = static_cast<int>(m_bars.size());
        const qreal plot_width = width() - (2.0 * PADDING);
        const qreal slot = plot_width / count;
        const qreal bar_width = qMax(1.0, slot - BAR_GAP);
        const qreal plot_bottom = height() - PADDING - LABEL_HEIGHT;
        const qreal plot_top = PADDING + VALUE_HEIGHT;

        const Bar& bar = m_bars.at(index);
        const qreal fraction = (m_max_value > 0) ? (static_cast<qreal>(bar.value) / m_max_value) : 0.0;
        const qreal bar_height = qMax((bar.value > 0) ? 1.0 : 0.0, fraction * (plot_bottom - plot_top));

        return QRectF(PADDING + (index * slot) + (BAR_GAP / 2.0), plot_bottom - bar_height, bar_width, bar_height);
    }

    int BarChart::barAt(int x) const
    {
        const int count = static_cast<int>(m_bars.size());
        if (count == 0) {
            return -1;
        }
        const qreal plot_width = width() - (2.0 * PADDING);
        const int index = static_cast<int>((x - PADDING) / (plot_width / count));
        return ((index >= 0) && (index < count)) ? index : -1;
    }

    void BarChart::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event);
        if (m_bars.isEmpty()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const qreal plot_bottom = height() - PADDING - LABEL_HEIGHT;
        const QFontMetrics metrics(font());

        // Bars: rounded top corners, flat baseline end.
        for (int i = 0; i < m_bars.size(); ++i) {
            const QRectF rect = barRect(i);
            QPainterPath path;
            path.addRoundedRect(rect, BAR_RADIUS, BAR_RADIUS);
            // Square the bottom corners off so the rounding is on the data end only.
            path.addRect(rect.x(), rect.y() + (rect.height() / 2.0), rect.width(), rect.height() / 2.0);
            painter.fillPath(path.simplified(), (i == m_hover_index) ? BAR_HOVER_COLOUR : BAR_COLOUR);
        }

        // Recessive baseline under the bars.
        painter.setPen(QPen(BASELINE_COLOUR, 1));
        painter.drawLine(QPointF(PADDING, plot_bottom), QPointF(width() - PADDING, plot_bottom));

        // Selective value labels: first, last and maximum bars only.
        int max_index = 0;
        for (int i = 1; i < m_bars.size(); ++i) {
            if (m_bars.at(i).value > m_bars.at(max_index).value) {
                max_index = i;
            }
        }
        painter.setPen(TEXT_COLOUR);
        for (const int i : {0, max_index, static_cast<int>(m_bars.size()) - 1}) {
            const QRectF rect = barRect(i);
            const QString text = QString::number(m_bars.at(i).value);
            const qreal text_x = rect.center().x() - (metrics.horizontalAdvance(text) / 2.0);
            painter.drawText(QPointF(text_x, rect.top() - 3), text);
        }

        // Category labels: first and last, in muted ink.
        painter.setPen(MUTED_COLOUR);
        painter.drawText(QPointF(PADDING, height() - PADDING), m_bars.first().label);
        const QString last = m_bars.last().label;
        painter.drawText(QPointF(width() - PADDING - metrics.horizontalAdvance(last), height() - PADDING), last);
    }

    void BarChart::mouseMoveEvent(QMouseEvent* event)
    {
        const int index = barAt(static_cast<int>(event->position().x()));
        if (index != m_hover_index) {
            m_hover_index = index;
            update();
        }
        if (index >= 0) {
            const Bar& bar = m_bars.at(index);
            QToolTip::showText(event->globalPosition().toPoint(), QString("%1: %2 messages").arg(bar.label).arg(bar.value), this);
        } else {
            QToolTip::hideText();
        }
    }

    void BarChart::leaveEvent(QEvent* event)
    {
        Q_UNUSED(event);
        if (m_hover_index != -1) {
            m_hover_index = -1;
            update();
        }
    }
}
