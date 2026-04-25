/***************************************************************************
 *  qgsqcopilotsserverplugin.cpp                                        *
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 ***************************************************************************/

#include "qgsqcopilotsserverplugin.h"
#include "moc_qgsqcopilotsserverplugin.cpp"

#include "qgisinterface.h"
#include "qgsapplication.h"
#include "qgsmessagebar.h"
#include "qgsmessagelog.h"

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QtGlobal>

namespace
{
  const QString sName = QObject::tr( "QCopilotsServer" );
  const QString sDescription = QObject::tr( "QCopilots Server 管理插件，提供三种功能：启动 QCopilotsServer、关闭 QCopilotsServer 和重启 QCopilotsServer。" );
  const QString sCategory = QObject::tr( "QCopilots" );
  const QString sPluginVersion = QObject::tr( "Version 1.0" );
  const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
  const QString sPluginIcon = QStringLiteral( ":/qcopilots_server/icons/qcopilots-server-plugin.svg" );

  const QString sMenuName = QObject::tr( "&QCopilots" );
  const QString sLogCategory = QStringLiteral( "QCopilotsServer" );

  const QString sInstallPrefixEnv = QStringLiteral( "QGIS34407_INSTALL_DIRECTORY_PREFIX_V_0_01_00" );
  const QString sQCopilotsConfigFileName = QStringLiteral( "QCopilotsConfigure.json" );
  const QString sLlamaServerExeName = QStringLiteral( "qcopilots-server.exe" );
  const QString sRuntimeLogFileName = QStringLiteral( "runtime.log" );

  constexpr int sProcessStartTimeoutMs = 5000;
  constexpr int sTerminateTimeoutMs = 5000;
  constexpr int sKillTimeoutMs = 3000;
}

QgsQCopilotsServerPlugin::QgsQCopilotsServerPlugin( QgisInterface *iface )
  : QgisPlugin( sName, sDescription, sCategory, sPluginVersion, sPluginType )
  , mIface( iface )
{
}

QgsQCopilotsServerPlugin::~QgsQCopilotsServerPlugin()
{
  stopProcessInternal( false, false );
}

void QgsQCopilotsServerPlugin::initGui()
{
  if ( !mIface )
    return;

  mStartAction = new QAction( QgsApplication::getThemeIcon( QStringLiteral( "/mActionStart.svg" ) ), tr( "启动 QCopilotsServer" ), this );
  mStartAction->setObjectName( QStringLiteral( "QCopilotsServerStartAction" ) );
  connect( mStartAction, &QAction::triggered, this, &QgsQCopilotsServerPlugin::startBackendModel );
  addActionToQCopilotsMenu( mStartAction );

  mStopAction = new QAction( QgsApplication::getThemeIcon( QStringLiteral( "/mActionStop.svg" ) ), tr( "关闭 QCopilotsServer" ), this );
  mStopAction->setObjectName( QStringLiteral( "QCopilotsServerStopAction" ) );
  connect( mStopAction, &QAction::triggered, this, &QgsQCopilotsServerPlugin::stopBackendModel );
  addActionToQCopilotsMenu( mStopAction );

  mRestartAction = new QAction( QgsApplication::getThemeIcon( QStringLiteral( "/mActionReload.svg" ) ), tr( "重启 QCopilotsServer" ), this );
  mRestartAction->setObjectName( QStringLiteral( "QCopilotsServerRestartAction" ) );
  connect( mRestartAction, &QAction::triggered, this, &QgsQCopilotsServerPlugin::applyConfigAndRestart );
  addActionToQCopilotsMenu( mRestartAction );

  updateActionStates();
}

void QgsQCopilotsServerPlugin::unload()
{
  stopProcessInternal( false, false );

  removeActionFromQCopilotsMenu( mStartAction );
  removeActionFromQCopilotsMenu( mStopAction );
  removeActionFromQCopilotsMenu( mRestartAction );

  if ( mOwnQCopilotsMenu && mQCopilotsMenu && mQCopilotsMenu->actions().isEmpty() )
  {
    if ( QMainWindow *mainWindow = qobject_cast<QMainWindow *>( mIface ? mIface->mainWindow() : nullptr ) )
    {
      if ( QMenuBar *menuBar = mainWindow->menuBar() )
        menuBar->removeAction( mQCopilotsMenu->menuAction() );
    }
    delete mQCopilotsMenu;
  }
  mQCopilotsMenu = nullptr;
  mOwnQCopilotsMenu = false;

  delete mStartAction;
  mStartAction = nullptr;
  delete mStopAction;
  mStopAction = nullptr;
  delete mRestartAction;
  mRestartAction = nullptr;
}

