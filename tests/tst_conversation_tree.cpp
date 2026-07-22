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
#include <QtTest>

using namespace ChatsBrowser;

namespace
{
    // The real export uses this sentinel as a root message's parent; no message has it
    // as its own uuid, so such messages become roots.
    const QString SENTINEL = QStringLiteral("00000000-0000-4000-8000-000000000000");

    QStringList pathUuids(const ConversationTree& tree)
    {
        QStringList uuids;
        for (const PathNode& node : tree.currentPath()) {
            uuids.append(node.uuid);
        }
        return uuids;
    }
}

class TestConversationTree : public QObject {
    Q_OBJECT

private slots:
    void emptyTree();
    void linearConversation();
    void singleForkDefaultsToNewest();
    void switchingForkChangesTailOnly();
    void switchingLowerForkLeavesUpperUntouched();
    void selectionClampsToRange();
    void multipleRoots();
    void selfParentDoesNotHang();
};

void TestConversationTree::emptyTree()
{
    ConversationTree tree;
    QVERIFY(tree.isEmpty());
    QVERIFY(tree.currentPath().isEmpty());
}

void TestConversationTree::linearConversation()
{
    ConversationTree tree;
    tree.build({{"a", SENTINEL}, {"b", "a"}, {"c", "b"}});

    QVERIFY(!tree.isEmpty());
    QCOMPARE(pathUuids(tree), QStringList({"a", "b", "c"}));
    for (const PathNode& node : tree.currentPath()) {
        QCOMPARE(node.branch_count, 1);
        QCOMPARE(node.branch_index, 0);
    }
}

void TestConversationTree::singleForkDefaultsToNewest()
{
    ConversationTree tree;
    // "a" has two replies: b1 then (newer) b2 — a retry.
    tree.build({{"a", SENTINEL}, {"b1", "a"}, {"b2", "a"}});

    const QList<PathNode> path = tree.currentPath();
    QCOMPARE(path.size(), 2);
    QCOMPARE(path.at(0).uuid, QString("a"));
    QCOMPARE(path.at(1).uuid, QString("b2")); // newest sibling is the default
    QCOMPARE(path.at(1).fork_key, QString("a"));
    QCOMPARE(path.at(1).branch_count, 2);
    QCOMPARE(path.at(1).branch_index, 1);
}

void TestConversationTree::switchingForkChangesTailOnly()
{
    ConversationTree tree;
    tree.build({{"a", SENTINEL}, {"b1", "a"}, {"b2", "a"}, {"c1", "b1"}, {"c2", "b2"}});

    // Default follows the newest at the fork: a -> b2 -> c2.
    QCOMPARE(pathUuids(tree), QStringList({"a", "b2", "c2"}));

    // Switch the fork at "a" to the older branch: a -> b1 -> c1.
    tree.selectBranch("a", 0);
    QCOMPARE(pathUuids(tree), QStringList({"a", "b1", "c1"}));
}

void TestConversationTree::switchingLowerForkLeavesUpperUntouched()
{
    ConversationTree tree;
    // Upper fork under "a" (u1, u2); the newest (u2) continues to "m"; lower fork under "m".
    tree.build({{"a", SENTINEL}, {"u1", "a"}, {"u2", "a"}, {"m", "u2"}, {"low1", "m"}, {"low2", "m"}});

    QCOMPARE(pathUuids(tree), QStringList({"a", "u2", "m", "low2"}));

    // Switching the LOWER fork must not disturb the upper part of the path.
    tree.selectBranch("m", 0);
    QCOMPARE(pathUuids(tree), QStringList({"a", "u2", "m", "low1"}));
}

void TestConversationTree::selectionClampsToRange()
{
    ConversationTree tree;
    tree.build({{"a", SENTINEL}, {"b1", "a"}, {"b2", "a"}});

    tree.selectBranch("a", -5); // below range -> clamps to 0
    QCOMPARE(tree.currentPath().at(1).uuid, QString("b1"));

    tree.selectBranch("a", 99); // above range -> clamps to last
    QCOMPARE(tree.currentPath().at(1).uuid, QString("b2"));

    tree.selectBranch("does-not-exist", 0); // unknown fork is a no-op
    QCOMPARE(tree.currentPath().at(1).uuid, QString("b2"));
}

void TestConversationTree::multipleRoots()
{
    ConversationTree tree;
    // Some exports have several root messages (the sentinel has multiple children).
    tree.build({{"r1", SENTINEL}, {"r2", SENTINEL}});

    const QList<PathNode> path = tree.currentPath();
    QCOMPARE(path.size(), 1);
    QCOMPARE(path.at(0).uuid, QString("r2")); // newest root by default
    QCOMPARE(path.at(0).branch_count, 2);
    QCOMPARE(path.at(0).fork_key, ConversationTree::ROOT_KEY);

    tree.selectBranch(ConversationTree::ROOT_KEY, 0);
    QCOMPARE(tree.currentPath().at(0).uuid, QString("r1"));
}

void TestConversationTree::selfParentDoesNotHang()
{
    ConversationTree tree;
    // Malformed: a message parented to itself must not cause an infinite walk.
    tree.build({{"x", "x"}, {"a", SENTINEL}, {"b", "a"}});

    const QList<PathNode> path = tree.currentPath();
    // "x" is not reachable from the root; the real chain still resolves and terminates.
    QCOMPARE(pathUuids(tree), QStringList({"a", "b"}));
    Q_UNUSED(path);
}

QTEST_MAIN(TestConversationTree)
#include "tst_conversation_tree.moc"
