#include "quarcsmonitor.h"
#include <unistd.h>
#include <algorithm>

// 辅助函数：将版本号字符串转换为可比较的整数
// 支持格式：
//  - x.y.z  （语义化版本号）
//  - 纯数字（兼容旧版本号，如 20251127）
static int parseVersionToInt(const QString &versionStr, bool &ok)
{
    ok = false;

    QString v = versionStr.trimmed();
    if (v.isEmpty())
        return 0;

    // 优先解析 x.y.z 语义化版本
    const QStringList parts = v.split('.');
    if (parts.size() == 3)
    {
        bool ok1 = false, ok2 = false, ok3 = false;
        int major = parts[0].toInt(&ok1);
        int minor = parts[1].toInt(&ok2);
        int patch = parts[2].toInt(&ok3);

        if (ok1 && ok2 && ok3 && major >= 0 && minor >= 0 && patch >= 0)
        {
            ok = true;
            // 按权重组合，保证可比较性
            return major * 1000000 + minor * 1000 + patch;
        }
    }

    // 回退到纯数字版本（兼容老版本）
    bool okInt = false;
    int val = v.toInt(&okInt);
    if (okInt)
    {
        ok = true;
        return val;
    }

    return 0;
}

QuarcsMonitor::QuarcsMonitor(QObject *parent) : QObject(parent)
{
    // 在这里初始化你需要监控的进程或者状态变量
    getHostAddress(); // 获取主机地址
    websocketClient = new WebSocketClient(websocketUrl); // 初始化WebSocketClient
    bool ok = connect(websocketClient, &WebSocketClient::messageReceived, this, &QuarcsMonitor::receivedMessage);
    if (!ok) {
        qDebug() << "Failed to connect messageReceived signal";
    }
    
    led = new Led();
    led->initLed();
    led->setLedSpeed("fast");

    isRestarting = false; // 初始化重启标志

    // 读取全局总版本号（来自环境变量 QUARCS_TOTAL_VERSION）
    QByteArray envVersion = qgetenv("QUARCS_TOTAL_VERSION");
    if (envVersion.isEmpty())
    {
        totalVersion = "0.0.0";
    }
    else
    {
        totalVersion = QString::fromUtf8(envVersion);
    }
    qDebug() << "QuarcsMonitor 当前全局总版本号:" << totalVersion;

    // 程序启动时，默认检测并必要时拉起 QT 端
    // 使用 singleShot 避免在构造函数中直接启动外部进程
    QTimer::singleShot(1500, this, [this]() {
        autoStartQtIfNotRunning();
    });

    QTimer::singleShot(1000, this, &QuarcsMonitor::monitorProcess);
}

