#include "tools/compare/text_compare_page.h"

#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

namespace tw {

// ============================================================================
// 带行号边栏的等宽代码编辑器
// ============================================================================

class DiffCodeEditor final : public QPlainTextEdit
{
public:
    explicit DiffCodeEditor(QWidget *parent = nullptr)
        : QPlainTextEdit(parent)
        , m_gutter(new Gutter(this))
    {
        setLineWrapMode(QPlainTextEdit::NoWrap);
        QFont f = font();
        f.setFamily(QStringLiteral("DejaVu Sans Mono"));
        f.setStyleHint(QFont::Monospace);
        f.setPixelSize(12);
        setFont(f);

        connect(this, &QPlainTextEdit::blockCountChanged, this, [this](int) { updateGutterWidth(); });
        connect(this, &QPlainTextEdit::updateRequest, this, &DiffCodeEditor::updateGutter);
        updateGutterWidth();
    }

    int gutterWidth() const
    {
        int digits = 2;
        int maxVal = qMax(1, blockCount());
        while (maxVal >= 100) {
            maxVal /= 10;
            ++digits;
        }
        return 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    }

    void scrollToLine(int lineNo1Based)
    {
        if (lineNo1Based < 1)
            return;
        const QTextBlock blk = document()->findBlockByNumber(lineNo1Based - 1);
        if (!blk.isValid())
            return;
        QTextCursor cur(blk);
        setTextCursor(cur);
        centerCursor();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QPlainTextEdit::resizeEvent(event);
        const QRect cr = contentsRect();
        m_gutter->setGeometry(QRect(cr.left(), cr.top(), gutterWidth(), cr.height()));
    }

private:
    class Gutter final : public QWidget
    {
    public:
        explicit Gutter(DiffCodeEditor *editor)
            : QWidget(editor)
            , m_editor(editor)
        {
        }

        QSize sizeHint() const override { return QSize(m_editor->gutterWidth(), 0); }

    protected:
        void paintEvent(QPaintEvent *event) override { m_editor->paintGutter(event); }

    private:
        DiffCodeEditor *m_editor;
    };

    void updateGutterWidth() { setViewportMargins(gutterWidth(), 0, 0, 0); }

    void updateGutter(const QRect &rect, int dy)
    {
        if (dy != 0)
            m_gutter->scroll(0, dy);
        else
            m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());
        if (rect.contains(viewport()->rect()))
            updateGutterWidth();
    }

    void paintGutter(QPaintEvent *event)
    {
        const Theme &theme = Theme::workbench();
        QPainter p(m_gutter);
        p.fillRect(event->rect(), theme.panelAlt);
        p.setPen(theme.borderSoft);
        p.drawLine(m_gutter->width() - 1, event->rect().top(),
                   m_gutter->width() - 1, event->rect().bottom());

        QTextBlock block = firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + qRound(blockBoundingRect(block).height());

        p.setPen(theme.textMuted);
        p.setFont(font());
        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                p.drawText(0, top, m_gutter->width() - 5, fontMetrics().height(),
                           Qt::AlignRight | Qt::AlignVCenter, QString::number(blockNumber + 1));
            }
            block = block.next();
            top = bottom;
            bottom = top + qRound(blockBoundingRect(block).height());
            ++blockNumber;
        }
    }

    Gutter *m_gutter = nullptr;
};

// ============================================================================
// 中央差异缩略条（参考 Beyond Compare 概览柱）
// ============================================================================

