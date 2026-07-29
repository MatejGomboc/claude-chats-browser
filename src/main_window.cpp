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

#include "main_window.hpp"
#include "ui_main_window.h"
#include "artifacts_panel.hpp"
#include "conversation_list_model.hpp"
#include "conversation_exporter.hpp"
#include "conversation_reader.hpp"
#include "database.hpp"
#include "find_bar.hpp"
#include "icon_util.hpp"
#include "import_worker.hpp"
#include "stats_panel.hpp"
#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QSaveFile>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QModelIndex>
#include <QProgressBar>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

namespace ChatsBrowser
{
    namespace
    {
        constexpr int SEARCH_DEBOUNCE_MS = 150;
        constexpr int SEARCH_BUSY_DELAY_MS = 150;
        constexpr int STATUS_MESSAGE_TIMEOUT_MS = 10000;
        constexpr int SIDEBAR_WIDTH = 300;

        //! Formats an ISO timestamp down to just the date part for compact display.
        [[nodiscard]] QString shortDate(const QString& iso_timestamp)
        {
            return iso_timestamp.left(10);
        }
    }

    MainWindow::MainWindow(QWidget* parent) :
        QMainWindow(parent),
        m_ui(std::make_unique<Ui::MainWindow>())
    {
        m_ui->setupUi(this);
        setWindowTitle("Claude Chats Browser");

        QString open_error;
        QSqlDatabase database = Database::open("main", Database::defaultPath(), &open_error);
        if (!database.isValid()) {
            QMessageBox::critical(this, "Database error", open_error);
        }

        QWidget* side_bar = buildSideBar();
        QWidget* editor_area = buildEditorArea();

        QHBoxLayout* central_layout = new QHBoxLayout(m_ui->centralWidget);
        central_layout->setContentsMargins(0, 0, 0, 0);
        central_layout->setSpacing(0);
        central_layout->addWidget(side_bar);
        central_layout->addWidget(editor_area, 1);

        m_status_totals = new QLabel(this);
        m_progress_bar = new QProgressBar(this);
        m_progress_bar->setObjectName("renderProgress");
        m_progress_bar->setFixedWidth(160);
        m_progress_bar->setTextVisible(false);
        m_progress_bar->hide();
        m_status_conversation = new QLabel(this);
        statusBar()->addWidget(m_status_totals);
        statusBar()->addWidget(m_progress_bar);
        statusBar()->addPermanentWidget(m_status_conversation);
        statusBar()->setSizeGripEnabled(false);

        // Reader: streaming a large conversation reports determinate progress.
        connect(m_reader, &ConversationReader::renderProgressChanged, this, [this](int done, int total) {
            showDeterminateProgress(done, total, QString());
        });
        connect(m_reader, &ConversationReader::renderFinished, this, &MainWindow::hideProgress);
        // Once a (possibly chunked) render settles, re-apply any open find so matches in the
        // messages that streamed in late are highlighted too.
        connect(m_reader, &ConversationReader::renderFinished, this, [this]() {
            if (m_find_bar->isVisible() && !m_find_bar->query().isEmpty()) {
                m_reader->findText(m_find_bar->query());
            }
        });

        // Search: the query runs off the UI thread. Only show a busy indicator if the query
        // is still running after a short delay, so quick searches do not flicker the bar.
        m_search_busy_timer = new QTimer(this);
        m_search_busy_timer->setSingleShot(true);
        m_search_busy_timer->setInterval(SEARCH_BUSY_DELAY_MS);
        connect(m_search_busy_timer, &QTimer::timeout, this, [this]() {
            showBusyProgress("Searching…");
        });
        connect(m_conversation_model, &ConversationListModel::searchStarted, this, [this]() {
            m_search_busy_timer->start();
        });
        connect(m_conversation_model, &ConversationListModel::searchFinished, this, [this]() {
            m_search_busy_timer->stop();
            hideProgress();
        });

        setupMenus();
        setupImportWorker();

        m_conversation_model->refresh();
        updateTotalsStatus();
    }

    MainWindow::~MainWindow()
    {
        m_import_thread.quit();
        m_import_thread.wait();
    }

