#include "tools/compare/compare_engine.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QTextCodec>
#include <QTextStream>

#include <algorithm>

namespace tw {
namespace cmp {
namespace {

QString normalizeLine(const QString &line, const TextDiffOptions &opt)
{
    QString s = line;
    if (opt.ignoreWhitespace)
        s = s.simplified();
    if (opt.ignoreCase)
        s = s.toLower();
    return s;
}

bool linesMatch(const QString &a, const QString &b, const TextDiffOptions &opt)
{
    if (opt.ignoreBlankLines && a.trimmed().isEmpty() && b.trimmed().isEmpty())
        return true;
    return normalizeLine(a, opt) == normalizeLine(b, opt);
}

enum class EditOp { Equal, DeleteLeft, InsertRight };

struct EditStep
{
    EditOp op;
    int leftIdx = -1;
    int rightIdx = -1;
};

// 线性空间/可控规模的 LCS + 锚点分治比对（对大文件做首尾剥离，避免 O(N*M) 内存爆炸）。
QVector<EditStep> computeSequenceDiff(const QStringList &a, const QStringList &b,
                                      const TextDiffOptions &opt)
{
    const int n = a.size();
    const int m = b.size();

    int prefix = 0;
    while (prefix < n && prefix < m && linesMatch(a.at(prefix), b.at(prefix), opt))
        ++prefix;

    int suffix = 0;
    while (suffix < (n - prefix) && suffix < (m - prefix)
           && linesMatch(a.at(n - 1 - suffix), b.at(m - 1 - suffix), opt)) {
        ++suffix;
    }

    QVector<EditStep> result;
    result.reserve(n + m);
    for (int i = 0; i < prefix; ++i)
        result.append({EditOp::Equal, i, i});

    const int midN = n - prefix - suffix;
    const int midM = m - prefix - suffix;

    if (midN == 0) {
        for (int j = 0; j < midM; ++j)
            result.append({EditOp::InsertRight, -1, prefix + j});
    } else if (midM == 0) {
        for (int i = 0; i < midN; ++i)
            result.append({EditOp::DeleteLeft, prefix + i, -1});
    } else if (static_cast<qint64>(midN) * static_cast<qint64>(midM) <= 4000000LL) {
        // 标准精确 LCS 动态规划（<= 2000x2000 约 16MB 以内，毫秒级完成）
        const int cols = midM + 1;
        QVector<int> dp((midN + 1) * cols, 0);
        for (int i = midN - 1; i >= 0; --i) {
            for (int j = midM - 1; j >= 0; --j) {
                if (linesMatch(a.at(prefix + i), b.at(prefix + j), opt))
                    dp[i * cols + j] = dp[(i + 1) * cols + (j + 1)] + 1;
                else
                    dp[i * cols + j] = qMax(dp[(i + 1) * cols + j], dp[i * cols + (j + 1)]);
            }
        }
        int i = 0;
        int j = 0;
        while (i < midN && j < midM) {
            if (linesMatch(a.at(prefix + i), b.at(prefix + j), opt)) {
                result.append({EditOp::Equal, prefix + i, prefix + j});
                ++i;
                ++j;
            } else if (dp[(i + 1) * cols + j] >= dp[i * cols + (j + 1)]) {
                result.append({EditOp::DeleteLeft, prefix + i, -1});
                ++i;
            } else {
                result.append({EditOp::InsertRight, -1, prefix + j});
                ++j;
            }
        }
        while (i < midN) {
            result.append({EditOp::DeleteLeft, prefix + i, -1});
            ++i;
        }
        while (j < midM) {
            result.append({EditOp::InsertRight, -1, prefix + j});
            ++j;
        }
    } else {
        // 超大文件回退：基于哈希锚点贪心同步窗口，确保毫秒级响应不卡死 UI
        QHash<QString, QVector<int>> rightPositions;
        for (int j = 0; j < midM; ++j) {
            const QString key = normalizeLine(b.at(prefix + j), opt);
            if (rightPositions[key].size() < 16)
                rightPositions[key].append(j);
        }
        int i = 0;
        int j = 0;
        while (i < midN && j < midM) {
            if (linesMatch(a.at(prefix + i), b.at(prefix + j), opt)) {
                result.append({EditOp::Equal, prefix + i, prefix + j});
                ++i;
                ++j;
                continue;
            }
            // 在未来小窗口内寻找最近匹配锚点
            int bestI = -1;
            int bestJ = -1;
            int bestDist = 128;
            for (int di = 0; di < qMin(64, midN - i); ++di) {
                const QString key = normalizeLine(a.at(prefix + i + di), opt);
                const auto it = rightPositions.constFind(key);
                if (it == rightPositions.constEnd())
                    continue;
                for (int rj : it.value()) {
                    if (rj >= j && (di + (rj - j)) < bestDist) {
                        bestDist = di + (rj - j);
                        bestI = i + di;
                        bestJ = rj;
                    }
                }
            }
            if (bestI >= 0) {
                while (i < bestI)
                    result.append({EditOp::DeleteLeft, prefix + i++, -1});
                while (j < bestJ)
                    result.append({EditOp::InsertRight, -1, prefix + j++});
            } else {
                result.append({EditOp::DeleteLeft, prefix + i++, -1});
                result.append({EditOp::InsertRight, -1, prefix + j++});
            }
        }
        while (i < midN)
            result.append({EditOp::DeleteLeft, prefix + i++, -1});
        while (j < midM)
            result.append({EditOp::InsertRight, -1, prefix + j++});
    }

    for (int k = 0; k < suffix; ++k)
        result.append({EditOp::Equal, n - suffix + k, m - suffix + k});

    return result;
}

bool matchWildcard(const QString &pattern, const QString &text)
{
    // 简单高效的 * / ? 通配符匹配
    int pi = 0;
    int ti = 0;
    int starIdx = -1;
    int matchIdx = 0;
    while (ti < text.size()) {
        if (pi < pattern.size() && (pattern.at(pi) == QLatin1Char('?') || pattern.at(pi) == text.at(ti))) {
            ++pi;
            ++ti;
        } else if (pi < pattern.size() && pattern.at(pi) == QLatin1Char('*')) {
            starIdx = pi++;
            matchIdx = ti;
        } else if (starIdx != -1) {
            pi = starIdx + 1;
            ti = ++matchIdx;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern.at(pi) == QLatin1Char('*'))
        ++pi;
    return pi == pattern.size();
}

bool isIgnoredRelPath(const QString &relPath, const QStringList &patterns)
{
    if (patterns.isEmpty())
        return false;
    const QStringList parts = relPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &rawPat : patterns) {
        const QString pat = rawPat.trimmed();
        if (pat.isEmpty())
            continue;
        if (matchWildcard(pat, relPath))
            return true;
        for (const QString &part : parts) {
            if (matchWildcard(pat, part))
                return true;
        }
    }
    return false;
}

QByteArray fileSha256(const QString &path, bool *ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok)
            *ok = false;
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        if (ok)
            *ok = false;
        return {};
    }
    if (ok)
        *ok = true;
    return hash.result();
}

QString normIniKey(const QString &s, bool ignoreCase)
{
    return ignoreCase ? s.toLower() : s;
}

} // namespace

