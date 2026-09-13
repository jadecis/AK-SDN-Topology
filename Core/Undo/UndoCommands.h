#ifndef UNDOCOMMANDS_H
#define UNDOCOMMANDS_H

#include <QString>
#include <QStringList>

/* UndoManager — снимочный (snapshot-based) механизм undo/redo для NetworkMap.
 *
 * Каждое изменение модели (добавление/удаление узла или канала, перемещение,
 * правка свойств, отключение канала) сохраняет XML-снимок состояния ДО
 * изменения. Откат/повтор восстанавливают модель через тот же проверенный
 * путь, что и загрузка проекта (XmlDeserializer::deserialize → map.clear()
 * + пересборка), поэтому не зависит от тонкостей владения объектами Node/Link.
 *
 * Модель работы (две очереди):
 *   push(pre)        — перед изменением: pre кладётся в undo-стек, redo очищается.
 *   undo(current)    — current уходит в redo, возвращается верхушка undo.
 *   redo(current)    — current уходит в undo, возвращается верхушка redo.
 */
class UndoManager
{
public:
    explicit UndoManager(int limit = 50) : limit_(limit) {}

    /* Сброс истории (новый/открытый проект). */
    void clear()
    {
        undo_.clear();
        redo_.clear();
    }

    /* Зафиксировать предыдущее (до изменения) состояние. */
    void push(const QString &preState)
    {
        undo_.append(preState);
        while (undo_.size() > limit_)
            undo_.removeFirst();
        redo_.clear();
    }

    bool canUndo() const { return !undo_.isEmpty(); }
    bool canRedo() const { return !redo_.isEmpty(); }
    int undoCount() const { return undo_.size(); }
    int redoCount() const { return redo_.size(); }

    /* Возвращает состояние для восстановления; current уходит в redo.
     * Если откатывать нечего — возвращает isNull() строку. */
    QString undo(const QString &currentState)
    {
        if (undo_.isEmpty()) return QString();
        redo_.append(currentState);
        return undo_.takeLast();
    }

    QString redo(const QString &currentState)
    {
        if (redo_.isEmpty()) return QString();
        undo_.append(currentState);
        return redo_.takeLast();
    }

private:
    QStringList undo_;
    QStringList redo_;
    int limit_;
};

#endif // UNDOCOMMANDS_H
