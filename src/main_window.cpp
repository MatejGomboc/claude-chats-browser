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
#include "conversation_list_model.hpp"
#include "conversation_reader.hpp"
#include "database.hpp"
#include "icon_util.hpp"
#include "import_worker.hpp"
#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QModelIndex>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

namespace ChatsBrowser
{
    namespace
    {
        constexpr int SEARCH_DEBOUNCE_MS = 150;
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
        m_status_conversation = new QLabel(this);
        statusBar()->addWidget(m_status_totals);
        statusBar()->addPermanentWidget(m_status_conversation);
        statusBar()->setSizeGripEnabled(false);

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

        m_breadcrumb = new QLabel(editor_area);
        m_breadcrumb->setObjectName("breadcrumb");
        m_breadcrumb->setText(" ");

        m_reader = new ConversationReader(editor_area);

        QVBoxLayout* editor_layout = new QVBoxLayout(editor_area);
        editor_layout->setContentsMargins(0, 0, 0, 0);
        editor_layout->setSpacing(0);
        editor_layout->addWidget(m_tabs);
        editor_layout->addWidget(m_breadcrumb);
        editor_layout->addWidget(m_reader, 1);

        return editor_area;
    }

    void MainWindow::setupMenus()
    {
        QMenu* file_menu = menuBar()->addMenu("&File");
        m_import_action = file_menu->addAction("&Import claude.ai export…");
        m_import_action->setShortcut(QKeySequence("Ctrl+I"));
        connect(m_import_action, &QAction::triggered, this, &MainWindow::onImportActionTriggered);
        file_menu->addSeparator();
        QAction* quit_action = file_menu->addAction("&Quit");
        quit_action->setShortcut(QKeySequence::Quit);
        connect(quit_action, &QAction::triggered, this, &MainWindow::close);

        QMenu* view_menu = menuBar()->addMenu("&View");
        QAction* focus_search_action = view_menu->addAction("&Search Conversations");
        focus_search_action->setShortcut(QKeySequence::Find);
        connect(focus_search_action, &QAction::triggered, this, [this]() {
            m_search_edit->setFocus();
            m_search_edit->selectAll();
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
        statusBar()->showMessage("Importing…");
        emit importRequested(export_dir);
    }

    void MainWindow::onImportProgress(int done_conversations, int total_conversations)
    {
        statusBar()->showMessage(QString("Importing… %1 / %2 conversations").arg(done_conversations).arg(total_conversations));
    }

    void MainWindow::onImportFinished(int imported_conversations, int skipped_conversations, int imported_messages)
    {
        m_import_action->setEnabled(true);
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
        statusBar()->clearMessage();
        QMessageBox::warning(this, "Import failed", error_message);
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

    void MainWindow::onTabChanged(int index)
    {
        if (index < 0) {
            m_reader->clearConversation();
            updateBreadcrumb(QString());
            updateConversationStatus(QString());
            return;
        }
        QString uuid = m_tabs->tabData(index).toString();
        m_reader->showConversation(uuid);
        updateBreadcrumb(uuid);
        updateConversationStatus(uuid);
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
                    "Browse, search and read claude.ai data exports offline.<br>"
                    "Qt %2 · GPL v3")
                .arg(QApplication::applicationVersion(), QT_VERSION_STR));
    }
}
