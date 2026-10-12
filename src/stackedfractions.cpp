/* Qalculate! Qt UI - GPL-2.0-or-later */
#include "stackedfractions.h"

#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QFontMetricsF>
#include <QPainter>
#include <QPicture>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <cmath>
#include <memory>
#include <vector>

namespace {
struct Node {
    enum Kind { Text, Row, Group, Fraction } kind = Text;
    QString html, text;
    std::vector<std::shared_ptr<Node>> children;
};
using Ptr = std::shared_ptr<Node>;
Ptr node(Node::Kind kind, std::initializer_list<Ptr> children = {}) {
    Ptr n = std::make_shared<Node>(); n->kind = kind; n->children = children; return n;
}
Ptr row(Ptr a, Ptr b) { return node(Node::Row, {a, b}); }

struct Token { QString text, html; bool script = false; bool spaceBefore = false; };
// Parse the *already interpreted* rich text, not the user's input. Formatting
// runs (including superscripts) remain intact. Never evaluate an expression.
class Parser {
public:
    std::vector<Token> tokens;
    size_t pos = 0;
    bool valid = true;
    int fractions = 0, depth = 0;
    explicit Parser(QTextDocument &doc) {
        QString plain = doc.toPlainText();
        for(int i = 0; i < plain.size();) {
            if(plain[i].isSpace()) { ++i; continue; }
            int start = i;
            QTextCursor c(&doc); c.setPosition(i); c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            auto align = c.charFormat().verticalAlignment();
            bool script = align == QTextCharFormat::AlignSuperScript || align == QTextCharFormat::AlignSubScript;
            ++i;
            if(script) {
                while(i < plain.size()) {
                    c.setPosition(i); c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                    if(c.charFormat().verticalAlignment() != align) break;
                    ++i;
                }
            } else if(!isOperator(plain[start])) {
                while(i < plain.size() && !plain[i].isSpace() && !isOperator(plain[i])) {
                    c.setPosition(i); c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                    if(c.charFormat().verticalAlignment() != QTextCharFormat::AlignNormal) break;
                    ++i;
                }
            }
            c.setPosition(start); c.setPosition(i, QTextCursor::KeepAnchor);
            tokens.push_back({plain.mid(start, i - start), c.selection().toHtml(), script, start > 0 && plain[start - 1].isSpace()});
        }
        if(tokens.size() > 512) valid = false;
    }
    static bool isOperator(QChar c) {
        return QStringLiteral("()+−-*/∕÷×·⋅=≈≠<>≤≥,;[]{}^!%&|±∓:").contains(c);
    }
    bool at(const QString &s) const { return pos < tokens.size() && tokens[pos].text == s; }
    Ptr take() {
        Ptr n = node(Node::Text); n->text = tokens[pos].text; n->html = tokens[pos++].html; return n;
    }
    static bool division(const QString &s) { return s == "/" || s == QString::fromUtf8("∕") || s == QString::fromUtf8("÷"); }
    static int precedence(const QString &s) {
        if(s == "^" ) return 40;
        if(division(s) || s == "*" || s == QString::fromUtf8("×") || s == QString::fromUtf8("·") || s == QString::fromUtf8("⋅")) return 30;
        if(s == "+" || s == "-" || s == QString::fromUtf8("−") || s == QString::fromUtf8("±") || s == QString::fromUtf8("∓")) return 20;
        if(QStringLiteral("=≈≠<>≤≥,;&|:").contains(s) && s.size() == 1) return 10;
        return -1;
    }
    Ptr primary() {
        if(++depth > 64 || pos >= tokens.size()) { valid = false; --depth; return node(Node::Text); }
        Ptr n;
        if(at("+") || at("-") || at(QString::fromUtf8("−"))) {
            Ptr sign = take(); n = row(sign, expression(40));
        } else if(at("(") || at("[") || at("{")) {
            QString close = at("(") ? ")" : (at("[") ? "]" : "}");
            Ptr open = take(); Ptr content = expression(0);
            if(!at(close)) { valid = false; n = content; }
            else n = node(Node::Group, {open, content, take()});
        } else if(precedence(tokens[pos].text) >= 0 || at(")") || at("]") || at("}")) {
            valid = false; n = take();
        } else {
            n = take();
            // Function arguments bind to their name, so 1/sin(x) retains its
            // complete denominator. Qalculate prints explicit multiplication
            // when juxtaposition would otherwise be ambiguous.
            if(at("(")) n = row(n, primary());
        }
        while(valid && pos < tokens.size() && (tokens[pos].script || at("!") || at("%"))) n = row(n, take());
        --depth; return n;
    }
    Ptr expression(int minPrecedence) {
        Ptr left = primary();
        while(valid && pos < tokens.size() && !at(")") && !at("]") && !at("}")) {
            QString op = tokens[pos].text;
            int prec = precedence(op);
            bool implicit = prec < 0;
            if(implicit) prec = 30;
            if(prec < minPrecedence) break;
            bool separated = tokens[pos].spaceBefore;
            Ptr operation;
            if(!implicit) operation = take();
            Ptr right = expression(prec + (op == "^" ? 0 : 1));
            if(!implicit && division(op)) { left = node(Node::Fraction, {left, right}); ++fractions; }
            else if(implicit) {
                if(separated) { Ptr space = node(Node::Text); space->text = " "; space->html = "&nbsp;"; left = row(left, space); }
                left = row(left, right);
            }
            else left = row(row(left, operation), right);
        }
        return left;
    }
};

QString describe(Ptr n) {
    if(n->kind == Node::Text) return n->text;
    QStringList parts; for(auto child : n->children) parts << describe(child);
    if(n->kind == Node::Fraction) return "frac(" + parts.join(",") + ")";
    return parts.join("");
}
struct Box { QPicture picture; qreal width = 0, height = 0, axis = 0; };
void draw(QPainter &p, const Box &box, qreal x, qreal y) {
    p.save(); p.translate(x, y); p.drawPicture(0, 0, box.picture); p.restore();
}
Ptr unwrap(Ptr n) {
    if(n->kind == Node::Group && n->children[0]->text == "(") return n->children[1];
    return n;
}
Box layout(Ptr n, const QFont &font, const QColor &color) {
    QFontMetricsF fm(font);
    Box b;
    if(n->kind == Node::Text) {
        QTextDocument doc; doc.setDocumentMargin(0); doc.setDefaultFont(font);
        doc.setDefaultStyleSheet("body { color: " + color.name() + "; }");
        doc.setHtml(n->html); doc.adjustSize();
        b.width = doc.idealWidth(); b.height = doc.size().height(); b.axis = fm.ascent() - fm.xHeight() / 2;
        QPainter p(&b.picture);
        QAbstractTextDocumentLayout::PaintContext context; context.palette.setColor(QPalette::Text, color);
        doc.documentLayout()->draw(&p, context);
        return b;
    }
    if(n->kind == Node::Fraction) {
        Box top = layout(unwrap(n->children[0]), font, color), bottom = layout(unwrap(n->children[1]), font, color);
        const qreal pad = std::max<qreal>(3, fm.height() * .16), gap = std::max<qreal>(2, fm.height() * .10);
        const qreal line = std::max<qreal>(1, fm.height() / 22);
        b.width = std::max(top.width, bottom.width) + 2 * pad;
        b.axis = top.height + gap + line / 2;
        b.height = top.height + bottom.height + 2 * gap + line;
        QPainter p(&b.picture);
        draw(p, top, (b.width - top.width) / 2, 0);
        draw(p, bottom, (b.width - bottom.width) / 2, top.height + 2 * gap + line);
        p.setPen(QPen(color, line)); p.drawLine(QPointF(1, b.axis), QPointF(b.width - 1, b.axis));
        return b;
    }
    std::vector<Box> boxes;
    for(auto child : n->children) { boxes.push_back(layout(child, font, color)); b.axis = std::max(b.axis, boxes.back().axis); }
    for(auto box : boxes) { b.width += box.width; b.height = std::max(b.height, b.axis + box.height - box.axis); }
    // Retain grouping outside a fraction and stretch brackets around tall math.
    if(n->kind == Node::Group && boxes.size() == 3) {
        b.axis = boxes[1].axis; b.height = boxes[1].height;
        for(int i : {0, 2}) {
            Box old = boxes[i]; boxes[i].picture = QPicture();
            QPainter p(&boxes[i].picture); p.scale(1, b.height / old.height); draw(p, old, 0, 0);
            boxes[i].height = b.height; boxes[i].axis = b.axis;
        }
    }
    QPainter p(&b.picture); qreal x = 0;
    for(auto box : boxes) { draw(p, box, x, b.axis - box.axis); x += box.width; }
    return b;
}
}

