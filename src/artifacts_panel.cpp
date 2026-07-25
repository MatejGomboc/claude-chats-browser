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

#include "artifacts_panel.hpp"
#include "code_highlighter.hpp"
#include <QClipboard>
#include <QFileDialog>
#include <QFont>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSize>
#include <QSplitter>
#include <QStackedWidget>
#include <QSvgRenderer>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

namespace ChatsBrowser
{
    namespace
    {
        //! Maps a code artifact's language to a file extension (leading dot).
        [[nodiscard]] QString extensionForLanguage(const QString& language)
        {
            static const QHash<QString, QString> map = {{"python", ".py"}, {"cpp", ".cpp"}, {"c", ".c"}, {"rust", ".rs"}, {"bash", ".sh"}, {"cmake", ".cmake"},
                {"json", ".json"}, {"yaml", ".yaml"}, {"toml", ".toml"}, {"assembly", ".s"}, {"asm", ".s"}, {"ld", ".ld"}, {"tcl", ".tcl"}, {"markdown", ".md"},
                {"text", ".txt"}, {"rust", ".rs"}};
            return map.value(language.toLower(), ".txt");
        }
    }

    ArtifactsPanel::ArtifactsPanel(const QString& conversation_uuid, const QString& conversation_title, QWidget* parent) :
        QDialog(parent)
    {
        setWindowTitle(conversation_title.isEmpty() ? QString("Artifacts") : QString("Artifacts · %1").arg(conversation_title));
        setObjectName("artifactsPanel");
        resize(1000, 680);
        setAttribute(Qt::WA_DeleteOnClose);

        m_artifacts = ArtifactReconstructor::reconstruct(conversation_uuid);

        // Left: the artifact list.
        m_list = new QListWidget(this);
        m_list->setObjectName("artifactList");
        for (const ReconstructedArtifact& artifact : m_artifacts) {
            const QString name = artifact.title.isEmpty() ? artifact.id : artifact.title;
            QListWidgetItem* item = new QListWidgetItem(name, m_list);
            item->setToolTip(QString("%1\n%2 · %3 revision(s)").arg(name, typeLabel(artifact)).arg(artifact.revisions));
        }

        // Right: header, viewer, actions.
        m_title = new QLabel(this);
        m_title->setObjectName("artifactTitle");
        m_title->setWordWrap(true);

        m_meta = new QLabel(this);
        m_meta->setObjectName("artifactMeta");

        m_toggle = new QToolButton(this);
        m_toggle->setObjectName("artifactAction");
        m_toggle->setText("View source");
        m_toggle->setToolTip("Switch between the rendered artifact and its source");
        connect(m_toggle, &QToolButton::clicked, this, &ArtifactsPanel::toggleSource);

        m_copy = new QToolButton(this);
        m_copy->setObjectName("artifactAction");
        m_copy->setText("Copy");
        m_copy->setToolTip("Copy the artifact's content to the clipboard");
        connect(m_copy, &QToolButton::clicked, this, &ArtifactsPanel::copyCurrent);

        m_export = new QToolButton(this);
        m_export->setObjectName("artifactAction");
        m_export->setText("Export…");
        m_export->setToolTip("Save the artifact to a file");
        connect(m_export, &QToolButton::clicked, this, &ArtifactsPanel::exportCurrent);

        QHBoxLayout* action_row = new QHBoxLayout();
        action_row->setContentsMargins(0, 0, 0, 0);
        action_row->addWidget(m_title, 1);
        action_row->addWidget(m_toggle);
        action_row->addWidget(m_copy);
        action_row->addWidget(m_export);

        // Viewer pages: rich text, rendered SVG, and highlighted source.
        m_rich = new QTextBrowser(this);
        m_rich->setObjectName("artifactRich");
        m_rich->setOpenExternalLinks(true);

        m_svg_label = new QLabel(this);
        m_svg_label->setAlignment(Qt::AlignCenter);
        m_svg_area = new QScrollArea(this);
        m_svg_area->setObjectName("artifactSvgArea");
        m_svg_area->setWidgetResizable(true);
        m_svg_area->setWidget(m_svg_label);

        m_source = new QPlainTextEdit(this);
        m_source->setObjectName("artifactSource");
        m_source->setReadOnly(true);
        m_source->setLineWrapMode(QPlainTextEdit::NoWrap);
        QFont mono("Cascadia Code");
        mono.setStyleHint(QFont::Monospace);
        mono.setPointSize(9);
        m_source->setFont(mono);
        new CodeHighlighter(m_source->document()); // owned by the document

        m_stack = new QStackedWidget(this);
        m_stack->insertWidget(PageRich, m_rich);
        m_stack->insertWidget(PageSvg, m_svg_area);
        m_stack->insertWidget(PageSource, m_source);

        QWidget* right = new QWidget(this);
        QVBoxLayout* right_layout = new QVBoxLayout(right);
        right_layout->setContentsMargins(12, 12, 12, 12);
        right_layout->setSpacing(6);
        right_layout->addLayout(action_row);
        right_layout->addWidget(m_meta);
        right_layout->addWidget(m_stack, 1);

        QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->addWidget(m_list);
        splitter->addWidget(right);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({260, 740});

        QVBoxLayout* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        if (m_artifacts.isEmpty()) {
            QLabel* empty = new QLabel("This conversation has no artifacts.", this);
            empty->setObjectName("artifactMeta");
            empty->setAlignment(Qt::AlignCenter);
            root->addWidget(empty);
            return;
        }
        root->addWidget(splitter);

        connect(m_list, &QListWidget::currentRowChanged, this, &ArtifactsPanel::showArtifact);
        m_list->setCurrentRow(0);
    }

