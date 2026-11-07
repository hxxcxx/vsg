#include "host/vsg_host.h"

#include "framework/window_params.h"
#include "host/log_bridge.h"
#include "host/native_surface.h"
#include "host/platform/native_window_handle.h"

#include <QDir>
#include <QHideEvent>
#include <QLabel>
#include <QShowEvent>
#include <QVBoxLayout>

#include <vsg/core/Exception.h>
#include <vsg/io/FileSystem.h>
#include <vsg/io/Logger.h>

#ifdef vsgXchange_FOUND
#include <vsgXchange/all.h>
#endif

namespace vsglab::host {

VsgHost::VsgHost(const framework::WindowParams& windowParams, QWidget* parent)
    : QWidget(parent), window_params_(windowParams) {
    options_ = vsg::Options::create();
#ifdef vsgXchange_FOUND
    // 挂上 vsgXchange 的全部读写器：模型/图片/字体等外部格式即可加载
    options_->readerWriters.push_back(vsgXchange::all::create());
#endif
    options_->paths.push_back(vsg::Path(VSGLAB_DATA_DIR));
    for (auto& path : vsg::getEnvPaths("VSG_FILE_PATH"))
        options_->paths.push_back(path);
    options_->fileCache = vsg::getEnv("VSG_FILE_CACHE");

    if (!QDir(QStringLiteral(VSGLAB_DATA_DIR)).exists())
        vsg::warn("[Host] Example data directory not found: ", VSGLAB_DATA_DIR,
                  " (is vsgExamples placed in the project directory?)");

    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(32, 32, 36));
    pal.setColor(QPalette::WindowText, QColor(150, 150, 160));
    setPalette(pal);

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);

    placeholder_ = new QLabel(this);
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setWordWrap(true);
    layout_->addWidget(placeholder_);
    showPlaceholder(tr("Not started"));

    // 间隔 0：事件队列空闲即出帧；FIFO 呈现模式下由 present 节流
    timer_.setInterval(0);
    connect(&timer_, &QTimer::timeout, this, &VsgHost::onFrameTick);
}

VsgHost::~VsgHost() {
    teardown();
}

void VsgHost::run(framework::Example* example) {
    stop();
    example_ = example;
    if (!example_)
        return;
    pending_start_ = true;
    tryStartPending();
}

void VsgHost::restart() {
    if (!example_)
        return;
    stop();
    pending_start_ = true;
    tryStartPending();
}

void VsgHost::stop() {
    const bool wasRunning = running_;
    teardown();
    showPlaceholder(tr("Example stopped"));
    if (wasRunning)
        emit stopped();
}

void VsgHost::handleParamChanged(framework::ParamBase& param) {
    if (param.apply() == framework::Apply::Restart) {
        // 合并同一轮事件中的多次修改
        if (example_ && !restart_queued_) {
            restart_queued_ = true;
            QTimer::singleShot(0, this, [this]() {
                restart_queued_ = false;
                restart();
            });
        }
        return;
    }

    if (!running_ || !example_ || !ctx_)
        return;
    try {
        LogTagScope tag(log_tag_);
        example_->onParamChanged(*ctx_, param);
    } catch (const vsg::Exception& ex) {
        fail(QString::fromStdString(ex.message));
    } catch (const std::exception& ex) {
        fail(QString::fromUtf8(ex.what()));
    }
}

void VsgHost::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (running_ && !isConsole())
        timer_.start();
    tryStartPending();
}

void VsgHost::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // 标签页切走后不再出帧，避免多个实例在主线程上轮流等待垂直同步
    timer_.stop();
}

bool VsgHost::isConsole() const {
    return example_ && example_->info().kind == framework::ExampleKind::Console;
}

void VsgHost::tryStartPending() {
    if (!pending_start_)
        return;
    // Window 示例的原生窗口需在控件可见、尺寸确定后再交给 VSG
    if (!isConsole() && !isVisible())
        return;
    pending_start_ = false;
    startNow();
}

void VsgHost::startNow() {
    ctx_ = std::make_unique<framework::ExampleContext>();
    ctx_->options_ = options_;

    if (isConsole())
        startConsole();
    else
        startWindow();
}

