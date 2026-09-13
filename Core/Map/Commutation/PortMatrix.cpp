#include "PortMatrix.h"

void PortMatrix::addNode(Node *node)
{
    foreach (Node *other, matrix.keys())
    {
        matrix[other].insert(node, closed);
    }
    matrix.insert(node, QMap<Node*, int>());
    foreach (Node *other, matrix.keys())
    {
        matrix[node].insert(other, closed);
    }
}

void PortMatrix::addLink(Link *ln)
{
    Node *node1 = ln->getNode1();
    Node *node2 = ln->getNode2();
    if (!matrix.contains(node1) || !matrix.contains(node2))
    {
        return;
    }
    matrix[node1][node2] = computePortNumber(node1);
    matrix[node2][node1] = computePortNumber(node2);
}

void PortMatrix::removeNode(Node *node)
{
    matrix.remove(node);
    foreach (Node *other, matrix.keys())
    {
        matrix[other].remove(node);
    }
}

void PortMatrix::removeLink(Link *ln)
{
    Node *node1 = ln->getNode1();
    Node *node2 = ln->getNode2();
    matrix[node1][node2] = closed;
    matrix[node2][node1] = closed;
    recomputePortsFor(node1);
    recomputePortsFor(node2);
}


int PortMatrix::computePortNumber(Node *node)
{
    int connectionsNumber = 0;
    foreach (Node *neighbor, matrix[node].keys())
    {
        connectionsNumber += (matrix[node][neighbor] != closed) ? 1 : 0;
    }
    return connectionsNumber + 1;
}

void PortMatrix::recomputePortsFor(Node *node)
{
    int connectionNumber = 0;
    foreach (Node *neighbor, matrix[node].keys())
    {
        bool isSameNode = node->getGlobalId() == neighbor->getGlobalId();
        bool isNodesConnected = matrix[node][neighbor] != closed;
        if (!isSameNode && isNodesConnected)
        {
            matrix[node][neighbor] = ++connectionNumber;
        }
    }
}