// ============================================================================
// 1. Text Diff
// ============================================================================

QStringList splitLines(const QString &text)
{
    if (text.isEmpty())
        return {};
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return normalized.split(QLatin1Char('\n'));
}

QString joinLines(const QStringList &lines)
{
    return lines.join(QLatin1Char('\n'));
}

void diffInlineChars(const QString &left, const QString &right, bool ignoreCase,
                     QVector<CharSpan> *leftSpans, QVector<CharSpan> *rightSpans)
{
    if (leftSpans)
        leftSpans->clear();
    if (rightSpans)
        rightSpans->clear();

    const int n = left.size();
    const int m = right.size();
    auto eq = [ignoreCase](QChar a, QChar b) {
        return ignoreCase ? (a.toLower() == b.toLower()) : (a == b);
    };

    int prefix = 0;
    while (prefix < n && prefix < m && eq(left.at(prefix), right.at(prefix)))
        ++prefix;

    int suffix = 0;
    while (suffix < (n - prefix) && suffix < (m - prefix)
           && eq(left.at(n - 1 - suffix), right.at(m - 1 - suffix))) {
        ++suffix;
    }

    const int leftMid = n - prefix - suffix;
    const int rightMid = m - prefix - suffix;
    if (leftSpans && leftMid > 0)
        leftSpans->append({prefix, leftMid});
    if (rightSpans && rightMid > 0)
        rightSpans->append({prefix, rightMid});
}

TextDiffResult diffLines(const QStringList &leftLines, const QStringList &rightLines,
                         const TextDiffOptions &options)
{
    TextDiffResult out;
    const QVector<EditStep> steps = computeSequenceDiff(leftLines, rightLines, options);

    int i = 0;
    while (i < steps.size()) {
        if (steps.at(i).op == EditOp::Equal) {
            DiffRow row;
            row.leftLineNo = steps.at(i).leftIdx + 1;
            row.rightLineNo = steps.at(i).rightIdx + 1;
            row.leftText = leftLines.at(steps.at(i).leftIdx);
            row.rightText = rightLines.at(steps.at(i).rightIdx);
            row.kind = DiffRowKind::Equal;
            row.hunkIndex = -1;
            out.rows.append(row);
            ++out.equalCount;
            ++i;
            continue;
        }

        // 收集连续的非 Equal 编辑步骤，配对成 Changed + LeftOnly/RightOnly
        QVector<int> delIndices;
        QVector<int> insIndices;
        while (i < steps.size() && steps.at(i).op != EditOp::Equal) {
            if (steps.at(i).op == EditOp::DeleteLeft)
                delIndices.append(steps.at(i).leftIdx);
            else
                insIndices.append(steps.at(i).rightIdx);
            ++i;
        }

        const int hunkIdx = out.hunks.size();
        DiffHunk hunk;
        hunk.index = hunkIdx;
        hunk.startRow = out.rows.size();
        hunk.leftLineCount = delIndices.size();
        hunk.rightLineCount = insIndices.size();

        if (!delIndices.isEmpty()) {
            hunk.leftStartLine = delIndices.first();
        } else {
            // 往前找最近的左侧行号
            int prevLeft = 0;
            for (int r = out.rows.size() - 1; r >= 0; --r) {
                if (out.rows.at(r).leftLineNo > 0) {
                    prevLeft = out.rows.at(r).leftLineNo;
                    break;
                }
            }
            hunk.leftStartLine = prevLeft;
        }

        if (!insIndices.isEmpty()) {
            hunk.rightStartLine = insIndices.first();
        } else {
            int prevRight = 0;
            for (int r = out.rows.size() - 1; r >= 0; --r) {
                if (out.rows.at(r).rightLineNo > 0) {
                    prevRight = out.rows.at(r).rightLineNo;
                    break;
                }
            }
            hunk.rightStartLine = prevRight;
        }

        const int paired = qMin(delIndices.size(), insIndices.size());
        for (int p = 0; p < paired; ++p) {
            DiffRow row;
            row.leftLineNo = delIndices.at(p) + 1;
            row.rightLineNo = insIndices.at(p) + 1;
            row.leftText = leftLines.at(delIndices.at(p));
            row.rightText = rightLines.at(insIndices.at(p));
            row.kind = DiffRowKind::Changed;
            row.hunkIndex = hunkIdx;
            diffInlineChars(row.leftText, row.rightText, options.ignoreCase,
                            &row.leftSpans, &row.rightSpans);
            out.rows.append(row);
            ++out.changedCount;
        }
        for (int p = paired; p < delIndices.size(); ++p) {
            DiffRow row;
            row.leftLineNo = delIndices.at(p) + 1;
            row.rightLineNo = -1;
            row.leftText = leftLines.at(delIndices.at(p));
            row.kind = DiffRowKind::LeftOnly;
            row.hunkIndex = hunkIdx;
            out.rows.append(row);
            ++out.leftOnlyCount;
        }
        for (int p = paired; p < insIndices.size(); ++p) {
            DiffRow row;
            row.leftLineNo = -1;
            row.rightLineNo = insIndices.at(p) + 1;
            row.rightText = rightLines.at(insIndices.at(p));
            row.kind = DiffRowKind::RightOnly;
            row.hunkIndex = hunkIdx;
            out.rows.append(row);
            ++out.rightOnlyCount;
        }

        hunk.endRow = out.rows.size() - 1;
        out.hunks.append(hunk);
    }

    return out;
}

