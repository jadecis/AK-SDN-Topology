#ifndef PATHSDECODER_H
#define PATHSDECODER_H

#include "QList"

class PathsDecoder
{
public:
    QList<QList<int>> decode(QString pathsInString);
};

#endif // PATHSDECODER_H
