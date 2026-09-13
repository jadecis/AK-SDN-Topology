#include "IslandsDecoder.h"

// The islands representation: 1,2,3;4,5,6;7,8
QList<QList<int> > IslandsDecoder::decode(QString islandsInString)
{
    QList<QList<int>> islands;
    foreach (QString islandInString, islandsInString.split(";"))
    {
        QList<int> island;
        foreach (QString switchNumberInString, islandInString.split(","))
        {
            island.append(switchNumberInString.toInt());
        }
        islands.append(island);
    }
    return islands;
}
