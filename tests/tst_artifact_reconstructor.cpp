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

#include "artifact_reconstructor.hpp"
#include <QJsonObject>
#include <QList>
#include <QtTest>

using ChatsBrowser::ArtifactReconstructor;
using ChatsBrowser::ReconstructedArtifact;

namespace
{
    QJsonObject op(const QString& command, const QString& id, const QJsonObject& extra = {})
    {
        QJsonObject o = extra;
        o.insert("command", command);
        o.insert("id", id);
        return o;
    }
}

class TestArtifactReconstructor : public QObject {
    Q_OBJECT

private slots:
    //! create then a str_replace update yields the patched final content.
    void createThenUpdate()
    {
        const QList<QJsonObject> ops = {op("create", "a", {{"title", "Doc"}, {"type", "text/markdown"}, {"content", "Hello world"}}),
            op("update", "a", {{"old_str", "world"}, {"new_str", "there"}})};

        const QList<ReconstructedArtifact> result = ArtifactReconstructor::replay(ops);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.at(0).id, QString("a"));
        QCOMPARE(result.at(0).title, QString("Doc"));
        QCOMPARE(result.at(0).type, QString("text/markdown"));
        QCOMPARE(result.at(0).content, QString("Hello there"));
        QCOMPARE(result.at(0).revisions, 2);
    }

    //! Only the first occurrence is replaced (str_replace targets a unique anchor).
    void updateReplacesFirstOccurrenceOnly()
    {
        const QList<QJsonObject> ops = {op("create", "a", {{"content", "x x x"}}), op("update", "a", {{"old_str", "x"}, {"new_str", "y"}})};

        const QList<ReconstructedArtifact> result = ArtifactReconstructor::replay(ops);
        QCOMPARE(result.at(0).content, QString("y x x"));
    }

    //! An update with no old_str appends its new_str.
    void updateWithoutOldStrAppends()
    {
        const QList<QJsonObject> ops = {op("create", "a", {{"content", "line1"}}), op("update", "a", {{"new_str", "\nline2"}})};

        const QList<ReconstructedArtifact> result = ArtifactReconstructor::replay(ops);
        QCOMPARE(result.at(0).content, QString("line1\nline2"));
    }

    //! rewrite replaces the whole content and can change metadata.
    void rewriteReplacesContent()
    {
        const QList<QJsonObject> ops = {
            op("create", "a", {{"content", "old"}, {"language", "python"}}), op("rewrite", "a", {{"content", "brand new"}, {"language", "cpp"}})};

        const QList<ReconstructedArtifact> result = ArtifactReconstructor::replay(ops);
        QCOMPARE(result.at(0).content, QString("brand new"));
        QCOMPARE(result.at(0).language, QString("cpp"));
    }

    //! Distinct ids become distinct artifacts, kept in first-appearance order.
    void multipleArtifactsKeepOrder()
    {
        const QList<QJsonObject> ops = {
            op("create", "second", {{"content", "2"}}), op("create", "first", {{"content", "1"}}), op("update", "second", {{"old_str", "2"}, {"new_str", "two"}})};

        const QList<ReconstructedArtifact> result = ArtifactReconstructor::replay(ops);
        QCOMPARE(result.size(), 2);
        QCOMPARE(result.at(0).id, QString("second"));
        QCOMPARE(result.at(0).content, QString("two"));
        QCOMPARE(result.at(1).id, QString("first"));
    }

    //! An update whose old_str is absent from the content leaves it unchanged.
    void updateWithMissingAnchorIsNoOp()
    {
        const QList<QJsonObject> ops = {op("create", "a", {{"content", "hello"}}), op("update", "a", {{"old_str", "zzz"}, {"new_str", "!"}})};

        const QList<ReconstructedArtifact> result = ArtifactReconstructor::replay(ops);
        QCOMPARE(result.at(0).content, QString("hello"));
    }

    //! Ops without an id are ignored rather than crashing.
    void opsWithoutIdAreSkipped()
    {
        const QList<QJsonObject> ops = {op("create", "", {{"content", "orphan"}}), op("create", "a", {{"content", "kept"}})};

        const QList<ReconstructedArtifact> result = ArtifactReconstructor::replay(ops);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.at(0).id, QString("a"));
    }
};

QTEST_MAIN(TestArtifactReconstructor)
#include "tst_artifact_reconstructor.moc"
