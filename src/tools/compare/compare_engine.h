#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace tw {
namespace cmp {

// ============================================================================
// 1. 文本比较（行级对齐 + 行内字符级差异 + 差异块操作）
// ============================================================================

struct TextDiffOptions
{
    bool ignoreWhitespace = false;
    bool ignoreCase = false;
    bool ignoreBlankLines = false;
};

struct CharSpan
{
    int start = 0;
    int length = 0;
};

enum class DiffRowKind
{
    Equal,
    Changed,
    LeftOnly,
    RightOnly
};

struct DiffRow
{
    int leftLineNo = -1;  // 1-based，-1 表示占位空行
    int rightLineNo = -1; // 1-based，-1 表示占位空行
    QString leftText;
    QString rightText;
    DiffRowKind kind = DiffRowKind::Equal;
    int hunkIndex = -1; // 所属差异块索引，Equal 行为 -1
    QVector<CharSpan> leftSpans;
    QVector<CharSpan> rightSpans;
};

struct DiffHunk
{
    int index = 0;
    int startRow = 0;
    int endRow = 0; // inclusive
    int leftStartLine = 0;  // 0-based line index in leftLines
    int leftLineCount = 0;
    int rightStartLine = 0; // 0-based line index in rightLines
    int rightLineCount = 0;
};

struct TextDiffResult
{
    QVector<DiffRow> rows;
    QVector<DiffHunk> hunks;
    int equalCount = 0;
    int changedCount = 0;
    int leftOnlyCount = 0;
    int rightOnlyCount = 0;
};

QStringList splitLines(const QString &text);
QString joinLines(const QStringList &lines);

TextDiffResult diffLines(const QStringList &leftLines, const QStringList &rightLines,
                         const TextDiffOptions &options = {});
void diffInlineChars(const QString &left, const QString &right, bool ignoreCase,
                     QVector<CharSpan> *leftSpans, QVector<CharSpan> *rightSpans);

// 将指定差异块从左侧复制到右侧，或从右侧复制到左侧，返回更新后的目标行列表。
QStringList applyHunkToRight(const QStringList &leftLines, const QStringList &rightLines,
                             const DiffHunk &hunk);
QStringList applyHunkToLeft(const QStringList &leftLines, const QStringList &rightLines,
                            const DiffHunk &hunk);

// 导出 Unified 风格的文本差异报告。
QString formatTextDiffReport(const QString &leftLabel, const QString &rightLabel,
                             const TextDiffResult &diff);

// 文本编码支持（适配麒麟电力现场 UTF-8 / GB18030(GBK) / UTF-16 混合配置文件）。
enum class TextEncoding
{
    Auto,
    Utf8,
    Gb18030,
    Utf16
};

TextEncoding detectEncoding(const QByteArray &bytes);
QString encodingDisplayName(TextEncoding enc);
QString decodeBytes(const QByteArray &bytes, TextEncoding requested,
                    TextEncoding *detected = nullptr);
QByteArray encodeText(const QString &text, TextEncoding encoding);

// 文件读写工具（支持自动编码探测、按指定编码回写及保存前自动生成 .bak 备份）。
bool loadTextFile(const QString &path, QString *content, QString *error = nullptr,
                  TextEncoding encoding = TextEncoding::Auto,
                  TextEncoding *detectedEncoding = nullptr);
bool saveTextFileWithBackup(const QString &path, const QString &content, bool createBackup,
                            QString *backupPath = nullptr, QString *error = nullptr,
                            TextEncoding encoding = TextEncoding::Utf8);

// 对比历史会话（持久化在工具隔离目录 <dataDir>/compare_history.json，权限 0600）。
struct CompareHistoryItem
{
    QString leftPath;
    QString rightPath;
};

QVector<CompareHistoryItem> loadCompareHistory(const QString &dataDir, const QString &category);
bool recordCompareHistory(const QString &dataDir, const QString &category,
                          const QString &leftPath, const QString &rightPath,
                          int maxItems = 10);

QStringList parseIgnorePatterns(const QString &rawText);

// ============================================================================
// 2. 三方合并（Base / Left / Right -> Output）
// ============================================================================

enum class MergeChunkKind
{
    Equal,
    LeftChanged,
    RightChanged,
    BothSame,
    Conflict
};

enum class MergeResolution
{
    Auto,
    UseLeft,
    UseRight,
    UseBase,
    UseBothLR,
    UseBothRL
};

struct MergeChunk
{
    int index = 0;
    MergeChunkKind kind = MergeChunkKind::Equal;
    MergeResolution resolution = MergeResolution::Auto;
    QStringList baseLines;
    QStringList leftLines;
    QStringList rightLines;
    int baseStartLine = 1;
    int leftStartLine = 1;
    int rightStartLine = 1;

