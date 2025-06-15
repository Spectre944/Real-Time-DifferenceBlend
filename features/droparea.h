#ifndef DROPAREA_H
#define DROPAREA_H

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QMimeData>
#include <QDragEnterEvent>

class DropArea : public QLabel {
    Q_OBJECT

public:
    explicit DropArea(QWidget *parent = nullptr);

signals:

    void dropAreaFileReviced(const QList<QUrl> &list);
protected:

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};
#endif // DROPAREA_H
