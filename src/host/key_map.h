/**
 * @file key_map.h
 * @brief Qt 键盘事件 → vsg 键码
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include <vsg/ui/KeyEvent.h>

class QKeyEvent;

namespace vsglab::host {

struct VsgKey {
    vsg::KeySymbol base = vsg::KEY_Undefined;      ///< 未叠加修饰键的键值（如 'a'）
    vsg::KeySymbol modified = vsg::KEY_Undefined;  ///< 叠加 Shift 等之后的键值（如 'A'）
    vsg::KeyModifier modifier = vsg::MODKEY_OFF;
};

/// 无法映射时 base 为 KEY_Undefined
VsgKey toVsgKey(const QKeyEvent& e);

}  // namespace vsglab::host
