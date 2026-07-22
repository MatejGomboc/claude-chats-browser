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

#include "icon_util.hpp"
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace ChatsBrowser::IconUtil
{
    QIcon tinted(const QString& resource_path, const QColor& colour, int size)
    {
        constexpr qreal RENDER_SCALE = 2.0;

        QSvgRenderer renderer(resource_path);
        if (!renderer.isValid()) {
            return QIcon();
        }

        QPixmap pixmap(QSize(size, size) * RENDER_SCALE);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        renderer.render(&painter);
        // Recolour every opaque pixel of the glyph while keeping its alpha shape.
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), colour);
        painter.end();

        pixmap.setDevicePixelRatio(RENDER_SCALE);
        return QIcon(pixmap);
    }
}
