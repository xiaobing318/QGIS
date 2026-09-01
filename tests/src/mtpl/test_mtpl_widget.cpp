/***************************************************************************
  test_mtpl_widget.cpp
  --------------------
  UX structure and safety tests for the MTPL dock widget.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "mtpltestfixtures.h"
#include "mtpltestutils.h"

#include "qgsmtplpackagetoolswidget.h"
#include "qgsmtplcredentialstore.h"
#include "qgsmtpldockwidget.h"
#include "qgsmtplpackageoperation.h"
#include "qgsmtplpackageservice.h"
#include "qgstest.h"

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>

namespace
{
  bool isDescendantOf( const QWidget *child, const QWidget *ancestor )
  {
    for ( const QWidget *current = child; current; current = current->parentWidget() )
    {
      if ( current == ancestor )
        return true;
    }
    return false;
  }

  QAction *passwordVisibilityAction( QLineEdit *edit )
  {
    for ( QAction *action : edit->actions() )
    {
      if ( action->isCheckable() )
        return action;
    }
    return nullptr;
  }
}

class TestMtplWidget : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void tabStructureAndOwnership();
    void detailsPageUsesIndependentScrolling();
    void packageToolsAreEmbedded();
    void smartSourcePathFollowing();
    void clearingMainPathClearsSuggestionOnly();
    void credentialLifecycle();
    void operationStateAndSignals();
    void shutdownLeavesOnlyValidTerminalOutput();
    void dockStatePersistsAcrossHide();
    void secretsAreConcealedWhenLeavingTabs();
    void portableProbePopulatesPackageAndDetailsPages();

  private:
    QgsMtplTest::PluginSettingsSnapshot mSettingsSnapshot;
    QgsMtplTest::ArtifactWorkspace mWorkspace;
    QString mPortableVtpPath;
};

void TestMtplWidget::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
  mSettingsSnapshot.capture( {
    QStringLiteral( "mtpl/credentials/remember" ),
    QStringLiteral( "mtpl/credentials/privateKey" ),
    QStringLiteral( "mtpl/credentials/deviceKey" ),
    QStringLiteral( "mtpl/lastPath" )
  } );
  QString error;
  if ( !mWorkspace.initialize( QStringLiteral( "widget" ), error ) )
    QFAIL( qPrintable( error ) );
  mPortableVtpPath = mWorkspace.filePath( QStringLiteral( "portable-widget.vtp" ) );
  if ( !QgsMtplTest::writeTileFixture(
         mPortableVtpPath, QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_PLAIN, {}, error ) )
    QFAIL( qPrintable( error ) );
}

void TestMtplWidget::cleanupTestCase()
{
  QgsMtplPackageOperationTask::cancelAndWaitForAllActiveTasks();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
  mSettingsSnapshot.restore();
  QgsApplication::exitQgis();
}

void TestMtplWidget::tabStructureAndOwnership()
{
  QMainWindow window;
  QgsMtplDockWidget dock( &window );
  window.addDockWidget( Qt::RightDockWidgetArea, &dock );
  window.resize( 1000, 720 );
  window.show();
  QCoreApplication::processEvents();

  QTabWidget *tabs = dock.findChild<QTabWidget *>( QStringLiteral( "mtplMainTabs" ) );
  QVERIFY( tabs );
  QCOMPARE( tabs->count(), 3 );
  QCOMPARE( tabs->tabText( 0 ), QStringLiteral( "数据包" ) );
  QCOMPARE( tabs->tabText( 1 ), QStringLiteral( "数据包详情" ) );
  QCOMPARE( tabs->tabText( 2 ), QStringLiteral( "数据包工具" ) );
  QCOMPARE( tabs->currentIndex(), 0 );

  QWidget *packagesTab = dock.findChild<QWidget *>( QStringLiteral( "mtplPackagesTab" ) );
  QWidget *detailsTab = dock.findChild<QWidget *>( QStringLiteral( "mtplPackageDetailsTab" ) );
  QWidget *toolsTab = dock.findChild<QWidget *>( QStringLiteral( "mtplPackageToolsTab" ) );
  QVERIFY( packagesTab );
  QVERIFY( detailsTab );
  QVERIFY( toolsTab );
  QCOMPARE( tabs->widget( 0 ), packagesTab );
  QCOMPARE( tabs->widget( 1 ), detailsTab );
  QCOMPARE( tabs->widget( 2 ), toolsTab );

  QWidget *keyPanel = dock.findChild<QWidget *>( QStringLiteral( "mtplKeyPanel" ) );
  QVERIFY( keyPanel );
  QVERIFY( isDescendantOf( keyPanel, packagesTab ) );
  QVERIFY( keyPanel->isVisibleTo( packagesTab ) );
  QVERIFY( !dock.findChild<QObject *>( QStringLiteral( "mtplUnlockButton" ) ) );
  QVERIFY( !dock.findChild<QObject *>( QStringLiteral( "mtplDetailsButton" ) ) );
  QVERIFY( !dock.findChild<QObject *>( QStringLiteral( "mtplPackageToolsButton" ) ) );
  QVERIFY( dock.minimumWidth() >= 620 );

  dock.cancelPendingOperations();
}

void TestMtplWidget::detailsPageUsesIndependentScrolling()
{
  QgsMtplDockWidget dock;
  QTabWidget *tabs = dock.findChild<QTabWidget *>( QStringLiteral( "mtplMainTabs" ) );
  QStackedWidget *detailsStack = dock.findChild<QStackedWidget *>( QStringLiteral( "mtplDetailsStack" ) );
  QLabel *emptyLabel = dock.findChild<QLabel *>( QStringLiteral( "mtplDetailsEmptyLabel" ) );
  QScrollArea *scrollArea = dock.findChild<QScrollArea *>( QStringLiteral( "mtplDetailsScrollArea" ) );
  QPushButton *backButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplBackToPackagesButton" ) );
  QWidget *detailsPanel = dock.findChild<QWidget *>( QStringLiteral( "mtplDetailsPanel" ) );
  QTableWidget *metadataTable = dock.findChild<QTableWidget *>( QStringLiteral( "mtplMetadataTable" ) );
  QVERIFY( tabs );
  QVERIFY( detailsStack );
  QVERIFY( emptyLabel );
  QVERIFY( scrollArea );
  QVERIFY( backButton );
  QVERIFY( detailsPanel );
  QVERIFY( metadataTable );

  QCOMPARE( detailsStack->currentWidget(), emptyLabel );
  QCOMPARE( scrollArea->widget(), detailsPanel );
  QVERIFY( scrollArea->widgetResizable() );
  QCOMPARE( scrollArea->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff );
  QCOMPARE( metadataTable->verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff );
  QCOMPARE( metadataTable->sizePolicy().verticalPolicy(), QSizePolicy::Fixed );
  QVERIFY( isDescendantOf( scrollArea, tabs->widget( 1 ) ) );

  tabs->setCurrentIndex( 1 );
  QCoreApplication::processEvents();
  QCOMPARE( tabs->currentWidget()->objectName(), QStringLiteral( "mtplPackageDetailsTab" ) );
  backButton->click();
  QCOMPARE( tabs->currentWidget()->objectName(), QStringLiteral( "mtplPackagesTab" ) );
  dock.cancelPendingOperations();
}

void TestMtplWidget::packageToolsAreEmbedded()
{
  QgsMtplDockWidget dock;
  QTabWidget *tabs = dock.findChild<QTabWidget *>( QStringLiteral( "mtplMainTabs" ) );
  QWidget *toolsWidget = dock.findChild<QWidget *>( QStringLiteral( "mtplPackageToolsWidget" ) );
  QScrollArea *toolsScroll = dock.findChild<QScrollArea *>( QStringLiteral( "mtplPackageToolsScrollArea" ) );
  QComboBox *operation = dock.findChild<QComboBox *>( QStringLiteral( "mtplPackageToolAction" ) );
  QLineEdit *source = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolSource" ) );
  QLineEdit *output = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolOutput" ) );
  QProgressBar *progress = dock.findChild<QProgressBar *>( QStringLiteral( "mtplPackageToolProgress" ) );
  QWidget *result = dock.findChild<QWidget *>( QStringLiteral( "mtplPackageToolResult" ) );
  QVERIFY( tabs );
  QVERIFY( toolsWidget );
  QVERIFY( toolsScroll );
  QVERIFY( operation );
  QVERIFY( source );
  QVERIFY( output );
  QVERIFY( progress );
  QVERIFY( result );
  QVERIFY( isDescendantOf( toolsWidget, tabs->widget( 2 ) ) );
  QVERIFY( toolsScroll->widgetResizable() );
  QVERIFY( dock.findChild<QObject *>( QStringLiteral( "mtplPackageToolRestoreSource" ) ) );
  QVERIFY( dock.findChild<QObject *>( QStringLiteral( "mtplPackageToolRun" ) ) );
  QVERIFY( dock.findChild<QObject *>( QStringLiteral( "mtplPackageToolCancel" ) ) );

  tabs->setCurrentIndex( 2 );
  QCoreApplication::processEvents();
  QCOMPARE( tabs->currentWidget()->objectName(), QStringLiteral( "mtplPackageToolsTab" ) );
  dock.cancelPendingOperations();
}

void TestMtplWidget::smartSourcePathFollowing()
{
  QgsMtplPackageToolsWidget widget;
  widget.show();
  QLineEdit *source = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolSource" ) );
  QPushButton *restore = widget.findChild<QPushButton *>( QStringLiteral( "mtplPackageToolRestoreSource" ) );
  QVERIFY( source );
  QVERIFY( restore );
  QCOMPARE( restore->text(), QStringLiteral( "使用当前数据包路径" ) );

  const QString firstSuggested = QDir::toNativeSeparators( QDir::cleanPath( QStringLiteral( "C:/fixtures/first" ) ) );
  const QString secondSuggested = QDir::toNativeSeparators( QDir::cleanPath( QStringLiteral( "C:/fixtures/second" ) ) );
  const QString customPath = QDir::toNativeSeparators( QDir::cleanPath( QStringLiteral( "C:/fixtures/custom" ) ) );
  widget.setSuggestedSourcePath( firstSuggested );
  QCOMPARE( source->text(), firstSuggested );

  source->setFocus();
  source->selectAll();
  QTest::keyClicks( source, customPath );
  QCOMPARE( source->text(), customPath );
  widget.setSuggestedSourcePath( secondSuggested );
  QCOMPARE( source->text(), customPath );
  QVERIFY( restore->isEnabled() );

  restore->click();
  QCOMPARE( source->text(), secondSuggested );
  widget.setSuggestedSourcePath( firstSuggested );
  QCOMPARE( source->text(), firstSuggested );
  widget.setSuggestedSourcePath( QString() );
  QCOMPARE( source->text(), firstSuggested );
  widget.shutdown();
}

void TestMtplWidget::clearingMainPathClearsSuggestionOnly()
{
  QgsMtplDockWidget dock;
  QLineEdit *mainPath = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPathEdit" ) );
  QLineEdit *source = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolSource" ) );
  QPushButton *restore = dock.findChild<QPushButton *>( QStringLiteral( "mtplPackageToolRestoreSource" ) );
  QVERIFY( mainPath );
  QVERIFY( source );
  QVERIFY( restore );

  const QString suggestedPath = QDir::toNativeSeparators( mWorkspace.filePath( QStringLiteral( "suggested-source.vtp" ) ) );
  const QString customPath = QDir::toNativeSeparators( mWorkspace.filePath( QStringLiteral( "custom-source" ) ) );
  mainPath->setText( suggestedPath );
  QCOMPARE( source->text(), suggestedPath );

  source->setFocus();
  source->selectAll();
  QTest::keyClicks( source, customPath );
  QCOMPARE( source->text(), customPath );
  QVERIFY( !restore->isHidden() );

  mainPath->clear();
  QCOMPARE( source->text(), customPath );
  QVERIFY( restore->isHidden() );
  restore->click();
  QCOMPARE( source->text(), customPath );
  dock.cancelPendingOperations();
}

void TestMtplWidget::credentialLifecycle()
{
  QgsMtplCredentialStore::clear();
  QgsMtplCredentialStore::setRememberEnabled( true );
  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  QVERIFY( keys.isValid() );
  const QString savedPath = QDir::toNativeSeparators( mWorkspace.filePath( QStringLiteral( "remembered-source.vtp" ) ) );
  QString error;
  QVERIFY2( QgsMtplCredentialStore::saveManualKeys( keys, savedPath, error ), qPrintable( error ) );

  {
    QgsMtplPackageToolsWidget restored;
    QLineEdit *privateKey = restored.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolPrivateKey" ) );
    QLineEdit *deviceKey = restored.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolDeviceKey" ) );
    QVERIFY( privateKey );
    QVERIFY( deviceKey );
    QVERIFY( privateKey->text().toLatin1() == keys.privateKey );
    QVERIFY( deviceKey->text().toLatin1() == keys.deviceKey );
    QCOMPARE( privateKey->echoMode(), QLineEdit::Password );
    QCOMPARE( deviceKey->echoMode(), QLineEdit::Password );
    restored.shutdown();
  }
  QCOMPARE( QgsMtplCredentialStore::lastPath(), savedPath );

  QgsMtplCredentialStore::clearRememberedKeys();
  {
    QgsMtplPackageToolsWidget cleared;
    QLineEdit *privateKey = cleared.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolPrivateKey" ) );
    QLineEdit *deviceKey = cleared.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolDeviceKey" ) );
    QVERIFY( privateKey );
    QVERIFY( deviceKey );
    QVERIFY( privateKey->text().isEmpty() );
    QVERIFY( deviceKey->text().isEmpty() );
    cleared.shutdown();
  }
  QCOMPARE( QgsMtplCredentialStore::lastPath(), savedPath );
  QVERIFY( QgsMtplCredentialStore::rememberedKeys().isEmpty() );

  QgsMtplCredentialStore::setRememberEnabled( false );
  QVERIFY( !QgsMtplCredentialStore::rememberEnabled() );
  const QString disabledPath = QDir::toNativeSeparators(
    mWorkspace.filePath( QStringLiteral( "remember-disabled.sfp" ) ) );
  QVERIFY2( QgsMtplCredentialStore::saveManualKeys( keys, disabledPath, error ), qPrintable( error ) );
  QVERIFY( QgsMtplCredentialStore::rememberedKeys().isEmpty() );
  QCOMPARE( QgsMtplCredentialStore::lastPath(), disabledPath );
  QgsMtplCredentialStore::setLastPath( QString() );
  keys.clear();
}

void TestMtplWidget::operationStateAndSignals()
{
  const QString sourceDirectory = mWorkspace.filePath( QStringLiteral( "widget-operation-source" ) );
  QVERIFY( QDir().mkpath( sourceDirectory ) );
  QFile sourceFile( QDir( sourceDirectory ).filePath( QStringLiteral( "payload.txt" ) ) );
  QVERIFY( sourceFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
  QCOMPARE( sourceFile.write( QByteArrayLiteral( "widget operation payload" ) ), 24 );
  sourceFile.close();
  const QString outputPath = mWorkspace.filePath( QStringLiteral( "widget-created.sfp" ) );

  QgsMtplPackageToolsWidget widget;
  widget.show();
  QComboBox *operation = widget.findChild<QComboBox *>( QStringLiteral( "mtplPackageToolAction" ) );
  QLineEdit *source = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolSource" ) );
  QLineEdit *output = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolOutput" ) );
  QPushButton *run = widget.findChild<QPushButton *>( QStringLiteral( "mtplPackageToolRun" ) );
  QPushButton *cancel = widget.findChild<QPushButton *>( QStringLiteral( "mtplPackageToolCancel" ) );
  QPlainTextEdit *resultDetails = widget.findChild<QPlainTextEdit *>( QStringLiteral( "mtplPackageToolResult" ) );
  QVERIFY( operation );
  QVERIFY( source );
  QVERIFY( output );
  QVERIFY( run );
  QVERIFY( cancel );
  QVERIFY( resultDetails );
  const int createSfpIndex = operation->findText( QStringLiteral( "创建 SFP 数据包" ) );
  QVERIFY( createSfpIndex >= 0 );
  operation->setCurrentIndex( createSfpIndex );
  source->setText( sourceDirectory );
  output->setText( outputPath );
  QSignalSpy messageSpy( &widget, &QgsMtplPackageToolsWidget::messageRequested );
  QSignalSpy outputsSpy( &widget, &QgsMtplPackageToolsWidget::outputsCommitted );

  run->click();
  QVERIFY( widget.isOperationRunning() );
  QCOMPARE( widget.property( "mtplOperationState" ).toString(), QStringLiteral( "running" ) );
  QVERIFY( !source->isEnabled() );
  QVERIFY( cancel->isEnabled() );
  QTRY_VERIFY_WITH_TIMEOUT( !widget.isOperationRunning(), 30000 );
  QCOMPARE( widget.property( "mtplOperationState" ).toString(), QStringLiteral( "succeeded" ) );
  QCOMPARE( messageSpy.count(), 1 );
  QCOMPARE( outputsSpy.count(), 1 );
  QVERIFY( QFileInfo( outputPath ).isFile() );
  QVERIFY( resultDetails->isVisible() );
  QVERIFY( !resultDetails->toPlainText().isEmpty() );
  QVERIFY( source->isEnabled() );
  QVERIFY( !cancel->isEnabled() );
  widget.shutdown();
  QgsMtplPackageOperationTask::cancelAndWaitForAllActiveTasks();
}

void TestMtplWidget::shutdownLeavesOnlyValidTerminalOutput()
{
  const QString sourceDirectory = mWorkspace.filePath( QStringLiteral( "widget-cancel-source" ) );
  QVERIFY( QDir().mkpath( sourceDirectory ) );
  QFile sourceFile( QDir( sourceDirectory ).filePath( QStringLiteral( "payload.bin" ) ) );
  QVERIFY( sourceFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
  const QByteArray block( 1024 * 1024, 'x' );
  QCOMPARE( sourceFile.write( block ), static_cast<qint64>( block.size() ) );
  sourceFile.close();
  const QString outputPath = mWorkspace.filePath( QStringLiteral( "widget-canceled.sfp" ) );

  QgsMtplPackageToolsWidget widget;
  QComboBox *operation = widget.findChild<QComboBox *>( QStringLiteral( "mtplPackageToolAction" ) );
  QLineEdit *source = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolSource" ) );
  QLineEdit *output = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolOutput" ) );
  QPushButton *run = widget.findChild<QPushButton *>( QStringLiteral( "mtplPackageToolRun" ) );
  QVERIFY( operation );
  QVERIFY( source );
  QVERIFY( output );
  QVERIFY( run );
  operation->setCurrentIndex( operation->findText( QStringLiteral( "创建 SFP 数据包" ) ) );
  source->setText( sourceDirectory );
  output->setText( outputPath );
  run->click();
  QVERIFY( widget.isOperationRunning() );
  widget.shutdown();
  QVERIFY( !widget.isOperationRunning() );
  QgsMtplPackageOperationTask::cancelAndWaitForAllActiveTasks();
  const QStringList stagingFiles = QDir( mWorkspace.path() ).entryList(
    QStringList() << QStringLiteral( ".widget-canceled.sfp.mtpl-staging-*" ), QDir::Files | QDir::Hidden );
  QVERIFY( stagingFiles.isEmpty() );
  if ( QFileInfo::exists( outputPath ) )
  {
    const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
      outputPath, {}, true, QgsMtpl::CredentialSource::None );
    QVERIFY2( probe.ok, qPrintable( probe.error ) );
    QCOMPARE( probe.packages.size(), 1 );
    QVERIFY( probe.packages.constFirst().isReady() );
  }
}

void TestMtplWidget::dockStatePersistsAcrossHide()
{
  QMainWindow window;
  QgsMtplDockWidget dock( &window );
  window.addDockWidget( Qt::RightDockWidgetArea, &dock );
  window.show();
  dock.setSelectedPath( mPortableVtpPath );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok, 30000 );
  QTabWidget *tabs = dock.findChild<QTabWidget *>( QStringLiteral( "mtplMainTabs" ) );
  QTreeWidget *tree = dock.findChild<QTreeWidget *>( QStringLiteral( "mtplPackageTree" ) );
  QLineEdit *source = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolSource" ) );
  QVERIFY( tabs );
  QVERIFY( tree );
  QVERIFY( tree->currentItem() );
  QVERIFY( source );
  const QString selectedName = tree->currentItem()->text( 0 );
  const QString customSource = QDir::toNativeSeparators( mWorkspace.filePath( QStringLiteral( "persistent-custom-source" ) ) );
  tabs->setCurrentIndex( 2 );
  source->setFocus();
  source->selectAll();
  QTest::keyClicks( source, customSource );
  dock.hide();
  QCoreApplication::processEvents();
  dock.show();
  QCoreApplication::processEvents();
  QCOMPARE( tabs->currentIndex(), 2 );
  QCOMPARE( source->text(), customSource );
  QVERIFY( tree->currentItem() );
  QCOMPARE( tree->currentItem()->text( 0 ), selectedName );
  dock.cancelPendingOperations();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
}

void TestMtplWidget::secretsAreConcealedWhenLeavingTabs()
{
  QgsMtplDockWidget dock;
  QTabWidget *tabs = dock.findChild<QTabWidget *>( QStringLiteral( "mtplMainTabs" ) );
  QLineEdit *packagePrivateKey = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPrivateKeyEdit" ) );
  QLineEdit *packageDeviceKey = dock.findChild<QLineEdit *>( QStringLiteral( "mtplDeviceKeyEdit" ) );
  QLineEdit *toolPrivateKey = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolPrivateKey" ) );
  QLineEdit *toolDeviceKey = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolDeviceKey" ) );
  QVERIFY( tabs );
  QVERIFY( packagePrivateKey );
  QVERIFY( packageDeviceKey );
  QVERIFY( toolPrivateKey );
  QVERIFY( toolDeviceKey );
  QCOMPARE( packagePrivateKey->echoMode(), QLineEdit::Password );
  QCOMPARE( packageDeviceKey->echoMode(), QLineEdit::Password );
  QCOMPARE( toolPrivateKey->echoMode(), QLineEdit::Password );
  QCOMPARE( toolDeviceKey->echoMode(), QLineEdit::Password );

  packagePrivateKey->setText( QStringLiteral( "synthetic-private-value" ) );
  QAction *packageVisibility = passwordVisibilityAction( packagePrivateKey );
  QVERIFY( packageVisibility );
  packageVisibility->trigger();
  QVERIFY( packageVisibility->isChecked() );
  tabs->setCurrentIndex( 1 );
  QCoreApplication::processEvents();
  QCOMPARE( packagePrivateKey->echoMode(), QLineEdit::Password );
  QVERIFY( !packageVisibility->isChecked() );

  tabs->setCurrentIndex( 2 );
  toolPrivateKey->setText( QStringLiteral( "synthetic-tool-private-value" ) );
  QAction *toolVisibility = passwordVisibilityAction( toolPrivateKey );
  QVERIFY( toolVisibility );
  toolVisibility->trigger();
  QVERIFY( toolVisibility->isChecked() );
  tabs->setCurrentIndex( 0 );
  QCoreApplication::processEvents();
  QCOMPARE( toolPrivateKey->echoMode(), QLineEdit::Password );
  QVERIFY( !toolVisibility->isChecked() );

  packagePrivateKey->clear();
  packageDeviceKey->clear();
  toolPrivateKey->clear();
  toolDeviceKey->clear();
  dock.cancelPendingOperations();
}

void TestMtplWidget::portableProbePopulatesPackageAndDetailsPages()
{
  QMainWindow window;
  QgsMtplDockWidget dock( &window );
  window.addDockWidget( Qt::RightDockWidgetArea, &dock );
  window.resize( 1000, 720 );
  window.show();
  dock.setSelectedPath( mPortableVtpPath );

  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok, 30000 );
  QCOMPARE( dock.selectedPath(), QFileInfo( mPortableVtpPath ).absoluteFilePath() );
  QCOMPARE( dock.currentProbe().packages.size(), 1 );
  QCOMPARE( dock.currentProbe().packages.constFirst().format, QgsMtpl::PackageFormat::Vtp );
  QCOMPARE( dock.currentProbe().packages.constFirst().readiness, QgsMtpl::ReadinessState::PlainReady );

  QTreeWidget *packageTree = dock.findChild<QTreeWidget *>( QStringLiteral( "mtplPackageTree" ) );
  QTabWidget *tabs = dock.findChild<QTabWidget *>( QStringLiteral( "mtplMainTabs" ) );
  QStackedWidget *detailsStack = dock.findChild<QStackedWidget *>( QStringLiteral( "mtplDetailsStack" ) );
  QScrollArea *detailsScroll = dock.findChild<QScrollArea *>( QStringLiteral( "mtplDetailsScrollArea" ) );
  QPushButton *backButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplBackToPackagesButton" ) );
  QVERIFY( packageTree );
  QVERIFY( tabs );
  QVERIFY( detailsStack );
  QVERIFY( detailsScroll );
  QVERIFY( backButton );
  QCOMPARE( packageTree->topLevelItemCount(), 1 );
  QVERIFY( packageTree->currentItem() );

  tabs->setCurrentIndex( 1 );
  QCoreApplication::processEvents();
  QCOMPARE( detailsStack->currentWidget(), detailsScroll );
  backButton->click();
  QCOMPARE( tabs->currentIndex(), 0 );
  dock.cancelPendingOperations();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
}

QGSTEST_MAIN( TestMtplWidget )
#include "test_mtpl_widget.moc"
