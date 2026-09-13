/* OpenFlow switch:
 *
 */

#ifndef SWITCH_H
#define SWITCH_H

#include "Node.h"
#include "SelectedSwitchState.h"
#include "UnselectedSwitchState.h"
#include "IslandPartSwitchState.h"

class Switch : public Node
{
public:
    Switch(QPoint);
    ~Switch();

    void setup(int num);

    virtual void draw(NetworkMapDrawer *drawer);
    virtual DeviceType getDeviceType();
    virtual QSize getSize() const;
    virtual void unselect();
    virtual void select();
    void includeInIsland(QColor color);

private:
    SwitchState *state;
    SelectedSwitchState selectedState;
    UnselectedSwitchState unselectedState;
    IslandPartSwitchState islandPartState;
};
#endif // SWITCH_H
