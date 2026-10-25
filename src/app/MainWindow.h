#pragma once

/// app 层：Qt 外壳。不直接 include OCCT/VSG 业务头。

#include "core/MeshData.h"

#include <QMainWindow>

namespace facet::render {
class VsgViewport;
}

namespace facet {

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const MeshData& mesh, QWidget* parent = nullptr);

private:
    render::VsgViewport* _viewport = nullptr;
};

} // namespace facet
