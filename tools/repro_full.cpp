#include <QApplication>
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

    /* Те же связи Core→сцена, что в main.cpp. */
    QObject::connect(&c, &Core::signalHighlightTree, [&](QList<QList<int>> t, QColor col) {
        if (view.topologyScene()) view.topologyScene()->highlightTreeByGroupId(t, col); });
    QObject::connect(&c, &Core::signalHighlightPath, [&](QList<int> p, QColor col) {
        if (view.topologyScene()) view.topologyScene()->highlightPathByGroupId(p, col); });
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

    QString segId = idByName("Жадное сегментирование");
    QString pairId = idByName("Парные переходы");
    qDebug() << "segId=" << segId << "pairId=" << pairId;

    int S = c.getMap()->getSwitches().size();
    QString laracId = idByName("LARAC");
    QString dijId = idByName("Дейкстра");
    QStringList metrics = {"delay","bandwidth","loss"};
    auto run=[&](const QString&id,int root,const QString&m,QVariantMap pr){
        c.runRoutingAlgorithmExt(id, root, m, pr); app.processEvents();
    };
    for (int pass=0; pass<2; ++pass) {
      for (const QString &m : metrics) {
        for (int k=2; k<=qMin(S,6); ++k){ QVariantMap pr; pr.insert("k",k); run(segId,0,m,pr); }
        for (int root=0; root<S; ++root){
            run(dijId, root, m, QVariantMap());
            run(pairId, root, m, QVariantMap());
            QVariantMap lp; lp.insert("metric1",m); lp.insert("metric2","loss");
            lp.insert("restriction1",30.0); lp.insert("restriction2",10.0); lp.insert("lambd",0.5);
            run(laracId, root, m, lp);
        }
        qDebug() << "  swept metric" << m << "pass" << pass;
      }
    }
    qDebug() << "DONE — без падения";
    return 0;
}
