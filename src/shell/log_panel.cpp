#include "shell/log_panel.h"

#include "shell/icon_factory.h"
#include "shell/theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>

namespace tw {

LogPanel::LogPanel(LogBuffer *buffer, QWidget *parent)
    : QFrame(parent)
    , m_buffer(buffer)
{
    const Theme &theme = Theme::workbench();

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
    headerLayout->setSpacing(8);

    m_title = new QLabel;
    m_title->setObjectName(QStringLiteral("logTitle"));

    m_search = new QLineEdit;
    m_search->setObjectName(QStringLiteral("logSearch"));
    m_search->setPlaceholderText(QStringLiteral("筛选日志关键字…"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(176);
    m_search->setFocusPolicy(Qt::ClickFocus);
    m_search->setAccessibleName(QStringLiteral("日志关键字筛选"));

    m_levelFilter = new QComboBox;
    m_levelFilter->setObjectName(QStringLiteral("logLevelFilter"));
    m_levelFilter->addItem(QStringLiteral("全部级别"), static_cast<int>(LevelFilter::All));
    m_levelFilter->addItem(QStringLiteral("警告与错误"), static_cast<int>(LevelFilter::WarningAndError));
    m_levelFilter->addItem(QStringLiteral("仅错误"), static_cast<int>(LevelFilter::ErrorOnly));
    m_levelFilter->setFocusPolicy(Qt::ClickFocus);
    m_levelFilter->setAccessibleName(QStringLiteral("日志级别筛选"));

    m_showAll = new QCheckBox(QStringLiteral("显示全部工具"));
    m_showAll->setObjectName(QStringLiteral("logShowAll"));

    m_export = new QPushButton(QStringLiteral("导出"));
    m_export->setObjectName(QStringLiteral("logExport"));
    m_export->setIcon(IconFactory::icon(QStringLiteral("export"), theme.primary, theme.primary,
                                        theme.disabledText, theme.accent));
    m_export->setFocusPolicy(Qt::ClickFocus);
    m_export->setToolTip(QStringLiteral("将当前筛选出的日志导出为文本文件"));

    m_clear = new QPushButton(QStringLiteral("清空"));
    m_clear->setObjectName(QStringLiteral("logClear"));
    m_clear->setToolTip(QStringLiteral("清空平台日志缓冲（所有工具的日志）"));

    m_close = new QPushButton(QStringLiteral("收起"));
    m_close->setObjectName(QStringLiteral("logClose"));
    m_close->setToolTip(QStringLiteral("收起日志面板 (Ctrl+L)"));

    headerLayout->addWidget(m_title);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_search);
    headerLayout->addWidget(m_levelFilter);
    headerLayout->addWidget(m_showAll);
    headerLayout->addWidget(m_export);
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

    connect(m_search, &QLineEdit::textChanged, this, [this] { reload(); });
    connect(m_levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { reload(); });
    connect(m_showAll, &QCheckBox::toggled, this, [this] {
        updateTitle();
        reload();
    });
    connect(m_export, &QPushButton::clicked, this, &LogPanel::promptExport);
    connect(m_clear, &QPushButton::clicked, this, [this] { m_buffer->clear(); });
    connect(m_close, &QPushButton::clicked, this, &LogPanel::closeRequested);
    connect(m_buffer, &LogBuffer::entryAppended, this, [this](const LogEntry &entry) {
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

LogPanel::LevelFilter LogPanel::levelFilter() const
{
    return static_cast<LevelFilter>(m_levelFilter->currentData().toInt());
}

void LogPanel::setLevelFilter(LevelFilter filter)
{
    const int idx = m_levelFilter->findData(static_cast<int>(filter));
    if (idx >= 0)
        m_levelFilter->setCurrentIndex(idx);
}

QString LogPanel::searchText() const
{
    return m_search->text();
}

void LogPanel::setSearchText(const QString &text)
{
    m_search->setText(text);
}

bool LogPanel::exportToFile(const QString &filePath, QString *error) const
{
    if (filePath.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("导出文件路径为空");
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("无法写入文件：%1").arg(filePath);
        return false;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QByteArray payload = m_view->toPlainText().toUtf8();
    if (!payload.isEmpty() && !payload.endsWith('\n'))
        payload.append('\n');
    if (file.write(payload) != payload.size()) {
        if (error)
            *error = QStringLiteral("写入日志文件不完整：%1").arg(filePath);
        return false;
    }
    file.flush();
    return true;
}

void LogPanel::promptExport()
{
    const QString defaultName = QDir::home().filePath(
        QStringLiteral("tengwangge-log-%1.log")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
    const QString chosen = QFileDialog::getSaveFileName(
        window(), QStringLiteral("导出消息日志"), defaultName,
        QStringLiteral("日志文件 (*.log *.txt);;所有文件 (*)"));
    if (chosen.isEmpty())
        return;

    QString err;
    if (exportToFile(chosen, &err))
        emit exported(chosen);
    else
        emit exportFailed(err);
}

QString LogPanel::plainText() const
{
    return m_view->toPlainText();
}

QString LogPanel::formatLine(const LogEntry &entry) const
{
    const QString stamp = entry.time.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
    const QString level = LogBuffer::levelName(entry.level);

    if (showsAllTools()) {
        QString source = QStringLiteral("平台");
        if (!entry.toolId.isEmpty())
            source = m_resolver ? m_resolver(entry.toolId) : entry.toolId;
        if (source.isEmpty())
            source = entry.toolId;
        return QStringLiteral("[%1][%2] [%3] %4").arg(level, stamp, source, entry.message);
    }

    if (entry.toolId.isEmpty())
        return QStringLiteral("[%1][%2] [平台] %3").arg(level, stamp, entry.message);
    return QStringLiteral("[%1][%2] %3").arg(level, stamp, entry.message);
}

bool LogPanel::matches(const LogEntry &entry, const QString &formattedLine) const
{
    if (!showsAllTools() && !m_toolId.isEmpty()
        && !entry.toolId.isEmpty() && entry.toolId != m_toolId) {
        return false;
    }

    switch (levelFilter()) {
    case LevelFilter::All:
        break;
    case LevelFilter::WarningAndError:
        if (entry.level != LogLevel::Warning && entry.level != LogLevel::Error)
            return false;
        break;
    case LevelFilter::ErrorOnly:
        if (entry.level != LogLevel::Error)
            return false;
        break;
    }

    const QString needle = m_search->text().trimmed();
    if (!needle.isEmpty() && !formattedLine.contains(needle, Qt::CaseInsensitive))
        return false;

    return true;
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

    m_view->setUpdatesEnabled(false);
    m_view->clear();
    QTextCursor cursor(m_view->document());
    cursor.beginEditBlock();
    bool first = true;
    for (const LogEntry &entry : all) {
        const QString line = formatLine(entry);
        if (!matches(entry, line))
            continue;
        QTextCharFormat format;
        format.setForeground(theme.logColor(entry.level));
        if (!first)
            cursor.insertBlock();
        cursor.insertText(line, format);
        first = false;
    }
    cursor.endEditBlock();
    m_view->setUpdatesEnabled(true);

    QScrollBar *bar = m_view->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void LogPanel::appendEntry(const LogEntry &entry)
{
    const QString line = formatLine(entry);
    if (!matches(entry, line))
        return;

    QScrollBar *bar = m_view->verticalScrollBar();
    const bool atEnd = bar->value() >= bar->maximum() - 2;

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
