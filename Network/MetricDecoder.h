#ifndef METRICDECODER_H
#define METRICDECODER_H

#include <QString>
#include <QVector>

class MetricDecoder
{
public:
    QVector<float> decode(QString data);
};

#endif // METRICDECODER_H