void QgsQCopilotsServerPlugin::startBackendModel()
{
  if ( mProcess && mProcess->state() != QProcess::NotRunning )
  {
    const QString message = tr( "QCopilotsServer 已在运行，当前状态：%1" ).arg( processStateText() );
    logMessage( message, Qgis::Warning );
    pushMessageBar( message, Qgis::Warning );
    updateActionStates();
    return;
  }

  LauncherConfig config;
  QString errorMessage;
  if ( !loadLauncherConfig( config, errorMessage ) )
  {
    logMessage( errorMessage, Qgis::Warning );
    pushMessageBar( errorMessage, Qgis::Warning );
    updateActionStates();
    return;
  }

  if ( !startProcessWithConfig( config, errorMessage ) )
  {
    logMessage( errorMessage, Qgis::Warning );
    pushMessageBar( errorMessage, Qgis::Warning );
    updateActionStates();
    return;
  }

  const QString message = tr( "QCopilotsServer 启动命令已执行，正在进行健康检查，具体信息请查看日志面板中的 QCopilotsServer 页面信息。" );
  logMessage( message, Qgis::Info );
  pushMessageBar( message, Qgis::Info );
}

void QgsQCopilotsServerPlugin::stopBackendModel()
{
  stopProcessInternal( true, true );
}

void QgsQCopilotsServerPlugin::applyConfigAndRestart()
{
  LauncherConfig config;
  QString errorMessage;
  if ( !loadLauncherConfig( config, errorMessage ) )
  {
    logMessage( errorMessage, Qgis::Warning );
    pushMessageBar( errorMessage, Qgis::Warning );
    updateActionStates();
    return;
  }

  const bool wasRunning = mProcess && mProcess->state() != QProcess::NotRunning;
  if ( wasRunning )
  {
    stopProcessInternal( false, false );
  }

  if ( !startProcessWithConfig( config, errorMessage ) )
  {
    logMessage( errorMessage, Qgis::Warning );
    pushMessageBar( errorMessage, Qgis::Warning );
    updateActionStates();
    return;
  }

  const QString message = wasRunning
                            ? tr( "QCopilotsServer 已按最新配置重启，正在进行健康检查。" )
                            : tr( "QCopilotsServer 已按最新配置启动，正在进行健康检查。" );
  logMessage( message, Qgis::Info );
  pushMessageBar( message, Qgis::Info );
}

void QgsQCopilotsServerPlugin::onProcessStarted()
{
  logMessage( tr( "QCopilotsServer 进程已启动。" ), Qgis::Info );
  updateActionStates();
}

void QgsQCopilotsServerPlugin::onProcessFinished( int exitCode, QProcess::ExitStatus exitStatus )
{
  stopHealthPolling();

  const bool stoppedByRequest = mStopInProgress;
  const bool usedForceKill = mStopUsedForceKill;
  mStopInProgress = false;
  mStopUsedForceKill = false;

  const QString statusText = exitStatus == QProcess::NormalExit ? tr( "NormalExit" ) : tr( "CrashExit" );
  if ( stoppedByRequest )
  {
    const QString message = usedForceKill
                              ? tr( "QCopilotsServer 已按关闭动作停止（terminate 超时后执行 kill）。原始退出信息：exitCode=%1，exitStatus=%2。" ).arg( exitCode ).arg( statusText )
                              : tr( "QCopilotsServer 已按关闭动作停止。原始退出信息：exitCode=%1，exitStatus=%2。" ).arg( exitCode ).arg( statusText );
    logMessage( message, Qgis::Info );
  }
  else
  {
    const QString message = tr( "QCopilotsServer 进程已结束，exitCode=%1，exitStatus=%2。" ).arg( exitCode ).arg( statusText );
    logMessage( message, exitStatus == QProcess::NormalExit ? Qgis::Info : Qgis::Warning );

    if ( exitStatus != QProcess::NormalExit )
      pushMessageBar( message, Qgis::Warning );
  }

  updateActionStates();
}

