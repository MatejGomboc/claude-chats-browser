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
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

using namespace ChatsBrowser;

namespace
{
    const QString SENTINEL = QStringLiteral("00000000-0000-4000-8000-000000000000");
    const QString CONVERSATION = QStringLiteral("conv-0001");
    // Enough messages to force several streaming chunks past the first; kept modest so the
    // whole test (which builds and tears down real widgets) stays quick in a debug build.
    constexpr int MESSAGE_COUNT = 120;

    //! Builds a representative assistant message (thinking + text + tool blocks).
    QByteArray messageJson(const QString& sender)
    {
        QJsonArray content;

        QJsonObject thinking;
        thinking.insert("type", "thinking");
        thinking.insert("thinking", QString("reasoning ").repeated(30));
        content.append(thinking);

        QJsonObject text;
        text.insert("type", "text");
        text.insert("text", QString("This is a **paragraph** of reply text. ").repeated(12));
        content.append(text);

        QJsonObject tool_use;
        tool_use.insert("type", "tool_use");
        tool_use.insert("name", "bash");
        QJsonObject input;
        input.insert("command", QString("echo hello; ").repeated(15));
        tool_use.insert("input", input);
        content.append(tool_use);

        QJsonObject tool_result;
        tool_result.insert("type", "tool_result");
        tool_result.insert("content", QString("output line\n").repeated(20));
        content.append(tool_result);

        QJsonObject message;
        message.insert("sender", sender);
        message.insert("content", content);
        return QJsonDocument(message).toJson(QJsonDocument::Compact);
    }
}

class TestReaderPerformance : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void openingLargeConversationStaysResponsive();
    void cleanupTestCase();
};

void TestReaderPerformance::initTestCase()
{
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", "main");
    database.setDatabaseName(":memory:");
    QVERIFY2(database.open(), "failed to open in-memory database");

    QSqlQuery create(database);
    QVERIFY(create.exec("CREATE TABLE messages ("
                        " uuid TEXT PRIMARY KEY, conversation_uuid TEXT, parent_uuid TEXT,"
                        " sender TEXT, created_at TEXT, text TEXT, raw_json TEXT)"));

    QVERIFY(database.transaction());
    QSqlQuery insert(database);
    insert.prepare("INSERT INTO messages (uuid, conversation_uuid, parent_uuid, sender, created_at, raw_json)"
                   " VALUES (?, ?, ?, ?, ?, ?)");
    QString parent = SENTINEL;
    for (int i = 0; i < MESSAGE_COUNT; ++i) {
        const QString uuid = QString("m%1").arg(i, 5, 10, QChar('0'));
        const QString sender = (i % 2 == 0) ? "human" : "assistant";
        insert.addBindValue(uuid);
        insert.addBindValue(CONVERSATION);
        insert.addBindValue(parent);
        insert.addBindValue(sender);
        insert.addBindValue(QString("2026-01-01T00:%1:00Z").arg(i, 2, 10, QChar('0')));
        insert.addBindValue(QString::fromUtf8(messageJson(sender)));
        QVERIFY(insert.exec());
        parent = uuid;
    }
    QVERIFY(database.commit());
}

void TestReaderPerformance::openingLargeConversationStaysResponsive()
{
    // Owned by the event loop, not this stack frame, so its widget tree is torn down the
    // normal (deleteLater) way rather than synchronously at scope exit.
    ConversationReader* reader = new ConversationReader;
    reader->resize(1000, 700);
    reader->show();
    QTest::qWait(50); // let the initial layout settle

    QSignalSpy progress_spy(reader, &ConversationReader::renderProgressChanged);
    QSignalSpy finished_spy(reader, &ConversationReader::renderFinished);

    // The synchronous cost of opening the conversation: this is what used to freeze the UI
    // for seconds. With chunked rendering only the first small chunk is built here.
    QElapsedTimer timer;
    timer.start();
    reader->showConversation(CONVERSATION);
    const qint64 synchronous_ms = timer.elapsed();
    qInfo() << "synchronous open of" << MESSAGE_COUNT << "messages:" << synchronous_ms << "ms";

    // Opening must not block for seconds — the whole point of the change.
    QVERIFY2(synchronous_ms < 1500, qPrintable(QString("synchronous open took %1 ms").arg(synchronous_ms)));

    // The render is deferred across the event loop: the first chunk reported progress and
    // the conversation is not yet fully built when showConversation() returns. (finished_spy
    // holds only the initial clear at this point, not a completed render.)
    QVERIFY2(!progress_spy.isEmpty(), "a large conversation should render in progress-reporting chunks");
    QCOMPARE(finished_spy.count(), 1);
    const int progress_after_sync = progress_spy.count();

    // Yielding to the event loop streams more in — proof the work is spread out across
    // event-loop turns rather than done in one blocking call. This, together with the small
    // synchronous cost above, is what keeps the UI responsive while a big chat loads.
    QTest::qWait(250);
    QVERIFY2(progress_spy.count() > progress_after_sync, "rendering should continue on the event loop");

    // Cancel the still-streaming render (bumps the generation so queued chunks bail), then
    // tear down through the event loop. Pending chunk timers auto-disconnect with their
    // context object, so this must not crash.
    reader->clearConversation();
    reader->deleteLater();
    QTest::qWait(50);
}

void TestReaderPerformance::cleanupTestCase()
{
    {
        QSqlDatabase database = QSqlDatabase::database("main");
        database.close();
    }
    QSqlDatabase::removeDatabase("main");
}

QTEST_MAIN(TestReaderPerformance)
#include "tst_reader_performance.moc"