void QuarcsMonitor::monitorProcess()
{
    // 只依据当前进程管理的 qtServerProcess 状态来判断 QT 端是否在运行，
    // 不再通过 pgrep 等手段检测系统中其它同名进程，做到“只认自己这份 QProcess”。
    bool processRunning = false;

    if (qtServerProcess && qtServerProcess->state() != QProcess::NotRunning)
    {
        // 由本监控程序通过 QProcess 启动的 QT 端仍在运行
        processRunning = true;
    }

    if (!processRunning) {
        // 如果当前处于顺序更新流程中，则认为 QT 服务器可能因更新而暂时关闭，
        // 不再向前端反复上报“未启动/阻塞”的错误，避免干扰更新进度 UI。
        if (isSequentialUpdate)
        {
            qDebug() << "QT server is not running, but update sequence is in progress. "
                     << "Skip qtServerIsOver notifications during update.";
            QTimer::singleShot(1000, this, &QuarcsMonitor::monitorProcess);
            return;
        }

        // 只有在“之前认为服务器是运行状态 / 已经成功启动”时，
        // 才在第一次检测到进程不存在时打印/上报一次“进程结束”日志，避免持续刷屏。
        if (lastQtServerRunning || qtServerInitSuccess)
        {
            led->setLedSpeed("slow");
            qDebug() << "QTServerProcessOver:The Qt server has unexpectedly shut down or has not started.";
        }

        // 标记当前为“未运行”状态
        lastQtServerRunning = false;

        // websocketClient->messageSend("QTServerProcess:The Qt server has unexpectedly shut down or has not started.");
        
        // 检查是否处于重启过程中且已超时
        if (isRestarting) {
            QDateTime currentTime = QDateTime::currentDateTime();
            int elapsedSecs = restartStartTime.secsTo(currentTime);
            
            if (elapsedSecs > restartTimeout) {
                // 重启超时，发送信息并重置状态
                qDebug() << "QT Server restart timed out after" << elapsedSecs << "seconds";
                websocketClient->messageSend("qtServerIsOver");
                isRestarting = false;
            } else {
                qDebug() << "Still waiting for QT Server to start, elapsed:" << elapsedSecs << "seconds";
            }
        }
        // 仅当不处于重启过程中时才发送qtServerIsOver消息
        else if (!isRestarting) {
            websocketClient->messageSend("qtServerIsOver");
            qtServerInitSuccess = false;
        }
        
        QTimer::singleShot(1000, this, &QuarcsMonitor::monitorProcess);
    } else {
        // 检测到进程存在
        qtServerInitSuccess = false;
        lastQtServerRunning = true;
        
        // 发送检测 / 重启相关信号时做 30 秒节流控制：
        // 一次发送后，30 秒内不再重复发送，避免过于频繁。
        QDateTime now = QDateTime::currentDateTime();
        bool canSendTestSignal = false;
        if (!lastTestQtServerProcessTime.isValid())
        {
            // 从未发送过，可以发送
            canSendTestSignal = true;
        }
        else
        {
            int secsDiff = lastTestQtServerProcessTime.secsTo(now);
            if (secsDiff >= 30)
            {
                canSendTestSignal = true;
            }
            else
            {
                // qDebug() << "距离上次 testQtServerProcess 发送仅过去"
                //          << secsDiff << "秒，小于 30 秒，本次不再发送检测信号";
            }
        }

        if (canSendTestSignal)
        {
            websocketClient->messageSend("testQtServerProcess");
            lastTestQtServerProcessTime = now;
            qDebug() << "发送 testQtServerProcess 检测信号";
        }

        QTimer::singleShot(1000, this, SLOT(checkQtServerInitSuccess()));
        
        // 如果检测到进程且正在重启中，重置重启标志
        if (isRestarting) {
            isRestarting = false;
            qDebug() << "QT Server restart completed successfully";
        }
    }
}

void QuarcsMonitor::checkQtServerInitSuccess()
{
    // 在顺序更新过程中，不再向前端重复推送 QT 服务器未启动/阻塞的告警，
    // 以免与更新进度信息混在一起，造成前端 UI 混乱。

    // 在重启过程中，不累加计数器
    if (!isRestarting) {
        checkQtServerLostCount++;
        if (checkQtServerLostCount >= 300) {
            websocketClient->messageSend("qtServerIsOver");
        }
    } else {
        // 重置计数器，因为我们正在重启过程中
        checkQtServerLostCount = 0;
        qDebug() << "Resetting lost count during restart process";
    }

    QTimer::singleShot(1000, this, &QuarcsMonitor::monitorProcess);
}

void QuarcsMonitor::getHostAddress()
{
    retryCount = 0;
    tryGetHostAddress();
}

