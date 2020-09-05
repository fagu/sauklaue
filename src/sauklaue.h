#ifndef SAUKLAUE_H
#define SAUKLAUE_H

#include <QMainWindow>
#include <QScopedPointer>

namespace Ui {
class sauklaue;
}

class sauklaue : public QMainWindow
{
    Q_OBJECT

public:
    explicit sauklaue(QWidget *parent = nullptr);
    ~sauklaue() override;

private:
    QScopedPointer<Ui::sauklaue> m_ui;
};

#endif // SAUKLAUE_H
