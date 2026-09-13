#ifndef CONNECTIONCLASSIFIER_H
#define CONNECTIONCLASSIFIER_H

class Node;

class ConnectionClassifier
{
public:
    bool isSSConnection(Node *node1, Node *node2);
    bool isCSConnection(Node *node1, Node *node2);
    bool isHSConnection(Node *node1, Node *node2);
};

#endif // CONNECTIONCLASSIFIER_H
