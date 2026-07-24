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

#include <QJsonObject>
#include <QList>
#include <QString>

namespace ChatsBrowser
{
    //! One artifact rebuilt from its create / update / rewrite tool calls.
    struct ReconstructedArtifact {
        QString id; //!< The artifact's stable id across its versions.
        QString title; //!< Human title (last one set wins).
        QString type; //!< MIME-ish type, e.g. "text/markdown", "application/vnd.ant.code".
        QString language; //!< Source language for code artifacts (may be empty).
        QString content; //!< Final content after replaying every op in order.
        int revisions{0}; //!< How many tool calls touched this artifact (1 = just created).
    };

    /*!
        Rebuilds the artifacts of a conversation by replaying the `artifacts` tool calls.

        claude.ai streams artifacts as a sequence of tool calls sharing an `id`: a `create`
        (or `rewrite`) carries the full content, while `update` is a string replacement
        (`old_str` → `new_str`). Replaying them in message order yields each artifact's final
        state — which is what a reader wants to see, rather than the raw diff calls.
    */
    class ArtifactReconstructor {
    public:
        //! Reconstructs every artifact in the conversation, in first-appearance order.
        //! Reads the "main" database connection.
        [[nodiscard]] static QList<ReconstructedArtifact> reconstruct(const QString& conversation_uuid);

        //! Pure replay of ordered `artifacts` tool-call inputs — the core of reconstruct(),
        //! separated out so it can be tested without a database.
        [[nodiscard]] static QList<ReconstructedArtifact> replay(const QList<QJsonObject>& artifact_inputs);
    };
}
