#include "prompts.h"

#include <QMessageBox>
#include <QWidget>

namespace prompts {

namespace {

std::function<void(const QString&)>& problemHandler()
{
    static std::function<void(const QString&)> handler;
    return handler;
}

std::function<bool(const QString&)>& answerer()
{
    static std::function<bool(const QString&)> answer;
    return answer;
}

QString title(QWidget* parent)
{
    return parent && !parent->window()->windowTitle().isEmpty() ? parent->window()->windowTitle() : QStringLiteral("TuxCMP");
}

}  // namespace

void problem(QWidget* parent, const QString& text)
{
    if (problemHandler()) {
        problemHandler()(text);
        return;
    }
    QMessageBox::warning(parent, title(parent), text);
}

bool ask(QWidget* parent, const QString& heading, const QString& text)
{
    if (answerer())
        return answerer()(text);
    return QMessageBox::question(parent, heading.isEmpty() ? title(parent) : heading, text,
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
           == QMessageBox::Yes;
}

void setProblemHandler(std::function<void(const QString&)> handler)
{
    problemHandler() = std::move(handler);
}

void setAnswerer(std::function<bool(const QString&)> answer)
{
    answerer() = std::move(answer);
}

}  // namespace prompts
