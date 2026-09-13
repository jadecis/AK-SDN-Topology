#ifndef NETWORKVIZUALIZER_H
#define NETWORKVIZUALIZER_H

#include "CommutationMatrix.h"
#include "Switch.h"
#include "SSLink.h"

class NetworkVisualizer
{
public:
    NetworkVisualizer(CommutationMatrix<SSLink> commutationMatrix,
                      QList<Switch *> switches);
    inline void setGraphMatrix(CommutationMatrix<SSLink> commutationMatrix);
    void visualizePath(QList<int> path,
                       QColor color,
                       QList<SSLink *> crossingLines = QList<SSLink *>());
    void visualizePaths(QList<QList<int> > paths);
    void visualizeTree(QList<QList<int> > tree, QColor color);
    void visualizeTrees(QList<QList<QList<int> > > trees);
    void visualizeIslands(QList<QList<int> > islands);

private:
    QList<Node *> convertPathToNodeSequence(QList<int> path);
    QList<Switch *> convertIslandToSwitchSet(QList<int> island);
    QList<SSLink *> edjectLinksFrom(QList<QList<int> > paths);

private:
    CommutationMatrix<SSLink> commutationMatrix;
    QList<Switch *> switches;
    QList<QColor> islandColors;
    QList<QColor> pathColors;
};

inline void NetworkVisualizer::setGraphMatrix(CommutationMatrix<SSLink> graphMatrix)
{
    this->commutationMatrix = graphMatrix;
}

#endif // NETWORKVIZUALIZER_H
