#include "mainwindow.h"
#include <QApplication>

#include <QDebug>

#define PACKETS_PER_TRANSFER 2

int main(int argc, char *argv[])
{
    qDebug() << "Hello World!";

    QApplication a(argc, argv);
    MainWindow w;

    w.show();
    return a.exec();
}
