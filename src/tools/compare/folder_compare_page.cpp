#include "tools/compare/folder_compare_page.h"

#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
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

} // namespace

FolderComparePage::FolderComparePage(const ToolServices &services, QWidget *parent)
    : QWidget(parent)
    , m_services(services)
    , m_runner(new TaskRunner(services, this))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(5);

    // ---- 路径栏 ----
    auto *pathRow = new QHBoxLayout;
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(6);

    m_leftDir = new QLineEdit;
    m_leftDir->setPlaceholderText(QStringLiteral("左侧本地文件夹路径"));
    m_leftDir->setAccessibleName(QStringLiteral("左侧文件夹路径"));
    QPushButton *browseL = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));
    QPushButton *swapBtn = ui::makeActionButton(QStringLiteral("交换"), QStringLiteral("swap"));

    m_rightDir = new QLineEdit;
    m_rightDir->setPlaceholderText(QStringLiteral("右侧本地文件夹路径"));
    m_rightDir->setAccessibleName(QStringLiteral("右侧文件夹路径"));
    QPushButton *browseR = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));

    m_modeCombo = new QComboBox;
    m_modeCombo->addItem(QStringLiteral("大小与时间"), static_cast<int>(cmp::FolderCompareMode::SizeAndTime));
    m_modeCombo->addItem(QStringLiteral("内容 SHA-256"), static_cast<int>(cmp::FolderCompareMode::ContentSha256));

    m_compareBtn = ui::makeActionButton(QStringLiteral("开始比较"), QStringLiteral("refresh"));
    m_cancelBtn = ui::makeActionButton(QStringLiteral("取消"), QStringLiteral("delete"));
    m_cancelBtn->setEnabled(false);

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

    QPushButton *copyRBtn = ui::makeActionButton(QStringLiteral("复制到右 →"), QStringLiteral("copy_right"));
    QPushButton *copyLBtn = ui::makeActionButton(QStringLiteral("复制到左 ←"), QStringLiteral("copy_left"));
    QPushButton *openTextBtn = ui::makeActionButton(QStringLiteral("文本对比"), QStringLiteral("compare"));
    QPushButton *openIniBtn = ui::makeActionButton(QStringLiteral("INI 对比"), QStringLiteral("info"));
    QPushButton *exportBtn = ui::makeActionButton(QStringLiteral("导出"), QStringLiteral("export"));

    m_autoBackup = new QCheckBox(QStringLiteral("覆盖前 .bak"));
    m_autoBackup->setChecked(true);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("过滤相对路径..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(180);
    m_searchEdit->setAccessibleName(QStringLiteral("过滤文件夹对比结果"));

    actionRow->addWidget(m_filterAllBtn);
    actionRow->addWidget(m_filterDiffBtn);
    actionRow->addWidget(m_filterLeftBtn);
    actionRow->addWidget(m_filterRightBtn);
    actionRow->addSpacing(4);
    actionRow->addWidget(copyRBtn);
    actionRow->addWidget(copyLBtn);
    actionRow->addWidget(openTextBtn);
    actionRow->addWidget(openIniBtn);
    actionRow->addWidget(m_autoBackup);
    actionRow->addStretch(1);
    actionRow->addWidget(exportBtn);
    actionRow->addWidget(m_searchEdit);
    root->addLayout(actionRow);

    // ---- 进度与统计条 ----
    auto *infoRow = new QHBoxLayout;
    infoRow->setContentsMargins(0, 0, 0, 0);
    infoRow->setSpacing(8);
    m_summaryLabel = ui::makeLabel(
        QStringLiteral("选择左右两个本地文件夹后点击“开始比较”；双击差异文件可直接进入文本或 INI 对比。"),
        "hint");
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedWidth(160);
    m_progressBar->setFixedHeight(8);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    infoRow->addWidget(m_summaryLabel, 1);
    infoRow->addWidget(m_progressBar);
    root->addLayout(infoRow);

    // ---- 结果表格 ----
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
    root->addWidget(m_table, 1);

    // ---- 信号连接 ----
    connect(browseL, &QPushButton::clicked, this, &FolderComparePage::browseLeft);
    connect(browseR, &QPushButton::clicked, this, &FolderComparePage::browseRight);
    connect(swapBtn, &QPushButton::clicked, this, &FolderComparePage::swapDirs);
    connect(m_compareBtn, &QPushButton::clicked, this, &FolderComparePage::startCompare);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() { m_runner->cancelAndWait(); });

    connect(m_filterAllBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::All); });
    connect(m_filterDiffBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::DiffOnly); });
    connect(m_filterLeftBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::LeftOnly); });
    connect(m_filterRightBtn, &QPushButton::clicked, this, [this]() { setFilterMode(FilterMode::RightOnly); });

    connect(copyRBtn, &QPushButton::clicked, this, [this]() { copySelectedToRight(); });
    connect(copyLBtn, &QPushButton::clicked, this, [this]() { copySelectedToLeft(); });
    connect(openTextBtn, &QPushButton::clicked, this, [this]() { openSelectedInCompare(false); });
    connect(openIniBtn, &QPushButton::clicked, this, [this]() { openSelectedInCompare(true); });
    connect(exportBtn, &QPushButton::clicked, this, &FolderComparePage::exportReport);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { rebuildTable(); });

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        const int idx = selectedEntryIndex();
        if (idx < 0 || idx >= m_result.entries.size())
            return;
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
                    rebuildTable();
                emit compareFinished(success && !canceled);
            });
}

