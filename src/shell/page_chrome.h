#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class QFrame;
class QLabel;
class QWidget;

namespace tw {
namespace ui {

// 工具页面共用 UI 脚手架（浅色工业工作台风格）：
// 统一标签 role、操作工具条、框线卡片与表格行构造，消除各业务工具的样板代码。
QLabel *makeLabel(const QString &text, const char *role, QWidget *parent = nullptr);
void setRole(QWidget *widget, const char *role);
QFrame *makeCard(QWidget *parent = nullptr);
QFrame *makeToolbar(QWidget *parent = nullptr);
QFrame *makeTableRow(const QStringList &columns, const QList<int> &widths,
                     bool header, bool alternate = false);

} // namespace ui
} // namespace tw
