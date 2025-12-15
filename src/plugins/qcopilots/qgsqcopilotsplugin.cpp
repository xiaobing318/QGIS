/***************************************************************************
 *  qgsqcopilotsplugin.cpp                                                *
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 ***************************************************************************/

#include "qgsqcopilotsplugin.h"
#include "moc_qgsqcopilotsplugin.cpp"

#include "qgisinterface.h"
#include "qgsapplication.h"
#include "qgsmessagebar.h"
#include "qgsqcopilotsdock.h"

#include <QAction>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

QgsQCopilotsPlugin::QgsQCopilotsPlugin( QgisInterface *iface )
  : QgisPlugin( sName, sDescription, sCategory, sPluginVersion, sPluginType )
  , mIface( iface )
{
}

void QgsQCopilotsPlugin::initGui()
{
  ensureDock();

  if ( !mQgsQCopilotsDock )
    return;

  /*
  1. 设置 QCopilots 交互界面显示
  */
  mToggleAction = new QAction( QIcon( sPluginIcon ), tr( "QCopilots" ), this );
  mToggleAction->setObjectName( QStringLiteral( "QCopilotsAction" ) );
  mToggleAction->setWhatsThis( tr( "Show/hide the QCopilots dock" ) );
  mToggleAction->setCheckable( true );
  mToggleAction->setChecked( mQgsQCopilotsDock->isVisible() );
  connect( mToggleAction, &QAction::toggled, mQgsQCopilotsDock, &QWidget::setVisible );
  connect( mQgsQCopilotsDock, &QDockWidget::visibilityChanged, mToggleAction, &QAction::setChecked );
  mIface->addToolBarIcon( mToggleAction );

  /*
  1. 设置 QCopilot Server 地址
  */
  mConfigureUrlAction = new QAction( QIcon( QStringLiteral( ":/qcopilots/icons/qcopilots-configureurl.svg" ) ), tr( "SetQCopilotsURL" ), this );
  mConfigureUrlAction->setObjectName( QStringLiteral( "QCopilotsConfigureUrlAction" ) );
  mConfigureUrlAction->setWhatsThis( tr( "Configure the server URL used by QCopilots" ) );
  connect( mConfigureUrlAction, &QAction::triggered, this, &QgsQCopilotsPlugin::configureServerUrl );
  mIface->addToolBarIcon( mConfigureUrlAction );

  if ( !mMenu )
  {
    QMainWindow *mainWindow = qobject_cast<QMainWindow *>( mIface->mainWindow() );
    if ( !mainWindow )
      return;

    mMenu = new QMenu( tr( "&QCopilots" ), mainWindow );
    mMenu->setObjectName( QStringLiteral( "mQCopilotsMenu" ) );

    if ( QMenuBar *menuBar = mainWindow->menuBar() )
    {
      QAction *before = nullptr;
      if ( QMenu *rightMostMenu = mIface->firstRightStandardMenu() )
        before = rightMostMenu->menuAction();

      if ( before )
        menuBar->insertMenu( before, mMenu );
      else
        menuBar->addMenu( mMenu );
    }
  }

  mMenu->clear();
  mMenu->addAction( mToggleAction );
  mMenu->addSeparator();
  mMenu->addAction( mConfigureUrlAction );

  connect( mQgsQCopilotsDock, &QgsQCopilotsDock::loadFailed, this, &QgsQCopilotsPlugin::handleDockLoadFailed );
}