    QWidget* MainWindow::buildSideBar()
    {
        QWidget* side_bar = new QWidget(m_ui->centralWidget);
        side_bar->setObjectName("sideBar");
        side_bar->setFixedWidth(SIDEBAR_WIDTH);

        QLabel* header = new QLabel("CONVERSATIONS", side_bar);
        header->setObjectName("sideBarHeader");

        m_search_edit = new QLineEdit(side_bar);
        m_search_edit->setPlaceholderText("Search conversations");
        m_search_edit->setClearButtonEnabled(true);

        m_conversation_model = new ConversationListModel(this);

        m_conversation_view = new QListView(side_bar);
        m_conversation_view->setModel(m_conversation_model);
        m_conversation_view->setFrameShape(QFrame::NoFrame);
        m_conversation_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_conversation_view->setUniformItemSizes(true);
        m_conversation_view->setIconSize(QSize(16, 16));
        m_conversation_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QVBoxLayout* side_layout = new QVBoxLayout(side_bar);
        side_layout->setContentsMargins(0, 0, 0, 0);
        side_layout->setSpacing(0);
        side_layout->addWidget(header);
        side_layout->addWidget(m_search_edit);
        side_layout->addWidget(m_conversation_view, 1);

        m_search_timer = new QTimer(this);
        m_search_timer->setSingleShot(true);
        m_search_timer->setInterval(SEARCH_DEBOUNCE_MS);
        connect(m_search_edit, &QLineEdit::textChanged, m_search_timer, static_cast<void (QTimer::*)()>(&QTimer::start));
        connect(m_search_timer, &QTimer::timeout, this, [this]() {
            m_conversation_model->setSearchFilter(m_search_edit->text());
        });
        connect(m_conversation_view->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::onConversationSelected);

        return side_bar;
    }

