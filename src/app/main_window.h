/**
 * @file main_window.h
 * @brief Qt 主窗口：左侧示例清单、中间示例实例标签页、右侧当前实例的信息与参数、底部输出区
 *
 * 双击左侧示例即创建一个实例（可重复创建），每个实例占一个可关闭的标签页。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include <QMainWindow>

#include <map>
#include <string>

class ExampleTab;
class ParamPanel;
class QAction;
class QCloseEvent;
class QLabel;
class QStackedWidget;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace vsglab::host {
class LogBridge;
class OutputPanel;
}  // namespace vsglab::host

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onTreeItemActivated(QTreeWidgetItem* item);
    void onNewInstance();
    void onCurrentTabChanged(int index);
    void onTabCloseRequested(int index);
    void onRestartInstance();
    void onStopInstance();
    void onCloseInstance();

private:
    // --- UI 构建 ---
    void buildUi();
    void populateTree();

    // --- 实例管理 ---
    void createInstance(int descriptorIndex);
    void closeTab(int index);
    void closeAllTabs();
    ExampleTab* currentTab() const;
    void refreshCurrentInstanceUi();

    void closeEvent(QCloseEvent* e) override;

    std::map<std::string, int> instance_counters_;  // 示例 id → 已创建实例数，用于命名

    // --- 核心组件 ---
    vsglab::host::LogBridge* log_bridge_ = nullptr;

    // --- 界面 ---
    QTreeWidget* tree_ = nullptr;
    QStackedWidget* center_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* brief_label_ = nullptr;
    ParamPanel* param_panel_ = nullptr;
    vsglab::host::OutputPanel* output_ = nullptr;

    // --- Actions ---
    QAction* action_new_ = nullptr;
    QAction* action_restart_ = nullptr;
    QAction* action_stop_ = nullptr;
    QAction* action_close_ = nullptr;
};
