#include "render/VsgViewport.h"

#include <QMouseEvent>
#include <QWheelEvent>

#include <vsg/app/Trackball.h>

#ifndef NOMINMAX
#    define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace facet::render {
namespace {

vsg::ButtonMask toButtonMask(Qt::MouseButtons buttons)
{
    uint32_t mask = 0;
    if (buttons & Qt::LeftButton) mask |= vsg::BUTTON_MASK_1;
    if (buttons & Qt::MiddleButton) mask |= vsg::BUTTON_MASK_2;
    if (buttons & Qt::RightButton) mask |= vsg::BUTTON_MASK_3;
    return static_cast<vsg::ButtonMask>(mask);
}

uint32_t toVsgButton(Qt::MouseButton button)
{
    switch (button)
    {
    case Qt::LeftButton: return 1;
    case Qt::MiddleButton: return 2;
    case Qt::RightButton: return 3;
    default: return 0;
    }
}

} // namespace

VsgViewport::VsgViewport(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);       // 直接拿原生 HWND 给 Vulkan
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(64, 64);

    _homeEye = {3.4, 2.6, 2.2};
    _homeCenter = {0.0, 0.0, 0.0};
    _homeUp = {0.0, 0.0, 1.0};               // CAD 习惯 Z 朝上

    _timer.setInterval(15);                  // ~60fps，vsync 下由 present 节流
    _timer.setParent(this);
    connect(&_timer, &QTimer::timeout, this, &VsgViewport::tick);
}

VsgViewport::~VsgViewport()
{
    shutdownVsg();   // 必须在基类析构（销毁 HWND）之前释放 Vulkan 资源
}

void VsgViewport::setScene(vsg::ref_ptr<vsg::Node> node)
{
    _scene = node;
}

void VsgViewport::resetCamera()
{
    if (!_camera) return;
    if (auto lookAt = dynamic_cast<vsg::LookAt*>(_camera->viewMatrix.get()))
    {
        lookAt->eye = _homeEye;
        lookAt->center = _homeCenter;
        lookAt->up = _homeUp;
    }
}

void VsgViewport::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (isVisible() && !_vsgReady && _scene) initializeVsg();
}

void VsgViewport::initializeVsg()
{
    // 认领 Qt 的原生窗口：VSG 用自己的 VkInstance 在该 HWND 上建 surface
    auto traits = vsg::WindowTraits::create();
    const double dpr = devicePixelRatioF();
    traits->width = std::max(1, int(width() * dpr));
    traits->height = std::max(1, int(height() * dpr));
    traits->nativeWindow = std::any(reinterpret_cast<HWND>(winId()));
    traits->systemConnection = std::any(static_cast<HINSTANCE>(GetModuleHandleW(nullptr)));

    _window = vsg::Window::create(traits);
    if (!_window)
    {
        qWarning("VsgViewport: VSG 窗口/表面创建失败");
        return;
    }

    const auto extent = _window->extent2D();
    _perspective = vsg::Perspective::create(30.0,
                                            double(extent.width) / double(std::max(1u, extent.height)),
                                            0.05, 100.0);
    auto lookAt = vsg::LookAt::create(_homeEye, _homeCenter, _homeUp);
    _camera = vsg::Camera::create(_perspective, lookAt, vsg::ViewportState::create(extent));

    _trackball = vsg::Trackball::create(_camera);

    _viewer = vsg::Viewer::create();
    _viewer->addWindow(_window);
    _viewer->addEventHandler(_trackball);

    auto commandGraph = vsg::createCommandGraphForView(_window, _camera, _scene);
    _viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    _viewer->compile();

    _vsgReady = true;
    _timer.start();
}

void VsgViewport::tick()
{
    if (!_vsgReady || !_viewer) return;

    // advanceToNextFrame 内部 pollEvents(true) 会清空事件列表，
    // 因此 Qt 注入的事件必须在它之后、handleEvents 之前灌入
    _viewer->advanceToNextFrame();
    auto& events = _viewer->getEvents();
    for (auto& e : _pendingEvents) events.push_back(e);
    _pendingEvents.clear();

    _viewer->handleEvents();
    _viewer->recordAndSubmit();
    _viewer->present();
}

void VsgViewport::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!_vsgReady || !_window) return;

    _window->resize();           // 内部 GetClientRect + 重建交换链

    const double dpr = devicePixelRatioF();
    const auto extent = _window->extent2D();
    if (_perspective && extent.height > 0)
        _perspective->aspectRatio = double(extent.width) / double(extent.height);
    if (_camera && extent.width > 0 && extent.height > 0)
        _camera->viewportState = vsg::ViewportState::create(extent);
}

void VsgViewport::pushEvent(const vsg::ref_ptr<vsg::UIEvent>& event)
{
    _pendingEvents.push_back(event);
}

void VsgViewport::mouseMoveEvent(QMouseEvent* event)
{
    if (!_vsgReady) return;
    const double dpr = devicePixelRatioF();
    const auto p = event->position();
    pushEvent(vsg::MoveEvent::create(_window.get(), vsg::clock::now(),
                                     int32_t(p.x() * dpr), int32_t(p.y() * dpr),
                                     toButtonMask(event->buttons())));
}

void VsgViewport::mousePressEvent(QMouseEvent* event)
{
    if (!_vsgReady) return;
    const double dpr = devicePixelRatioF();
    const auto p = event->position();
    const uint32_t button = toVsgButton(event->button());
    if (button == 0) return;
    pushEvent(vsg::ButtonPressEvent::create(_window.get(), vsg::clock::now(),
                                            int32_t(p.x() * dpr), int32_t(p.y() * dpr),
                                            toButtonMask(event->buttons()), button));
}

void VsgViewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (!_vsgReady) return;
    const double dpr = devicePixelRatioF();
    const auto p = event->position();
    const uint32_t button = toVsgButton(event->button());
    if (button == 0) return;
    pushEvent(vsg::ButtonReleaseEvent::create(_window.get(), vsg::clock::now(),
                                              int32_t(p.x() * dpr), int32_t(p.y() * dpr),
                                              toButtonMask(event->buttons()), button));
}

void VsgViewport::wheelEvent(QWheelEvent* event)
{
    if (!_vsgReady) return;
    const float steps = event->angleDelta().y() / 120.f;
    pushEvent(vsg::ScrollWheelEvent::create(_window.get(), vsg::clock::now(),
                                            vsg::vec3(0.f, steps, 0.f)));
}

void VsgViewport::shutdownVsg()
{
    if (!_vsgReady) return;
    _vsgReady = false;
    _timer.stop();
    _pendingEvents.clear();

    // 等 GPU 空闲后按依赖逆序释放，viewer（持有 device）最后
    if (_viewer) _viewer->deviceWaitIdle();

    _trackball = nullptr;
    _camera = nullptr;
    _perspective = nullptr;
    _window = nullptr;
    _viewer = nullptr;
}

} // namespace facet::render