QStringList applyHunkToRight(const QStringList &leftLines, const QStringList &rightLines,
                             const DiffHunk &hunk)
{
    QStringList out;
    const int rStart = qBound(0, hunk.rightStartLine, rightLines.size());
    const int rEnd = qBound(rStart, rStart + hunk.rightLineCount, rightLines.size());
    const int lStart = qBound(0, hunk.leftStartLine, leftLines.size());
    const int lEnd = qBound(lStart, lStart + hunk.leftLineCount, leftLines.size());

    for (int i = 0; i < rStart; ++i)
        out.append(rightLines.at(i));
    for (int i = lStart; i < lEnd; ++i)
        out.append(leftLines.at(i));
    for (int i = rEnd; i < rightLines.size(); ++i)
        out.append(rightLines.at(i));
    return out;
}

QStringList applyHunkToLeft(const QStringList &leftLines, const QStringList &rightLines,
                            const DiffHunk &hunk)
{
    QStringList out;
    const int lStart = qBound(0, hunk.leftStartLine, leftLines.size());
    const int lEnd = qBound(lStart, lStart + hunk.leftLineCount, leftLines.size());
    const int rStart = qBound(0, hunk.rightStartLine, rightLines.size());
    const int rEnd = qBound(rStart, rStart + hunk.rightLineCount, rightLines.size());

    for (int i = 0; i < lStart; ++i)
        out.append(leftLines.at(i));
    for (int i = rStart; i < rEnd; ++i)
        out.append(rightLines.at(i));
    for (int i = lEnd; i < leftLines.size(); ++i)
        out.append(leftLines.at(i));
    return out;
}

QString formatTextDiffReport(const QString &leftLabel, const QString &rightLabel,
                             const TextDiffResult &diff)
{
    QString report;
    QTextStream ts(&report);
    ts << "--- " << leftLabel << "\n";
    ts << "+++ " << rightLabel << "\n";
    ts << QStringLiteral("# 统计：相同 %1 行，修改 %2 行，仅左侧 %3 行，仅右侧 %4 行，差异块 %5 处\n")
              .arg(diff.equalCount)
              .arg(diff.changedCount)
              .arg(diff.leftOnlyCount)
              .arg(diff.rightOnlyCount)
              .arg(diff.hunks.size());
    for (const DiffHunk &h : diff.hunks) {
        ts << QStringLiteral("@@ -%1,%2 +%3,%4 @@\n")
                  .arg(h.leftStartLine + 1)
                  .arg(h.leftLineCount)
                  .arg(h.rightStartLine + 1)
                  .arg(h.rightLineCount);
        for (int r = h.startRow; r <= h.endRow && r < diff.rows.size(); ++r) {
            const DiffRow &row = diff.rows.at(r);
            if (row.kind == DiffRowKind::LeftOnly || row.kind == DiffRowKind::Changed)
                ts << "-" << row.leftText << "\n";
            if (row.kind == DiffRowKind::RightOnly || row.kind == DiffRowKind::Changed)
                ts << "+" << row.rightText << "\n";
        }
    }
    return report;
}

namespace {

bool isValidStrictUtf8(const QByteArray &bytes)
{
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const int len = bytes.size();
    int i = 0;
    while (i < len) {
        const unsigned char c = data[i];
        if (c <= 0x7F) {
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len || (data[i + 1] & 0xC0) != 0x80 || c < 0xC2)
                return false;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80)
                return false;
            if (c == 0xE0 && data[i + 1] < 0xA0)
                return false;
            if (c == 0xED && data[i + 1] >= 0xA0)
                return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80
                || (data[i + 3] & 0xC0) != 0x80)
                return false;
            if (c == 0xF0 && data[i + 1] < 0x90)
                return false;
            if (c > 0xF4 || (c == 0xF4 && data[i + 1] > 0x8F))
                return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

QTextCodec *codecForEncoding(TextEncoding enc)
{
    switch (enc) {
    case TextEncoding::Gb18030: {
        QTextCodec *c = QTextCodec::codecForName("GB18030");
        if (!c)
            c = QTextCodec::codecForName("GBK");
        return c ? c : QTextCodec::codecForName("UTF-8");
    }
    case TextEncoding::Utf16:
        return QTextCodec::codecForName("UTF-16");
    case TextEncoding::Auto:
    case TextEncoding::Utf8:
    default:
        return QTextCodec::codecForName("UTF-8");
    }
}

} // namespace

TextEncoding detectEncoding(const QByteArray &bytes)
{
    if (bytes.size() >= 3
        && static_cast<unsigned char>(bytes.at(0)) == 0xEF
        && static_cast<unsigned char>(bytes.at(1)) == 0xBB
        && static_cast<unsigned char>(bytes.at(2)) == 0xBF) {
        return TextEncoding::Utf8;
    }
    if (bytes.size() >= 2) {
        const unsigned char b0 = static_cast<unsigned char>(bytes.at(0));
        const unsigned char b1 = static_cast<unsigned char>(bytes.at(1));
        if ((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF))
            return TextEncoding::Utf16;
    }
    if (isValidStrictUtf8(bytes))
        return TextEncoding::Utf8;
    return TextEncoding::Gb18030;
}

QString encodingDisplayName(TextEncoding enc)
{
    switch (enc) {
    case TextEncoding::Auto: return QStringLiteral("自动探测");
    case TextEncoding::Utf8: return QStringLiteral("UTF-8");
    case TextEncoding::Gb18030: return QStringLiteral("GB18030/GBK");
    case TextEncoding::Utf16: return QStringLiteral("UTF-16");
    }
    return QStringLiteral("UTF-8");
}

QString decodeBytes(const QByteArray &bytes, TextEncoding requested, TextEncoding *detected)
{
    const TextEncoding actual = (requested == TextEncoding::Auto) ? detectEncoding(bytes) : requested;
    if (detected)
        *detected = actual;
    if (actual == TextEncoding::Utf8) {
        int offset = 0;
        if (bytes.size() >= 3
            && static_cast<unsigned char>(bytes.at(0)) == 0xEF
            && static_cast<unsigned char>(bytes.at(1)) == 0xBB
            && static_cast<unsigned char>(bytes.at(2)) == 0xBF) {
            offset = 3;
        }
        return QString::fromUtf8(bytes.constData() + offset, bytes.size() - offset);
    }
    QTextCodec *codec = codecForEncoding(actual);
    return codec ? codec->toUnicode(bytes) : QString::fromUtf8(bytes);
}