class DiffOverviewBar final : public QWidget
{
public:
    explicit DiffOverviewBar(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedWidth(14);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("点击跳转到对应差异位置"));
    }

    void setDiff(const cmp::TextDiffResult *diff, int activeHunk)
    {
        m_diff = diff;
        m_activeHunk = activeHunk;
        update();
    }

    std::function<void(int rowIdx)> onRowClicked;

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Theme &theme = Theme::workbench();
        QPainter p(this);
        p.fillRect(rect(), theme.panelAlt);
        p.setPen(theme.border);
        p.drawRect(rect().adjusted(0, 0, -1, -1));

        if (!m_diff || m_diff->rows.isEmpty())
            return;

        const int total = m_diff->rows.size();
        const qreal h = qMax(1, height() - 4);
        for (int i = 0; i < total; ++i) {
            const cmp::DiffRow &row = m_diff->rows.at(i);
            if (row.kind == cmp::DiffRowKind::Equal)
                continue;
            QColor col;
            if (row.kind == cmp::DiffRowKind::Changed)
                col = QColor(0xe0, 0x48, 0x48);
            else if (row.kind == cmp::DiffRowKind::LeftOnly)
                col = QColor(0x25, 0x78, 0xb5);
            else
                col = QColor(0xee, 0x78, 0x1f);

            const int y = 2 + qRound(h * i / total);
            const int barH = qMax(3, qRound(h / total));
            p.fillRect(QRect(2, y, width() - 4, barH), col);
        }

        if (m_activeHunk >= 0 && m_activeHunk < m_diff->hunks.size()) {
            const cmp::DiffHunk &hk = m_diff->hunks.at(m_activeHunk);
            const int y = 2 + qRound(h * hk.startRow / total);
            const int barH = qMax(5, qRound(h * (hk.endRow - hk.startRow + 1) / total));
            p.setPen(QPen(theme.text, 1));
            p.drawRect(QRect(1, y, width() - 3, barH));
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (!m_diff || m_diff->rows.isEmpty() || !onRowClicked)
            return;
        const int idx = qBound(0, event->pos().y() * m_diff->rows.size() / qMax(1, height()),
                               m_diff->rows.size() - 1);
        onRowClicked(idx);
    }

private:
    const cmp::TextDiffResult *m_diff = nullptr;
    int m_activeHunk = -1;
};

namespace {

QTableWidgetItem *makeTableItem(const QString &text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(static_cast<int>(align));
    return item;
}

} // namespace

// ============================================================================
// TextComparePage
// ============================================================================

