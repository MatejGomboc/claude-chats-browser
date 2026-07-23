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

#include "conversation_tree.hpp"
#include <QSet>

namespace ChatsBrowser
{
    // A control-character prefix that no real UUID can contain, so it never collides.
    const QString ConversationTree::ROOT_KEY = QStringLiteral("\x01root");

    void ConversationTree::clear()
    {
        m_children.clear();
        m_selection.clear();
        m_message_count = 0;
    }

    bool ConversationTree::isEmpty() const
    {
        return m_message_count == 0;
    }

    void ConversationTree::build(const QList<QPair<QString, QString>>& ordered_messages)
    {
        clear();

        QSet<QString> known;
        for (const QPair<QString, QString>& message : ordered_messages) {
            known.insert(message.first);
        }

        // A message whose parent is not itself a message here is a root (its parent is the
        // sentinel or points outside the conversation); group those under ROOT_KEY.
        for (const QPair<QString, QString>& message : ordered_messages) {
            const QString& uuid = message.first;
            const QString& parent = message.second;
            const QString key = known.contains(parent) ? parent : ROOT_KEY;
            m_children[key].append(uuid);
            m_message_count++;
        }
    }

    void ConversationTree::selectBranch(const QString& fork_key, int index)
    {
        const QStringList children = m_children.value(fork_key);
        if (children.isEmpty()) {
            return;
        }
        m_selection[fork_key] = qBound(0, index, static_cast<int>(children.size()) - 1);
    }

    QList<PathNode> ConversationTree::currentPath() const
    {
        QList<PathNode> path;
        QSet<QString> visited; // defends against malformed cycles
        QString parent = ROOT_KEY;

        while (m_children.contains(parent)) {
            const QStringList& children = m_children.value(parent);
            if (children.isEmpty()) {
                break;
            }
            const int count = static_cast<int>(children.size());
            const int index = qBound(0, m_selection.value(parent, count - 1), count - 1);
            const QString node = children.at(index);
            if (visited.contains(node)) {
                break;
            }
            visited.insert(node);
            path.append({node, parent, index, count});
            parent = node;
        }

        return path;
    }
}
