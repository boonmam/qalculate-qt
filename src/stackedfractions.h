/* Qalculate! Qt UI - GPL-2.0-or-later */
#ifndef STACKED_FRACTIONS_H
#define STACKED_FRACTIONS_H

#include <QColor>
#include <QFont>
#include <QImage>
#include <QString>
#include <QVariant>
#include <QUrl>
#include <QTextDocument>
#include <QTextCursor>

// Display-only conversion. Unsupported or excessively large expressions are
// returned unchanged; the calculator's original HTML is never modified.
QString stackedFractionHtml(const QString &html, const QFont &font, const QColor &color);
QString restoreStackedFractions(QString html);
QTextCursor findFractionText(QTextDocument *document, const QString &text, int from, QTextDocument::FindFlags flags = QTextDocument::FindFlags());
QVariant stackedFractionResource(int type, const QUrl &url);
QImage renderStackedFractions(const QString &html, const QFont &font,
                             const QColor &color, QString *structure = nullptr);
#endif
