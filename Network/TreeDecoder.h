#ifndef TREEPARSER_H
#define TREEPARSER_H

#include <QList>

class TreeDecoder
{
public:
    QList<QList<int> > decode(QString treeInStr);
};

#endif // TREEPARSER_H