TextComparePage::TextComparePage(const ToolServices &services, QWidget *parent)
    : QWidget(parent)
    , m_services(services)
    , m_recompareTimer(new QTimer(this))
{
    m_recompareTimer->setSingleShot(true);
    m_recompareTimer->setInterval(260);
    connect(m_recompareTimer, &QTimer::timeout, this, &TextComparePage::recompare);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(5);

    // ---- 路径栏 ----
    auto *pathRow = new QHBoxLayout;
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(6);

    m_leftPath = new QLineEdit;
    m_leftPath->setPlaceholderText(QStringLiteral("左侧本地文件路径（也可直接编辑内容）"));
    m_leftPath->setAccessibleName(QStringLiteral("左侧文件路径"));
    QPushButton *browseL = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));
    m_saveLeftBtn = ui::makeActionButton(QStringLiteral("保存左"), QStringLiteral("save"));

    QPushButton *swapBtn = ui::makeActionButton(QStringLiteral("交换"), QStringLiteral("swap"));

    m_rightPath = new QLineEdit;
    m_rightPath->setPlaceholderText(QStringLiteral("右侧本地文件路径（也可直接编辑内容）"));
    m_rightPath->setAccessibleName(QStringLiteral("右侧文件路径"));
    QPushButton *browseR = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));
    m_saveRightBtn = ui::makeActionButton(QStringLiteral("保存右"), QStringLiteral("save"));

    QPushButton *compareBtn = ui::makeActionButton(QStringLiteral("比较"), QStringLiteral("refresh"));

    pathRow->addWidget(ui::makeLabel(QStringLiteral("左:"), "fieldLabel"));
    pathRow->addWidget(m_leftPath, 1);
    pathRow->addWidget(browseL);
    pathRow->addWidget(m_saveLeftBtn);
    pathRow->addWidget(swapBtn);
    pathRow->addWidget(ui::makeLabel(QStringLiteral("右:"), "fieldLabel"));
    pathRow->addWidget(m_rightPath, 1);
    pathRow->addWidget(browseR);
    pathRow->addWidget(m_saveRightBtn);
    pathRow->addWidget(compareBtn);
    root->addLayout(pathRow);

    // ---- 操作工具栏 ----
    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(6);

    m_modeAllBtn = new QPushButton(QStringLiteral("全部 (并排编辑)"));
    m_modeAllBtn->setCheckable(true);
    m_modeAllBtn->setChecked(true);
    m_modeDiffBtn = new QPushButton(QStringLiteral("仅差异"));
    m_modeDiffBtn->setCheckable(true);

    m_prevBtn = ui::makeActionButton(QStringLiteral("上一处"), QStringLiteral("update"));
    m_nextBtn = ui::makeActionButton(QStringLiteral("下一处"), QStringLiteral("compare"));
    m_hunkLabel = ui::makeLabel(QStringLiteral("0 / 0 差异块"), "code");
    m_hunkLabel->setWordWrap(false);

    m_copyRightBtn = ui::makeActionButton(QStringLiteral("当前块 → 右"), QStringLiteral("copy_right"));
    m_copyLeftBtn = ui::makeActionButton(QStringLiteral("当前块 ← 左"), QStringLiteral("copy_left"));

    m_ignoreWs = new QCheckBox(QStringLiteral("忽略空白"));
    m_ignoreCase = new QCheckBox(QStringLiteral("忽略大小写"));
    m_autoBackup = new QCheckBox(QStringLiteral("保存前 .bak"));
    m_autoBackup->setChecked(true);

    QPushButton *sampleBtn = ui::makeActionButton(QStringLiteral("示例"), QStringLiteral("add"));
    QPushButton *exportBtn = ui::makeActionButton(QStringLiteral("导出"), QStringLiteral("export"));

    actionRow->addWidget(m_modeAllBtn);
    actionRow->addWidget(m_modeDiffBtn);
    actionRow->addSpacing(4);
    actionRow->addWidget(m_prevBtn);
    actionRow->addWidget(m_nextBtn);
    actionRow->addWidget(m_hunkLabel);
    actionRow->addSpacing(4);
    actionRow->addWidget(m_copyRightBtn);
    actionRow->addWidget(m_copyLeftBtn);
    actionRow->addSpacing(4);
    actionRow->addWidget(m_ignoreWs);
    actionRow->addWidget(m_ignoreCase);
    actionRow->addWidget(m_autoBackup);
    actionRow->addStretch(1);
    actionRow->addWidget(sampleBtn);
    actionRow->addWidget(exportBtn);
    root->addLayout(actionRow);

    // ---- 视图栈：0 = 并排编辑视图；1 = 仅差异行表视图 ----
    m_viewStack = new QStackedWidget;

    auto *sideBySideWidget = new QWidget;
    auto *sideLayout = new QHBoxLayout(sideBySideWidget);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(4);

    QSplitter *editorSplit = ui::makeSplitter(Qt::Horizontal, sideBySideWidget);

    auto *leftBox = new QWidget;
    auto *leftBoxLayout = new QVBoxLayout(leftBox);
    leftBoxLayout->setContentsMargins(0, 0, 0, 0);
    leftBoxLayout->setSpacing(2);
    m_leftTitle = ui::makeLabel(QStringLiteral("左侧文档"), "paneTitle");
    m_leftEditor = new DiffCodeEditor;
    m_leftEditor->setAccessibleName(QStringLiteral("左侧文本编辑区"));
    leftBoxLayout->addWidget(m_leftTitle);
    leftBoxLayout->addWidget(m_leftEditor, 1);

    auto *rightBox = new QWidget;
    auto *rightBoxLayout = new QVBoxLayout(rightBox);
    rightBoxLayout->setContentsMargins(0, 0, 0, 0);
    rightBoxLayout->setSpacing(2);
    m_rightTitle = ui::makeLabel(QStringLiteral("右侧文档"), "paneTitle");
    m_rightEditor = new DiffCodeEditor;
    m_rightEditor->setAccessibleName(QStringLiteral("右侧文本编辑区"));
    rightBoxLayout->addWidget(m_rightTitle);
    rightBoxLayout->addWidget(m_rightEditor, 1);

    editorSplit->addWidget(leftBox);
    editorSplit->addWidget(rightBox);
    editorSplit->setStretchFactor(0, 1);
    editorSplit->setStretchFactor(1, 1);

    m_overviewBar = new DiffOverviewBar;
    m_overviewBar->onRowClicked = [this](int rowIdx) {
        if (rowIdx < 0 || rowIdx >= m_diff.rows.size())
            return;
        const cmp::DiffRow &r = m_diff.rows.at(rowIdx);
        if (r.hunkIndex >= 0) {
            jumpToHunk(r.hunkIndex);
        } else {
            if (r.leftLineNo > 0)
                m_leftEditor->scrollToLine(r.leftLineNo);
            if (r.rightLineNo > 0)
                m_rightEditor->scrollToLine(r.rightLineNo);
        }
    };

    sideLayout->addWidget(editorSplit, 1);
    sideLayout->addWidget(m_overviewBar);

    m_diffTable = new QTableWidget;
    m_diffTable->setAccessibleName(QStringLiteral("文本仅差异列表"));
    ui::configureDataTable(m_diffTable,
                           {QStringLiteral("块"),
                            QStringLiteral("左行"),
                            QStringLiteral("左侧内容"),
                            QStringLiteral("右行"),
                            QStringLiteral("右侧内容"),
                            QStringLiteral("差异类型")});
    m_diffTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_diffTable->setColumnWidth(0, 44);
    m_diffTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_diffTable->setColumnWidth(1, 52);
    m_diffTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_diffTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_diffTable->setColumnWidth(3, 52);
    m_diffTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_diffTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_diffTable->setColumnWidth(5, 76);

    m_viewStack->addWidget(sideBySideWidget);
    m_viewStack->addWidget(m_diffTable);
    root->addWidget(m_viewStack, 1);

    // ---- 信号连接 ----
    connect(browseL, &QPushButton::clicked, this, &TextComparePage::browseLeft);
    connect(browseR, &QPushButton::clicked, this, &TextComparePage::browseRight);
    connect(m_saveLeftBtn, &QPushButton::clicked, this, [this]() { saveLeft(); });
    connect(m_saveRightBtn, &QPushButton::clicked, this, [this]() { saveRight(); });
    connect(swapBtn, &QPushButton::clicked, this, &TextComparePage::swapSides);
    connect(compareBtn, &QPushButton::clicked, this, [this]() {
        if (!m_leftPath->text().trimmed().isEmpty() && !m_rightPath->text().trimmed().isEmpty()
            && !m_leftDirty && !m_rightDirty) {
            openFiles(m_leftPath->text().trimmed(), m_rightPath->text().trimmed());
        } else {
            recompare();
        }
    });

    connect(m_modeAllBtn, &QPushButton::clicked, this, [this]() {
        m_modeAllBtn->setChecked(true);
        m_modeDiffBtn->setChecked(false);
        m_viewStack->setCurrentIndex(0);
    });
    connect(m_modeDiffBtn, &QPushButton::clicked, this, [this]() {
        m_modeAllBtn->setChecked(false);
        m_modeDiffBtn->setChecked(true);
        rebuildDiffOnlyTable();
        m_viewStack->setCurrentIndex(1);
    });

    connect(m_prevBtn, &QPushButton::clicked, this, &TextComparePage::prevHunk);
    connect(m_nextBtn, &QPushButton::clicked, this, &TextComparePage::nextHunk);
    connect(m_copyRightBtn, &QPushButton::clicked, this, [this]() { copyCurrentHunkToRight(); });
    connect(m_copyLeftBtn, &QPushButton::clicked, this, [this]() { copyCurrentHunkToLeft(); });
    connect(m_ignoreWs, &QCheckBox::toggled, this, [this](bool) { recompare(); });
    connect(m_ignoreCase, &QCheckBox::toggled, this, [this](bool) { recompare(); });
    connect(sampleBtn, &QPushButton::clicked, this, &TextComparePage::loadSample);
    connect(exportBtn, &QPushButton::clicked, this, &TextComparePage::exportReport);

    // 同步滚动条
    connect(m_leftEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int v) {
        if (m_syncingScroll)
            return;
        m_syncingScroll = true;
        m_rightEditor->verticalScrollBar()->setValue(v);
        m_syncingScroll = false;
    });
    connect(m_rightEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int v) {
        if (m_syncingScroll)
            return;
        m_syncingScroll = true;
        m_leftEditor->verticalScrollBar()->setValue(v);
        m_syncingScroll = false;
    });
    connect(m_leftEditor->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int v) {
        if (m_syncingScroll)
            return;
        m_syncingScroll = true;
        m_rightEditor->horizontalScrollBar()->setValue(v);
        m_syncingScroll = false;
    });
    connect(m_rightEditor->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int v) {
        if (m_syncingScroll)
            return;
        m_syncingScroll = true;
        m_leftEditor->horizontalScrollBar()->setValue(v);
        m_syncingScroll = false;
    });

    connect(m_leftEditor, &QPlainTextEdit::textChanged, this, [this]() {
        if (m_suppressEditSignal)
            return;
        m_leftDirty = true;
        updateDirtyLabels();
        m_recompareTimer->start();
    });
    connect(m_rightEditor, &QPlainTextEdit::textChanged, this, [this]() {
        if (m_suppressEditSignal)
            return;
        m_rightDirty = true;
        updateDirtyLabels();
        m_recompareTimer->start();
    });

    connect(m_diffTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const QTableWidgetItem *item = m_diffTable->item(row, 0);
        if (!item)
            return;
        const int hunkIdx = item->data(Qt::UserRole).toInt();
        m_modeAllBtn->setChecked(true);
        m_modeDiffBtn->setChecked(false);
        m_viewStack->setCurrentIndex(0);
        jumpToHunk(hunkIdx);
    });

    updateHunkCounter();
    updateDirtyLabels();
}

