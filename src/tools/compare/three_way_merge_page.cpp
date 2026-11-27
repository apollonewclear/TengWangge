#include "tools/compare/three_way_merge_page.h"

#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>

namespace tw {
namespace {

QPlainTextEdit *makeMonoEditor(bool readOnly, const QString &accessibleName)
{
    auto *ed = new QPlainTextEdit;
    ed->setReadOnly(readOnly);
    ed->setLineWrapMode(QPlainTextEdit::NoWrap);
    ed->setAccessibleName(accessibleName);
    QFont f = ed->font();
    f.setFamily(QStringLiteral("DejaVu Sans Mono"));
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(12);
    ed->setFont(f);
    return ed;
}

QTableWidgetItem *makeCell(const QString &text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(static_cast<int>(align));
    return item;
}

QString resolutionLabel(const cmp::MergeChunk &c)
{
    switch (c.resolution) {
    case cmp::MergeResolution::UseLeft: return QStringLiteral("采纳左侧");
    case cmp::MergeResolution::UseRight: return QStringLiteral("采纳右侧");
    case cmp::MergeResolution::UseBase: return QStringLiteral("采纳基线");
    case cmp::MergeResolution::UseBothLR: return QStringLiteral("左+右合并");
    case cmp::MergeResolution::UseBothRL: return QStringLiteral("右+左合并");
    case cmp::MergeResolution::Auto:
        if (c.kind == cmp::MergeChunkKind::Conflict)
            return QStringLiteral("待解决");
        return QStringLiteral("自动合并");
    }
    return QStringLiteral("自动");
}

} // namespace

ThreeWayMergePage::ThreeWayMergePage(const ToolServices &services, QWidget *parent)
    : QWidget(parent)
    , m_services(services)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(5);

    // ---- 四路径紧凑网格 ----
    auto *pathGrid = new QGridLayout;
    pathGrid->setContentsMargins(0, 0, 0, 0);
    pathGrid->setHorizontalSpacing(6);
    pathGrid->setVerticalSpacing(4);

