/******************************************************************************\
* jamsoul IPC (stdin 监听, 接收 jamony 窗口跟随指令)                          *
\******************************************************************************/

#include "jamsoulipc.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include <cstdio>

JamsoulIpc::JamsoulIpc ( QObject* parent ) : QObject ( parent ), m_pNotifier ( nullptr ) {}

void JamsoulIpc::Start()
{
    // 监听 stdin (fileno=0) 可读, 主线程 event loop 触发, 无需独立线程
    m_pNotifier = new QSocketNotifier ( fileno ( stdin ), QSocketNotifier::Read, this );
    connect ( m_pNotifier, &QSocketNotifier::activated, this, &JamsoulIpc::OnStdinReady );
}

void JamsoulIpc::OnStdinReady()
{
    char buf[256];
    if ( !fgets ( buf, sizeof ( buf ), stdin ) ) { return; }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson ( QByteArray ( buf ), &err );
    if ( err.error != QJsonParseError::NoError ) { return; }

    QJsonObject obj = doc.object();
    QString cmd = obj.value ( "cmd" ).toString();

    if ( cmd == "raise" )
    {
        emit RaiseRequested();
    }
    else if ( cmd == "move" )
    {
        emit MoveRequested ( obj.value ( "x" ).toInt(), obj.value ( "y" ).toInt() );
    }
}