void QuarcsMonitor::tryGetHostAddress()
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    bool found = false;

    foreach (const QNetworkInterface &interface, interfaces) {
        if (interface.flags() & QNetworkInterface::IsLoopBack || !(interface.flags() & QNetworkInterface::IsUp))
            continue;

        QList<QNetworkAddressEntry> addresses = interface.addressEntries();
        foreach (const QNetworkAddressEntry &address, addresses) {
            if (address.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                QString localIpAddress = address.ip().toString();
                qDebug() << "Local IP Address:" << localIpAddress;

                if (!localIpAddress.isEmpty()) {
                    QUrl getUrl(QStringLiteral("ws://%1:8600").arg(localIpAddress));
                    qDebug() << "WebSocket URL:" << getUrl.toString();
                    websocketUrl = getUrl;
                    found = true;
                    break;
                }
            }
        }
        if (found) break;
    }

    if (found) {
        return; // 成功获取到地址
    }

    retryCount++;
    if (retryCount < maxRetries) {
        // 使用QTimer替代QThread::sleep
        if (!networkRetryTimer) {
            networkRetryTimer = new QTimer(this);
            networkRetryTimer->setSingleShot(true);
            connect(networkRetryTimer, &QTimer::timeout, this, &QuarcsMonitor::tryGetHostAddress);
        }
        networkRetryTimer->start(5000); // 5秒后重试
    } else {
        qCritical() << "Failed to detect any network interfaces after" << maxRetries << "attempts.";
    }
}

// 程序启动后，自动检查一次 QT 端是否已运行；如果未运行则默认拉起一份
void QuarcsMonitor::autoStartQtIfNotRunning()
{
    // 只关心由当前监控程序管理的这一份 QT 端：
    // 如果 qtServerProcess 还在运行，则认为已启动；否则自动拉起一份。
    if (qtServerProcess && qtServerProcess->state() != QProcess::NotRunning)
    {
        qDebug() << "autoStartQtIfNotRunning: QT Server already running via QProcess, skip auto start.";
        return;
    }

    qDebug() << "autoStartQtIfNotRunning: QT Server not running (or not managed yet), start one instance.";
    startQTServer();
}

void QuarcsMonitor::receivedMessage(const QString &message)
{
    QStringList messageList = message.split(":");
    // qDebug() << "Received message:" << message;
    if (messageList[0] == "ServerInitSuccess") {
        qtServerInitSuccess = true;
        isRestarting = false; // 收到服务器初始化成功消息，重置重启标志
    }else if (messageList[0] == "restartQtServer") {
        if (!isRestarting || !qtServerInitSuccess) {
            reRunQTServer();
        }
    }else if (messageList[0] == "VueClientVersion" && messageList.size() >= 2) {
        QString clientVersion = messageList[1];
        vueClientVersion = clientVersion;
        qDebug() << "VueClientVersion:" << vueClientVersion;
        checkVueClientVersion();
    }else if (messageList[0] == "updateCurrentClient" && messageList.size() >= 2) {
        QString fileVersion = messageList[1];
        qDebug() << "收到前端更新请求(updateCurrentClient)，目标版本:" << fileVersion;
        // 前端确认更新后，启动顺序更新流程（从当前全局版本依次更新到最新）
        Q_UNUSED(fileVersion);
        startSequentialUpdate();
    }else if (messageList[0] == "ForceUpdate") {
        qDebug() << "ForceUpdate";
        // 强制更新
        forceUpdate();
    }
}

void QuarcsMonitor::reRunQTServer()
{
    isRestarting = true;
    restartStartTime = QDateTime::currentDateTime();
    checkQtServerLostCount = 0;

    killQTServer();

    // 使用QTimer替代sleep，避免阻塞主线程
    if (!restartTimer) {
        restartTimer = new QTimer(this);
        restartTimer->setSingleShot(true);
        connect(restartTimer, &QTimer::timeout, this, &QuarcsMonitor::startQTServer);
    }
    
    restartTimer->start(3000); // 3秒后启动
}

