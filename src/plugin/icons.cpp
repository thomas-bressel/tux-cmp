#include "icons.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPolygonF>

namespace tuxcmp {

QIcon compressIcon()
{
    const QColor ink = QApplication::palette().color(QPalette::WindowText);
    // On a grid of 20, at twice the size a button shows it.
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    QPen pen(ink, 1.5);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);
    // The file, squat, a corner turned down; its lines.
    painter.drawPolygon(QPolygonF({{5.5, 6.5}, {12.0, 6.5}, {14.5, 9.0}, {14.5, 13.5}, {5.5, 13.5}}));
    painter.drawLine(QPointF(7.5, 9.5), QPointF(11.0, 9.5));
    painter.drawLine(QPointF(7.5, 11.5), QPointF(12.5, 11.5));
    // The arrows that press it, from above and from below.
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    painter.drawPolygon(QPolygonF({{6.5, 0.5}, {13.5, 0.5}, {10.0, 4.5}}));
    painter.drawPolygon(QPolygonF({{6.5, 19.5}, {13.5, 19.5}, {10.0, 15.5}}));
    return QIcon(pixmap);
}

}  // namespace tuxcmp