void VsgHost::startWindow() {
    placeholder_->hide();
    surface_ = new NativeSurface(this);
    layout_->addWidget(surface_);
    surface_->show();
    layout_->activate();
    connect(surface_, &NativeSurface::resized, this, &VsgHost::onSurfaceResized);

    QPointer<NativeSurface> surface = surface_;
    const framework::WindowParams& windowParams = window_params_;
    ctx_->make_window_traits_ = [surface, &windowParams]() {
        auto traits = vsg::WindowTraits::create();
        windowParams.applyTo(*traits);
        if (surface) {
            const QSize size = surface->physicalSize();
            traits->width = static_cast<uint32_t>(size.width());
            traits->height = static_cast<uint32_t>(size.height());
            assignNativeWindowHandle(*traits, *surface);
        }
        return traits;
    };

    try {
        LogTagScope tag(log_tag_);
        example_->start(*ctx_);
        if (!ctx_->viewer)
            throw vsg::Exception{ "Example did not create a viewer (assign ctx.viewer in start())" };
        if (ctx_->viewer->windows().empty())
            throw vsg::Exception{ "Viewer has no window" };
        surface_->setTargetWindow(ctx_->viewer->windows().front().get());
    } catch (const vsg::Exception& ex) {
        fail(QString::fromStdString(ex.message));
        return;
    } catch (const std::exception& ex) {
        fail(QString::fromUtf8(ex.what()));
        return;
    } catch (...) {
        fail(tr("Unknown exception"));
        return;
    }

    running_ = true;
    if (isVisible())
        timer_.start();
    emit started();
}

void VsgHost::startConsole() {
    running_ = true;
    emit started();

    try {
        LogTagScope tag(log_tag_);
        example_->start(*ctx_);
    } catch (const vsg::Exception& ex) {
        fail(QString::fromStdString(ex.message));
        return;
    } catch (const std::exception& ex) {
        fail(QString::fromUtf8(ex.what()));
        return;
    } catch (...) {
        fail(tr("Unknown exception"));
        return;
    }

    teardown();
    showPlaceholder(tr("Example finished"));
    emit finished();
}

void VsgHost::teardown() {
    timer_.stop();
    pending_start_ = false;
    running_ = false;

    if (surface_)
        surface_->setTargetWindow(nullptr);

    if (ctx_) {
        LogTagScope tag(log_tag_);
        try {
            if (example_)
                example_->stop(*ctx_);
            if (ctx_->viewer)
                ctx_->viewer->deviceWaitIdle();
        } catch (...) {
            // 清理阶段的异常不再上报
        }
        // 释放 viewer → window：VSG 销毁交换链/surface，并 DestroyWindow 原生句柄
        ctx_.reset();
    }

    // 必须在 VSG 窗口释放之后再删除控件（Vulkan 要求 surface 先于原生窗口销毁）
    if (surface_) {
        layout_->removeWidget(surface_);
        delete surface_;
    }
}

void VsgHost::onFrameTick() {
    if (!running_ || !ctx_ || !ctx_->viewer || !surface_)
        return;
    if (!surface_->isVisible() || window()->isMinimized() || surface_->width() <= 0 || surface_->height() <= 0)
        return;

    auto& viewer = ctx_->viewer;
    try {
        LogTagScope tag(log_tag_);
        if (!viewer->active()) {
            stop();
            return;
        }

        // advanceToNextFrame 内部 pollEvents 会清空事件列表，
        // 因此界面事件必须在它之后、handleEvents 之前注入
        if (!viewer->advanceToNextFrame())
            return;

        auto events = surface_->takeEvents();
        auto& viewerEvents = viewer->getEvents();
        viewerEvents.insert(viewerEvents.end(), events.begin(), events.end());

        example_->frame(*ctx_);
    } catch (const vsg::Exception& ex) {
        fail(QString::fromStdString(ex.message));
    } catch (const std::exception& ex) {
        fail(QString::fromUtf8(ex.what()));
    } catch (...) {
        fail(tr("Unknown exception"));
    }
}

void VsgHost::onSurfaceResized() {
    if (!running_ || !ctx_ || !ctx_->viewer || !surface_)
        return;
    if (surface_->width() <= 0 || surface_->height() <= 0)
        return;

    try {
        LogTagScope tag(log_tag_);
        // 重建交换链；相机视口与投影由 RenderGraph 的 WindowResizeHandler 自动更新
        for (auto& w : ctx_->viewer->windows())
            w->resize();
    } catch (const vsg::Exception& ex) {
        fail(QString::fromStdString(ex.message));
    }
}

void VsgHost::fail(const QString& what) {
    teardown();
    emit errorOccurred(what);
    showPlaceholder(tr("Example failed:\n%1").arg(what));
}

void VsgHost::showPlaceholder(const QString& text) {
    placeholder_->setText(text);
    placeholder_->show();
}

}  // namespace vsglab::host