void QuarcsMonitor::startQTServer()
{
    qDebug() << "Re-running QT Server via QProcess";

    // 如果之前已经有一个 QProcess 在管理 QT 端，先清理掉
    if (qtServerProcess)
    {
        if (qtServerProcess->state() != QProcess::NotRunning)
        {
            qDebug() << "Previous QT Server process still running, killing it first";
            qtServerProcess->kill();
            qtServerProcess->waitForFinished(3000);
        }
        qtServerProcess->deleteLater();
        qtServerProcess = nullptr;
    }

    qtServerProcess = new QProcess(this);

    // 直接由 QProcess 启动 QT 端 ./client，本监控程序只认并管理这一份进程，
    // 不再依赖外部终端进程（如 lxterminal）的存活状态，避免“窗口在但监控认为未启动”的情况。
    qtServerProcess->setWorkingDirectory("/home/quarcs/workspace/QUARCS/QUARCS_QT-SeverProgram/src/BUILD");
    qtServerProcess->setProgram("./client");
    qtServerProcess->setProcessChannelMode(QProcess::MergedChannels);

    // 如需查看 client 的标准输出，可以通过 qDebug 打印出来，后续也可以改为写入日志文件
    connect(qtServerProcess, &QProcess::readyReadStandardOutput,
            this, [this]() {
                if (!qtServerProcess) return;
                const QString out = QString::fromLocal8Bit(qtServerProcess->readAllStandardOutput());
                if (!out.trimmed().isEmpty()) {
                    qDebug() << "QT Server stdout:" << out;
                }
            });

    connect(qtServerProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                qDebug() << "QT Server process finished, exitCode =" << exitCode
                         << ", exitStatus =" << exitStatus;

                if (qtServerProcess) {
                    qtServerProcess->deleteLater();
                    qtServerProcess = nullptr;
                }

                // 如果是在重启流程中结束的，也认为重启流程到此结束
                if (isRestarting) {
                    isRestarting = false;
                }
            });

    connect(qtServerProcess, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError error) {
                qDebug() << "QT Server process error:" << error;
            });

    qtServerProcess->start();

    if (!qtServerProcess->waitForStarted(5000)) {
        qDebug() << "Failed to start QT Server via QProcess, error:"
                 << qtServerProcess->errorString();
        isRestarting = false;
        qtServerProcess->deleteLater();
        qtServerProcess = nullptr;
    }
}

void QuarcsMonitor::killQTServer()
{
    // 使用 QProcess 管理 QT 端，只杀掉由当前监控程序启动的这一份，
    // 不再通过 pkill 之类的命令去模糊匹配进程名，避免误杀自身和其它服务。
    if (!qtServerProcess) {
        qDebug() << "qtServerProcess is null, no QT Server to kill";
        return;
    }

    qDebug() << "Killing QT Server process via QProcess";

    if (qtServerProcess->state() != QProcess::NotRunning)
    {
        // 优先尝试优雅结束
        qtServerProcess->terminate();
        if (!qtServerProcess->waitForFinished(5000)) {
            qDebug() << "QT Server did not terminate gracefully, forcing kill";
            qtServerProcess->kill();
            qtServerProcess->waitForFinished(3000);
        }
    }

    qtServerProcess->deleteLater();
    qtServerProcess = nullptr;
}