    m_basePath = new QLineEdit;
    m_basePath->setPlaceholderText(QStringLiteral("共同基线文件 (Base)"));
    QPushButton *browseBase = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));

    m_leftPath = new QLineEdit;
    m_leftPath->setPlaceholderText(QStringLiteral("左侧修改文件 (Left)"));
    QPushButton *browseLeft = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));

    m_rightPath = new QLineEdit;
    m_rightPath->setPlaceholderText(QStringLiteral("右侧修改文件 (Right)"));
    QPushButton *browseRight = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));

    m_outputPath = new QLineEdit;
    m_outputPath->setPlaceholderText(QStringLiteral("合并输出目标路径 (Output)"));
    QPushButton *browseOut = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));

    QPushButton *mergeBtn = ui::makeActionButton(QStringLiteral("执行三方合并"), QStringLiteral("merge"));
    QPushButton *saveOutBtn = ui::makeActionButton(QStringLiteral("保存输出"), QStringLiteral("save"));

    pathGrid->addWidget(ui::makeLabel(QStringLiteral("基线:"), "fieldLabel"), 0, 0);
    pathGrid->addWidget(m_basePath, 0, 1);
    pathGrid->addWidget(browseBase, 0, 2);
    pathGrid->addWidget(ui::makeLabel(QStringLiteral("左侧:"), "fieldLabel"), 0, 3);
    pathGrid->addWidget(m_leftPath, 0, 4);
    pathGrid->addWidget(browseLeft, 0, 5);
    pathGrid->addWidget(mergeBtn, 0, 6);

    pathGrid->addWidget(ui::makeLabel(QStringLiteral("右侧:"), "fieldLabel"), 1, 0);
    pathGrid->addWidget(m_rightPath, 1, 1);
    pathGrid->addWidget(browseRight, 1, 2);
    pathGrid->addWidget(ui::makeLabel(QStringLiteral("输出:"), "fieldLabel"), 1, 3);
    pathGrid->addWidget(m_outputPath, 1, 4);
    pathGrid->addWidget(browseOut, 1, 5);
    pathGrid->addWidget(saveOutBtn, 1, 6);

    pathGrid->setColumnStretch(1, 1);
    pathGrid->setColumnStretch(4, 1);
    root->addLayout(pathGrid);

    // ---- 冲突解决工具栏 ----
    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(6);

    QPushButton *prevBtn = ui::makeActionButton(QStringLiteral("上一冲突"), QStringLiteral("update"));
    QPushButton *nextBtn = ui::makeActionButton(QStringLiteral("下一冲突"), QStringLiteral("compare"));
    m_conflictBadge = ui::makeLabel(QStringLiteral("无冲突"), "pill");
    m_conflictBadge->setWordWrap(false);

    QPushButton *useLeftBtn = ui::makeActionButton(QStringLiteral("采纳左侧"), QStringLiteral("copy_left"));
    QPushButton *useRightBtn = ui::makeActionButton(QStringLiteral("采纳右侧"), QStringLiteral("copy_right"));
    QPushButton *useBaseBtn = new QPushButton(QStringLiteral("采纳基线"));
    QPushButton *useBothBtn = new QPushButton(QStringLiteral("双侧保留"));
    QPushButton *allLeftBtn = new QPushButton(QStringLiteral("全部用左"));
    QPushButton *allRightBtn = new QPushButton(QStringLiteral("全部用右"));

    m_autoBackup = new QCheckBox(QStringLiteral("保存前 .bak"));
    m_autoBackup->setChecked(true);
    QPushButton *sampleBtn = ui::makeActionButton(QStringLiteral("示例"), QStringLiteral("add"));

    actionRow->addWidget(prevBtn);
    actionRow->addWidget(nextBtn);
    actionRow->addWidget(m_conflictBadge);
    actionRow->addSpacing(4);
    actionRow->addWidget(useLeftBtn);
    actionRow->addWidget(useRightBtn);
    actionRow->addWidget(useBaseBtn);
    actionRow->addWidget(useBothBtn);
    actionRow->addWidget(allLeftBtn);
    actionRow->addWidget(allRightBtn);
    actionRow->addWidget(m_autoBackup);
    actionRow->addStretch(1);
    actionRow->addWidget(sampleBtn);
    root->addLayout(actionRow);

    // ---- 上下分栏：上方 3 栏源视图，下方 块清单 + 可编辑输出 ----
    QSplitter *vSplit = ui::makeSplitter(Qt::Vertical, this);

    QSplitter *topSplit = ui::makeSplitter(Qt::Horizontal, vSplit);
    auto wrapEditor = [](const QString &title, QPlainTextEdit *ed) {
        auto *w = new QWidget;
        auto *l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(2);
        l->addWidget(ui::makeLabel(title, "paneTitle"));
        l->addWidget(ed, 1);
        return w;
    };

    m_leftView = makeMonoEditor(false, QStringLiteral("三方合并左侧内容"));
    m_baseView = makeMonoEditor(false, QStringLiteral("三方合并基线内容"));
    m_rightView = makeMonoEditor(false, QStringLiteral("三方合并右侧内容"));
    topSplit->addWidget(wrapEditor(QStringLiteral("左侧 (Left)"), m_leftView));
    topSplit->addWidget(wrapEditor(QStringLiteral("基线 (Base)"), m_baseView));
    topSplit->addWidget(wrapEditor(QStringLiteral("右侧 (Right)"), m_rightView));
    topSplit->setStretchFactor(0, 1);
    topSplit->setStretchFactor(1, 1);
    topSplit->setStretchFactor(2, 1);

    QSplitter *bottomSplit = ui::makeSplitter(Qt::Horizontal, vSplit);

    auto *chunkBox = new QWidget;
    auto *chunkLayout = new QVBoxLayout(chunkBox);
    chunkLayout->setContentsMargins(0, 0, 4, 0);
    chunkLayout->setSpacing(2);
    chunkLayout->addWidget(ui::makeLabel(QStringLiteral("变更与冲突块"), "paneTitle"));
    m_chunkTable = new QTableWidget;
    m_chunkTable->setAccessibleName(QStringLiteral("三方合并冲突块列表"));
    ui::configureDataTable(m_chunkTable, {QStringLiteral("#"), QStringLiteral("类型"), QStringLiteral("决议")});
    m_chunkTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_chunkTable->setColumnWidth(0, 38);
    m_chunkTable->setColumnWidth(1, 86);
    m_chunkTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    chunkLayout->addWidget(m_chunkTable, 1);
    chunkBox->setMinimumWidth(205);
    chunkBox->setMaximumWidth(280);

    m_outputEdit = makeMonoEditor(false, QStringLiteral("三方合并输出编辑区"));
    bottomSplit->addWidget(chunkBox);
    bottomSplit->addWidget(wrapEditor(QStringLiteral("合并输出 (Output — 支持直接手动编辑)"), m_outputEdit));
    bottomSplit->setStretchFactor(0, 0);
    bottomSplit->setStretchFactor(1, 1);
    bottomSplit->setSizes({230, 880});

    vSplit->addWidget(topSplit);
    vSplit->addWidget(bottomSplit);
    vSplit->setStretchFactor(0, 1);
    vSplit->setStretchFactor(1, 1);
    vSplit->setSizes({240, 260});
    root->addWidget(vSplit, 1);

    // ---- 信号连接 ----
    connect(browseBase, &QPushButton::clicked, this, [this]() {
        browseFile(m_basePath, QStringLiteral("选择基线文件 (Base)"));
    });
    connect(browseLeft, &QPushButton::clicked, this, [this]() {
        browseFile(m_leftPath, QStringLiteral("选择左侧文件 (Left)"));
    });
    connect(browseRight, &QPushButton::clicked, this, [this]() {
        browseFile(m_rightPath, QStringLiteral("选择右侧文件 (Right)"));
    });
    connect(browseOut, &QPushButton::clicked, this, [this]() {
        browseFile(m_outputPath, QStringLiteral("选择输出保存路径"), true);
    });
    connect(mergeBtn, &QPushButton::clicked, this, [this]() {
        if (!m_basePath->text().trimmed().isEmpty()
            && !m_leftPath->text().trimmed().isEmpty()
            && !m_rightPath->text().trimmed().isEmpty()
            && m_baseView->toPlainText().isEmpty()
            && m_leftView->toPlainText().isEmpty()) {
            openFiles(m_basePath->text().trimmed(), m_leftPath->text().trimmed(),
                      m_rightPath->text().trimmed(), m_outputPath->text().trimmed());
        } else {
            runMerge();
        }
    });
    connect(saveOutBtn, &QPushButton::clicked, this, [this]() { saveOutput(); });

    connect(prevBtn, &QPushButton::clicked, this, &ThreeWayMergePage::prevConflict);
    connect(nextBtn, &QPushButton::clicked, this, &ThreeWayMergePage::nextConflict);
    connect(useLeftBtn, &QPushButton::clicked, this, [this]() {
        resolveCurrentChunk(cmp::MergeResolution::UseLeft);
    });
    connect(useRightBtn, &QPushButton::clicked, this, [this]() {
        resolveCurrentChunk(cmp::MergeResolution::UseRight);
    });
    connect(useBaseBtn, &QPushButton::clicked, this, [this]() {
        resolveCurrentChunk(cmp::MergeResolution::UseBase);
    });
    connect(useBothBtn, &QPushButton::clicked, this, [this]() {
        resolveCurrentChunk(cmp::MergeResolution::UseBothLR);
    });
    connect(allLeftBtn, &QPushButton::clicked, this, [this]() {
        resolveAllConflicts(cmp::MergeResolution::UseLeft);
    });
    connect(allRightBtn, &QPushButton::clicked, this, [this]() {
        resolveAllConflicts(cmp::MergeResolution::UseRight);
    });
    connect(sampleBtn, &QPushButton::clicked, this, &ThreeWayMergePage::loadSample);

    connect(m_chunkTable, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row >= 0 && m_chunkTable->item(row, 0))
            highlightChunkInSources(m_chunkTable->item(row, 0)->data(Qt::UserRole).toInt());
    });
    connect(m_outputEdit, &QPlainTextEdit::textChanged, this, [this]() {
        if (!m_suppressOutputSignal)
            m_outputManuallyEdited = true;
    });

    auto bind = [this](const QKeySequence &seq, const std::function<void()> &fn) {
        auto *sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, fn);
    };
    bind(QKeySequence(Qt::Key_F7), [this]() { prevConflict(); });
    bind(QKeySequence(Qt::Key_F8), [this]() { nextConflict(); });
    bind(QKeySequence(Qt::CTRL + Qt::Key_S), [this]() { saveOutput(); });
}

