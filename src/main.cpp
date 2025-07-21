#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <sys/prctl.h>
#include "quarcsmonitor.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

int main(int argc, char *argv[])
{
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