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
#include "database.hpp"
#include "import_worker.hpp"
#include <QAction>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QSqlDatabase>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

namespace ChatsBrowser
{
    namespace
    {
        constexpr int SEARCH_DEBOUNCE_MS = 150;
        constexpr int STATUS_MESSAGE_TIMEOUT_MS = 10000;
    }

    MainWindow::MainWindow(QWidget* parent) :
        QMainWindow(parent),
        m_ui(std::make_unique<Ui::MainWindow>())
    {
        m_ui->setupUi(this);

        QString open_error;
        QSqlDatabase database = Database::open("main", Database::defaultPath(), &open_error);
        if (!database.isValid()) {
            QMessageBox::critical(this, "Database error", open_error);
        }

        setupUi();
        setupMenus();
        setupImportWorker();

        m_conversation_model->refresh();
    }

    MainWindow::~MainWindow()
    {
        m_import_thread.quit();
        m_import_thread.wait();
    }

    void MainWindow::setupUi()
    {
        m_conversation_model = new ConversationListModel(this);

        m_search_edit = new QLineEdit(m_ui->centralWidget);
        m_search_edit->setPlaceholderText("Search conversations…");
        m_search_edit->setClearButtonEnabled(true);

        m_conversation_view = new QListView(m_ui->centralWidget);
        m_conversation_view->setModel(m_conversation_model);
        m_conversation_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_conversation_view->setUniformItemSizes(true);

        QWidget* sidebar = new QWidget(m_ui->centralWidget);
        QVBoxLayout* sidebar_layout = new QVBoxLayout(sidebar);
        sidebar_layout->setContentsMargins(0, 0, 0, 0);
        sidebar_layout->addWidget(m_search_edit);
        sidebar_layout->addWidget(m_conversation_view);

        QLabel* reader_placeholder = new QLabel("Select a conversation", m_ui->centralWidget);
        reader_placeholder->setAlignment(Qt::AlignCenter);
        reader_placeholder->setEnabled(false);

        QSplitter* splitter = new QSplitter(Qt::Horizontal, m_ui->centralWidget);
        splitter->addWidget(sidebar);
        splitter->addWidget(reader_placeholder);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({360, 920});

        QVBoxLayout* central_layout = new QVBoxLayout(m_ui->centralWidget);
        central_layout->setContentsMargins(0, 0, 0, 0);
        central_layout->addWidget(splitter);

        m_search_timer = new QTimer(this);
        m_search_timer->setSingleShot(true);
        m_search_timer->setInterval(SEARCH_DEBOUNCE_MS);
        connect(m_search_edit, &QLineEdit::textChanged, m_search_timer, static_cast<void (QTimer::*)()>(&QTimer::start));
        connect(m_search_timer, &QTimer::timeout, this, [this]() {
            m_conversation_model->setSearchFilter(m_search_edit->text());
        });
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
}
