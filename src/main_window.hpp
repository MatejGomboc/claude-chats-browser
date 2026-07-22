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

#include <QMainWindow>
#include <QThread>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
class QAction;
class QLineEdit;
class QListView;
class QModelIndex;
class QTimer;
QT_END_NAMESPACE

namespace ChatsBrowser
{
    class ConversationListModel;
    class ConversationReader;

    //! Application main window: hosts the conversation browser and reader views.
    class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow() override;

    signals:
        //! Requests the import worker (queued onto its thread) to import an export directory.
        void importRequested(const QString& export_dir);

    private slots:
        void onImportActionTriggered();
        void onImportProgress(int done_conversations, int total_conversations);
        void onImportFinished(int imported_conversations, int skipped_conversations, int imported_messages);
        void onImportFailed(const QString& error_message);
        void onConversationSelected(const QModelIndex& current, const QModelIndex& previous);

    private:
        void setupUi();
        void setupMenus();
        void setupImportWorker();

        std::unique_ptr<Ui::MainWindow> m_ui; //!< Designer-generated form.
        ConversationListModel* m_conversation_model{nullptr}; //!< Sidebar model (owned by this window).
        ConversationReader* m_reader{nullptr}; //!< Right-pane reader for the selected conversation.
        QLineEdit* m_search_edit{nullptr}; //!< Search-as-you-type box above the conversation list.
        QListView* m_conversation_view{nullptr}; //!< Sidebar conversation list.
        QTimer* m_search_timer{nullptr}; //!< Debounce timer for the search box.
        QAction* m_import_action{nullptr}; //!< File menu import action; disabled while importing.
        QThread m_import_thread; //!< Worker thread owning the ImportWorker.
    };
}
