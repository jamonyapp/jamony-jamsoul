/******************************************************************************\
* jamsoul IPC (stdin 监听, 接收 jamony 窗口跟随指令)                          *
\******************************************************************************/
/*
    jamsoul 接收 jamony (Electron) 通过 stdin 发来的窗口跟随指令:
      {"cmd":"raise"}        → jamsoul 窗口跟随 jamony 前置
      {"cmd":"move","x":N,"y":N} → jamsoul 窗口跟随 jamony 移动

    jamony spawn jamsoul 时 stdio:['pipe','ignore','ignore'], 通过 child.stdin 写 JSON。
    jamsoul 用 QSocketNotifier 监听 stdin 可读 (主线程 event loop, 无需单独线程),
    fgets 读一行, QJsonDocument 解析, 发 Qt 信号到 CClientDlg 调 raise/move。
*/

#pragma once
#include <QObject>
#include <QSocketNotifier>

class JamsoulIpc : public QObject
{
    Q_OBJECT
public:
    explicit JamsoulIpc ( QObject* parent = nullptr );

    // 启动 stdin 监听 (主线程 event loop, QSocketNotifier)
    void Start();

signals:
    void RaiseRequested();
    void MoveRequested ( int x, int y );

private slots:
    void OnStdinReady();

private:
    QSocketNotifier* m_pNotifier;
};
