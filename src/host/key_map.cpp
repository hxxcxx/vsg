#include "host/key_map.h"

#include <QKeyEvent>

#include <unordered_map>

namespace vsglab::host {
namespace {

vsg::KeyModifier toVsgModifier(Qt::KeyboardModifiers mods) {
    uint16_t m = vsg::MODKEY_OFF;
    if (mods & Qt::ShiftModifier)
        m |= vsg::MODKEY_Shift;
    if (mods & Qt::ControlModifier)
        m |= vsg::MODKEY_Control;
    if (mods & Qt::AltModifier)
        m |= vsg::MODKEY_Alt;
    if (mods & Qt::MetaModifier)
        m |= vsg::MODKEY_Meta;
    return static_cast<vsg::KeyModifier>(m);
}

/// 非字符键
const std::unordered_map<int, vsg::KeySymbol>& specialKeys() {
    static const std::unordered_map<int, vsg::KeySymbol> kMap = {
        { Qt::Key_Escape, vsg::KEY_Escape },
        { Qt::Key_Tab, vsg::KEY_Tab },
        { Qt::Key_Backtab, vsg::KEY_Tab },
        { Qt::Key_Backspace, vsg::KEY_BackSpace },
        { Qt::Key_Return, vsg::KEY_Return },
        { Qt::Key_Enter, vsg::KEY_KP_Enter },
        { Qt::Key_Insert, vsg::KEY_Insert },
        { Qt::Key_Delete, vsg::KEY_Delete },
        { Qt::Key_Pause, vsg::KEY_Pause },
        { Qt::Key_Print, vsg::KEY_Print },
        { Qt::Key_SysReq, vsg::KEY_Sys_Req },
        { Qt::Key_Clear, vsg::KEY_Clear },
        { Qt::Key_Home, vsg::KEY_Home },
        { Qt::Key_End, vsg::KEY_End },
        { Qt::Key_Left, vsg::KEY_Left },
        { Qt::Key_Up, vsg::KEY_Up },
        { Qt::Key_Right, vsg::KEY_Right },
        { Qt::Key_Down, vsg::KEY_Down },
        { Qt::Key_PageUp, vsg::KEY_Page_Up },
        { Qt::Key_PageDown, vsg::KEY_Page_Down },
        { Qt::Key_Shift, vsg::KEY_Shift_L },
        { Qt::Key_Control, vsg::KEY_Control_L },
        { Qt::Key_Meta, vsg::KEY_Meta_L },
        { Qt::Key_Alt, vsg::KEY_Alt_L },
        { Qt::Key_AltGr, vsg::KEY_Alt_R },
        { Qt::Key_CapsLock, vsg::KEY_Caps_Lock },
        { Qt::Key_NumLock, vsg::KEY_Num_Lock },
        { Qt::Key_ScrollLock, vsg::KEY_Scroll_Lock },
        { Qt::Key_Super_L, vsg::KEY_Super_L },
        { Qt::Key_Super_R, vsg::KEY_Super_R },
        { Qt::Key_Hyper_L, vsg::KEY_Hyper_L },
        { Qt::Key_Hyper_R, vsg::KEY_Hyper_R },
        { Qt::Key_Menu, vsg::KEY_Menu },
        { Qt::Key_Help, vsg::KEY_Help },
        { Qt::Key_Undo, vsg::KEY_Undo },
        { Qt::Key_Redo, vsg::KEY_Redo },
        { Qt::Key_Find, vsg::KEY_Find },
        { Qt::Key_Cancel, vsg::KEY_Cancel },
        { Qt::Key_Execute, vsg::KEY_Execute },
        { Qt::Key_Select, vsg::KEY_Select },
        { Qt::Key_Mode_switch, vsg::KEY_Mode_switch },
    };
    return kMap;
}

/// 小键盘（带 KeypadModifier 时）
vsg::KeySymbol keypadKey(int key) {
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return static_cast<vsg::KeySymbol>(vsg::KEY_KP_0 + (key - Qt::Key_0));
    switch (key) {
    case Qt::Key_Asterisk: return vsg::KEY_KP_Multiply;
    case Qt::Key_Plus: return vsg::KEY_KP_Add;
    case Qt::Key_Minus: return vsg::KEY_KP_Subtract;
    case Qt::Key_Period: return vsg::KEY_KP_Decimal;
    case Qt::Key_Slash: return vsg::KEY_KP_Divide;
    case Qt::Key_Equal: return vsg::KEY_KP_Equal;
    case Qt::Key_Enter: return vsg::KEY_KP_Enter;
    case Qt::Key_Home: return vsg::KEY_KP_Home;
    case Qt::Key_End: return vsg::KEY_KP_End;
    case Qt::Key_Left: return vsg::KEY_KP_Left;
    case Qt::Key_Up: return vsg::KEY_KP_Up;
    case Qt::Key_Right: return vsg::KEY_KP_Right;
    case Qt::Key_Down: return vsg::KEY_KP_Down;
    case Qt::Key_PageUp: return vsg::KEY_KP_Page_Up;
    case Qt::Key_PageDown: return vsg::KEY_KP_Page_Down;
    case Qt::Key_Insert: return vsg::KEY_KP_Insert;
    case Qt::Key_Delete: return vsg::KEY_KP_Delete;
    default: return vsg::KEY_Undefined;
    }
}

}  // namespace

VsgKey toVsgKey(const QKeyEvent& e) {
    VsgKey result;
    result.modifier = toVsgModifier(e.modifiers());

    const int key = e.key();

    if (e.modifiers() & Qt::KeypadModifier) {
        if (auto kp = keypadKey(key); kp != vsg::KEY_Undefined) {
            result.base = result.modified = kp;
            return result;
        }
    }

    if (key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        result.base = result.modified = static_cast<vsg::KeySymbol>(vsg::KEY_F1 + (key - Qt::Key_F1));
        return result;
    }

    if (auto itr = specialKeys().find(key); itr != specialKeys().end()) {
        result.base = result.modified = itr->second;
        return result;
    }

    // 可打印 ASCII：Qt 的键码与 ASCII 一致，字母键码为大写
    if (key >= 0x20 && key <= 0x7E) {
        int base = key;
        if (key >= Qt::Key_A && key <= Qt::Key_Z)
            base = 'a' + (key - Qt::Key_A);
        result.base = static_cast<vsg::KeySymbol>(base);

        // 叠加修饰键后的实际字符取自 text()（Ctrl 组合时 text 为控制字符，回退到 base）
        const QString text = e.text();
        const char16_t ch = text.isEmpty() ? 0 : text.at(0).unicode();
        result.modified = (ch >= 0x20 && ch <= 0x7E) ? static_cast<vsg::KeySymbol>(ch) : result.base;
        return result;
    }

    return result;
}

}  // namespace vsglab::host