    QWidget* MainWindow::buildEditorArea()
    {
        QWidget* editor_area = new QWidget(m_ui->centralWidget);
        editor_area->setObjectName("editorArea");

        m_tabs = new QTabBar(editor_area);
        m_tabs->setDocumentMode(true);
        m_tabs->setTabsClosable(true);
        m_tabs->setMovable(true);
        m_tabs->setExpanding(false);
        m_tabs->setElideMode(Qt::ElideRight);
        m_tabs->setDrawBase(false);
        m_tabs->setFocusPolicy(Qt::NoFocus);
        connect(m_tabs, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
        connect(m_tabs, &QTabBar::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

        // Breadcrumb row: the path on the left, an Artifacts button on the right.
        QWidget* breadcrumb_row = new QWidget(editor_area);
        breadcrumb_row->setObjectName("breadcrumbRow");
        QHBoxLayout* breadcrumb_layout = new QHBoxLayout(breadcrumb_row);
        breadcrumb_layout->setContentsMargins(0, 0, 0, 0);
        breadcrumb_layout->setSpacing(0);

        m_breadcrumb = new QLabel(breadcrumb_row);
        m_breadcrumb->setObjectName("breadcrumb");
        m_breadcrumb->setText(" ");

        m_artifacts_button = new QToolButton(breadcrumb_row);
        m_artifacts_button->setObjectName("artifactsButton");
        m_artifacts_button->setText("Artifacts");
        m_artifacts_button->setToolTip("Show this conversation's artifacts");
        m_artifacts_button->setCursor(Qt::PointingHandCursor);
        m_artifacts_button->setEnabled(false);
        connect(m_artifacts_button, &QToolButton::clicked, this, &MainWindow::openArtifactsPanel);

        breadcrumb_layout->addWidget(m_breadcrumb, 1);
        breadcrumb_layout->addWidget(m_artifacts_button);

        m_find_bar = new FindBar(editor_area);
        m_find_bar->hide();

        m_reader = new ConversationReader(editor_area);

        QVBoxLayout* editor_layout = new QVBoxLayout(editor_area);
        editor_layout->setContentsMargins(0, 0, 0, 0);
        editor_layout->setSpacing(0);
        editor_layout->addWidget(m_tabs);
        editor_layout->addWidget(breadcrumb_row);
        editor_layout->addWidget(m_find_bar);
        editor_layout->addWidget(m_reader, 1);

        // Find bar drives the reader; the reader reports match counts back to the bar.
        connect(m_find_bar, &FindBar::queryChanged, this, [this](const QString& term) {
            m_reader->findText(term);
        });
        connect(m_find_bar, &FindBar::nextRequested, m_reader, &ConversationReader::findNext);
        connect(m_find_bar, &FindBar::prevRequested, m_reader, &ConversationReader::findPrev);
        connect(m_find_bar, &FindBar::closed, this, &MainWindow::hideFindBar);
        connect(m_reader, &ConversationReader::findResultsChanged, m_find_bar, &FindBar::setResultLabel);

        return editor_area;
    }

    void MainWindow::setupMenus()
    {
        QMenu* file_menu = menuBar()->addMenu("&File");
        m_import_action = file_menu->addAction("&Import claude.ai export…");
        m_import_action->setShortcut(QKeySequence("Ctrl+I"));
        connect(m_import_action, &QAction::triggered, this, &MainWindow::onImportActionTriggered);

        file_menu->addSeparator();
        m_export_markdown_action = file_menu->addAction("&Export Conversation as Markdown…");
        m_export_markdown_action->setShortcut(QKeySequence("Ctrl+E"));
        m_export_markdown_action->setEnabled(false);
        connect(m_export_markdown_action, &QAction::triggered, this, [this]() {
            exportCurrentConversation(true);
        });
        m_export_json_action = file_menu->addAction("Export Conversation as &JSON…");
        m_export_json_action->setEnabled(false);
        connect(m_export_json_action, &QAction::triggered, this, [this]() {
            exportCurrentConversation(false);
        });

        file_menu->addSeparator();
        QAction* quit_action = file_menu->addAction("&Quit");
        quit_action->setShortcut(QKeySequence::Quit);
        connect(quit_action, &QAction::triggered, this, &MainWindow::close);

        QMenu* view_menu = menuBar()->addMenu("&View");
        // Editor-scoped find (like VS Code's Ctrl+F): search within the open conversation.
        QAction* find_action = view_menu->addAction("&Find in Conversation");
        find_action->setShortcut(QKeySequence::Find);
        connect(find_action, &QAction::triggered, this, &MainWindow::showFindBar);

        // Workspace-scoped find (like VS Code's Ctrl+Shift+F): jump to the sidebar search.
        QAction* focus_search_action = view_menu->addAction("Search &All Conversations");
        focus_search_action->setShortcut(QKeySequence("Ctrl+Shift+F"));
        connect(focus_search_action, &QAction::triggered, this, [this]() {
            m_search_edit->setFocus();
            m_search_edit->selectAll();
        });

        view_menu->addSeparator();
        QAction* stats_action = view_menu->addAction("S&tatistics");
        connect(stats_action, &QAction::triggered, this, [this]() {
            StatsPanel* panel = new StatsPanel(this);
            panel->show();
        });

        QMenu* help_menu = menuBar()->addMenu("&Help");
        QAction* about_action = help_menu->addAction("&About");
        connect(about_action, &QAction::triggered, this, &MainWindow::showAbout);
    }

    void MainWindow::setupImportWorker()
    {
        ImportWorker* worker = new ImportWorker(Database::defaultPath());
        worker->moveToThread(&m_import_thread);
        connect(&m_import_thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(this, &MainWindow::importRequested, worker, &ImportWorker::importExport);
        connect(worker, &ImportWorker::progressPhase, this, [this](const QString& message) {
            showBusyProgress(message);
        });
        connect(worker, &ImportWorker::progressChanged, this, &MainWindow::onImportProgress);
        connect(worker, &ImportWorker::finished, this, &MainWindow::onImportFinished);
        connect(worker, &ImportWorker::failed, this, &MainWindow::onImportFailed);
        m_import_thread.start();
    }

    void MainWindow::onImportActionTriggered()
    {
        QString export_dir = QFileDialog::getExistingDirectory(this, "Select an extracted claude.ai export directory");
        if (export_dir.isEmpty()) {
            return;
        }
        m_import_action->setEnabled(false);
        showBusyProgress("Importing…");
        emit importRequested(export_dir);
    }

    void MainWindow::onImportProgress(int done_conversations, int total_conversations)
    {
        showDeterminateProgress(done_conversations, total_conversations, QString("Importing… %1 / %2 conversations").arg(done_conversations).arg(total_conversations));
    }

    void MainWindow::onImportFinished(int imported_conversations, int skipped_conversations, int imported_messages)
    {
        m_import_action->setEnabled(true);
        hideProgress();
        m_conversation_model->refresh();
        updateTotalsStatus();
        statusBar()->showMessage(QString("Import complete: %1 conversations (%2 messages) imported, %3 skipped")
                                     .arg(imported_conversations)
                                     .arg(imported_messages)
                                     .arg(skipped_conversations),
            STATUS_MESSAGE_TIMEOUT_MS);
    }

    void MainWindow::onImportFailed(const QString& error_message)
    {
        m_import_action->setEnabled(true);
        hideProgress();
        QMessageBox::warning(this, "Import failed", error_message);
    }

    void MainWindow::showBusyProgress(const QString& message)
    {
        if (!message.isEmpty()) {
            statusBar()->showMessage(message);
        }
        m_progress_bar->setRange(0, 0); // indeterminate "busy" sweep
        m_progress_bar->show();
    }

    void MainWindow::showDeterminateProgress(int done, int total, const QString& message)
    {
        if (!message.isEmpty()) {
            statusBar()->showMessage(message);
        }
        m_progress_bar->setRange(0, qMax(total, 1));
        m_progress_bar->setValue(done);
        m_progress_bar->show();
    }

    void MainWindow::hideProgress()
    {
        m_progress_bar->hide();
        m_progress_bar->setRange(0, 100); // leave determinate so the next busy use re-arms it
        statusBar()->clearMessage();
    }

    void MainWindow::onConversationSelected(const QModelIndex& current, const QModelIndex& previous)
    {
        Q_UNUSED(previous);
        if (!current.isValid()) {
            return;
        }
        QString uuid = current.data(ConversationListModel::UuidRole).toString();
        QString title = current.data(Qt::DisplayRole).toString();
        bool has_content = current.data(ConversationListModel::HasContentRole).toBool();
        openConversationTab(uuid, title, has_content);
    }

    void MainWindow::openConversationTab(const QString& uuid, const QString& title, bool has_content)
    {
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (m_tabs->tabData(i).toString() == uuid) {
                m_tabs->setCurrentIndex(i);
                return;
            }
        }

        QColor icon_colour = has_content ? QColor("#C5C5C5") : QColor("#6A6A6A");
        // Add the tab with signals blocked: adding the first tab auto-fires currentChanged
        // before the UUID is attached, which would render an empty reader. Set everything
        // up, then drive the update explicitly.
        m_tabs->blockSignals(true);
        int index = m_tabs->addTab(IconUtil::tinted(":/icons/chat.svg", icon_colour), title);
        m_tabs->setTabData(index, uuid);
        m_tabs->setCurrentIndex(index);
        m_tabs->blockSignals(false);
        onTabChanged(index);
    }

    void MainWindow::showFindBar()
    {
        // Nothing to search inside when no conversation is open — fall back to the sidebar
        // search so Ctrl+F is never a dead key.
        if (m_tabs->currentIndex() < 0) {
            m_search_edit->setFocus();
            m_search_edit->selectAll();
            return;
        }
        m_find_bar->activate();
        m_reader->findText(m_find_bar->query());
    }

    void MainWindow::hideFindBar()
    {
        m_find_bar->hide();
        m_reader->clearFind();
        m_reader->setFocus();
    }

    void MainWindow::onTabChanged(int index)
    {
        // The find applies to one conversation; switching away retires it.
        hideFindBar();

        if (index < 0) {
            m_reader->clearConversation();
            updateBreadcrumb(QString());
            updateConversationStatus(QString());
            updateArtifactsButton(QString());
            m_export_markdown_action->setEnabled(false);
            m_export_json_action->setEnabled(false);
            return;
        }
        QString uuid = m_tabs->tabData(index).toString();
        m_reader->showConversation(uuid);
        updateBreadcrumb(uuid);
        updateConversationStatus(uuid);
        updateArtifactsButton(uuid);
        m_export_markdown_action->setEnabled(true);
        m_export_json_action->setEnabled(true);
    }

    void MainWindow::exportCurrentConversation(bool as_markdown)
    {
        const QString uuid = m_reader->conversationUuid();
        if (uuid.isEmpty()) {
            return;
        }

        ConversationInfo info;
        info.uuid = uuid;
        QSqlQuery query(QSqlDatabase::database("main"));
        query.prepare("SELECT name, summary, created_at, updated_at FROM conversations WHERE uuid = ?");
        query.addBindValue(uuid);
        if (query.exec() && query.next()) {
            info.name = query.value(0).toString();
            info.summary = query.value(1).toString();
            info.created_at = query.value(2).toString();
            info.updated_at = query.value(3).toString();
        }

        QByteArray payload;
        QString filter;
        QString suggested = ConversationExporter::suggestedBaseName(info);
        if (as_markdown) {
            // Markdown exports what is on screen: the currently selected branch path.
            payload = ConversationExporter::toMarkdown(info, m_reader->currentPathMessages()).toUtf8();
            filter = "Markdown (*.md)";
            suggested += ".md";
        } else {
            // JSON is lossless: every message of every branch, original export JSON.
            QList<QJsonObject> all_messages;
            QSqlQuery messages(QSqlDatabase::database("main"));
            messages.prepare("SELECT raw_json FROM messages WHERE conversation_uuid = ? ORDER BY created_at, rowid");
            messages.addBindValue(uuid);
            if (messages.exec()) {
                while (messages.next()) {
                    const QJsonDocument document = QJsonDocument::fromJson(messages.value(0).toString().toUtf8());
                    if (document.isObject()) {
                        all_messages.append(document.object());
                    }
                }
            }
            payload = QJsonDocument(ConversationExporter::toExportJson(info, all_messages)).toJson(QJsonDocument::Indented);
            filter = "JSON (*.json)";
            suggested += ".json";
        }

        const QString path = QFileDialog::getSaveFileName(this, "Export conversation", suggested, filter);
        if (path.isEmpty()) {
            return;
        }

        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly) && (file.write(payload) == payload.size()) && file.commit()) {
            statusBar()->showMessage(QString("Exported to %1").arg(path), STATUS_MESSAGE_TIMEOUT_MS);
        } else {
            QMessageBox::warning(this, "Export failed", QString("Could not write %1").arg(path));
        }
    }

