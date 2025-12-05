#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <sys/prctl.h>
#include "quarcsmonitor.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

// 使用简单的锁文件机制，保证同一时间只有一个管理进程实例在运行。
// 如果已有实例持有锁文件，本次启动会直接退出。
// 锁文件放在 /tmp 下，避免权限问题。
static int createSingleInstanceLock()
{
    const char *lockPath = "/tmp/QUARCS_QMANAGE.lock";
    int fd = ::open(lockPath, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        // 打不开锁文件时，不强行退出，允许继续运行（以免因权限问题完全起不来）
        return -1;
    }

    struct flock fl;
    fl.l_type   = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    fl.l_pid    = getpid();

    if (fcntl(fd, F_SETLK, &fl) < 0) {
        // 上锁失败，说明已有其他进程持有锁 -> 视为已有实例在运行
        ::close(fd);
        return -1;
    }

    // 写入当前 PID（仅用于调试/查看）
    ::ftruncate(fd, 0);
    char buf[32];
    int len = ::snprintf(buf, sizeof(buf), "%d\n", getpid());
    if (len > 0) {
        ::write(fd, buf, static_cast<size_t>(len));
    }

    // 注意：不要关闭 fd，进程退出时由内核自动关闭并释放文件锁
    return fd;
}

int main(int argc, char *argv[])
{
    // 单实例保护：如果已有管理进程在运行，则本次直接退出
    int lockFd = createSingleInstanceLock();
    if (lockFd < 0) {
        // 这里可以按需打印一行日志到 stderr（若当前仍有终端），
        // 但在 daemon 模式下一般是静默退出。
        return 0;
    }

    QCoreApplication a(argc, argv);
    a.setApplicationName("QMANAGE");
    a.setApplicationVersion("1.0");
    // Command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription("QUARCS process monitor");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // Add normal mode option
    QCommandLineOption normalOption(QStringList() << "n" << "normal", 
                                   "Run in normal mode (not as daemon)");
    parser.addOption(normalOption);
    
    // Process the command line arguments
    parser.process(a);
    
    // Check if normal mode is requested (default is daemon mode)
    bool normalMode = parser.isSet(normalOption);
    
    if (!normalMode) {
        // 守护进程模式 - 默认模式
        
        // 创建子进程
        pid_t pid = fork();
        
        if (pid < 0) {
            // fork失败
            exit(EXIT_FAILURE);
        }
        
        if (pid > 0) {
            // 父进程退出
            exit(EXIT_SUCCESS);
        }
        
        // 创建新会话，成为会话首进程
        setsid();
        
        // 忽略某些信号
        signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);
        
        // 再次fork，确保进程不能获取终端
        pid = fork();
        
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }
        
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
        
        // 更改工作目录
        chdir("/");
        
        // 关闭所有文件描述符
        for (int i = sysconf(_SC_OPEN_MAX); i >= 0; i--) {
            close(i);
        }
        
        // 重定向标准输入输出到/dev/null
        open("/dev/null", O_RDWR);
        dup(0);
        dup(0);
        
        // 设置进程名
        prctl(PR_SET_NAME, "QMANAGE");
        
        // 重新初始化Qt应用（因为在daemon流程中可能已经关闭了文件描述符）
        int dummy_argc = 1;
        char *dummy_argv[] = {(char*)"QMANAGE", nullptr};
        QCoreApplication b(dummy_argc, dummy_argv);
        QuarcsMonitor monitor;
        return b.exec();
    } else {
        // 普通模式 - 需要使用--normal参数
        prctl(PR_SET_NAME, "QMANAGE");
        QuarcsMonitor monitor;
        return a.exec();
    }
}