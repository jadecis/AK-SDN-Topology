#ifndef SSLINK_H
#define SSLINK_H

#include "Link.h"
#include <QMap>
#include "PortMatrix.h"
#include "SelectedSSLinkState.h"
#include "UnselectedSSLinkState.h"
#include "IncludedInPathSSLinkState.h"
#include "IncludedInPathsSSLinkState.h"


class SSLink : public Link
{
public:
    SSLink(Node *node1, Node *node2);
    inline ~SSLink();
    inline float getBandwidth() const;
    inline float getDelay() const;
    inline float getPacketLoss() const;
    inline void setBandwidth(float value);
    inline void setDelay(float value);
    inline void setPacketLossRate(float value);
    inline bool isDisabled() const;
    inline void setDisabled(bool value);

    void draw(NetworkMapDrawer *drawer, PortMatrix portMatrix);
    virtual QSize getSize() const;
    virtual void configure();
    virtual void select();
    virtual void unselect();
    void includeInPath(QColor color);
    void includeInPaths(QColor color);
    virtual DeviceType getDeviceType();

private:
    int generateRandomNumber(int min, int max);

private:
    float packetLossRate;
    float delay;
    float bandwidth;
    bool disabled_;
    SSLinkState *state;
    SelectedSSLinkState selectedState;
    UnselectedSSLinkState unselectedState;
    IncludedInPathSSLinkState includedInPathState;
    IncludedInPathsSSLinkState includedInPathsState;
};

inline SSLink::~SSLink()
{

}

inline float SSLink::getBandwidth() const
{
    return bandwidth;
}

inline float SSLink::getDelay() const
{
    return delay;
}

inline float SSLink::getPacketLoss() const
{
    return packetLossRate;
}

inline void SSLink::setBandwidth(float value)
{
    bandwidth = value;
}

inline void SSLink::setDelay(float value)
{
    delay = value;
}

inline void SSLink::setPacketLossRate(float value)
{
    packetLossRate = value;
}

inline bool SSLink::isDisabled() const
{
    return disabled_;
}

inline void SSLink::setDisabled(bool value)
{
    disabled_ = value;
}

#endif // SSLINK_H
