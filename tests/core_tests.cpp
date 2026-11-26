#include "core/app_info.h"
#include "core/data_paths.h"
#include "core/log_buffer.h"
#include "core/navigation_gate.h"
#include "core/platform_services.h"
#include "core/status_board.h"
#include "core/tool_info.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

using namespace tw;

class CoreTests : public QObject
{
    Q_OBJECT

private slots:
    void identifiersAcceptOnlyStableNames_data();
    void identifiersAcceptOnlyStableNames();

    void appInfoIsSelfContained();

    void dataPathsStayInsideTheirRoot();
    void dataPathsRejectSystemRoots();
    void dataPathsRejectSymlinkToSystemRoot();
    void dataPathsEnforcePrivatePermissions();
    void dataPathsRelativeRootBecomesAbsolute();
    void standardPathsHonourOverrideVariable();
    void standardPathsDefaultIsIsolated();

    void logBufferKeepsNewestEntries();
    void logBufferFiltersByTool();
    void logBufferFormatsReadableLines();

    void statusBoardStartsIdleAndIsPerTool();
    void statusBoardBlankTextMeansIdle();
    void statusBoardTimeoutReturnsToIdle();
    void statusBoardNewerMessageSurvivesOlderTimer();
    void statusBoardDefaultTimeoutsByState();

    void gateRefCountsLocks();
    void gateScopedLockReleasesOnDestruction();
    void gateScopedLockIsMovable();
    void gateReleaseIsIdempotent();
    void gateReleaseAllForOwner();
    void gateOutlivedLockIsHarmless();

    void toolServicesTagEverythingWithTheirTool();
    void toolServicesDataDirsAreIsolated();
    void toolServicesCrossThreadCallsAreSafe();
    void toolServicesAfterPlatformIsGoneAreNoOps();
};

void CoreTests::identifiersAcceptOnlyStableNames_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<bool>("valid");

    QTest::newRow("simple") << "welcome" << true;
    QTest::newRow("dash") << "task-demo" << true;
    QTest::newRow("dot") << "deploy.history" << true;
    QTest::newRow("digits") << "tool2" << true;
    QTest::newRow("empty") << "" << false;
    QTest::newRow("upper") << "Welcome" << false;
    QTest::newRow("leading-digit") << "2tool" << false;
    QTest::newRow("leading-dash") << "-tool" << false;
    QTest::newRow("trailing-dash") << "tool-" << false;
    QTest::newRow("double-dot") << "a..b" << false;
    QTest::newRow("slash") << "a/b" << false;
    QTest::newRow("traversal") << "../etc" << false;
    QTest::newRow("space") << "a b" << false;
    QTest::newRow("underscore") << "a_b" << false;
    QTest::newRow("chinese") << QStringLiteral("工具") << false;
    QTest::newRow("too-long") << QString(49, QLatin1Char('a')) << false;
    QTest::newRow("max-length") << QString(48, QLatin1Char('a')) << true;
}

void CoreTests::identifiersAcceptOnlyStableNames()
{
    QFETCH(QString, id);
    QFETCH(bool, valid);
    QCOMPARE(isValidIdentifier(id), valid);
}

void CoreTests::appInfoIsSelfContained()
{
    QCOMPARE(AppInfo::internalName(), QStringLiteral("TengWangge"));
    QCOMPARE(AppInfo::productName(), QStringLiteral("滕王阁"));
    QVERIFY(!AppInfo::version().isEmpty());
    QVERIFY(isValidIdentifier(AppInfo::internalName().toLower()));
    QCOMPARE(AppInfo::dataDirName(), AppInfo::internalName());
}

