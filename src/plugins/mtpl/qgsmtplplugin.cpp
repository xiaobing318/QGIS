/***************************************************************************
  qgsmtplplugin.cpp
  -----------------
  copyright            : (C) 2026 QGIS Project
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmtplplugin.h"

#include "qgsmtpldockwidget.h"
#include "qgsmtplpluginlayer.h"

#include "qgis.h"
#include "qgisinterface.h"
#include "qgsapplication.h"
#include "qgsmapcanvas.h"
#include "qgsmessagebar.h"
#include "qgspluginlayerregistry.h"
#include "qgsproject.h"

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QStringList>

#include "moc_qgsmtplplugin.cpp"

namespace
{
const QString sName = QObject::tr( "MTPL 数据包" );
const QString sDescription = QObject::tr( "加载、查看、创建、加密和解密 MTPL 数据包。" );
const QString sCategory = QObject::tr( "栅格" );
const QString sPluginVersion = QStringLiteral( "版本 1.0" );
const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
const QString sPluginIcon = QStringLiteral( ":/images/themes/default/mActionAddRasterLayer.svg" );
}

QgsMtplPlugin::QgsMtplPlugin( QgisInterface *iface )
  : QgisPlugin( sName, sDescription, sCategory, sPluginVersion, sPluginType )
  , mIface( iface )
{}

void QgsMtplPlugin::initGui()
{
  if ( !mIface )
    return;

  mLayerTypeRegistered = QgsApplication::pluginLayerRegistry()->addPluginLayerType( new QgsMtplPluginLayerType() );
  ensureDock();
  ensureMenu();

  mToggleAction = new QAction( QIcon( sPluginIcon ), tr( "MTPL 数据包" ), this );
  mToggleAction->setObjectName( QStringLiteral( "mActionMtplPackages" ) );
  mToggleAction->setCheckable( true );
  mToggleAction->setWhatsThis( tr( "显示或隐藏 MTPL 数据包面板" ) );
  if ( mDock )
  {
    mDock->setToggleVisibilityAction( mToggleAction );
    mToggleAction->setChecked( mDock->isVisible() );
  }

  mIface->addToolBarIcon( mToggleAction );
  if ( mMenu )
    mMenu->addAction( mToggleAction );
}

void QgsMtplPlugin::unload()
{
  if ( mDock )
    mDock->cancelPendingOperations();

  QgsMapCanvas *canvas = mIface ? mIface->mapCanvas() : nullptr;
  bool canvasWasFrozen = false;
  bool previewJobsWereEnabled = false;
  if ( canvas )
  {
    canvasWasFrozen = canvas->isFrozen();
    previewJobsWereEnabled = canvas->previewJobsEnabled();
    canvas->freeze( true );
    canvas->setPreviewJobsEnabled( false );

    // This blocks until the main and preview renderer jobs owned by this canvas
    // are canceled and destroyed, without waiting for unrelated global work.
    canvas->cancelJobs();
    QCoreApplication::processEvents( QEventLoop::ExcludeUserInputEvents );
    QCoreApplication::sendPostedEvents( nullptr, QEvent::DeferredDelete );
  }

  if ( mIface && mToggleAction )
    mIface->removeToolBarIcon( mToggleAction );

  if ( mMenu )
  {
    if ( mToggleAction )
      mMenu->removeAction( mToggleAction );

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

  if ( mIface && mDock )
    mIface->removeDockWidget( mDock );
  delete mDock;
  mDock = nullptr;

  if ( mLayerTypeRegistered )
  {
    QgsApplication::pluginLayerRegistry()->removePluginLayerType( QgsMtplPluginLayer::layerTypeKey() );
    mLayerTypeRegistered = false;
  }

  if ( canvas )
  {
    canvas->setPreviewJobsEnabled( previewJobsWereEnabled );
    canvas->freeze( canvasWasFrozen );
    if ( !canvasWasFrozen )
      canvas->refresh();
  }

  delete mToggleAction;
  mToggleAction = nullptr;
}

void QgsMtplPlugin::ensureDock()
{
  if ( mDock || !mIface )
    return;

  mDock = new QgsMtplDockWidget( mIface->mainWindow() );
  mIface->addDockWidget( Qt::RightDockWidgetArea, mDock );
  mDock->show();
  mDock->raise();

  connect( mDock, &QgsMtplDockWidget::layersLoaded, this, [this]( int count ) {
    if ( mIface )
      mIface->messageBar()->pushSuccess( tr( "MTPL 数据包" ), tr( "已加载 %1 个图层。" ).arg( count ) );
  } );
  connect( mDock, &QgsMtplDockWidget::loadPartiallySucceeded, this, [this]( int loadedCount, int failedItemCount, const QStringList &messages ) {
    if ( !mIface )
      return;

    QString message = tr( "已加载 %1 个图层，%2 项失败。" ).arg( loadedCount ).arg( failedItemCount );
    const QStringList details = messages.mid( 0, 3 );
    if ( !details.isEmpty() )
      message += QLatin1Char( '\n' ) + details.join( QLatin1Char( '\n' ) );
    mIface->messageBar()->pushWarning( tr( "MTPL 数据包" ), message );
  } );
  connect( mDock, &QgsMtplDockWidget::loadFailed, this, [this]( const QString &message ) {
    if ( mIface )
      mIface->messageBar()->pushWarning( tr( "MTPL 数据包" ), message );
  } );
  connect( mDock, &QgsMtplDockWidget::existingLayerActivated, this, [this]( const QString &layerId )
  {
    if ( !mIface )
      return;
    QgsMapLayer *layer = QgsProject::instance()->mapLayer( layerId );
    if ( !layer )
      return;
    mIface->setActiveLayer( layer );
    mIface->zoomToActiveLayer();
  } );
  connect( mDock, &QgsMtplDockWidget::messageRequested, this,
           [this]( const QString &title, const QString &message, Qgis::MessageLevel level )
  {
    if ( mIface )
      mIface->messageBar()->pushMessage( title, message, level );
  } );
}

void QgsMtplPlugin::ensureMenu()
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
    if ( candidate && ( candidate->objectName() == QLatin1String( "mMtplMenu" ) || QString( candidate->title() ).remove( QLatin1Char( '&' ) ) == QLatin1String( "MTPL" ) ) )
    {
      mMenu = candidate;
      mOwnsMenu = false;
      return;
    }
  }

  mMenu = new QMenu( tr( "&MTPL" ), mainWindow );
  mMenu->setObjectName( QStringLiteral( "mMtplMenu" ) );
  mOwnsMenu = true;

  QAction *before = nullptr;
  if ( QMenu *rightMostMenu = mIface->firstRightStandardMenu() )
    before = rightMostMenu->menuAction();

  if ( before )
    menuBar->insertMenu( before, mMenu );
  else
    menuBar->addMenu( mMenu );
}

QGISEXTERN QgisPlugin *classFactory( QgisInterface *qgisInterfacePointer )
{
  return new QgsMtplPlugin( qgisInterfacePointer );
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