bool TextComparePage::openFiles(const QString &leftPath, const QString &rightPath)
{
    QString leftContent;
    QString rightContent;
    QString err;

    if (!leftPath.isEmpty() && !cmp::loadTextFile(leftPath, &leftContent, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    if (!rightPath.isEmpty() && !cmp::loadTextFile(rightPath, &rightContent, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }

    m_leftPath->setText(leftPath);
    m_rightPath->setText(rightPath);
    setTexts(leftContent, rightContent, leftPath, rightPath);
    m_services.logInfo(QStringLiteral("文本对比完成：%1 ⇄ %2（差异块 %3 处）")
                           .arg(leftPath, rightPath)
                           .arg(m_diff.hunks.size()));
    return true;
}

void TextComparePage::setTexts(const QString &leftText, const QString &rightText,
                               const QString &leftLabel, const QString &rightLabel)
{
    m_suppressEditSignal = true;
    m_leftEditor->setPlainText(leftText);
    m_rightEditor->setPlainText(rightText);
    m_suppressEditSignal = false;

    m_leftDirty = false;
    m_rightDirty = false;
    if (!leftLabel.isEmpty())
        m_leftPath->setText(leftLabel);
    if (!rightLabel.isEmpty())
        m_rightPath->setText(rightLabel);
    updateDirtyLabels();
    recompare();
}

void TextComparePage::loadSample()
{
    const QString leftSample = QStringLiteral(
        "# 服务器 A 运行参数\n"
        "node_name = nuspV2-wk1\n"
        "scan_interval_ms = 500\n"
        "enable_auto_solidify = true\n"
        "max_workers = 4\n"
        "log_level = INFO\n"
        "backup_retention_days = 7\n");
    const QString rightSample = QStringLiteral(
        "# 服务器 B 运行参数\n"
        "node_name = nuspV2-wk2\n"
        "scan_interval_ms = 500\n"
        "enable_auto_solidify = false\n"
        "max_workers = 8\n"
        "retry_limit = 3\n"
        "log_level = INFO\n"
        "backup_retention_days = 7\n");
    setTexts(leftSample, rightSample, QStringLiteral("sample_left.conf"), QStringLiteral("sample_right.conf"));
    m_services.showStatus(StatusState::Info, QStringLiteral("已载入文本对比示例，可直接编辑或复制差异块"));
}

QString TextComparePage::leftText() const
{
    return m_leftEditor->toPlainText();
}

QString TextComparePage::rightText() const
{
    return m_rightEditor->toPlainText();
}

void TextComparePage::recompare()
{
    cmp::TextDiffOptions opt;
    opt.ignoreWhitespace = m_ignoreWs->isChecked();
    opt.ignoreCase = m_ignoreCase->isChecked();

    m_diff = cmp::diffLines(cmp::splitLines(m_leftEditor->toPlainText()),
                            cmp::splitLines(m_rightEditor->toPlainText()), opt);
    if (m_diff.hunks.isEmpty())
        m_currentHunk = -1;
    else if (m_currentHunk < 0 || m_currentHunk >= m_diff.hunks.size())
        m_currentHunk = 0;

    applyHighlights();
    if (m_viewStack->currentIndex() == 1)
        rebuildDiffOnlyTable();
    updateHunkCounter();

    m_services.showStatus(m_diff.hunks.isEmpty() ? StatusState::Success : StatusState::Info,
                          QStringLiteral("文本比对：相同 %1 行，修改 %2 行，仅左 %3 行，仅右 %4 行（共 %5 处差异块）")
                              .arg(m_diff.equalCount)
                              .arg(m_diff.changedCount)
                              .arg(m_diff.leftOnlyCount)
                              .arg(m_diff.rightOnlyCount)
                              .arg(m_diff.hunks.size()));
}

void TextComparePage::applyHighlights()
{
    QList<QTextEdit::ExtraSelection> leftSel;
    QList<QTextEdit::ExtraSelection> rightSel;

    const QColor changedBg(0xfd, 0xe8, 0xe8);
    const QColor changedActiveBg(0xfb, 0xd1, 0xd1);
    const QColor changedSpanBg(0xf6, 0x9c, 0x9c);
    const QColor leftOnlyBg(0xe6, 0xf2, 0xff);
    const QColor leftOnlyActiveBg(0xcf, 0xe5, 0xfd);
    const QColor rightOnlyBg(0xff, 0xf3, 0xdc);
    const QColor rightOnlyActiveBg(0xfd, 0xe2, 0xb4);

    auto addLineBg = [](QPlainTextEdit *ed, QList<QTextEdit::ExtraSelection> &list,
                        int lineNo1Based, const QColor &bg) {
        if (lineNo1Based < 1)
            return;
        const QTextBlock blk = ed->document()->findBlockByNumber(lineNo1Based - 1);
        if (!blk.isValid())
            return;
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(bg);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = QTextCursor(blk);
        list.append(sel);
    };

    auto addInlineSpans = [](QPlainTextEdit *ed, QList<QTextEdit::ExtraSelection> &list,
                             int lineNo1Based, const QVector<cmp::CharSpan> &spans, const QColor &bg) {
        if (lineNo1Based < 1 || spans.isEmpty())
            return;
        const QTextBlock blk = ed->document()->findBlockByNumber(lineNo1Based - 1);
        if (!blk.isValid())
            return;
        for (const cmp::CharSpan &sp : spans) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(bg);
            QTextCursor cur(blk);
            cur.setPosition(blk.position() + sp.start);
            cur.setPosition(blk.position() + sp.start + sp.length, QTextCursor::KeepAnchor);
            sel.cursor = cur;
            list.append(sel);
        }
    };

    for (const cmp::DiffRow &row : m_diff.rows) {
        if (row.kind == cmp::DiffRowKind::Equal)
            continue;
        const bool active = (row.hunkIndex == m_currentHunk);
        if (row.kind == cmp::DiffRowKind::Changed) {
            addLineBg(m_leftEditor, leftSel, row.leftLineNo, active ? changedActiveBg : changedBg);
            addLineBg(m_rightEditor, rightSel, row.rightLineNo, active ? changedActiveBg : changedBg);
            addInlineSpans(m_leftEditor, leftSel, row.leftLineNo, row.leftSpans, changedSpanBg);
            addInlineSpans(m_rightEditor, rightSel, row.rightLineNo, row.rightSpans, changedSpanBg);
        } else if (row.kind == cmp::DiffRowKind::LeftOnly) {
            addLineBg(m_leftEditor, leftSel, row.leftLineNo, active ? leftOnlyActiveBg : leftOnlyBg);
        } else if (row.kind == cmp::DiffRowKind::RightOnly) {
            addLineBg(m_rightEditor, rightSel, row.rightLineNo, active ? rightOnlyActiveBg : rightOnlyBg);
        }
    }

    m_leftEditor->setExtraSelections(leftSel);
    m_rightEditor->setExtraSelections(rightSel);
    m_overviewBar->setDiff(&m_diff, m_currentHunk);
}

void TextComparePage::rebuildDiffOnlyTable()
{
    const Theme &theme = Theme::workbench();
    m_diffTable->setRowCount(0);
    int r = 0;
    for (const cmp::DiffRow &row : m_diff.rows) {
        if (row.kind == cmp::DiffRowKind::Equal)
            continue;
        m_diffTable->insertRow(r);
        m_diffTable->setRowHeight(r, 25);

        QTableWidgetItem *hunkItem = makeTableItem(QString::number(row.hunkIndex + 1), Qt::AlignCenter);
        hunkItem->setData(Qt::UserRole, row.hunkIndex);
        m_diffTable->setItem(r, 0, hunkItem);
        m_diffTable->setItem(r, 1, makeTableItem(row.leftLineNo > 0 ? QString::number(row.leftLineNo) : QStringLiteral("-"), Qt::AlignCenter));
        m_diffTable->setItem(r, 2, makeTableItem(row.leftText));
        m_diffTable->setItem(r, 3, makeTableItem(row.rightLineNo > 0 ? QString::number(row.rightLineNo) : QStringLiteral("-"), Qt::AlignCenter));
        m_diffTable->setItem(r, 4, makeTableItem(row.rightText));

        QString kindText;
        QColor col = theme.text;
        if (row.kind == cmp::DiffRowKind::Changed) {
            kindText = QStringLiteral("修改");
            col = theme.danger;
        } else if (row.kind == cmp::DiffRowKind::LeftOnly) {
            kindText = QStringLiteral("仅左侧");
            col = theme.primary;
        } else {
            kindText = QStringLiteral("仅右侧");
            col = theme.accent;
        }
        QTableWidgetItem *kindItem = makeTableItem(kindText, Qt::AlignCenter);
        kindItem->setForeground(QBrush(col));
        m_diffTable->setItem(r, 5, kindItem);
        ++r;
    }
}

void TextComparePage::jumpToHunk(int hunkIndex)
{
    if (hunkIndex < 0 || hunkIndex >= m_diff.hunks.size())
        return;
    m_currentHunk = hunkIndex;
    const cmp::DiffHunk &h = m_diff.hunks.at(m_currentHunk);
    m_leftEditor->scrollToLine(qMax(1, h.leftStartLine + 1));
    m_rightEditor->scrollToLine(qMax(1, h.rightStartLine + 1));
    applyHighlights();
    updateHunkCounter();
}

void TextComparePage::prevHunk()
{
    if (m_diff.hunks.isEmpty())
        return;
    const int next = (m_currentHunk <= 0) ? (m_diff.hunks.size() - 1) : (m_currentHunk - 1);
    jumpToHunk(next);
}

void TextComparePage::nextHunk()
{
    if (m_diff.hunks.isEmpty())
        return;
    const int next = (m_currentHunk + 1) % m_diff.hunks.size();
    jumpToHunk(next);
}

bool TextComparePage::copyCurrentHunkToRight()
{
    if (m_currentHunk < 0 || m_currentHunk >= m_diff.hunks.size())
        return false;
    const QStringList leftLines = cmp::splitLines(m_leftEditor->toPlainText());
    const QStringList rightLines = cmp::splitLines(m_rightEditor->toPlainText());
    const QStringList updated = cmp::applyHunkToRight(leftLines, rightLines, m_diff.hunks.at(m_currentHunk));

    m_suppressEditSignal = true;
    m_rightEditor->setPlainText(cmp::joinLines(updated));
    m_suppressEditSignal = false;
    m_rightDirty = true;
    updateDirtyLabels();
    recompare();
    m_services.logInfo(QStringLiteral("已将当前差异块从左侧复制到右侧"));
    return true;
}

bool TextComparePage::copyCurrentHunkToLeft()
{
    if (m_currentHunk < 0 || m_currentHunk >= m_diff.hunks.size())
        return false;
    const QStringList leftLines = cmp::splitLines(m_leftEditor->toPlainText());
    const QStringList rightLines = cmp::splitLines(m_rightEditor->toPlainText());
    const QStringList updated = cmp::applyHunkToLeft(leftLines, rightLines, m_diff.hunks.at(m_currentHunk));

    m_suppressEditSignal = true;
    m_leftEditor->setPlainText(cmp::joinLines(updated));
    m_suppressEditSignal = false;
    m_leftDirty = true;
    updateDirtyLabels();
    recompare();
    m_services.logInfo(QStringLiteral("已将当前差异块从右侧复制到左侧"));
    return true;
}

bool TextComparePage::saveLeft(QString *error)
{
    QString path = m_leftPath->text().trimmed();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, QStringLiteral("保存左侧文件"));
        if (path.isEmpty())
            return false;
        m_leftPath->setText(path);
    }
    QString bakPath;
    QString err;
    if (!cmp::saveTextFileWithBackup(path, m_leftEditor->toPlainText(), m_autoBackup->isChecked(),
                                     &bakPath, &err)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_leftDirty = false;
    updateDirtyLabels();
    const QString msg = bakPath.isEmpty()
        ? QStringLiteral("已保存左侧文件：%1").arg(path)
        : QStringLiteral("已保存左侧文件：%1（自动备份：%2）").arg(path, bakPath);
    m_services.logSuccess(msg);
    m_services.showStatus(StatusState::Success, msg);
    return true;
}

bool TextComparePage::saveRight(QString *error)
{
    QString path = m_rightPath->text().trimmed();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, QStringLiteral("保存右侧文件"));
        if (path.isEmpty())
            return false;
        m_rightPath->setText(path);
    }
    QString bakPath;
    QString err;
    if (!cmp::saveTextFileWithBackup(path, m_rightEditor->toPlainText(), m_autoBackup->isChecked(),
                                     &bakPath, &err)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_rightDirty = false;
    updateDirtyLabels();
    const QString msg = bakPath.isEmpty()
        ? QStringLiteral("已保存右侧文件：%1").arg(path)
        : QStringLiteral("已保存右侧文件：%1（自动备份：%2）").arg(path, bakPath);
    m_services.logSuccess(msg);
    m_services.showStatus(StatusState::Success, msg);
    return true;
}

