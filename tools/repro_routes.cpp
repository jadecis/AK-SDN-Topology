#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QDebug>
#include "MainWindow.h"
#include "Core.h"
#include "TopologyScene.h"
#include "AlgorithmRegistry.h"
#include "IRoutingAlgorithm.h"

static QString idByName(const QString &name)
{
    for (IRoutingAlgorithm *a : AlgorithmRegistry::instance()->all())
        if (a->displayName() == name) return a->id();
    return QString();
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    MainWindow view;
    Core c;
    view.attachCore(&c);
    QObject::connect(&c, &Core::signalHighlightTrees, [&](QList<QList<QList<int>>> t, QList<QColor> cols) {
        if (view.topologyScene()) view.topologyScene()->highlightTreesByGroupId(t, cols); });
    QObject::connect(&c, &Core::signalClearHighlights, [&]() {
        if (view.topologyScene()) view.topologyScene()->clearAllHighlights(); });
    QObject::connect(&c, &Core::signalRefreshNetworkMapView, [&](QPixmap) {
        if (view.topologyScene()) view.topologyScene()->rebuildFromModel(); });
    view.show();
    app.processEvents();

    QString path = argc > 1 ? QString::fromUtf8(argv[1]) : QString("/home/stu/sdn-demo-ring/topology.sdn.xml");
    c.loadNetworkMap(path);
    app.processEvents();

    auto render = [&](const char *tag) {
        QImage img(2200, 1700, QImage::Format_ARGB32); img.fill(Qt::white);
        QPainter p(&img); if (view.topologyScene()) view.topologyScene()->render(&p); p.end();
        qDebug() << "   render OK:" << tag;
    };

    qDebug() << ">> запуск Дейкстры (по умолчанию маршруты от s1)";
    c.runRoutingAlgorithmExt(idByName("Дейкстра"), 0, "delay", QVariantMap());
    app.processEvents(); render("after run");

    for (const QString &s : {"s5", "s9", "s5", "s12"}) {
        qDebug() << ">> toggle routes from" << s << "(was on:" << c.isSwitchRouteHighlighted(s) << ")";
        c.highlightRoutesFromSwitch(s);
        app.processEvents(); render(qPrintable(s));
    }
    qDebug() << ">> eraseMarks";
    c.eraseMarks(); app.processEvents(); render("after erase");
    qDebug() << "DONE — без падения";
    return 0;
}
