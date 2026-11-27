#include "tools/compare/ini_compare_page.h"

#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace tw {
namespace {

QTableWidgetItem *makeCell(const QString &text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(static_cast<int>(align));
    return item;
}

QString sectionDisplay(const QString &sec)
{
    return sec.isEmpty() ? QStringLiteral("(全局)") : QStringLiteral("[%1]").arg(sec);
}

} // namespace

IniComparePage::IniComparePage(const ToolServices &services, QWidget *parent)
    : QWidget(parent)
    , m_services(services)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(5);

    // ---- 路径栏 ----
    auto *pathRow = new QHBoxLayout;
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(6);

    m_leftPath = new QLineEdit;
    m_leftPath->setPlaceholderText(QStringLiteral("左侧 INI/配置文件（如主服务器 sca_model_conf.ini）"));
    m_leftPath->setAccessibleName(QStringLiteral("左侧 INI 路径"));
    QPushButton *browseL = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));
    QPushButton *saveL = ui::makeActionButton(QStringLiteral("保存左"), QStringLiteral("save"));
    QPushButton *swapBtn = ui::makeActionButton(QStringLiteral("交换"), QStringLiteral("swap"));

    m_rightPath = new QLineEdit;
    m_rightPath->setPlaceholderText(QStringLiteral("右侧 INI/配置文件（如备服务器 sca_model_conf.ini）"));
    m_rightPath->setAccessibleName(QStringLiteral("右侧 INI 路径"));
    QPushButton *browseR = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));
    QPushButton *saveR = ui::makeActionButton(QStringLiteral("保存右"), QStringLiteral("save"));
    QPushButton *compareBtn = ui::makeActionButton(QStringLiteral("比较"), QStringLiteral("refresh"));

    pathRow->addWidget(ui::makeLabel(QStringLiteral("左:"), "fieldLabel"));
    pathRow->addWidget(m_leftPath, 1);
    pathRow->addWidget(browseL);
    pathRow->addWidget(saveL);
    pathRow->addWidget(swapBtn);
    pathRow->addWidget(ui::makeLabel(QStringLiteral("右:"), "fieldLabel"));
    pathRow->addWidget(m_rightPath, 1);
    pathRow->addWidget(browseR);
    pathRow->addWidget(saveR);
    pathRow->addWidget(compareBtn);
    root->addLayout(pathRow);

    // ---- 过滤与同步工具栏 ----
    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(6);

    m_filterAllBtn = new QPushButton(QStringLiteral("全部"));
    m_filterDiffBtn = new QPushButton(QStringLiteral("仅差异"));
    m_filterLeftBtn = new QPushButton(QStringLiteral("仅左侧"));
    m_filterRightBtn = new QPushButton(QStringLiteral("仅右侧"));
    for (QPushButton *b : {m_filterAllBtn, m_filterDiffBtn, m_filterLeftBtn, m_filterRightBtn})
        b->setCheckable(true);
    m_filterAllBtn->setChecked(true);

    QPushButton *copyRightBtn = ui::makeActionButton(QStringLiteral("同步 → 右"), QStringLiteral("copy_right"));
    QPushButton *copyLeftBtn = ui::makeActionButton(QStringLiteral("同步 ← 左"), QStringLiteral("copy_left"));
    QPushButton *editBtn = ui::makeActionButton(QStringLiteral("编辑值"), QStringLiteral("update"));

    m_ignoreCase = new QCheckBox(QStringLiteral("忽略大小写"));
    m_autoBackup = new QCheckBox(QStringLiteral("保存前 .bak"));
    m_autoBackup->setChecked(true);

    QPushButton *sampleBtn = ui::makeActionButton(QStringLiteral("示例"), QStringLiteral("add"));
    QPushButton *exportBtn = ui::makeActionButton(QStringLiteral("导出"), QStringLiteral("export"));

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("检索节名、键名或键值..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(190);
    m_searchEdit->setAccessibleName(QStringLiteral("检索 INI 差异"));

    actionRow->addWidget(m_filterAllBtn);
    actionRow->addWidget(m_filterDiffBtn);
    actionRow->addWidget(m_filterLeftBtn);
    actionRow->addWidget(m_filterRightBtn);
    actionRow->addSpacing(4);
    actionRow->addWidget(copyRightBtn);
    actionRow->addWidget(copyLeftBtn);
    actionRow->addWidget(editBtn);
    actionRow->addWidget(m_ignoreCase);
    actionRow->addWidget(m_autoBackup);
    actionRow->addStretch(1);
    actionRow->addWidget(sampleBtn);
    actionRow->addWidget(exportBtn);
    actionRow->addWidget(m_searchEdit);
    root->addLayout(actionRow);

    // ---- 左右工业分栏：左侧配置节统计表 + 右侧节键明细表 ----
    QSplitter *splitter = ui::makeSplitter(Qt::Horizontal, this);

    auto *leftPane = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(ui::makeLabel(QStringLiteral("配置节 [Section]"), "paneTitle"));

    m_sectionTable = new QTableWidget;
    m_sectionTable->setAccessibleName(QStringLiteral("INI 配置节列表"));
    ui::configureDataTable(m_sectionTable, {QStringLiteral("节名称"), QStringLiteral("差异/总计")});
    m_sectionTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_sectionTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_sectionTable->setColumnWidth(1, 74);
    leftLayout->addWidget(m_sectionTable, 1);
    leftPane->setMinimumWidth(195);
    leftPane->setMaximumWidth(270);

    auto *rightPane = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(4, 0, 0, 0);
    rightLayout->setSpacing(4);
    m_rightTitle = ui::makeLabel(QStringLiteral("全部配置节 — 键照明细"), "paneTitle");
    rightLayout->addWidget(m_rightTitle);

    m_entryTable = new QTableWidget;
    m_entryTable->setAccessibleName(QStringLiteral("INI 键值差异表"));
    ui::configureDataTable(m_entryTable,
                           {QStringLiteral("序号"),
                            QStringLiteral("节 [Section]"),
                            QStringLiteral("键名"),
                            QStringLiteral("左侧值"),
                            QStringLiteral("右侧值"),
                            QStringLiteral("状态")});
    m_entryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_entryTable->setColumnWidth(0, 48);
    m_entryTable->setColumnWidth(1, 130);
    m_entryTable->setColumnWidth(2, 155);
    m_entryTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_entryTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_entryTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_entryTable->setColumnWidth(5, 76);
    rightLayout->addWidget(m_entryTable, 1);

    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({215, 920});
    root->addWidget(splitter, 1);

    // ---- 信号连接 ----
    connect(browseL, &QPushButton::clicked, this, &IniComparePage::browseLeft);
    connect(browseR, &QPushButton::clicked, this, &IniComparePage::browseRight);
    connect(saveL, &QPushButton::clicked, this, [this]() { saveLeft(); });
    connect(saveR, &QPushButton::clicked, this, [this]() { saveRight(); });
    connect(swapBtn, &QPushButton::clicked, this, &IniComparePage::swapSides);
    connect(compareBtn, &QPushButton::clicked, this, [this]() {
        if (!m_leftPath->text().trimmed().isEmpty() && !m_rightPath->text().trimmed().isEmpty()
            && !m_leftDirty && !m_rightDirty) {
            openFiles(m_leftPath->text().trimmed(), m_rightPath->text().trimmed());
        } else {
            recompare();
        }
    });

    connect(m_filterAllBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::All); });
    connect(m_filterDiffBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::DiffOnly); });
    connect(m_filterLeftBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::LeftOnly); });
    connect(m_filterRightBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::RightOnly); });

    connect(copyRightBtn, &QPushButton::clicked, this, [this]() { copySelectedToRight(); });
    connect(copyLeftBtn, &QPushButton::clicked, this, [this]() { copySelectedToLeft(); });
    connect(editBtn, &QPushButton::clicked, this, &IniComparePage::editSelectedEntry);
    connect(m_ignoreCase, &QCheckBox::toggled, this, [this](bool) { recompare(); });
    connect(sampleBtn, &QPushButton::clicked, this, &IniComparePage::loadSample);
    connect(exportBtn, &QPushButton::clicked, this, &IniComparePage::exportReport);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { rebuildEntryTable(); });

    connect(m_sectionTable, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) {
                if (row >= 0 && m_sectionTable->item(row, 0)) {
                    m_rightTitle->setText(QStringLiteral("%1 — 键照明细")
                                              .arg(m_sectionTable->item(row, 0)->text()));
                }
                rebuildEntryTable();
            });
    connect(m_entryTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        editSelectedEntry();
    });
}

