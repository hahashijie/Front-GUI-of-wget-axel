#include "downloadmanager.h"
#include "commonhelper.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //CommonHelper::setStyle("Style_Gray.qss");

    //a.setStyle();
    DownLoadManager w;
    w.show();

    return a.exec();
}
