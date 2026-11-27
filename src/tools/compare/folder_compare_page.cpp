#include "tools/compare/folder_compare_page.h"

#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
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

QString formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    return QStringLiteral("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 2));
}

bool isIniExtension(const QString &relPath)
{
    const QString ext = QFileInfo(relPath).suffix().toLower();
    return ext == QLatin1String("ini") || ext == QLatin1String("conf") || ext == QLatin1String("cfg");
}

QPair<QString, QColor> statusAppearance(cmp::FolderEntryStatus status, const Theme &theme)
{
    switch (status) {
    case cmp::FolderEntryStatus::Same:
        return {QStringLiteral("相同"), theme.statusSuccess};
    case cmp::FolderEntryStatus::Different:
        return {QStringLiteral("内容不同"), theme.danger};
    case cmp::FolderEntryStatus::LeftNewer:
        return {QStringLiteral("左侧较新"), theme.primary};
    case cmp::FolderEntryStatus::RightNewer:
        return {QStringLiteral("右侧较新"), theme.accent};
    case cmp::FolderEntryStatus::LeftOnly:
        return {QStringLiteral("仅左侧"), theme.primary};
    case cmp::FolderEntryStatus::RightOnly:
        return {QStringLiteral("仅右侧"), theme.accent};
    }
    return {QStringLiteral("未知"), theme.textMuted};
}

} // namespace