QByteArray encodeText(const QString &text, TextEncoding encoding)
{
    const TextEncoding actual = (encoding == TextEncoding::Auto) ? TextEncoding::Utf8 : encoding;
    if (actual == TextEncoding::Utf8)
        return text.toUtf8();
    QTextCodec *codec = codecForEncoding(actual);
    return codec ? codec->fromUnicode(text) : text.toUtf8();
}

bool loadTextFile(const QString &path, QString *content, QString *error,
                  TextEncoding encoding, TextEncoding *detectedEncoding)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("无法读取文件 %1：%2").arg(path, file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    const QString decoded = decodeBytes(bytes, encoding, detectedEncoding);
    if (content)
        *content = decoded;
    return true;
}

bool saveTextFileWithBackup(const QString &path, const QString &content, bool createBackup,
                            QString *backupPath, QString *error, TextEncoding encoding)
{
    if (path.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("保存路径不能为空");
        return false;
    }
    const QFileInfo fi(path);
    if (createBackup && fi.exists() && fi.isFile()) {
        const QString bak = path + QStringLiteral(".bak");
        if (QFile::exists(bak))
            QFile::remove(bak);
        if (!QFile::copy(path, bak)) {
            if (error)
                *error = QStringLiteral("创建备份文件失败：%1").arg(bak);
            return false;
        }
        if (backupPath)
            *backupPath = bak;
    } else if (backupPath) {
        backupPath->clear();
    }

    QDir parentDir = fi.dir();
    if (!parentDir.exists() && !parentDir.mkpath(QStringLiteral("."))) {
        if (error)
            *error = QStringLiteral("无法创建父目录：%1").arg(parentDir.absolutePath());
        return false;
    }

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("无法打开写入 %1：%2").arg(path, out.errorString());
        return false;
    }
    const QByteArray payload = encodeText(content, encoding);
    if (out.write(payload) != payload.size() || !out.commit()) {
        if (error)
            *error = QStringLiteral("写入文件失败 %1：%2").arg(path, out.errorString());
        return false;
    }
    return true;
}

QVector<CompareHistoryItem> loadCompareHistory(const QString &dataDir, const QString &category)
{
    QVector<CompareHistoryItem> items;
    if (dataDir.isEmpty() || category.isEmpty())
        return items;
    QFile file(QDir(dataDir).filePath(QStringLiteral("compare_history.json")));
    if (!file.open(QIODevice::ReadOnly))
        return items;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return items;
    const QJsonArray arr = doc.object().value(category).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject obj = v.toObject();
        const QString l = obj.value(QStringLiteral("left")).toString();
        const QString r = obj.value(QStringLiteral("right")).toString();
        if (!l.isEmpty() || !r.isEmpty())
            items.append({l, r});
    }
    return items;
}

bool recordCompareHistory(const QString &dataDir, const QString &category,
                          const QString &leftPath, const QString &rightPath, int maxItems)
{
    if (dataDir.isEmpty() || category.isEmpty() || (leftPath.isEmpty() && rightPath.isEmpty()))
        return false;
    const QString filePath = QDir(dataDir).filePath(QStringLiteral("compare_history.json"));

    QJsonObject root;
    {
        QFile in(filePath);
        if (in.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(in.readAll());
            if (doc.isObject())
                root = doc.object();
        }
    }

    QVector<CompareHistoryItem> list = loadCompareHistory(dataDir, category);
    for (int i = list.size() - 1; i >= 0; --i) {
        if (list.at(i).leftPath == leftPath && list.at(i).rightPath == rightPath)
            list.removeAt(i);
    }
    list.prepend({leftPath, rightPath});
    while (list.size() > qMax(1, maxItems))
        list.removeLast();

    QJsonArray arr;
    for (const CompareHistoryItem &item : list) {
        QJsonObject o;
        o.insert(QStringLiteral("left"), item.leftPath);
        o.insert(QStringLiteral("right"), item.rightPath);
        arr.append(o);
    }
    root.insert(category, arr);

    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (out.write(bytes) != bytes.size() || !out.commit())
        return false;
    QFile::setPermissions(filePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

QStringList parseIgnorePatterns(const QString &rawText)
{
    QString s = rawText;
    s.replace(QLatin1Char(','), QLatin1Char(';'));
    const QStringList raw = s.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    QStringList out;
    for (const QString &part : raw) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
            out.append(trimmed);
    }
    return out;
}

QString computeBytesMd5(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Md5).toHex());
}

QString computeTextMd5(const QString &text, TextEncoding encoding)
{
    return computeBytesMd5(encodeText(text, encoding));
}

QString computeFileMd5(const QString &path, bool *ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok)
            *ok = false;
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    if (!hash.addData(&file)) {
        if (ok)
            *ok = false;
        return {};
    }
    if (ok)
        *ok = true;
    return QString::fromLatin1(hash.result().toHex());
}

// ============================================================================
// 2. Three-Way Merge
// ============================================================================

QStringList MergeChunk::resolvedLines() const
{
    if (resolution == MergeResolution::UseLeft)
        return leftLines;
    if (resolution == MergeResolution::UseRight)
        return rightLines;
    if (resolution == MergeResolution::UseBase)
        return baseLines;
    if (resolution == MergeResolution::UseBothLR) {
        QStringList combined = leftLines;
        combined.append(rightLines);
        return combined;
    }
    if (resolution == MergeResolution::UseBothRL) {
        QStringList combined = rightLines;
        combined.append(leftLines);
        return combined;
    }

    // Auto
    switch (kind) {
    case MergeChunkKind::Equal:
        return baseLines;
    case MergeChunkKind::LeftChanged:
    case MergeChunkKind::BothSame:
        return leftLines;
    case MergeChunkKind::RightChanged:
        return rightLines;
    case MergeChunkKind::Conflict: {
        QStringList out;
        out.append(QStringLiteral("<<<<<<< 左侧 (LEFT)"));
        out.append(leftLines);
        out.append(QStringLiteral("======="));
        out.append(rightLines);
        out.append(QStringLiteral(">>>>>>> 右侧 (RIGHT)"));
        return out;
    }
    }
    return baseLines;
}

bool MergeChunk::isConflictUnresolved() const
{
    return kind == MergeChunkKind::Conflict && resolution == MergeResolution::Auto;
}

QString ThreeWayMergeResult::buildMergedText() const
{
    QStringList all;
    for (const MergeChunk &c : chunks)
        all.append(c.resolvedLines());
    return joinLines(all);
}

