/**
 * @file output_panel.h
 * @brief 底部输出区：显示 vsg 日志与示例输出，按级别着色
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include "host/log_bridge.h"

#include <QPlainTextEdit>

class OutputPanel : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit OutputPanel(QWidget* parent = nullptr);

    void appendMessage(vsglab::host::LogBridge::Level level, const QString& text);
};
