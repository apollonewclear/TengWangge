#include "tools/compare/ini_compare_page.h"

#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
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
    setAcceptDrops(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(5);

    // ---- 路径栏 ----
    auto *pathRow = new QHBoxLayout;
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(5);

    m_leftPath = new QLineEdit;
    m_leftPath->setPlaceholderText(QStringLiteral("左侧 INI/配置文件（支持拖放，如 sca_model_conf.ini）"));
    m_leftPath->setAccessibleName(QStringLiteral("左侧 INI 路径"));
    QPushButton *browseL = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));
    QPushButton *saveL = ui::makeActionButton(QStringLiteral("保存左"), QStringLiteral("save"));
    QPushButton *swapBtn = ui::makeActionButton(QStringLiteral("交换"), QStringLiteral("swap"));

    m_rightPath = new QLineEdit;
    m_rightPath->setPlaceholderText(QStringLiteral("右侧 INI/配置文件（支持拖放，如 sca_model_conf.ini）"));
    m_rightPath->setAccessibleName(QStringLiteral("右侧 INI 路径"));
    QPushButton *browseR = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));
    QPushButton *saveR = ui::makeActionButton(QStringLiteral("保存右"), QStringLiteral("save"));

    m_encodingCombo = new QComboBox;
    m_encodingCombo->setToolTip(QStringLiteral("配置文件字符编码"));
    m_encodingCombo->addItem(QStringLiteral("自动编码"), static_cast<int>(cmp::TextEncoding::Auto));
    m_encodingCombo->addItem(QStringLiteral("UTF-8"), static_cast<int>(cmp::TextEncoding::Utf8));
    m_encodingCombo->addItem(QStringLiteral("GB18030/GBK"), static_cast<int>(cmp::TextEncoding::Gb18030));
    m_encodingCombo->addItem(QStringLiteral("UTF-16"), static_cast<int>(cmp::TextEncoding::Utf16));

    QPushButton *compareBtn = ui::makeActionButton(QStringLiteral("比较"), QStringLiteral("refresh"));
    m_historyBtn = ui::makeActionButton(QStringLiteral("历史"), QStringLiteral("log"));

    pathRow->addWidget(ui::makeLabel(QStringLiteral("左:"), "fieldLabel"));
    pathRow->addWidget(m_leftPath, 1);
    pathRow->addWidget(browseL);
    pathRow->addWidget(saveL);
    pathRow->addWidget(swapBtn);
    pathRow->addWidget(ui::makeLabel(QStringLiteral("右:"), "fieldLabel"));
    pathRow->addWidget(m_rightPath, 1);
    pathRow->addWidget(browseR);
    pathRow->addWidget(saveR);
    pathRow->addWidget(m_encodingCombo);
    pathRow->addWidget(compareBtn);
    pathRow->addWidget(m_historyBtn);
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

    // ---- 汇总统计条（节/键 KPI 徽章 + MD5 比对）----
    auto *summaryBar = new QFrame;
    summaryBar->setObjectName(QStringLiteral("summaryBar"));
    auto *sumLayout = new QHBoxLayout(summaryBar);
    sumLayout->setContentsMargins(8, 4, 8, 4);
    sumLayout->setSpacing(6);

    auto makeBadge = [](const QString &text, const char *role) {
        QLabel *lbl = ui::makeLabel(text, role);
        lbl->setWordWrap(false);
        return lbl;
    };
    m_statSections = makeBadge(QStringLiteral("配置节: 0"), "statTotal");
    m_statTotal = makeBadge(QStringLiteral("总键: 0"), "statTotal");
    m_statSame = makeBadge(QStringLiteral("相同: 0 (0%)"), "statSame");
    m_statChanged = makeBadge(QStringLiteral("值不同: 0"), "statDiff");
    m_statLeft = makeBadge(QStringLiteral("仅左: 0"), "statLeft");
    m_statRight = makeBadge(QStringLiteral("仅右: 0"), "statRight");
    m_statMd5 = makeBadge(QStringLiteral("MD5: —"), "statMd5Ok");

    sumLayout->addWidget(m_statSections);
    sumLayout->addWidget(m_statTotal);
    sumLayout->addWidget(m_statSame);
    sumLayout->addWidget(m_statChanged);
    sumLayout->addWidget(m_statLeft);
    sumLayout->addWidget(m_statRight);
    sumLayout->addStretch(1);
    sumLayout->addWidget(m_statMd5);
    root->addWidget(summaryBar);

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
    m_entryTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
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
    connect(m_historyBtn, &QPushButton::clicked, this, &IniComparePage::showHistoryMenu);
    connect(m_encodingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString lp = m_leftPath->text().trimmed();
        const QString rp = m_rightPath->text().trimmed();
        if (!lp.isEmpty() && !rp.isEmpty() && QFileInfo::exists(lp) && QFileInfo::exists(rp)
            && !m_leftDirty && !m_rightDirty) {
            openFiles(lp, rp);
        }
    });

    auto *saveSc = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_S), this);
    saveSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(saveSc, &QShortcut::activated, this, [this]() {
        if (m_leftDirty)
            saveLeft();
        if (m_rightDirty)
            saveRight();
    });
}

