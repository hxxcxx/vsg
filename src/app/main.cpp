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

    // 临时调试：输出网格样本数据（定位黑色渲染问题后删除）
    if (FILE* dbg = std::fopen("facet_debug.txt", "w"))
    {
        std::fprintf(dbg, "verts=%zu tris=%u scale=%.3f\n", mesh.positions.size(),
                     mesh.triangleCount(), mesh.sourceScaleMm);
        for (size_t i = 0; i < mesh.positions.size() && i < 6; ++i)
            std::fprintf(dbg, "v[%zu]=(%.3f,%.3f,%.3f) n=(%.3f,%.3f,%.3f)\n", i,
                         mesh.positions[i].x, mesh.positions[i].y, mesh.positions[i].z,
                         mesh.normals[i].x, mesh.normals[i].y, mesh.normals[i].z);
        std::fclose(dbg);
    }

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