FolderComparePage::FolderComparePage(const ToolServices &services, QWidget *parent)
    : QWidget(parent)
    , m_services(services)
    , m_runner(new TaskRunner(services, this))
{
    setAcceptDrops(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(5);

    // ---- 路径栏 ----
    auto *pathRow = new QHBoxLayout;
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(6);

    m_leftDir = new QLineEdit;
    m_leftDir->setPlaceholderText(QStringLiteral("左侧本地文件夹路径（支持拖放目录）"));
    m_leftDir->setAccessibleName(QStringLiteral("左侧文件夹路径"));
    QPushButton *browseL = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));
    QPushButton *swapBtn = ui::makeActionButton(QStringLiteral("交换"), QStringLiteral("swap"));

    m_rightDir = new QLineEdit;
    m_rightDir->setPlaceholderText(QStringLiteral("右侧本地文件夹路径（支持拖放目录）"));
    m_rightDir->setAccessibleName(QStringLiteral("右侧文件夹路径"));
    QPushButton *browseR = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));

    m_modeCombo = new QComboBox;
    m_modeCombo->addItem(QStringLiteral("大小与时间"), static_cast<int>(cmp::FolderCompareMode::SizeAndTime));
    m_modeCombo->addItem(QStringLiteral("内容 SHA-256"), static_cast<int>(cmp::FolderCompareMode::ContentSha256));

    m_compareBtn = ui::makeActionButton(QStringLiteral("开始比较"), QStringLiteral("refresh"));
    m_cancelBtn = ui::makeActionButton(QStringLiteral("取消"), QStringLiteral("delete"));
    m_cancelBtn->setEnabled(false);
    m_historyBtn = ui::makeActionButton(QStringLiteral("历史"), QStringLiteral("log"));

    pathRow->addWidget(ui::makeLabel(QStringLiteral("左:"), "fieldLabel"));
    pathRow->addWidget(m_leftDir, 1);
    pathRow->addWidget(browseL);
    pathRow->addWidget(swapBtn);
    pathRow->addWidget(ui::makeLabel(QStringLiteral("右:"), "fieldLabel"));
    pathRow->addWidget(m_rightDir, 1);
    pathRow->addWidget(browseR);
    pathRow->addWidget(m_modeCombo);
    pathRow->addWidget(m_compareBtn);
    pathRow->addWidget(m_cancelBtn);
    pathRow->addWidget(m_historyBtn);
    root->addLayout(pathRow);

    // ---- 过滤、视图切换与批量同步工具栏 ----
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

    m_viewListBtn = new QPushButton(QStringLiteral("列表"));
    m_viewTreeBtn = new QPushButton(QStringLiteral("树形"));
    m_viewListBtn->setCheckable(true);
    m_viewTreeBtn->setCheckable(true);
    m_viewListBtn->setChecked(true);

    QPushButton *copyRBtn = ui::makeActionButton(QStringLiteral("复制到右 →"), QStringLiteral("copy_right"));
    QPushButton *copyLBtn = ui::makeActionButton(QStringLiteral("复制到左 ←"), QStringLiteral("copy_left"));
    QPushButton *openTextBtn = ui::makeActionButton(QStringLiteral("文本对比"), QStringLiteral("compare"));
    QPushButton *openIniBtn = ui::makeActionButton(QStringLiteral("INI 对比"), QStringLiteral("info"));
    QPushButton *exportBtn = ui::makeActionButton(QStringLiteral("导出"), QStringLiteral("export"));

    m_autoBackup = new QCheckBox(QStringLiteral(".bak"));
    m_autoBackup->setToolTip(QStringLiteral("覆盖目标文件前自动创建 .bak 备份"));
    m_autoBackup->setChecked(true);

    m_ignoreEdit = new QLineEdit(QStringLiteral(".git;*.bak;*.o;build"));
    m_ignoreEdit->setPlaceholderText(QStringLiteral("忽略规则(分号分隔)"));
    m_ignoreEdit->setToolTip(QStringLiteral("忽略文件/目录通配符，分号或逗号分隔，如 .git;*.bak;build"));
    m_ignoreEdit->setFixedWidth(155);
    m_ignoreEdit->setAccessibleName(QStringLiteral("忽略规则"));

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("过滤相对路径..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(160);
    m_searchEdit->setAccessibleName(QStringLiteral("过滤文件夹对比结果"));

    actionRow->addWidget(m_filterAllBtn);
    actionRow->addWidget(m_filterDiffBtn);
    actionRow->addWidget(m_filterLeftBtn);
    actionRow->addWidget(m_filterRightBtn);
    actionRow->addSpacing(4);
    actionRow->addWidget(m_viewListBtn);
    actionRow->addWidget(m_viewTreeBtn);
    actionRow->addSpacing(4);
    actionRow->addWidget(copyRBtn);
    actionRow->addWidget(copyLBtn);
    actionRow->addWidget(openTextBtn);
    actionRow->addWidget(openIniBtn);
    actionRow->addWidget(m_autoBackup);
    actionRow->addStretch(1);
    actionRow->addWidget(exportBtn);
    root->addLayout(actionRow);

    // ---- 统计、忽略规则与路径过滤条 ----
    auto *infoRow = new QHBoxLayout;
    infoRow->setContentsMargins(0, 0, 0, 0);
    infoRow->setSpacing(6);
    m_summaryLabel = ui::makeLabel(
        QStringLiteral("选择或拖放左右本地文件夹后点击“开始比较”；支持多选批量同步，双击文件直达对比。"),
        "hint");
    m_summaryLabel->setWordWrap(false);
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedWidth(120);
    m_progressBar->setFixedHeight(8);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    QLabel *ignoreLbl = ui::makeLabel(QStringLiteral("忽略:"), "hint");
    ignoreLbl->setWordWrap(false);
    infoRow->addWidget(m_summaryLabel, 1);
    infoRow->addWidget(m_progressBar);
    infoRow->addWidget(ignoreLbl);
    infoRow->addWidget(m_ignoreEdit);
    infoRow->addWidget(m_searchEdit);
    root->addLayout(infoRow);

    // ---- 列表视图 + 树形视图栈 ----
    m_viewStack = new QStackedWidget;

    m_table = new QTableWidget;
    m_table->setAccessibleName(QStringLiteral("文件夹比较明细表"));
    ui::configureDataTable(m_table,
                           {QStringLiteral("序号"),
                            QStringLiteral("相对路径"),
                            QStringLiteral("左侧大小"),
                            QStringLiteral("左侧修改时间"),
                            QStringLiteral("状态"),
                            QStringLiteral("右侧大小"),
                            QStringLiteral("右侧修改时间")});
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 48);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 84);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(3, 138);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_table->setColumnWidth(4, 82);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_table->setColumnWidth(5, 84);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    m_table->setColumnWidth(6, 138);

    m_tree = new QTreeWidget;
    m_tree->setAccessibleName(QStringLiteral("文件夹比较树形视图"));
    m_tree->setHeaderLabels({QStringLiteral("目录 / 文件名"),
                             QStringLiteral("左侧大小"),
                             QStringLiteral("左侧修改时间"),
                             QStringLiteral("状态"),
                             QStringLiteral("右侧大小"),
                             QStringLiteral("右侧修改时间")});
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setUniformRowHeights(true);
    m_tree->setIndentation(18);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_tree->setColumnWidth(1, 84);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_tree->setColumnWidth(2, 138);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_tree->setColumnWidth(3, 96);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_tree->setColumnWidth(4, 84);
    m_tree->header()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_tree->setColumnWidth(5, 138);

    m_viewStack->addWidget(m_table);
    m_viewStack->addWidget(m_tree);
    root->addWidget(m_viewStack, 1);

    // ---- 信号连接 ----
    connect(browseL, &QPushButton::clicked, this, &FolderComparePage::browseLeft);
    connect(browseR, &QPushButton::clicked, this, &FolderComparePage::browseRight);
    connect(swapBtn, &QPushButton::clicked, this, &FolderComparePage::swapDirs);
    connect(m_compareBtn, &QPushButton::clicked, this, &FolderComparePage::startCompare);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() { m_runner->cancelAndWait(); });
    connect(m_historyBtn, &QPushButton::clicked, this, &FolderComparePage::showHistoryMenu);

    connect(m_filterAllBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::All); });
    connect(m_filterDiffBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::DiffOnly); });
    connect(m_filterLeftBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::LeftOnly); });
    connect(m_filterRightBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::RightOnly); });

    connect(m_viewListBtn, &QPushButton::clicked, this, [this]() { setTreeViewMode(false); });
    connect(m_viewTreeBtn, &QPushButton::clicked, this, [this]() { setTreeViewMode(true); });

    connect(copyRBtn, &QPushButton::clicked, this, [this]() { copySelectedToRight(); });
    connect(copyLBtn, &QPushButton::clicked, this, [this]() { copySelectedToLeft(); });
    connect(openTextBtn, &QPushButton::clicked, this, [this]() { openSelectedInCompare(false); });
    connect(openIniBtn, &QPushButton::clicked, this, [this]() { openSelectedInCompare(true); });
    connect(exportBtn, &QPushButton::clicked, this, &FolderComparePage::exportReport);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { rebuildViews(); });

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        const int idx = primarySelectedEntryIndex();
        if (idx >= 0 && idx < m_result.entries.size())
            openSelectedInCompare(isIniExtension(m_result.entries.at(idx).relPath));
    });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
        if (!item)
            return;
        const QVariant v = item->data(0, Qt::UserRole);
        if (!v.isValid() || v.toInt() < 0)
            return;
        const int idx = v.toInt();
        if (idx >= 0 && idx < m_result.entries.size())
            openSelectedInCompare(isIniExtension(m_result.entries.at(idx).relPath));
    });

    connect(m_runner, &TaskRunner::progressChanged, this, [this](int pct) {
        m_progressBar->setValue(pct);
    });
    connect(m_runner, &TaskRunner::finished, this,
            [this](bool success, bool canceled, const QString &) {
                if (success && !canceled) {
                    QMutexLocker lock(&m_resultMutex);
                    m_result = m_pendingResult;
                }
                updateRunningUi();
                if (success && !canceled)
                    rebuildViews();
                emit compareFinished(success && !canceled);
            });
}