bool ThreeWayMergePage::openFiles(const QString &basePath, const QString &leftPath,
                                  const QString &rightPath, const QString &outputPath)
{
    QString b, l, r, err;
    if (!cmp::loadTextFile(basePath, &b, &err)
        || !cmp::loadTextFile(leftPath, &l, &err)
        || !cmp::loadTextFile(rightPath, &r, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_basePath->setText(basePath);
    m_leftPath->setText(leftPath);
    m_rightPath->setText(rightPath);
    if (!outputPath.isEmpty())
        m_outputPath->setText(outputPath);
    setInputs(b, l, r);
    return true;
}

void ThreeWayMergePage::setInputs(const QString &baseText, const QString &leftText,
                                  const QString &rightText)
{
    m_baseView->setPlainText(baseText);
    m_leftView->setPlainText(leftText);
    m_rightView->setPlainText(rightText);
    runMerge();
}

void ThreeWayMergePage::loadSample()
{
    const QString base = QStringLiteral(
        "[Global]\n"
        "sync_interval = 5\n"
        "timeout_ms = 3000\n"
        "enable_check = 1\n"
        "log_level = INFO\n");
    const QString left = QStringLiteral(
        "[Global]\n"
        "sync_interval = 10\n"
        "timeout_ms = 5000\n"
        "enable_check = 1\n"
        "log_level = INFO\n");
    const QString right = QStringLiteral(
        "[Global]\n"
        "sync_interval = 15\n"
        "timeout_ms = 3000\n"
        "enable_check = 1\n"
        "log_level = WARN\n");
    setInputs(base, left, right);
    m_services.showStatus(StatusState::Info, QStringLiteral("已载入三方合并示例（含自动合并与冲突块）"));
}

void ThreeWayMergePage::runMerge()
{
    m_result = cmp::mergeThreeWay(cmp::splitLines(m_baseView->toPlainText()),
                                  cmp::splitLines(m_leftView->toPlainText()),
                                  cmp::splitLines(m_rightView->toPlainText()));
    m_outputManuallyEdited = false;
    rebuildChunkTable();
    refreshOutputEditor();
    updateConflictBadge();

    m_services.logInfo(QStringLiteral("三方合并完成：自动合并 %1 处，冲突 %2 处")
                           .arg(m_result.autoMergedCount)
                           .arg(m_result.conflictCount));
}

void ThreeWayMergePage::rebuildChunkTable()
{
    const Theme &theme = Theme::workbench();
    m_chunkTable->setRowCount(0);
    int firstConflictRow = -1;

    for (const cmp::MergeChunk &c : m_result.chunks) {
        if (c.kind == cmp::MergeChunkKind::Equal)
            continue;
        const int row = m_chunkTable->rowCount();
        m_chunkTable->insertRow(row);
        m_chunkTable->setRowHeight(row, 25);

        QTableWidgetItem *idItem = makeCell(QString::number(row + 1), Qt::AlignCenter);
        idItem->setData(Qt::UserRole, c.index);
        m_chunkTable->setItem(row, 0, idItem);

        QString kindText;
        QColor kindCol = theme.text;
        switch (c.kind) {
        case cmp::MergeChunkKind::LeftChanged:
            kindText = QStringLiteral("左侧修改");
            kindCol = theme.primary;
            break;
        case cmp::MergeChunkKind::RightChanged:
            kindText = QStringLiteral("右侧修改");
            kindCol = theme.accent;
            break;
        case cmp::MergeChunkKind::BothSame:
            kindText = QStringLiteral("双侧一致");
            kindCol = theme.statusSuccess;
            break;
        case cmp::MergeChunkKind::Conflict:
            kindText = QStringLiteral("冲突");
            kindCol = theme.danger;
            if (firstConflictRow < 0)
                firstConflictRow = row;
            break;
        case cmp::MergeChunkKind::Equal:
            break;
        }
        QTableWidgetItem *kindItem = makeCell(kindText, Qt::AlignCenter);
        kindItem->setForeground(QBrush(kindCol));
        m_chunkTable->setItem(row, 1, kindItem);

        QTableWidgetItem *resItem = makeCell(resolutionLabel(c), Qt::AlignCenter);
        resItem->setForeground(QBrush(c.isConflictUnresolved() ? theme.danger : theme.statusSuccess));
        m_chunkTable->setItem(row, 2, resItem);
    }

    if (firstConflictRow >= 0)
        m_chunkTable->selectRow(firstConflictRow);
    else if (m_chunkTable->rowCount() > 0)
        m_chunkTable->selectRow(0);
}

void ThreeWayMergePage::refreshOutputEditor()
{
    m_suppressOutputSignal = true;
    m_outputEdit->setPlainText(m_result.buildMergedText());
    m_suppressOutputSignal = false;
}

void ThreeWayMergePage::updateConflictBadge()
{
    const int unresolved = m_result.unresolvedConflictCount();
    if (unresolved > 0) {
        m_conflictBadge->setText(QStringLiteral("未解决冲突: %1").arg(unresolved));
        ui::setRole(m_conflictBadge, "pillDanger");
        m_services.showStatus(StatusState::Error, QStringLiteral("三方合并存在 %1 处待解决冲突").arg(unresolved));
    } else {
        m_conflictBadge->setText(QStringLiteral("全部已解决 (%1 变更)").arg(m_result.autoMergedCount + m_result.conflictCount));
        ui::setRole(m_conflictBadge, "pill");
        m_services.showStatus(StatusState::Success, QStringLiteral("三方合并无未解决冲突，可直接保存输出"));
    }
}

void ThreeWayMergePage::highlightChunkInSources(int chunkIndex)
{
    if (chunkIndex < 0 || chunkIndex >= m_result.chunks.size())
        return;
    const cmp::MergeChunk &c = m_result.chunks.at(chunkIndex);

    auto mark = [](QPlainTextEdit *ed, int startLine1, int count, const QColor &bg) {
        QList<QTextEdit::ExtraSelection> sels;
        const int lines = qMax(1, count);
        for (int i = 0; i < lines; ++i) {
            const QTextBlock blk = ed->document()->findBlockByNumber(qMax(0, startLine1 - 1 + i));
            if (!blk.isValid())
                break;
            QTextEdit::ExtraSelection s;
            s.format.setBackground(bg);
            s.format.setProperty(QTextFormat::FullWidthSelection, true);
            s.cursor = QTextCursor(blk);
            sels.append(s);
        }
        ed->setExtraSelections(sels);
        const QTextBlock anchor = ed->document()->findBlockByNumber(qMax(0, startLine1 - 1));
        if (anchor.isValid()) {
            ed->setTextCursor(QTextCursor(anchor));
            ed->centerCursor();
        }
    };

    const QColor conflictBg(0xfd, 0xd8, 0xd8);
    const QColor leftBg(0xdc, 0xec, 0xfd);
    const QColor rightBg(0xff, 0xe8, 0xc2);
    const QColor baseBg(0xee, 0xf1, 0xf5);

    const bool isConflict = (c.kind == cmp::MergeChunkKind::Conflict);
    mark(m_leftView, c.leftStartLine, c.leftLines.size(), isConflict ? conflictBg : leftBg);
    mark(m_baseView, c.baseStartLine, c.baseLines.size(), baseBg);
    mark(m_rightView, c.rightStartLine, c.rightLines.size(), isConflict ? conflictBg : rightBg);
}

int ThreeWayMergePage::selectedChunkIndex() const
{
    const int row = m_chunkTable->currentRow();
    if (row < 0 || !m_chunkTable->item(row, 0))
        return -1;
    return m_chunkTable->item(row, 0)->data(Qt::UserRole).toInt();
}

void ThreeWayMergePage::jumpToConflict(int chunkIndex)
{
    for (int r = 0; r < m_chunkTable->rowCount(); ++r) {
        if (m_chunkTable->item(r, 0)->data(Qt::UserRole).toInt() == chunkIndex) {
            m_chunkTable->selectRow(r);
            return;
        }
    }
}

void ThreeWayMergePage::prevConflict()
{
    const int curRow = m_chunkTable->currentRow();
    const int n = m_chunkTable->rowCount();
    if (n == 0)
        return;
    for (int step = 1; step <= n; ++step) {
        const int r = (curRow - step + n) % n;
        const int idx = m_chunkTable->item(r, 0)->data(Qt::UserRole).toInt();
        if (m_result.chunks.at(idx).kind == cmp::MergeChunkKind::Conflict) {
            m_chunkTable->selectRow(r);
            return;
        }
    }
}

void ThreeWayMergePage::nextConflict()
{
    const int curRow = m_chunkTable->currentRow();
    const int n = m_chunkTable->rowCount();
    if (n == 0)
        return;
    for (int step = 1; step <= n; ++step) {
        const int r = (curRow + step) % n;
        const int idx = m_chunkTable->item(r, 0)->data(Qt::UserRole).toInt();
        if (m_result.chunks.at(idx).kind == cmp::MergeChunkKind::Conflict) {
            m_chunkTable->selectRow(r);
            return;
        }
    }
}

bool ThreeWayMergePage::resolveCurrentChunk(cmp::MergeResolution resolution)
{
    const int idx = selectedChunkIndex();
    if (idx < 0 || idx >= m_result.chunks.size())
        return false;
    m_result.chunks[idx].resolution = resolution;
    const int curRow = m_chunkTable->currentRow();
    rebuildChunkTable();
    if (curRow >= 0 && curRow < m_chunkTable->rowCount())
        m_chunkTable->selectRow(curRow);
    refreshOutputEditor();
    updateConflictBadge();
    return true;
}

void ThreeWayMergePage::resolveAllConflicts(cmp::MergeResolution resolution)
{
    for (cmp::MergeChunk &c : m_result.chunks) {
        if (c.kind == cmp::MergeChunkKind::Conflict)
            c.resolution = resolution;
    }
    rebuildChunkTable();
    refreshOutputEditor();
    updateConflictBadge();
    m_services.logInfo(QStringLiteral("已批量解决全部冲突块"));
}

QString ThreeWayMergePage::mergedText() const
{
    return m_outputEdit->toPlainText();
}

bool ThreeWayMergePage::saveOutput(QString *error)
{
    QString path = m_outputPath->text().trimmed();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, QStringLiteral("保存三方合并结果"));
        if (path.isEmpty())
            return false;
        m_outputPath->setText(path);
    }
    QString bak;
    QString err;
    if (!cmp::saveTextFileWithBackup(path, m_outputEdit->toPlainText(), m_autoBackup->isChecked(),
                                     &bak, &err)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    const QString msg = bak.isEmpty()
        ? QStringLiteral("已保存三方合并结果：%1").arg(path)
        : QStringLiteral("已保存三方合并结果：%1（自动备份：%2）").arg(path, bak);
    m_services.logSuccess(msg);
    m_services.showStatus(StatusState::Success, msg);
    return true;
}

void ThreeWayMergePage::browseFile(QLineEdit *target, const QString &title, bool saveMode)
{
    const QString path = saveMode
        ? QFileDialog::getSaveFileName(this, title, target->text())
        : QFileDialog::getOpenFileName(this, title, target->text());
    if (path.isEmpty())
        return;
    target->setText(path);
    if (!saveMode) {
        QString content;
        QString err;
        if (!cmp::loadTextFile(path, &content, &err)) {
            m_services.logError(err);
            m_services.showStatus(StatusState::Error, err);
            return;
        }
        if (target == m_basePath)
            m_baseView->setPlainText(content);
        else if (target == m_leftPath)
            m_leftView->setPlainText(content);
        else if (target == m_rightPath)
            m_rightView->setPlainText(content);
    }
}

} // namespace tw
