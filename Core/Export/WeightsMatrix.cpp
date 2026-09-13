#include "WeightsMatrix.h"

QString WeightsMatrix::build(CommutationMatrix<SSLink> graphMatrix, QList<Switch *> switches)
{
    QString weightsMatrix;
    foreach (Switch *sw, switches)
    {
        foreach (Node *node, graphMatrix.getNeighbors(sw))
        {
            SSLink *ln = graphMatrix.getLink(sw, node);
            if (ln != NULL)
            {
                int n1 = sw->getGroupId();
                int n2 = node->getGroupId();
                weightsMatrix.append(QString("%0-%1:%2,%3,%4\n").arg(n1).arg(n2).arg(ln->getDelay()).arg(ln->getBandwidth()).arg(ln->getPacketLoss()));
            }
        }
    }
    return weightsMatrix;
}

