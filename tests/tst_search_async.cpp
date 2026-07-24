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

#include "conversation_list_model.hpp"
#include "database.hpp"
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace ChatsBrowser;

class TestSearchAsync : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void searchRunsOffTheUiThread();
    void cleanupTestCase();

private:
    QTemporaryDir m_dir;
};

void TestSearchAsync::initTestCase()
{
    QVERIFY(m_dir.isValid());

    QString error;
    QSqlDatabase database = Database::open("main", m_dir.filePath("chats.db"), &error);
    QVERIFY2(database.isValid(), qPrintable(error));

    QVERIFY(database.transaction());
    QSqlQuery conversation(database);
    conversation.prepare("INSERT INTO conversations (uuid, name, created_at, updated_at, message_count, has_content)"
                         " VALUES (?, ?, ?, ?, ?, 1)");
    QSqlQuery message(database);
    message.prepare("INSERT INTO messages (uuid, conversation_uuid, parent_uuid, sender, created_at, text, raw_json)"
                    " VALUES (?, ?, '', 'assistant', ?, ?, '{}')");
    for (int c = 0; c < 400; ++c) {
        const QString conversation_uuid = QString("c%1").arg(c, 4, 10, QChar('0'));
        conversation.addBindValue(conversation_uuid);
        conversation.addBindValue(QString("Conversation %1").arg(c));
        conversation.addBindValue("2026-01-01T00:00:00Z");
        conversation.addBindValue("2026-01-01T00:00:00Z");
        conversation.addBindValue(3);
        QVERIFY(conversation.exec());
        for (int m = 0; m < 3; ++m) {
            message.addBindValue(QString("%1-m%2").arg(conversation_uuid).arg(m));
            message.addBindValue(conversation_uuid);
            message.addBindValue("2026-01-01T00:00:00Z");
            message.addBindValue("the quick brown fox jumps over the lazy dog the the the");
            QVERIFY(message.exec());
        }
    }
    QVERIFY(database.commit());
}

void TestSearchAsync::searchRunsOffTheUiThread()
{
    ConversationListModel model;
    QSignalSpy started(&model, &ConversationListModel::searchStarted);
    QSignalSpy finished(&model, &ConversationListModel::searchFinished);

    // "the" matches every message — the worst case that used to freeze the UI.
    QElapsedTimer timer;
    timer.start();
    model.setSearchFilter("the");
    const qint64 dispatch_ms = timer.elapsed();
    qInfo() << "setSearchFilter returned in" << dispatch_ms << "ms";

    // The call must return immediately: the query is dispatched to a worker thread, and its
    // result is delivered later via the event loop — so it cannot block the UI thread.
    QVERIFY2(dispatch_ms < 100, qPrintable(QString("setSearchFilter blocked for %1 ms").arg(dispatch_ms)));
    QCOMPARE(started.count(), 1);
    QCOMPARE(finished.count(), 0); // not completed synchronously

    // The results arrive on the UI thread once the event loop runs.
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
    QVERIFY2(model.rowCount() > 0, "the search should match the seeded conversations");
}

void TestSearchAsync::cleanupTestCase()
{
    {
        QSqlDatabase database = QSqlDatabase::database("main");
        database.close();
    }
    QSqlDatabase::removeDatabase("main");
}

QTEST_MAIN(TestSearchAsync)
#include "tst_search_async.moc"
