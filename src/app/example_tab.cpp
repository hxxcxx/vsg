#include "app/example_tab.h"

#include "framework/example_registry.h"
#include "host/console_runner.h"
#include "host/window_runner.h"

#include <QVBoxLayout>

using vsglab::framework::ConsoleExample;
using vsglab::framework::ParamBase;
using vsglab::framework::WindowExample;
using vsglab::host::ConsoleRunner;
using vsglab::host::ExampleRunner;
using vsglab::host::LogBridge;
using vsglab::host::WindowRunner;

ExampleTab::ExampleTab(const vsglab::framework::ExampleDescriptor& descriptor, const QString& tag, LogBridge* logBridge,
                       QWidget* parent)
    : QWidget(parent), info_(descriptor.info), tag_(tag), example_(descriptor.create()) {
    example_->declareParams(params_);

    // 按示例继承的基类选择运行器（登记时已保证二者必居其一）
    if (auto* console = dynamic_cast<ConsoleExample*>(example_.get())) {
        runner_ = new ConsoleRunner(*console, tag_.toStdString(), logBridge, this);
    } else {
        window_runner_ = new WindowRunner(dynamic_cast<WindowExample&>(*example_), tag_.toStdString(), this);
        runner_ = window_runner_;
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(runner_);

    params_.setListener([this](ParamBase& param) { runner_->handleParamChanged(param); });

    connect(runner_, &ExampleRunner::started, this, &ExampleTab::started);
    connect(runner_, &ExampleRunner::stopped, this, &ExampleTab::stopped);
    connect(runner_, &ExampleRunner::finished, this, &ExampleTab::finished);
    connect(runner_, &ExampleRunner::errorOccurred, this, &ExampleTab::failed);
}

ExampleTab::~ExampleTab() {
    // 成员（示例对象、参数）先于子控件（运行器）析构，因此先停止示例
    runner_->stop();
}

vsglab::framework::WindowParams* ExampleTab::windowParams() {
    return window_runner_ ? &window_runner_->windowParams() : nullptr;
}

void ExampleTab::start() {
    runner_->start();
}

void ExampleTab::restart() {
    runner_->restart();
}

void ExampleTab::stop() {
    runner_->stop();
}
