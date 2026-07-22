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

#include "database.hpp"
#include "import_worker.hpp"
#include "main_window.hpp"
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>
#include <cstdio>

namespace
{
    //! Applies the bundled VS Code Dark+ inspired stylesheet to the application.
    void applyTheme(QApplication& application)
    {
        QFile theme_file(":/theme.qss");
        if (theme_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            application.setStyleSheet(QString::fromUtf8(theme_file.readAll()));
        }
    }
}

namespace
{
    //! Runs a blocking, windowless import for the --import command line option.
    [[nodiscard]] int runHeadlessImport(const QString& export_dir)
    {
        // A GUI-subsystem app has no console, and Qt's default message handler routes
        // qWarning/qInfo to the debugger on Windows — so write results straight to
        // stderr (redirectable) with an explicit flush.
        QTextStream error_stream(stderr);
        bool success = false;

        ChatsBrowser::ImportWorker worker(ChatsBrowser::Database::defaultPath());
        QObject::connect(&worker, &ChatsBrowser::ImportWorker::finished,
            [&success, &error_stream](int imported_conversations, int skipped_conversations, int imported_messages) {
                success = true;
                error_stream << "Import complete: " << imported_conversations << " conversations (" << imported_messages << " messages) imported, "
                             << skipped_conversations << " skipped\n";
            });
        QObject::connect(&worker, &ChatsBrowser::ImportWorker::failed, [&error_stream](const QString& error_message) {
            error_stream << "Import failed: " << error_message << "\n";
        });

        worker.importExport(export_dir);
        error_stream.flush();
        std::fflush(stderr);
        return success ? 0 : 1;
    }
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName("claude-chats-browser");
    QApplication::setApplicationDisplayName("Claude Chats Browser");
    QApplication::setApplicationVersion(APP_VERSION_STRING);
    QApplication::setOrganizationName("MatejGomboc");

    applyTheme(application);

    QCommandLineParser parser;
    parser.setApplicationDescription("Browse, search and read claude.ai data exports offline.");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption import_option("import", "Import an extracted claude.ai export directory and exit.", "directory");
    parser.addOption(import_option);
    parser.process(application);

    if (parser.isSet(import_option)) {
        return runHeadlessImport(parser.value(import_option));
    }

    ChatsBrowser::MainWindow main_window;
    main_window.show();

    return QApplication::exec();
}
