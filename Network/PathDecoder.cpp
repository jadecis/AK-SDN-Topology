#include "PathDecoder.h"


// The path representation: 1,2,3,6,5,4
QList<int> PathDecoder::decode(QString path)
{
    QList<int> decodedPath;
    foreach (QString switchNumberInString, path.split(","))
    {
        decodedPath.append(switchNumberInString.toInt());
    }
    return decodedPath;
}