void IniComparePage::setEncoding(cmp::TextEncoding encoding)
{
    const int idx = m_encodingCombo->findData(static_cast<int>(encoding));
    if (idx >= 0)
        m_encodingCombo->setCurrentIndex(idx);
}

cmp::TextEncoding IniComparePage::selectedEncoding() const
{
    return static_cast<cmp::TextEncoding>(m_encodingCombo->currentData().toInt());
}

bool IniComparePage::openFiles(const QString &leftPath, const QString &rightPath)
{
    QString leftText;
    QString rightText;
    QString err;
    const cmp::TextEncoding req = selectedEncoding();
    if (!leftPath.isEmpty() && !cmp::loadTextFile(leftPath, &leftText, &err, req, &m_leftDetectedEnc)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    if (!rightPath.isEmpty() && !cmp::loadTextFile(rightPath, &rightText, &err, req, &m_rightDetectedEnc)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_leftPath->setText(leftPath);
    m_rightPath->setText(rightPath);
    cmp::recordCompareHistory(m_services.dataDir(), QStringLiteral("ini"), leftPath, rightPath);
    setContents(leftText, rightText, leftPath, rightPath);
    m_services.logInfo(QStringLiteral("INI 配置对比完成：%1 [%2] ⇄ %3 [%4]（值不同 %5，仅左 %6，仅右 %7）")
                           .arg(leftPath, cmp::encodingDisplayName(m_leftDetectedEnc),
                                rightPath, cmp::encodingDisplayName(m_rightDetectedEnc))
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
    const cmp::TextEncoding reqEnc = selectedEncoding();
    const cmp::TextEncoding leftEnc = (reqEnc == cmp::TextEncoding::Auto) ? m_leftDetectedEnc : reqEnc;
    const cmp::TextEncoding rightEnc = (reqEnc == cmp::TextEncoding::Auto) ? m_rightDetectedEnc : reqEnc;
    m_leftMd5 = cmp::computeTextMd5(m_leftDoc.serialize(), leftEnc);
    m_rightMd5 = cmp::computeTextMd5(m_rightDoc.serialize(), rightEnc);

    rebuildSectionTable();
    rebuildEntryTable();
    updateSummaryBar();

    const int totalDiff = m_diff.changedCount + m_diff.leftOnlyCount + m_diff.rightOnlyCount;
    m_services.showStatus(totalDiff == 0 ? StatusState::Success : StatusState::Info,
                          QStringLiteral("INI 比对：总计 %1 键（相同 %2，值不同 %3，仅左侧 %4，仅右侧 %5）")
                              .arg(m_diff.entries.size())
                              .arg(m_diff.equalCount)
                              .arg(m_diff.changedCount)
                              .arg(m_diff.leftOnlyCount)
                              .arg(m_diff.rightOnlyCount));
}

void IniComparePage::updateSummaryBar()
{
    const int total = m_diff.entries.size();
    const double pct = total > 0 ? (100.0 * m_diff.equalCount / total) : 100.0;
    m_statSections->setText(QStringLiteral("配置节: %1").arg(m_diff.sections.size()));
    m_statTotal->setText(QStringLiteral("总键: %1").arg(total));
    m_statSame->setText(QStringLiteral("相同: %1 (%2%)")
                            .arg(m_diff.equalCount)
                            .arg(QString::number(pct, 'f', total > 0 ? 1 : 0)));
    m_statChanged->setText(QStringLiteral("值不同: %1").arg(m_diff.changedCount));
    m_statLeft->setText(QStringLiteral("仅左: %1").arg(m_diff.leftOnlyCount));
    m_statRight->setText(QStringLiteral("仅右: %1").arg(m_diff.rightOnlyCount));

    if (m_leftMd5.isEmpty() && m_rightMd5.isEmpty()) {
        m_statMd5->setText(QStringLiteral("MD5: —"));
        ui::setRole(m_statMd5, "statMd5Ok");
        m_statMd5->setToolTip(QString());
    } else if (m_leftMd5 == m_rightMd5) {
        m_statMd5->setText(QStringLiteral("MD5 一致: %1").arg(m_leftMd5.left(8)));
        ui::setRole(m_statMd5, "statMd5Ok");
        m_statMd5->setToolTip(QStringLiteral("左右 INI 序列化内容 MD5 一致\nMD5: %1").arg(m_leftMd5));
    } else {
        m_statMd5->setText(QStringLiteral("MD5 不同: %1 ≠ %2").arg(m_leftMd5.left(8), m_rightMd5.left(8)));
        ui::setRole(m_statMd5, "statMd5Diff");
        m_statMd5->setToolTip(QStringLiteral("左侧 MD5: %1\n右侧 MD5: %2").arg(m_leftMd5, m_rightMd5));
    }
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

QVector<int> IniComparePage::selectedDiffIndices() const
{
    QSet<int> set;
    for (const QModelIndex &mi : m_entryTable->selectionModel()->selectedRows()) {
        if (QTableWidgetItem *it = m_entryTable->item(mi.row(), 0))
            set.insert(it->data(Qt::UserRole).toInt());
    }
    QVector<int> out = set.values().toVector();
    std::sort(out.begin(), out.end());
    return out;
}

int IniComparePage::selectedDiffIndex() const
{
    const QVector<int> list = selectedDiffIndices();
    return list.isEmpty() ? -1 : list.first();
}

bool IniComparePage::copySelectedToRight()
{
    const QVector<int> indices = selectedDiffIndices();
    if (indices.isEmpty())
        return false;
    for (int idx : indices) {
        if (idx < 0 || idx >= m_diff.entries.size())
            continue;
        const cmp::IniDiffEntry &e = m_diff.entries.at(idx);
        if (e.kind == cmp::IniDiffKind::RightOnly)
            m_rightDoc.removeKey(e.section, e.key, m_ignoreCase->isChecked());
        else
            m_rightDoc.setValue(e.section, e.key, e.leftValue, m_ignoreCase->isChecked());
    }
    m_rightDirty = true;
    recompare();
    m_services.logInfo(QStringLiteral("已批量同步 %1 项 INI 配置：左 → 右").arg(indices.size()));
    return true;
}

bool IniComparePage::copySelectedToLeft()
{
    const QVector<int> indices = selectedDiffIndices();
    if (indices.isEmpty())
        return false;
    for (int idx : indices) {
        if (idx < 0 || idx >= m_diff.entries.size())
            continue;
        const cmp::IniDiffEntry &e = m_diff.entries.at(idx);
        if (e.kind == cmp::IniDiffKind::LeftOnly)
            m_leftDoc.removeKey(e.section, e.key, m_ignoreCase->isChecked());
        else
            m_leftDoc.setValue(e.section, e.key, e.rightValue, m_ignoreCase->isChecked());
    }
    m_leftDirty = true;
    recompare();
    m_services.logInfo(QStringLiteral("已批量同步 %1 项 INI 配置：右 → 左").arg(indices.size()));
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
    const cmp::TextEncoding saveEnc = (selectedEncoding() == cmp::TextEncoding::Auto)
        ? m_leftDetectedEnc
        : selectedEncoding();
    QString bak;
    QString err;
    if (!cmp::saveTextFileWithBackup(path, m_leftDoc.serialize(), m_autoBackup->isChecked(), &bak, &err, saveEnc)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_leftDirty = false;
    const QString msg = bak.isEmpty()
        ? QStringLiteral("已保存左侧 INI：%1 [%2]").arg(path, cmp::encodingDisplayName(saveEnc))
        : QStringLiteral("已保存左侧 INI：%1 [%2]（备份：%3）")
              .arg(path, cmp::encodingDisplayName(saveEnc), bak);
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
    const cmp::TextEncoding saveEnc = (selectedEncoding() == cmp::TextEncoding::Auto)
        ? m_rightDetectedEnc
        : selectedEncoding();
    QString bak;
    QString err;
    if (!cmp::saveTextFileWithBackup(path, m_rightDoc.serialize(), m_autoBackup->isChecked(), &bak, &err, saveEnc)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_rightDirty = false;
    const QString msg = bak.isEmpty()
        ? QStringLiteral("已保存右侧 INI：%1 [%2]").arg(path, cmp::encodingDisplayName(saveEnc))
        : QStringLiteral("已保存右侧 INI：%1 [%2]（备份：%3）")
              .arg(path, cmp::encodingDisplayName(saveEnc), bak);
    m_services.logSuccess(msg);
    m_services.showStatus(StatusState::Success, msg);
    return true;
}

void IniComparePage::showHistoryMenu()
{
    const QVector<cmp::CompareHistoryItem> items =
        cmp::loadCompareHistory(m_services.dataDir(), QStringLiteral("ini"));
    QMenu menu(this);
    if (items.isEmpty()) {
        QAction *empty = menu.addAction(QStringLiteral("暂无历史对比记录"));
        empty->setEnabled(false);
    } else {
        for (const cmp::CompareHistoryItem &it : items) {
            QAction *act = menu.addAction(QStringLiteral("%1  ⇄  %2").arg(it.leftPath, it.rightPath));
            connect(act, &QAction::triggered, this, [this, it]() {
                openFiles(it.leftPath, it.rightPath);
            });
        }
    }
    menu.exec(m_historyBtn->mapToGlobal(QPoint(0, m_historyBtn->height())));
}

void IniComparePage::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void IniComparePage::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &u : event->mimeData()->urls()) {
        if (!u.isLocalFile())
            continue;
        const QString p = u.toLocalFile();
        if (QFileInfo(p).isFile())
            files.append(p);
    }
    if (files.isEmpty())
        return;
    if (files.size() >= 2) {
        openFiles(files.at(0), files.at(1));
    } else {
        QString text, err;
        cmp::TextEncoding det = cmp::TextEncoding::Utf8;
        if (!cmp::loadTextFile(files.at(0), &text, &err, selectedEncoding(), &det)) {
            m_services.logError(err);
            return;
        }
        if (event->pos().x() < width() / 2) {
            m_leftPath->setText(files.at(0));
            m_leftDoc = cmp::parseIni(text);
            m_leftDetectedEnc = det;
            m_leftDirty = false;
        } else {
            m_rightPath->setText(files.at(0));
            m_rightDoc = cmp::parseIni(text);
            m_rightDetectedEnc = det;
            m_rightDirty = false;
        }
        recompare();
    }
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
    if (!cmp::loadTextFile(path, &text, &err, selectedEncoding(), &m_leftDetectedEnc)) {
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
    if (!cmp::loadTextFile(path, &text, &err, selectedEncoding(), &m_rightDetectedEnc)) {
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
    std::swap(m_leftDetectedEnc, m_rightDetectedEnc);
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