    void MainWindow::updateArtifactsButton(const QString& uuid)
    {
        if (uuid.isEmpty()) {
            m_artifacts_button->setEnabled(false);
            m_artifacts_button->setText("Artifacts");
            return;
        }

        // Cheap existence + op count via LIKE; the exact artifact list is rebuilt only when
        // the panel is opened. Compact JSON in the store has no spaces after colons.
        QSqlQuery query(QSqlDatabase::database("main"));
        query.prepare("SELECT COUNT(*) FROM messages WHERE conversation_uuid = ? AND raw_json LIKE '%\"name\":\"artifacts\"%'");
        query.addBindValue(uuid);
        int op_messages = 0;
        if (query.exec() && query.next()) {
            op_messages = query.value(0).toInt();
        }
        m_artifacts_button->setEnabled(op_messages > 0);
        m_artifacts_button->setText(op_messages > 0 ? "Artifacts ●" : "Artifacts");
    }

    void MainWindow::openArtifactsPanel()
    {
        const int index = m_tabs->currentIndex();
        if (index < 0) {
            return;
        }
        const QString uuid = m_tabs->tabData(index).toString();
        const QString title = m_tabs->tabText(index);
        ArtifactsPanel* panel = new ArtifactsPanel(uuid, title, this);
        panel->show();
    }

