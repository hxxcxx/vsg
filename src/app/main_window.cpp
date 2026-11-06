#include "app/main_window.h"

#include "app/output_panel.h"
#include "app/param_panel.h"
#include "framework/example_registry.h"
#include "host/log_bridge.h"
#include "host/vsg_host.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <vsg/io/Logger.h>

#include <map>

using vsglab::framework::ParamBase;
using vsglab::framework::ParamSet;
using vsglab::host::LogBridge;
using vsglab::host::VsgHost;

namespace {

constexpr int kIndexRole = Qt::UserRole + 1;

QString qstr(const std::string& s) {
    return QString::fromStdString(s);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("vsglab"));
    resize(1600, 960);

    // 日志桥最先安装，后续 vsg 输出都进入输出区
    log_bridge_ = new LogBridge(this);

    for (auto& example : vsglab::framework::createExamples()) {
        Entry entry;
        entry.info = example->info();
        entry.example = std::move(example);
        entries_.push_back(std::move(entry));
    }

    window_params_.params().setListener([this](ParamBase& param) { host_->handleParamChanged(param); });

    buildUi();
    populateTree();

    if (!entries_.empty())
        tree_->setCurrentItem(tree_->topLevelItem(0)->child(0));
}

MainWindow::~MainWindow() {
    // 成员（示例对象）先于子控件（宿主）析构，因此先让宿主停止并解除对示例的引用
    if (host_)
        host_->run(nullptr);
}

void MainWindow::closeEvent(QCloseEvent* e) {
    host_->stop();
    QMainWindow::closeEvent(e);
}

void MainWindow::buildUi() {
    // --- 底：输出（最先创建，确保后续各部件的日志都能显示）---
    output_ = new OutputPanel(this);
    connect(log_bridge_, &LogBridge::message, output_, &OutputPanel::appendMessage, Qt::QueuedConnection);

    // --- 左：示例树 ---
    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setMinimumWidth(180);
    connect(tree_, &QTreeWidget::currentItemChanged, this, &MainWindow::onCurrentItemChanged);

    // --- 中：VSG 宿主 ---
    host_ = new VsgHost(window_params_, this);
    host_->setMinimumSize(320, 240);
    connect(host_, &VsgHost::started, this, &MainWindow::onExampleStarted);
    connect(host_, &VsgHost::stopped, this, &MainWindow::onExampleStopped);
    connect(host_, &VsgHost::errorOccurred, this, &MainWindow::onExampleFailed);

    // --- 右：示例信息 + 参数 ---
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

    auto* sourceButton = new QPushButton(tr("Open Original Source"), right);
    connect(sourceButton, &QPushButton::clicked, this, &MainWindow::onOpenOriginalSource);

    param_panel_ = new ParamPanel(right);
    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(param_panel_);

    rightLayout->addWidget(title_label_);
    rightLayout->addWidget(brief_label_);
    rightLayout->addWidget(sourceButton, 0, Qt::AlignLeft);
    rightLayout->addWidget(scroll, 1);

    // --- 三栏 ---
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(tree_);
    splitter->addWidget(host_);
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
    toolbar->addAction(tr("Restart Example"), this, [this]() { host_->restart(); });
    toolbar->addAction(tr("Stop"), this, [this]() { host_->stop(); });
    toolbar->addSeparator();
    toolbar->addAction(tr("Clear Output"), this, [this]() { output_->clear(); });

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::populateTree() {
    std::map<std::string, QTreeWidgetItem*> categories;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        const auto& info = entries_[i].info;
        auto& categoryItem = categories[info.category];
        if (!categoryItem) {
            categoryItem = new QTreeWidgetItem(tree_, { qstr(info.category) });
            categoryItem->setFlags(Qt::ItemIsEnabled);
        }
        auto* item = new QTreeWidgetItem(categoryItem, { qstr(info.id) });
        item->setData(0, kIndexRole, i);
        item->setToolTip(0, qstr(info.brief));
    }
    tree_->expandAll();
}

void MainWindow::selectExample(int index) {
    if (index == current_ || index < 0 || index >= static_cast<int>(entries_.size()))
        return;
    current_ = index;

    auto& entry = entries_[index];
    if (!entry.params) {
        entry.params = std::make_unique<ParamSet>("Example");
        entry.example->declareParams(*entry.params);
        entry.params->setListener([this](ParamBase& param) { host_->handleParamChanged(param); });
    }

    title_label_->setText(qstr(entry.info.id));
    brief_label_->setText(qstr(entry.info.brief));
    rebuildParamPanel();

    vsg::info("[App] Running example ", entry.info.category, "/", entry.info.id);
    host_->run(entry.example.get());
}

void MainWindow::rebuildParamPanel() {
    param_panel_->clear();
    if (current_ >= 0) {
        auto& entry = entries_[current_];
        if (entry.params)
            param_panel_->addGroup(*entry.params, true);
    }
    param_panel_->addGroup(window_params_.params(), false);
    param_panel_->finish();
}

void MainWindow::onCurrentItemChanged(QTreeWidgetItem* item) {
    if (!item)
        return;
    const QVariant v = item->data(0, kIndexRole);
    if (v.isValid())
        selectExample(v.toInt());
}

void MainWindow::onExampleStarted(const QString& id) {
    statusBar()->showMessage(tr("Running: %1").arg(id));
}

void MainWindow::onExampleStopped() {
    statusBar()->showMessage(tr("Stopped"));
}

void MainWindow::onExampleFailed(const QString& message) {
    output_->appendMessage(LogBridge::Level::Error, tr("Example failed: %1").arg(message));
    statusBar()->showMessage(tr("Error: %1").arg(message));
}

void MainWindow::onOpenOriginalSource() {
    if (current_ < 0)
        return;
    const auto& info = entries_[current_].info;
    const QString dir = QStringLiteral(VSGLAB_EXAMPLES_SRC_DIR "/%1/%2").arg(qstr(info.category), qstr(info.id));
    const QString file = dir + QStringLiteral("/%1.cpp").arg(qstr(info.id));

    // 少数示例的主文件不与目录同名，退回打开目录
    const QString target = QFileInfo::exists(file) ? file : dir;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(target)))
        output_->appendMessage(LogBridge::Level::Warn, tr("Cannot open: %1").arg(target));
}
