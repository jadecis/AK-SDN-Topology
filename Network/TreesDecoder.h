#ifndef TREESDECODER_H
#define TREESDECODER_H

#include <QList>

class TreesDecoder
{
public:
    QList<QList<QList<int> > > decode(QString treesInStr);
};

#endif // TREESDECODER_H
