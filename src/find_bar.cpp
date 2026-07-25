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

#include "find_bar.hpp"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

namespace ChatsBrowser
{
    FindBar::FindBar(QWidget* parent) :
        QWidget(parent)
    {
        setObjectName("findBar");

        QHBoxLayout* row = new QHBoxLayout(this);
        row->setContentsMargins(8, 4, 8, 4);
        row->setSpacing(4);

        m_input = new QLineEdit(this);
        m_input->setObjectName("findInput");
        m_input->setPlaceholderText("Find in conversation");
        m_input->setClearButtonEnabled(true);
        // Enter / Shift+Enter / Escape are handled in eventFilter (returnPressed alone
        // cannot tell the modifier apart).
        m_input->installEventFilter(this);

        m_count = new QLabel(this);
        m_count->setObjectName("findCount");

        QToolButton* prev = new QToolButton(this);
        prev->setObjectName("findNav");
        prev->setText("‹");
        prev->setToolTip("Previous match (Shift+Enter)");
        prev->setCursor(Qt::PointingHandCursor);

        QToolButton* next = new QToolButton(this);
        next->setObjectName("findNav");
        next->setText("›");
        next->setToolTip("Next match (Enter)");
        next->setCursor(Qt::PointingHandCursor);

        QToolButton* close = new QToolButton(this);
        close->setObjectName("findNav");
        close->setText("✕");
        close->setToolTip("Close (Esc)");
        close->setCursor(Qt::PointingHandCursor);

        row->addWidget(m_input, 1);
        row->addWidget(m_count);
        row->addWidget(prev);
        row->addWidget(next);
        row->addWidget(close);

        connect(m_input, &QLineEdit::textChanged, this, &FindBar::queryChanged);
        connect(prev, &QToolButton::clicked, this, &FindBar::prevRequested);
        connect(next, &QToolButton::clicked, this, &FindBar::nextRequested);
        connect(close, &QToolButton::clicked, this, &FindBar::closed);
    }

    void FindBar::activate()
    {
        show();
        m_input->setFocus();
        m_input->selectAll();
    }

    QString FindBar::query() const
    {
        return m_input->text();
    }

    void FindBar::setResultLabel(int current, int total)
    {
        if (total > 0) {
            m_count->setText(QString("%1 / %2").arg(current).arg(total));
        } else if (m_input->text().isEmpty()) {
            m_count->clear();
        } else {
            m_count->setText("No results");
        }
    }

    bool FindBar::eventFilter(QObject* watched, QEvent* event)
    {
        if ((watched == m_input) && (event->type() == QEvent::KeyPress)) {
            QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
            switch (key_event->key()) {
            case Qt::Key_Escape:
                emit closed();
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if ((key_event->modifiers() & Qt::ShiftModifier) != 0) {
                    emit prevRequested();
                } else {
                    emit nextRequested();
                }
                return true;
            default:
                break;
            }
        }
        return QWidget::eventFilter(watched, event);
    }
}
