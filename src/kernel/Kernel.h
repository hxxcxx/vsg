#pragma once

/// kernel 层对外唯一入口。OCCT 类型不越过此边界，
/// 调用方（app 层）只见到 core 层的中性 MeshData。

#include "core/MeshData.h"

namespace facet {

/// demo 零件：圆角矩形板 + 中心通孔。
/// 尺寸参数均为毫米。内部完成建模、网格化与归一化。
MeshData createDemoPlate(double lengthMm, double widthMm, double thicknessMm,
                         double holeRadiusMm, double filletRadiusMm);

} // namespace facet