void TextComparePage::browseLeft()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择左侧文件"),
                                                      m_leftPath->text());
    if (path.isEmpty())
        return;
    QString content;
    QString err;
    if (!cmp::loadTextFile(path, &content, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return;
    }
    m_leftPath->setText(path);
    m_suppressEditSignal = true;
    m_leftEditor->setPlainText(content);
    m_suppressEditSignal = false;
    m_leftDirty = false;
    updateDirtyLabels();
    recompare();
}

void TextComparePage::browseRight()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择右侧文件"),
                                                      m_rightPath->text());
    if (path.isEmpty())
        return;
    QString content;
    QString err;
    if (!cmp::loadTextFile(path, &content, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return;
    }
    m_rightPath->setText(path);
    m_suppressEditSignal = true;
    m_rightEditor->setPlainText(content);
    m_suppressEditSignal = false;
    m_rightDirty = false;
    updateDirtyLabels();
    recompare();
}

void TextComparePage::swapSides()
{
    const QString lp = m_leftPath->text();
    const QString rp = m_rightPath->text();
    const QString lt = m_leftEditor->toPlainText();
    const QString rt = m_rightEditor->toPlainText();
    const bool ld = m_leftDirty;
    const bool rd = m_rightDirty;

    m_leftPath->setText(rp);
    m_rightPath->setText(lp);
    m_suppressEditSignal = true;
    m_leftEditor->setPlainText(rt);
    m_rightEditor->setPlainText(lt);
    m_suppressEditSignal = false;
    m_leftDirty = rd;
    m_rightDirty = ld;
    updateDirtyLabels();
    recompare();
}