void QgsQCopilotsServerPlugin::onProcessErrorOccurred( QProcess::ProcessError error )
{
  QString reason;
  switch ( error )
  {
    case QProcess::FailedToStart:
      reason = tr( "进程启动失败，可能是可执行文件不存在或缺少依赖。" );
      break;
    case QProcess::Crashed:
      reason = tr( "进程异常崩溃。" );
      break;
    case QProcess::Timedout:
      reason = tr( "进程操作超时。" );
      break;
    case QProcess::WriteError:
      reason = tr( "进程写入错误。" );
      break;
    case QProcess::ReadError:
      reason = tr( "进程读取错误。" );
      break;
    case QProcess::UnknownError:
      reason = tr( "进程未知错误。" );
      break;
  }

  if ( mStopInProgress && error == QProcess::Crashed )
  {
    logMessage( tr( "QCopilotsServer 在停止流程中触发进程事件：%1" ).arg( reason ), Qgis::Info );
    return;
  }

  const QString message = tr( "QCopilotsServer 进程错误：%1（状态：%2）" ).arg( reason, processStateText() );
  logMessage( message, Qgis::Warning );
  pushMessageBar( message, Qgis::Warning );
  updateActionStates();
}

void QgsQCopilotsServerPlugin::onHealthTimerTimeout()
{
  if ( !mNetworkAccessManager || !mHealthTimer || !mHealthTimer->isActive() )
    return;

  if ( QDateTime::currentDateTimeUtc() >= mHealthDeadlineUtc )
  {
    finalizeHealthFailure( tr( "健康检查超时：在限定时间内未收到就绪响应。" ) );
    return;
  }

  if ( mHealthRequestInFlight )
    return;

  mHealthRequestInFlight = true;
  ++mHealthAttempt;

  const quint64 sessionId = mHealthSessionId;
  QNetworkRequest request( mHealthUrl );
  request.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );

  QNetworkReply *reply = mNetworkAccessManager->get( request );
  connect( reply, &QNetworkReply::finished, this, [this, reply, sessionId]()
  {
    if ( sessionId != mHealthSessionId )
    {
      reply->deleteLater();
      return;
    }

    mHealthRequestInFlight = false;

    const int statusCode = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
    const bool isHttpSuccess = statusCode >= 200 && statusCode < 400;
    const bool isSuccess = reply->error() == QNetworkReply::NoError && isHttpSuccess;

    if ( isSuccess )
    {
      const QString message = tr( "健康检查成功（第 %1 次），URL=%2，HTTP=%3。" ).arg( mHealthAttempt ).arg( mHealthUrl.toString() ).arg( statusCode );
      logMessage( message, Qgis::Info );
      pushMessageBar( tr( "QCopilotsServer 已就绪。" ), Qgis::Info );
      stopHealthPolling();
      reply->deleteLater();
      return;
    }

    QString detail = reply->errorString();
    if ( statusCode > 0 )
      detail = tr( "HTTP %1，%2" ).arg( statusCode ).arg( reply->errorString() );

    if ( QDateTime::currentDateTimeUtc() >= mHealthDeadlineUtc )
    {
      finalizeHealthFailure( tr( "健康检查超时：最后一次探测失败详情：%1" ).arg( detail ) );
      reply->deleteLater();
      return;
    }

    logMessage( tr( "健康检查未就绪（第 %1 次）：%2" ).arg( mHealthAttempt ).arg( detail ), Qgis::Info );
    reply->deleteLater();
  } );
}

void QgsQCopilotsServerPlugin::ensureProcess()
{
  if ( mProcess )
    return;

  mProcess = new QProcess( this );
  connect( mProcess, &QProcess::started, this, &QgsQCopilotsServerPlugin::onProcessStarted );
  connect( mProcess, qOverload< int, QProcess::ExitStatus >( &QProcess::finished ), this, &QgsQCopilotsServerPlugin::onProcessFinished );
  connect( mProcess, &QProcess::errorOccurred, this, &QgsQCopilotsServerPlugin::onProcessErrorOccurred );
}

void QgsQCopilotsServerPlugin::ensureHealthInfrastructure()
{
  if ( !mNetworkAccessManager )
    mNetworkAccessManager = new QNetworkAccessManager( this );

  if ( !mHealthTimer )
  {
    mHealthTimer = new QTimer( this );
    mHealthTimer->setSingleShot( false );
    connect( mHealthTimer, &QTimer::timeout, this, &QgsQCopilotsServerPlugin::onHealthTimerTimeout );
  }
}

