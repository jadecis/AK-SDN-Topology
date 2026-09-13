#include "TextLabel.h"
#include "TextLabelDialog.h"
#include "XmlSerializer.h"
#include "NetworkMapDrawer.h"

TextLabel::TextLabel(QPoint position, bool quickConfig) : content("Текст…")
{
    unselect();
    setPosition(position);
    if (quickConfig)
    {
        configure();
    }
}

QSize TextLabel::getSize() const
{
    QFontMetrics fontMetrics(QFont("Any", fontSize_));
    return fontMetrics.size(Qt::TextExpandTabs, content);
}

void TextLabel::draw(NetworkMapDrawer *drawer)
{
    if (isSelected)
    {
        drawer->drawSelectedText(content, getPosition());
    }
    else
    {
        drawer->drawText(content, getPosition());
    }
}

void TextLabel::configure()
{
    /* Старый путь для обратной совместимости (XML deserializer, контекстное
     * меню «Свойства»): запускаем диалог без проверки результата. */
    TextLabelDialog dialog(this);
    dialog.exec();
}

bool TextLabel::configureInteractive()
{
    /* Возвращает true только если пользователь нажал OK. Используется при
     * создании метки через палитру: при Cancel вызвавший должен удалить
     * объект, чтобы пустая метка с дефолтным текстом не появлялась на
     * канве. */
    TextLabelDialog dialog(this);
    return dialog.exec() == QDialog::Accepted;
}

void TextLabel::addDataIn(XmlSerializer *builder)
{
    builder->addTextLabelData(this);
}
