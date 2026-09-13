#ifndef COMMUTATIONMATRIX_H
#define COMMUTATIONMATRIX_H

#include <QMap>
#include "Node.h"


template<class LinkType>
class CommutationMatrix
{
private:
    QMap<Node*, QMap<Node*, LinkType*>> matrix;

public:
    void addNode(Node *node);
    void addLink(LinkType *ln);
    void removeNode(Node *node);
    void removeLink(LinkType *ln);
    inline LinkType *getLink(Node *node1, Node *node2);
    QList<Node *> getNeighbors(Node *node);
};

template<class LinkType>
inline LinkType *CommutationMatrix<LinkType>::getLink(Node *node1, Node *node2)
{
    if (!matrix.contains(node1) || !matrix[node1].contains(node2))
    {
        return NULL;
    }
    return matrix[node1][node2];
}

template<class LinkType>
void CommutationMatrix<LinkType>::addNode(Node *node)
{
    foreach (Node *other, matrix.keys())
    {
        matrix[other].insert(node, NULL);
    }
    matrix.insert(node, QMap<Node*, LinkType*>());
    foreach (Node *other, matrix.keys())
    {
        matrix[node].insert(other, NULL);
    }
}

template<class LinkType>
void CommutationMatrix<LinkType>::addLink(LinkType *ln)
{
    Node *node1 = ln->getNode1();
    Node *node2 = ln->getNode2();
    if (!matrix.contains(node1) || !matrix.contains(node2))
    {
        return;
    }
    matrix[node1][node2] = ln;
    matrix[node2][node1] = ln;
}

template<class LinkType>
void CommutationMatrix<LinkType>::removeNode(Node *node)
{
    foreach (Node *other, matrix.keys())
    {
        matrix[other].remove(node);
    }
    matrix.remove(node);
}

template<class LinkType>
void CommutationMatrix<LinkType>::removeLink(LinkType *ln)
{
    Node *node1 = ln->getNode1();
    Node *node2 = ln->getNode2();
    matrix[node1][node2] = NULL;
    matrix[node2][node1] = NULL;
}

template<class LinkType>
QList<Node *> CommutationMatrix<LinkType>::getNeighbors(Node *node)
{
    QList<Node *> result;
    if (!matrix.contains(node))
    {
        return result;
    }
    foreach (Node *neighbor, matrix[node].keys())
    {
        result.append(neighbor);
    }
    result.removeOne(node);
    return result;
}

#endif // COMMUTATIONMATRIX_H
