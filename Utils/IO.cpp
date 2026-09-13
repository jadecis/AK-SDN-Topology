#include "IO.h"
#include <QTextStream>
#include <QFile>

void IO::writeFile(QString text, QString filePath) const
{
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        file.write(text.toUtf8());
        file.close();
    }
}

QString IO::readFile(QString filePath) const
{
    QString text;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        text = QString(file.readAll());
        file.close();
    }
    return text;
}


