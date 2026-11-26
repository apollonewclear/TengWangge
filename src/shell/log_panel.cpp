#include "shell/log_panel.h"

#include "shell/theme.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>

namespace tw {
namespace {

QString formatPanelLine(const LogEntry &entry, bool showAll,
                        const LogPanel::TitleResolver &resolver)
{
    const QString stamp = entry.time.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
    const QString level = LogBuffer::levelName(entry.level);

    if (showAll) {
        QString source = QStringLiteral("平台");
        if (!entry.toolId.isEmpty())
            source = resolver ? resolver(entry.toolId) : entry.toolId;
        if (source.isEmpty())
            source = entry.toolId;
        return QStringLiteral("[%1][%2] [%3] %4").arg(level, stamp, source, entry.message);
    }

    if (entry.toolId.isEmpty())
        return QStringLiteral("[%1][%2] [平台] %3").arg(level, stamp, entry.message);
    return QStringLiteral("[%1][%2] %3").arg(level, stamp, entry.message);
}

} // namespace

LogPanel::LogPanel(LogBuffer *buffer, QWidget *parent)
    : QFrame(parent)
    , m_buffer(buffer)
{
    setObjectName(QStringLiteral("logPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAccessibleName(QStringLiteral("日志面板"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 6);
    layout->setSpacing(0);

    auto *header = new QFrame;
    header->setObjectName(QStringLiteral("logHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(10, 4, 8, 4);
    headerLayout->setSpacing(10);

    m_title = new QLabel;
    m_title->setObjectName(QStringLiteral("logTitle"));
    m_showAll = new QCheckBox(QStringLiteral("显示全部工具"));
    m_showAll->setObjectName(QStringLiteral("logShowAll"));
    m_clear = new QPushButton(QStringLiteral("清空"));
    m_clear->setObjectName(QStringLiteral("logClear"));
    m_clear->setToolTip(QStringLiteral("清空平台日志缓冲（所有工具的日志）"));
    m_close = new QPushButton(QStringLiteral("收起"));
    m_close->setObjectName(QStringLiteral("logClose"));
    m_close->setToolTip(QStringLiteral("收起日志面板 (Ctrl+L)"));

    headerLayout->addWidget(m_title);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_showAll);
    headerLayout->addWidget(m_clear);
    headerLayout->addWidget(m_close);
    layout->addWidget(header);

    m_view = new QPlainTextEdit;
    m_view->setObjectName(QStringLiteral("logView"));
    m_view->setReadOnly(true);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setMaximumBlockCount(2000);
    m_view->setAccessibleName(QStringLiteral("日志内容"));
    layout->addWidget(m_view, 1);

    connect(m_showAll, &QCheckBox::toggled, this, [this] {
        updateTitle();
        reload();
    });
    connect(m_clear, &QPushButton::clicked, this, [this] { m_buffer->clear(); });
    connect(m_close, &QPushButton::clicked, this, &LogPanel::closeRequested);
    connect(m_buffer, &LogBuffer::entryAppended, this, [this](const LogEntry &entry) {
        if (matches(entry))
            appendEntry(entry);
    });
    connect(m_buffer, &LogBuffer::cleared, this, [this] { m_view->clear(); });

    updateTitle();
    reload();
}

void LogPanel::setCurrentTool(const QString &toolId, const QString &toolTitle)
{
    if (m_toolId == toolId && m_toolTitle == toolTitle)
        return;
    m_toolId = toolId;
    m_toolTitle = toolTitle;
    updateTitle();
    if (!showsAllTools())
        reload();
}

void LogPanel::setTitleResolver(TitleResolver resolver)
{
    m_resolver = std::move(resolver);
    if (showsAllTools())
        reload();
}

bool LogPanel::showsAllTools() const
{
    return m_showAll->isChecked();
}

void LogPanel::setShowsAllTools(bool all)
{
    m_showAll->setChecked(all);
}

QString LogPanel::plainText() const
{
    return m_view->toPlainText();
}

bool LogPanel::matches(const LogEntry &entry) const
{
    if (showsAllTools() || m_toolId.isEmpty())
        return true;
    return entry.toolId.isEmpty() || entry.toolId == m_toolId;
}

void LogPanel::updateTitle()
{
    if (showsAllTools() || m_toolTitle.isEmpty())
        m_title->setText(QStringLiteral("消息日志 · 全部工具"));
    else
        m_title->setText(QStringLiteral("消息日志 · %1").arg(m_toolTitle));
}

void LogPanel::reload()
{
    const Theme &theme = Theme::workbench();
    const QVector<LogEntry> all = m_buffer->entries();
    const bool allTools = showsAllTools();

    m_view->setUpdatesEnabled(false);
    m_view->clear();
    QTextCursor cursor(m_view->document());
    cursor.beginEditBlock();
    bool first = true;
    for (const LogEntry &entry : all) {
        if (!matches(entry))
            continue;
        QTextCharFormat format;
        format.setForeground(theme.logColor(entry.level));
        if (!first)
            cursor.insertBlock();
        cursor.insertText(formatPanelLine(entry, allTools, m_resolver), format);
        first = false;
    }
    cursor.endEditBlock();
    m_view->setUpdatesEnabled(true);

    QScrollBar *bar = m_view->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void LogPanel::appendEntry(const LogEntry &entry)
{
    QScrollBar *bar = m_view->verticalScrollBar();
    const bool atEnd = bar->value() >= bar->maximum() - 2;

    const QString line = formatPanelLine(entry, showsAllTools(), m_resolver);
    QTextCursor cursor(m_view->document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat format;
    format.setForeground(Theme::workbench().logColor(entry.level));
    if (!m_view->document()->isEmpty())
        cursor.insertBlock();
    cursor.insertText(line, format);

    if (atEnd)
        bar->setValue(bar->maximum());
}

} // namespace tw