void QuarcsMonitor::checkVueClientVersion(bool isForceUpdate)
{
    qDebug() << "开始检查Vue客户端版本更新...";
    // 使用全局总版本号而不是 VueClientVersion
    qDebug() << "当前全局总版本号（字符串）：" << totalVersion;
    bool okCurrent = false;
    int currentVersion = parseVersionToInt(totalVersion, okCurrent);
    if (!okCurrent)
    {
        qDebug() << "【警告】无法解析当前全局总版本号：" << totalVersion
                 << "，将按 0.0.0 处理";
        currentVersion = 0;
    }
    qDebug() << "当前全局总版本号（整数）：" << currentVersion;

    qDebug() << "更新包路径：" << UpdatePackPath;
    
    // 检查UpdatePackPath文件夹下是否存在更新包文件
    QDir dir(UpdatePackPath);
    if (dir.exists()) {
        qDebug() << "更新包目录存在，开始检索文件...";
        
        QStringList nameFilters;
        nameFilters << "*.zip";  // 只检查zip文件
        dir.setNameFilters(nameFilters);
        
        QStringList fileList = dir.entryList(QDir::Files, QDir::Name);
        qDebug() << "找到" << fileList.size() << "个zip文件：" << fileList.join(", ");
        
        if (fileList.isEmpty()) {
            qDebug() << "【警告】更新包目录中没有找到任何zip文件，无法检查更新";
            return;
        }
        
        // 清空顺序更新队列
        pendingUpdateVersions.clear();
        int highestVersion = currentVersion;
        QString highestVersionFile;
        
        // 查找所有高于当前版本的更新包，并按版本号排序
        foreach (const QString &file, fileList) {
            qDebug() << "正在分析文件：" << file;
            
            // 提取版本号，允许文件名格式为：版本号.zip 或 版本号-其他信息.zip
            // 之前使用 file.split(\".\").at(0) 会把 \"1.0.2.zip\" 错误解析为 \"1\"
            // 这里改为从最后一个 '.' 之前截取，得到完整的 \"1.0.2\" 部分
            QString baseName = file;
            int lastDotIndex = baseName.lastIndexOf('.');
            if (lastDotIndex > 0) {
                baseName = baseName.left(lastDotIndex);
            }
            qDebug() << "  基本名称（去除扩展名）：" << baseName;
            
            QString versionStr = baseName.split("-").at(0);
            qDebug() << "  提取的版本号字符串：" << versionStr;

            bool okFile = false;
            int fileVersion = parseVersionToInt(versionStr, okFile);
            if (!okFile) {
                qDebug() << "  【错误】无法将'" << versionStr << "'解析为有效的版本号，跳过此文件";
                continue;
            }
            
            qDebug() << "  文件版本号（整数）：" << fileVersion;

            // 普通检查模式：只收集「高于当前版本」的包
            // ForceUpdate 模式：不管当前认为是多少版本，把所有合法包都纳入顺序更新队列
            bool shouldCollect = false;
            if (isForceUpdate)
            {
                shouldCollect = true;
            }
            else
            {
                shouldCollect = (fileVersion > currentVersion);
            }

            if (shouldCollect) {
                if (!isForceUpdate && fileVersion > currentVersion) {
                    qDebug() << "  发现高于当前版本的更新包：" << fileVersion << ">" << currentVersion;
                } else if (isForceUpdate) {
                    qDebug() << "  ForceUpdate 模式下收集更新包，版本号：" << fileVersion;
                }

                // 收集到候选列表中，后续统一排序
                pendingUpdateVersions.append(versionStr);
                if (fileVersion > highestVersion) {
                    highestVersion = fileVersion;
                    highestVersionFile = versionStr;
                }
            } else {
                qDebug() << "  版本号不高于当前版本，且当前非 ForceUpdate 模式：" << fileVersion << "<=" << currentVersion;
            }
        }
        
        // 去重并按版本号排序（升序）
        if (!pendingUpdateVersions.isEmpty())
        {
            // 使用临时列表对 (versionInt, versionStr) 排序
            QList<QPair<int, QString>> versionPairs;
            for (const QString &vStr : pendingUpdateVersions)
            {
                bool okVal = false;
                int vInt = parseVersionToInt(vStr, okVal);
                if (okVal)
                {
                    versionPairs.append(qMakePair(vInt, vStr));
                }
            }

            std::sort(versionPairs.begin(), versionPairs.end(),
                      [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                          return a.first < b.first;
                      });

            pendingUpdateVersions.clear();
            for (const auto &pair : versionPairs)
            {
                if (!pendingUpdateVersions.contains(pair.second))
                {
                    pendingUpdateVersions.append(pair.second);
                }
            }
        }

        qDebug() << "扫描完成，高于当前版本的更新包数量：" << pendingUpdateVersions.size()
                 << "，当前版本号：" << currentVersion;

        if (!pendingUpdateVersions.isEmpty()) {
            highestVersionFile = pendingUpdateVersions.last();
            currentMaxClientVersion = highestVersionFile;

            qDebug() << "【发现更新】最高版本更新包：" << highestVersionFile;
            if (!isForceUpdate) {
                // 通知前端有新的最高版本可用
                websocketClient->messageSend("checkHasNewUpdatePack:" + highestVersionFile);
                qDebug() << "已发送更新通知：checkHasNewUpdatePack:" + highestVersionFile;
            } else {
                // 强制更新模式下，仅更新内部队列，不重复提示
                qDebug() << "强制更新模式下，仅更新内部顺序更新队列";
            }
        } else {
            qDebug() << "【无更新】没有找到比当前版本" << currentVersion << "更高的版本，无需更新";
        }
    } else {
        qDebug() << "【错误】更新包目录不存在，路径：" << UpdatePackPath;
        // 可选：创建目录
        if (dir.mkpath(UpdatePackPath)) {
            qDebug() << "【提示】已创建更新包目录：" << UpdatePackPath;
        } else {
            qDebug() << "【错误】无法创建更新包目录：" << UpdatePackPath;
        }
    }
    qDebug() << "版本检查完成";
}