QImage renderStackedFractions(const QString &html, const QFont &font, const QColor &color, QString *structure) {
    if(structure) structure->clear();
    if(html.size() > 16384 || (!html.contains('/') && !html.contains(QChar(0x2215)) && !html.contains(QChar(0x00f7)))) return QImage();
    QString clean = html;
    clean.remove(QRegularExpression("<img[^>]*src=\"data://img1px\\.png\"[^>]*>"));
    // Matrices, quoted text, and multiline/unsupported markup keep Qt's native
    // rendering. This avoids changing the meaning of non-mathematical slashes.
    if(clean.contains(QRegularExpression("<(?:table|img|br)\\b", QRegularExpression::CaseInsensitiveOption))) return QImage();
    QTextDocument doc; doc.setDefaultFont(font); doc.setHtml(clean);
    QString text = doc.toPlainText();
    if(text.size() > 2048 || text.contains('\n') || text.contains('"') || text.contains('\'') || text.contains("://")) return QImage();
    // Preserve scientific notation intact rather than treating its sign as an
    // additive operator. Such uncommon mixed expressions use the native view.
    if(text.contains(QRegularExpression("[0-9][eE][+−-][0-9]"))) return QImage();
    Parser parser(doc); if(!parser.valid || parser.tokens.empty()) return QImage();
    Ptr root = parser.expression(0);
    if(!parser.valid || parser.pos != parser.tokens.size() || !parser.fractions) return QImage();
    Box b = layout(root, font, color);
    if(b.width > 8192 || b.height > 2048 || b.width * b.height > 2000000) return QImage();
    if(structure) *structure = describe(root);
    // Render at Retina resolution, with a transparent background for either
    // palette. HTML dimensions keep the logical size independent of the DPI.
    QImage image(qCeil(b.width + 4) * 2, qCeil(b.height + 4) * 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent); image.setDevicePixelRatio(2);
    QPainter p(&image); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::TextAntialiasing);
    draw(p, b, 2, 2);
    return image;
}