int ThreeWayMergeResult::unresolvedConflictCount() const
{
    int count = 0;
    for (const MergeChunk &c : chunks) {
        if (c.isConflictUnresolved())
            ++count;
    }
    return count;
}

ThreeWayMergeResult mergeThreeWay(const QStringList &baseLines,
                                  const QStringList &leftLines,
                                  const QStringList &rightLines,
                                  const TextDiffOptions &options)
{
    const int baseLen = baseLines.size();
    const QVector<EditStep> leftSteps = computeSequenceDiff(baseLines, leftLines, options);
    const QVector<EditStep> rightSteps = computeSequenceDiff(baseLines, rightLines, options);

    // 将相对 Base 的编辑映射为按 Base 行索引对齐的区间集合
    struct BaseEdit
    {
        int baseStart = 0;
        int baseEnd = 0; // exclusive
        QStringList replacement;
        int sideStartLine = 1;
    };

    auto extractEdits = [](const QVector<EditStep> &steps, const QStringList &sideLines) {
        QVector<BaseEdit> edits;
        int i = 0;
        int curBase = 0;
        int curSide = 0;
        while (i < steps.size()) {
            if (steps.at(i).op == EditOp::Equal) {
                curBase = steps.at(i).leftIdx + 1;
                curSide = steps.at(i).rightIdx + 1;
                ++i;
                continue;
            }
            BaseEdit e;
            e.baseStart = curBase;
            e.baseEnd = curBase;
            e.sideStartLine = curSide + 1;
            while (i < steps.size() && steps.at(i).op != EditOp::Equal) {
                if (steps.at(i).op == EditOp::DeleteLeft) {
                    curBase = steps.at(i).leftIdx + 1;
                    e.baseEnd = curBase;
                } else {
                    const int rIdx = steps.at(i).rightIdx;
                    e.replacement.append(sideLines.at(rIdx));
                    curSide = rIdx + 1;
                }
                ++i;
            }
            edits.append(e);
        }
        return edits;
    };

    const QVector<BaseEdit> lEdits = extractEdits(leftSteps, leftLines);
    const QVector<BaseEdit> rEdits = extractEdits(rightSteps, rightLines);

    // 构建每侧对 [baseStart, baseEnd) 的替换文本辅助函数
    auto sliceSide = [&](const QVector<BaseEdit> &edits, int bStart, int bEnd, int *outLine) {
        QStringList res;
        int pos = bStart;
        bool lineSet = false;
        for (const BaseEdit &e : edits) {
            if (e.baseEnd < bStart || (e.baseEnd == bStart && e.baseStart < bStart))
                continue;
            if (e.baseStart > bEnd || (e.baseStart == bEnd && e.baseEnd > bEnd))
                break;
            if (e.baseStart == e.baseEnd && (e.baseStart < bStart || e.baseStart > bEnd))
                continue;
            while (pos < e.baseStart && pos < bEnd)
                res.append(baseLines.at(pos++));
            if (!lineSet && outLine) {
                *outLine = e.sideStartLine;
                lineSet = true;
            }
            res.append(e.replacement);
            pos = qMax(pos, e.baseEnd);
        }
        while (pos < bEnd)
            res.append(baseLines.at(pos++));
        return res;
    };

    // 合并所有重叠的编辑区间为有序变更段
    struct Span { int start; int end; };
    QVector<Span> allSpans;
    for (const BaseEdit &e : lEdits)
        allSpans.append({e.baseStart, e.baseEnd});
    for (const BaseEdit &e : rEdits)
        allSpans.append({e.baseStart, e.baseEnd});

    std::sort(allSpans.begin(), allSpans.end(), [](const Span &a, const Span &b) {
        return a.start != b.start ? a.start < b.start : a.end < b.end;
    });

    QVector<Span> mergedSpans;
    for (const Span &sp : allSpans) {
        if (mergedSpans.isEmpty()) {
            mergedSpans.append(sp);
        } else {
            Span &last = mergedSpans.last();
            const bool overlaps = (sp.start < last.end)
                || (sp.start == sp.end && last.start == last.end && sp.start == last.start);
            if (overlaps)
                last.end = qMax(last.end, sp.end);
            else
                mergedSpans.append(sp);
        }
    }

    ThreeWayMergeResult result;
    int cursor = 0;
    auto pushEqual = [&](int from, int to) {
        if (from >= to)
            return;
        MergeChunk c;
        c.index = result.chunks.size();
        c.kind = MergeChunkKind::Equal;
        c.baseStartLine = from + 1;
        c.leftStartLine = from + 1;
        c.rightStartLine = from + 1;
        for (int k = from; k < to; ++k) {
            c.baseLines.append(baseLines.at(k));
            c.leftLines.append(baseLines.at(k));
            c.rightLines.append(baseLines.at(k));
        }
        result.chunks.append(c);
    };

    for (const Span &sp : mergedSpans) {
        pushEqual(cursor, sp.start);

        MergeChunk c;
        c.index = result.chunks.size();
        c.baseStartLine = sp.start + 1;
        c.leftStartLine = sp.start + 1;
        c.rightStartLine = sp.start + 1;
        for (int k = sp.start; k < sp.end; ++k)
            c.baseLines.append(baseLines.at(k));
        c.leftLines = sliceSide(lEdits, sp.start, sp.end, &c.leftStartLine);
        c.rightLines = sliceSide(rEdits, sp.start, sp.end, &c.rightStartLine);

        const bool leftChanged = (c.leftLines != c.baseLines);
        const bool rightChanged = (c.rightLines != c.baseLines);
        if (!leftChanged && !rightChanged) {
            c.kind = MergeChunkKind::Equal;
        } else if (leftChanged && !rightChanged) {
            c.kind = MergeChunkKind::LeftChanged;
            ++result.autoMergedCount;
        } else if (!leftChanged && rightChanged) {
            c.kind = MergeChunkKind::RightChanged;
            ++result.autoMergedCount;
        } else if (c.leftLines == c.rightLines) {
            c.kind = MergeChunkKind::BothSame;
            ++result.autoMergedCount;
        } else {
            c.kind = MergeChunkKind::Conflict;
            ++result.conflictCount;
        }
        result.chunks.append(c);
        cursor = sp.end;
    }
    pushEqual(cursor, baseLen);
    return result;
}

// ============================================================================
// 3. INI Config Diff
// ============================================================================

