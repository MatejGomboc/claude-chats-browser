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

#include "collapsible_section.hpp"
#include <QToolButton>
#include <QVBoxLayout>

namespace ChatsBrowser
{
    CollapsibleSection::CollapsibleSection(const QString& title, bool expanded, QWidget* parent) :
        QWidget(parent)
    {
        m_toggle = new QToolButton(this);
        m_toggle->setStyleSheet("QToolButton { border: none; }");
        m_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_toggle->setCheckable(true);
        m_toggle->setChecked(expanded);
        m_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        m_toggle->setText(title);
        m_toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        layout->addWidget(m_toggle);

        connect(m_toggle, &QToolButton::toggled, this, &CollapsibleSection::onToggled);
    }

    void CollapsibleSection::setContentWidget(QWidget* content)
    {
        m_content = content;
        m_content->setVisible(m_toggle->isChecked());
        layout()->addWidget(m_content);
    }

    void CollapsibleSection::onToggled(bool expanded)
    {
        m_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        if (m_content != nullptr) {
            m_content->setVisible(expanded);
        }
    }
}
