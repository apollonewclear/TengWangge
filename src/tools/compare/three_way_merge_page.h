#pragma once

#include "core/platform_services.h"
#include "tools/compare/compare_engine.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace tw {

// 三方合并页（参考 Beyond Compare 3-Way Merge）：
// 顶部三栏（左侧 / 基线 / 右侧）+ 底部左侧变更与冲突清单 + 右侧可直接编辑的合并输出区。
class ThreeWayMergePage : public QWidget
{
    Q_OBJECT

public:
    explicit ThreeWayMergePage(const ToolServices &services, QWidget *parent = nullptr);

    bool openFiles(const QString &basePath, const QString &leftPath,
                   const QString &rightPath, const QString &outputPath = QString());
    void setInputs(const QString &baseText, const QString &leftText, const QString &rightText);
    void loadSample();

    void runMerge();
    void jumpToConflict(int chunkIndex);
    void prevConflict();
    void nextConflict();
    bool resolveCurrentChunk(cmp::MergeResolution resolution);
    void resolveAllConflicts(cmp::MergeResolution resolution);
    bool saveOutput(QString *error = nullptr);

    const cmp::ThreeWayMergeResult &mergeResult() const { return m_result; }
    QString mergedText() const;

private:
    void browseFile(QLineEdit *target, const QString &title, bool saveMode = false);
    void rebuildChunkTable();
    void refreshOutputEditor();
    void updateConflictBadge();
    void updateSummaryBar();
    void highlightChunkInSources(int chunkIndex);
    int selectedChunkIndex() const;

    ToolServices m_services;
    cmp::ThreeWayMergeResult m_result;
    bool m_outputManuallyEdited = false;
    bool m_suppressOutputSignal = false;

    QLineEdit *m_basePath = nullptr;
    QLineEdit *m_leftPath = nullptr;
    QLineEdit *m_rightPath = nullptr;
    QLineEdit *m_outputPath = nullptr;

    QLabel *m_conflictBadge = nullptr;
    QCheckBox *m_autoBackup = nullptr;

    QLabel *m_statTotal = nullptr;
    QLabel *m_statAuto = nullptr;
    QLabel *m_statResolved = nullptr;
    QLabel *m_statUnresolved = nullptr;
    QLabel *m_statOutLines = nullptr;

    QPlainTextEdit *m_leftView = nullptr;
    QPlainTextEdit *m_baseView = nullptr;
    QPlainTextEdit *m_rightView = nullptr;
    QTableWidget *m_chunkTable = nullptr;
    QPlainTextEdit *m_outputEdit = nullptr;
};

} // namespace tw
