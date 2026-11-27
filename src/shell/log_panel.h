#pragma once

#include "core/log_buffer.h"

#include <QFrame>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace tw {

// 底部消息日志面板，读平台唯一的 LogBuffer。
// 支持按当前工具/全部工具切换、按日志级别筛选、按关键字实时检索以及一键导出日志文件。
class LogPanel : public QFrame
{
    Q_OBJECT

public:
    enum class LevelFilter { All = 0, WarningAndError = 1, ErrorOnly = 2 };
    using TitleResolver = std::function<QString(const QString &toolId)>;

    explicit LogPanel(LogBuffer *buffer, QWidget *parent = nullptr);

    void setCurrentTool(const QString &toolId, const QString &toolTitle);
    void setTitleResolver(TitleResolver resolver);

    bool showsAllTools() const;
    void setShowsAllTools(bool all);

    LevelFilter levelFilter() const;
    void setLevelFilter(LevelFilter filter);

    QString searchText() const;
    void setSearchText(const QString &text);

    // 将当前筛选条件下可见的日志内容导出到指定文件（0600 权限）。
    bool exportToFile(const QString &filePath, QString *error = nullptr) const;

    QPlainTextEdit *view() const { return m_view; }
    QCheckBox *showAllCheck() const { return m_showAll; }
    QComboBox *levelCombo() const { return m_levelFilter; }
    QLineEdit *searchEdit() const { return m_search; }
    QPushButton *exportButton() const { return m_export; }
    QString plainText() const;

signals:
    void closeRequested();
    void exported(const QString &filePath);
    void exportFailed(const QString &error);

private:
    void reload();
    void appendEntry(const LogEntry &entry);
    bool matches(const LogEntry &entry, const QString &formattedLine) const;
    QString formatLine(const LogEntry &entry) const;
    void updateTitle();
    void promptExport();

    LogBuffer *m_buffer;
    QString m_toolId;
    QString m_toolTitle;
    TitleResolver m_resolver;
    QFrame *m_header = nullptr;
    QLabel *m_title = nullptr;
    QLineEdit *m_search = nullptr;
    QComboBox *m_levelFilter = nullptr;
    QCheckBox *m_showAll = nullptr;
    QPushButton *m_export = nullptr;
    QPushButton *m_clear = nullptr;
    QPushButton *m_close = nullptr;
    QPlainTextEdit *m_view = nullptr;
};

} // namespace tw
