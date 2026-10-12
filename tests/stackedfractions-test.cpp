#include <QtTest>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include "../src/stackedfractions.h"

class StackedFractionsTest : public QObject {
    Q_OBJECT
private slots:
    void grouping_data() {
        QTest::addColumn<QString>("html"); QTest::addColumn<QString>("expected");
        QTest::newRow("rational") << "1/2" << "frac(1,2)";
        QTest::newRow("grouped") << "(x+1)/(x-2)" << "frac((x+1),(x-2))";
        QTest::newRow("nested") << "1/(1+1/x)" << "frac(1,(1+frac(1,x)))";
        QTest::newRow("left-associative") << "a/b/c" << "frac(frac(a,b),c)";
        QTest::newRow("multiply-after") << "a/b*c" << "frac(a,b)*c";
        QTest::newRow("multiply-before") << "a*b/c" << "frac(a*b,c)";
        QTest::newRow("negative-denominator") << "1/-2" << "frac(1,-2)";
        QTest::newRow("power") << "x^2/y^3" << "frac(x^2,y^3)";
        QTest::newRow("superscript") << "<i>x</i><sup>2</sup> / (<i>y</i>+1)" << "frac(x2,(y+1))";
        QTest::newRow("function") << "1/sin(x)" << "frac(1,sin(x))";
        QTest::newRow("unicode") << QString::fromUtf8("1 ∕ 2 + 3 ÷ 4") << "frac(1,2)+frac(3,4)";
        QTest::newRow("equation") << "(x+1)/(x-2)=3/4" << "frac((x+1),(x-2))=frac(3,4)";
        QTest::newRow("factorial") << "1/3!" << "frac(1,3!)";
    }
    void grouping() {
        QFETCH(QString, html); QFETCH(QString, expected);
        QString structure; auto image = renderStackedFractions(html, QFont("Arial", 18), Qt::white, &structure);
        QVERIFY(!image.isNull()); QCOMPARE(structure, expected);
        QVERIFY(image.height() > 36); QCOMPARE(image.devicePixelRatio(), 2.0);
    }
    void fallback_data() {
        QTest::addColumn<QString>("html");
        for(QString input : {"2+2", "1/", "(1/2", "1/2)", "\"a/b\"", "https://example.com", "1e-3/2", "<table><tr><td>1/2</td></tr></table>", "1/2<br>3/4"})
            QTest::newRow(qPrintable(input)) << input;
        QTest::newRow("deep") << QString(100, '(') + "1/2" + QString(100, ')');
        QTest::newRow("long") << QString(20000, 'x') + "/2";
    }
    void fallback() {
        QFETCH(QString, html); QCOMPARE(stackedFractionHtml(html, QFont(), Qt::black), html);
    }
    void copyAndResourceRoundTrip() {
        const QString original = "(<i>x</i><sup>2</sup>+1) / (<i>y</i>-2)";
        QString html = stackedFractionHtml(original, QFont("Arial", 18), Qt::white);
        QVERIFY(html.startsWith("<img")); QCOMPARE(restoreStackedFractions(html), original);
        QTextDocument doc; doc.setHtml(html);
        // Qt serializes selected HTML differently; restoration must survive it.
        QTextCursor cursor(&doc); cursor.select(QTextCursor::Document);
        QVERIFY(restoreStackedFractions(cursor.selection().toHtml()).contains(original));
        auto format = doc.begin().begin().fragment().charFormat().toImageFormat();
        QVariant image = stackedFractionResource(QTextDocument::ImageResource, QUrl(format.name()));
        QVERIFY(!image.value<QImage>().isNull());
        QVERIFY(!stackedFractionResource(QTextDocument::ImageResource, QUrl("file:///tmp/anything")).isValid());
    }
    void search() {
        QString html = stackedFractionHtml("(x+1)/(y-2)", QFont("Arial", 18), Qt::black);
        QTextDocument doc; doc.setHtml("before " + html + " after");
        QTextCursor fraction = findFractionText(&doc, "y-2", 0);
        QVERIFY(!fraction.isNull()); QCOMPARE(fraction.selectedText(), QString(QChar::ObjectReplacementCharacter));
        QVERIFY(findFractionText(&doc, "y-2", fraction.selectionEnd()).isNull());
        QCOMPARE(findFractionText(&doc, "y-2", doc.characterCount()-1, QTextDocument::FindBackward).selectionStart(), fraction.selectionStart());
        QVERIFY(!findFractionText(&doc, "after", 0).isNull());
        QVERIFY(findFractionText(&doc, "missing", 0).isNull());
    }
    void palettes() {
        for(auto color : {QColor(Qt::white), QColor(Qt::black)}) {
            auto image = renderStackedFractions("1/2", QFont("Arial", 18), color);
            int ink = 0;
            for(int y = 0; y < image.height(); ++y) for(int x = 0; x < image.width(); ++x) {
                auto pixel = image.pixelColor(x, y);
                if(pixel.alpha() > 240) { ++ink; QCOMPARE(pixel.red(), color.red()); }
            }
            QVERIFY(ink > 20); QVERIFY(image.pixelColor(0,0).alpha() == 0);
        }
    }
};
QTEST_MAIN(StackedFractionsTest)
#include "stackedfractions-test.moc"
