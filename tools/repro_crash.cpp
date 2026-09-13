#include <QApplication>
#include <QPixmap>
#include <QString>
#include <QDebug>
#include "NetworkMap.h"
#include "XmlDeserializer.h"
#include "IO.h"
#include "DijkstraAlgorithm.h"
#include "Graph.h"
#include "Switch.h"
#include "SSLink.h"
#include "CommutationMatrix.h"
#include "DrawingMode.h"

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QString path = argc > 1 ? QString::fromUtf8(argv[1])
                            : QString("/home/stu/sdn-demo-ring/topology.sdn.xml");
    DrawingMode::showDelay();   // как в конструкторе Core
    IO io;
    QString xml = io.readFile(path);
    NetworkMap map;
    XmlDeserializer d;
    d.deserialize(xml, &map);
    QList<Switch *> switches = map.getSwitches();
    qDebug() << "switches:" << switches.size();

    CommutationMatrix<SSLink> commGraph = map.getGraphMatrix();
    int n = switches.size();
    Graph w(n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            SSLink *ln = commGraph.getLink(switches[i], switches[j]);
            if (!ln) continue;
            if (ln->isDisabled()) continue;
            double weight = ln->getDelay();
            if (weight <= 0.0) weight = 1.0;
            w.set(i, j, weight);
        }

    DijkstraAlgorithm dj;
    for (int root = 0; root < n; ++root) {
        QList<QList<int>> tree = dj.computeTree(w, root);
        QList<QList<int>> named;
        for (const QList<int> &e : tree) {
            if (e.size() != 2) continue;
            named.append({switches[e[0]]->getGroupId(), switches[e[1]]->getGroupId()});
        }
        qDebug() << "root" << root << "tree edges:" << named.size();
        map.unselectLinks();
        map.visualizeTree(named);
        QPixmap pm = map.draw();   // <-- как refreshNetworkMap()
        (void)pm;
    }
    qDebug() << "DONE — без падения";
    return 0;
}
