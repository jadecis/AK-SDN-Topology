#ifndef IO_H
#define IO_H

#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QtXml>

class IO
{
public:
    void writeFile(QString text, QString filePath) const;
    QString readFile(QString filePath) const;
};

#endif // IO_H
