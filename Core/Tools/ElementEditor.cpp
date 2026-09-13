#include "ElementEditor.h"

ElementEditor::ElementEditor(NetworkMap *map) :
    networkMap(map)
{

}

void ElementEditor::handleMouseUp(QPoint position)
{
    SelectionFrame *frame = networkMap->getSelectionFrame();
    if (frame->isActive())
    {
        foreach (Node *n, networkMap->getNodesBySelectedFrame())
        {
            n->select();
            selectedElements.append(n);
            selectedElementsOffsets.append(QSize());
        }
        frame->deactivate();
    }
}

void ElementEditor::handleMouseDown(QPoint position)
{
    Element *e = networkMap->getElementByPosition(position);
    if (e == NULL)
    {
        foreach (Element *e, selectedElements)
        {
            e->unselect();
        }
        selectedElements.clear();
        selectedElementsOffsets.clear();
        networkMap->getSelectionFrame()->activate(position);
    }
    else if(!selectedElements.contains(e))
    {
        foreach (Element *e, selectedElements)
        {
            e->unselect();
        }
        selectedElements.clear();
        selectedElementsOffsets.clear();
        e->select();
        selectedElements.append(e);
        selectedElementsOffsets.append(QSize());
    }
    else
    {
        for(int e = 0; e < selectedElementsOffsets.count(); e++)
        {
            selectedElementsOffsets[e] = QSize(position.x() - selectedElements[e]->getPosition().x(),
                                               position.y() - selectedElements[e]->getPosition().y());
        }
    }
}

bool ElementEditor::handleMouseMove(QPoint position)
{
    SelectionFrame *frame = networkMap->getSelectionFrame();
    if (frame->isActive())
    {
        frame->setDynamicPoint(position);
        return true;
    }
    if (!selectedElements.isEmpty())
    {
        for(int e = 0; e < selectedElements.count(); e++)
        {
            selectedElements[e]->setPosition(QPoint(position.x() - selectedElementsOffsets[e].width(),
                                                    position.y() - selectedElementsOffsets[e].height()));
        }
        return true;
    }
    return false;
}

void ElementEditor::handleMouseDoubleClicked(QPoint position)
{
    Element *e = networkMap->getElementByPosition(position);
    if (e != NULL)
    {
        e->configure();
    }
}

void ElementEditor::handleKeyDeletePressEvent()
{
    foreach (Element *e, selectedElements)
    {
        networkMap->removeElement(e);
    }
    selectedElements.clear();
    selectedElementsOffsets.clear();
}

void ElementEditor::AlignVertically()
{
    if (selectedElements.count() == 0) {
        return;
    }
    int x = selectedElements[0]->getPosition().x();
    foreach (Element *e, selectedElements) {
        e->setPosition(QPoint(x, e->getPosition().y()));
    }
}

void ElementEditor::AlignHorizontally()
{
    if (selectedElements.count() == 0) {
        return;
    }
    int y = selectedElements[0]->getPosition().y();
    foreach (Element *e, selectedElements) {
        e->setPosition(QPoint(e->getPosition().x(), y));
    }
}
