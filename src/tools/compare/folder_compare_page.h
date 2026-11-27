#pragma once

#include "core/platform_services.h"
#include "core/task_runner.h"
#include "tools/compare/compare_engine.h"

#include <QMutex>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;

namespace tw {

// 文件夹比较页（参考 Beyond Compare Folder Compare）：
// 后台工作线程递归扫描（TaskRunner + 导航锁）+ 大小时间/SHA-256 双模式 +
// 过滤/双向文件同步（.bak 自动备份）+ 双击文件直达文本或 INI 对比页。
class FolderComparePage : public QWidget
{
    Q_OBJECT

public:
    explicit FolderComparePage(const ToolServices &services, QWidget *parent = nullptr);

    void setDirectories(const QString &leftDir, const QString &rightDir);
    void startCompare();
    void compareSyncForTest();
    bool copySelectedToRight(QString *error = nullptr);
    bool copySelectedToLeft(QString *error = nullptr);
    void abort();

    const cmp::FolderCompareResult &result() const { return m_result; }
    bool isRunning() const;

signals:
    void openFileCompareRequested(const QString &leftPath, const QString &rightPath, bool isIni);
    void compareFinished(bool success);

private:
    enum class FilterMode { All, DiffOnly, LeftOnly, RightOnly };

    void browseLeft();
    void browseRight();
    void swapDirs();
    void openSelectedInCompare(bool forceIni);
    void exportReport();
    void setFilterMode(FilterMode mode);
    void rebuildTable();
    void updateRunningUi();
    int selectedEntryIndex() const;

    ToolServices m_services;
    TaskRunner *m_runner = nullptr;
    mutable QMutex m_resultMutex;
    cmp::FolderCompareResult m_pendingResult;
    cmp::FolderCompareResult m_result;
    FilterMode m_filter = FilterMode::All;

    QLineEdit *m_leftDir = nullptr;
    QLineEdit *m_rightDir = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QPushButton *m_compareBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    QPushButton *m_filterAllBtn = nullptr;
    QPushButton *m_filterDiffBtn = nullptr;
    QPushButton *m_filterLeftBtn = nullptr;
    QPushButton *m_filterRightBtn = nullptr;
    QCheckBox *m_autoBackup = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QTableWidget *m_table = nullptr;
};

} // namespace tw
