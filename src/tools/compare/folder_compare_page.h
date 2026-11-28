#pragma once

#include "core/platform_services.h"
#include "core/task_runner.h"
#include "tools/compare/compare_engine.h"

#include <QMutex>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTreeWidget;

namespace tw {

// 文件夹比较页（参考 Beyond Compare Folder Compare）：
// 后台工作线程递归扫描（TaskRunner + 导航锁）+ 大小时间/SHA-256 双模式 +
// 树形/列表双视图 + 自定义忽略规则 + 多选批量双向同步（.bak 自动备份）+ 历史会话 + 拖放。
class FolderComparePage : public QWidget
{
    Q_OBJECT

public:
    explicit FolderComparePage(const ToolServices &services, QWidget *parent = nullptr);

    void setDirectories(const QString &leftDir, const QString &rightDir);
    void setCompareMode(cmp::FolderCompareMode mode);
    cmp::FolderCompareMode compareMode() const;
    void setIgnorePatternsText(const QString &patterns);
    void setTreeViewMode(bool treeMode);
    bool isTreeViewMode() const { return m_treeMode; }
    void loadSample();

    void startCompare();
    void compareSyncForTest();
    bool copySelectedToRight(QString *error = nullptr);
    bool copySelectedToLeft(QString *error = nullptr);
    int copyEntriesToRight(const QVector<int> &entryIndices, QString *error = nullptr);
    int copyEntriesToLeft(const QVector<int> &entryIndices, QString *error = nullptr);
    void abort();

    const cmp::FolderCompareResult &result() const { return m_result; }
    QTreeWidget *treeWidget() const { return m_tree; }
    QTableWidget *tableWidget() const { return m_table; }
    bool isRunning() const;

signals:
    void openFileCompareRequested(const QString &leftPath, const QString &rightPath, bool isIni);
    void compareFinished(bool success);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    enum class FilterMode { All, DiffOnly, LeftOnly, RightOnly };

    void browseLeft();
    void browseRight();
    void swapDirs();
    void showHistoryMenu();
    void openSelectedInCompare(bool forceIni);
    void exportReport();
    void setFilterMode(FilterMode mode);
    void rebuildViews();
    void rebuildTable();
    void rebuildTree();
    void updateSummaryBar();
    void updateRunningUi();
    QVector<int> selectedEntryIndices() const;
    int primarySelectedEntryIndex() const;

    ToolServices m_services;
    TaskRunner *m_runner = nullptr;
    mutable QMutex m_resultMutex;
    cmp::FolderCompareResult m_pendingResult;
    cmp::FolderCompareResult m_result;
    FilterMode m_filter = FilterMode::All;
    bool m_treeMode = false;

    QLineEdit *m_leftDir = nullptr;
    QLineEdit *m_rightDir = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QPushButton *m_compareBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QPushButton *m_historyBtn = nullptr;

    QPushButton *m_filterAllBtn = nullptr;
    QPushButton *m_filterDiffBtn = nullptr;
    QPushButton *m_filterLeftBtn = nullptr;
    QPushButton *m_filterRightBtn = nullptr;
    QPushButton *m_viewListBtn = nullptr;
    QPushButton *m_viewTreeBtn = nullptr;
    QCheckBox *m_autoBackup = nullptr;
    QLineEdit *m_ignoreEdit = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_statTotal = nullptr;
    QLabel *m_statSame = nullptr;
    QLabel *m_statDiff = nullptr;
    QLabel *m_statLeft = nullptr;
    QLabel *m_statRight = nullptr;
    QLabel *m_statMd5Same = nullptr;
    QLabel *m_statMd5Diff = nullptr;
    QLabel *m_statElapsed = nullptr;
    QProgressBar *m_progressBar = nullptr;

    QStackedWidget *m_viewStack = nullptr;
    QTableWidget *m_table = nullptr;
    QTreeWidget *m_tree = nullptr;
};

} // namespace tw