QMenu *QgsQCopilotsServerPlugin::resolveQCopilotsMenu()
{
  if ( mQCopilotsMenu )
    return mQCopilotsMenu.data();

  if ( !mIface )
    return nullptr;

  QMainWindow *mainWindow = qobject_cast<QMainWindow *>( mIface->mainWindow() );
  if ( !mainWindow )
    return nullptr;

  if ( QMenu *existingMenu = mainWindow->findChild<QMenu *>( QStringLiteral( "mQCopilotsMenu" ) ) )
  {
    mQCopilotsMenu = existingMenu;
    mOwnQCopilotsMenu = false;
    return mQCopilotsMenu.data();
  }

  QMenuBar *menuBar = mainWindow->menuBar();
  if ( !menuBar )
    return nullptr;

  QMenu *menu = new QMenu( sMenuName, mainWindow );
  menu->setObjectName( QStringLiteral( "mQCopilotsMenu" ) );

  QAction *before = nullptr;
  if ( QMenu *rightMostMenu = mIface->firstRightStandardMenu() )
    before = rightMostMenu->menuAction();

  if ( before )
    menuBar->insertMenu( before, menu );
  else
    menuBar->addMenu( menu );

  mQCopilotsMenu = menu;
  mOwnQCopilotsMenu = true;
  return mQCopilotsMenu.data();
}

void QgsQCopilotsServerPlugin::addActionToQCopilotsMenu( QAction *action )
{
  if ( !action )
    return;

  if ( QMenu *menu = resolveQCopilotsMenu() )
    menu->addAction( action );
}

void QgsQCopilotsServerPlugin::removeActionFromQCopilotsMenu( QAction *action )
{
  if ( !action || !mQCopilotsMenu )
    return;

  mQCopilotsMenu->removeAction( action );
}

void QgsQCopilotsServerPlugin::updateActionStates()
{
  const bool isRunning = mProcess && mProcess->state() != QProcess::NotRunning;

  if ( mStartAction )
    mStartAction->setEnabled( !isRunning );
  if ( mStopAction )
    mStopAction->setEnabled( isRunning );
  if ( mRestartAction )
    mRestartAction->setEnabled( true );
}

bool QgsQCopilotsServerPlugin::resolveInstallLayout(
  QString &binDirPath,
  QString &backendDirPath,
  QString &configFilePath,
  QString &executablePath,
  QString &runtimeLogPath,
  QString &errorMessage ) const
{
  const QString installPrefix = qEnvironmentVariable( sInstallPrefixEnv.toUtf8().constData() ).trimmed();
  if ( installPrefix.isEmpty() )
  {
    errorMessage = tr( "环境变量 %1 未设置，无法定位 QGIS 安装目录。" ).arg( sInstallPrefixEnv );
    return false;
  }

  const QDir installPrefixDir( QDir::fromNativeSeparators( installPrefix ) );
  const QString qgisRootPath = installPrefixDir.absoluteFilePath( QStringLiteral( "QGIS34407" ) );
  binDirPath = QDir( qgisRootPath ).absoluteFilePath( QStringLiteral( "bin" ) );
  backendDirPath = QDir( binDirPath ).absoluteFilePath( QStringLiteral( "backend" ) );
  configFilePath = QDir( binDirPath ).absoluteFilePath( sQCopilotsConfigFileName );
  executablePath = QDir( backendDirPath ).absoluteFilePath( sLlamaServerExeName );
  runtimeLogPath = QDir( backendDirPath ).absoluteFilePath( sRuntimeLogFileName );

  if ( !QDir( qgisRootPath ).exists() )
  {
    errorMessage = tr( "目录不存在：%1" ).arg( qgisRootPath );
    return false;
  }
  if ( !QDir( binDirPath ).exists() )
  {
    errorMessage = tr( "目录不存在：%1" ).arg( binDirPath );
    return false;
  }
  if ( !QDir( backendDirPath ).exists() )
  {
    errorMessage = tr( "目录不存在：%1" ).arg( backendDirPath );
    return false;
  }
  if ( !QFile::exists( configFilePath ) )
  {
    errorMessage = tr( "配置文件不存在：%1" ).arg( configFilePath );
    return false;
  }
  if ( !QFile::exists( executablePath ) )
  {
    errorMessage = tr( "可执行文件不存在：%1" ).arg( executablePath );
    return false;
  }

  return true;
}

