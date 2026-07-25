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
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtLogging>

namespace ChatsBrowser
{
    QList<ReconstructedArtifact> ArtifactReconstructor::replay(const QList<QJsonObject>& artifact_inputs)
    {
        QList<ReconstructedArtifact> artifacts;
        QHash<QString, int> index_of; //!< artifact id → position in `artifacts`.

        const auto ensure = [&](const QString& id) -> ReconstructedArtifact& {
            auto it = index_of.find(id);
            if (it != index_of.end()) {
                return artifacts[it.value()];
            }
            ReconstructedArtifact fresh;
            fresh.id = id;
            index_of.insert(id, static_cast<int>(artifacts.size()));
            artifacts.append(fresh);
            return artifacts.last();
        };

        for (const QJsonObject& input : artifact_inputs) {
            const QString id = input.value("id").toString();
            if (id.isEmpty()) {
                continue;
            }
            const QString command = input.value("command").toString();
            ReconstructedArtifact& artifact = ensure(id);
            artifact.revisions++;

            // Metadata may be set on any command; the most recent non-empty value wins.
            if (input.contains("title")) {
                artifact.title = input.value("title").toString();
            }
            if (input.contains("type")) {
                artifact.type = input.value("type").toString();
            }
            if (input.contains("language")) {
                artifact.language = input.value("language").toString();
            }

            if ((command == "create") || (command == "rewrite")) {
                artifact.content = input.value("content").toString();
            } else if (command == "update") {
                const QString old_str = input.value("old_str").toString();
                const QString new_str = input.value("new_str").toString();
                if (old_str.isEmpty()) {
                    // No anchor to replace: treat it as an append (the shape a few updates take).
                    artifact.content += new_str;
                } else {
                    // str_replace targets a single unique occurrence — replace only the first.
                    const int at = artifact.content.indexOf(old_str);
                    if (at >= 0) {
                        artifact.content.replace(at, old_str.length(), new_str);
                    }
                }
            }
        }

        return artifacts;
    }

    QList<ReconstructedArtifact> ArtifactReconstructor::reconstruct(const QString& conversation_uuid)
    {
        QSqlQuery query(QSqlDatabase::database("main"));
        query.prepare("SELECT raw_json FROM messages WHERE conversation_uuid = ? ORDER BY created_at, rowid");
        query.addBindValue(conversation_uuid);
        if (!query.exec()) {
            qWarning() << "Artifact query failed:" << query.lastError().text();
            return {};
        }

        QList<QJsonObject> inputs;
        while (query.next()) {
            const QJsonDocument document = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
            if (!document.isObject()) {
                continue;
            }
            const QJsonArray blocks = document.object().value("content").toArray();
            for (const QJsonValue& block_value : blocks) {
                const QJsonObject block = block_value.toObject();
                if ((block.value("type").toString() == "tool_use") && (block.value("name").toString() == "artifacts")) {
                    inputs.append(block.value("input").toObject());
                }
            }
        }

        return replay(inputs);
    }
}
