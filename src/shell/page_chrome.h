#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <Qt>

class QFrame;
class QLabel;
class QPushButton;
class QSplitter;
class QTableWidget;
class QWidget;

namespace tw {
namespace ui {

// 工具页面共用 UI 脚手架（对齐电力自动化工具高密度浅色工作台规范）：
// 提供带五圆点抓手的纵横分割条、彩色小图标操作按钮、标准数据表格初始化与面板组件。
QLabel *makeLabel(const QString &text, const char *role, QWidget *parent = nullptr);
void setRole(QWidget *widget, const char *role);
QFrame *makeCard(QWidget *parent = nullptr);
QFrame *makeToolbar(QWidget *parent = nullptr);
QPushButton *makeActionButton(const QString &text, const QString &iconKey, QWidget *parent = nullptr);
QSplitter *makeSplitter(Qt::Orientation orientation, QWidget *parent = nullptr);
void configureDataTable(QTableWidget *table, const QStringList &headers);
QFrame *makeTableRow(const QStringList &columns, const QList<int> &widths,
                     bool header, bool alternate = false);

} // namespace ui
} // namespace tw