bool IniComparePage::openFiles(const QString &leftPath, const QString &rightPath)
{
    QString leftText;
    QString rightText;
    QString err;
    if (!leftPath.isEmpty() && !cmp::loadTextFile(leftPath, &leftText, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    if (!rightPath.isEmpty() && !cmp::loadTextFile(rightPath, &rightText, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_leftPath->setText(leftPath);
    m_rightPath->setText(rightPath);
    setContents(leftText, rightText, leftPath, rightPath);
    m_services.logInfo(QStringLiteral("INI 配置对比完成：%1 ⇄ %2（值不同 %3，仅左 %4，仅右 %5）")
                           .arg(leftPath, rightPath)
                           .arg(m_diff.changedCount)
                           .arg(m_diff.leftOnlyCount)
                           .arg(m_diff.rightOnlyCount));
    return true;
}

void IniComparePage::setContents(const QString &leftIniText, const QString &rightIniText,
                                 const QString &leftLabel, const QString &rightLabel)
{
    m_leftDoc = cmp::parseIni(leftIniText);
    m_rightDoc = cmp::parseIni(rightIniText);
    m_leftDirty = false;
    m_rightDirty = false;
    if (!leftLabel.isEmpty())
        m_leftPath->setText(leftLabel);
    if (!rightLabel.isEmpty())
        m_rightPath->setText(rightLabel);
    recompare();
}

void IniComparePage::loadSample()
{
    const QString leftIni = QStringLiteral(
        "# 主机 1 模型段配置 (sca_model_conf.ini)\n"
        "[Global]\n"
        "app_name = sca_obj\n"
        "sync_interval = 5\n"
        "enable_check = 1\n\n"
        "[Segment_Bus]\n"
        "sub_scene = real_time\n"
        "table_no = 401\n"
        "fields = id,name,voltage,quality\n\n"
        "[Segment_Line]\n"
        "sub_scene = real_time\n"
        "table_no = 402\n"
        "fields = id,name,active_power,reactive_power\n");
    const QString rightIni = QStringLiteral(
        "# 主机 2 模型段配置 (sca_model_conf.ini)\n"
        "[Global]\n"
        "app_name = sca_obj\n"
        "sync_interval = 10\n"
        "enable_check = 1\n"
        "strict_mode = 1\n\n"
        "[Segment_Bus]\n"
        "sub_scene = real_time\n"
        "table_no = 401\n"
        "fields = id,name,voltage,quality,freq\n\n"
        "[Segment_Line]\n"
        "sub_scene = real_time\n"
        "table_no = 402\n");
    setContents(leftIni, rightIni, QStringLiteral("wk1/sca_model_conf.ini"),
                QStringLiteral("wk2/sca_model_conf.ini"));
    m_services.showStatus(StatusState::Info, QStringLiteral("已载入 sca_model_conf.ini 多机配置核对示例"));
}

void IniComparePage::recompare()
{
    m_diff = cmp::diffIni(m_leftDoc, m_rightDoc, m_ignoreCase->isChecked());
    rebuildSectionTable();
    rebuildEntryTable();

    const int totalDiff = m_diff.changedCount + m_diff.leftOnlyCount + m_diff.rightOnlyCount;
    m_services.showStatus(totalDiff == 0 ? StatusState::Success : StatusState::Info,
                          QStringLiteral("INI 比对：相同 %1，值不同 %2，仅左侧 %3，仅右侧 %4")
                              .arg(m_diff.equalCount)
                              .arg(m_diff.changedCount)
                              .arg(m_diff.leftOnlyCount)
                              .arg(m_diff.rightOnlyCount));
}

void IniComparePage::rebuildSectionTable()
{
    const Theme &theme = Theme::workbench();
    QString prevSec;
    bool hadAll = true;
    const int curRow = m_sectionTable->currentRow();
    if (curRow > 0 && m_sectionTable->item(curRow, 0)) {
        hadAll = false;
        prevSec = m_sectionTable->item(curRow, 0)->data(Qt::UserRole).toString();
    }

    m_sectionTable->setRowCount(0);
    const int totalDiff = m_diff.changedCount + m_diff.leftOnlyCount + m_diff.rightOnlyCount;
    m_sectionTable->insertRow(0);
    m_sectionTable->setRowHeight(0, 26);
    QTableWidgetItem *allItem = makeCell(QStringLiteral("全部配置节"));
    allItem->setData(Qt::UserRole, QStringLiteral("__ALL__"));
    m_sectionTable->setItem(0, 0, allItem);
    m_sectionTable->setItem(0, 1, makeCell(QStringLiteral("%1/%2").arg(totalDiff).arg(m_diff.entries.size()), Qt::AlignCenter));

    int selectIdx = 0;
    for (const QString &sec : m_diff.sections) {
        int secTotal = 0;
        int secDiff = 0;
        for (const cmp::IniDiffEntry &e : m_diff.entries) {
            if (e.section == sec) {
                ++secTotal;
                if (e.kind != cmp::IniDiffKind::Equal)
                    ++secDiff;
            }
        }
        const int row = m_sectionTable->rowCount();
        m_sectionTable->insertRow(row);
        m_sectionTable->setRowHeight(row, 26);
        QTableWidgetItem *secItem = makeCell(sectionDisplay(sec));
        secItem->setData(Qt::UserRole, sec);
        m_sectionTable->setItem(row, 0, secItem);

        QTableWidgetItem *cntItem = makeCell(QStringLiteral("%1/%2").arg(secDiff).arg(secTotal), Qt::AlignCenter);
        cntItem->setForeground(QBrush(secDiff > 0 ? theme.danger : theme.statusSuccess));
        m_sectionTable->setItem(row, 1, cntItem);

        if (!hadAll && sec == prevSec)
            selectIdx = row;
    }
    m_sectionTable->selectRow(selectIdx);
}

void IniComparePage::rebuildEntryTable()
{
    const Theme &theme = Theme::workbench();
    QString selectedSection = QStringLiteral("__ALL__");
    const int secRow = m_sectionTable->currentRow();
    if (secRow >= 0 && m_sectionTable->item(secRow, 0))
        selectedSection = m_sectionTable->item(secRow, 0)->data(Qt::UserRole).toString();
    const QString kw = m_searchEdit->text().trimmed();

    m_entryTable->setRowCount(0);
    int visible = 0;
    for (int i = 0; i < m_diff.entries.size(); ++i) {
        const cmp::IniDiffEntry &e = m_diff.entries.at(i);
        if (selectedSection != QLatin1String("__ALL__") && e.section != selectedSection)
            continue;
        if (m_filter == FilterMode::DiffOnly && e.kind == cmp::IniDiffKind::Equal)
            continue;
        if (m_filter == FilterMode::LeftOnly && e.kind != cmp::IniDiffKind::LeftOnly)
            continue;
        if (m_filter == FilterMode::RightOnly && e.kind != cmp::IniDiffKind::RightOnly)
            continue;
        if (!kw.isEmpty()
            && !e.section.contains(kw, Qt::CaseInsensitive)
            && !e.key.contains(kw, Qt::CaseInsensitive)
            && !e.leftValue.contains(kw, Qt::CaseInsensitive)
            && !e.rightValue.contains(kw, Qt::CaseInsensitive)) {
            continue;
        }

        const int row = visible++;
        m_entryTable->insertRow(row);
        m_entryTable->setRowHeight(row, 26);

        QTableWidgetItem *seqItem = makeCell(QString::number(row + 1), Qt::AlignCenter);
        seqItem->setData(Qt::UserRole, i);
        m_entryTable->setItem(row, 0, seqItem);
        m_entryTable->setItem(row, 1, makeCell(sectionDisplay(e.section)));

        QTableWidgetItem *keyItem = makeCell(e.key);
        keyItem->setForeground(QBrush(theme.primary));
        m_entryTable->setItem(row, 2, keyItem);
        m_entryTable->setItem(row, 3, makeCell(e.kind == cmp::IniDiffKind::RightOnly ? QStringLiteral("—") : e.leftValue));
        m_entryTable->setItem(row, 4, makeCell(e.kind == cmp::IniDiffKind::LeftOnly ? QStringLiteral("—") : e.rightValue));

        QString stText;
        QColor stCol = theme.statusSuccess;
        if (e.kind == cmp::IniDiffKind::Equal) {
            stText = QStringLiteral("相同");
            stCol = theme.statusSuccess;
        } else if (e.kind == cmp::IniDiffKind::Changed) {
            stText = QStringLiteral("值不同");
            stCol = theme.danger;
        } else if (e.kind == cmp::IniDiffKind::LeftOnly) {
            stText = QStringLiteral("仅左侧");
            stCol = theme.primary;
        } else {
            stText = QStringLiteral("仅右侧");
            stCol = theme.accent;
        }
        QTableWidgetItem *stItem = makeCell(stText, Qt::AlignCenter);
        stItem->setForeground(QBrush(stCol));
        m_entryTable->setItem(row, 5, stItem);
    }
    if (m_entryTable->rowCount() > 0)
        m_entryTable->selectRow(0);
}

int IniComparePage::selectedDiffIndex() const
{
    const int row = m_entryTable->currentRow();
    if (row < 0 || !m_entryTable->item(row, 0))
        return -1;
    return m_entryTable->item(row, 0)->data(Qt::UserRole).toInt();
}

bool IniComparePage::copySelectedToRight()
{
    const int idx = selectedDiffIndex();
    if (idx < 0 || idx >= m_diff.entries.size())
        return false;
    const cmp::IniDiffEntry &e = m_diff.entries.at(idx);
    if (e.kind == cmp::IniDiffKind::RightOnly)
        m_rightDoc.removeKey(e.section, e.key, m_ignoreCase->isChecked());
    else
        m_rightDoc.setValue(e.section, e.key, e.leftValue, m_ignoreCase->isChecked());
    m_rightDirty = true;
    recompare();
    m_services.logInfo(QStringLiteral("已同步配置项 [%1] %2：左 → 右").arg(e.section, e.key));
    return true;
}

bool IniComparePage::copySelectedToLeft()
{
    const int idx = selectedDiffIndex();
    if (idx < 0 || idx >= m_diff.entries.size())
        return false;
    const cmp::IniDiffEntry &e = m_diff.entries.at(idx);
    if (e.kind == cmp::IniDiffKind::LeftOnly)
        m_leftDoc.removeKey(e.section, e.key, m_ignoreCase->isChecked());
    else
        m_leftDoc.setValue(e.section, e.key, e.rightValue, m_ignoreCase->isChecked());
    m_leftDirty = true;
    recompare();
    m_services.logInfo(QStringLiteral("已同步配置项 [%1] %2：右 → 左").arg(e.section, e.key));
    return true;
}

void IniComparePage::editSelectedEntry()
{
    const int idx = selectedDiffIndex();
    if (idx < 0 || idx >= m_diff.entries.size())
        return;
    const cmp::IniDiffEntry &e = m_diff.entries.at(idx);
    const int col = m_entryTable->currentColumn();
    const bool editRight = (col == 4);
    const QString sideLabel = editRight ? QStringLiteral("右侧") : QStringLiteral("左侧");
    const QString curVal = editRight ? e.rightValue : e.leftValue;

    bool ok = false;
    const QString newVal = QInputDialog::getText(
        this,
        QStringLiteral("编辑%1配置值").arg(sideLabel),
        QStringLiteral("[%1] %2 =").arg(e.section, e.key),
        QLineEdit::Normal,
        curVal,
        &ok);
    if (!ok)
        return;

    if (editRight) {
        m_rightDoc.setValue(e.section, e.key, newVal, m_ignoreCase->isChecked());
        m_rightDirty = true;
    } else {
        m_leftDoc.setValue(e.section, e.key, newVal, m_ignoreCase->isChecked());
        m_leftDirty = true;
    }
    recompare();
    m_services.logInfo(QStringLiteral("已修改%1配置项 [%2] %3 = %4").arg(sideLabel, e.section, e.key, newVal));
}

bool IniComparePage::saveLeft(QString *error)
{
    QString path = m_leftPath->text().trimmed();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, QStringLiteral("保存左侧 INI 文件"));
        if (path.isEmpty())
            return false;
        m_leftPath->setText(path);
    }
    QString bak;
    QString err;
    if (!cmp::saveTextFileWithBackup(path, m_leftDoc.serialize(), m_autoBackup->isChecked(), &bak, &err)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_leftDirty = false;
    const QString msg = bak.isEmpty()
        ? QStringLiteral("已保存左侧 INI：%1").arg(path)
        : QStringLiteral("已保存左侧 INI：%1（自动备份：%2）").arg(path, bak);
    m_services.logSuccess(msg);
    m_services.showStatus(StatusState::Success, msg);
    return true;
}

bool IniComparePage::saveRight(QString *error)
{
    QString path = m_rightPath->text().trimmed();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, QStringLiteral("保存右侧 INI 文件"));
        if (path.isEmpty())
            return false;
        m_rightPath->setText(path);
    }
    QString bak;
    QString err;
    if (!cmp::saveTextFileWithBackup(path, m_rightDoc.serialize(), m_autoBackup->isChecked(), &bak, &err)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_rightDirty = false;
    const QString msg = bak.isEmpty()
        ? QStringLiteral("已保存右侧 INI：%1").arg(path)
        : QStringLiteral("已保存右侧 INI：%1（自动备份：%2）").arg(path, bak);
    m_services.logSuccess(msg);
    m_services.showStatus(StatusState::Success, msg);
    return true;
}

void IniComparePage::browseLeft()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择左侧 INI/配置文件"),
                                                      m_leftPath->text(),
                                                      QStringLiteral("配置文件 (*.ini *.conf *.cfg);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    QString text;
    QString err;
    if (!cmp::loadTextFile(path, &text, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return;
    }
    m_leftPath->setText(path);
    m_leftDoc = cmp::parseIni(text);
    m_leftDirty = false;
    recompare();
}

void IniComparePage::browseRight()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择右侧 INI/配置文件"),
                                                      m_rightPath->text(),
                                                      QStringLiteral("配置文件 (*.ini *.conf *.cfg);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    QString text;
    QString err;
    if (!cmp::loadTextFile(path, &text, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return;
    }
    m_rightPath->setText(path);
    m_rightDoc = cmp::parseIni(text);
    m_rightDirty = false;
    recompare();
}

void IniComparePage::swapSides()
{
    const QString lp = m_leftPath->text();
    m_leftPath->setText(m_rightPath->text());
    m_rightPath->setText(lp);
    std::swap(m_leftDoc, m_rightDoc);
    std::swap(m_leftDirty, m_rightDirty);
    recompare();
}

void IniComparePage::exportReport()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 INI 差异报告"),
                                                      QStringLiteral("ini_diff_report.txt"),
                                                      QStringLiteral("文本报告 (*.txt *.tsv);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    const QString report = cmp::formatIniDiffReport(m_leftPath->text(), m_rightPath->text(), m_diff);
    QString err;
    if (!cmp::saveTextFileWithBackup(path, report, false, nullptr, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return;
    }
    m_services.logSuccess(QStringLiteral("已导出 INI 差异报告：%1").arg(path));
    m_services.showStatus(StatusState::Success, QStringLiteral("已导出 INI 差异报告"));
}

void IniComparePage::setFilterMode(FilterMode mode)
{
    m_filter = mode;
    m_filterAllBtn->setChecked(mode == FilterMode::All);
    m_filterDiffBtn->setChecked(mode == FilterMode::DiffOnly);
    m_filterLeftBtn->setChecked(mode == FilterMode::LeftOnly);
    m_filterRightBtn->setChecked(mode == FilterMode::RightOnly);
    rebuildEntryTable();
}

} // namespace tw
