#pragma once

#include <functional>

#include <QString>

class QWidget;

// What TuxCMP's windows tell and ask the user in a box of its own: a
// problem, and a question answered Yes or No.
//
// A test that drives the windows has no one to answer: it says what the
// answers are, and notes what was told.

namespace prompts {

void problem(QWidget* parent, const QString& text);
bool ask(QWidget* parent, const QString& title, const QString& text);

// For tests: null puts the boxes back.
void setProblemHandler(std::function<void(const QString&)> handler);
void setAnswerer(std::function<bool(const QString& text)> answerer);

}  // namespace prompts
