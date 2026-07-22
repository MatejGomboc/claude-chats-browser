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
class QLabel;
class QLineEdit;
class QListView;
class QModelIndex;
class QTabBar;
class QTimer;
QT_END_NAMESPACE

namespace ChatsBrowser
{
    class ConversationListModel;
    class ConversationReader;

    //! Application main window: VS Code-style shell around the conversation browser and reader.
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
        void onTabChanged(int index);
        void onTabCloseRequested(int index);
        void showAbout();

    private:
        QWidget* buildSideBar();
        QWidget* buildEditorArea();
        void setupMenus();
        void setupImportWorker();

        //! Opens (or re-activates) a tab for the given conversation and shows it.
        void openConversationTab(const QString& uuid, const QString& title, bool has_content);

        //! Refreshes the breadcrumb bar for the given conversation (empty clears it).
        void updateBreadcrumb(const QString& uuid);

        //! Updates the status-bar segment describing the given conversation.
        void updateConversationStatus(const QString& uuid);

        //! Updates the status-bar total-conversations counter from the database.
        void updateTotalsStatus();

        std::unique_ptr<Ui::MainWindow> m_ui; //!< Designer-generated form.
        ConversationListModel* m_conversation_model{nullptr}; //!< Sidebar model (owned by this window).
        ConversationReader* m_reader{nullptr}; //!< Right-pane reader for the current tab.
        QLineEdit* m_search_edit{nullptr}; //!< Search-as-you-type box in the sidebar.
        QListView* m_conversation_view{nullptr}; //!< Sidebar conversation list.
        QTabBar* m_tabs{nullptr}; //!< Open-conversation tabs above the reader.
        QLabel* m_breadcrumb{nullptr}; //!< Path bar below the tabs.
        QLabel* m_status_totals{nullptr}; //!< Left status-bar segment: total conversation count.
        QLabel* m_status_conversation{nullptr}; //!< Right status-bar segment: current conversation info.
        QTimer* m_search_timer{nullptr}; //!< Debounce timer for the search box.
        QAction* m_import_action{nullptr}; //!< File menu import action; disabled while importing.
        QThread m_import_thread; //!< Worker thread owning the ImportWorker.
    };
}
