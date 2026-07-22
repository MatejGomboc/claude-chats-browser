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
#include <QIcon>
#include <QString>

namespace ChatsBrowser::IconUtil
{
    //! Loads an SVG resource and recolours it, returning a crisp high-DPI icon.
    [[nodiscard]] QIcon tinted(const QString& resource_path, const QColor& colour, int size = 16);
}