void FolderComparePage::setDirectories(const QString &leftDir, const QString &rightDir)
{
    m_leftDir->setText(leftDir);
    m_rightDir->setText(rightDir);
}

void FolderComparePage::setIgnorePatternsText(const QString &patterns)
{
    m_ignoreEdit->setText(patterns);
}

void FolderComparePage::setTreeViewMode(bool treeMode)
{
    m_treeMode = treeMode;
    m_viewListBtn->setChecked(!treeMode);
    m_viewTreeBtn->setChecked(treeMode);
    m_viewStack->setCurrentIndex(treeMode ? 1 : 0);
    rebuildViews();
}

bool FolderComparePage::isRunning() const
{
    return m_runner->isRunning();
}

void FolderComparePage::abort()
{
    m_runner->abort();
    updateRunningUi();
}

void FolderComparePage::startCompare()
{
    if (m_runner->isRunning())
        return;

    const QString left = m_leftDir->text().trimmed();
    const QString right = m_rightDir->text().trimmed();
    if (left.isEmpty() || right.isEmpty() || !QDir(left).exists() || !QDir(right).exists()) {
        const QString err = QStringLiteral("请先选择有效的左右本地文件夹路径");
        m_services.logWarning(err);
        m_services.showStatus(StatusState::Error, err);
        return;
    }

    cmp::recordCompareHistory(m_services.dataDir(), QStringLiteral("folder"), left, right);

    cmp::FolderCompareOptions opts;
    opts.mode = static_cast<cmp::FolderCompareMode>(m_modeCombo->currentData().toInt());
    opts.ignorePatterns = cmp::parseIgnorePatterns(m_ignoreEdit->text());

    m_progressBar->setValue(0);
    m_progressBar->show();
    m_runner->start(QStringLiteral("文件夹比较扫描中"),
                    [this, left, right, opts](TaskControl &ctl, QString *) -> bool {
                        cmp::FolderCompareResult res = cmp::compareFolders(
                            left, right, opts,
                            [&ctl]() { return ctl.isCancelRequested(); },
                            [&ctl](int pct, const QString &st) { ctl.reportProgress(pct, st); });
                        if (res.canceled)
                            return false;
                        QMutexLocker lock(&m_resultMutex);
                        m_pendingResult = std::move(res);
                        return true;
                    });
    updateRunningUi();
}

