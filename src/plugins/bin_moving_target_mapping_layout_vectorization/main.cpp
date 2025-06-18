#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QProcessEnvironment>

#include "moving_target_mapping_layout_vectorization.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QgsMovingTargetMappingLayoutVectorizationDialog pDlg;
    pDlg.show();
    return a.exec();
}