void CoreTests::dataPathsStayInsideTheirRoot()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const DataPaths paths(temp.filePath(QStringLiteral("root")));

    QString error;
    const QString welcome = paths.toolDataDir(QStringLiteral("welcome"), &error);
    QVERIFY2(!welcome.isEmpty(), qPrintable(error));
    QVERIFY(QFileInfo(welcome).isDir());
    QVERIFY(welcome.startsWith(paths.root() + QLatin1Char('/')));
    QVERIFY(welcome.endsWith(QStringLiteral("/tools/welcome")));

    const QString other = paths.toolDataDir(QStringLiteral("task-demo"), &error);
    QVERIFY(!other.isEmpty());
    QVERIFY(other != welcome);

    for (const QString &bad : {QStringLiteral("../escape"), QStringLiteral("a/b"),
                               QStringLiteral(""), QStringLiteral("Upper")}) {
        error.clear();
        QVERIFY2(paths.toolDataDir(bad, &error).isEmpty(), qPrintable(bad));
        QVERIFY(!error.isEmpty());
    }
    QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral("escape"))));

    QCOMPARE(paths.logDir(), paths.root() + QStringLiteral("/logs"));
}

void CoreTests::dataPathsRejectSystemRoots()
{
    for (const QString &root : {QStringLiteral("/"), QStringLiteral("/usr"),
                                QStringLiteral("/usr/local/share/x"), QStringLiteral("/etc"),
                                QStringLiteral("/etc/tengwangge"), QStringLiteral("/boot"),
                                QStringLiteral("/lib64/x"), QStringLiteral("")}) {
        QVERIFY2(!DataPaths::rootProblem(root).isEmpty(), qPrintable(root));
        QString error;
        QVERIFY2(DataPaths(root).toolDataDir(QStringLiteral("welcome"), &error).isEmpty(),
                 qPrintable(root));
        QVERIFY(!error.isEmpty());
    }
    QVERIFY(!QFileInfo::exists(QStringLiteral("/usr/tools/welcome")));
    QVERIFY(DataPaths::rootProblem(QStringLiteral("/home/someone/.local/share/TengWangge")).isEmpty());
    QVERIFY(DataPaths::rootProblem(QStringLiteral("/tmp/tw")).isEmpty());
    QVERIFY(DataPaths::rootProblem(QStringLiteral("/usrlocal/x")).isEmpty());
}