void QgsQCopilotsPlugin::unload()
{
  if ( mIface )
  {
    if ( mToggleAction )
    {
      mIface->removeToolBarIcon( mToggleAction );
    }

    if ( mConfigureUrlAction )
    {
      mIface->removeToolBarIcon( mConfigureUrlAction );
    }
  }

  if ( mMenu )
  {
    if ( QMainWindow *mainWindow = qobject_cast<QMainWindow *>( mIface ? mIface->mainWindow() : nullptr ) )
    {
      if ( QMenuBar *menuBar = mainWindow->menuBar() )
        menuBar->removeAction( mMenu->menuAction() );
    }

    delete mMenu;
    mMenu = nullptr;
  }

  delete mToggleAction;
  mToggleAction = nullptr;

  delete mConfigureUrlAction;
  mConfigureUrlAction = nullptr;

  if ( mIface && mQgsQCopilotsDock )
  {
    mIface->removeDockWidget( mQgsQCopilotsDock );
  }

  delete mQgsQCopilotsDock;
  mQgsQCopilotsDock = nullptr;
}

void QgsQCopilotsPlugin::configureServerUrl()
{
  if ( !mIface || !mQgsQCopilotsDock )
    return;

  const QString currentUrl = mQgsQCopilotsDock->serverUrl().toString();
  bool ok = false;
  const QString urlString = QInputDialog::getText( mIface->mainWindow(), tr( "QCopilots" ), tr( "Set QCopilots Server URL:" ), QLineEdit::Normal, currentUrl, &ok ).trimmed();
  if ( !ok || urlString.isEmpty() )
    return;

  const QUrl url = QUrl::fromUserInput( urlString );
  if ( !url.isValid() || url.scheme().isEmpty() )
  {
    mIface->messageBar()->pushWarning( tr( "QCopilots" ), tr( "Invalid URL: %1" ).arg( urlString ) );
    return;
  }

  mQgsQCopilotsDock->loadUrl( url );
  mQgsQCopilotsDock->show();
  mQgsQCopilotsDock->raise();
}

void QgsQCopilotsPlugin::handleDockLoadFailed( const QUrl &url )
{
  if ( !mIface )
    return;

  mIface->messageBar()->pushWarning( tr( "QCopilots" ), tr( "Failed to load %1. Is QCopilots server running?" ).arg( url.toString() ) );
}

void QgsQCopilotsPlugin::ensureDock()
{
  if ( mQgsQCopilotsDock || !mIface )
    return;

  mQgsQCopilotsDock = new QgsQCopilotsDock( mIface->mainWindow() );
  mQgsQCopilotsDock->setObjectName( QStringLiteral( "QCopilotsDock" ) );
  mIface->addDockWidget( Qt::RightDockWidgetArea, mQgsQCopilotsDock );
  mQgsQCopilotsDock->raise();
}


//////////////////////////////////////////////////////////////////////////
//
//
//  THE FOLLOWING CODE IS AUTOGENERATED BY THE PLUGIN BUILDER SCRIPT
//    YOU WOULD NORMALLY NOT NEED TO MODIFY THIS, AND YOUR PLUGIN
//      MAY NOT WORK PROPERLY IF YOU MODIFY THIS INCORRECTLY
//
//
//////////////////////////////////////////////////////////////////////////


/**
 * Required extern functions needed  for every plugin
 * These functions can be called prior to creating an instance
 * of the plugin class
 */
// Class factory to return a new instance of the plugin class
QGISEXTERN QgisPlugin *classFactory( QgisInterface *qgisInterfacePointer )
{
  return new QgsQCopilotsPlugin( qgisInterfacePointer );
}
// Return the name of the plugin - note that we do not use class members as
// the class may not yet be instantiated when this method is called.
QGISEXTERN const QString *name()
{
  return &sName;
}

// Return the description
QGISEXTERN const QString *description()
{
  return &sDescription;
}

// Return the category
QGISEXTERN const QString *category()
{
  return &sCategory;
}

// Return the type (either UI or MapLayer plugin)
QGISEXTERN int type()
{
  return sPluginType;
}

// Return the version number for the plugin
QGISEXTERN const QString *version()
{
  return &sPluginVersion;
}

QGISEXTERN const QString *icon()
{
  return &sPluginIcon;
}

// Delete ourself
QGISEXTERN void unload( QgisPlugin *pluginPointer )
{
  delete pluginPointer;
}