bool QgsQCopilotsServerPlugin::loadLauncherConfig( LauncherConfig &config, QString &errorMessage ) const
{
  QString binDirPath;
  QString backendDirPath;
  QString configFilePath;
  QString executablePath;
  QString runtimeLogPath;
  if ( !resolveInstallLayout( binDirPath, backendDirPath, configFilePath, executablePath, runtimeLogPath, errorMessage ) )
    return false;

  QFile configFile( configFilePath );
  if ( !configFile.open( QIODevice::ReadOnly | QIODevice::Text ) )
  {
    errorMessage = tr( "无法读取配置文件：%1，错误：%2" ).arg( configFilePath, configFile.errorString() );
    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument configDoc = QJsonDocument::fromJson( configFile.readAll(), &parseError );
  if ( parseError.error != QJsonParseError::NoError )
  {
    errorMessage = tr( "配置文件 JSON 解析失败：%1（偏移 %2）" ).arg( parseError.errorString() ).arg( parseError.offset );
    return false;
  }

  if ( !configDoc.isObject() )
  {
    errorMessage = tr( "配置文件格式错误：根节点必须是 JSON 对象。" );
    return false;
  }

  const QJsonObject rootObject = configDoc.object();
  const QJsonValue launcherValue = rootObject.value( QStringLiteral( "launcher" ) );
  if ( !launcherValue.isObject() )
  {
    errorMessage = tr( "配置文件格式错误：缺少对象字段 launcher。" );
    return false;
  }
  const QJsonObject launcherObject = launcherValue.toObject();

  const QJsonValue argumentsValue = launcherObject.value( QStringLiteral( "arguments" ) );
  if ( !argumentsValue.isArray() )
  {
    errorMessage = tr( "配置文件格式错误：launcher.arguments 必须是字符串数组。" );
    return false;
  }

  QStringList arguments;
  const QJsonArray argumentsArray = argumentsValue.toArray();
  for ( int i = 0; i < argumentsArray.count(); ++i )
  {
    const QJsonValue argumentValue = argumentsArray.at( i );
    if ( !argumentValue.isString() )
    {
      errorMessage = tr( "配置文件格式错误：launcher.arguments[%1] 不是字符串。" ).arg( i );
      return false;
    }
    arguments << argumentValue.toString();
  }

  if ( arguments.isEmpty() )
  {
    errorMessage = tr( "配置文件格式错误：launcher.arguments 不能为空数组。" );
    return false;
  }

  QString workingDirectoryPath = backendDirPath;
  const QJsonValue workingDirectoryValue = launcherObject.value( QStringLiteral( "workingDirectory" ) );
  if ( !workingDirectoryValue.isUndefined() && !workingDirectoryValue.isNull() )
  {
    if ( !workingDirectoryValue.isString() )
    {
      errorMessage = tr( "配置文件格式错误：launcher.workingDirectory 必须是字符串。" );
      return false;
    }

    const QString configuredWorkingDirectory = workingDirectoryValue.toString().trimmed();
    if ( !configuredWorkingDirectory.isEmpty() )
    {
      if ( QDir::isAbsolutePath( configuredWorkingDirectory ) )
        workingDirectoryPath = QDir::cleanPath( QDir::fromNativeSeparators( configuredWorkingDirectory ) );
      else
        workingDirectoryPath = QDir( binDirPath ).absoluteFilePath( QDir::fromNativeSeparators( configuredWorkingDirectory ) );
    }
  }

  if ( !QDir( workingDirectoryPath ).exists() )
  {
    errorMessage = tr( "配置无效：workingDirectory 不存在：%1" ).arg( workingDirectoryPath );
    return false;
  }

  QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
  const QJsonValue environmentValue = launcherObject.value( QStringLiteral( "environment" ) );
  if ( !environmentValue.isUndefined() && !environmentValue.isNull() )
  {
    if ( !environmentValue.isObject() )
    {
      errorMessage = tr( "配置文件格式错误：launcher.environment 必须是对象。" );
      return false;
    }

    const QJsonObject environmentObject = environmentValue.toObject();
    for ( auto it = environmentObject.constBegin(); it != environmentObject.constEnd(); ++it )
    {
      if ( !it.value().isString() )
      {
        errorMessage = tr( "配置文件格式错误：launcher.environment.%1 必须是字符串。" ).arg( it.key() );
        return false;
      }
      processEnvironment.insert( it.key(), it.value().toString() );
    }
  }

  const QJsonValue healthValue = launcherObject.value( QStringLiteral( "health" ) );
  if ( !healthValue.isObject() )
  {
    errorMessage = tr( "配置文件格式错误：launcher.health 必须是对象。" );
    return false;
  }

  const QJsonObject healthObject = healthValue.toObject();
  QString healthHost;
  int healthPort = 0;
  if ( !extractHostPortFromArguments( arguments, healthHost, healthPort, errorMessage ) )
    return false;

  const QString healthPath = healthObject.value( QStringLiteral( "path" ) ).toString();
  bool urlOk = false;
  QUrl healthUrl = buildHealthUrl( healthHost, healthPort, healthPath, urlOk, errorMessage );
  if ( !urlOk )
    return false;

  int healthPollIntervalMs = 1000;
  if ( !parsePositiveInt( healthObject, QStringLiteral( "pollIntervalMs" ), 1000, 100, 60000, healthPollIntervalMs, errorMessage ) )
    return false;

  int healthStartupTimeoutSec = 60;
  if ( !parsePositiveInt( healthObject, QStringLiteral( "startupTimeoutSec" ), 60, 1, 3600, healthStartupTimeoutSec, errorMessage ) )
    return false;

  config.configFilePath = configFilePath;
  config.executablePath = executablePath;
  config.runtimeLogPath = runtimeLogPath;
  config.workingDirectory = workingDirectoryPath;
  config.arguments = arguments;
  config.environment = processEnvironment;
  config.healthUrl = healthUrl;
  config.healthPollIntervalMs = healthPollIntervalMs;
  config.healthStartupTimeoutSec = healthStartupTimeoutSec;
  return true;
}

bool QgsQCopilotsServerPlugin::startProcessWithConfig( const LauncherConfig &config, QString &errorMessage )
{
  ensureProcess();
  ensureHealthInfrastructure();
  mStopInProgress = false;
  mStopUsedForceKill = false;

  QFile runtimeLogFile( config.runtimeLogPath );
  if ( !runtimeLogFile.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) )
  {
    errorMessage = tr( "无法写入运行日志文件：%1，错误：%2" ).arg( config.runtimeLogPath, runtimeLogFile.errorString() );
    return false;
  }
  runtimeLogFile.close();

  mConfigPath = config.configFilePath;
  mRuntimeLogPath = config.runtimeLogPath;

  mProcess->setProgram( config.executablePath );
  mProcess->setArguments( config.arguments );
  mProcess->setWorkingDirectory( config.workingDirectory );
  mProcess->setProcessEnvironment( config.environment );
  mProcess->setProcessChannelMode( QProcess::MergedChannels );
  mProcess->setStandardOutputFile( config.runtimeLogPath, QIODevice::Append );
  mProcess->setStandardErrorFile( config.runtimeLogPath, QIODevice::Append );

  logMessage( tr( "准备启动 QCopilotsServer program=%1，workingDirectory=%2，config=%3，runtimeLog=%4" )
                .arg( config.executablePath, config.workingDirectory, config.configFilePath, config.runtimeLogPath ),
              Qgis::Info );

  mProcess->start();
  if ( !mProcess->waitForStarted( sProcessStartTimeoutMs ) )
  {
    errorMessage = tr( "QCopilotsServer 启动失败：%1" ).arg( mProcess->errorString() );
    return false;
  }

  startHealthPolling( config.healthUrl, config.healthPollIntervalMs, config.healthStartupTimeoutSec );
  updateActionStates();
  return true;
}

