#pragma once

#include "core/platform_services.h"
#include "tools/compare/compare_engine.h"

#include <QPlainTextEdit>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
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
// 左右并排可编辑视图（行号栏、行级/字符级高亮、同步滚动）+ 中央差异缩略条 +
// 多编码自动探测/切换（UTF-8 / GB18030 / UTF-16）+ 文内查找 + F7/F8/Ctrl+S 快捷键 + 历史与拖放。
class TextComparePage : public QWidget
{
    Q_OBJECT

public:
    explicit TextComparePage(const ToolServices &services, QWidget *parent = nullptr);

    bool openFiles(const QString &leftPath, const QString &rightPath);
    void setTexts(const QString &leftText, const QString &rightText,
                  const QString &leftLabel = QString(), const QString &rightLabel = QString());
    void loadSample();

    void setEncoding(cmp::TextEncoding encoding);
    cmp::TextEncoding selectedEncoding() const;
    cmp::TextEncoding detectedLeftEncoding() const { return m_leftDetectedEnc; }
    cmp::TextEncoding detectedRightEncoding() const { return m_rightDetectedEnc; }

    void recompare();
    void jumpToHunk(int hunkIndex);
    void prevHunk();
    void nextHunk();
    bool copyCurrentHunkToRight();
    bool copyCurrentHunkToLeft();
    bool findInEditors(const QString &keyword);
    bool saveLeft(QString *error = nullptr);
    bool saveRight(QString *error = nullptr);

    const cmp::TextDiffResult &diffResult() const { return m_diff; }
    int currentHunkIndex() const { return m_currentHunk; }
    QString leftText() const;
    QString rightText() const;
    QString leftMd5() const { return m_leftMd5; }
    QString rightMd5() const { return m_rightMd5; }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void installShortcuts();
    void browseLeft();
    void browseRight();
    void swapSides();
    void showHistoryMenu();
    void exportReport();
    void applyHighlights();
    void rebuildDiffOnlyTable();
    void updateHunkCounter();
    void updateDirtyLabels();
    void updateSummaryBar();

    ToolServices m_services;
    cmp::TextDiffResult m_diff;
    QString m_leftMd5;
    QString m_rightMd5;
    int m_currentHunk = -1;
    bool m_leftDirty = false;
    bool m_rightDirty = false;
    bool m_suppressEditSignal = false;
    bool m_syncingScroll = false;
    cmp::TextEncoding m_leftDetectedEnc = cmp::TextEncoding::Utf8;
    cmp::TextEncoding m_rightDetectedEnc = cmp::TextEncoding::Utf8;

    QLineEdit *m_leftPath = nullptr;
    QLineEdit *m_rightPath = nullptr;
    QLabel *m_leftTitle = nullptr;
    QLabel *m_rightTitle = nullptr;
    QPushButton *m_saveLeftBtn = nullptr;
    QPushButton *m_saveRightBtn = nullptr;
    QPushButton *m_historyBtn = nullptr;
    QComboBox *m_encodingCombo = nullptr;

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
    QLineEdit *m_findEdit = nullptr;

    QLabel *m_statTotal = nullptr;
    QLabel *m_statSame = nullptr;
    QLabel *m_statChanged = nullptr;
    QLabel *m_statLeft = nullptr;
    QLabel *m_statRight = nullptr;
    QLabel *m_statMd5 = nullptr;

    QStackedWidget *m_viewStack = nullptr;
    DiffCodeEditor *m_leftEditor = nullptr;
    DiffCodeEditor *m_rightEditor = nullptr;
    DiffOverviewBar *m_overviewBar = nullptr;
    QTableWidget *m_diffTable = nullptr;
    QTimer *m_recompareTimer = nullptr;
};

} // namespace tw
