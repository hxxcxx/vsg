/**
 * @file param_panel.h
 * @brief 根据 ParamSet 声明自动生成控件，一个 ParamSet 对应一个可折叠分组
 * @author hxxcxx
 * @date 2026-11-06
 */
#pragma once

#include <QWidget>

class QVBoxLayout;

namespace vsglab::framework {
class ParamBase;
class ParamSet;
}  // namespace vsglab::framework

class ParamPanel : public QWidget {
    Q_OBJECT
public:
    explicit ParamPanel(QWidget* parent = nullptr);

    /// 清空所有分组
    void clear();

    /// 追加一个分组；控件修改直接写回 ParamSet 中的参数（由参数监听者负责后续处理）
    void addGroup(vsglab::framework::ParamSet& params, bool expanded = true);

    /// 在末尾追加弹性空间
    void finish();

private:
    QWidget* createEditor(vsglab::framework::ParamBase& param);

    QVBoxLayout* layout_ = nullptr;
};