void TextComparePage::exportReport()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出文本差异报告"),
                                                      QStringLiteral("text_diff_report.diff"),
                                                      QStringLiteral("差异报告 (*.diff *.txt);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    const QString report = cmp::formatTextDiffReport(
        m_leftPath->text().isEmpty() ? QStringLiteral("left") : m_leftPath->text(),
        m_rightPath->text().isEmpty() ? QStringLiteral("right") : m_rightPath->text(),
        m_diff);
    QString err;
    if (!cmp::saveTextFileWithBackup(path, report, false, nullptr, &err)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return;
    }
    m_services.logSuccess(QStringLiteral("已导出文本差异报告：%1").arg(path));
    m_services.showStatus(StatusState::Success, QStringLiteral("已导出差异报告：%1").arg(path));
}

void TextComparePage::updateHunkCounter()
{
    const int total = m_diff.hunks.size();
    const int cur = (total > 0 && m_currentHunk >= 0) ? (m_currentHunk + 1) : 0;
    m_hunkLabel->setText(QStringLiteral("%1 / %2 差异块").arg(cur).arg(total));
    const bool hasHunk = (total > 0);
    m_prevBtn->setEnabled(hasHunk);
    m_nextBtn->setEnabled(hasHunk);
    m_copyRightBtn->setEnabled(hasHunk);
    m_copyLeftBtn->setEnabled(hasHunk);
}

void TextComparePage::updateDirtyLabels()
{
    const QString lName = m_leftPath->text().isEmpty() ? QStringLiteral("左侧文档") : m_leftPath->text();
    const QString rName = m_rightPath->text().isEmpty() ? QStringLiteral("右侧文档") : m_rightPath->text();
    m_leftTitle->setText(m_leftDirty ? (lName + QStringLiteral(" *（未保存）")) : lName);
    m_rightTitle->setText(m_rightDirty ? (rName + QStringLiteral(" *（未保存）")) : rName);
}

} // namespace tw
