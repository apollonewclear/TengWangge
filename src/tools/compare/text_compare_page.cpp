#include "tools/compare/text_compare_page.h"

#include "shell/page_chrome.h"
#include "shell/theme.h"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QUrl>
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
        setTextCursor(QTextCursor(blk));
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
// 中央差异缩略条
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
    setAcceptDrops(true);
    m_recompareTimer->setSingleShot(true);
    m_recompareTimer->setInterval(260);
    connect(m_recompareTimer, &QTimer::timeout, this, &TextComparePage::recompare);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(5);

    // ---- 路径与编码栏 ----
    auto *pathRow = new QHBoxLayout;
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(5);

    m_leftPath = new QLineEdit;
    m_leftPath->setPlaceholderText(QStringLiteral("左侧本地文件路径（支持拖放或直接编辑）"));
    m_leftPath->setAccessibleName(QStringLiteral("左侧文件路径"));
    QPushButton *browseL = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));
    m_saveLeftBtn = ui::makeActionButton(QStringLiteral("保存左"), QStringLiteral("save"));
    QPushButton *swapBtn = ui::makeActionButton(QStringLiteral("交换"), QStringLiteral("swap"));

    m_rightPath = new QLineEdit;
    m_rightPath->setPlaceholderText(QStringLiteral("右侧本地文件路径（支持拖放或直接编辑）"));
    m_rightPath->setAccessibleName(QStringLiteral("右侧文件路径"));
    QPushButton *browseR = ui::makeActionButton(QStringLiteral("浏览"), QStringLiteral("folder"));
    m_saveRightBtn = ui::makeActionButton(QStringLiteral("保存右"), QStringLiteral("save"));

    m_encodingCombo = new QComboBox;
    m_encodingCombo->setToolTip(QStringLiteral("文件读取与保存字符编码"));
    m_encodingCombo->addItem(QStringLiteral("自动编码"), static_cast<int>(cmp::TextEncoding::Auto));
    m_encodingCombo->addItem(QStringLiteral("UTF-8"), static_cast<int>(cmp::TextEncoding::Utf8));
    m_encodingCombo->addItem(QStringLiteral("GB18030/GBK"), static_cast<int>(cmp::TextEncoding::Gb18030));
    m_encodingCombo->addItem(QStringLiteral("UTF-16"), static_cast<int>(cmp::TextEncoding::Utf16));

    QPushButton *compareBtn = ui::makeActionButton(QStringLiteral("比较"), QStringLiteral("refresh"));
    m_historyBtn = ui::makeActionButton(QStringLiteral("历史"), QStringLiteral("log"));

    pathRow->addWidget(ui::makeLabel(QStringLiteral("左:"), "fieldLabel"));
    pathRow->addWidget(m_leftPath, 1);
    pathRow->addWidget(browseL);
    pathRow->addWidget(m_saveLeftBtn);
    pathRow->addWidget(swapBtn);
    pathRow->addWidget(ui::makeLabel(QStringLiteral("右:"), "fieldLabel"));
    pathRow->addWidget(m_rightPath, 1);
    pathRow->addWidget(browseR);
    pathRow->addWidget(m_saveRightBtn);
    pathRow->addWidget(m_encodingCombo);
    pathRow->addWidget(compareBtn);
    pathRow->addWidget(m_historyBtn);
    root->addLayout(pathRow);

    // ---- 操作与文内查找工具栏 ----
    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(5);

    m_modeAllBtn = new QPushButton(QStringLiteral("并排编辑"));
    m_modeAllBtn->setCheckable(true);
    m_modeAllBtn->setChecked(true);
    m_modeDiffBtn = new QPushButton(QStringLiteral("仅差异"));
    m_modeDiffBtn->setCheckable(true);

    m_prevBtn = ui::makeActionButton(QStringLiteral("上一处"), QStringLiteral("update"));
    m_prevBtn->setToolTip(QStringLiteral("上一处差异 (F7)"));
    m_nextBtn = ui::makeActionButton(QStringLiteral("下一处"), QStringLiteral("compare"));
    m_nextBtn->setToolTip(QStringLiteral("下一处差异 (F8)"));
    m_hunkLabel = ui::makeLabel(QStringLiteral("0 / 0"), "code");
    m_hunkLabel->setWordWrap(false);

    m_copyRightBtn = ui::makeActionButton(QStringLiteral("块 → 右"), QStringLiteral("copy_right"));
    m_copyRightBtn->setToolTip(QStringLiteral("复制当前差异块到右侧 (Alt+→)"));
    m_copyLeftBtn = ui::makeActionButton(QStringLiteral("块 ← 左"), QStringLiteral("copy_left"));
    m_copyLeftBtn->setToolTip(QStringLiteral("复制当前差异块到左侧 (Alt+←)"));

    m_ignoreWs = new QCheckBox(QStringLiteral("忽略空白"));
    m_ignoreCase = new QCheckBox(QStringLiteral("忽略大小写"));
    m_autoBackup = new QCheckBox(QStringLiteral(".bak"));
    m_autoBackup->setToolTip(QStringLiteral("保存修改前自动生成 .bak 备份"));
    m_autoBackup->setChecked(true);

    m_findEdit = new QLineEdit;
    m_findEdit->setPlaceholderText(QStringLiteral("文内查找 (Ctrl+F)..."));
    m_findEdit->setClearButtonEnabled(true);
    m_findEdit->setFixedWidth(155);
    m_findEdit->setAccessibleName(QStringLiteral("文内查找输入框"));

    QPushButton *sampleBtn = ui::makeActionButton(QStringLiteral("示例"), QStringLiteral("add"));
    QPushButton *exportBtn = ui::makeActionButton(QStringLiteral("导出"), QStringLiteral("export"));

    actionRow->addWidget(m_modeAllBtn);
    actionRow->addWidget(m_modeDiffBtn);
    actionRow->addWidget(m_prevBtn);
    actionRow->addWidget(m_nextBtn);
    actionRow->addWidget(m_hunkLabel);
    actionRow->addWidget(m_copyRightBtn);
    actionRow->addWidget(m_copyLeftBtn);
    actionRow->addWidget(m_ignoreWs);
    actionRow->addWidget(m_ignoreCase);
    actionRow->addWidget(m_autoBackup);
    actionRow->addStretch(1);
    actionRow->addWidget(m_findEdit);
    actionRow->addWidget(sampleBtn);
    actionRow->addWidget(exportBtn);
    root->addLayout(actionRow);

    // ---- 汇总统计条（行数 KPI 徽章 + 实时 MD5 比对）----
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
    m_statTotal = makeBadge(QStringLiteral("总计: 0 行"), "statTotal");
    m_statSame = makeBadge(QStringLiteral("相同: 0 (0%)"), "statSame");
    m_statChanged = makeBadge(QStringLiteral("修改: 0"), "statDiff");
    m_statLeft = makeBadge(QStringLiteral("仅左: 0"), "statLeft");
    m_statRight = makeBadge(QStringLiteral("仅右: 0"), "statRight");
    m_statMd5 = makeBadge(QStringLiteral("MD5: —"), "statMd5Ok");

    sumLayout->addWidget(m_statTotal);
    sumLayout->addWidget(m_statSame);
    sumLayout->addWidget(m_statChanged);
    sumLayout->addWidget(m_statLeft);
    sumLayout->addWidget(m_statRight);
    sumLayout->addStretch(1);
    sumLayout->addWidget(m_statMd5);
    root->addWidget(summaryBar);

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
    connect(m_historyBtn, &QPushButton::clicked, this, &TextComparePage::showHistoryMenu);
    connect(m_encodingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString lp = m_leftPath->text().trimmed();
        const QString rp = m_rightPath->text().trimmed();
        if (!lp.isEmpty() && !rp.isEmpty() && QFileInfo::exists(lp) && QFileInfo::exists(rp)
            && !m_leftDirty && !m_rightDirty) {
            openFiles(lp, rp);
        }
    });
    connect(compareBtn, &QPushButton::clicked, this, [this]() {
        const QString lp = m_leftPath->text().trimmed();
        const QString rp = m_rightPath->text().trimmed();
        if (!lp.isEmpty() && !rp.isEmpty() && QFileInfo::exists(lp) && QFileInfo::exists(rp)
            && !m_leftDirty && !m_rightDirty) {
            openFiles(lp, rp);
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
    connect(m_findEdit, &QLineEdit::returnPressed, this, [this]() {
        findInEditors(m_findEdit->text());
    });
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

    installShortcuts();
    updateHunkCounter();
    updateDirtyLabels();
}

void TextComparePage::installShortcuts()
{
    auto bind = [this](const QKeySequence &seq, const std::function<void()> &fn) {
        auto *sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, fn);
    };
    bind(QKeySequence(Qt::Key_F7), [this]() { prevHunk(); });
    bind(QKeySequence(Qt::Key_F8), [this]() { nextHunk(); });
    bind(QKeySequence(Qt::ALT + Qt::Key_Right), [this]() { copyCurrentHunkToRight(); });
    bind(QKeySequence(Qt::ALT + Qt::Key_Left), [this]() { copyCurrentHunkToLeft(); });
    bind(QKeySequence(Qt::CTRL + Qt::Key_F), [this]() {
        m_findEdit->setFocus();
        m_findEdit->selectAll();
    });
    bind(QKeySequence(Qt::CTRL + Qt::Key_S), [this]() {
        if (m_rightEditor->hasFocus() || (m_rightDirty && !m_leftDirty))
            saveRight();
        else if (m_leftDirty || m_leftEditor->hasFocus())
            saveLeft();
    });
}

void TextComparePage::setEncoding(cmp::TextEncoding encoding)
{
    const int idx = m_encodingCombo->findData(static_cast<int>(encoding));
    if (idx >= 0)
        m_encodingCombo->setCurrentIndex(idx);
}

cmp::TextEncoding TextComparePage::selectedEncoding() const
{
    return static_cast<cmp::TextEncoding>(m_encodingCombo->currentData().toInt());
}

bool TextComparePage::openFiles(const QString &leftPath, const QString &rightPath)
{
    QString leftContent;
    QString rightContent;
    QString err;
    const cmp::TextEncoding req = selectedEncoding();

    if (!leftPath.isEmpty() && !cmp::loadTextFile(leftPath, &leftContent, &err, req, &m_leftDetectedEnc)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    if (!rightPath.isEmpty() && !cmp::loadTextFile(rightPath, &rightContent, &err, req, &m_rightDetectedEnc)) {
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }

    m_leftPath->setText(leftPath);
    m_rightPath->setText(rightPath);
    cmp::recordCompareHistory(m_services.dataDir(), QStringLiteral("text"), leftPath, rightPath);
    setTexts(leftContent, rightContent, leftPath, rightPath);
    m_services.logInfo(QStringLiteral("文本对比完成：%1 [%2] ⇄ %3 [%4]（差异块 %5 处）")
                           .arg(leftPath, cmp::encodingDisplayName(m_leftDetectedEnc),
                                rightPath, cmp::encodingDisplayName(m_rightDetectedEnc))
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
    m_leftDetectedEnc = cmp::TextEncoding::Utf8;
    m_rightDetectedEnc = cmp::TextEncoding::Utf8;
    setTexts(leftSample, rightSample, QStringLiteral("sample_left.conf"), QStringLiteral("sample_right.conf"));
    m_services.showStatus(StatusState::Info, QStringLiteral("已载入文本对比示例（F7/F8 跳差异，Alt+←/→ 块复制，Ctrl+F 查找）"));
}

QString TextComparePage::leftText() const
{
    return m_leftEditor->toPlainText();
}

QString TextComparePage::rightText() const
{
    return m_rightEditor->toPlainText();
}

bool TextComparePage::findInEditors(const QString &keyword)
{
    const QString kw = keyword.trimmed();
    if (kw.isEmpty())
        return false;

    auto findOrWrap = [&kw](DiffCodeEditor *ed) -> bool {
        if (ed->find(kw))
            return true;
        QTextCursor c = ed->textCursor();
        c.movePosition(QTextCursor::Start);
        ed->setTextCursor(c);
        return ed->find(kw);
    };

    const bool hitL = findOrWrap(m_leftEditor);
    const bool hitR = findOrWrap(m_rightEditor);
    if (hitL || hitR) {
        m_services.showStatus(StatusState::Info, QStringLiteral("已定位查找关键字：“%1”").arg(kw));
        return true;
    }
    m_services.showStatus(StatusState::Error, QStringLiteral("未找到关键字：“%1”").arg(kw));
    return false;
}

void TextComparePage::recompare()
{
    cmp::TextDiffOptions opt;
    opt.ignoreWhitespace = m_ignoreWs->isChecked();
    opt.ignoreCase = m_ignoreCase->isChecked();

    const QString leftPlain = m_leftEditor->toPlainText();
    const QString rightPlain = m_rightEditor->toPlainText();
    m_diff = cmp::diffLines(cmp::splitLines(leftPlain), cmp::splitLines(rightPlain), opt);

    const cmp::TextEncoding reqEnc = selectedEncoding();
    const cmp::TextEncoding leftEnc = (reqEnc == cmp::TextEncoding::Auto) ? m_leftDetectedEnc : reqEnc;
    const cmp::TextEncoding rightEnc = (reqEnc == cmp::TextEncoding::Auto) ? m_rightDetectedEnc : reqEnc;
    m_leftMd5 = cmp::computeTextMd5(leftPlain, leftEnc);
    m_rightMd5 = cmp::computeTextMd5(rightPlain, rightEnc);

    if (m_diff.hunks.isEmpty())
        m_currentHunk = -1;
    else if (m_currentHunk < 0 || m_currentHunk >= m_diff.hunks.size())
        m_currentHunk = 0;

    applyHighlights();
    if (m_viewStack->currentIndex() == 1)
        rebuildDiffOnlyTable();
    updateHunkCounter();
    updateSummaryBar();

    m_services.showStatus(m_diff.hunks.isEmpty() ? StatusState::Success : StatusState::Info,
                          QStringLiteral("文本比对：总计 %1 行（相同 %2，修改 %3，仅左 %4，仅右 %5，%6 处差异块）")
                              .arg(m_diff.rows.size())
                              .arg(m_diff.equalCount)
                              .arg(m_diff.changedCount)
                              .arg(m_diff.leftOnlyCount)
                              .arg(m_diff.rightOnlyCount)
                              .arg(m_diff.hunks.size()));
}

void TextComparePage::updateSummaryBar()
{
    const int total = m_diff.rows.size();
    const double pct = total > 0 ? (100.0 * m_diff.equalCount / total) : 100.0;
    m_statTotal->setText(QStringLiteral("总计: %1 行").arg(total));
    m_statSame->setText(QStringLiteral("相同: %1 (%2%)")
                            .arg(m_diff.equalCount)
                            .arg(QString::number(pct, 'f', total > 0 ? 1 : 0)));
    m_statChanged->setText(QStringLiteral("修改: %1").arg(m_diff.changedCount));
    m_statLeft->setText(QStringLiteral("仅左: %1").arg(m_diff.leftOnlyCount));
    m_statRight->setText(QStringLiteral("仅右: %1").arg(m_diff.rightOnlyCount));

    if (m_leftMd5.isEmpty() && m_rightMd5.isEmpty()) {
        m_statMd5->setText(QStringLiteral("MD5: —"));
        ui::setRole(m_statMd5, "statMd5Ok");
        m_statMd5->setToolTip(QString());
    } else if (m_leftMd5 == m_rightMd5) {
        m_statMd5->setText(QStringLiteral("MD5 一致: %1").arg(m_leftMd5.left(8)));
        ui::setRole(m_statMd5, "statMd5Ok");
        m_statMd5->setToolTip(QStringLiteral("左右内容 MD5 完全一致\nMD5: %1").arg(m_leftMd5));
    } else {
        m_statMd5->setText(QStringLiteral("MD5 不同: %1 ≠ %2").arg(m_leftMd5.left(8), m_rightMd5.left(8)));
        ui::setRole(m_statMd5, "statMd5Diff");
        m_statMd5->setToolTip(QStringLiteral("左侧 MD5: %1\n右侧 MD5: %2").arg(m_leftMd5, m_rightMd5));
    }
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
    jumpToHunk((m_currentHunk <= 0) ? (m_diff.hunks.size() - 1) : (m_currentHunk - 1));
}

void TextComparePage::nextHunk()
{
    if (m_diff.hunks.isEmpty())
        return;
    jumpToHunk((m_currentHunk + 1) % m_diff.hunks.size());
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
    const cmp::TextEncoding saveEnc = (selectedEncoding() == cmp::TextEncoding::Auto)
        ? m_leftDetectedEnc
        : selectedEncoding();
    QString bakPath, err;
    if (!cmp::saveTextFileWithBackup(path, m_leftEditor->toPlainText(), m_autoBackup->isChecked(),
                                     &bakPath, &err, saveEnc)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_leftDirty = false;
    updateDirtyLabels();
    const QString msg = bakPath.isEmpty()
        ? QStringLiteral("已保存左侧文件：%1 [%2]").arg(path, cmp::encodingDisplayName(saveEnc))
        : QStringLiteral("已保存左侧文件：%1 [%2]（备份：%3）")
              .arg(path, cmp::encodingDisplayName(saveEnc), bakPath);
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
    const cmp::TextEncoding saveEnc = (selectedEncoding() == cmp::TextEncoding::Auto)
        ? m_rightDetectedEnc
        : selectedEncoding();
    QString bakPath, err;
    if (!cmp::saveTextFileWithBackup(path, m_rightEditor->toPlainText(), m_autoBackup->isChecked(),
                                     &bakPath, &err, saveEnc)) {
        if (error)
            *error = err;
        m_services.logError(err);
        m_services.showStatus(StatusState::Error, err);
        return false;
    }
    m_rightDirty = false;
    updateDirtyLabels();
    const QString msg = bakPath.isEmpty()
        ? QStringLiteral("已保存右侧文件：%1 [%2]").arg(path, cmp::encodingDisplayName(saveEnc))
        : QStringLiteral("已保存右侧文件：%1 [%2]（备份：%3）")
              .arg(path, cmp::encodingDisplayName(saveEnc), bakPath);
    m_services.logSuccess(msg);
    m_services.showStatus(StatusState::Success, msg);
    return true;
}

void TextComparePage::showHistoryMenu()
{
    const QVector<cmp::CompareHistoryItem> items =
        cmp::loadCompareHistory(m_services.dataDir(), QStringLiteral("text"));
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

void TextComparePage::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void TextComparePage::dropEvent(QDropEvent *event)
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
        QString content, err;
        cmp::TextEncoding det = cmp::TextEncoding::Utf8;
        if (!cmp::loadTextFile(files.at(0), &content, &err, selectedEncoding(), &det)) {
            m_services.logError(err);
            return;
        }
        m_suppressEditSignal = true;
        if (event->pos().x() < width() / 2) {
            m_leftPath->setText(files.at(0));
            m_leftEditor->setPlainText(content);
            m_leftDetectedEnc = det;
            m_leftDirty = false;
        } else {
            m_rightPath->setText(files.at(0));
            m_rightEditor->setPlainText(content);
            m_rightDetectedEnc = det;
            m_rightDirty = false;
        }
        m_suppressEditSignal = false;
        updateDirtyLabels();
        recompare();
    }
}

void TextComparePage::browseLeft()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择左侧文件"), m_leftPath->text());
    if (path.isEmpty())
        return;
    QString content, err;
    if (!cmp::loadTextFile(path, &content, &err, selectedEncoding(), &m_leftDetectedEnc)) {
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
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择右侧文件"), m_rightPath->text());
    if (path.isEmpty())
        return;
    QString content, err;
    if (!cmp::loadTextFile(path, &content, &err, selectedEncoding(), &m_rightDetectedEnc)) {
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
    std::swap(m_leftDirty, m_rightDirty);
    std::swap(m_leftDetectedEnc, m_rightDetectedEnc);

    m_leftPath->setText(rp);
    m_rightPath->setText(lp);
    m_suppressEditSignal = true;
    m_leftEditor->setPlainText(rt);
    m_rightEditor->setPlainText(lt);
    m_suppressEditSignal = false;
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
    m_hunkLabel->setText(QStringLiteral("%1 / %2").arg(cur).arg(total));
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
    const QString lEnc = cmp::encodingDisplayName(m_leftDetectedEnc);
    const QString rEnc = cmp::encodingDisplayName(m_rightDetectedEnc);
    m_leftTitle->setText(m_leftDirty
                             ? QStringLiteral("%1 [%2] *（未保存）").arg(lName, lEnc)
                             : QStringLiteral("%1 [%2]").arg(lName, lEnc));
    m_rightTitle->setText(m_rightDirty
                              ? QStringLiteral("%1 [%2] *（未保存）").arg(rName, rEnc)
                              : QStringLiteral("%1 [%2]").arg(rName, rEnc));
}

} // namespace tw
