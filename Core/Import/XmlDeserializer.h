#ifndef XMLDESERIALIZER_H
#define XMLDESERIALIZER_H

#include <QString>

class NetworkMap;

class XmlDeserializer
{
public:
    void deserialize(QString xmlDocument, NetworkMap *networkMap);
};

#endif // XMLDESERIALIZER_H
