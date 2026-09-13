#include "ConnectionClassifier.h"
#include "Node.h"

bool ConnectionClassifier::isCSConnection(Node *node1, Node *node2)
{
    DeviceType device1 = node1->getDeviceType();
    DeviceType device2 = node2->getDeviceType();
    return ((device1 == SDNCONTROLLER && device2 == SWITCH) ||
            (device1 == SWITCH && device2 == SDNCONTROLLER)) ? true : false;
}

bool ConnectionClassifier::isSSConnection(Node *node1, Node *node2)
{
    DeviceType device1 = node1->getDeviceType();
    DeviceType device2 = node2->getDeviceType();
    return (device1 == device2 && device1 == SWITCH) ? true : false;
}

bool ConnectionClassifier::isHSConnection(Node *node1, Node *node2)
{
    DeviceType device1 = node1->getDeviceType();
    DeviceType device2 = node2->getDeviceType();
    return ((device1 == HOST && device2 == SWITCH) ||
            (device1 == SWITCH && device2 == HOST)) ? true : false;
}