void QuarcsMonitor::updateCurrentClient(const QString &newFileVersion)
{
    qDebug() << "updateCurrentClient:" << newFileVersion;
    
    QDir dir(UpdatePackPath);
    if (!dir.exists()) {
        qDebug() << "UpdatePackPath does not exist";
        websocketClient->messageSend("update_error:0:Update package path does not exist");
        return;
    }
    
    // 查找匹配的更新包文件
    QString targetFile;
    foreach (const QString &file, dir.entryList(QDir::Files)) {
        // 与 checkVueClientVersion 中保持一致的版本号提取逻辑：
        // 支持：1.0.2.zip、1.0.2-suffix.zip 等文件名格式
        QString baseName = file;
        int lastDotIndex = baseName.lastIndexOf('.');
        if (lastDotIndex > 0) {
            baseName = baseName.left(lastDotIndex);   // 去掉扩展名 -> 1.0.2 或 1.0.2-suffix
        }
        QString fileVersion = baseName.split("-").at(0); // 取前缀部分 -> 1.0.2

        qDebug() << "检查更新包文件是否匹配版本:" << file
                 << "解析得到版本号:" << fileVersion
                 << "目标版本:" << newFileVersion;

        if (fileVersion == newFileVersion) {
            targetFile = file;
            break;
        }
    }

    if (targetFile.isEmpty()) {
        qDebug() << "未找到匹配版本" << newFileVersion << "的更新包";
        websocketClient->messageSend("update_error:0:No matching version update package found");
        return;
    }

    // 为了避免上一次解压残留的 /update 目录内容导致本次 unzip 出现
    // “cannot delete old ... No such file or directory” 并返回非 0 退出码，
    // 在每次解压前主动清理 UpdatePackPath/update 目录。
    QDir updateTempDir(UpdatePackPath + "update");
    if (updateTempDir.exists())
    {
        qDebug() << "在解压前清理上一次残留的 update 目录:" << updateTempDir.absolutePath();
        if (!updateTempDir.removeRecursively())
        {
            qDebug() << "【警告】无法递归删除 update 临时目录，可能会导致 unzip 报错";
        }
    }

    // 异步解压文件
    unzipProcess = new QProcess(this);
    connect(unzipProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &QuarcsMonitor::onUnzipFinished);
    connect(unzipProcess, &QProcess::errorOccurred,
            this, &QuarcsMonitor::onUnzipError);
    
    // 只监听错误输出（stderr），避免打印大量正常的 inflating 日志
    unzipProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(unzipProcess, &QProcess::readyReadStandardError,
            this, [this]() {
                if (!unzipProcess) {
                    return;
                }
                const QString err = QString::fromLocal8Bit(unzipProcess->readAllStandardError());
                if (!err.trimmed().isEmpty()) {
                    qDebug() << "unzip 错误/警告输出:" << err;
                }
            });
    
    QString command = "unzip -o " + UpdatePackPath + targetFile + " -d " + UpdatePackPath;
    unzipProcess->start(command);
    
    qDebug() << "开始异步解压更新包:" << targetFile;
}