void FolderComparePage::setDirectories(const QString &leftDir, const QString &rightDir)
{
    m_leftDir->setText(leftDir);
    m_rightDir->setText(rightDir);
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

    cmp::FolderCompareOptions opts;
    opts.mode = static_cast<cmp::FolderCompareMode>(m_modeCombo->currentData().toInt());

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
    m_result = cmp::compareFolders(m_leftDir->text().trimmed(), m_rightDir->text().trimmed(), opts);
    rebuildTable();
}

void FolderComparePage::updateRunningUi()
{
    const bool running = m_runner->isRunning();
    m_compareBtn->setEnabled(!running);
    m_cancelBtn->setEnabled(running);
    m_leftDir->setEnabled(!running);
    m_rightDir->setEnabled(!running);
    m_modeCombo->setEnabled(!running);
    if (!running)
        m_progressBar->hide();
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

        QString stText;
        QColor stCol = theme.textMuted;
        switch (e.status) {
        case cmp::FolderEntryStatus::Same:
            stText = QStringLiteral("相同");
            stCol = theme.statusSuccess;
            break;
        case cmp::FolderEntryStatus::Different:
            stText = QStringLiteral("内容不同");
            stCol = theme.danger;
            break;
        case cmp::FolderEntryStatus::LeftNewer:
            stText = QStringLiteral("左侧较新");
            stCol = theme.primary;
            break;
        case cmp::FolderEntryStatus::RightNewer:
            stText = QStringLiteral("右侧较新");
            stCol = theme.accent;
            break;
        case cmp::FolderEntryStatus::LeftOnly:
            stText = QStringLiteral("仅左侧");
            stCol = theme.primary;
            break;
        case cmp::FolderEntryStatus::RightOnly:
            stText = QStringLiteral("仅右侧");
            stCol = theme.accent;
            break;
        }
        QTableWidgetItem *stItem = makeCell(stText, Qt::AlignCenter);
        stItem->setForeground(QBrush(stCol));
        m_table->setItem(row, 4, stItem);

        m_table->setItem(row, 5, makeCell(e.rightExists ? formatSize(e.rightSize) : QStringLiteral("—"), Qt::AlignRight | Qt::AlignVCenter));
        m_table->setItem(row, 6, makeCell(e.rightExists ? e.rightMtime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—"), Qt::AlignCenter));
    }
    if (m_table->rowCount() > 0)
        m_table->selectRow(0);

    m_summaryLabel->setText(
        QStringLiteral("共 %1 项（相同 %2，差异 %3，仅左侧 %4，仅右侧 %5）— 双击行打开详细对比")
            .arg(m_result.entries.size())
            .arg(m_result.sameCount)
            .arg(m_result.diffCount)
            .arg(m_result.leftOnlyCount)
            .arg(m_result.rightOnlyCount));
}

int FolderComparePage::selectedEntryIndex() const
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0))
        return -1;
    return m_table->item(row, 0)->data(Qt::UserRole).toInt();
}

bool FolderComparePage::copySelectedToRight(QString *error)
{
    const int idx = selectedEntryIndex();
    if (idx < 0 || idx >= m_result.entries.size())
        return false;
    const cmp::FolderEntry &e = m_result.entries.at(idx);
    if (!e.leftExists)
        return false;

    const QString src = QDir(m_leftDir->text().trimmed()).filePath(e.relPath);
    const QString dst = QDir(m_rightDir->text().trimmed()).filePath(e.relPath);
    QString bak, err;
    if (!cmp::copyFileWithBackup(src, dst, m_autoBackup->isChecked(), &bak, &err)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_services.logSuccess(QStringLiteral("已复制到右侧：%1").arg(e.relPath));
    compareSyncForTest();
    return true;
}

bool FolderComparePage::copySelectedToLeft(QString *error)
{
    const int idx = selectedEntryIndex();
    if (idx < 0 || idx >= m_result.entries.size())
        return false;
    const cmp::FolderEntry &e = m_result.entries.at(idx);
    if (!e.rightExists)
        return false;

    const QString src = QDir(m_rightDir->text().trimmed()).filePath(e.relPath);
    const QString dst = QDir(m_leftDir->text().trimmed()).filePath(e.relPath);
    QString bak, err;
    if (!cmp::copyFileWithBackup(src, dst, m_autoBackup->isChecked(), &bak, &err)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_services.logSuccess(QStringLiteral("已复制到左侧：%1").arg(e.relPath));
    compareSyncForTest();
    return true;
}

void FolderComparePage::openSelectedInCompare(bool forceIni)
{
    const int idx = selectedEntryIndex();
    if (idx < 0 || idx >= m_result.entries.size())
        return;
    const cmp::FolderEntry &e = m_result.entries.at(idx);
    const QString leftPath = e.leftExists ? QDir(m_leftDir->text().trimmed()).filePath(e.relPath) : QString();
    const QString rightPath = e.rightExists ? QDir(m_rightDir->text().trimmed()).filePath(e.relPath) : QString();
    emit openFileCompareRequested(leftPath, rightPath, forceIni);
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
    rebuildTable();
}

} // namespace tw
