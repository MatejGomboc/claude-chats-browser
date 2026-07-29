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

#include "conversation_exporter.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

using ChatsBrowser::ConversationExporter;
using ChatsBrowser::ConversationInfo;

namespace
{
    QJsonObject textMessage(const QString& sender, const QString& text, const QString& created_at = "2026-01-01T10:00:00.000000Z")
    {
        QJsonObject message;
        message.insert("sender", sender);
        message.insert("created_at", created_at);
        message.insert("content", QJsonArray{QJsonObject{{"type", "text"}, {"text", text}}});
        return message;
    }

    ConversationInfo info()
    {
        ConversationInfo i;
        i.uuid = "abc-123";
        i.name = "Test chat";
        i.summary = "a summary";
        i.created_at = "2026-01-01T09:00:00.000000Z";
        i.updated_at = "2026-01-02T09:00:00.000000Z";
        return i;
    }
}

class TestConversationExporter : public QObject {
    Q_OBJECT

private slots:
    //! Markdown carries the title, sender headers and message text in order.
    void markdownBasics()
    {
        const QString md = ConversationExporter::toMarkdown(info(), {textMessage("human", "Hello there"), textMessage("assistant", "General reply")});

        QVERIFY(md.startsWith("# Test chat\n"));
        QVERIFY(md.contains("## You"));
        QVERIFY(md.contains("Hello there"));
        QVERIFY(md.contains("## Claude"));
        QVERIFY(md.contains("General reply"));
        QVERIFY(md.indexOf("Hello there") < md.indexOf("General reply"));
    }

    //! Code fences inside message text survive verbatim.
    void markdownPreservesFences()
    {
        const QString md = ConversationExporter::toMarkdown(info(), {textMessage("assistant", "Look:\n\n```cpp\nint x = 1;\n```")});
        QVERIFY(md.contains("```cpp\nint x = 1;\n```"));
    }

    //! Attachments are noted by name; thinking-only messages are skipped entirely.
    void markdownNotesAttachmentsAndSkipsThinkingOnly()
    {
        QJsonObject thinking_only;
        thinking_only.insert("sender", "assistant");
        thinking_only.insert("created_at", "2026-01-01T10:00:00.000000Z");
        thinking_only.insert("content", QJsonArray{QJsonObject{{"type", "thinking"}, {"thinking", "private reasoning"}}});

        QJsonObject with_attachment = textMessage("human", "see attached");
        with_attachment.insert("attachments", QJsonArray{QJsonObject{{"file_name", "notes.txt"}}});

        const QString md = ConversationExporter::toMarkdown(info(), {thinking_only, with_attachment});
        QVERIFY(!md.contains("private reasoning"));
        QVERIFY(md.contains("📎 Attachment: notes.txt"));
        QVERIFY(md.contains("see attached"));
    }

    //! Tool calls leave a one-line trace, in place, without their input.
    void markdownTracesToolCalls()
    {
        QJsonObject message;
        message.insert("sender", "assistant");
        message.insert("created_at", "2026-01-01T10:00:00.000000Z");
        message.insert("content",
            QJsonArray{
                QJsonObject{{"type", "text"}, {"text", "Let me look."}},
                QJsonObject{{"type", "tool_use"}, {"name", "web_fetch"}, {"input", QJsonObject{{"url", "https://example.com/page"}}}},
                QJsonObject{{"type", "tool_use"}, {"name", "conversation_search"}, {"input", QJsonObject{{"query", "a private query"}}}},
                QJsonObject{{"type", "tool_result"}, {"content", "an enormous payload"}},
                QJsonObject{{"type", "text"}, {"text", "Found it."}},
            });

        const QString md = ConversationExporter::toMarkdown(info(), {message});

        QVERIFY(md.contains("🔧 Tool: web_fetch — https://example.com/page"));
        QVERIFY(md.contains("🔧 Tool: conversation_search"));
        // Inputs other than the URL, and tool results, never reach the file.
        QVERIFY(!md.contains("a private query"));
        QVERIFY(!md.contains("an enormous payload"));
        // Order is preserved: prose, traces, then the prose that followed them.
        QVERIFY(md.indexOf("Let me look.") < md.indexOf("web_fetch"));
        QVERIFY(md.indexOf("conversation_search") < md.indexOf("Found it."));
    }

    //! A message that is nothing but a tool call still shows its trace.
    void markdownKeepsToolOnlyMessages()
    {
        QJsonObject tool_only;
        tool_only.insert("sender", "assistant");
        tool_only.insert("created_at", "2026-01-01T10:00:00.000000Z");
        tool_only.insert("content", QJsonArray{QJsonObject{{"type", "tool_use"}, {"name", "artifacts"}}});

        const QString md = ConversationExporter::toMarkdown(info(), {tool_only});
        QVERIFY(md.contains("## Claude"));
        QVERIFY(md.contains("🔧 Tool: artifacts"));
    }

    //! A non-HTTP or unnamed tool degrades gracefully rather than leaking input.
    void toolTraceHandlesOddBlocks()
    {
        const QJsonObject file_url{{"type", "tool_use"}, {"name", "reader"}, {"input", QJsonObject{{"url", "file:///c:/secrets.txt"}}}};
        QCOMPARE(ConversationExporter::toolTrace(file_url), QString("> 🔧 Tool: reader\n"));

        const QJsonObject unnamed{{"type", "tool_use"}};
        QCOMPARE(ConversationExporter::toolTrace(unnamed), QString("> 🔧 Tool: (unnamed)\n"));
    }

    //! Legacy messages with only the top-level text field still export.
    void markdownLegacyTextFallback()
    {
        QJsonObject legacy;
        legacy.insert("sender", "human");
        legacy.insert("created_at", "2024-10-01T10:00:00.000000Z");
        legacy.insert("text", "old style message");
        legacy.insert("content", QJsonArray{});

        const QString md = ConversationExporter::toMarkdown(info(), {legacy});
        QVERIFY(md.contains("old style message"));
    }

    //! The JSON envelope matches the export shape and keeps messages verbatim.
    void jsonEnvelope()
    {
        const QJsonObject message = textMessage("human", "hi");
        const QJsonObject out = ConversationExporter::toExportJson(info(), {message});

        QCOMPARE(out.value("uuid").toString(), QString("abc-123"));
        QCOMPARE(out.value("name").toString(), QString("Test chat"));
        QCOMPARE(out.value("summary").toString(), QString("a summary"));
        QCOMPARE(out.value("chat_messages").toArray().size(), 1);
        QCOMPARE(out.value("chat_messages").toArray().at(0).toObject(), message);
    }

    //! Suggested file names are filesystem-safe and never empty.
    void suggestedNames()
    {
        ConversationInfo i = info();
        i.name = "What? A/B \"test\": <yes>|no";
        QCOMPARE(ConversationExporter::suggestedBaseName(i), QString("What_ A_B _test__ _yes__no"));

        i.name.clear();
        QCOMPARE(ConversationExporter::suggestedBaseName(i), QString("abc-123"));
    }
};

QTEST_MAIN(TestConversationExporter)
#include "tst_conversation_exporter.moc"