void QuarcsMonitor::onUnzipFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "unzip 进程结束, exitCode =" << exitCode
             << ", exitStatus =" << exitStatus;

    // 进程结束后再读一次错误缓冲区，避免遗漏最后一小段错误信息
    if (unzipProcess) {
        const QString remaining = QString::fromLocal8Bit(unzipProcess->readAllStandardError());
        if (!remaining.isEmpty()) {
            qDebug() << "unzip 结束时剩余错误/警告输出:" << remaining;
        }
    }

    if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
        qDebug() << "解压失败，退出代码:" << exitCode;
        websocketClient->messageSend("update_error:0:Failed to extract update package");

        // 如果当前处于顺序更新模式，解压失败也要视为该步骤失败，终止顺序更新流程
        if (isSequentialUpdate)
        {
            qDebug() << "顺序更新在索引" << currentUpdateIndex << "处解压失败，终止后续更新";
            isSequentialUpdate = false;
            pendingUpdateVersions.clear();
            websocketClient->messageSend("update_sequence_failed:" + QString::number(currentUpdateIndex));
        }

        unzipProcess->deleteLater();
        unzipProcess = nullptr;
        return;
    }
    
    qDebug() << "解压完成，开始执行更新脚本";
    
    // 检查更新脚本是否存在
    QString updateScriptPath = UpdatePackPath + "update/Update.sh";
    QFile updateScript(updateScriptPath);
    if (!updateScript.exists()) {
        qDebug() << "更新脚本不存在，路径:" << updateScriptPath;
        websocketClient->messageSend("update_error:0:Update script does not exist");
        unzipProcess->deleteLater();
        return;
    }
    
    // 异步执行更新脚本
    updateProcess = new QProcess(this);
    updateProcess->setWorkingDirectory(UpdatePackPath + "update/");
    updateProcess->setProcessChannelMode(QProcess::MergedChannels);
    
    // 连接信号处理输出
    connect(updateProcess, &QProcess::readyReadStandardOutput,
            this, &QuarcsMonitor::onUpdateProcessOutput);
    connect(updateProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &QuarcsMonitor::onUpdateProcessFinished);
    connect(updateProcess, &QProcess::errorOccurred,
            this, &QuarcsMonitor::onUpdateProcessError);
    
    updateProcess->start("sudo", QStringList() << "bash" << "Update.sh");
    
    unzipProcess->deleteLater();
    unzipProcess = nullptr;
}

void QuarcsMonitor::onUnzipError(QProcess::ProcessError error)
{
    qDebug() << "解压过程出错:" << error;
    websocketClient->messageSend("update_error:0:Error during extraction process");

    // 顺序更新模式下，解压过程报错同样要中止整个顺序更新队列
    if (isSequentialUpdate)
    {
        qDebug() << "顺序更新在索引" << currentUpdateIndex << "处解压进程错误，终止后续更新";
        isSequentialUpdate = false;
        pendingUpdateVersions.clear();
        websocketClient->messageSend("update_sequence_failed:" + QString::number(currentUpdateIndex));
    }

    unzipProcess->deleteLater();
    unzipProcess = nullptr;
}

void QuarcsMonitor::onUpdateProcessOutput()
{
    QString output = updateProcess->readAllStandardOutput();
    qDebug() << "完整标准输出:" << output;
    
    // 处理进度输出（保持原有逻辑）
    QStringList lines = output.split("\n", Qt::SkipEmptyParts);
    foreach (const QString &line, lines) {
        qDebug() << "Update.sh output:" << line;
        
        if (line.startsWith("PROGRESS:")) {
            QStringList parts = line.split(":", Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                QString progressValue = parts[1];
                QString progressMessage = parts[2];
                qDebug() << "更新进度:" << progressValue << "% -" << progressMessage;
                websocketClient->messageSend("update_progress:" + progressValue + ":" + progressMessage);
            }
        }
        else if (line.startsWith("ERROR:")) {
            QStringList parts = line.split(":", Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                QString progressValue = parts[1];
                QString errorMessage = parts[2];
                qDebug() << "更新错误:" << progressValue << "% -" << errorMessage;
                websocketClient->messageSend("update_error:" + progressValue + ":" + errorMessage);
            }
        }
        else if (line.startsWith("SUCCESS:")) {
            QStringList parts = line.split(":", Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                QString progressValue = parts[1];
                QString successMessage = parts[2];
                qDebug() << "更新成功:" << progressValue << "% -" << successMessage;
                websocketClient->messageSend("update_success:" + progressValue + ":" + successMessage);
            }
        }
        else if (line.startsWith("REBOOT:") || line.startsWith("NOREBOOT:")) {
            qDebug() << line;
            websocketClient->messageSend(line);
        }
    }
}

