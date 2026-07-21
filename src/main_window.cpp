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

namespace ChatsBrowser
{
    MainWindow::MainWindow(QWidget* parent) :
        QMainWindow(parent),
        m_ui(std::make_unique<Ui::MainWindow>())
    {
        m_ui->setupUi(this);
    }

    MainWindow::~MainWindow() = default;
}
