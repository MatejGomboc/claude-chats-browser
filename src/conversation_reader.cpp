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

#include "conversation_reader.hpp"
#include "message_widget.hpp"
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVBoxLayout>
#include <QtLogging>

namespace ChatsBrowser
{
    ConversationReader::ConversationReader(QWidget* parent) :
        QScrollArea(parent)
    {
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);

        m_container = new QWidget(this);
        m_container->setObjectName("readerContainer");

        m_layout = new QVBoxLayout(m_container);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(0);

        m_placeholder = new QLabel(m_container);
        m_placeholder->setObjectName("readerPlaceholder");
        m_placeholder->setAlignment(Qt::AlignCenter);
        m_placeholder->setWordWrap(true);
        m_layout->addWidget(m_placeholder);

        m_layout->addStretch();

        setWidget(m_container);
        showPlaceholder("Select a conversation to read it here.");
    }

    void ConversationReader::clearMessages()
    {
        // Remove every item except the trailing stretch (always the last item).
        while (m_layout->count() > 1) {
            QLayoutItem* item = m_layout->takeAt(0);
            if (item->widget() != nullptr) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        m_placeholder = nullptr;
    }

    void ConversationReader::showPlaceholder(const QString& text)
    {
        clearMessages();
        m_placeholder = new QLabel(m_container);
        m_placeholder->setObjectName("readerPlaceholder");
        m_placeholder->setAlignment(Qt::AlignCenter);
        m_placeholder->setWordWrap(true);
        m_placeholder->setText(text);
        m_layout->insertWidget(0, m_placeholder);
    }

    void ConversationReader::showConversation(const QString& conversation_uuid)
    {
        m_conversation_uuid = conversation_uuid;

        if (conversation_uuid.isEmpty()) {
            clearConversation();
            return;
        }

        QSqlDatabase database = QSqlDatabase::database("main");
        QSqlQuery query(database);
        query.prepare("SELECT raw_json FROM messages WHERE conversation_uuid = ? ORDER BY created_at, rowid");
        query.addBindValue(conversation_uuid);
        if (!query.exec()) {
            qWarning() << "Message query failed:" << query.lastError().text();
            showPlaceholder("Could not load this conversation.");
            return;
        }

        clearMessages();

        int insert_position = 0;
        bool any_content = false;
        while (query.next()) {
            QJsonDocument document = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
            if (!document.isObject()) {
                continue;
            }

            MessageWidget* message_widget = new MessageWidget(document.object(), m_container);
            if (!message_widget->hasRenderedContent()) {
                message_widget->deleteLater();
                continue;
            }

            if (any_content) {
                QFrame* divider = new QFrame(m_container);
                divider->setObjectName("messageDivider");
                divider->setFrameShape(QFrame::HLine);
                m_layout->insertWidget(insert_position, divider);
                insert_position++;
            }

            m_layout->insertWidget(insert_position, message_widget);
            insert_position++;
            any_content = true;
        }

        if (!any_content) {
            showPlaceholder("This conversation was deleted; its content is no longer available.");
            return;
        }

        updateContentHeight();
        verticalScrollBar()->setValue(0);
    }

    void ConversationReader::clearConversation()
    {
        m_conversation_uuid.clear();
        showPlaceholder("Select a conversation to read it here.");
    }

    void ConversationReader::resizeEvent(QResizeEvent* event)
    {
        QScrollArea::resizeEvent(event);
        updateContentHeight();
    }

    void ConversationReader::updateContentHeight()
    {
        // widgetResizable sizes the content to its unconstrained sizeHint, which is too short
        // for word-wrapped messages. Pin a minimum height computed at the real (viewport)
        // width so heightForWidth is honoured and messages never overlap.
        QLayout* content_layout = m_container->layout();
        if ((content_layout == nullptr) || (!content_layout->hasHeightForWidth())) {
            return;
        }
        m_container->setMinimumHeight(content_layout->heightForWidth(viewport()->width()));
    }
}
