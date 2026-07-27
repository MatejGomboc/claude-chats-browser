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

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

namespace ChatsBrowser
{
    /*!
        A minimal single-series vertical bar chart, painted directly.

        One hue only (magnitude, not identity), rounded data-ends anchored to the
        baseline, a gap between bars, a recessive baseline, and a hover tooltip
        per bar. Values are labelled selectively (first, last, and maximum), with
        the full detail available on hover — no dependency on Qt Charts.
    */
    class BarChart : public QWidget {
        Q_OBJECT

    public:
        struct Bar {
            QString label; //!< Category label (e.g. "2025-04").
            int value{0};
        };

        explicit BarChart(QWidget* parent = nullptr);

        //! Replaces the data and repaints. Bars keep the given order.
        void setBars(const QList<Bar>& bars);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        //! Index of the bar at an x pixel position, or -1.
        [[nodiscard]] int barAt(int x) const;

        //! The horizontal span of bar \a index within the plot area.
        [[nodiscard]] QRectF barRect(int index) const;

        QList<Bar> m_bars;
        int m_max_value{0};
        int m_hover_index{-1};
    };
}