void QuarcsMonitor::onUpdateProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    bool success = (exitCode == 0 && exitStatus == QProcess::NormalExit);

    if (!success) {
        qDebug() << "更新失败，退出代码:" << exitCode;
        websocketClient->messageSend("update_failed:" + QString::number(exitCode));
    } else {
        qDebug() << "更新脚本执行完成";
    }

    updateProcess->deleteLater();
    updateProcess = nullptr;

    // 如果处于顺序更新模式，则根据结果决定是否继续后续更新包
    if (isSequentialUpdate)
    {
        if (success)
        {
            // 当前步骤成功，继续执行下一个更新包
            startNextUpdateInQueue();
        }
        else
        {
            // 当前步骤失败，终止后续更新
            qDebug() << "顺序更新在索引" << currentUpdateIndex << "处失败，终止后续更新";
            isSequentialUpdate = false;
            pendingUpdateVersions.clear();
            websocketClient->messageSend("update_sequence_failed:" + QString::number(currentUpdateIndex));
        }
    }
}

void QuarcsMonitor::onUpdateProcessError(QProcess::ProcessError error)
{
    qDebug() << "更新脚本执行出错:" << error;
    websocketClient->messageSend("update_error:0:Error during update script execution");
    updateProcess->deleteLater();
    updateProcess = nullptr;
}

void QuarcsMonitor::forceUpdate()
{
    qDebug() << "ForceUpdate";
    // 重新检查并构建顺序更新队列，然后启动顺序更新
    checkVueClientVersion(true);
    startSequentialUpdate();
}

// 启动顺序更新流程
void QuarcsMonitor::startSequentialUpdate()
{
    qDebug() << "startSequentialUpdate called, 当前待更新包数量:" << pendingUpdateVersions.size();

    // 若队列为空，则尝试重新扫描一次
    if (pendingUpdateVersions.isEmpty())
    {
        qDebug() << "pendingUpdateVersions 为空，重新检查更新包";
        checkVueClientVersion(true);
    }

    if (pendingUpdateVersions.isEmpty())
    {
        qDebug() << "没有可用的更新包，顺序更新结束";
        websocketClient->messageSend("No_update_pack_found");
        return;
    }

    isSequentialUpdate = true;
    currentUpdateIndex = -1;

    // 通知前端顺序更新开始，总步骤数
    websocketClient->messageSend("update_sequence_start:" + QString::number(pendingUpdateVersions.size()));

    startNextUpdateInQueue();
}

// 执行队列中的下一个更新包
void QuarcsMonitor::startNextUpdateInQueue()
{
    if (!isSequentialUpdate)
    {
        qDebug() << "startNextUpdateInQueue 在非顺序更新模式下被调用，忽略";
        return;
    }

    currentUpdateIndex++;

    if (currentUpdateIndex >= pendingUpdateVersions.size())
    {
        qDebug() << "所有更新包已顺序执行完成";
        isSequentialUpdate = false;
        websocketClient->messageSend("update_sequence_finished");
        pendingUpdateVersions.clear();
        return;
    }

    const QString version = pendingUpdateVersions.at(currentUpdateIndex);
    qDebug() << "开始顺序更新，第" << (currentUpdateIndex + 1)
             << "个版本：" << version
             << "，总共：" << pendingUpdateVersions.size();

    // 通知前端当前执行到第几个版本
    websocketClient->messageSend("update_sequence_step:"
                                 + QString::number(currentUpdateIndex + 1) + ":"
                                 + QString::number(pendingUpdateVersions.size()) + ":"
                                 + version);

    // 启动当前版本的单次更新流程
    updateCurrentClient(version);
}