IniDocument parseIni(const QString &text)
{
    IniDocument doc;
    const QStringList rawLines = splitLines(text);
    QString currentSection;

    for (int idx = 0; idx < rawLines.size(); ++idx) {
        const QString &raw = rawLines.at(idx);
        const QString trimmed = raw.trimmed();
        IniLine item;
        item.rawText = raw;
        item.section = currentSection;
        item.lineNo = idx + 1;

        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))
            || trimmed.startsWith(QLatin1Char(';'))) {
            item.type = IniLine::Type::BlankOrComment;
            doc.lines.append(item);
            continue;
        }

        if (trimmed.startsWith(QLatin1Char('['))) {
            const int closePos = trimmed.indexOf(QLatin1Char(']'));
            if (closePos > 1) {
                currentSection = trimmed.mid(1, closePos - 1).trimmed();
                item.type = IniLine::Type::Section;
                item.section = currentSection;
                doc.lines.append(item);
                continue;
            }
        }

        const int eqPos = raw.indexOf(QLatin1Char('='));
        if (eqPos > 0) {
            item.type = IniLine::Type::KeyValue;
            item.key = raw.left(eqPos).trimmed();
            const QString rhs = raw.mid(eqPos + 1);
            // 保留行尾注释（仅当注释符前有空白，避免截断含 # 的值）
            int commentPos = -1;
            for (int c = 1; c < rhs.size(); ++c) {
                if ((rhs.at(c) == QLatin1Char('#') || rhs.at(c) == QLatin1Char(';'))
                    && rhs.at(c - 1).isSpace()) {
                    commentPos = c;
                    break;
                }
            }
            if (commentPos >= 0) {
                item.value = rhs.left(commentPos).trimmed();
                item.inlineComment = rhs.mid(commentPos);
            } else {
                item.value = rhs.trimmed();
            }
            doc.lines.append(item);
        } else {
            item.type = IniLine::Type::BlankOrComment;
            doc.lines.append(item);
        }
    }
    return doc;
}

QStringList IniDocument::sections() const
{
    QStringList out;
    QSet<QString> seen;
    for (const IniLine &l : lines) {
        if (l.type == IniLine::Type::Section || l.type == IniLine::Type::KeyValue) {
            if (!seen.contains(l.section)) {
                seen.insert(l.section);
                out.append(l.section);
            }
        }
    }
    return out;
}

bool IniDocument::hasKey(const QString &section, const QString &key, bool ignoreCase) const
{
    const QString sNorm = normIniKey(section, ignoreCase);
    const QString kNorm = normIniKey(key, ignoreCase);
    for (const IniLine &l : lines) {
        if (l.type == IniLine::Type::KeyValue
            && normIniKey(l.section, ignoreCase) == sNorm
            && normIniKey(l.key, ignoreCase) == kNorm) {
            return true;
        }
    }
    return false;
}

QString IniDocument::value(const QString &section, const QString &key, bool ignoreCase) const
{
    const QString sNorm = normIniKey(section, ignoreCase);
    const QString kNorm = normIniKey(key, ignoreCase);
    for (const IniLine &l : lines) {
        if (l.type == IniLine::Type::KeyValue
            && normIniKey(l.section, ignoreCase) == sNorm
            && normIniKey(l.key, ignoreCase) == kNorm) {
            return l.value;
        }
    }
    return {};
}

void IniDocument::setValue(const QString &section, const QString &key, const QString &val,
                           bool ignoreCase)
{
    const QString sNorm = normIniKey(section, ignoreCase);
    const QString kNorm = normIniKey(key, ignoreCase);
    int lastSectionLine = -1;

    for (int i = 0; i < lines.size(); ++i) {
        IniLine &l = lines[i];
        if (normIniKey(l.section, ignoreCase) == sNorm) {
            if (l.type == IniLine::Type::Section || l.type == IniLine::Type::KeyValue)
                lastSectionLine = i;
            if (l.type == IniLine::Type::KeyValue && normIniKey(l.key, ignoreCase) == kNorm) {
                l.value = val;
                l.rawText = l.inlineComment.isEmpty()
                    ? QStringLiteral("%1 = %2").arg(l.key, val)
                    : QStringLiteral("%1 = %2 %3").arg(l.key, val, l.inlineComment);
                return;
            }
        }
    }

    IniLine newItem;
    newItem.type = IniLine::Type::KeyValue;
    newItem.section = section;
    newItem.key = key;
    newItem.value = val;
    newItem.rawText = QStringLiteral("%1 = %2").arg(key, val);

    if (lastSectionLine >= 0) {
        lines.insert(lastSectionLine + 1, newItem);
        return;
    }

    if (!section.isEmpty()) {
        if (!lines.isEmpty() && !lines.last().rawText.trimmed().isEmpty()) {
            IniLine blank;
            blank.type = IniLine::Type::BlankOrComment;
            lines.append(blank);
        }
        IniLine secLine;
        secLine.type = IniLine::Type::Section;
        secLine.section = section;
        secLine.rawText = QStringLiteral("[%1]").arg(section);
        lines.append(secLine);
    }
    lines.append(newItem);
}

bool IniDocument::removeKey(const QString &section, const QString &key, bool ignoreCase)
{
    const QString sNorm = normIniKey(section, ignoreCase);
    const QString kNorm = normIniKey(key, ignoreCase);
    for (int i = 0; i < lines.size(); ++i) {
        const IniLine &l = lines.at(i);
        if (l.type == IniLine::Type::KeyValue
            && normIniKey(l.section, ignoreCase) == sNorm
            && normIniKey(l.key, ignoreCase) == kNorm) {
            lines.removeAt(i);
            return true;
        }
    }
    return false;
}

QString IniDocument::serialize() const
{
    QStringList out;
    out.reserve(lines.size());
    for (const IniLine &l : lines)
        out.append(l.rawText);
    return joinLines(out);
}

