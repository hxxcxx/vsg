#include "host/native_surface.h"

#include "host/key_map.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <vsg/ui/KeyEvent.h>
#include <vsg/ui/PointerEvent.h>
#include <vsg/ui/ScrollWheelEvent.h>
#include <vsg/ui/WindowEvent.h>

#include <algorithm>

namespace vsglab::host {
namespace {

vsg::ButtonMask toButtonMask(Qt::MouseButtons buttons) {
    uint16_t mask = 0;
    if (buttons & Qt::LeftButton)
        mask |= vsg::BUTTON_MASK_1;
    if (buttons & Qt::MiddleButton)
        mask |= vsg::BUTTON_MASK_2;
    if (buttons & Qt::RightButton)
        mask |= vsg::BUTTON_MASK_3;
    return static_cast<vsg::ButtonMask>(mask);
}

uint32_t toVsgButton(Qt::MouseButton button) {
    switch (button) {
    case Qt::LeftButton: return 1;
    case Qt::MiddleButton: return 2;
    case Qt::RightButton: return 3;
    default: return 0;
    }
}

}  // namespace

NativeSurface::NativeSurface(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow);  // 原生句柄直接交给 Vulkan
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);  // 无按键移动也上报（拾取类示例需要）
    setMinimumSize(64, 64);
}

QSize NativeSurface::physicalSize() const {
    const qreal dpr = devicePixelRatioF();
    return { std::max(1, qRound(width() * dpr)), std::max(1, qRound(height() * dpr)) };
}

void NativeSurface::setTargetWindow(vsg::Window* window) {
    window_ = window;
    events_.clear();
}

std::vector<vsg::ref_ptr<vsg::UIEvent>> NativeSurface::takeEvents() {
    std::vector<vsg::ref_ptr<vsg::UIEvent>> events;
    events.swap(events_);
    return events;
}

void NativeSurface::push(vsg::ref_ptr<vsg::UIEvent> event) {
    if (window_)
        events_.push_back(std::move(event));
}

void NativeSurface::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    emit resized();
}

void NativeSurface::pushButton(QMouseEvent* e, bool press) {
    if (!window_)
        return;
    const uint32_t button = toVsgButton(e->button());
    if (button == 0)
        return;

    const qreal dpr = devicePixelRatioF();
    const auto p = e->position();
    const auto x = static_cast<int32_t>(p.x() * dpr);
    const auto y = static_cast<int32_t>(p.y() * dpr);
    const auto mask = toButtonMask(e->buttons());

    if (press)
        push(vsg::ButtonPressEvent::create(window_, vsg::clock::now(), x, y, mask, button));
    else
        push(vsg::ButtonReleaseEvent::create(window_, vsg::clock::now(), x, y, mask, button));
}

void NativeSurface::mousePressEvent(QMouseEvent* e) {
    setFocus(Qt::MouseFocusReason);
    pushButton(e, true);
}

void NativeSurface::mouseDoubleClickEvent(QMouseEvent* e) {
    // Qt 以双击事件代替第二次按下
    pushButton(e, true);
}

void NativeSurface::mouseReleaseEvent(QMouseEvent* e) {
    pushButton(e, false);
}

void NativeSurface::mouseMoveEvent(QMouseEvent* e) {
    if (!window_)
        return;
    const qreal dpr = devicePixelRatioF();
    const auto p = e->position();
    push(vsg::MoveEvent::create(window_, vsg::clock::now(), static_cast<int32_t>(p.x() * dpr),
                                static_cast<int32_t>(p.y() * dpr), toButtonMask(e->buttons())));
}

void NativeSurface::wheelEvent(QWheelEvent* e) {
    if (!window_)
        return;
    const auto delta = e->angleDelta();
    push(vsg::ScrollWheelEvent::create(window_, vsg::clock::now(),
                                       vsg::vec3(delta.x() / 120.f, delta.y() / 120.f, 0.f)));
}

void NativeSurface::keyPressEvent(QKeyEvent* e) {
    if (!window_)
        return;
    const auto key = toVsgKey(*e);
    if (key.base == vsg::KEY_Undefined)
        return;
    push(vsg::KeyPressEvent::create(window_, vsg::clock::now(), key.base, key.modified, key.modifier,
                                    e->isAutoRepeat() ? 1u : 0u));
}

void NativeSurface::keyReleaseEvent(QKeyEvent* e) {
    // 自动重复时 Qt 会成对发送释放/按下，忽略中间的释放
    if (!window_ || e->isAutoRepeat())
        return;
    const auto key = toVsgKey(*e);
    if (key.base == vsg::KEY_Undefined)
        return;
    push(vsg::KeyReleaseEvent::create(window_, vsg::clock::now(), key.base, key.modified, key.modifier));
}

void NativeSurface::focusInEvent(QFocusEvent* e) {
    QWidget::focusInEvent(e);
    if (window_)
        push(vsg::FocusInEvent::create(window_, vsg::clock::now()));
}

void NativeSurface::focusOutEvent(QFocusEvent* e) {
    QWidget::focusOutEvent(e);
    if (window_)
        push(vsg::FocusOutEvent::create(window_, vsg::clock::now()));
}

}  // namespace vsglab::host
