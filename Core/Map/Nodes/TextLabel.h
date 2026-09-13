#ifndef TEXTLABEL_H
#define TEXTLABEL_H

#include "Node.h"
#include "Devicetype.h"

class TextLabel : public Node
{
public:
    TextLabel(QPoint position, bool quickConfig = false);
    inline DeviceType getDeviceType();
    virtual QSize getSize() const;
    void draw(NetworkMapDrawer *drawer);
    inline QString getContent() const;
    inline void setContent(QString text);
    inline int getFontSize() const { return fontSize_; }
    inline void setFontSize(int pt) { fontSize_ = (pt < 6 ? 6 : (pt > 64 ? 64 : pt)); }
    /* Открывает диалог свойств. Возвращает true если пользователь
     * подтвердил, false если Cancel — это критично при «быстром
     * создании» метки: при Cancel созданный узел должен быть удалён,
     * а не оставлен на канве с дефолтным «Text...». */
    bool configureInteractive();
    virtual void configure();
    virtual void addDataIn(XmlSerializer *builder);

private:
    QString content;
    int fontSize_ = 11;
};

inline DeviceType TextLabel::getDeviceType()
{
    return TEXTLABEL;
}

inline QString TextLabel::getContent() const
{
    return content;
}

inline void TextLabel::setContent(QString text)
{
    content = text;
}

#endif // TEXTLABEL_H
