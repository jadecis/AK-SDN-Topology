#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QPixmap>
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
    Core c;
    TopologyScene scene(&c);

    QObject::connect(&c, &Core::signalHighlightTree, [&](QList<QList<int>> t, QColor col) {
        scene.highlightTreeByGroupId(t, col); });
    QObject::connect(&c, &Core::signalHighlightPath, [&](QList<int> p, QColor col) {
        scene.highlightPathByGroupId(p, col); });
    QObject::connect(&c, &Core::signalClearHighlights, [&]() {
        scene.clearAllHighlights(); });
    QObject::connect(&c, &Core::signalRefreshNetworkMapView, [&](QPixmap) {
        scene.rebuildFromModel(); });
    QObject::connect(&c, &Core::signalHighlightSegments, [&](QList<QList<int>> s) {
        scene.highlightSegmentsByGroupId(s); });

    QString path = argc > 1 ? QString::fromUtf8(argv[1])
                            : QString("/home/stu/sdn-demo-ring/topology.sdn.xml");
    c.loadNetworkMap(path);
    scene.rebuildFromModel();

    QString segId = idByName("Жадное сегментирование");
    QString pairId = idByName("Парные переходы");
    qDebug() << "segId=" << segId << "pairId=" << pairId;

    auto render = [&](const char* tag){
        QImage img(2200, 1700, QImage::Format_ARGB32);
        img.fill(Qt::white);
        QPainter pnt(&img);
        scene.render(&pnt);
        pnt.end();
        qDebug() << "   render OK:" << tag;
    };
    render("after rebuild");
    QVariantMap segParams; segParams.insert("k", 5);
    qDebug() << ">> запуск сегментирования";
    c.runRoutingAlgorithmExt(segId, 0, "delay", segParams);
    render("after segmentation");
    qDebug() << ">> запуск парных переходов";
    c.runRoutingAlgorithmExt(pairId, 0, "delay", QVariantMap());
    render("after pair transitions");
    qDebug() << "DONE — без падения";
    return 0;
}
