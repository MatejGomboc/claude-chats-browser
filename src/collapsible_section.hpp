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

#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QToolButton;
QT_END_NAMESPACE

namespace ChatsBrowser
{
    //! A titled disclosure widget: a clickable header that shows or hides a content widget.
    class CollapsibleSection : public QWidget {
        Q_OBJECT

    public:
        explicit CollapsibleSection(const QString& title, bool expanded, QWidget* parent = nullptr);

        //! Takes ownership of \a content and places it under the header.
        void setContentWidget(QWidget* content);

    private slots:
        void onToggled(bool expanded);

    private:
        QToolButton* m_toggle{nullptr}; //!< Header button; its checked state is the expanded state.
        QWidget* m_content{nullptr}; //!< The disclosed widget (owned via the layout).
    };
}
