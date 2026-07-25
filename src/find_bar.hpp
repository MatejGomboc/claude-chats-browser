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

#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QEvent;
class QLabel;
class QLineEdit;
class QObject;
QT_END_NAMESPACE

namespace ChatsBrowser
{
    /*!
        A slim "find in conversation" bar: a text box, a match counter and
        previous / next / close controls. It holds no search state itself — it
        emits what the user typed or clicked and shows back the result count.

        Enter jumps to the next match, Shift+Enter to the previous, Escape closes.
    */
    class FindBar : public QWidget {
        Q_OBJECT

    public:
        explicit FindBar(QWidget* parent = nullptr);

        //! Shows the bar and gives the (pre-selected) text box focus.
        void activate();

        //! The current query text.
        [[nodiscard]] QString query() const;

        //! Updates the "k / n" counter for \a total matches at 1-based \a current
        //! (0 = none). Shows "No results" when a non-empty query matches nothing.
        void setResultLabel(int current, int total);

    signals:
        //! Emitted whenever the query text changes.
        void queryChanged(const QString& term);

        //! Emitted on Enter / the next button.
        void nextRequested();

        //! Emitted on Shift+Enter / the previous button.
        void prevRequested();

        //! Emitted on Escape / the close button.
        void closed();

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        QLineEdit* m_input{nullptr};
        QLabel* m_count{nullptr};
    };
}
