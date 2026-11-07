#include "app/main_window.h"

#include "app/example_tab.h"
#include "app/output_panel.h"
#include "app/param_panel.h"
#include "framework/example_registry.h"
#include "host/log_bridge.h"
#include "host/vsg_host.h"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <vsg/io/Logger.h>

using vsglab::framework::exampleDescriptors;
using vsglab::host::LogBridge;
using vsglab::host::VsgHost;

namespace {

constexpr int kDescriptorIndexRole = Qt::UserRole + 1;

QString qstr(const std::string& s) {
    return QString::fromStdString(s);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("vsglab"));
    resize(1600, 960);

    // 日志桥最先安装，后续 vsg 输出都进入输出区
    log_bridge_ = new LogBridge(this);

    buildUi();
    populateTree();
    refreshCurrentInstanceUi();
}

MainWindow::~MainWindow() {
    // 实例在日志桥之前销毁，保证停止阶段的输出仍能送达界面
    closeAllTabs();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    closeAllTabs();
    QMainWindow::closeEvent(e);
}

void MainWindow::buildUi() {
    // --- 底：输出（最先创建，确保后续各部件的日志都能显示）---
    output_ = new OutputPanel(this);
    connect(
            log_bridge_, &LogBridge::message, output_,
            [this](LogBridge::Level level, const QString& text, const QString& tag) {
                output_->appendMessage(level, tag.isEmpty() ? text : QStringLiteral("[%1] %2").arg(tag, text));
            },
            Qt::QueuedConnection);

    // --- 左：示例清单 ---
    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setMinimumWidth(180);
    connect(tree_, &QTreeWidget::itemActivated, this, &MainWindow::onTreeItemActivated);

    // --- 中：示例实例标签页（无实例时显示提示）---
    tabs_ = new QTabWidget(this);
    tabs_->setTabsClosable(true);
    tabs_->setMovable(true);
    tabs_->setDocumentMode(true);
    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(tabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    auto* hint = new QLabel(tr("Double-click an example on the left to create an instance"), this);
    hint->setAlignment(Qt::AlignCenter);

    center_ = new QStackedWidget(this);
    center_->setMinimumSize(320, 240);
    center_->addWidget(hint);
    center_->addWidget(tabs_);

    // --- 右：当前实例信息 + 参数 ---
    auto* right = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(8, 8, 8, 8);

    title_label_ = new QLabel(right);
    QFont titleFont = title_label_->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
    titleFont.setBold(true);
    title_label_->setFont(titleFont);
    title_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    brief_label_ = new QLabel(right);
    brief_label_->setWordWrap(true);

    param_panel_ = new ParamPanel(right);
    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(param_panel_);

    rightLayout->addWidget(title_label_);
    rightLayout->addWidget(brief_label_);
    rightLayout->addWidget(scroll, 1);

    // --- 三栏 ---
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(tree_);
    splitter->addWidget(center_);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({ 220, 1000, 340 });
    setCentralWidget(splitter);

    // --- 底：输出停靠窗 ---
    auto* dock = new QDockWidget(tr("Output"), this);
    dock->setObjectName(QStringLiteral("OutputDock"));
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock->setWidget(output_);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
    resizeDocks({ dock }, { 200 }, Qt::Vertical);

    // --- 工具栏 ---
    auto* toolbar = addToolBar(tr("Example"));
    toolbar->setObjectName(QStringLiteral("ExampleToolBar"));
    toolbar->setMovable(false);
    action_new_ = toolbar->addAction(tr("New Instance"), this, &MainWindow::onNewInstance);
    action_restart_ = toolbar->addAction(tr("Restart"), this, &MainWindow::onRestartInstance);
    action_stop_ = toolbar->addAction(tr("Stop"), this, &MainWindow::onStopInstance);
    action_close_ = toolbar->addAction(tr("Close"), this, &MainWindow::onCloseInstance);
    toolbar->addSeparator();
    toolbar->addAction(tr("Clear Output"), output_, &OutputPanel::clear);

    action_new_->setToolTip(tr("Create an instance of the example selected on the left"));

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::populateTree() {
    const auto& descriptors = exampleDescriptors();
    std::map<std::string, QTreeWidgetItem*> categories;
    for (int i = 0; i < static_cast<int>(descriptors.size()); ++i) {
        const auto& info = descriptors[i].info;
        auto& categoryItem = categories[info.category];
        if (!categoryItem) {
            categoryItem = new QTreeWidgetItem(tree_, { qstr(info.category) });
            categoryItem->setFlags(Qt::ItemIsEnabled);
        }
        auto* item = new QTreeWidgetItem(categoryItem, { qstr(info.id) });
        item->setData(0, kDescriptorIndexRole, i);
        item->setToolTip(0, qstr(info.brief));
    }
    tree_->expandAll();
}

void MainWindow::createInstance(int descriptorIndex) {
    const auto& descriptors = exampleDescriptors();
    if (descriptorIndex < 0 || descriptorIndex >= static_cast<int>(descriptors.size()))
        return;

    const auto& descriptor = descriptors[descriptorIndex];
    const int number = ++instance_counters_[descriptor.info.id];
    const QString tag = QStringLiteral("%1 #%2").arg(qstr(descriptor.info.id)).arg(number);

    auto* tab = new ExampleTab(descriptor, tag, log_bridge_, tabs_);
    VsgHost* host = tab->host();
    connect(host, &VsgHost::started, tab, [this, tag]() { statusBar()->showMessage(tr("Running: %1").arg(tag)); });
    connect(host, &VsgHost::stopped, tab, [this, tag]() { statusBar()->showMessage(tr("Stopped: %1").arg(tag)); });
    connect(host, &VsgHost::finished, tab, [this, tag]() { statusBar()->showMessage(tr("Finished: %1").arg(tag)); });
    connect(
            host, &VsgHost::errorOccurred, tab,
            [this, tag](const QString& message) {
                output_->appendMessage(LogBridge::Level::Error,
                                       QStringLiteral("[%1] %2").arg(tag, tr("Example failed: %1").arg(message)));
                statusBar()->showMessage(tr("Error in %1: %2").arg(tag, message));
            },
            Qt::QueuedConnection);

    vsg::info("[App] Create instance ", tag.toStdString());

    // 先成为当前标签（可见）再启动：Window 示例需要可见的原生窗口
    center_->setCurrentWidget(tabs_);
    const int index = tabs_->addTab(tab, tag);
    tabs_->setTabToolTip(index, qstr(descriptor.info.brief));
    tabs_->setCurrentIndex(index);
    tab->start();
}

void MainWindow::closeTab(int index) {
    auto* tab = qobject_cast<ExampleTab*>(tabs_->widget(index));
    if (!tab)
        return;
    vsg::info("[App] Close instance ", tab->tag().toStdString());
    // 先移出标签（触发当前实例切换、参数面板重建），再销毁实例
    tabs_->removeTab(index);
    delete tab;
}

void MainWindow::closeAllTabs() {
    while (tabs_ && tabs_->count() > 0)
        closeTab(tabs_->count() - 1);
}

ExampleTab* MainWindow::currentTab() const {
    return qobject_cast<ExampleTab*>(tabs_->currentWidget());
}

void MainWindow::refreshCurrentInstanceUi() {
    ExampleTab* tab = currentTab();
    center_->setCurrentIndex(tab ? 1 : 0);

    param_panel_->clear();
    if (tab) {
        title_label_->setText(tab->tag());
        brief_label_->setText(qstr(tab->info().brief));
        param_panel_->addGroup(tab->params(), true);
        if (!tab->isConsole())
            param_panel_->addGroup(tab->windowParams().params(), false);
    } else {
        title_label_->setText(tr("No instance"));
        brief_label_->setText(tr("Double-click an example on the left to create an instance."));
    }
    param_panel_->finish();

    action_restart_->setEnabled(tab != nullptr);
    action_stop_->setEnabled(tab != nullptr);
    action_close_->setEnabled(tab != nullptr);
}

void MainWindow::onTreeItemActivated(QTreeWidgetItem* item) {
    if (!item)
        return;
    const QVariant v = item->data(0, kDescriptorIndexRole);
    if (v.isValid())
        createInstance(v.toInt());
}

void MainWindow::onNewInstance() {
    onTreeItemActivated(tree_->currentItem());
}

void MainWindow::onCurrentTabChanged(int /*index*/) {
    refreshCurrentInstanceUi();
}

void MainWindow::onTabCloseRequested(int index) {
    closeTab(index);
}

void MainWindow::onRestartInstance() {
    if (auto* tab = currentTab())
        tab->restart();
}

void MainWindow::onStopInstance() {
    if (auto* tab = currentTab())
        tab->stop();
}

void MainWindow::onCloseInstance() {
    if (tabs_->currentIndex() >= 0)
        closeTab(tabs_->currentIndex());
}
