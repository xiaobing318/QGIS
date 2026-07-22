/***************************************************************************
    qcopilots_container_plugin.cpp
    ------------------------------
    begin                : July 2026
    copyright            : (C) 2026
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qcopilots_container_plugin.h"
#include "moc_qcopilots_container_plugin.cpp"

#include "qgisinterface.h"
#include "qgsmessagebar.h"
#include "qcopilots_container_dock.h"
#include "qcopilots_container_utils.h"

#include <QAction>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

static const QString sName = QObject::tr( "QCopilots" );
static const QString sDescription = QObject::tr( "QCopilots browser container for an existing Web UI." );
static const QString sCategory = QObject::tr( "QCopilots" );
static const QString sPluginVersion = QObject::tr( "Version 1.0" );
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
static const QString sPluginIcon = QStringLiteral( ":/qcopilots/icons/qcopilots-plugin.svg" );

QgsQCopilotsPlugin::QgsQCopilotsPlugin( QgisInterface *iface )
  : QgisPlugin( sName, sDescription, sCategory, sPluginVersion, sPluginType )
  , mIface( iface )
{
}

void QgsQCopilotsPlugin::initGui()
{
  ensureDock();
  ensureMenu();

  mToggleAction = new QAction( QIcon( sPluginIcon ), tr( "QCopilots" ), this );
  mToggleAction->setObjectName( QStringLiteral( "mActionQCopilots" ) );
  mToggleAction->setCheckable( true );
  mToggleAction->setChecked( mDock && mDock->isVisible() );
  mToggleAction->setWhatsThis( tr( "Show or hide the QCopilots browser container" ) );
  connect( mToggleAction, &QAction::toggled, this, &QgsQCopilotsPlugin::toggleDock );

  mConfigureUrlAction = new QAction(
    QIcon( QStringLiteral( ":/qcopilots/icons/qcopilots-configureurl.svg" ) ),
    tr( "Configure QCopilots URL" ),
    this
  );
  mConfigureUrlAction->setObjectName( QStringLiteral( "mActionConfigureQCopilotsUrl" ) );
  mConfigureUrlAction->setWhatsThis( tr( "Configure the QCopilots web UI URL" ) );
  connect( mConfigureUrlAction, &QAction::triggered, this, &QgsQCopilotsPlugin::configureServerUrl );

  if ( mIface )
  {
    mIface->addToolBarIcon( mToggleAction );
    mIface->addToolBarIcon( mConfigureUrlAction );
  }

  if ( mMenu )
  {
    mMenu->addAction( mToggleAction );
    mMenuSeparator = mMenu->addSeparator();
    mMenu->addAction( mConfigureUrlAction );
  }

  if ( mDock )
  {
    connect( mDock, &QDockWidget::visibilityChanged, mToggleAction, &QAction::setChecked );
    connect( mDock, &QgsQCopilotsDock::configureUrlRequested, this, &QgsQCopilotsPlugin::configureServerUrl );
    connect( mDock, &QgsQCopilotsDock::loadFailed, this, &QgsQCopilotsPlugin::handleDockLoadFailed );
  }

  QgsQCopilotsUtils::logMessage( tr( "QCopilots plugin initialized. Default URL is %1." ).arg( QgsQCopilotsUtils::defaultServerUrl().toString() ), Qgis::Info );
}

void QgsQCopilotsPlugin::unload()
{
  if ( mDock )
  {
    disconnect( mDock, nullptr, this, nullptr );
  }

  if ( mIface )
  {
    if ( mToggleAction )
      mIface->removeToolBarIcon( mToggleAction );
    if ( mConfigureUrlAction )
      mIface->removeToolBarIcon( mConfigureUrlAction );
  }

  if ( mMenu )
  {
    if ( mToggleAction )
      mMenu->removeAction( mToggleAction );
    if ( mMenuSeparator )
    {
      mMenu->removeAction( mMenuSeparator );
      delete mMenuSeparator;
      mMenuSeparator = nullptr;
    }
    if ( mConfigureUrlAction )
      mMenu->removeAction( mConfigureUrlAction );

    if ( mOwnsMenu && mMenu->actions().isEmpty() )
    {
      if ( QMainWindow *mainWindow = qobject_cast<QMainWindow *>( mIface ? mIface->mainWindow() : nullptr ) )
      {
        if ( QMenuBar *menuBar = mainWindow->menuBar() )
          menuBar->removeAction( mMenu->menuAction() );
      }
      delete mMenu;
    }
  }
  mMenu = nullptr;
  mOwnsMenu = false;
  mMenuSeparator = nullptr;

  delete mToggleAction;
  mToggleAction = nullptr;
  delete mConfigureUrlAction;
  mConfigureUrlAction = nullptr;

  if ( mIface && mDock )
    mIface->removeDockWidget( mDock );

  delete mDock;
  mDock = nullptr;

  QgsQCopilotsUtils::logMessage( tr( "QCopilots plugin unloaded." ), Qgis::Info );
}

void QgsQCopilotsPlugin::ensureDock()
{
  if ( mDock || !mIface )
    return;

  mDock = new QgsQCopilotsDock( mIface->mainWindow() );
  mDock->setObjectName( QStringLiteral( "QCopilotsDock" ) );
  mIface->addDockWidget( Qt::RightDockWidgetArea, mDock );
  mDock->show();
  mDock->raise();
}

void QgsQCopilotsPlugin::ensureMenu()
{
  if ( mMenu || !mIface )
    return;

  QMainWindow *mainWindow = qobject_cast<QMainWindow *>( mIface->mainWindow() );
  if ( !mainWindow )
    return;

  QMenuBar *menuBar = mainWindow->menuBar();
  if ( !menuBar )
    return;

  const QList<QAction *> actions = menuBar->actions();
  for ( QAction *action : actions )
  {
    QMenu *candidate = action ? action->menu() : nullptr;
    if ( candidate && ( candidate->objectName() == QLatin1String( "mQCopilotsMenu" ) || QString( candidate->title() ).remove( QLatin1Char( '&' ) ) == QLatin1String( "QCopilots" ) ) )
    {
      mMenu = candidate;
      mOwnsMenu = false;
      return;
    }
  }

  mMenu = new QMenu( tr( "&QCopilots" ), mainWindow );
  mMenu->setObjectName( QStringLiteral( "mQCopilotsMenu" ) );
  mMenu->setProperty( "qcopilotsCreatedByPlugin", true );
  mOwnsMenu = true;

  QAction *before = nullptr;
  if ( QMenu *rightMostMenu = mIface->firstRightStandardMenu() )
    before = rightMostMenu->menuAction();

  if ( before )
    menuBar->insertMenu( before, mMenu );
  else
    menuBar->addMenu( mMenu );
}

void QgsQCopilotsPlugin::toggleDock( bool visible )
{
  ensureDock();
  if ( !mDock )
    return;

  mDock->setVisible( visible );
  if ( visible )
  {
    mDock->raise();
    mDock->activateWindow();
  }
}

void QgsQCopilotsPlugin::configureServerUrl()
{
  ensureDock();
  if ( !mDock || !mIface )
    return;

  bool ok = false;
  const QString input = QInputDialog::getText( mIface->mainWindow(), tr( "QCopilots" ), tr( "QCopilots URL:" ), QLineEdit::Normal, mDock->configuredUrl().toString(), &ok );
  if ( !ok )
    return;

  const QUrl url = QgsQCopilotsUtils::urlFromUserInput( input );
  if ( !url.isValid() )
  {
    const QString message = tr( "Invalid QCopilots URL. Use an http or https URL." );
    mIface->messageBar()->pushWarning( tr( "QCopilots" ), message );
    QgsQCopilotsUtils::logMessage( message, Qgis::Warning );
    return;
  }

  mDock->loadUrl( url, true );
  mDock->show();
  mDock->raise();
}

void QgsQCopilotsPlugin::handleDockLoadFailed( const QUrl &url )
{
  if ( !mIface )
    return;

  QString message = tr( "Failed to load QCopilots URL %1." ).arg( QgsQCopilotsUtils::displayUrl( url ) );
  if ( mDock && !mDock->lastFailureSummary().isEmpty() )
    message += tr( " Details: %1" ).arg( mDock->lastFailureSummary() );

  mIface->messageBar()->pushWarning( tr( "QCopilots" ), message );
}

//////////////////////////////////////////////////////////////////////////
//
// THE FOLLOWING METHODS ARE MANDATORY FOR ALL PLUGINS
//
//////////////////////////////////////////////////////////////////////////

QGISEXTERN QgisPlugin *classFactory( QgisInterface *qgisInterfacePointer )
{
  return new QgsQCopilotsPlugin( qgisInterfacePointer );
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