void QgsQCopilotsServerPlugin::stopProcessInternal( bool notifyIfAlreadyStopped, bool notifyWhenStopped )
{
  stopHealthPolling();

  if ( !mProcess || mProcess->state() == QProcess::NotRunning )
  {
    if ( notifyIfAlreadyStopped )
    {
      const QString message = tr( "QCopilotsServer 未在运行，无需关闭。" );
      logMessage( message, Qgis::Info );
      pushMessageBar( message, Qgis::Info );
    }
    updateActionStates();
    return;
  }

  mStopInProgress = true;
  mStopUsedForceKill = false;
  logMessage( tr( "正在停止 QCopilotsServer 进程..." ), Qgis::Info );
  mProcess->terminate();
  if ( !mProcess->waitForFinished( sTerminateTimeoutMs ) )
  {
    mStopUsedForceKill = true;
    logMessage( tr( "terminate 超时，开始强制 kill 进程。" ), Qgis::Warning );
    mProcess->kill();
    if ( !mProcess->waitForFinished( sKillTimeoutMs ) )
      logMessage( tr( "kill 后进程仍未结束，请检查系统进程状态。" ), Qgis::Warning );
  }

  if ( mProcess->state() != QProcess::NotRunning )
  {
    // Stop flow failed to complete; avoid masking later genuine crash events.
    mStopInProgress = false;
    mStopUsedForceKill = false;
  }

  if ( notifyWhenStopped )
  {
    const QString message = tr( "QCopilotsServer 进程已停止。" );
    logMessage( message, Qgis::Info );
    pushMessageBar( message, Qgis::Info );
  }

  updateActionStates();
}

