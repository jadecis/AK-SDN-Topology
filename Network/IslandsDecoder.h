#ifndef COMMANDCONTROLLER_H
#define COMMANDCONTROLLER_H

#include <QString>
#include <QList>

class IslandsDecoder
{
public:
    QList<QList<int>> decode(QString islandsInString);
};

#endif // COMMANDCONTROLLER_H
