#include "app/MainWindow.h"
#include "kernel/Kernel.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("facet");

    // kernel：建模 + 网格化 + 归一化（OCCT 不出 kernel 边界）
    const facet::MeshData mesh = facet::createDemoPlate(120.0, 80.0, 12.0, 15.0, 4.0);
    if (mesh.positions.empty())
    {
        QMessageBox::critical(nullptr, QObject::tr("Facet"),
                              QObject::tr("零件网格化失败：没有生成任何三角形。"));
        return 1;
    }

    facet::MainWindow window(mesh);
    window.show();
    return QApplication::exec();
}
