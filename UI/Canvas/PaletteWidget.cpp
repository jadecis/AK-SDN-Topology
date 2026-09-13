#include "PaletteWidget.h"
#include "ThemeManager.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>
#include <QButtonGroup>
#include <QFrame>
#include <QApplication>

/* DraggableToolButton — QToolButton, который дополнительно умеет начать
 * drag-операцию с указанным MIME-типом. Если drag не начат — работает
 * как обычная checkable-кнопка инструмента. */
class DraggableToolButton : public QToolButton
{
public:
    DraggableToolButton(const QString &dragKind, const QString &iconRes, QWidget *parent)
        : QToolButton(parent), dragKind_(dragKind), iconRes_(iconRes), pressed_(false)
    {
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            pressed_ = true;
            pressPos_ = event->pos();
        }
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!pressed_ || dragKind_.isEmpty())
        {
            QToolButton::mouseMoveEvent(event);
            return;
        }
        if ((event->pos() - pressPos_).manhattanLength() < QApplication::startDragDistance())
        {
            return;
        }
        pressed_ = false;
        QMimeData *mime = new QMimeData;
        mime->setData("application/x-snet-tool", dragKind_.toUtf8());
        QDrag *drag = new QDrag(this);
        drag->setMimeData(mime);
        QIcon icon = ThemeManager::instance()->makeIcon(
            iconRes_, ThemeManager::instance()->accentColor(), QSize(48, 48));
        drag->setPixmap(icon.pixmap(48, 48));
        drag->setHotSpot(QPoint(24, 24));
        drag->exec(Qt::CopyAction);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        pressed_ = false;
        QToolButton::mouseReleaseEvent(event);
    }

private:
    QString dragKind_;
    QString iconRes_;
    bool pressed_;
    QPoint pressPos_;
};


PaletteWidget::PaletteWidget(QWidget *parent) :
    QWidget(parent),
    activeTool_("select"),
    group_(new QButtonGroup(this))
{
    setMinimumWidth(110);
    setMaximumWidth(130);
    setStyleSheet("QWidget { background: transparent; }");
    group_->setExclusive(true);
    buildButtons();
}

QToolButton *PaletteWidget::addToolButton(const QString &id, const QString &iconRes,
                                          const QString &label, const QString &tip,
                                          bool draggable, const QString &shortcut)
{
    DraggableToolButton *btn = new DraggableToolButton(
        draggable ? id : QString(), iconRes, this);
    btn->setCheckable(true);
    QIcon icon = ThemeManager::instance()->makeIcon(
        iconRes, ThemeManager::instance()->accentColor(), QSize(28, 28));
    btn->setIcon(icon);
    btn->setIconSize(QSize(28, 28));
    btn->setText(label);
    QString full = tip;
    if (!shortcut.isEmpty()) full += QString(" [%1]").arg(shortcut);
    btn->setToolTip(full);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setFixedSize(96, 64);
    /* Стиль кнопки задаётся через тему (QSS, селектор #paletteButton).
     * Раньше жёстко прописанные светлые цвета ломали тёмную тему —
     * кнопки оставались белыми на тёмном фоне (баг #4 из отчёта). */
    btn->setObjectName("paletteButton");
    btn->setProperty("toolId", id);
    if (!shortcut.isEmpty()) btn->setShortcut(QKeySequence(shortcut));
    group_->addButton(btn);
    buttons_.append(btn);
    connect(btn, &QToolButton::clicked, this, &PaletteWidget::onButtonClicked);
    return btn;
}

void PaletteWidget::buildButtons()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    QLabel *grp1 = new QLabel(tr("Инструменты"), this);
    grp1->setAlignment(Qt::AlignCenter);
    grp1->setStyleSheet("color:#6B7280; font-size: 8pt; font-weight:600;");
    layout->addWidget(grp1);

    QToolButton *bSel = addToolButton("select", ":/modern/modern/edit.svg",
                                       tr("Выбор"),
                                       tr("Выделение и редактирование"),
                                       false, "E");
    bSel->setChecked(true);
    layout->addWidget(bSel);

    layout->addWidget(addToolButton("link", ":/modern/modern/link.svg",
                                    tr("Канал"),
                                    tr("Создание канала: клик по двум узлам"),
                                    false, "L"));

    layout->addWidget(addToolButton("delete", ":/modern/modern/clear_marks.svg",
                                    tr("Ластик"),
                                    tr("Удаление: клик по узлу/каналу"),
                                    false, "D"));

    /* Разделитель */
    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#E5E7EB;");
    layout->addWidget(sep);

    QLabel *grp2 = new QLabel(tr("Добавить"), this);
    grp2->setAlignment(Qt::AlignCenter);
    grp2->setStyleSheet("color:#6B7280; font-size: 8pt; font-weight:600;");
    layout->addWidget(grp2);

    layout->addWidget(addToolButton("host", ":/modern/modern/host.svg",
                                    tr("Хост"),
                                    tr("Хост: клик по холсту или перетащить"),
                                    true, "H"));

    layout->addWidget(addToolButton("switch", ":/modern/modern/switch.svg",
                                    tr("Свитч"),
                                    tr("Коммутатор: клик по холсту или перетащить"),
                                    true, "S"));

    layout->addWidget(addToolButton("controller", ":/modern/modern/controller.svg",
                                    tr("Контроллер"),
                                    tr("SDN-контроллер: клик по холсту или перетащить"),
                                    true, "C"));

    layout->addWidget(addToolButton("docker", ":/modern/modern/docker.svg",
                                    tr("Docker"),
                                    tr("Docker-контейнер: клик по холсту или перетащить"),
                                    true, "K"));

    /* Текстовая метка — соответствует пункту T из методички (рис. 1.4):
     * Tools → Text. Иконка SVG может отсутствовать в теме (тогда
     * makeIcon просто отдаст пустую) — это нормально, кнопка
     * подписывается текстом. */
    layout->addWidget(addToolButton("text", ":/modern/modern/text.svg",
                                    tr("Текст"),
                                    tr("Текстовая метка: клик по холсту — поставить надпись"),
                                    false, "T"));

    QLabel *hint = new QLabel(tr("• ЛКМ — выделить\n• Space+ЛКМ — pan\n• Del — удалить\n• Shift+ЛКМ — канал"), this);
    hint->setAlignment(Qt::AlignLeft);
    hint->setStyleSheet("color:#9CA3AF; font-size: 7pt;");
    hint->setWordWrap(true);
    layout->addSpacing(8);
    layout->addWidget(hint);

    layout->addStretch();
}

void PaletteWidget::onButtonClicked()
{
    QToolButton *btn = qobject_cast<QToolButton *>(sender());
    if (!btn) return;
    QString id = btn->property("toolId").toString();
    if (id.isEmpty()) return;
    activeTool_ = id;
    emit toolSelected(id);
}

void PaletteWidget::setActiveTool(const QString &id)
{
    activeTool_ = id;
    for (QToolButton *b : buttons_)
    {
        if (b->property("toolId").toString() == id)
        {
            b->setChecked(true);
            return;
        }
    }
}
