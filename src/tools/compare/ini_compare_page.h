#pragma once

#include "core/platform_services.h"
#include "tools/compare/compare_engine.h"

#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace tw {

// INI / 配置文件按节键比较页（适配 sca_model_conf.ini 等多服务器配置核对）：
// 左侧 [Section] 统计表 + 五圆点分割条 + 右侧节键差异明细表 + 多选批量同步 + 编码探测切换 + 历史与拖放。
class IniComparePage : public QWidget
{
    Q_OBJECT

public:
    explicit IniComparePage(const ToolServices &services, QWidget *parent = nullptr);

    bool openFiles(const QString &leftPath, const QString &rightPath);
    void setContents(const QString &leftIniText, const QString &rightIniText,
                     const QString &leftLabel = QString(), const QString &rightLabel = QString());
    void loadSample();

    void setEncoding(cmp::TextEncoding encoding);
    cmp::TextEncoding selectedEncoding() const;

    void recompare();
    bool copySelectedToRight();
    bool copySelectedToLeft();
    bool saveLeft(QString *error = nullptr);
    bool saveRight(QString *error = nullptr);

    const cmp::IniDiffResult &diffResult() const { return m_diff; }
    const cmp::IniDocument &leftDocument() const { return m_leftDoc; }
    const cmp::IniDocument &rightDocument() const { return m_rightDoc; }
    QString leftMd5() const { return m_leftMd5; }
    QString rightMd5() const { return m_rightMd5; }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    enum class FilterMode { All, DiffOnly, LeftOnly, RightOnly };

    void browseLeft();
    void browseRight();
    void swapSides();
    void showHistoryMenu();
    void editSelectedEntry();
    void exportReport();
    void rebuildSectionTable();
    void rebuildEntryTable();
    void updateSummaryBar();
    void setFilterMode(FilterMode mode);
    QVector<int> selectedDiffIndices() const;
    int selectedDiffIndex() const;

    ToolServices m_services;
    cmp::IniDocument m_leftDoc;
    cmp::IniDocument m_rightDoc;
    cmp::IniDiffResult m_diff;
    QString m_leftMd5;
    QString m_rightMd5;
    FilterMode m_filter = FilterMode::All;
    bool m_leftDirty = false;
    bool m_rightDirty = false;
    cmp::TextEncoding m_leftDetectedEnc = cmp::TextEncoding::Utf8;
    cmp::TextEncoding m_rightDetectedEnc = cmp::TextEncoding::Utf8;

    QLineEdit *m_leftPath = nullptr;
    QLineEdit *m_rightPath = nullptr;
    QComboBox *m_encodingCombo = nullptr;
    QPushButton *m_historyBtn = nullptr;
    QPushButton *m_filterAllBtn = nullptr;
    QPushButton *m_filterDiffBtn = nullptr;
    QPushButton *m_filterLeftBtn = nullptr;
    QPushButton *m_filterRightBtn = nullptr;
    QCheckBox *m_ignoreCase = nullptr;
    QCheckBox *m_autoBackup = nullptr;
    QLineEdit *m_searchEdit = nullptr;

    QLabel *m_statSections = nullptr;
    QLabel *m_statTotal = nullptr;
    QLabel *m_statSame = nullptr;
    QLabel *m_statChanged = nullptr;
    QLabel *m_statLeft = nullptr;
    QLabel *m_statRight = nullptr;
    QLabel *m_statMd5 = nullptr;

    QTableWidget *m_sectionTable = nullptr;
    QTableWidget *m_entryTable = nullptr;
    QLabel *m_rightTitle = nullptr;
};

} // namespace tw
