/**
 * @file output_panel.h
 * @brief 日志输出区：显示 LogBridge 转来的 vsg 日志与示例输出，按级别着色
 *
 * 主窗口底部的输出区与控制台示例的标签页都使用它。
 *
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include "host/log_bridge.h"

#include <QPlainTextEdit>

namespace vsglab::host {

class OutputPanel : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit OutputPanel(QWidget* parent = nullptr);

    void appendMessage(LogBridge::Level level, const QString& text);
};

}  // namespace vsglab::host
