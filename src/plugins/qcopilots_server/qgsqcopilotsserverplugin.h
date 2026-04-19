/***************************************************************************
 *  qgsqcopilotsserverplugin.h                                          *
 *                                                                         *
 *  Standalone launcher plugin for managing llama-server lifecycle.        *
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 ***************************************************************************/

#ifndef QGS_QCOPILOTS_SERVER_PLUGIN_H
#define QGS_QCOPILOTS_SERVER_PLUGIN_H

#include "qgis.h"
#include "qgisplugin.h"

#include <QJsonObject>
#include <QDateTime>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QUrl>

class QAction;
class QNetworkAccessManager;
class QMenu;
class QTimer;
class QgisInterface;

class QgsQCopilotsServerPlugin : public QObject, public QgisPlugin
{
    Q_OBJECT

  public:
    explicit QgsQCopilotsServerPlugin( QgisInterface *iface );
    ~QgsQCopilotsServerPlugin() override;

    void initGui() override;
    void unload() override;

  private slots:
    void startBackendModel();
    void stopBackendModel();
    void applyConfigAndRestart();
    void onProcessStarted();
    void onProcessFinished( int exitCode, QProcess::ExitStatus exitStatus );
    void onProcessErrorOccurred( QProcess::ProcessError error );
    void onHealthTimerTimeout();

  private:
    struct LauncherConfig
    {
      QString configFilePath;
      QString executablePath;
      QString runtimeLogPath;
      QString workingDirectory;
      QStringList arguments;
      QProcessEnvironment environment;
      QUrl healthUrl;
      int healthPollIntervalMs = 1000;
      int healthStartupTimeoutSec = 60;
    };

    void ensureProcess();
    void ensureHealthInfrastructure();
    QMenu *resolveQCopilotsMenu();
    void addActionToQCopilotsMenu( QAction *action );
    void removeActionFromQCopilotsMenu( QAction *action );
    void updateActionStates();

    bool resolveInstallLayout(
      QString &binDirPath,
      QString &backendDirPath,
      QString &configFilePath,
      QString &executablePath,
      QString &runtimeLogPath,
      QString &errorMessage ) const;

    bool loadLauncherConfig( LauncherConfig &config, QString &errorMessage ) const;
    bool startProcessWithConfig( const LauncherConfig &config, QString &errorMessage );
    void stopProcessInternal( bool notifyIfAlreadyStopped, bool notifyWhenStopped );

    bool extractHostPortFromArguments(
      const QStringList &arguments,
      QString &host,
      int &port,
      QString &errorMessage ) const;
    QUrl buildHealthUrl( const QString &host, int port, const QString &path, bool &ok, QString &errorMessage ) const;
    bool parsePositiveInt(
      const QJsonObject &object,
      const QString &name,
      int defaultValue,
      int minValue,
      int maxValue,
      int &value,
      QString &errorMessage ) const;

    void startHealthPolling( const QUrl &healthUrl, int pollIntervalMs, int startupTimeoutSec );
    void stopHealthPolling();
    void finalizeHealthFailure( const QString &message );

    QString processStateText() const;
    void logMessage( const QString &message, Qgis::MessageLevel level = Qgis::Info ) const;
    void pushMessageBar( const QString &message, Qgis::MessageLevel level = Qgis::Info ) const;

    QgisInterface *mIface = nullptr;

    QAction *mStartAction = nullptr;
    QAction *mStopAction = nullptr;
    QAction *mRestartAction = nullptr;
    QPointer<QMenu> mQCopilotsMenu;
    bool mOwnQCopilotsMenu = false;

    QProcess *mProcess = nullptr;
    QNetworkAccessManager *mNetworkAccessManager = nullptr;
    QTimer *mHealthTimer = nullptr;

    QUrl mHealthUrl;
    QDateTime mHealthDeadlineUtc;
    int mHealthPollIntervalMs = 1000;
    int mHealthAttempt = 0;
    quint64 mHealthSessionId = 0;
    bool mHealthRequestInFlight = false;
    bool mStopInProgress = false;
    bool mStopUsedForceKill = false;

    QString mRuntimeLogPath;
    QString mConfigPath;
};

#endif // QGS_QCOPILOTS_SERVER_PLUGIN_H

