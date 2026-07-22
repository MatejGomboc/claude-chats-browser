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

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace ChatsBrowser
{
    //! One message on the currently-selected path, with the branch state at its fork.
    struct PathNode {
        QString uuid; //!< The message this node refers to.
        QString fork_key; //!< Parent key whose children are the sibling branches.
        int branch_index{0}; //!< Zero-based position of this message among its siblings.
        int branch_count{1}; //!< Number of sibling branches at this fork (1 = no fork).
    };

    /*!
        The reply tree of one conversation.

        Editing a prompt or retrying a reply gives a parent message several children;
        each is a branch. This class builds the parent→children structure and produces a
        single linear path through it, following a selectable child at each fork (default:
        the newest). It is pure data — no Qt widgets — so it is unit-testable in isolation.
    */
    class ConversationTree {
    public:
        //! Synthetic parent key grouping the conversation's root message(s).
        static const QString ROOT_KEY;

        //! Rebuilds from messages given in chronological order as (uuid, parent_uuid) pairs.
        void build(const QList<QPair<QString, QString>>& ordered_messages);

        //! Drops all structure and branch selections.
        void clear();

        //! True when no messages have been added.
        [[nodiscard]] bool isEmpty() const;

        //! The linear path from root, following the selected (default newest) child at each fork.
        [[nodiscard]] QList<PathNode> currentPath() const;

        //! Selects which sibling to follow at a fork; index is clamped to range.
        void selectBranch(const QString& fork_key, int index);

    private:
        QHash<QString, QStringList> m_children; //!< parent key → child uuids, chronological.
        QHash<QString, int> m_selection; //!< fork key → chosen child index.
        int m_message_count{0};
    };
}
