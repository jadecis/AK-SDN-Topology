#include <QApplication>
#include <QComboBox>
#include <QMetaObject>
#include <QDebug>
#include "MainWindow.h"
#include "Core.h"
#include "TopologyScene.h"
#include "AlgorithmDialog.h"

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    MainWindow view;
    Core c;
    view.attachCore(&c);
    QObject::connect(&c, &Core::signalHighlightTree, [&](QList<QList<int>> t, QColor col) {
        if (view.topologyScene()) view.topologyScene()->highlightTreeByGroupId(t, col); });
    QObject::connect(&c, &Core::signalClearHighlights, [&]() {
        if (view.topologyScene()) view.topologyScene()->clearAllHighlights(); });
    QObject::connect(&c, &Core::signalHighlightSegments, [&](QList<QList<int>> s) {
        if (view.topologyScene()) view.topologyScene()->highlightSegmentsByGroupId(s); });
    view.show();
    app.processEvents();

    QString path = argc > 1 ? QString::fromUtf8(argv[1])
                            : QString("/home/stu/sdn-demo-ring/topology.sdn.xml");
    c.loadNetworkMap(path);
    app.processEvents();

    AlgorithmDialog dlg(c.getMap(), &view);
    QObject::connect(&dlg, &AlgorithmDialog::signalRunWithParams,
                     &c, &Core::runRoutingAlgorithmExt);
    dlg.show();
    app.processEvents();

    // найдём комбо алгоритмов (в нём есть пункт "Парные переходы")
    QComboBox *algoCombo = nullptr;
    for (QComboBox *cb : dlg.findChildren<QComboBox *>()) {
        for (int i = 0; i < cb->count(); ++i)
            if (cb->itemText(i) == QStringLiteral("Парные переходы")) { algoCombo = cb; break; }
        if (algoCombo) break;
    }
    qDebug() << "algoCombo found:" << (algoCombo != nullptr);

    auto pick = [&](const QString &name) {
        for (int i = 0; i < algoCombo->count(); ++i)
            if (algoCombo->itemText(i) == name) { algoCombo->setCurrentIndex(i); break; }
        app.processEvents();
    };

    qDebug() << ">> выбираю и запускаю Жадное сегментирование";
    pick("Жадное сегментирование");
    QMetaObject::invokeMethod(&dlg, "onRunClicked");
    app.processEvents();

    qDebug() << ">> выбираю Парные переходы (onAlgorithmChanged)";
    pick("Парные переходы");
    qDebug() << ">> запускаю Парные переходы";
    QMetaObject::invokeMethod(&dlg, "onRunClicked");
    app.processEvents();

    qDebug() << "DONE — без падения";
    return 0;
}