QString stackedFractionHtml(const QString &html, const QFont &font, const QColor &color) {
    QImage image = renderStackedFractions(html, font, color);
    if(image.isNull()) return html;
    QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG");
    QTextDocument text; text.setHtml(html);
    return QStringLiteral("<img src=\"qalculate-fraction:%1#%4\" width=\"%2\" height=\"%3\" style=\"vertical-align:middle\" alt=\"%5\" />")
        .arg(QString::fromLatin1(png.toBase64())).arg(image.width() / 2).arg(image.height() / 2)
        .arg(QString::fromLatin1(html.toUtf8().toBase64())).arg(text.toPlainText().toHtmlEscaped());
}

QString restoreStackedFractions(QString html) {
    QRegularExpression re("<img\\b[^>]*src=\"qalculate-fraction:[^\"#]+#([A-Za-z0-9+/=]+)\"[^>]*>");
    auto matches = re.globalMatch(html); std::vector<QRegularExpressionMatch> items;
    while(matches.hasNext()) items.push_back(matches.next());
    for(auto it = items.rbegin(); it != items.rend(); ++it)
        html.replace(it->capturedStart(), it->capturedLength(), QString::fromUtf8(QByteArray::fromBase64(it->captured(1).toLatin1())));
    return html;
}

QVariant stackedFractionResource(int type, const QUrl &url) {
    if(type != QTextDocument::ImageResource || url.scheme() != "qalculate-fraction") return QVariant();
    QImage image; image.loadFromData(QByteArray::fromBase64(url.toString(QUrl::RemoveFragment).mid(QStringLiteral("qalculate-fraction:").size()).toLatin1()), "PNG");
    return image;
}

QTextCursor findFractionText(QTextDocument *document, const QString &text, int from, QTextDocument::FindFlags flags) {
    QTextCursor best = document->find(text, from, flags);
    if(text.isEmpty()) return best;
    const bool backward = flags.testFlag(QTextDocument::FindBackward);
    const auto sensitivity = flags.testFlag(QTextDocument::FindCaseSensitively) ? Qt::CaseSensitive : Qt::CaseInsensitive;
    for(QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for(auto it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment fragment = it.fragment();
            if(!fragment.charFormat().isImageFormat()) continue;
            int pos = fragment.position();
            if((backward && pos >= from) || (!backward && pos < from)) continue;
            if(!best.isNull() && (backward ? pos <= best.selectionStart() : pos >= best.selectionStart())) continue;
            QUrl url(fragment.charFormat().toImageFormat().name());
            if(url.scheme() != "qalculate-fraction") continue;
            QTextDocument original;
            original.setHtml(QString::fromUtf8(QByteArray::fromBase64(url.fragment().toLatin1())));
            if(!original.toPlainText().contains(text, sensitivity)) continue;
            best = QTextCursor(document); best.setPosition(pos); best.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        }
    }
    return best;
}
