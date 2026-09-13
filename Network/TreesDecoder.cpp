#include "TreesDecoder.h"


QList<QList<QList<int> > > TreesDecoder::decode(QString treesInStr)
{
    QList<QList<QList<int> > > trees;
    foreach (QString treeInStr, treesInStr.split(";"))
    {
        QList<QList<int>> tree;
        foreach (QString switchPairInStr, treeInStr.split(","))
        {
            QList<int> switchPair;
            foreach (QString switchNumberInString, switchPairInStr.split("-"))
            {
                switchPair.append(switchNumberInString.toInt());
            }
            tree.append(switchPair);
        }
        trees.append(tree);
    }
    return trees;
}