IniDiffResult diffIni(const IniDocument &leftDoc, const IniDocument &rightDoc, bool ignoreCase)
{
    IniDiffResult res;
    struct KeyRecord
    {
        QString section;
        QString key;
        bool hasLeft = false;
        bool hasRight = false;
        QString leftVal;
        QString rightVal;
    };

    QVector<QString> orderKeys;
    QHash<QString, KeyRecord> records;
    QSet<QString> sectionSeen;

    auto addSection = [&](const QString &sec) {
        const QString norm = normIniKey(sec, ignoreCase);
        if (!sectionSeen.contains(norm)) {
            sectionSeen.insert(norm);
            res.sections.append(sec);
        }
    };

    auto makeComposite = [ignoreCase](const QString &sec, const QString &key) {
        return normIniKey(sec, ignoreCase) + QLatin1Char('\x1f') + normIniKey(key, ignoreCase);
    };

    for (const IniLine &l : leftDoc.lines) {
        if (l.type != IniLine::Type::KeyValue)
            continue;
        addSection(l.section);
        const QString id = makeComposite(l.section, l.key);
        if (!records.contains(id)) {
            orderKeys.append(id);
            records.insert(id, {l.section, l.key, true, false, l.value, QString()});
        } else {
            records[id].hasLeft = true;
            records[id].leftVal = l.value;
        }
    }

    for (const IniLine &r : rightDoc.lines) {
        if (r.type != IniLine::Type::KeyValue)
            continue;
        addSection(r.section);
        const QString id = makeComposite(r.section, r.key);
        if (!records.contains(id)) {
            orderKeys.append(id);
            records.insert(id, {r.section, r.key, false, true, QString(), r.value});
        } else {
            records[id].hasRight = true;
            records[id].rightVal = r.value;
        }
    }

    for (const QString &id : orderKeys) {
        const KeyRecord &rec = records.value(id);
        IniDiffEntry e;
        e.section = rec.section;
        e.key = rec.key;
        e.leftValue = rec.leftVal;
        e.rightValue = rec.rightVal;
        if (rec.hasLeft && !rec.hasRight) {
            e.kind = IniDiffKind::LeftOnly;
            ++res.leftOnlyCount;
        } else if (!rec.hasLeft && rec.hasRight) {
            e.kind = IniDiffKind::RightOnly;
            ++res.rightOnlyCount;
        } else {
            const bool equal = ignoreCase
                ? (rec.leftVal.compare(rec.rightVal, Qt::CaseInsensitive) == 0)
                : (rec.leftVal == rec.rightVal);
            if (equal) {
                e.kind = IniDiffKind::Equal;
                ++res.equalCount;
            } else {
                e.kind = IniDiffKind::Changed;
                ++res.changedCount;
            }
        }
        res.entries.append(e);
    }
    return res;
}

QString formatIniDiffReport(const QString &leftLabel, const QString &rightLabel,
                            const IniDiffResult &diff)
{
    QString report;
    QTextStream ts(&report);
    ts << QStringLiteral("# INI 配置比较报告\n");
    ts << QStringLiteral("左侧：%1\n右侧：%2\n").arg(leftLabel, rightLabel);
    ts << QStringLiteral("统计：相同 %1，值不同 %2，仅左侧 %3，仅右侧 %4\n\n")
              .arg(diff.equalCount)
              .arg(diff.changedCount)
              .arg(diff.leftOnlyCount)
              .arg(diff.rightOnlyCount);
    ts << QStringLiteral("节\t键名\t状态\t左侧值\t右侧值\n");
    for (const IniDiffEntry &e : diff.entries) {
        if (e.kind == IniDiffKind::Equal)
            continue;
        const QString status = e.kind == IniDiffKind::Changed
            ? QStringLiteral("值不同")
            : (e.kind == IniDiffKind::LeftOnly ? QStringLiteral("仅左侧") : QStringLiteral("仅右侧"));
        ts << QStringLiteral("[%1]\t%2\t%3\t%4\t%5\n")
                  .arg(e.section.isEmpty() ? QStringLiteral("全局") : e.section,
                       e.key, status, e.leftValue, e.rightValue);
    }
    return report;
}

// ============================================================================
// 4. Folder Compare
// ============================================================================

FolderCompareResult compareFolders(
    const QString &leftDir,
    const QString &rightDir,
    const FolderCompareOptions &options,
    const std::function<bool()> &isCanceled,
    const std::function<void(int percent, const QString &status)> &onProgress)
{
    QElapsedTimer timer;
    timer.start();

    FolderCompareResult result;
    result.mode = options.mode;
    QMap<QString, QFileInfo> leftMap;
    QMap<QString, QFileInfo> rightMap;

    auto scanDir = [&](const QString &rootPath, QMap<QString, QFileInfo> &outMap) {
        QDir root(rootPath);
        if (!root.exists())
            return;
        QDirIterator it(rootPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (isCanceled && isCanceled()) {
                result.canceled = true;
                return;
            }
            it.next();
            const QFileInfo fi = it.fileInfo();
            const QString rel = QDir::cleanPath(root.relativeFilePath(fi.filePath()));
            if (isIgnoredRelPath(rel, options.ignorePatterns))
                continue;
            outMap.insert(rel, fi);
        }
    };

    if (onProgress)
        onProgress(5, QStringLiteral("正在扫描左侧目录..."));
    scanDir(leftDir, leftMap);
    if (result.canceled)
        return result;

    if (onProgress)
        onProgress(20, QStringLiteral("正在扫描右侧目录..."));
    scanDir(rightDir, rightMap);
    if (result.canceled)
        return result;

    QSet<QString> keySet;
    for (auto it = leftMap.constBegin(); it != leftMap.constEnd(); ++it)
        keySet.insert(it.key());
    for (auto it = rightMap.constBegin(); it != rightMap.constEnd(); ++it)
        keySet.insert(it.key());

    QStringList allRel = keySet.values();
    std::sort(allRel.begin(), allRel.end());

    const int total = allRel.size();
    result.entries.reserve(total);

    for (int idx = 0; idx < total; ++idx) {
        if (isCanceled && isCanceled()) {
            result.canceled = true;
            return result;
        }
        const QString &rel = allRel.at(idx);
        const bool hasL = leftMap.contains(rel);
        const bool hasR = rightMap.contains(rel);

        FolderEntry e;
        e.relPath = rel;
        e.isDir = false;
        e.leftExists = hasL;
        e.rightExists = hasR;
        if (hasL) {
            const QFileInfo &fi = leftMap.value(rel);
            e.leftSize = fi.size();
            e.leftMtime = fi.lastModified();
            if (options.mode == FolderCompareMode::ContentMd5)
                e.leftMd5 = computeFileMd5(fi.filePath());
        }
        if (hasR) {
            const QFileInfo &fi = rightMap.value(rel);
            e.rightSize = fi.size();
            e.rightMtime = fi.lastModified();
            if (options.mode == FolderCompareMode::ContentMd5)
                e.rightMd5 = computeFileMd5(fi.filePath());
        }

        if (hasL && !hasR) {
            e.status = FolderEntryStatus::LeftOnly;
            ++result.leftOnlyCount;
        } else if (!hasL && hasR) {
            e.status = FolderEntryStatus::RightOnly;
            ++result.rightOnlyCount;
        } else {
            bool same = false;
            if (options.mode == FolderCompareMode::ContentMd5) {
                same = !e.leftMd5.isEmpty() && (e.leftMd5 == e.rightMd5);
            } else if (options.mode == FolderCompareMode::ContentSha256) {
                if (e.leftSize == e.rightSize) {
                    bool okL = false;
                    bool okR = false;
                    const QByteArray hL = fileSha256(leftMap.value(rel).filePath(), &okL);
                    const QByteArray hR = fileSha256(rightMap.value(rel).filePath(), &okR);
                    same = okL && okR && (hL == hR);
                }
            } else {
                const qint64 dt = qAbs(e.leftMtime.secsTo(e.rightMtime));
                same = (e.leftSize == e.rightSize) && (dt <= options.timeToleranceSecs);
            }

            if (same) {
                e.status = FolderEntryStatus::Same;
                ++result.sameCount;
            } else {
                const qint64 secs = e.leftMtime.secsTo(e.rightMtime);
                if (secs < -options.timeToleranceSecs)
                    e.status = FolderEntryStatus::LeftNewer;
                else if (secs > options.timeToleranceSecs)
                    e.status = FolderEntryStatus::RightNewer;
                else
                    e.status = FolderEntryStatus::Different;
                ++result.diffCount;
            }
        }
        result.entries.append(e);

        if (onProgress && total > 0 && (idx % 20 == 0 || idx == total - 1)) {
            const int pct = 20 + (idx + 1) * 80 / total;
            onProgress(pct, QStringLiteral("正在比对文件 (%1/%2)").arg(idx + 1).arg(total));
        }
    }

    if (onProgress && total == 0)
        onProgress(100, QStringLiteral("目录扫描完成"));

    result.elapsedMs = timer.elapsed();
    return result;
}

