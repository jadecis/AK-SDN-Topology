#include "NetworkVisualizer.h"
#include "SearchEngine.h"
#include "QSet"

NetworkVisualizer::NetworkVisualizer(CommutationMatrix<SSLink> commutationMatrix,
                                     QList<Switch *> switches)
{
    this->commutationMatrix = commutationMatrix;
    this->switches = switches;
    islandColors =
    {
        QColor(220, 20, 20, 95),
        QColor(0, 255, 20, 95),
        QColor(0, 255, 255, 95),
        QColor(0, 0, 255, 95),
        QColor(255, 0, 255, 95),
        QColor(200, 80, 0, 95),
    };
    pathColors =
    {
        QColor(220, 20, 20),
        QColor(0, 255, 20),
        QColor(0, 255, 255),
        QColor(0, 0, 255),
        QColor(255, 0, 255),
        QColor(200, 80, 0),
    };
}

void NetworkVisualizer::visualizePath(QList<int> path,
                                      QColor color,
                                      QList<SSLink *> crossingLines)
{
    QList<Node *> nodeSequence = convertPathToNodeSequence(path);
    for (int i = 0; i < nodeSequence.length() - 1; i++)
    {
        SSLink *ln = commutationMatrix.getLink(nodeSequence[i], nodeSequence[i + 1]);
        if (crossingLines.contains(ln))
        {
            ln->includeInPaths(color);
        }
        else
        {
            ln->includeInPath(color);
        }
    }
}

void NetworkVisualizer::visualizePaths(QList<QList<int> > paths)
{
    QList<SSLink *> temporaryLinks;
    QList<SSLink *> crossingLinks;
    foreach (SSLink *ln, edjectLinksFrom(paths))
    {
        if (!temporaryLinks.contains(ln))
        {
            temporaryLinks.append(ln);
        }
        else
        {
            if (!crossingLinks.contains(ln))
            {
                crossingLinks.append(ln);
            }
        }
    }

    int current = 0;
    foreach (QList<int> path, paths)
    {
        visualizePath(path, pathColors[current], crossingLinks);
        if (current == pathColors.count() - 1)
        {
            current = 0;
        }
        else
        {
            current++;
        }
    }
}

void NetworkVisualizer::visualizeTree(QList<QList<int> > tree, QColor color)
{
    QList<SSLink*> crossingLinks;
    foreach (QList<int> path, tree)
    {
        visualizePath(path, color, crossingLinks);
    }
}

void NetworkVisualizer::visualizeTrees(QList<QList<QList<int> > > trees)
{
    int current = 0;
    foreach (QList<QList<int> > tree, trees)
    {
        visualizeTree(tree, pathColors[current]);
        if (current == pathColors.count() - 1)
        {
            current = 0;
        }
        else
        {
            current++;
        }
    }
}

void NetworkVisualizer::visualizeIslands(QList<QList<int> > islands)
{
    int current = 0;
    foreach (QList<int> island, islands)
    {
        QList<Switch *> switchesInIsland = convertIslandToSwitchSet(island);
        foreach (Switch *sw, switchesInIsland)
        {
            sw->includeInIsland(islandColors[current]);
        }

        if (current == islandColors.count() - 1)
        {
            current = 0;
        }
        else
        {
            current++;
        }
    }
}

QList<Switch *> NetworkVisualizer::convertIslandToSwitchSet(QList<int> island)
{
    SearchEngine search;
    QList<Switch *> switchSet;
    foreach (int id, island)
    {
        Switch *sw = search.findSwitch(switches, id);
        if (sw != NULL)
        {
            switchSet.append(sw);
        }
    }
    return switchSet;
}

QList<SSLink *> NetworkVisualizer::edjectLinksFrom(QList<QList<int> > paths)
{
    QList<SSLink *> links;
    foreach (QList<int> path, paths)
    {
        QList<Node *> nodeSequence = convertPathToNodeSequence(path);
        for (int i = 0; i < nodeSequence.length() - 1; i++)
        {
            SSLink *ln = commutationMatrix.getLink(nodeSequence[i], nodeSequence[i + 1]);
            links.append(ln);
        }
    }
    return links;
}


QList<Node *> NetworkVisualizer::convertPathToNodeSequence(QList<int> path)
{
    SearchEngine search;
    QList<Node *> nodeSequence;
    foreach (int id, path)
    {
        Node *node = search.findNode(switches, id);
        if (node != NULL)
        {
            nodeSequence.append(node);
        }
    }
    return nodeSequence;
}


