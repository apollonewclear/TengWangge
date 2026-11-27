#pragma once

#include "core/platform_services.h"
#include "tools/compare/compare_engine.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace tw {

// INI / 配置文件按节键比较页（适配 sca_model_conf.ini 等多服务器配置核对）：
// 左侧 [Section] 统计表 + 五圆点分割条 + 右侧节键差异明细表 + 选中项左右同步与原位编辑。
class IniComparePage : public QWidget
{
    Q_OBJECT

public:
    explicit IniComparePage(const ToolServices &services, QWidget *parent = nullptr);

    bool openFiles(const QString &leftPath, const QString &rightPath);
    void setContents(const QString &leftIniText, const QString &rightIniText,
                     const QString &leftLabel = QString(), const QString &rightLabel = QString());
    void loadSample();

    void recompare();
    bool copySelectedToRight();
    bool copySelectedToLeft();
    bool saveLeft(QString *error = nullptr);
    bool saveRight(QString *error = nullptr);

    const cmp::IniDiffResult &diffResult() const { return m_diff; }
    const cmp::IniDocument &leftDocument() const { return m_leftDoc; }
    const cmp::IniDocument &rightDocument() const { return m_rightDoc; }

private:
    enum class FilterMode { All, DiffOnly, LeftOnly, RightOnly };

    void browseLeft();
    void browseRight();
    void swapSides();
    void editSelectedEntry();
    void exportReport();
    void rebuildSectionTable();
    void rebuildEntryTable();
    void setFilterMode(FilterMode mode);
    int selectedDiffIndex() const;

    ToolServices m_services;
    cmp::IniDocument m_leftDoc;
    cmp::IniDocument m_rightDoc;
    cmp::IniDiffResult m_diff;
    FilterMode m_filter = FilterMode::All;
    bool m_leftDirty = false;
    bool m_rightDirty = false;

    QLineEdit *m_leftPath = nullptr;
    QLineEdit *m_rightPath = nullptr;
    QPushButton *m_filterAllBtn = nullptr;
    QPushButton *m_filterDiffBtn = nullptr;
    QPushButton *m_filterLeftBtn = nullptr;
    QPushButton *m_filterRightBtn = nullptr;
    QCheckBox *m_ignoreCase = nullptr;
    QCheckBox *m_autoBackup = nullptr;
    QLineEdit *m_searchEdit = nullptr;

    QTableWidget *m_sectionTable = nullptr;
    QTableWidget *m_entryTable = nullptr;
    QLabel *m_rightTitle = nullptr;
};

} // namespace tw