    void MainWindow::onTabCloseRequested(int index)
    {
        m_tabs->removeTab(index);
        if (m_tabs->count() == 0) {
            onTabChanged(-1);
        }
    }

    void MainWindow::updateBreadcrumb(const QString& uuid)
    {
        if (uuid.isEmpty()) {
            m_breadcrumb->setText(" ");
            return;
        }

        QSqlQuery query(QSqlDatabase::database("main"));
        query.prepare("SELECT name, created_at FROM conversations WHERE uuid = ?");
        query.addBindValue(uuid);
        if ((!query.exec()) || (!query.next())) {
            m_breadcrumb->setText(" ");
            return;
        }

        QString name = query.value(0).toString();
        QString month = query.value(1).toString().left(7);
        if (name.isEmpty()) {
            name = "(untitled)";
        }
        m_breadcrumb->setText(QString("Conversations   ›   %1   ›   %2").arg(month, name));
    }

    void MainWindow::updateConversationStatus(const QString& uuid)
    {
        if (uuid.isEmpty()) {
            m_status_conversation->clear();
            return;
        }

        QSqlQuery query(QSqlDatabase::database("main"));
        query.prepare("SELECT message_count, created_at, updated_at FROM conversations WHERE uuid = ?");
        query.addBindValue(uuid);
        if ((!query.exec()) || (!query.next())) {
            m_status_conversation->clear();
            return;
        }

        int message_count = query.value(0).toInt();
        QString created = shortDate(query.value(1).toString());
        QString updated = shortDate(query.value(2).toString());
        m_status_conversation->setText(QString("%1 messages    created %2    updated %3").arg(message_count).arg(created, updated));
    }

    void MainWindow::updateTotalsStatus()
    {
        QSqlQuery query(QSqlDatabase::database("main"));
        if (query.exec("SELECT COUNT(*), SUM(has_content) FROM conversations") && query.next()) {
            int total = query.value(0).toInt();
            int with_content = query.value(1).toInt();
            m_status_totals->setText(QString("%1 conversations    %2 readable").arg(total).arg(with_content));
        }
    }

    void MainWindow::showAbout()
    {
        QMessageBox::about(this, "About Claude Chats Browser",
            QString("<b>Claude Chats Browser</b> %1<br><br>"
                    "Browse, search and read claude.ai data exports offline.<br><br>"
                    "<a href=\"https://github.com/MatejGomboc/claude-chats-browser\">"
                    "github.com/MatejGomboc/claude-chats-browser</a><br><br>"
                    "Copyright © 2026 Matej Gomboc<br>"
                    "GNU General Public License v3.0 · Qt %2")
                .arg(QApplication::applicationVersion(), QT_VERSION_STR));
    }
}
