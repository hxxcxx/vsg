/**
 * @file native_surface.h
 * @brief 交给 VSG 渲染的原生子窗口，并把 Qt 输入事件转换成 vsg::UIEvent
 *
 * 本控件的原生句柄经 WindowTraits::nativeWindow 交给 VSG（见 platform/native_window_handle.h），
 * VkInstance/设备/交换链全部由 VSG 自建；Qt 只负责窗口与输入。
 * 输入事件缓存在本控件中，由宿主在每帧 advanceToNextFrame() 之后注入 viewer。
 *
 * 注意：vsg::Win32_Window 析构时会 DestroyWindow 外部传入的句柄，
 * 因此每运行一次示例就新建一个 NativeSurface，示例停止后整体丢弃。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include <QWidget>

#include <vsg/app/Window.h>
#include <vsg/ui/UIEvent.h>

#include <vector>

namespace vsglab::host {

class NativeSurface : public QWidget {
    Q_OBJECT
public:
    explicit NativeSurface(QWidget* parent = nullptr);

    /// 物理像素尺寸（已乘 devicePixelRatio）
    QSize physicalSize() const;

    /// 事件所属的 vsg 窗口；为空时丢弃输入
    void setTargetWindow(vsg::Window* window);

    /// 取走自上次调用以来累积的事件
    std::vector<vsg::ref_ptr<vsg::UIEvent>> takeEvents();

    QPaintEngine* paintEngine() const override { return nullptr; }

signals:
    void resized();

protected:
    void paintEvent(QPaintEvent*) override {}
    void resizeEvent(QResizeEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    bool focusNextPrevChild(bool) override { return false; }  // Tab 交给 vsg

private:
    void push(vsg::ref_ptr<vsg::UIEvent> event);
    void pushButton(QMouseEvent* e, bool press);

    vsg::Window* window_ = nullptr;  // 宿主保证在窗口销毁前置空
    std::vector<vsg::ref_ptr<vsg::UIEvent>> events_;
};

}  // namespace vsglab::host
