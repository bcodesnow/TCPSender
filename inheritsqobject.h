#ifndef INHERITSQOBJECT_H
#define INHERITSQOBJECT_H

#include <QObject>

class InheritsQObject : public QObject
{
    Q_OBJECT
public:
    explicit InheritsQObject(QObject *parent = nullptr);

signals:

public slots:
};

#endif // INHERITSQOBJECT_H