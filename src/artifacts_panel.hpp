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

#include "artifact_reconstructor.hpp"
#include <QDialog>
#include <QList>
#include <QString>

QT_BEGIN_NAMESPACE
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QScrollArea;
class QStackedWidget;
class QTextBrowser;
class QToolButton;
QT_END_NAMESPACE

namespace ChatsBrowser
{
    /*!
        A non-modal window listing a conversation's artifacts, each rebuilt to its final
        state from its create / update / rewrite tool calls.

        Markdown and HTML render as rich text, SVG renders as an image, and everything
        (plus code, React and Mermaid, which have no in-app preview) is viewable as
        highlighted source. Each artifact can be copied or exported to a file.
    */
    class ArtifactsPanel : public QDialog {
        Q_OBJECT

    public:
        ArtifactsPanel(const QString& conversation_uuid, const QString& conversation_title, QWidget* parent = nullptr);

    private:
        //! Which stacked page shows the currently-selected artifact.
        enum Page {
            PageRich = 0, //!< QTextBrowser (markdown / HTML).
            PageSvg = 1, //!< Rendered SVG image.
            PageSource = 2 //!< Highlighted plain-text source.
        };

        void showArtifact(int index);
        void copyCurrent();
        void exportCurrent();
        void toggleSource();

        //! The rendered page a type maps to, or PageSource if it has no in-app preview.
        [[nodiscard]] static Page renderedPageFor(const QString& type);

        //! A short human label for an artifact type ("Markdown", "Code · python", …).
        [[nodiscard]] static QString typeLabel(const ReconstructedArtifact& artifact);

        //! A sensible export filename (with extension) for an artifact.
        [[nodiscard]] static QString suggestedFileName(const ReconstructedArtifact& artifact);

        QList<ReconstructedArtifact> m_artifacts;
        int m_current{-1};
        Page m_rendered_page{PageSource}; //!< The non-source page for the current artifact.
        bool m_showing_source{false};

        QListWidget* m_list{nullptr};
        QLabel* m_title{nullptr};
        QLabel* m_meta{nullptr};
        QToolButton* m_toggle{nullptr};
        QToolButton* m_copy{nullptr};
        QToolButton* m_export{nullptr};
        QStackedWidget* m_stack{nullptr};
        QTextBrowser* m_rich{nullptr};
        QScrollArea* m_svg_area{nullptr};
        QLabel* m_svg_label{nullptr};
        QPlainTextEdit* m_source{nullptr};
    };
}