void FolderComparePage::compareSyncForTest()
{
    cmp::FolderCompareOptions opts;
    opts.mode = static_cast<cmp::FolderCompareMode>(m_modeCombo->currentData().toInt());
    opts.ignorePatterns = cmp::parseIgnorePatterns(m_ignoreEdit->text());
    m_result = cmp::compareFolders(m_leftDir->text().trimmed(), m_rightDir->text().trimmed(), opts);
    rebuildViews();
}

void FolderComparePage::updateRunningUi()
{
    const bool running = m_runner->isRunning();
    m_compareBtn->setEnabled(!running);
    m_cancelBtn->setEnabled(running);
    m_leftDir->setEnabled(!running);
    m_rightDir->setEnabled(!running);
    m_modeCombo->setEnabled(!running);
    m_ignoreEdit->setEnabled(!running);
    if (!running)
        m_progressBar->hide();
}

void FolderComparePage::rebuildViews()
{
    if (m_treeMode)
        rebuildTree();
    else
        rebuildTable();

    m_summaryLabel->setText(
        QStringLiteral("共 %1 项（相同 %2，差异 %3，仅左侧 %4，仅右侧 %5）— 支持多选批量同步，双击文件直达详细对比")
            .arg(m_result.entries.size())
            .arg(m_result.sameCount)
            .arg(m_result.diffCount)
            .arg(m_result.leftOnlyCount)
            .arg(m_result.rightOnlyCount));
}

