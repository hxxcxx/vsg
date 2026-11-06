/**
 * @file main_window.h
 * @brief Qt 主窗口：左侧示例树、中间 VSG 视口、右侧示例信息与参数、底部输出区
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include "framework/example.h"
#include "framework/param_set.h"
#include "framework/window_params.h"

#include <QMainWindow>

#include <memory>
#include <vector>

class OutputPanel;
class ParamPanel;
class QCloseEvent;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace vsglab::host {
class LogBridge;
class VsgHost;
}  // namespace vsglab::host

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onCurrentItemChanged(QTreeWidgetItem* item);
    void onExampleStarted(const QString& id);
    void onExampleStopped();
    void onExampleFailed(const QString& message);
    void onOpenOriginalSource();

private:
    struct Entry {
        std::unique_ptr<vsglab::framework::Example> example;
        vsglab::framework::ExampleInfo info;
        std::unique_ptr<vsglab::framework::ParamSet> params;  // 首次选中时声明，跨重启/切换保留
    };

    // --- UI 构建 ---
    void buildUi();
    void populateTree();
    void selectExample(int index);
    void rebuildParamPanel();

    void closeEvent(QCloseEvent* e) override;

    // --- 示例 ---
    vsglab::framework::WindowParams window_params_;
    std::vector<Entry> entries_;
    int current_ = -1;

    // --- 核心组件 ---
    vsglab::host::LogBridge* log_bridge_ = nullptr;
    vsglab::host::VsgHost* host_ = nullptr;

    // --- 界面 ---
    QTreeWidget* tree_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* brief_label_ = nullptr;
    ParamPanel* param_panel_ = nullptr;
    OutputPanel* output_ = nullptr;
};
