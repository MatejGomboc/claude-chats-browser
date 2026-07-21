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
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName("claude-chats-browser");
    QApplication::setApplicationDisplayName("Claude Chats Browser");
    QApplication::setApplicationVersion(APP_VERSION_STRING);
    QApplication::setOrganizationName("MatejGomboc");

    ChatsBrowser::MainWindow main_window;
    main_window.show();

    return QApplication::exec();
}
