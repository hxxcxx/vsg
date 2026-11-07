#include "app/example_tab.h"

#include "app/output_panel.h"
#include "framework/example_registry.h"
#include "host/vsg_host.h"

#include <QVBoxLayout>

using vsglab::framework::ParamBase;
using vsglab::host::LogBridge;
using vsglab::host::VsgHost;

ExampleTab::ExampleTab(const vsglab::framework::ExampleDescriptor& descriptor, const QString& tag, LogBridge* logBridge,
                       QWidget* parent)
    : QWidget(parent), info_(descriptor.info), tag_(tag), example_(descriptor.create()) {
    example_->declareParams(params_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    host_ = new VsgHost(window_params_, this);
    host_->setLogTag(tag_.toStdString());
    // 与日志一样走队列，保证错误信息排在该次运行的输出之后
    connect(host_, &VsgHost::errorOccurred, this, &ExampleTab::onExampleFailed, Qt::QueuedConnection);

    if (isConsole()) {
        // Console 示例没有视口，标签页内显示本实例的输出
        console_ = new OutputPanel(this);
        layout->addWidget(console_);
        host_->hide();
        connect(logBridge, &LogBridge::message, this, &ExampleTab::onLogMessage, Qt::QueuedConnection);
    } else {
        layout->addWidget(host_);
    }

    params_.setListener([this](ParamBase& param) { host_->handleParamChanged(param); });
    window_params_.params().setListener([this](ParamBase& param) { host_->handleParamChanged(param); });
}

ExampleTab::~ExampleTab() {
    // 成员（示例对象、参数）先于子控件（宿主）析构，因此先让宿主停止并解除对示例的引用
    host_->run(nullptr);
}

void ExampleTab::start() {
    host_->run(example_.get());
}

void ExampleTab::restart() {
    if (console_)
        console_->appendMessage(LogBridge::Level::Debug, QStringLiteral("---- restart ----"));
    host_->restart();
}

void ExampleTab::stop() {
    host_->stop();
}

void ExampleTab::onLogMessage(LogBridge::Level level, const QString& text, const QString& tag) {
    if (tag == tag_)
        console_->appendMessage(level, text);
}

void ExampleTab::onExampleFailed(const QString& message) {
    if (console_)
        console_->appendMessage(LogBridge::Level::Error, tr("Example failed: %1").arg(message));
}
