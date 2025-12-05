#ifndef QUARCSMONITOR_H
#define QUARCSMONITOR_H

#include <QObject>
#include <QNetworkInterface>
#include <QThread> 
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTimer>
#include <QStringList>

#include "websocketclient.h"
#include "led.h"

class QuarcsMonitor : public QObject
{
    Q_OBJECT
public:
    explicit QuarcsMonitor(QObject *parent = nullptr);
    void getHostAddress();
    void receivedMessage(const QString &message);

    bool qtServerInitSuccess = false;
    
    Led *led;

    int checkQtServerLostCount = 0;

signals:
    void processUpdated(const QString &status);

public slots:
    void monitorProcess();
    void checkQtServerInitSuccess();
    void killQTServer();
    void reRunQTServer();
    void onApplicationAboutToQuit();
    void checkVueClientVersion(bool isForceUpdate = false);
    void updateCurrentClient(const QString &fileVersion);
    void forceUpdate();
    void onUnzipFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onUnzipError(QProcess::ProcessError error);
    void onUpdateProcessOutput();
    void onUpdateProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onUpdateProcessError(QProcess::ProcessError error);
    void startQTServer();
    void tryGetHostAddress();

private:
    WebSocketClient *websocketClient;
    QUrl websocketUrl;
    bool isRestarting = false; // 标记是否正在重启QT服务器
    QDateTime restartStartTime; // 重启开始时间
    const int restartTimeout = 30; // 重启超时时间(秒)
    QDateTime lastTestQtServerProcessTime; // 上次发送 testQtServerProcess 的时间，用于限流
    QString UpdatePackPath = "/var/www/update_pack/";
    QString vueClientVersion = "";
    QString currentMaxClientVersion = "";
    
    // 异步处理相关的成员变量
    QProcess *unzipProcess = nullptr;
    QProcess *updateProcess = nullptr;
    QTimer *restartTimer = nullptr;
    QTimer *networkRetryTimer = nullptr;
    int retryCount = 0;
    const int maxRetries = 20;

    // 全局版本与顺序更新相关
    QString totalVersion;                 // 当前全局总版本号（从环境变量读取）
    QStringList pendingUpdateVersions;    // 待顺序执行的更新版本列表（升序）
    bool isSequentialUpdate = false;      // 是否处于顺序更新流程中
    int currentUpdateIndex = -1;          // 当前正在执行的更新索引

    // Qt 服务器运行状态，用于只在状态变化（特别是“由运行变为停止”）时打印/上报
    bool lastQtServerRunning = false;

    // 通过 QProcess 管理的 QT 端进程，只杀掉由当前监控程序启动的这一份
    QProcess *qtServerProcess = nullptr;

    // 程序启动时，检测 QT 端是否已经在运行，如果没有则默认拉起一份
    void autoStartQtIfNotRunning();

    // 杀掉当前机器上所有与 QT 端可执行文件路径匹配的旧进程（包括孤儿进程）
    void killAllQtServerProcesses();

    // 启动/推进顺序更新流程
    void startSequentialUpdate();
    void startNextUpdateInQueue();
};

#endif // QUARCSMONITOR_H