    void ArtifactsPanel::showArtifact(int index)
    {
        if ((index < 0) || (index >= m_artifacts.size())) {
            return;
        }
        m_current = index;
        const ReconstructedArtifact& artifact = m_artifacts.at(index);

        m_title->setText(artifact.title.isEmpty() ? artifact.id : artifact.title);
        m_meta->setText(QString("%1  ·  %2 revision(s)  ·  %3 chars").arg(typeLabel(artifact)).arg(artifact.revisions).arg(artifact.content.size()));

        // Source view is always available; build it every time.
        m_source->setPlainText(artifact.content);

        m_rendered_page = renderedPageFor(artifact.type);
        if (m_rendered_page == PageRich) {
            if (artifact.type == "text/html") {
                m_rich->setHtml(artifact.content);
            } else {
                m_rich->setMarkdown(artifact.content);
            }
        } else if (m_rendered_page == PageSvg) {
            QSvgRenderer renderer(artifact.content.toUtf8());
            if (renderer.isValid()) {
                QSize size = renderer.defaultSize();
                if (size.isEmpty()) {
                    size = QSize(480, 480);
                }
                QPixmap pixmap(size);
                pixmap.fill(Qt::transparent);
                QPainter painter(&pixmap);
                renderer.render(&painter);
                painter.end();
                m_svg_label->setPixmap(pixmap);
            } else {
                m_rendered_page = PageSource; // malformed SVG: fall back to its source
            }
        }

        // A renderable artifact defaults to its preview; others show source with no toggle.
        const bool has_preview = (m_rendered_page != PageSource);
        m_toggle->setVisible(has_preview);
        m_showing_source = !has_preview;
        m_toggle->setText("View source");
        m_stack->setCurrentIndex(has_preview ? m_rendered_page : PageSource);
    }

    void ArtifactsPanel::toggleSource()
    {
        m_showing_source = !m_showing_source;
        m_stack->setCurrentIndex(m_showing_source ? PageSource : m_rendered_page);
        m_toggle->setText(m_showing_source ? "View rendered" : "View source");
    }

    void ArtifactsPanel::copyCurrent()
    {
        if ((m_current < 0) || (m_current >= m_artifacts.size())) {
            return;
        }
        QGuiApplication::clipboard()->setText(m_artifacts.at(m_current).content);
    }

    void ArtifactsPanel::exportCurrent()
    {
        if ((m_current < 0) || (m_current >= m_artifacts.size())) {
            return;
        }
        const ReconstructedArtifact& artifact = m_artifacts.at(m_current);
        const QString path = QFileDialog::getSaveFileName(this, "Export artifact", suggestedFileName(artifact));
        if (path.isEmpty()) {
            return;
        }
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(artifact.content.toUtf8());
            file.commit();
        }
    }

    ArtifactsPanel::Page ArtifactsPanel::renderedPageFor(const QString& type)
    {
        if ((type == "text/markdown") || (type == "text/html")) {
            return PageRich;
        }
        if (type == "image/svg+xml") {
            return PageSvg;
        }
        // Code, React and Mermaid have no dependency-light in-app preview: source only.
        return PageSource;
    }

    QString ArtifactsPanel::typeLabel(const ReconstructedArtifact& artifact)
    {
        const QString& type = artifact.type;
        if (type == "text/markdown") {
            return "Markdown";
        }
        if (type == "text/html") {
            return "HTML";
        }
        if (type == "image/svg+xml") {
            return "SVG";
        }
        if (type == "application/vnd.ant.mermaid") {
            return "Mermaid";
        }
        if (type == "application/vnd.ant.react") {
            return "React";
        }
        if (type == "application/vnd.ant.code") {
            return artifact.language.isEmpty() ? QString("Code") : QString("Code · %1").arg(artifact.language);
        }
        return type.isEmpty() ? QString("Artifact") : type;
    }

    QString ArtifactsPanel::suggestedFileName(const ReconstructedArtifact& artifact)
    {
        QString base = artifact.title.isEmpty() ? artifact.id : artifact.title;
        // A title is often already a filename ("worldbuilding/overview.md"); keep just the
        // leaf and drop characters that are awkward in a filename.
        base = base.section('/', -1).section('\\', -1);
        base.replace(QRegularExpression("[^A-Za-z0-9._ -]"), "_");
        if (base.isEmpty()) {
            base = "artifact";
        }

        // If the title already carries a sensible extension, keep it.
        if (base.contains('.') && !base.endsWith('.')) {
            return base;
        }

        const QString& type = artifact.type;
        if (type == "text/markdown") {
            return base + ".md";
        }
        if (type == "text/html") {
            return base + ".html";
        }
        if (type == "image/svg+xml") {
            return base + ".svg";
        }
        if (type == "application/vnd.ant.mermaid") {
            return base + ".mmd";
        }
        if (type == "application/vnd.ant.react") {
            return base + ".jsx";
        }
        if (type == "application/vnd.ant.code") {
            return base + extensionForLanguage(artifact.language);
        }
        return base + ".txt";
    }
}