void FolderComparePage::rebuildTable()
{
    const Theme &theme = Theme::workbench();
    const QString kw = m_searchEdit->text().trimmed();

    m_table->setRowCount(0);
    int visible = 0;
    for (int i = 0; i < m_result.entries.size(); ++i) {
        const cmp::FolderEntry &e = m_result.entries.at(i);
        if (m_filter == FilterMode::DiffOnly && e.status == cmp::FolderEntryStatus::Same)
            continue;
        if (m_filter == FilterMode::LeftOnly && e.status != cmp::FolderEntryStatus::LeftOnly)
            continue;
        if (m_filter == FilterMode::RightOnly && e.status != cmp::FolderEntryStatus::RightOnly)
            continue;
        if (!kw.isEmpty() && !e.relPath.contains(kw, Qt::CaseInsensitive))
            continue;

        const int row = visible++;
        m_table->insertRow(row);
        m_table->setRowHeight(row, 26);

        QTableWidgetItem *seqItem = makeCell(QString::number(row + 1), Qt::AlignCenter);
        seqItem->setData(Qt::UserRole, i);
        m_table->setItem(row, 0, seqItem);
        m_table->setItem(row, 1, makeCell(e.relPath));
        m_table->setItem(row, 2, makeCell(e.leftExists ? formatSize(e.leftSize) : QStringLiteral("—"), Qt::AlignRight | Qt::AlignVCenter));
        m_table->setItem(row, 3, makeCell(e.leftExists ? e.leftMtime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—"), Qt::AlignCenter));

        const auto st = statusAppearance(e.status, theme);
        QTableWidgetItem *stItem = makeCell(st.first, Qt::AlignCenter);
        stItem->setForeground(QBrush(st.second));
        m_table->setItem(row, 4, stItem);

        m_table->setItem(row, 5, makeCell(e.rightExists ? formatSize(e.rightSize) : QStringLiteral("—"), Qt::AlignRight | Qt::AlignVCenter));
        m_table->setItem(row, 6, makeCell(e.rightExists ? e.rightMtime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—"), Qt::AlignCenter));
    }
    if (m_table->rowCount() > 0)
        m_table->selectRow(0);
}

void FolderComparePage::rebuildTree()
{
    const Theme &theme = Theme::workbench();
    const QString kw = m_searchEdit->text().trimmed();

    m_tree->clear();
    QHash<QString, QTreeWidgetItem *> dirNodes;
    QHash<QString, int> dirDiffCounts;

    auto ensureDir = [&](const QString &dirPath) -> QTreeWidgetItem * {
        if (dirPath.isEmpty() || dirPath == QLatin1String("."))
            return nullptr;
        if (dirNodes.contains(dirPath))
            return dirNodes.value(dirPath);

        const QStringList parts = dirPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QString acc;
        QTreeWidgetItem *parent = nullptr;
        for (const QString &part : parts) {
            acc = acc.isEmpty() ? part : (acc + QLatin1Char('/') + part);
            if (!dirNodes.contains(acc)) {
                auto *node = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
                node->setText(0, part + QLatin1Char('/'));
                node->setData(0, Qt::UserRole, -1);
                node->setForeground(0, QBrush(theme.primary));
                dirNodes.insert(acc, node);
            }
            parent = dirNodes.value(acc);
        }
        return parent;
    };

    for (int i = 0; i < m_result.entries.size(); ++i) {
        const cmp::FolderEntry &e = m_result.entries.at(i);
        if (m_filter == FilterMode::DiffOnly && e.status == cmp::FolderEntryStatus::Same)
            continue;
        if (m_filter == FilterMode::LeftOnly && e.status != cmp::FolderEntryStatus::LeftOnly)
            continue;
        if (m_filter == FilterMode::RightOnly && e.status != cmp::FolderEntryStatus::RightOnly)
            continue;
        if (!kw.isEmpty() && !e.relPath.contains(kw, Qt::CaseInsensitive))
            continue;

        const QFileInfo fi(e.relPath);
        const QString dirPart = fi.path();
        QTreeWidgetItem *parent = ensureDir(dirPart);
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        item->setText(0, fi.fileName());
        item->setData(0, Qt::UserRole, i);
        item->setText(1, e.leftExists ? formatSize(e.leftSize) : QStringLiteral("—"));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setText(2, e.leftExists ? e.leftMtime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—"));
        item->setTextAlignment(2, Qt::AlignCenter);

        const auto st = statusAppearance(e.status, theme);
        item->setText(3, st.first);
        item->setTextAlignment(3, Qt::AlignCenter);
        item->setForeground(3, QBrush(st.second));

        item->setText(4, e.rightExists ? formatSize(e.rightSize) : QStringLiteral("—"));
        item->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
        item->setText(5, e.rightExists ? e.rightMtime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—"));
        item->setTextAlignment(5, Qt::AlignCenter);

        if (e.status != cmp::FolderEntryStatus::Same && dirPart != QLatin1String(".")) {
            QString acc;
            for (const QString &part : dirPart.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
                acc = acc.isEmpty() ? part : (acc + QLatin1Char('/') + part);
                dirDiffCounts[acc] += 1;
            }
        }
    }

    for (auto it = dirNodes.constBegin(); it != dirNodes.constEnd(); ++it) {
        const int diffs = dirDiffCounts.value(it.key(), 0);
        it.value()->setText(3, diffs > 0 ? QStringLiteral("差异 %1").arg(diffs) : QStringLiteral("一致"));
        it.value()->setTextAlignment(3, Qt::AlignCenter);
        it.value()->setForeground(3, QBrush(diffs > 0 ? theme.danger : theme.statusSuccess));
    }
    m_tree->expandAll();
}

QVector<int> FolderComparePage::selectedEntryIndices() const
{
    QSet<int> indices;
    if (m_treeMode) {
        for (QTreeWidgetItem *item : m_tree->selectedItems()) {
            const int idx = item->data(0, Qt::UserRole).toInt();
            if (idx >= 0)
                indices.insert(idx);
        }
    } else {
        for (const QModelIndex &mi : m_table->selectionModel()->selectedRows()) {
            if (QTableWidgetItem *it = m_table->item(mi.row(), 0)) {
                const int idx = it->data(Qt::UserRole).toInt();
                if (idx >= 0)
                    indices.insert(idx);
            }
        }
    }
    QVector<int> out = indices.values().toVector();
    std::sort(out.begin(), out.end());
    return out;
}

int FolderComparePage::primarySelectedEntryIndex() const
{
    const QVector<int> list = selectedEntryIndices();
    return list.isEmpty() ? -1 : list.first();
}

int FolderComparePage::copyEntriesToRight(const QVector<int> &entryIndices, QString *error)
{
    int copied = 0;
    for (int idx : entryIndices) {
        if (idx < 0 || idx >= m_result.entries.size())
            continue;
        const cmp::FolderEntry &e = m_result.entries.at(idx);
        if (!e.leftExists)
            continue;
        const QString src = QDir(m_leftDir->text().trimmed()).filePath(e.relPath);
        const QString dst = QDir(m_rightDir->text().trimmed()).filePath(e.relPath);
        QString bak, err;
        if (!cmp::copyFileWithBackup(src, dst, m_autoBackup->isChecked(), &bak, &err)) {
            if (error)
                *error = err;
            m_services.logError(err);
            continue;
        }
        ++copied;
    }
    if (copied > 0) {
        const QString msg = QStringLiteral("已批量复制到右侧：%1 个文件").arg(copied);
        m_services.logSuccess(msg);
        m_services.showStatus(StatusState::Success, msg);
        compareSyncForTest();
    }
    return copied;
}

int FolderComparePage::copyEntriesToLeft(const QVector<int> &entryIndices, QString *error)
{
    int copied = 0;
    for (int idx : entryIndices) {
        if (idx < 0 || idx >= m_result.entries.size())
            continue;
        const cmp::FolderEntry &e = m_result.entries.at(idx);
        if (!e.rightExists)
            continue;
        const QString src = QDir(m_rightDir->text().trimmed()).filePath(e.relPath);
        const QString dst = QDir(m_leftDir->text().trimmed()).filePath(e.relPath);
        QString bak, err;
        if (!cmp::copyFileWithBackup(src, dst, m_autoBackup->isChecked(), &bak, &err)) {
            if (error)
                *error = err;
            m_services.logError(err);
            continue;
        }
        ++copied;
    }
    if (copied > 0) {
        const QString msg = QStringLiteral("已批量复制到左侧：%1 个文件").arg(copied);
        m_services.logSuccess(msg);
        m_services.showStatus(StatusState::Success, msg);
        compareSyncForTest();
    }
    return copied;
}

bool FolderComparePage::copySelectedToRight(QString *error)
{
    return copyEntriesToRight(selectedEntryIndices(), error) > 0;
}

bool FolderComparePage::copySelectedToLeft(QString *error)
{
    return copyEntriesToLeft(selectedEntryIndices(), error) > 0;
}

void FolderComparePage::openSelectedInCompare(bool forceIni)
{
    const int idx = primarySelectedEntryIndex();
    if (idx < 0 || idx >= m_result.entries.size())
        return;
    const cmp::FolderEntry &e = m_result.entries.at(idx);
    const QString leftPath = e.leftExists ? QDir(m_leftDir->text().trimmed()).filePath(e.relPath) : QString();
    const QString rightPath = e.rightExists ? QDir(m_rightDir->text().trimmed()).filePath(e.relPath) : QString();
    emit openFileCompareRequested(leftPath, rightPath, forceIni);
}

void FolderComparePage::showHistoryMenu()
{
    const QVector<cmp::CompareHistoryItem> items =
        cmp::loadCompareHistory(m_services.dataDir(), QStringLiteral("folder"));
    QMenu menu(this);
    if (items.isEmpty()) {
        QAction *empty = menu.addAction(QStringLiteral("暂无历史对比记录"));
        empty->setEnabled(false);
    } else {
        for (const cmp::CompareHistoryItem &it : items) {
            QAction *act = menu.addAction(QStringLiteral("%1  ⇄  %2").arg(it.leftPath, it.rightPath));
            connect(act, &QAction::triggered, this, [this, it]() {
                setDirectories(it.leftPath, it.rightPath);
                startCompare();
            });
        }
    }
    menu.exec(m_historyBtn->mapToGlobal(QPoint(0, m_historyBtn->height())));
}

void FolderComparePage::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void FolderComparePage::dropEvent(QDropEvent *event)
{
    QStringList dirs;
    for (const QUrl &u : event->mimeData()->urls()) {
        if (!u.isLocalFile())
            continue;
        const QString p = u.toLocalFile();
        if (QFileInfo(p).isDir())
            dirs.append(p);
    }
    if (dirs.isEmpty())
        return;
    if (dirs.size() >= 2) {
        setDirectories(dirs.at(0), dirs.at(1));
        startCompare();
    } else {
        if (event->pos().x() < width() / 2)
            m_leftDir->setText(dirs.at(0));
        else
            m_rightDir->setText(dirs.at(0));
        if (!m_leftDir->text().isEmpty() && !m_rightDir->text().isEmpty())
            startCompare();
    }
}

void FolderComparePage::browseLeft()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择左侧文件夹"), m_leftDir->text());
    if (!dir.isEmpty())
        m_leftDir->setText(dir);
}

void FolderComparePage::browseRight()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择右侧文件夹"), m_rightDir->text());
    if (!dir.isEmpty())
        m_rightDir->setText(dir);
}

void FolderComparePage::swapDirs()
{
    const QString tmp = m_leftDir->text();
    m_leftDir->setText(m_rightDir->text());
    m_rightDir->setText(tmp);
    if (!m_leftDir->text().isEmpty() && !m_rightDir->text().isEmpty())
        startCompare();
}

void FolderComparePage::exportReport()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出文件夹比较报告"),
                                                      QStringLiteral("folder_diff_report.txt"),
                                                      QStringLiteral("文本报告 (*.txt *.tsv);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    const QString report = cmp::formatFolderDiffReport(m_leftDir->text(), m_rightDir->text(), m_result);
    QString err;
    if (!cmp::saveTextFileWithBackup(path, report, false, nullptr, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return;
    }
    m_services.logSuccess(QStringLiteral("已导出文件夹比较报告：%1").arg(path));
    m_services.showStatus(StatusState::Success, QStringLiteral("已导出文件夹比较报告"));
}

void FolderComparePage::setFilterMode(FilterMode mode)
{
    m_filter = mode;
    m_filterAllBtn->setChecked(mode == FilterMode::All);
    m_filterDiffBtn->setChecked(mode == FilterMode::DiffOnly);
    m_filterLeftBtn->setChecked(mode == FilterMode::LeftOnly);
    m_filterRightBtn->setChecked(mode == FilterMode::RightOnly);
    rebuildViews();
}

} // namespace tw
