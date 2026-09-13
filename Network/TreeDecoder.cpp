#include "TreeDecoder.h"


QList<QList<int> > TreeDecoder::decode(QString treeInStr)
{
    QList<QList<int>> tree;
    foreach (QString switchPairInStr, treeInStr.split("-"))
    {
        QList<int> switchPair;
        foreach (QString switchNumberInString, switchPairInStr.split(","))
        {
            switchPair.append(switchNumberInString.toInt());
        }
        tree.append(switchPair);
    }
    return tree;
}
