#include "quarcsmonitor.h"
#include <unistd.h>

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
    QTimer::singleShot(1000, this, &QuarcsMonitor::monitorProcess);
}

void QuarcsMonitor::monitorProcess()
{
    // 创建一个QProcess对象来获取进程的信息
    QProcess process;
    process.start("pgrep QUARCS");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    // 如果进程正在运行，output将不会为空
    if (output.isEmpty() || output == "") {
        // websocketClient->messageSend("QTServerProcess:The Qt server has unexpectedly shut down or has not started.");
        led->setLedSpeed("slow");
        qDebug() << "QTServerProcessOver:The Qt server has unexpectedly shut down or has not started.";
        
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
        qtServerInitSuccess = false;
        websocketClient->messageSend("testQtServerProcess");
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
    if (qtServerInitSuccess) {
        led->setLedSpeed("slow");
        qDebug() << "QTServerProcess:The Qt server is running.";
        checkQtServerLostCount = 0;
    }else{
        led->setLedSpeed("fast");
        qDebug() << "QTServerProcessOver:The Qt server has not started or Qt server is blocked.";
        
        // 在重启过程中，不累加计数器
        if (!isRestarting) {
            checkQtServerLostCount++;
            if (checkQtServerLostCount >= 60) {
                websocketClient->messageSend("qtServerIsOver");
            }
        } else {
            // 重置计数器，因为我们正在重启过程中
            checkQtServerLostCount = 0;
            qDebug() << "Resetting lost count during restart process";
        }
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
        qDebug() << "updateCurrentClient:" << fileVersion;
        // 更新当前客户端
        updateCurrentClient(fileVersion);
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
    qDebug() << "Re-running QT Server";
    
    int result = system("lxterminal -e bash -c 'cd /home/quarcs/workspace/QUARCS/QUARCS_QT-SeverProgram/src/BUILD && ./client ;$SHELL' &");
    
    if (result != 0) {
        qDebug() << "Failed to re-run QT Server";
        isRestarting = false;
    } else {
        qDebug() << "QT Server restart initiated, timeout set to" << restartTimeout << "seconds";
    }
}

void QuarcsMonitor::killQTServer()
{
    int result = system("pkill -f 'QUARCS'");
    if (result != 0) {
        qDebug() << "Failed to kill QT Server";
    }
}

void QuarcsMonitor::checkVueClientVersion(bool isForceUpdate)
{
    qDebug() << "开始检查Vue客户端版本更新...";
    qDebug() << "当前版本号：" << vueClientVersion;
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
        
        int currentVersion = vueClientVersion.toInt();
        qDebug() << "当前版本号（整数）：" << currentVersion;
        
        int highestVersion = currentVersion;
        QString highestVersionFile;
        
        // 查找最高版本的更新包
        foreach (const QString &file, fileList) {
            qDebug() << "正在分析文件：" << file;
            
            // 提取版本号，允许文件名格式为：版本号.zip 或 版本号-其他信息.zip
            QString baseName = file.split(".").at(0);
            qDebug() << "  基本名称（去除扩展名）：" << baseName;
            
            QString versionStr = baseName.split("-").at(0);
            qDebug() << "  提取的版本号字符串：" << versionStr;
            
            bool ok;
            int fileVersion = versionStr.toInt(&ok);
            if (!ok) {
                qDebug() << "  【错误】无法将'" << versionStr << "'解析为有效的版本号，跳过此文件";
                continue;
            }
            
            qDebug() << "  文件版本号（整数）：" << fileVersion;
            
            if (fileVersion > highestVersion) {
                qDebug() << "  发现更高版本：" << fileVersion << ">" << highestVersion;
                highestVersion = fileVersion;
                highestVersionFile = versionStr;
            } else {
                qDebug() << "  版本号不高于当前最高版本：" << fileVersion << "<=" << highestVersion;
            }
        }
        
        qDebug() << "扫描完成，最高版本号：" << highestVersion << "，当前版本号：" << currentVersion;
        currentMaxClientVersion = highestVersionFile;
        
        if (highestVersion > currentVersion) {
            qDebug() << "【发现更新】找到新版本更新包：" << highestVersionFile << "，准备通知客户端";
            if (!isForceUpdate) {
                websocketClient->messageSend("checkHasNewUpdatePack:" + highestVersionFile);
                qDebug() << "已发送更新通知：checkHasNewUpdatePack:" + highestVersionFile;
            }else{
                // websocketClient->messageSend("checkHasNewUpdatePack:" + highestVersionFile);
                // qDebug() << "已发送更新通知：checkHasNewUpdatePack:" + highestVersionFile;
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
        QStringList fileList = file.split(".");
        QString fileVersion = fileList[0];
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
    
    // 异步解压文件
    unzipProcess = new QProcess(this);
    connect(unzipProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &QuarcsMonitor::onUnzipFinished);
    connect(unzipProcess, &QProcess::errorOccurred,
            this, &QuarcsMonitor::onUnzipError);
    
    QString command = "unzip -o " + UpdatePackPath + targetFile + " -d " + UpdatePackPath;
    unzipProcess->start(command);
    
    qDebug() << "开始异步解压更新包:" << targetFile;
}

void QuarcsMonitor::onUnzipFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
        qDebug() << "解压失败，退出代码:" << exitCode;
        websocketClient->messageSend("update_error:0:Failed to extract update package");
        unzipProcess->deleteLater();
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
    
    updateProcess->start("sudo", QStringList() << "bash" << "Update.sh" << "-r");
    
    unzipProcess->deleteLater();
    unzipProcess = nullptr;
}

void QuarcsMonitor::onUnzipError(QProcess::ProcessError error)
{
    qDebug() << "解压过程出错:" << error;
    websocketClient->messageSend("update_error:0:Error during extraction process");
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
    if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
        qDebug() << "更新失败，退出代码:" << exitCode;
        websocketClient->messageSend("update_failed:" + QString::number(exitCode));
    } else {
        qDebug() << "更新脚本执行完成";
    }
    
    // this->checkVueClientVersion(true);
    
    updateProcess->deleteLater();
    updateProcess = nullptr;
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
    // 检查当前最新版本
    checkVueClientVersion(true);
    if (!currentMaxClientVersion.isEmpty()) {
        // 更新当前客户端
        updateCurrentClient(currentMaxClientVersion);
    }else{
        qDebug() << "No update pack found";
        websocketClient->messageSend("No_update_pack_found");
    }
}