#pragma once

#include "core/platform_services.h"
#include "tools/compare/compare_engine.h"

#include <QPlainTextEdit>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTimer;

namespace tw {

class DiffCodeEditor;
class DiffOverviewBar;

// 文本比较页（参考 Beyond Compare）：
// 左右并排可编辑视图（带行号栏、行级/字符级差异高亮与同步滚动）+ 中央差异缩略图 +
// 仅差异行表视图 + 差异块导航与双向复制 + 保存前自动 .bak 备份。
class TextComparePage : public QWidget
{
    Q_OBJECT

public:
    explicit TextComparePage(const ToolServices &services, QWidget *parent = nullptr);

    bool openFiles(const QString &leftPath, const QString &rightPath);
    void setTexts(const QString &leftText, const QString &rightText,
                  const QString &leftLabel = QString(), const QString &rightLabel = QString());
    void loadSample();

    void recompare();
    void jumpToHunk(int hunkIndex);
    void prevHunk();
    void nextHunk();
    bool copyCurrentHunkToRight();
    bool copyCurrentHunkToLeft();
    bool saveLeft(QString *error = nullptr);
    bool saveRight(QString *error = nullptr);

    const cmp::TextDiffResult &diffResult() const { return m_diff; }
    int currentHunkIndex() const { return m_currentHunk; }
    QString leftText() const;
    QString rightText() const;

private:
    void browseLeft();
    void browseRight();
    void swapSides();
    void exportReport();
    void applyHighlights();
    void rebuildDiffOnlyTable();
    void updateHunkCounter();
    void updateDirtyLabels();

    ToolServices m_services;
    cmp::TextDiffResult m_diff;
    int m_currentHunk = -1;
    bool m_leftDirty = false;
    bool m_rightDirty = false;
    bool m_suppressEditSignal = false;
    bool m_syncingScroll = false;

    QLineEdit *m_leftPath = nullptr;
    QLineEdit *m_rightPath = nullptr;
    QLabel *m_leftTitle = nullptr;
    QLabel *m_rightTitle = nullptr;
    QPushButton *m_saveLeftBtn = nullptr;
    QPushButton *m_saveRightBtn = nullptr;

    QPushButton *m_modeAllBtn = nullptr;
    QPushButton *m_modeDiffBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_copyRightBtn = nullptr;
    QPushButton *m_copyLeftBtn = nullptr;
    QLabel *m_hunkLabel = nullptr;
    QCheckBox *m_ignoreWs = nullptr;
    QCheckBox *m_ignoreCase = nullptr;
    QCheckBox *m_autoBackup = nullptr;

    QStackedWidget *m_viewStack = nullptr;
    DiffCodeEditor *m_leftEditor = nullptr;
    DiffCodeEditor *m_rightEditor = nullptr;
    DiffOverviewBar *m_overviewBar = nullptr;
    QTableWidget *m_diffTable = nullptr;
    QTimer *m_recompareTimer = nullptr;
};

} // namespace tw