bool copyFileWithBackup(const QString &srcPath, const QString &dstPath, bool createBackup,
                        QString *backupPath, QString *error)
{
    if (!QFileInfo::exists(srcPath)) {
        if (error)
            *error = QStringLiteral("源文件不存在：%1").arg(srcPath);
        return false;
    }
    const QFileInfo dstInfo(dstPath);
    QDir parentDir = dstInfo.dir();
    if (!parentDir.exists() && !parentDir.mkpath(QStringLiteral("."))) {
        if (error)
            *error = QStringLiteral("无法创建目标目录：%1").arg(parentDir.absolutePath());
        return false;
    }
    if (createBackup && dstInfo.exists() && dstInfo.isFile()) {
        const QString bak = dstPath + QStringLiteral(".bak");
        if (QFile::exists(bak))
            QFile::remove(bak);
        if (!QFile::copy(dstPath, bak)) {
            if (error)
                *error = QStringLiteral("创建备份失败：%1").arg(bak);
            return false;
        }
        if (backupPath)
            *backupPath = bak;
    } else if (backupPath) {
        backupPath->clear();
    }

    if (QFile::exists(dstPath) && !QFile::remove(dstPath)) {
        if (error)
            *error = QStringLiteral("无法覆盖目标文件：%1").arg(dstPath);
        return false;
    }
    if (!QFile::copy(srcPath, dstPath)) {
        if (error)
            *error = QStringLiteral("复制文件失败：%1 -> %2").arg(srcPath, dstPath);
        return false;
    }
    return true;
}

QString formatFolderDiffReport(const QString &leftDir, const QString &rightDir,
                               const FolderCompareResult &result)
{
    QString report;
    QTextStream ts(&report);
    const int total = result.entries.size();
    const double samePct = total > 0 ? (100.0 * result.sameCount / total) : 100.0;
    const bool showMd5 = (result.mode == FolderCompareMode::ContentMd5);

    ts << QStringLiteral("# 文件夹比较汇总报告\n");
    ts << QStringLiteral("左侧目录：%1\n右侧目录：%2\n").arg(leftDir, rightDir);
    ts << QStringLiteral("汇总统计：总计 %1 项，相同 %2 项（%3%），不同 %4 项，仅左侧 %5 项，仅右侧 %6 项，耗时 %7 ms\n\n")
              .arg(total)
              .arg(result.sameCount)
              .arg(QString::number(samePct, 'f', 1))
              .arg(result.diffCount)
              .arg(result.leftOnlyCount)
              .arg(result.rightOnlyCount)
              .arg(result.elapsedMs);
    if (showMd5)
        ts << QStringLiteral("相对路径\t状态\t左侧大小\t左侧MD5\t右侧大小\t右侧MD5\n");
    else
        ts << QStringLiteral("相对路径\t状态\t左侧大小\t右侧大小\n");
    for (const FolderEntry &e : result.entries) {
        QString st;
        switch (e.status) {
        case FolderEntryStatus::Same: st = QStringLiteral("相同"); break;
        case FolderEntryStatus::Different: st = QStringLiteral("内容不同"); break;
        case FolderEntryStatus::LeftNewer: st = QStringLiteral("左侧较新"); break;
        case FolderEntryStatus::RightNewer: st = QStringLiteral("右侧较新"); break;
        case FolderEntryStatus::LeftOnly: st = QStringLiteral("仅左侧"); break;
        case FolderEntryStatus::RightOnly: st = QStringLiteral("仅右侧"); break;
        }
        if (showMd5) {
            ts << QStringLiteral("%1\t%2\t%3\t%4\t%5\t%6\n")
                      .arg(e.relPath, st)
                      .arg(e.leftExists ? QString::number(e.leftSize) : QStringLiteral("-"))
                      .arg(e.leftMd5.isEmpty() ? QStringLiteral("-") : e.leftMd5)
                      .arg(e.rightExists ? QString::number(e.rightSize) : QStringLiteral("-"))
                      .arg(e.rightMd5.isEmpty() ? QStringLiteral("-") : e.rightMd5);
        } else {
            ts << QStringLiteral("%1\t%2\t%3\t%4\n")
                      .arg(e.relPath, st)
                      .arg(e.leftExists ? QString::number(e.leftSize) : QStringLiteral("-"))
                      .arg(e.rightExists ? QString::number(e.rightSize) : QStringLiteral("-"));
        }
    }
    return report;
}

} // namespace cmp
} // namespace tw
