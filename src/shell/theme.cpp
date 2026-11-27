#include "shell/theme.h"

#include <QFile>

static void initShellResources()
{
    Q_INIT_RESOURCE(tengwangge_shell);
}

namespace tw {

const Theme &Theme::workbench()
{
    static const Theme theme;
    return theme;
}

QColor Theme::statusColor(StatusState state) const
{
    switch (state) {
    case StatusState::Idle: return statusIdle;
    case StatusState::Info: return statusInfo;
    case StatusState::Busy: return statusBusy;
    case StatusState::Success: return statusSuccess;
    case StatusState::Error: return statusError;
    }
    return statusIdle;
}

QColor Theme::logColor(LogLevel level) const
{
    switch (level) {
    case LogLevel::Info: return logInfo;
    case LogLevel::Success: return logSuccess;
    case LogLevel::Warning: return logWarning;
    case LogLevel::Error: return logError;
    }
    return logInfo;
}

QMap<QString, QString> Theme::tokens() const
{
    QMap<QString, QString> map;
    auto color = [&map](const char *name, const QColor &value) {
        map.insert(QString::fromLatin1(name), value.name(QColor::HexRgb));
    };
    color("chrome", chrome);
    color("chromeRaised", chromeRaised);
    color("chromeHover", chromeHover);
    color("chromeSelected", chromeSelected);
    color("chromeSelectedBorder", chromeSelectedBorder);
    color("chromeBorder", chromeBorder);
    color("chromeText", chromeText);
    color("chromeMuted", chromeMuted);
    color("chromeDisabled", chromeDisabled);
    color("accent", accent);
    color("teal", teal);
    color("emerald", emerald);
    color("surface", surface);
    color("panel", panel);
    color("panelAlt", panelAlt);
    color("selectionSoft", selectionSoft);
    color("dangerSoft", dangerSoft);
    color("lockBg", lockBg);
    color("lockBorder", lockBorder);
    color("lockText", lockText);
    color("border", border);
    color("borderSoft", borderSoft);
    color("text", text);
    color("textMuted", textMuted);
    color("disabledText", disabledText);
    color("primary", primary);
    color("primaryHover", primaryHover);
    color("danger", danger);
    color("statusIdle", statusIdle);
    color("statusInfo", statusInfo);
    color("statusBusy", statusBusy);
    color("statusSuccess", statusSuccess);
    color("statusError", statusError);
    map.insert(QStringLiteral("fontFamily"), fontFamily);
    map.insert(QStringLiteral("monoFamily"), monoFamily);
    map.insert(QStringLiteral("fontSize"), QString::number(fontPixelSize));
    return map;
}

QString Theme::expand(const QString &templateText, const QMap<QString, QString> &tokens)
{
    QString result = templateText;
    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it)
        result.replace(QLatin1Char('@') + it.key() + QLatin1Char('@'), it.value());
    return result;
}

QString Theme::styleSheet() const
{
    initShellResources();
    QFile file(QStringLiteral(":/tengwangge/theme.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return expand(QString::fromUtf8(file.readAll()), tokens());
}

} // namespace tw
