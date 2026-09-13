#ifndef WEIGHTSMATRIX_H
#define WEIGHTSMATRIX_H

#include <CommutationMatrix.h>
#include "Switch.h"
#include "SSLink.h"
#include <QList>


class WeightsMatrix
{
public:
    QString build(CommutationMatrix<SSLink> graphMatrix, QList<Switch *> switches);
};

#endif // WEIGHTSMATRIX_H
