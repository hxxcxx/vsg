#pragma once

/// render 层：Qt 原生窗口认领 + VSG 渲染循环 + 输入事件桥。
///
/// 集成方式（无 QVulkanInstance）：
///   QWidget(WA_NativeWindow) 的 HWND 通过 WindowTraits::nativeWindow 交给 VSG，
///   VkInstance/设备/交换链全部由 VSG 自建；Qt 只负责窗口与输入。

#include <QTimer>
#include <QWidget>

#include <vsg/all.h>

namespace facet::render {

class VsgViewport : public QWidget
{
    Q_OBJECT
public:
    explicit VsgViewport(QWidget* parent = nullptr);
    ~VsgViewport() override;

    /// 设置场景内容（render 层节点）。须在窗口显示前调用。
    void setScene(vsg::ref_ptr<vsg::Node> node);

    /// 恢复初始视角
    void resetCamera();

    bool isVsgReady() const { return _vsgReady; }

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void initializeVsg();
    void tick();
    void shutdownVsg();
    void pushEvent(const vsg::ref_ptr<vsg::UIEvent>& event);

    vsg::ref_ptr<vsg::Node> _scene;
    vsg::ref_ptr<vsg::Viewer> _viewer;
    vsg::ref_ptr<vsg::Window> _window;
    vsg::ref_ptr<vsg::Camera> _camera;
    vsg::ref_ptr<vsg::Perspective> _perspective;
    vsg::ref_ptr<vsg::Trackball> _trackball;
    vsg::dvec3 _homeEye, _homeCenter, _homeUp;

    QTimer _timer;
    std::vector<vsg::ref_ptr<vsg::UIEvent>> _pendingEvents;   // Qt→VSG 待注入事件
    bool _vsgReady = false;
};

} // namespace facet::render