void CoreTests::dataPathsRejectSymlinkToSystemRoot()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString linkPath = temp.filePath(QStringLiteral("etc-link"));
    QVERIFY(QFile::link(QStringLiteral("/etc"), linkPath));

    QVERIFY(!DataPaths::rootProblem(linkPath).isEmpty());
    QVERIFY(!DataPaths::rootProblem(linkPath + QStringLiteral("/nested/data")).isEmpty());

    QString error;
    QVERIFY(DataPaths(linkPath + QStringLiteral("/nested")).toolDataDir(QStringLiteral("welcome"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void CoreTests::dataPathsEnforcePrivatePermissions()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.filePath(QStringLiteral("preexisting"));
    QVERIFY(QDir().mkpath(root));
    QFile::setPermissions(root, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                    | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                    | QFileDevice::ReadOther | QFileDevice::ExeOther);

    const DataPaths paths(root);
    QString error;
    const QString toolDir = paths.toolDataDir(QStringLiteral("welcome"), &error);
    QVERIFY2(!toolDir.isEmpty(), qPrintable(error));
    const QString logDir = paths.ensureLogDir(&error);
    QVERIFY2(!logDir.isEmpty(), qPrintable(error));

    constexpr QFileDevice::Permissions forbidden =
        QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;

    for (const QString &dir : {paths.root(), paths.root() + QStringLiteral("/tools"), toolDir, logDir}) {
        const QFileDevice::Permissions perms = QFileInfo(dir).permissions();
        QVERIFY2((perms & forbidden) == 0, qPrintable(dir));
    }
}

void CoreTests::dataPathsRelativeRootBecomesAbsolute()
{
    const DataPaths paths(QStringLiteral("relative/data"));
    QVERIFY(QDir::isAbsolutePath(paths.root()));
    QVERIFY(paths.root().endsWith(QStringLiteral("/relative/data")));
}

void CoreTests::standardPathsHonourOverrideVariable()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QByteArray previous = qgetenv(DataPaths::overrideVariable());
    qputenv(DataPaths::overrideVariable(), temp.path().toUtf8());
    const DataPaths paths = DataPaths::standard();
    QCOMPARE(paths.root(), QDir::cleanPath(temp.path()));
    if (previous.isNull())
        qunsetenv(DataPaths::overrideVariable());
    else
        qputenv(DataPaths::overrideVariable(), previous);
}

void CoreTests::standardPathsDefaultIsIsolated()
{
    const QByteArray previous = qgetenv(DataPaths::overrideVariable());
    qunsetenv(DataPaths::overrideVariable());
    const DataPaths paths = DataPaths::standard();
    QVERIFY(paths.root().endsWith(QStringLiteral("/TengWangge")));
    QVERIFY(!paths.root().contains(QStringLiteral("platformToolNew"), Qt::CaseInsensitive));
    QVERIFY(QDir::isAbsolutePath(paths.root()));
    QVERIFY(DataPaths::rootProblem(paths.root()).isEmpty());
    if (!previous.isNull())
        qputenv(DataPaths::overrideVariable(), previous);
}

void CoreTests::logBufferKeepsNewestEntries()
{
    LogBuffer buffer(3);
    QSignalSpy spy(&buffer, &LogBuffer::entryAppended);
    for (int i = 0; i < 5; ++i)
        buffer.append(LogLevel::Info, QStringLiteral("a"), QStringLiteral("m%1").arg(i));
    QCOMPARE(spy.count(), 5);
    QCOMPARE(buffer.size(), 3);
    const QVector<LogEntry> all = buffer.entries();
    QCOMPARE(all.size(), 3);
    QCOMPARE(all.first().message, QStringLiteral("m2"));
    QCOMPARE(all.last().message, QStringLiteral("m4"));

    QSignalSpy cleared(&buffer, &LogBuffer::cleared);
    buffer.clear();
    QCOMPARE(cleared.count(), 1);
    QCOMPARE(buffer.size(), 0);
    LogBuffer minimum(0);
    QCOMPARE(minimum.capacity(), 1);
}

void CoreTests::logBufferFiltersByTool()
{
    LogBuffer buffer;
    buffer.append(LogLevel::Info, QString(), QStringLiteral("shell"));
    buffer.append(LogLevel::Info, QStringLiteral("a"), QStringLiteral("from a"));
    buffer.append(LogLevel::Error, QStringLiteral("b"), QStringLiteral("from b"));

    QCOMPARE(buffer.entries().size(), 3);
    QCOMPARE(buffer.entries(QStringLiteral("a")).size(), 1);
    QCOMPARE(buffer.entries(QStringLiteral("a"), true).size(), 2);
    QCOMPARE(buffer.entries(QStringLiteral("b")).first().message, QStringLiteral("from b"));
    QCOMPARE(buffer.entries(QStringLiteral("missing"), true).size(), 1);
}

void CoreTests::logBufferFormatsReadableLines()
{
    LogEntry entry;
    entry.time = QDateTime(QDate(2026, 11, 15), QTime(12, 30, 5));
    entry.level = LogLevel::Error;
    entry.message = QStringLiteral("失败");
    QCOMPARE(LogBuffer::format(entry), QStringLiteral("2026-11-15 12:30:05  [错误]  失败"));
    QCOMPARE(LogBuffer::levelName(LogLevel::Success), QStringLiteral("成功"));
    QCOMPARE(LogBuffer::levelName(LogLevel::Warning), QStringLiteral("警告"));
}

void CoreTests::statusBoardStartsIdleAndIsPerTool()
{
    StatusBoard board;
    QCOMPARE(board.current(QStringLiteral("a")).state, StatusState::Idle);
    QCOMPARE(board.current(QStringLiteral("a")).text, StatusBoard::idleText());

    QSignalSpy spy(&board, &StatusBoard::changed);
    board.set(QStringLiteral("a"), StatusState::Busy, QStringLiteral("working"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("a"));
    QCOMPARE(board.current(QStringLiteral("a")).state, StatusState::Busy);
    QCOMPARE(board.current(QStringLiteral("a")).text, QStringLiteral("working"));
    QCOMPARE(board.current(QStringLiteral("b")).state, StatusState::Idle);
}

void CoreTests::statusBoardBlankTextMeansIdle()
{
    StatusBoard board;
    board.set(QStringLiteral("a"), StatusState::Error, QStringLiteral("   "));
    QCOMPARE(board.current(QStringLiteral("a")).state, StatusState::Idle);
    QCOMPARE(board.current(QStringLiteral("a")).text, StatusBoard::idleText());

    board.set(QStringLiteral("a"), StatusState::Error, QStringLiteral("boom"));
    board.clear(QStringLiteral("a"));
    QCOMPARE(board.current(QStringLiteral("a")).state, StatusState::Idle);
}

void CoreTests::statusBoardTimeoutReturnsToIdle()
{
    StatusBoard board;
    board.set(QStringLiteral("a"), StatusState::Info, QStringLiteral("hello"), 30);
    QCOMPARE(board.current(QStringLiteral("a")).state, StatusState::Info);
    QTRY_COMPARE_WITH_TIMEOUT(board.current(QStringLiteral("a")).state, StatusState::Idle, 1000);
    QCOMPARE(board.current(QStringLiteral("a")).text, StatusBoard::idleText());

    board.set(QStringLiteral("a"), StatusState::Error, QStringLiteral("sticky"), 0);
    QTest::qWait(80);
    QCOMPARE(board.current(QStringLiteral("a")).state, StatusState::Error);
}

void CoreTests::statusBoardNewerMessageSurvivesOlderTimer()
{
    StatusBoard board;
    board.set(QStringLiteral("a"), StatusState::Info, QStringLiteral("old"), 40);
    board.set(QStringLiteral("a"), StatusState::Busy, QStringLiteral("new"), 0);
    QTest::qWait(120);
    QCOMPARE(board.current(QStringLiteral("a")).state, StatusState::Busy);
    QCOMPARE(board.current(QStringLiteral("a")).text, QStringLiteral("new"));
}

void CoreTests::statusBoardDefaultTimeoutsByState()
{
    QVERIFY(StatusBoard::defaultTimeout(StatusState::Info) > 0);
    QVERIFY(StatusBoard::defaultTimeout(StatusState::Success) > 0);
    QCOMPARE(StatusBoard::defaultTimeout(StatusState::Busy), 0);
    QCOMPARE(StatusBoard::defaultTimeout(StatusState::Error), 0);
    QCOMPARE(StatusBoard::stateName(StatusState::Error), QStringLiteral("error"));
    QCOMPARE(StatusBoard::stateName(StatusState::Idle), QStringLiteral("idle"));
}

void CoreTests::gateRefCountsLocks()
{
    NavigationGate gate;
    QSignalSpy lockedSpy(&gate, &NavigationGate::lockedChanged);
    QVERIFY(!gate.isLocked());

    const auto first = gate.acquire(QStringLiteral("a"), QStringLiteral("first"));
    QVERIFY(gate.isLocked());
    QCOMPARE(lockedSpy.count(), 1);
    QCOMPARE(lockedSpy.last().first().toBool(), true);

    const auto second = gate.acquire(QStringLiteral("b"), QStringLiteral("second"));
    QCOMPARE(gate.holdCount(), 2);
    QCOMPARE(lockedSpy.count(), 1); // 已经锁定，不重复通知
    QCOMPARE(gate.reason(), QStringLiteral("second"));
    QCOMPARE(gate.ownerToolId(), QStringLiteral("b"));

    gate.release(second);
    QVERIFY(gate.isLocked());
    QCOMPARE(gate.reason(), QStringLiteral("first"));
    QCOMPARE(lockedSpy.count(), 1);

    gate.release(first);
    QVERIFY(!gate.isLocked());
    QCOMPARE(lockedSpy.count(), 2);
    QCOMPARE(lockedSpy.last().first().toBool(), false);
    QVERIFY(gate.reason().isEmpty());
}

void CoreTests::gateScopedLockReleasesOnDestruction()
{
    NavigationGate gate;
    {
        NavigationLock lock(&gate, QStringLiteral("a"), QStringLiteral("work"));
        QVERIFY(lock.active());
        QVERIFY(gate.isLocked());
    }
    QVERIFY(!gate.isLocked());

    NavigationLock early(&gate, QStringLiteral("a"), QStringLiteral("work"));
    early.release();
    QVERIFY(!early.active());
    QVERIFY(!gate.isLocked());

    NavigationLock none(nullptr, QStringLiteral("a"), QStringLiteral("work"));
    QVERIFY(!none.active());
}

void CoreTests::gateScopedLockIsMovable()
{
    NavigationGate gate;
    NavigationLock outer;
    {
        NavigationLock inner(&gate, QStringLiteral("a"), QStringLiteral("work"));
        outer = std::move(inner);
        QVERIFY(!inner.active());
    }
    QVERIFY(outer.active());
    QVERIFY(gate.isLocked());

    NavigationLock moved(std::move(outer));
    QVERIFY(!outer.active());
    QCOMPARE(gate.holdCount(), 1);
    moved.release();
    QVERIFY(!gate.isLocked());

    // 赋值覆盖时先释放旧锁
    NavigationLock a(&gate, QStringLiteral("a"), QStringLiteral("one"));
    NavigationLock b(&gate, QStringLiteral("a"), QStringLiteral("two"));
    QCOMPARE(gate.holdCount(), 2);
    a = std::move(b);
    QCOMPARE(gate.holdCount(), 1);
    QCOMPARE(gate.reason(), QStringLiteral("two"));
}

void CoreTests::gateReleaseIsIdempotent()
{
    NavigationGate gate;
    QSignalSpy spy(&gate, &NavigationGate::lockedChanged);
    const auto token = gate.acquire(QStringLiteral("a"), QStringLiteral("x"));
    gate.release(token);
    gate.release(token);
    gate.release(9999);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!gate.isLocked());
}

void CoreTests::gateReleaseAllForOwner()
{
    NavigationGate gate;
    gate.acquire(QStringLiteral("a"), QStringLiteral("a1"));
    gate.acquire(QStringLiteral("a"), QStringLiteral("a2"));
    gate.acquire(QStringLiteral("b"), QStringLiteral("b1"));
    gate.releaseAllFor(QStringLiteral("a"));
    QCOMPARE(gate.holdCount(), 1);
    QCOMPARE(gate.ownerToolId(), QStringLiteral("b"));
    QSignalSpy spy(&gate, &NavigationGate::lockedChanged);
    gate.releaseAllFor(QStringLiteral("nobody"));
    QCOMPARE(spy.count(), 0);
    gate.releaseAllFor(QStringLiteral("b"));
    QVERIFY(!gate.isLocked());
    QCOMPARE(spy.count(), 1);
}

void CoreTests::gateOutlivedLockIsHarmless()
{
    auto *gate = new NavigationGate;
    NavigationLock lock(gate, QStringLiteral("a"), QStringLiteral("work"));
    QVERIFY(lock.active());
    delete gate;
    QVERIFY(!lock.active());
    lock.release(); // 不得崩溃
}

void CoreTests::toolServicesTagEverythingWithTheirTool()
{
    QTemporaryDir temp;
    PlatformServices platform((DataPaths(temp.path())));
    const ToolServices a = platform.servicesFor(QStringLiteral("a"));
    const ToolServices b = platform.servicesFor(QStringLiteral("b"));
    QVERIFY(a.valid());
    QCOMPARE(a.toolId(), QStringLiteral("a"));

    a.logInfo(QStringLiteral("one"));
    b.logError(QStringLiteral("two"));
    a.logSuccess(QStringLiteral("three"));
    a.logWarning(QStringLiteral("four"));
    QCOMPARE(platform.log()->entries(QStringLiteral("a")).size(), 3);
    QCOMPARE(platform.log()->entries(QStringLiteral("b")).size(), 1);
    QCOMPARE(platform.log()->entries(QStringLiteral("b")).first().level, LogLevel::Error);

    a.showStatus(StatusState::Busy, QStringLiteral("a busy"));
    QCOMPARE(platform.status()->current(QStringLiteral("a")).state, StatusState::Busy);
    QCOMPARE(platform.status()->current(QStringLiteral("b")).state, StatusState::Idle);
    a.clearStatus();
    QCOMPARE(platform.status()->current(QStringLiteral("a")).state, StatusState::Idle);

    {
        NavigationLock lock = b.lockNavigation(QStringLiteral("b works"));
        QVERIFY(platform.gate()->isLocked());
        QCOMPARE(platform.gate()->ownerToolId(), QStringLiteral("b"));
        QCOMPARE(platform.gate()->reason(), QStringLiteral("b works"));
    }
    QVERIFY(!platform.gate()->isLocked());
}

void CoreTests::toolServicesDataDirsAreIsolated()
{
    QTemporaryDir temp;
    PlatformServices platform((DataPaths(temp.path())));
    QString error;
    const QString a = platform.servicesFor(QStringLiteral("a")).dataDir(&error);
    const QString b = platform.servicesFor(QStringLiteral("b")).dataDir(&error);
    QVERIFY2(!a.isEmpty() && !b.isEmpty(), qPrintable(error));
    QVERIFY(a != b);
    QVERIFY(!a.startsWith(b + QLatin1Char('/')));
    QVERIFY(!b.startsWith(a + QLatin1Char('/')));

    error.clear();
    QVERIFY(platform.servicesFor(QString()).dataDir(&error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void CoreTests::toolServicesCrossThreadCallsAreSafe()
{
    class Worker final : public QThread
    {
    public:
        explicit Worker(const ToolServices &s)
            : services(s)
        {
        }
        void run() override
        {
            services.logInfo(QStringLiteral("from-worker"));
            services.showStatus(StatusState::Busy, QStringLiteral("worker-busy"), 0);
        }
        ToolServices services;
    };

    QTemporaryDir temp;
    PlatformServices platform((DataPaths(temp.path())));
    Worker worker(platform.servicesFor(QStringLiteral("bg")));
    worker.start();
    QVERIFY(worker.wait(3000));

    QTRY_COMPARE_WITH_TIMEOUT(platform.log()->entries(QStringLiteral("bg")).size(), 1, 2000);
    QCOMPARE(platform.log()->entries(QStringLiteral("bg")).first().message, QStringLiteral("from-worker"));
    QTRY_COMPARE_WITH_TIMEOUT(platform.status()->current(QStringLiteral("bg")).state, StatusState::Busy, 2000);
    QCOMPARE(platform.status()->current(QStringLiteral("bg")).text, QStringLiteral("worker-busy"));
}

void CoreTests::toolServicesAfterPlatformIsGoneAreNoOps()
{
    QTemporaryDir temp;
    ToolServices orphan;
    {
        PlatformServices platform((DataPaths(temp.path())));
        orphan = platform.servicesFor(QStringLiteral("a"));
        QVERIFY(orphan.valid());
    }
    QVERIFY(!orphan.valid());
    orphan.logInfo(QStringLiteral("ignored"));
    orphan.showStatus(StatusState::Info, QStringLiteral("ignored"));
    orphan.clearStatus();
    QVERIFY(!orphan.lockNavigation(QStringLiteral("x")).active());
    QString error;
    QVERIFY(orphan.dataDir(&error).isEmpty());
    QVERIFY(!error.isEmpty());

    ToolServices defaultConstructed;
    QVERIFY(!defaultConstructed.valid());
    defaultConstructed.logInfo(QStringLiteral("ignored"));
}

QTEST_MAIN(CoreTests)

#include "core_tests.moc"
