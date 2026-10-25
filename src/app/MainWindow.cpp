#include "app/MainWindow.h"

#include "render/SceneBuilder.h"
#include "render/VsgViewport.h"

#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>

namespace facet {

MainWindow::MainWindow(const MeshData& mesh, QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(tr("Facet — Qt + OCCT + VulkanSceneGraph"));
    resize(1280, 800);

    auto* viewport = new render::VsgViewport(this);
    _viewport = viewport;
    viewport->setScene(render::buildMeshNode(mesh));
    setCentralWidget(viewport);

    auto* fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(tr("退出(&X)"), this, &QWidget::close, QKeySequence::Quit);

    auto* viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    viewMenu->addAction(tr("重置视角(&R)"), viewport, &render::VsgViewport::resetCamera,
                        QKeySequence(Qt::Key_F5));

    auto* info = new QLabel(this);
    info->setText(tr("顶点 %1    三角形 %2    1:%3 mm")
                      .arg(mesh.positions.size())
                      .arg(mesh.triangleCount())
                      .arg(mesh.sourceScaleMm, 0, 'f', 2));
    statusBar()->addPermanentWidget(info);
}

} // namespace facet