QUrl QgsQCopilotsServerPlugin::buildHealthUrl( const QString &host, int port, const QString &path, bool &ok, QString &errorMessage ) const
{
  ok = false;
  QString normalizedHost = host.trimmed();
  if ( normalizedHost.isEmpty() )
  {
    errorMessage = tr( "配置无效：launcher.arguments 中 --host 不能为空。" );
    return QUrl();
  }

  if ( normalizedHost.startsWith( QLatin1Char( '[' ) ) && normalizedHost.endsWith( QLatin1Char( ']' ) ) && normalizedHost.size() > 2 )
    normalizedHost = normalizedHost.mid( 1, normalizedHost.size() - 2 );

  if ( normalizedHost.contains( QStringLiteral( "://" ) ) || normalizedHost.contains( QLatin1Char( '/' ) ) )
  {
    errorMessage = tr( "配置无效：launcher.arguments 中 --host 仅允许主机名或 IP，不允许协议或路径：%1" ).arg( host );
    return QUrl();
  }

  if ( port < 1 || port > 65535 )
  {
    errorMessage = tr( "配置无效：launcher.arguments 中 --port 超出范围：%1（应为 1-65535）。" ).arg( port );
    return QUrl();
  }

  QString normalizedPath = path.trimmed();
  if ( normalizedPath.isEmpty() )
    normalizedPath = QStringLiteral( "/health" );
  if ( !normalizedPath.startsWith( QLatin1Char( '/' ) ) )
    normalizedPath.prepend( QLatin1Char( '/' ) );

  QUrl healthUrl;
  healthUrl.setScheme( QStringLiteral( "http" ) );
  healthUrl.setHost( normalizedHost );
  healthUrl.setPort( port );
  healthUrl.setPath( normalizedPath );

  if ( !healthUrl.isValid() || healthUrl.host().isEmpty() )
  {
    errorMessage = tr( "配置无效：根据 --host/--port 拼接健康检查 URL 失败，host=%1，port=%2，path=%3。" )
                     .arg( host )
                     .arg( port )
                     .arg( normalizedPath );
    return QUrl();
  }

  ok = true;
  return healthUrl;
}

bool QgsQCopilotsServerPlugin::extractHostPortFromArguments(
  const QStringList &arguments,
  QString &host,
  int &port,
  QString &errorMessage ) const
{
  QString hostValue;
  QString portValue;

  auto readOptionValue = [&]( int &index, const QString &optionName, QString &value ) -> bool
  {
    if ( index + 1 >= arguments.count() )
    {
      errorMessage = tr( "配置无效：launcher.arguments 中 %1 缺少取值。" ).arg( optionName );
      return false;
    }

    value = arguments.at( ++index ).trimmed();
    if ( value.isEmpty() )
    {
      errorMessage = tr( "配置无效：launcher.arguments 中 %1 取值不能为空。" ).arg( optionName );
      return false;
    }
    return true;
  };

  for ( int i = 0; i < arguments.count(); ++i )
  {
    const QString argument = arguments.at( i ).trimmed();

    if ( argument.compare( QStringLiteral( "--host" ), Qt::CaseInsensitive ) == 0 )
    {
      if ( !readOptionValue( i, QStringLiteral( "--host" ), hostValue ) )
        return false;
      continue;
    }
    if ( argument.startsWith( QStringLiteral( "--host=" ), Qt::CaseInsensitive ) )
    {
      hostValue = argument.mid( QStringLiteral( "--host=" ).size() ).trimmed();
      if ( hostValue.isEmpty() )
      {
        errorMessage = tr( "配置无效：launcher.arguments 中 --host 取值不能为空。" );
        return false;
      }
      continue;
    }
    if ( argument.compare( QStringLiteral( "--port" ), Qt::CaseInsensitive ) == 0 )
    {
      if ( !readOptionValue( i, QStringLiteral( "--port" ), portValue ) )
        return false;
      continue;
    }
    if ( argument.startsWith( QStringLiteral( "--port=" ), Qt::CaseInsensitive ) )
    {
      portValue = argument.mid( QStringLiteral( "--port=" ).size() ).trimmed();
      if ( portValue.isEmpty() )
      {
        errorMessage = tr( "配置无效：launcher.arguments 中 --port 取值不能为空。" );
        return false;
      }
      continue;
    }
  }

  if ( hostValue.isEmpty() )
  {
    errorMessage = tr( "配置无效：launcher.arguments 必须包含 --host 参数，用于拼接健康检查地址。" );
    return false;
  }

  if ( portValue.isEmpty() )
  {
    errorMessage = tr( "配置无效：launcher.arguments 必须包含 --port 参数，用于拼接健康检查地址。" );
    return false;
  }

  bool portOk = false;
  const int parsedPort = portValue.toInt( &portOk );
  if ( !portOk || parsedPort < 1 || parsedPort > 65535 )
  {
    errorMessage = tr( "配置无效：launcher.arguments 中 --port 取值非法：%1（需为 1-65535 的整数）。" ).arg( portValue );
    return false;
  }

  host = hostValue;
  port = parsedPort;
  return true;
}

