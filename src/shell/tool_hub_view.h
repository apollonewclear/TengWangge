#pragma once

#include "core/tool_info.h"

#include <QVector>
#include <QWidget>

class QGridLayout;
class QLabel;
class QLineEdit;
class QScrollArea;
class QTableWidget;

namespace tw {

// 工具中心页（面向 20~50+ 工具模块的可扩展导航首页）：
// 左侧分类统计表 + 五圆点分割条 + 右侧实时检索框与自适应多列工具卡片网格。
class ToolHubView : public QWidget
{
    Q_OBJECT

public:
    explicit ToolHubView(QWidget *parent = nullptr);

    void setTools(const QVector<ToolInfo> &tools);
    void setAvailable(int index, bool available, const QString &reason = QString());

    void selectCategory(const QString &category);
    void setSearchText(const QString &text);
    void focusSearch();

    int categoryCount() const;
    int visibleCardCount() const { return m_visibleIndices.size(); }

signals:
    void toolActivated(int index);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Entry
    {
        ToolInfo info;
        bool available = true;
        QString error;
    };

    void rebuildCategoryTable();
    void rebuildCards();
    int computeColumns() const;

    QVector<Entry> m_entries;
    QStringList m_categories;
    QVector<int> m_visibleIndices;
    QString m_selectedCategory;
    int m_lastCols = 0;

    QTableWidget *m_catTable = nullptr;
    QLabel *m_rightTitle = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_gridHost = nullptr;
    QGridLayout *m_grid = nullptr;
};

} // namespace tw
