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
};

#endif // QUARCSMONITOR_H