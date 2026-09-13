#ifndef PORTMATRIX_H
#define PORTMATRIX_H

#include <QMap>
#include "Link.h"
#include "Node.h"


class PortMatrix
{
private:
    QMap<Node*, QMap<Node*, int>> matrix;

public:
    void addNode(Node *node);
    void addLink(Link *ln);
    void removeNode(Node *node);
    void removeLink(Link *ln);
    inline int getPortNumber(Node *node1, Node *node2);

private:
    int computePortNumber(Node *node);
    void recomputePortsFor(Node *node);
    const int closed = -1;
};

inline int PortMatrix::getPortNumber(Node *node1, Node *node2)
{
    if (!matrix.contains(node1) || !matrix[node1].contains(node2))
    {
        return closed;
    }
    return matrix[node1][node2];
}

#endif // PORTMATRIX_H
