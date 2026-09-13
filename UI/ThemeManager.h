#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QIcon>

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum Theme {
        Light = 0,
        Dark  = 1
    };

    static ThemeManager *instance();

    Theme currentTheme() const;
    void applyTheme(Theme theme);
    void toggleTheme();
    void loadPersistedTheme();

    /* Цвет иконок (для замены currentColor в SVG) */
    QColor iconColor() const;
    QColor accentColor() const;
    QColor successColor() const;
    QColor dangerColor() const;

    /* Создать QIcon из SVG-ресурса с подменой currentColor под текущую тему */
    QIcon makeIcon(const QString &resourcePath, const QSize &size = QSize(28, 28)) const;
    QIcon makeIcon(const QString &resourcePath, const QColor &color,
                   const QSize &size = QSize(28, 28)) const;

signals:
    void themeChanged(Theme newTheme);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    QString readStyleSheet(Theme theme) const;
    Theme theme;
};

#endif // THEMEMANAGER_H