bool QgsQCopilotsServerPlugin::parsePositiveInt(
  const QJsonObject &object,
  const QString &name,
  int defaultValue,
  int minValue,
  int maxValue,
  int &value,
  QString &errorMessage ) const
{
  const QJsonValue rawValue = object.value( name );
  if ( rawValue.isUndefined() || rawValue.isNull() )
  {
    value = defaultValue;
    return true;
  }

  if ( !rawValue.isDouble() )
  {
    errorMessage = tr( "配置文件格式错误：launcher.health.%1 必须是整数。" ).arg( name );
    return false;
  }

  const int candidate = rawValue.toInt();
  if ( candidate < minValue || candidate > maxValue )
  {
    errorMessage = tr( "配置文件格式错误：launcher.health.%1 必须在 [%2, %3] 范围内。" ).arg( name ).arg( minValue ).arg( maxValue );
    return false;
  }

  value = candidate;
  return true;
}

void QgsQCopilotsServerPlugin::startHealthPolling( const QUrl &healthUrl, int pollIntervalMs, int startupTimeoutSec )
{
  ensureHealthInfrastructure();
  stopHealthPolling();

  mHealthUrl = healthUrl;
  mHealthPollIntervalMs = pollIntervalMs;
  mHealthDeadlineUtc = QDateTime::currentDateTimeUtc().addSecs( startupTimeoutSec );
  mHealthAttempt = 0;
  mHealthRequestInFlight = false;
  ++mHealthSessionId;

  mHealthTimer->setInterval( mHealthPollIntervalMs );
  mHealthTimer->start();

  logMessage( tr( "健康检查已启动：url=%1，interval=%2ms，timeout=%3s（默认端点为 llama.cpp 的 GET /health，可由配置覆盖）。" )
                .arg( mHealthUrl.toString() )
                .arg( mHealthPollIntervalMs )
                .arg( startupTimeoutSec ),
              Qgis::Info );

  onHealthTimerTimeout();
}

void QgsQCopilotsServerPlugin::stopHealthPolling()
{
  ++mHealthSessionId;
  mHealthRequestInFlight = false;
  if ( mHealthTimer )
    mHealthTimer->stop();
}

void QgsQCopilotsServerPlugin::finalizeHealthFailure( const QString &message )
{
  logMessage( message, Qgis::Warning );
  pushMessageBar( message, Qgis::Warning );
  stopHealthPolling();
}

QString QgsQCopilotsServerPlugin::processStateText() const
{
  if ( !mProcess )
    return tr( "NotRunning" );

  switch ( mProcess->state() )
  {
    case QProcess::NotRunning:
      return tr( "NotRunning" );
    case QProcess::Starting:
      return tr( "Starting" );
    case QProcess::Running:
      return tr( "Running" );
  }

  return tr( "Unknown" );
}

void QgsQCopilotsServerPlugin::logMessage( const QString &message, Qgis::MessageLevel level ) const
{
  const QString timestamped = QStringLiteral( "[%1] %2" )
                                .arg( QDateTime::currentDateTime().toString( QStringLiteral( "yyyy-MM-dd HH:mm:ss" ) ),
                                      message );
  QgsMessageLog::logMessage( timestamped, sLogCategory, level );
}

void QgsQCopilotsServerPlugin::pushMessageBar( const QString &message, Qgis::MessageLevel level ) const
{
  if ( !mIface || !mIface->messageBar() )
    return;

  switch ( level )
  {
    case Qgis::Critical:
      mIface->messageBar()->pushCritical( sName, message );
      break;
    case Qgis::Warning:
      mIface->messageBar()->pushWarning( sName, message );
      break;
    case Qgis::Success:
      mIface->messageBar()->pushSuccess( sName, message );
      break;
    case Qgis::Info:
    case Qgis::NoLevel:
      mIface->messageBar()->pushInfo( sName, message );
      break;
  }
}

QGISEXTERN QgisPlugin *classFactory( QgisInterface *qgisInterfacePointer )
{
  return new QgsQCopilotsServerPlugin( qgisInterfacePointer );
}

QGISEXTERN const QString *name()
{
  return &sName;
}

QGISEXTERN const QString *description()
{
  return &sDescription;
}

QGISEXTERN const QString *category()
{
  return &sCategory;
}

QGISEXTERN int type()
{
  return sPluginType;
}

QGISEXTERN const QString *version()
{
  return &sPluginVersion;
}

QGISEXTERN const QString *icon()
{
  return &sPluginIcon;
}

QGISEXTERN void unload( QgisPlugin *pluginPointer )
{
  delete pluginPointer;
}
