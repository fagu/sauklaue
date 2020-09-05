#include "sauklaue.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    sauklaue w;
    w.show();

    return app.exec();
}

