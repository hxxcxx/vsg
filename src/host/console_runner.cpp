#include "host/console_runner.h"

#include "host/output_panel.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QThreadPool>
#include <QVBoxLayout>

#include <vsg/core/Exception.h>

#include <condition_variable>
#include <mutex>

namespace vsglab::host {
namespace {

/// 控制台示例共用的后台线程池：只有一个线程，示例排队依次执行
QThreadPool& consolePool() {
    static QThreadPool* pool = [] {
        auto* p = new QThreadPool(QCoreApplication::instance());
        p->setMaxThreadCount(1);
        return p;
    }();
    return *pool;
}

}  // namespace

/// 一次运行的状态，由后台线程与界面线程共享
struct ConsoleRunner::Run {
    enum class State {
        Queued,
        Running,
        Done,
        Cancelled,
    };

    std::mutex mutex;
    std::condition_variable done;
    State state = State::Queued;
};

ConsoleRunner::ConsoleRunner(framework::ConsoleExample& example, std::string logTag, LogBridge* logBridge,
                             QWidget* parent)
    : ExampleRunner(std::move(logTag), parent), example_(example), tag_(QString::fromStdString(this->logTag())) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    output_ = new OutputPanel(this);
    layout->addWidget(output_);

    // 日志从任意线程发出，排队转到界面线程；只显示标注为本实例的输出
    connect(logBridge, &LogBridge::message, this, &ConsoleRunner::onLogMessage, Qt::QueuedConnection);
}

ConsoleRunner::~ConsoleRunner() {
    waitForRun();
}

void ConsoleRunner::start() {
    if (run_)
        return;

    auto run = std::make_shared<Run>();
    run_ = run;
    ctx_ = std::make_unique<framework::ConsoleContext>(options());
    emit started();

    // 后台线程只在确认开始运行（Queued → Running）后才访问示例与上下文；
    // 运行开始后 stop()/析构会等待它结束，因此这些引用在使用期间有效
    framework::ConsoleExample* example = &example_;
    framework::ConsoleContext* ctx = ctx_.get();
    consolePool().start([this, run, example, ctx, tag = logTag()]() {
        {
            std::scoped_lock lock(run->mutex);
            if (run->state == Run::State::Cancelled)
                return;
            run->state = Run::State::Running;
        }

        bool failed = false;
        QString error;
        try {
            LogTagScope tagScope(tag);
            example->run(*ctx);
        } catch (const vsg::Exception& ex) {
            failed = true;
            error = QString::fromStdString(ex.message);
        } catch (const std::exception& ex) {
            failed = true;
            error = QString::fromUtf8(ex.what());
        } catch (...) {
            failed = true;
            error = tr("Unknown exception");
        }

        // 先投递完成通知、再标记 Done：Done 之前运行器不会被销毁，保证投递时 this 有效；
        // 运行器若在通知处理前销毁，Qt 会丢弃发给它的待处理调用
        QMetaObject::invokeMethod(
                this, [this, run, failed, error]() { onRunFinished(run, failed, error); }, Qt::QueuedConnection);
        {
            std::scoped_lock lock(run->mutex);
            run->state = Run::State::Done;
        }
        run->done.notify_all();
    });
}

void ConsoleRunner::stop() {
    const bool wasRunning = run_ != nullptr;
    waitForRun();
    ctx_.reset();
    if (wasRunning)
        emit stopped();
}

void ConsoleRunner::restart() {
    output_->appendMessage(LogBridge::Level::Debug, QStringLiteral("---- restart ----"));
    ExampleRunner::restart();
}

void ConsoleRunner::onRunFinished(const std::shared_ptr<Run>& run, bool failed, const QString& error) {
    // 已被停止或重新运行的旧运行，忽略其完成通知
    if (run != run_)
        return;
    run_.reset();
    ctx_.reset();

    // 该通知与示例的日志都从后台线程排队而来，排在本次运行的全部输出之后
    if (failed) {
        output_->appendMessage(LogBridge::Level::Error, tr("Example failed: %1").arg(error));
        emit errorOccurred(error);
        return;
    }
    emit finished();
}

void ConsoleRunner::waitForRun() {
    if (!run_)
        return;

    {
        std::unique_lock lock(run_->mutex);
        if (run_->state == Run::State::Queued) {
            // 尚未开始：标记取消，轮到它时直接返回，不必等待
            run_->state = Run::State::Cancelled;
        } else if (run_->state == Run::State::Running) {
            QGuiApplication::setOverrideCursor(Qt::WaitCursor);
            run_->done.wait(lock, [this]() { return run_->state == Run::State::Done; });
            QGuiApplication::restoreOverrideCursor();
        }
    }
    run_.reset();
}

void ConsoleRunner::onLogMessage(LogBridge::Level level, const QString& text, const QString& tag) {
    if (tag == tag_)
        output_->appendMessage(level, text);
}

}  // namespace vsglab::host
