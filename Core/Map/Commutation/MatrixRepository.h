#ifndef MATRIXREPOSITORY_H
#define MATRIXREPOSITORY_H

#include "Link.h"
#include "Switch.h"
#include "SSLink.h"
#include "Host.h"
#include "CommutationMatrix.h"
#include "PortMatrix.h"

class DockerNode;

class MatrixRepository
{
public:
    MatrixRepository();
    void registerNode(Node *node);
    void registerHost(Host *hst);
    void registerDocker(DockerNode *d);
    void registerLink(Link *ln);
    void registerSwitch(Switch *sw);
    void registerSSLink(SSLink *ln);
    void unregisterHost(Host *hst);
    void unregisterDocker(DockerNode *d);
    void unregisterNode(Node *node);
    void unregisterLink(Link *ln);
    void unregisterSwitch(Switch *sw);
    void unregisterSSLink(SSLink *ln);

    inline CommutationMatrix<Link> getGlobalMatrix() const;
    inline CommutationMatrix<SSLink> getGraphMatrix() const;
    inline PortMatrix getPortMatrix() const;

private:
    CommutationMatrix<Link> globalMatrix;
    CommutationMatrix<SSLink> graphMatrix;
    PortMatrix portMatrix;
};

inline CommutationMatrix<Link> MatrixRepository::getGlobalMatrix() const
{
    return globalMatrix;
}

inline CommutationMatrix<SSLink> MatrixRepository::getGraphMatrix() const
{
    return graphMatrix;
}

inline PortMatrix MatrixRepository::getPortMatrix() const
{
    return portMatrix;
}


#endif // MATRIXREPOSITORY_H
