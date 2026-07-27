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

/*
    Component script for the application package: creates the Start Menu
    entry on Windows. QtIFW registers the operation, so uninstalling
    removes the shortcut again automatically.
*/

function Component()
{
}

Component.prototype.createOperations = function ()
{
    // Extract the package's data files first.
    component.createOperations();

    if (systemInfo.productType === "windows") {
        // Install the bundled MSVC C++ runtime silently so the app runs on
        // machines without the VC redistributable. Accepted exit codes:
        // 0 = installed, 1638 = a newer version is already installed,
        // 3010 = installed, reboot required (the app still runs immediately).
        component.addElevatedOperation(
            "Execute", "{0,1638,3010}",
            "@TargetDir@/bin/vc_redist.x64.exe", "/install", "/quiet", "/norestart");

        // The deployed tree keeps Qt's bin/plugins/translations layout.
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/bin/claude-chats-browser.exe",
            "@StartMenuDir@/Claude Chats Browser.lnk",
            "workingDirectory=@TargetDir@/bin",
            "description=Browse, search and read claude.ai data exports offline");
    }
}