    QStringList resolvedLines() const;
    bool isConflictUnresolved() const;
};

struct ThreeWayMergeResult
{
    QVector<MergeChunk> chunks;
    int conflictCount = 0;
    int autoMergedCount = 0;

    QString buildMergedText() const;
    int unresolvedConflictCount() const;
};

ThreeWayMergeResult mergeThreeWay(const QStringList &baseLines,
                                  const QStringList &leftLines,
                                  const QStringList &rightLines,
                                  const TextDiffOptions &options = {});

// ============================================================================
// 3. INI / 配置文件按节键比较与同步（支持 sca_model_conf.ini 等工程配置）
// ============================================================================

struct IniLine
{
    enum class Type { BlankOrComment, Section, KeyValue };
    Type type = Type::BlankOrComment;
    QString rawText;
    QString section;
    QString key;
    QString value;
    QString inlineComment;
    int lineNo = 0;
};

struct IniDocument
{
    QVector<IniLine> lines;

    QStringList sections() const;
    bool hasKey(const QString &section, const QString &key, bool ignoreCase = false) const;
    QString value(const QString &section, const QString &key, bool ignoreCase = false) const;
    void setValue(const QString &section, const QString &key, const QString &value,
                  bool ignoreCase = false);
    bool removeKey(const QString &section, const QString &key, bool ignoreCase = false);
    QString serialize() const;
};

IniDocument parseIni(const QString &text);

enum class IniDiffKind
{
    Equal,
    Changed,
    LeftOnly,
    RightOnly
};

struct IniDiffEntry
{
    QString section; // 空字符串表示全局区（无 [Section] 头）
    QString key;
    QString leftValue;
    QString rightValue;
    IniDiffKind kind = IniDiffKind::Equal;
};

struct IniDiffResult
{
    QVector<IniDiffEntry> entries;
    QStringList sections;
    int equalCount = 0;
    int changedCount = 0;
    int leftOnlyCount = 0;
    int rightOnlyCount = 0;
};

IniDiffResult diffIni(const IniDocument &leftDoc, const IniDocument &rightDoc,
                      bool ignoreCase = false);
QString formatIniDiffReport(const QString &leftLabel, const QString &rightLabel,
                            const IniDiffResult &diff);

// ============================================================================
// 4. 文件夹递归比较（支持 大小/修改时间 与 SHA-256 内容比对）
// ============================================================================

enum class FolderCompareMode
{
    SizeAndTime,
    ContentSha256
};

enum class FolderEntryStatus
{
    Same,
    Different,
    LeftNewer,
    RightNewer,
    LeftOnly,
    RightOnly
};

struct FolderEntry
{
    QString relPath;
    bool isDir = false;
    bool leftExists = false;
    bool rightExists = false;
    qint64 leftSize = 0;
    qint64 rightSize = 0;
    QDateTime leftMtime;
    QDateTime rightMtime;
    FolderEntryStatus status = FolderEntryStatus::Same;
};

struct FolderCompareOptions
{
    FolderCompareMode mode = FolderCompareMode::SizeAndTime;
    QStringList ignorePatterns = {QStringLiteral(".git"), QStringLiteral("*.bak")};
    int timeToleranceSecs = 2;
};

struct FolderCompareResult
{
    QVector<FolderEntry> entries;
    int sameCount = 0;
    int diffCount = 0;
    int leftOnlyCount = 0;
    int rightOnlyCount = 0;
    bool canceled = false;
};

FolderCompareResult compareFolders(
    const QString &leftDir,
    const QString &rightDir,
    const FolderCompareOptions &options = {},
    const std::function<bool()> &isCanceled = {},
    const std::function<void(int percent, const QString &status)> &onProgress = {});

// 安全复制单个文件（自动创建父目录，覆盖前可选生成 .bak 备份）。
bool copyFileWithBackup(const QString &srcPath, const QString &dstPath, bool createBackup,
                        QString *backupPath = nullptr, QString *error = nullptr);

QString formatFolderDiffReport(const QString &leftDir, const QString &rightDir,
                               const FolderCompareResult &result);

} // namespace cmp
} // namespace tw
