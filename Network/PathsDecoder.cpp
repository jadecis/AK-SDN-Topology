#include "PathsDecoder.h"

// The paths representation: 1,2,3;6,5,4;9,11,16
QList<QList<int> > PathsDecoder::decode(QString pathsInString)
{
    QList<QList<int>> paths;
    foreach (QString pathInString, pathsInString.split(";"))
    {
        QList<int> path;
        foreach (QString switchNumberInString, pathInString.split(","))
        {
            path.append(switchNumberInString.toInt());
        }
        paths.append(path);
    }
    return paths;
}
