#include "ThemeManager.h"
#include <QApplication>
#include <QFile>
#include <QSettings>
#include <QTextStream>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

ThemeManager *ThemeManager::instance()
{
    static ThemeManager s;
    return &s;
}

ThemeManager::ThemeManager(QObject *parent) :
    QObject(parent),
    theme(Light)
{
}

ThemeManager::Theme ThemeManager::currentTheme() const
{
    return theme;
}

QString ThemeManager::readStyleSheet(Theme t) const
{
    QString path = (t == Dark)
        ? QStringLiteral(":/themes/themes/dark.qss")
        : QStringLiteral(":/themes/themes/light.qss");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString();
    }
    QTextStream in(&f);
    return in.readAll();
}

void ThemeManager::applyTheme(Theme t)
{
    theme = t;
    QString qss = readStyleSheet(t);
    if (qApp)
    {
        qApp->setStyleSheet(qss);
    }
    QSettings settings("SNet", "Editor");
    settings.setValue("theme/dark", t == Dark);
    emit themeChanged(t);
}

void ThemeManager::toggleTheme()
{
    applyTheme(theme == Dark ? Light : Dark);
}

void ThemeManager::loadPersistedTheme()
{
    QSettings settings("SNet", "Editor");
    bool dark = settings.value("theme/dark", false).toBool();
    applyTheme(dark ? Dark : Light);
}

QColor ThemeManager::iconColor() const
{
    return theme == Dark ? QColor("#E2E8F0") : QColor("#1F2937");
}

QColor ThemeManager::accentColor() const
{
    return theme == Dark ? QColor("#5BA0FF") : QColor("#2D6CDF");
}

QColor ThemeManager::successColor() const
{
    return theme == Dark ? QColor("#22C55E") : QColor("#16A34A");
}

QColor ThemeManager::dangerColor() const
{
    return theme == Dark ? QColor("#EF4444") : QColor("#DC2626");
}

QIcon ThemeManager::makeIcon(const QString &resourcePath, const QSize &size) const
{
    return makeIcon(resourcePath, iconColor(), size);
}

QIcon ThemeManager::makeIcon(const QString &resourcePath, const QColor &color,
                             const QSize &size) const
{
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly))
    {
        return QIcon();
    }
    QByteArray data = f.readAll();
    data.replace("currentColor", color.name().toUtf8());

    QSvgRenderer renderer(data);
    if (!renderer.isValid())
    {
        return QIcon();
    }

    qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QSize px(qRound(size.width() * dpr), qRound(size.height() * dpr));
    QPixmap pixmap(px);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter);
    painter.end();
    pixmap.setDevicePixelRatio(dpr);
    return QIcon(pixmap);
}
