#include "host/example_runner.h"

#include <QDir>
#include <QTimer>

#include <vsg/io/FileSystem.h>
#include <vsg/io/Logger.h>

#ifdef vsgXchange_FOUND
#include <vsgXchange/all.h>
#endif

namespace vsglab::host {
namespace {

/// 示例读取文件用的选项：vsgXchange 读写器 + vsgExamples/data + VSG_FILE_PATH
vsg::ref_ptr<vsg::Options> createOptions() {
    static const bool kDataDirChecked = [] {
        if (!QDir(QStringLiteral(VSGLAB_DATA_DIR)).exists())
            vsg::warn("[Host] Example data directory not found: ", VSGLAB_DATA_DIR,
                      " (is vsgExamples placed in the project directory?)");
        return true;
    }();
    (void) kDataDirChecked;

    auto options = vsg::Options::create();
#ifdef vsgXchange_FOUND
    // 挂上 vsgXchange 的全部读写器：模型/图片/字体等外部格式即可加载
    options->readerWriters.push_back(vsgXchange::all::create());
#endif
    options->paths.push_back(vsg::Path(VSGLAB_DATA_DIR));
    for (auto& path : vsg::getEnvPaths("VSG_FILE_PATH"))
        options->paths.push_back(path);
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    return options;
}

}  // namespace

ExampleRunner::ExampleRunner(std::string logTag, QWidget* parent)
    : QWidget(parent), log_tag_(std::move(logTag)), options_(createOptions()) {
}

void ExampleRunner::restart() {
    stop();
    start();
}

void ExampleRunner::handleParamChanged(framework::ParamBase& param) {
    if (param.apply() == framework::Apply::Restart) {
        if (!restart_queued_) {
            restart_queued_ = true;
            QTimer::singleShot(0, this, [this]() {
                restart_queued_ = false;
                restart();
            });
        }
        return;
    }
    onLiveParamChanged(param);
}

}  // namespace vsglab::host
