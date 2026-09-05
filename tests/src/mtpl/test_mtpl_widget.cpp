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
#include "qgsmtplkeysidecar.h"
#include "qgsmtplpackageoperation.h"
#include "qgsmtplpackageservice.h"
#include "qgsmtplpartitionrulewidget.h"
#include "qgsmtplpluginlayer.h"
#include "qgspathresolver.h"
#include "qgsproject.h"
#include "qgsreadwritecontext.h"
#include "qgscollapsiblegroupbox.h"
#include "qgstest.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <algorithm>

namespace
{
  class MemoryCredentialBackend final : public QgsMtplCredentialBackend
  {
    public:
      bool contains( const QString &credentialId ) const override
      {
        return mCredentials.contains( credentialId );
      }

      LoadResult load( const QString &credentialId, bool allowUnlock ) override
      {
        Q_UNUSED( allowUnlock )
        ++loadCount;
        LoadResult result;
        if ( !mCredentials.contains( credentialId ) )
          return result;
        result.status = LoadStatus::Success;
        result.keys = mCredentials.value( credentialId );
        return result;
      }

      bool store( const QgsMtpl::CryptoKeys &keys, QString &credentialId, QString &error ) override
      {
        credentialId = QStringLiteral( "fake%1" ).arg( ++mNextId, 3, 10, QLatin1Char( '0' ) );
        mCredentials.insert( credentialId, keys );
        error.clear();
        return true;
      }

      bool remove( const QString &credentialId, QString &error ) override
      {
        mCredentials.remove( credentialId );
        error.clear();
        return true;
      }

      void reset()
      {
        mCredentials.clear();
        mNextId = 0;
        loadCount = 0;
      }

      int loadCount = 0;

    private:
      QHash<QString, QgsMtpl::CryptoKeys> mCredentials;
      int mNextId = 0;
  };

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

  QgsMtpl::PartitionRule editablePartitionRule()
  {
    QgsMtpl::PartitionRule rule;
    rule.id = QStringLiteral( "test.partition.rule" );
    rule.name = QStringLiteral( "测试规则" );
    rule.bands = { { 0, 3, 0 }, { 4, 7, 2 }, { 8, 11, 4 } };
    return rule;
  }

  QSpinBox *bandSpinBox( QTableWidget *table, int row, int column )
  {
    return qobject_cast<QSpinBox *>( table->cellWidget( row, column ) );
  }
} // namespace

class TestMtplWidget : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void tabStructureAndOwnership();
    void packagesTabRemainsReachableAtLowHeight();
    void detailsPageUsesIndependentScrolling();
    void packageToolsAreEmbedded();
    void partitionRuleEditorStructureAndBuiltIns();
    void partitionRuleEditorCopiesAreEditable();
    void partitionRuleEditorBandOperations();
    void partitionRuleEditorValidation();
    void partitionRuleEditorSaveAndDeleteSignals();
    void partitionRuleEditorPreservesOtherDrafts();
    void smartSourcePathFollowing();
    void clearingMainPathClearsSuggestionOnly();
    void credentialLifecycle();
    void dockCredentialSaveRestoreAndClear();
    void operationStateAndSignals();
    void ptpCreateInputValidationAndRecovery();
    void shutdownLeavesOnlyValidTerminalOutput();
    void dockStatePersistsAcrossHide();
    void secretsAreConcealedWhenLeavingTabs();
    void dockDatasetSummaryAndLoadControls();
    void dockCompletedPtpPathLoadsOnFirstClick_data();
    void dockCompletedPtpPathLoadsOnFirstClick();
    void dockDirectoryCreatesSingleDatasetLayer();
    void dockMixedPtpCredentialsRecoverAfterReselection();
    void dockPtpSelectionPrefersSidecarOverSessionKeys();
    void dockRejectsPtpReplacementBetweenProbeAndLoad();
    void dockRejectsInvalidPtpDatasetAtomically();
    void dockDisablesLoadForUnsavedRuleDraft();
    void portableProbePopulatesPackageAndDetailsPages();

  private:
    QgsMtplTest::PluginSettingsSnapshot mSettingsSnapshot;
    QgsMtplTest::ArtifactWorkspace mWorkspace;
    MemoryCredentialBackend mCredentialBackend;
    QString mPortableVtpPath;
    QString mPortablePtpPath;
};

void TestMtplWidget::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
  QgsMtplCredentialStore::setBackendForTesting( &mCredentialBackend );
  mSettingsSnapshot.capture( {
    QStringLiteral( "mtpl/credentials/authcfg" ),
    QStringLiteral( "mtpl/credentials/remember" ),
    QStringLiteral( "mtpl/credentials/privateKey" ),
    QStringLiteral( "mtpl/credentials/deviceKey" ),
    QStringLiteral( "mtpl/lastPath" ),
    QStringLiteral( "mtpl/partitionRules/v1" ),
    QStringLiteral( "mtpl/partitionRules/lastSelectedId" )
  } );
  QgsMtplCredentialStore::clear();
  QgsSettings settings;
  settings.remove( QStringLiteral( "mtpl/partitionRules/v1" ), QgsSettings::Section::Plugins );
  settings.remove( QStringLiteral( "mtpl/partitionRules/lastSelectedId" ), QgsSettings::Section::Plugins );
  settings.sync();
  QString error;
  if ( !mWorkspace.initialize( QStringLiteral( "widget" ), error ) )
    QFAIL( qPrintable( error ) );
  mPortableVtpPath = mWorkspace.filePath( QStringLiteral( "portable-widget.vtp" ) );
  if ( !QgsMtplTest::writeTileFixture(
         mPortableVtpPath, QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_PLAIN, {}, error ) )
    QFAIL( qPrintable( error ) );
  mPortablePtpPath = mWorkspace.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) );
  if ( !QgsMtplTest::writeTileFixture(
         mPortablePtpPath, QgsMtpl::PackageFormat::Ptp, MTPL_STORAGE_PLAIN, {}, error ) )
    QFAIL( qPrintable( error ) );
}

void TestMtplWidget::cleanupTestCase()
{
  QgsMtplPackageOperationTask::cancelAndWaitForAllActiveTasks();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
  QgsMtplCredentialStore::clear();
  mSettingsSnapshot.restore();
  QgsMtplCredentialStore::setBackendForTesting( nullptr );
  QgsApplication::exitQgis();
}

void TestMtplWidget::cleanup()
{
  QgsMtplPackageOperationTask::cancelAndWaitForAllActiveTasks();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
  QgsProject::instance()->removeAllMapLayers();
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

void TestMtplWidget::packagesTabRemainsReachableAtLowHeight()
{
  QMainWindow window;
  QgsMtplDockWidget dock( &window );
  window.addDockWidget( Qt::RightDockWidgetArea, &dock );
  window.resize( 900, 480 );
  window.show();
  QCoreApplication::processEvents();

  QScrollArea *scrollArea = dock.findChild<QScrollArea *>( QStringLiteral( "mtplPackagesScrollArea" ) );
  QWidget *packagesContent = dock.findChild<QWidget *>( QStringLiteral( "mtplPackagesContent" ) );
  QWidget *keyPanel = dock.findChild<QWidget *>( QStringLiteral( "mtplKeyPanel" ) );
  QPushButton *loadButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplLoadButton" ) );
  QVERIFY( scrollArea );
  QVERIFY( packagesContent );
  QVERIFY( keyPanel );
  QVERIFY( loadButton );
  QCOMPARE( scrollArea->widget(), packagesContent );
  QgsCollapsibleGroupBoxBasic *ruleGroup = dock.findChild<QgsCollapsibleGroupBoxBasic *>( QStringLiteral( "mtplPartitionRuleGroup" ) );
  QVERIFY( ruleGroup );
  QVERIFY( ruleGroup->isCollapsed() );
  QVERIFY( scrollArea->widgetResizable() );
  QVERIFY( keyPanel->isVisibleTo( packagesContent ) );
  QTRY_VERIFY( scrollArea->verticalScrollBar()->maximum() > 0 );

  const auto intersectsViewport = [scrollArea]( const QWidget *widget )
  {
    const QRect widgetRect( widget->mapTo( scrollArea->viewport(), QPoint( 0, 0 ) ), widget->size() );
    return scrollArea->viewport()->rect().intersects( widgetRect );
  };
  scrollArea->ensureWidgetVisible( keyPanel );
  QCoreApplication::processEvents();
  QVERIFY( intersectsViewport( keyPanel ) );
  scrollArea->ensureWidgetVisible( loadButton );
  QCoreApplication::processEvents();
  QVERIFY( intersectsViewport( loadButton ) );

  ruleGroup->setCollapsed( false );
  QCoreApplication::processEvents();
  scrollArea->ensureWidgetVisible( ruleGroup );
  QVERIFY( intersectsViewport( ruleGroup ) );
  scrollArea->ensureWidgetVisible( loadButton );
  QCoreApplication::processEvents();
  QVERIFY( intersectsViewport( loadButton ) );

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

void TestMtplWidget::partitionRuleEditorStructureAndBuiltIns()
{
  QgsMtplPartitionRuleWidget widget;
  widget.setRules( QgsMtpl::PartitionRuleStore::builtInRules() );
  widget.resize( 760, 480 );
  widget.show();
  QCoreApplication::processEvents();

  QSplitter *splitter = widget.findChild<QSplitter *>( QStringLiteral( "mtplPartitionRuleSplitter" ) );
  QListWidget *ruleList = widget.findChild<QListWidget *>( QStringLiteral( "mtplPartitionRuleList" ) );
  QLineEdit *nameEdit = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPartitionRuleNameEdit" ) );
  QWidget *summaryBar = widget.findChild<QWidget *>( QStringLiteral( "mtplPartitionRuleSummaryBar" ) );
  QLabel *summaryLabel = widget.findChild<QLabel *>( QStringLiteral( "mtplPartitionRuleSummaryLabel" ) );
  QTableWidget *bandTable = widget.findChild<QTableWidget *>( QStringLiteral( "mtplPartitionRuleBandTable" ) );
  QLabel *validationLabel = widget.findChild<QLabel *>( QStringLiteral( "mtplPartitionRuleValidationLabel" ) );
  QPushButton *addRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleAddButton" ) );
  QPushButton *copyRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleCopyButton" ) );
  QPushButton *deleteRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleDeleteButton" ) );
  QPushButton *saveRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleSaveButton" ) );
  QPushButton *saveAsRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleSaveAsButton" ) );
  QVERIFY( splitter );
  QVERIFY( ruleList );
  QVERIFY( nameEdit );
  QVERIFY( summaryBar );
  QVERIFY( summaryLabel );
  QVERIFY( bandTable );
  QVERIFY( validationLabel );
  QVERIFY( addRule );
  QVERIFY( copyRule );
  QVERIFY( deleteRule );
  QVERIFY( saveRule );
  QVERIFY( saveAsRule );
  QVERIFY( widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleBandAddButton" ) ) );
  QVERIFY( widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleBandDeleteButton" ) ) );
  QVERIFY( widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleBandUpButton" ) ) );
  QVERIFY( widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleBandDownButton" ) ) );

  QCOMPARE( splitter->count(), 2 );
  QCOMPARE( ruleList->count(), 2 );
  QCOMPARE( widget.currentRuleId(), QgsMtpl::PartitionRuleStore::ruleOneId() );
  QVERIFY( widget.currentRule().builtIn );
  QCOMPARE( bandTable->rowCount(), 4 );
  QVERIFY( nameEdit->isReadOnly() );
  QVERIFY( !bandTable->isEnabled() );
  QVERIFY( addRule->isEnabled() );
  QVERIFY( copyRule->isEnabled() );
  QVERIFY( !deleteRule->isEnabled() );
  QVERIFY( !saveRule->isEnabled() );
  QVERIFY( saveAsRule->isEnabled() );
  QVERIFY( summaryLabel->text().contains( QStringLiteral( "z0–7" ) ) );
  QVERIFY( validationLabel->property( "valid" ).toBool() );

  ruleList->setCurrentRow( 1 );
  QCOMPARE( widget.currentRuleId(), QgsMtpl::PartitionRuleStore::ruleTwoId() );
  QVERIFY( widget.currentRule().builtIn );
  QCOMPARE( bandTable->rowCount(), 3 );
  QVERIFY( nameEdit->isReadOnly() );
  QVERIFY( !bandTable->isEnabled() );
  QVERIFY( copyRule->isEnabled() );
  QVERIFY( !deleteRule->isEnabled() );
  QVERIFY( !saveRule->isEnabled() );
  QVERIFY( saveAsRule->isEnabled() );
}

void TestMtplWidget::partitionRuleEditorCopiesAreEditable()
{
  QgsMtplPartitionRuleWidget widget;
  widget.setRules( QgsMtpl::PartitionRuleStore::builtInRules() );
  QListWidget *ruleList = widget.findChild<QListWidget *>( QStringLiteral( "mtplPartitionRuleList" ) );
  QLineEdit *nameEdit = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPartitionRuleNameEdit" ) );
  QTableWidget *bandTable = widget.findChild<QTableWidget *>( QStringLiteral( "mtplPartitionRuleBandTable" ) );
  QPushButton *copyRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleCopyButton" ) );
  QPushButton *deleteRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleDeleteButton" ) );
  QPushButton *saveRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleSaveButton" ) );
  QPushButton *saveAsRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleSaveAsButton" ) );
  QVERIFY( ruleList );
  QVERIFY( nameEdit );
  QVERIFY( bandTable );
  QVERIFY( copyRule );
  QVERIFY( deleteRule );
  QVERIFY( saveRule );
  QVERIFY( saveAsRule );
  QSignalSpy saveSpy( &widget, &QgsMtplPartitionRuleWidget::ruleSaveRequested );

  copyRule->click();
  QCOMPARE( ruleList->count(), 3 );
  QVERIFY( !widget.currentRule().builtIn );
  QVERIFY( widget.currentRule().id.startsWith( QStringLiteral( "editor-" ) ) );
  QVERIFY( nameEdit->text().contains( QStringLiteral( "副本" ) ) );
  QVERIFY( !nameEdit->isReadOnly() );
  QVERIFY( bandTable->isEnabled() );
  QVERIFY( deleteRule->isEnabled() );
  QVERIFY( saveRule->isEnabled() );
  QCOMPARE( saveSpy.count(), 0 );

  widget.setCurrentRuleId( QgsMtpl::PartitionRuleStore::ruleTwoId() );
  saveAsRule->click();
  QCOMPARE( ruleList->count(), 4 );
  QVERIFY( !widget.currentRule().builtIn );
  QVERIFY( widget.currentRule().name.contains( QStringLiteral( "副本" ) ) );
  QVERIFY( !nameEdit->isReadOnly() );
  QVERIFY( bandTable->isEnabled() );
  QVERIFY( !saveRule->isEnabled() );
  QVERIFY( !widget.currentRuleIsDirty() );
  QCOMPARE( saveSpy.count(), 1 );
  const QgsMtpl::PartitionRule savedCopy = qvariant_cast<QgsMtpl::PartitionRule>( saveSpy.constFirst().constFirst() );
  QCOMPARE( savedCopy.id, widget.currentRuleId() );
  QCOMPARE( savedCopy.name, widget.currentRule().name );

  widget.ruleSaveFailed( savedCopy.id );
  QVERIFY( widget.currentRuleIsDirty() );
  QVERIFY( saveRule->isEnabled() );
}

void TestMtplWidget::partitionRuleEditorBandOperations()
{
  QgsMtplPartitionRuleWidget widget;
  widget.setRules( { editablePartitionRule() } );
  QTableWidget *bandTable = widget.findChild<QTableWidget *>( QStringLiteral( "mtplPartitionRuleBandTable" ) );
  QPushButton *addBand = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleBandAddButton" ) );
  QPushButton *deleteBand = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleBandDeleteButton" ) );
  QPushButton *moveUp = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleBandUpButton" ) );
  QPushButton *moveDown = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleBandDownButton" ) );
  QLabel *validationLabel = widget.findChild<QLabel *>( QStringLiteral( "mtplPartitionRuleValidationLabel" ) );
  QVERIFY( bandTable );
  QVERIFY( addBand );
  QVERIFY( deleteBand );
  QVERIFY( moveUp );
  QVERIFY( moveDown );
  QVERIFY( validationLabel );
  QCOMPARE( bandTable->rowCount(), 3 );

  addBand->click();
  QCOMPARE( bandTable->rowCount(), 4 );
  QCOMPARE( widget.currentRule().bands.size(), 4 );
  QCOMPARE( widget.currentRule().bands.constLast().minZoom, 12 );
  QCOMPARE( widget.currentRule().bands.constLast().maxZoom, 12 );
  QCOMPARE( widget.currentRule().bands.constLast().baseZoom, 12 );
  QVERIFY( validationLabel->property( "valid" ).toBool() );

  deleteBand->click();
  QCOMPARE( bandTable->rowCount(), 3 );
  QCOMPARE( widget.currentRule().bands.size(), 3 );

  bandTable->setCurrentCell( 1, 0 );
  moveUp->click();
  QCOMPARE( widget.currentRule().bands.at( 0 ).minZoom, 4 );
  QCOMPARE( bandTable->currentRow(), 0 );
  QVERIFY( !validationLabel->property( "valid" ).toBool() );

  moveDown->click();
  QCOMPARE( widget.currentRule().bands.at( 0 ).minZoom, 0 );
  QCOMPARE( widget.currentRule().bands.at( 1 ).minZoom, 4 );
  QCOMPARE( bandTable->currentRow(), 1 );
  QVERIFY( validationLabel->property( "valid" ).toBool() );
}

void TestMtplWidget::partitionRuleEditorValidation()
{
  QgsMtplPartitionRuleWidget widget;
  widget.setRules( { editablePartitionRule() } );
  QTableWidget *bandTable = widget.findChild<QTableWidget *>( QStringLiteral( "mtplPartitionRuleBandTable" ) );
  QLabel *validationLabel = widget.findChild<QLabel *>( QStringLiteral( "mtplPartitionRuleValidationLabel" ) );
  QPushButton *saveRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleSaveButton" ) );
  QVERIFY( bandTable );
  QVERIFY( validationLabel );
  QVERIFY( saveRule );

  QSpinBox *secondMinimum = bandSpinBox( bandTable, 1, 0 );
  QSpinBox *firstMinimum = bandSpinBox( bandTable, 0, 0 );
  QSpinBox *firstBase = bandSpinBox( bandTable, 0, 2 );
  QVERIFY( secondMinimum );
  QVERIFY( firstMinimum );
  QVERIFY( firstBase );

  secondMinimum->setValue( 5 );
  QVERIFY( !validationLabel->property( "valid" ).toBool() );
  QVERIFY( validationLabel->text().contains( QStringLiteral( "连续" ) ) );
  QVERIFY( !saveRule->isEnabled() );

  secondMinimum->setValue( 4 );
  QVERIFY( validationLabel->property( "valid" ).toBool() );
  QVERIFY( saveRule->isEnabled() );

  firstBase->setValue( 1 );
  QVERIFY( !validationLabel->property( "valid" ).toBool() );
  QVERIFY( validationLabel->text().contains( QStringLiteral( "基础缩放级别" ) ) );
  QVERIFY( !saveRule->isEnabled() );
  firstBase->setValue( 0 );

  firstMinimum->setValue( 1 );
  QVERIFY( validationLabel->property( "valid" ).toBool() );
  QVERIFY( saveRule->isEnabled() );
  firstMinimum->setValue( 0 );
  QVERIFY( validationLabel->property( "valid" ).toBool() );
}

void TestMtplWidget::partitionRuleEditorSaveAndDeleteSignals()
{
  QgsMtplPartitionRuleWidget widget;
  widget.setRules( { editablePartitionRule() } );
  QLineEdit *nameEdit = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPartitionRuleNameEdit" ) );
  QPushButton *saveRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleSaveButton" ) );
  QPushButton *deleteRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleDeleteButton" ) );
  QVERIFY( nameEdit );
  QVERIFY( saveRule );
  QVERIFY( deleteRule );
  QSignalSpy saveSpy( &widget, &QgsMtplPartitionRuleWidget::ruleSaveRequested );
  QSignalSpy deleteSpy( &widget, &QgsMtplPartitionRuleWidget::ruleDeleteRequested );

  nameEdit->setText( QStringLiteral( "已更新的测试规则" ) );
  QVERIFY( saveRule->isEnabled() );
  saveRule->click();
  QCOMPARE( saveSpy.count(), 1 );
  const QgsMtpl::PartitionRule savedRule = qvariant_cast<QgsMtpl::PartitionRule>( saveSpy.constFirst().constFirst() );
  QCOMPARE( savedRule.id, QStringLiteral( "test.partition.rule" ) );
  QCOMPARE( savedRule.name, QStringLiteral( "已更新的测试规则" ) );
  QCOMPARE( savedRule.bands.size(), 3 );
  QVERIFY( !saveRule->isEnabled() );

  widget.ruleSaveFailed( savedRule.id );
  QVERIFY( saveRule->isEnabled() );
  saveRule->click();
  QCOMPARE( saveSpy.count(), 2 );
  QVERIFY( !saveRule->isEnabled() );

  deleteRule->click();
  QCOMPARE( deleteSpy.count(), 1 );
  QCOMPARE( deleteSpy.constFirst().constFirst().toString(), QStringLiteral( "test.partition.rule" ) );
  QVERIFY( widget.rules().isEmpty() );

  widget.ruleDeleteFailed( QStringLiteral( "test.partition.rule" ) );
  QCOMPARE( widget.rules().size(), 1 );
  QCOMPARE( widget.currentRuleId(), QStringLiteral( "test.partition.rule" ) );
  deleteRule->click();
  QCOMPARE( deleteSpy.count(), 2 );
  widget.ruleDeleteSucceeded( QStringLiteral( "test.partition.rule" ) );
  QVERIFY( widget.rules().isEmpty() );
}

void TestMtplWidget::partitionRuleEditorPreservesOtherDrafts()
{
  QgsMtpl::PartitionRule first = editablePartitionRule();
  first.id = QStringLiteral( "test.partition.first" );
  first.name = QStringLiteral( "第一条" );
  QgsMtpl::PartitionRule second = first;
  second.id = QStringLiteral( "test.partition.second" );
  second.name = QStringLiteral( "第二条" );
  QgsMtpl::PartitionRule third = first;
  third.id = QStringLiteral( "test.partition.third" );
  third.name = QStringLiteral( "第三条" );

  QgsMtplPartitionRuleWidget widget;
  widget.setRules( { first, second, third }, first.id );
  QLineEdit *nameEdit = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPartitionRuleNameEdit" ) );
  QListWidget *ruleList = widget.findChild<QListWidget *>( QStringLiteral( "mtplPartitionRuleList" ) );
  QPushButton *saveRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleSaveButton" ) );
  QPushButton *deleteRule = widget.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleDeleteButton" ) );
  QVERIFY( nameEdit );
  QVERIFY( ruleList );
  QVERIFY( saveRule );
  QVERIFY( deleteRule );

  nameEdit->setText( QStringLiteral( "第一条草稿" ) );
  widget.setCurrentRuleId( second.id );
  nameEdit->setText( QStringLiteral( "第二条草稿" ) );
  saveRule->click();
  widget.ruleSaveFailed( second.id );

  QCOMPARE( widget.rules().size(), 3 );
  QCOMPARE( widget.rules().at( 0 ).name, QStringLiteral( "第一条草稿" ) );
  QCOMPARE( widget.rules().at( 1 ).name, QStringLiteral( "第二条草稿" ) );
  QVERIFY( ruleList->item( 0 )->text().endsWith( QLatin1String( " *" ) ) );
  QVERIFY( ruleList->item( 1 )->text().endsWith( QLatin1String( " *" ) ) );

  deleteRule->click();
  QCOMPARE( widget.rules().size(), 2 );
  widget.ruleDeleteFailed( second.id );
  QCOMPARE( widget.rules().size(), 3 );
  QCOMPARE( widget.rules().at( 0 ).id, first.id );
  QCOMPARE( widget.rules().at( 1 ).id, second.id );
  QCOMPARE( widget.rules().at( 2 ).id, third.id );
  QCOMPARE( widget.rules().at( 0 ).name, QStringLiteral( "第一条草稿" ) );
  QCOMPARE( widget.rules().at( 1 ).name, QStringLiteral( "第二条草稿" ) );
  QVERIFY( ruleList->item( 0 )->text().endsWith( QLatin1String( " *" ) ) );
  QVERIFY( ruleList->item( 1 )->text().endsWith( QLatin1String( " *" ) ) );
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
  mCredentialBackend.reset();
  QgsMtplCredentialStore::clear();
  QVERIFY( !QgsMtplCredentialStore::rememberEnabled() );
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
    QVERIFY( privateKey->text().isEmpty() );
    QVERIFY( deviceKey->text().isEmpty() );
    QCOMPARE( privateKey->echoMode(), QLineEdit::Password );
    QCOMPARE( deviceKey->echoMode(), QLineEdit::Password );
    restored.shutdown();
  }
  QCOMPARE( QgsMtplCredentialStore::lastPath(), savedPath );

  QVERIFY( QgsMtplCredentialStore::clearRememberedKeys( &error ) );
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
  QVERIFY( QgsMtplCredentialStore::rememberEnabled() );
  QgsMtpl::CryptoKeys explicitlyStored = QgsMtplCredentialStore::rememberedKeys( &error );
  QVERIFY2( explicitlyStored.isValid(), qPrintable( error ) );
  explicitlyStored.clear();
  QCOMPARE( QgsMtplCredentialStore::lastPath(), disabledPath );
  QgsMtplCredentialStore::setRememberEnabled( false );
  QgsMtplCredentialStore::setLastPath( QString() );
  keys.clear();
}

void TestMtplWidget::dockCredentialSaveRestoreAndClear()
{
  mCredentialBackend.reset();
  QgsMtplCredentialStore::clear();
  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  QVERIFY( keys.isValid() );

  const QString encryptedPath = mWorkspace.filePath( QStringLiteral( "dock-credential.vtp" ) );
  QString error;
  QVERIFY2( QgsMtplTest::writeTileFixture(
              encryptedPath, QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_ENCRYPTED, keys, error ),
            qPrintable( error ) );

  {
    QgsMtplDockWidget dock;
    QLineEdit *privateKey = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPrivateKeyEdit" ) );
    QLineEdit *deviceKey = dock.findChild<QLineEdit *>( QStringLiteral( "mtplDeviceKeyEdit" ) );
    QLabel *keyStatus = dock.findChild<QLabel *>( QStringLiteral( "mtplKeyStatusLabel" ) );
    QCheckBox *rememberKeys = dock.findChild<QCheckBox *>( QStringLiteral( "mtplRememberKeysCheckBox" ) );
    QPushButton *applyButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplApplyRememberedKeyButton" ) );
    QVERIFY( privateKey );
    QVERIFY( deviceKey );
    QVERIFY( keyStatus );
    QVERIFY( rememberKeys );
    QVERIFY( applyButton );
    QVERIFY( !rememberKeys->isChecked() );

    dock.setSelectedPath( encryptedPath );
    QTRY_VERIFY_WITH_TIMEOUT(
      dock.currentProbe().packages.size() == 1 &&
        dock.currentProbe().packages.constFirst().readiness == QgsMtpl::ReadinessState::KeyRequired,
      30000 );
    privateKey->setText( QString::fromLatin1( keys.privateKey ) );
    deviceKey->setText( QString::fromLatin1( keys.deviceKey ) );
    applyButton->click();
    QTRY_COMPARE_WITH_TIMEOUT( keyStatus->text(), QStringLiteral( "已验证 · 当前会话" ), 30000 );
    QVERIFY( !QgsMtplCredentialStore::hasRememberedKeys() );

    privateKey->setText( QString::fromLatin1( keys.privateKey ) );
    deviceKey->setText( QString::fromLatin1( keys.deviceKey ) );
    rememberKeys->setChecked( true );
    applyButton->click();
    QTRY_COMPARE_WITH_TIMEOUT( keyStatus->text(), QStringLiteral( "已验证 · 已安全保存" ), 30000 );
    QVERIFY( privateKey->text().isEmpty() );
    QVERIFY( deviceKey->text().isEmpty() );

    QgsMtpl::CryptoKeys storedKeys = QgsMtplCredentialStore::rememberedKeys( &error );
    QVERIFY2( error.isEmpty(), qPrintable( error ) );
    QCOMPARE( storedKeys.privateKey, keys.privateKey );
    QCOMPARE( storedKeys.deviceKey, keys.deviceKey );
    QCOMPARE( QgsMtplCredentialStore::lastPath(), QFileInfo( encryptedPath ).absoluteFilePath() );
    storedKeys.clear();
    dock.cancelPendingOperations();
  }

  {
    const int loadCountBeforeConstruction = mCredentialBackend.loadCount;
    QgsMtplDockWidget restored;
    QLineEdit *privateKey = restored.findChild<QLineEdit *>( QStringLiteral( "mtplPrivateKeyEdit" ) );
    QLineEdit *deviceKey = restored.findChild<QLineEdit *>( QStringLiteral( "mtplDeviceKeyEdit" ) );
    QPushButton *clearButton = restored.findChild<QPushButton *>( QStringLiteral( "mtplClearRememberedKeyButton" ) );
    QVERIFY( privateKey );
    QVERIFY( deviceKey );
    QVERIFY( clearButton );
    QVERIFY( privateKey->text().isEmpty() );
    QVERIFY( deviceKey->text().isEmpty() );
    QCOMPARE( privateKey->echoMode(), QLineEdit::Password );
    QCOMPARE( deviceKey->echoMode(), QLineEdit::Password );
    QCOMPARE( mCredentialBackend.loadCount, loadCountBeforeConstruction );

    restored.setSelectedPath( encryptedPath );
    QTRY_VERIFY_WITH_TIMEOUT(
      restored.currentProbe().packages.size() == 1 &&
        restored.currentProbe().packages.constFirst().readiness == QgsMtpl::ReadinessState::KeyVerified,
      30000 );
    QVERIFY( mCredentialBackend.loadCount > loadCountBeforeConstruction );

    clearButton->click();
    QVERIFY( privateKey->text().isEmpty() );
    QVERIFY( deviceKey->text().isEmpty() );
    QVERIFY( QgsMtplCredentialStore::rememberedKeys().isEmpty() );
    restored.cancelPendingOperations();
  }

  keys.clear();
  QgsMtplCredentialStore::clear();
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

void TestMtplWidget::ptpCreateInputValidationAndRecovery()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QString sourcePath = directory.filePath( QStringLiteral( "原始影像/0/0" ) );
  QVERIFY( QDir().mkpath( sourcePath ) );
  QString error;
  const QByteArray image = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "PNG" ), 256, 256, 1, error );
  QVERIFY2( !image.isEmpty(), qPrintable( error ) );
  QFile sourceFile( QDir( sourcePath ).filePath( QStringLiteral( "0.png" ) ) );
  QVERIFY( sourceFile.open( QIODevice::WriteOnly ) );
  QCOMPARE( sourceFile.write( image ), static_cast<qint64>( image.size() ) );
  sourceFile.close();

  QgsMtplPackageToolsWidget widget;
  QComboBox *operation = widget.findChild<QComboBox *>( QStringLiteral( "mtplPackageToolAction" ) );
  QLineEdit *source = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolSource" ) );
  QLineEdit *output = widget.findChild<QLineEdit *>( QStringLiteral( "mtplPackageToolOutput" ) );
  QComboBox *tileSize = widget.findChild<QComboBox *>( QStringLiteral( "mtplPackageToolTileSize" ) );
  QPushButton *run = widget.findChild<QPushButton *>( QStringLiteral( "mtplPackageToolRun" ) );
  QPlainTextEdit *details = widget.findChild<QPlainTextEdit *>( QStringLiteral( "mtplPackageToolResult" ) );
  QVERIFY( operation && source && output && tileSize && run && details );
  operation->setCurrentIndex( operation->findText( QStringLiteral( "创建 PTP 数据包" ) ) );
  source->clear();
  run->click();
  QVERIFY( !widget.isOperationRunning() );
  QVERIFY( details->toPlainText().contains( QStringLiteral( "请选择原始影像目录" ) ) );

  source->setText( QStringLiteral( "  \"%1\"  " ).arg( directory.filePath( QStringLiteral( "原始影像" ) ) ) );
  const QString outputPath = directory.filePath( QStringLiteral( "created.ptp" ) );
  output->setText( QStringLiteral( "  \"%1\"  " ).arg( outputPath ) );
  tileSize->setCurrentIndex( tileSize->findData( 129 ) );
  QCOMPARE( tileSize->currentData().toInt(), 129 );
  run->click();
  QTRY_VERIFY_WITH_TIMEOUT( !widget.isOperationRunning(), 30000 );
  QCOMPARE( widget.property( "mtplOperationState" ).toString(), QStringLiteral( "failed" ) );
  QVERIFY( !QFileInfo::exists( outputPath ) );
  QVERIFY2( details->toPlainText().contains( QStringLiteral( "尺寸" ) ), qPrintable( details->toPlainText() ) );

  tileSize->setCurrentIndex( tileSize->findData( 256 ) );
  run->click();
  QTRY_VERIFY_WITH_TIMEOUT( !widget.isOperationRunning(), 30000 );
  QCOMPARE( widget.property( "mtplOperationState" ).toString(), QStringLiteral( "succeeded" ) );
  const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath( outputPath );
  QVERIFY2( probe.ok, qPrintable( probe.error ) );
  const QgsMtpl::TileDatasetBuildResult dataset = QgsMtpl::buildTileDataset( outputPath, false, probe.packages );
  QVERIFY2( dataset.ok(), qPrintable( dataset.errorString() ) );
  QVERIFY( sourceFile.open( QIODevice::ReadOnly ) );
  QCOMPARE( sourceFile.readAll(), image );
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

void TestMtplWidget::dockDatasetSummaryAndLoadControls()
{
  QMainWindow window;
  QgsMtplDockWidget dock( &window );
  window.addDockWidget( Qt::RightDockWidgetArea, &dock );
  window.resize( 1100, 820 );
  window.show();

  QWidget *packagesTab = dock.findChild<QWidget *>( QStringLiteral( "mtplPackagesTab" ) );
  QgsMtplPartitionRuleWidget *ruleWidget = dock.findChild<QgsMtplPartitionRuleWidget *>( QStringLiteral( "mtplPartitionRuleWidget" ) );
  QGroupBox *datasetSummaryGroup = dock.findChild<QGroupBox *>( QStringLiteral( "mtplTileDatasetSummaryGroup" ) );
  QLabel *datasetSummary = dock.findChild<QLabel *>( QStringLiteral( "mtplTileDatasetSummaryLabel" ) );
  QLabel *keyStatus = dock.findChild<QLabel *>( QStringLiteral( "mtplKeyStatusLabel" ) );
  QLineEdit *layerName = dock.findChild<QLineEdit *>( QStringLiteral( "mtplLayerNameEdit" ) );
  QPushButton *loadButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplLoadButton" ) );
  QTreeWidget *packageTree = dock.findChild<QTreeWidget *>( QStringLiteral( "mtplPackageTree" ) );
  QComboBox *payloadOverride = dock.findChild<QComboBox *>( QStringLiteral( "mtplPayloadOverride" ) );
  QPushButton *applyOverrides = dock.findChild<QPushButton *>( QStringLiteral( "mtplApplyOverridesButton" ) );
  QVERIFY( packagesTab );
  QVERIFY( ruleWidget );
  QVERIFY( datasetSummaryGroup );
  QVERIFY( datasetSummary );
  QVERIFY( keyStatus );
  QVERIFY( layerName );
  QVERIFY( loadButton );
  QVERIFY( packageTree );
  QVERIFY( payloadOverride );
  QVERIFY( applyOverrides );
  QVERIFY( isDescendantOf( ruleWidget, packagesTab ) );
  QVERIFY( isDescendantOf( datasetSummaryGroup, packagesTab ) );
  QCOMPARE( datasetSummaryGroup->title(), QStringLiteral( "数据集摘要" ) );

  dock.setSelectedPath( mPortablePtpPath );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().packages.size() == 1 && dock.currentProbe().packages.constFirst().format == QgsMtpl::PackageFormat::Ptp, 30000 );
  QTRY_VERIFY_WITH_TIMEOUT( datasetSummary->text().contains( QStringLiteral( "数据集状态：可加载" ) ), 30000 );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "包数量：1" ) ) );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "格式：PTP" ) ) );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "瓦片大小：256 px" ) ) );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "Scheme：XYZ" ) ) );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "CRS：EPSG:3857" ) ) );
  QVERIFY2( datasetSummary->text().contains( QStringLiteral( "数据集允许层级：Z0–Z19" ) ),
            qPrintable( datasetSummary->text() ) );
  QVERIFY2( datasetSummary->text().contains( QStringLiteral( "有数据层级：Z0\n" ) ),
            qPrintable( datasetSummary->text() ) );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "局部空洞及覆盖范围外均透明显示" ) ) );
  QCOMPARE( ruleWidget->currentRuleId(), QgsMtpl::PartitionRuleStore::ruleOneId() );
  QCOMPARE( keyStatus->text(), QStringLiteral( "无需密钥" ) );
  QCOMPARE( layerName->text(), QStringLiteral( "0-7-0-0-0" ) );
  QCOMPARE( loadButton->text(), QStringLiteral( "加载为单一瓦片图层" ) );
  QVERIFY( loadButton->isEnabled() );
  QCOMPARE( packageTree->topLevelItemCount(), 1 );
  QVERIFY( !( packageTree->topLevelItem( 0 )->flags() & Qt::ItemIsUserCheckable ) );
  QCOMPARE( packageTree->topLevelItem( 0 )->checkState( 0 ), Qt::Checked );
  QCOMPARE( packageTree->headerItem()->text( 4 ), QStringLiteral( "单包状态" ) );
  QCOMPARE( packageTree->topLevelItem( 0 )->text( 4 ), QStringLiteral( "可读取" ) );

  packageTree->setCurrentItem( packageTree->topLevelItem( 0 ) );
  QVERIFY( payloadOverride->isHidden() );
  applyOverrides->click();
  QVERIFY( !( packageTree->topLevelItem( 0 )->flags() & Qt::ItemIsUserCheckable ) );
  QCOMPARE( packageTree->topLevelItem( 0 )->checkState( 0 ), Qt::Checked );
  QCOMPARE( dock.currentProbe().packages.constFirst().payload, QgsMtpl::PayloadType::RasterImage );

  dock.setSelectedPath( mPortableVtpPath );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().packages.size() == 1 && dock.currentProbe().packages.constFirst().format == QgsMtpl::PackageFormat::Vtp, 30000 );
  QTRY_VERIFY_WITH_TIMEOUT( datasetSummary->text().contains( QStringLiteral( "不含 PTP 数据包" ) ), 30000 );
  QCOMPARE( keyStatus->text(), QStringLiteral( "无需密钥" ) );
  QCOMPARE( layerName->text(), QStringLiteral( "portable-widget" ) );
  QCOMPARE( loadButton->text(), QStringLiteral( "加载到地图" ) );
  QVERIFY( loadButton->isEnabled() );

  dock.cancelPendingOperations();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
}

void TestMtplWidget::dockCompletedPtpPathLoadsOnFirstClick_data()
{
  QTest::addColumn<bool>( "directorySelection" );
  QTest::addColumn<bool>( "quotedPath" );
  QTest::addColumn<bool>( "metadataSfp" );
  QTest::addColumn<bool>( "selectedNonPtp" );
  QTest::newRow( "file-native-path" ) << false << false << false << false;
  QTest::newRow( "directory-native-path" ) << true << false << false << false;
  QTest::newRow( "file-quoted-forward-path" ) << false << true << false << false;
  QTest::newRow( "directory-quoted-forward-path" ) << true << true << false << false;
  QTest::newRow( "directory-unselected-metadata-sfp" ) << true << false << true << false;
  QTest::newRow( "directory-quoted-unselected-metadata-sfp" ) << true << true << true << false;
  QTest::newRow( "directory-selected-non-ptp-preserves-recheck" ) << true << false << false << true;
}

void TestMtplWidget::dockCompletedPtpPathLoadsOnFirstClick()
{
  QFETCH( bool, directorySelection );
  QFETCH( bool, quotedPath );
  QFETCH( bool, metadataSfp );
  QFETCH( bool, selectedNonPtp );
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QString packagePath = directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) );
  QString error;
  const QByteArray image = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "png" ), 256, 256, 1, error );
  QVERIFY2( !image.isEmpty(), qPrintable( error ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              packagePath, 256, QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
              MTPL_STORAGE_PLAIN, {}, { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, image } }, error ),
            qPrintable( error ) );
  if ( metadataSfp )
  {
    QVERIFY2( QgsMtplTest::writeSfpFixture(
                directory.filePath( QStringLiteral( "metadata.sfp" ) ),
                { { QStringLiteral( "metadata.json" ), QByteArrayLiteral( "{}" ), MTPL_STORAGE_PLAIN } }, {}, error ),
              qPrintable( error ) );
  }
  if ( selectedNonPtp )
  {
    QVERIFY2( QgsMtplTest::writeSfpFixture(
                directory.filePath( QStringLiteral( "selected.sfp" ) ),
                { { QStringLiteral( "points.geojson" ),
                    QByteArrayLiteral( "{\"type\":\"FeatureCollection\",\"features\":[]}" ), MTPL_STORAGE_PLAIN } },
                {}, error ), qPrintable( error ) );
  }

  QMainWindow window;
  QgsMtplDockWidget dock( &window );
  window.addDockWidget( Qt::RightDockWidgetArea, &dock );
  window.resize( 1100, 820 );
  window.show();
  window.activateWindow();
  QLineEdit *pathEdit = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPathEdit" ) );
  QPushButton *loadButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplLoadButton" ) );
  QLabel *summary = dock.findChild<QLabel *>( QStringLiteral( "mtplTileDatasetSummaryLabel" ) );
  QVERIFY( pathEdit );
  QVERIFY( loadButton );
  QVERIFY( summary );
  pathEdit->setFocus();
  QTRY_VERIFY_WITH_TIMEOUT( pathEdit->hasFocus(), 5000 );
  pathEdit->selectAll();
  const QString selectedPath = directorySelection ? directory.path() : packagePath;
  const QString pastedPath = quotedPath ? QStringLiteral( "  \"%1\"  " ).arg( QDir::fromNativeSeparators( selectedPath ) )
                                       : QDir::toNativeSeparators( selectedPath );
  // insert() follows the line editor's paste path without using the system clipboard.
  pathEdit->insert( pastedPath );
  const int expectedPackages = 1 + ( metadataSfp || selectedNonPtp ? 1 : 0 );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().packages.size() == expectedPackages &&
                           loadButton->isEnabled() && summary->text().contains( QStringLiteral( "数据集状态：可加载" ) ), 30000 );
  QVERIFY( pathEdit->hasFocus() );
  QCOMPARE( QDir::cleanPath( dock.selectedPath() ), QDir::cleanPath( selectedPath ) );
  QSignalSpy editingFinished( pathEdit, &QLineEdit::editingFinished );
  QSignalSpy loaded( &dock, &QgsMtplDockWidget::layersLoaded );
  QSignalSpy failed( &dock, &QgsMtplDockWidget::loadFailed );

  if ( selectedNonPtp )
  {
    QTreeWidget *tree = dock.findChild<QTreeWidget *>( QStringLiteral( "mtplPackageTree" ) );
    QVERIFY( tree );
    QTreeWidgetItem *otherEntry = nullptr;
    QTreeWidgetItemIterator iterator( tree );
    while ( *iterator )
    {
      if ( ( *iterator )->text( 0 ) == QLatin1String( "points.geojson" ) )
        otherEntry = *iterator;
      ++iterator;
    }
    QVERIFY( otherEntry );
    QVERIFY( otherEntry->flags() & Qt::ItemIsUserCheckable );
    otherEntry->setCheckState( 0, Qt::Checked );
    QCOMPARE( otherEntry->checkState( 0 ), Qt::Checked );
    // A genuine non-PTP selection keeps the existing commit-and-recheck behavior.
    QVERIFY( QMetaObject::invokeMethod( pathEdit, "editingFinished", Qt::DirectConnection ) );
    QCOMPARE( editingFinished.count(), 1 );
    QVERIFY( !loadButton->isEnabled() );
    QCOMPARE( loaded.count(), 0 );
    QCOMPARE( QgsProject::instance()->mapLayers().size(), 0 );
    dock.cancelPendingOperations();
    return;
  }

  QTest::mousePress( loadButton, Qt::LeftButton );
  QCOMPARE( editingFinished.count(), 1 );
  QVERIFY( dock.currentProbe().ok );
  QVERIFY( loadButton->isEnabled() );
  QTest::mouseRelease( loadButton, Qt::LeftButton );
  QTRY_VERIFY_WITH_TIMEOUT( !loaded.isEmpty() || !failed.isEmpty(), 30000 );
  QVERIFY2( failed.isEmpty(), qPrintable( failed.isEmpty() ? QString() : failed.constFirst().constFirst().toString() ) );
  QCOMPARE( loaded.count(), 1 );
  QCOMPARE( loaded.constFirst().constFirst().toInt(), 1 );
  QCOMPARE( QgsProject::instance()->mapLayers().size(), 1 );
  const auto *layer = qobject_cast<QgsMtplPluginLayer *>( QgsProject::instance()->mapLayers().constBegin().value() );
  QVERIFY( layer );
  QVERIFY( layer->isTileDataset() );
  QCOMPARE( layer->tileDataset()->packages.size(), 1 );
  QCOMPARE( layer->tileDataset()->directorySource, directorySelection );
  dock.cancelPendingOperations();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
}

void TestMtplWidget::dockDirectoryCreatesSingleDatasetLayer()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );

  const QByteArray metadata = QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" );
  const QString firstPackagePath = directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) );
  const QString secondPackagePath = directory.filePath( QStringLiteral( "8-11-3-1-1.ptp" ) );
  const QString refreshedPackagePath = directory.filePath( QStringLiteral( "8-11-3-2-1.ptp" ) );
  const QString invalidPackagePath = directory.filePath( QStringLiteral( "8-11-3-3-1.ptp" ) );
  QString error;
  const QByteArray tileImage = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "png" ), 256, 256, 1, error );
  QVERIFY2( !tileImage.isEmpty(), qPrintable( error ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              firstPackagePath,
              256,
              metadata,
              MTPL_STORAGE_PLAIN,
              {},
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, tileImage } },
              error ),
            qPrintable( error ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              secondPackagePath,
              256,
              metadata,
              MTPL_STORAGE_PLAIN,
              {},
              { { 8, 32, 32, 32, 32 } },
              { { 8, 32, 32, tileImage } },
              error ),
            qPrintable( error ) );

  QMainWindow window;
  QgsMtplDockWidget dock( &window );
  window.addDockWidget( Qt::RightDockWidgetArea, &dock );
  QPushButton *loadButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplLoadButton" ) );
  QPushButton *refreshButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplRefreshDatasetButton" ) );
  QVERIFY( loadButton );
  QVERIFY( refreshButton );

  dock.setSelectedPath( directory.path() );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().packages.size() == 2, 30000 );
  QTRY_VERIFY_WITH_TIMEOUT( loadButton->isEnabled(), 30000 );
  QLabel *summary = dock.findChild<QLabel *>( QStringLiteral( "mtplTileDatasetSummaryLabel" ) );
  QVERIFY( summary );
  QVERIFY2( summary->text().contains( QStringLiteral( "有数据层级：Z0、Z8" ) ), qPrintable( summary->text() ) );
  QVERIFY2( summary->text().contains( QStringLiteral( "数据集允许层级：Z0–Z19" ) ), qPrintable( summary->text() ) );
  QVERIFY2( summary->text().contains( QStringLiteral( "未发现 16-19 级分包，该级别范围将显示为透明。" ) ),
            qPrintable( summary->text() ) );

  QgsProject *project = QgsProject::instance();
  const QStringList existingLayerIds = project->mapLayers().keys();
  QSignalSpy loadedSpy( &dock, &QgsMtplDockWidget::layersLoaded );
  QSignalSpy partialSpy( &dock, &QgsMtplDockWidget::loadPartiallySucceeded );
  QSignalSpy failedSpy( &dock, &QgsMtplDockWidget::loadFailed );
  loadButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( !loadedSpy.isEmpty() || !partialSpy.isEmpty() || !failedSpy.isEmpty(), 30000 );

  QStringList addedLayerIds;
  const auto currentLayers = project->mapLayers();
  for ( auto it = currentLayers.constBegin(); it != currentLayers.constEnd(); ++it )
  {
    if ( !existingLayerIds.contains( it.key() ) )
      addedLayerIds.append( it.key() );
  }

  QgsMtplPluginLayer *datasetLayer = addedLayerIds.size() == 1
                                      ? qobject_cast<QgsMtplPluginLayer *>( project->mapLayer( addedLayerIds.constFirst() ) )
                                      : nullptr;
  const bool isMtplLayer = datasetLayer != nullptr;
  const bool isTileDataset = datasetLayer && datasetLayer->isTileDataset() && datasetLayer->tileDataset();
  const int packageCount = isTileDataset ? datasetLayer->tileDataset()->packages.size() : -1;
  QStringList packageNames;
  if ( isTileDataset )
  {
    for ( const QgsMtpl::TileDatasetPackage &package : datasetLayer->tileDataset()->packages )
      packageNames.append( QFileInfo( package.descriptor.path ).fileName() );
    packageNames.sort();
  }

  const QString failureMessage = failedSpy.isEmpty() ? QString() : failedSpy.constFirst().constFirst().toString();
  QVERIFY2( failedSpy.isEmpty(), qPrintable( failureMessage ) );
  QCOMPARE( partialSpy.count(), 0 );
  QCOMPARE( loadedSpy.count(), 1 );
  QCOMPARE( loadedSpy.constFirst().constFirst().toInt(), 1 );
  QCOMPARE( addedLayerIds.size(), 1 );
  QVERIFY( isMtplLayer );
  QVERIFY( isTileDataset );
  QCOMPARE( packageCount, 2 );
  QCOMPARE( datasetLayer->tileDataset()->minimumZoom, 0 );
  QCOMPARE( datasetLayer->tileDataset()->maximumZoom, 19 );
  QCOMPARE( datasetLayer->tileMatrixSet().maximumZoom(), 19 );
  QCOMPARE( datasetLayer->tileMatrixSet().scaleToZoomLevel(
              datasetLayer->tileMatrixSet().tileMatrix( 19 ).scale() / 30.0 ), 19 );
  QCOMPARE( packageNames,
            QStringList( { QStringLiteral( "0-7-0-0-0.ptp" ), QStringLiteral( "8-11-3-1-1.ptp" ) } ) );

  const QString datasetLayerId = datasetLayer->id();
  const QString datasetLayerName = datasetLayer->name();
  const int layerCountAfterFirstLoad = project->mapLayers().size();
  QSignalSpy activatedSpy( &dock, &QgsMtplDockWidget::existingLayerActivated );
  QTRY_VERIFY_WITH_TIMEOUT( refreshButton->isEnabled(), 30000 );
  QVERIFY2( loadButton->isEnabled(), qPrintable( loadButton->text() ) );

  loadButton->click();
  QTRY_COMPARE_WITH_TIMEOUT( activatedSpy.count(), 1, 30000 );
  QCOMPARE( activatedSpy.constFirst().constFirst().toString(), datasetLayerId );
  QCOMPARE( project->mapLayers().size(), layerCountAfterFirstLoad );
  QCOMPARE( project->mapLayer( datasetLayerId ), datasetLayer );

  const QByteArray refreshedMetadata = QByteArrayLiteral( "{\"tile_file_ext\":\"png\",\"refresh_marker\":\"v2\"}" );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              refreshedPackagePath,
              256,
              refreshedMetadata,
              MTPL_STORAGE_PLAIN,
              {},
              { { 8, 64, 64, 32, 32 } },
              {},
              error ),
            qPrintable( error ) );
  refreshButton->click();
  QTRY_COMPARE_WITH_TIMEOUT( activatedSpy.count(), 2, 30000 );
  QCOMPARE( project->mapLayers().size(), layerCountAfterFirstLoad );
  QCOMPARE( project->mapLayer( datasetLayerId ), datasetLayer );
  QCOMPARE( datasetLayer->name(), datasetLayerName );
  QVERIFY( datasetLayer->tileDataset() );
  QCOMPARE( datasetLayer->tileDataset()->matrix.tileSize, 256 );
  QCOMPARE( datasetLayer->tileDataset()->packages.size(), 3 );
  const auto refreshedPackage = std::find_if(
    datasetLayer->tileDataset()->packages.cbegin(), datasetLayer->tileDataset()->packages.cend(),
    [&refreshedPackagePath]( const QgsMtpl::TileDatasetPackage &package )
    {
      return QFileInfo( package.descriptor.path ).absoluteFilePath() == QFileInfo( refreshedPackagePath ).absoluteFilePath();
    } );
  QVERIFY( refreshedPackage != datasetLayer->tileDataset()->packages.cend() );
  QCOMPARE( refreshedPackage->descriptor.metadata.value( QStringLiteral( "refresh_marker" ) ).toString(),
            QStringLiteral( "v2" ) );
  const qint64 refreshedStamp = refreshedPackage->descriptor.fileLastModifiedMs;

  QVERIFY2( QgsMtplTest::writePtpFixture(
              invalidPackagePath,
              129,
              QByteArrayLiteral( "{\"tile_file_ext\":\"png\",\"refresh_marker\":\"v3\"}" ),
              MTPL_STORAGE_PLAIN,
              {},
              { { 8, 96, 96, 32, 32 } },
              {},
              error ),
            qPrintable( error ) );
  const int failuresBeforeRefresh = failedSpy.count();
  refreshButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( failedSpy.count() > failuresBeforeRefresh, 30000 );
  QVERIFY( failedSpy.constLast().constFirst().toString().contains( QStringLiteral( "现有图层保持不变" ) ) );
  QCOMPARE( activatedSpy.count(), 2 );
  QCOMPARE( project->mapLayers().size(), layerCountAfterFirstLoad );
  QCOMPARE( project->mapLayer( datasetLayerId ), datasetLayer );
  QVERIFY( datasetLayer->tileDataset() );
  QCOMPARE( datasetLayer->tileDataset()->matrix.tileSize, 256 );
  QCOMPARE( datasetLayer->tileDataset()->packages.size(), 3 );
  const auto retainedPackage = std::find_if(
    datasetLayer->tileDataset()->packages.cbegin(), datasetLayer->tileDataset()->packages.cend(),
    [&refreshedPackagePath]( const QgsMtpl::TileDatasetPackage &package )
    {
      return QFileInfo( package.descriptor.path ).absoluteFilePath() == QFileInfo( refreshedPackagePath ).absoluteFilePath();
    } );
  QVERIFY( retainedPackage != datasetLayer->tileDataset()->packages.cend() );
  QCOMPARE( retainedPackage->descriptor.metadata.value( QStringLiteral( "refresh_marker" ) ).toString(),
            QStringLiteral( "v2" ) );
  QCOMPARE( retainedPackage->descriptor.fileLastModifiedMs, refreshedStamp );

  const QgsMtpl::TileDatasetDescriptor restoredDataset = *datasetLayer->tileDataset();
  project->removeMapLayers( addedLayerIds );
  QTRY_VERIFY_WITH_TIMEOUT( !refreshButton->isEnabled(), 30000 );

  auto *restoredLayer = new QgsMtplPluginLayer( restoredDataset, QgsMtpl::CryptoKeys() );
  project->addMapLayer( restoredLayer );
  QTRY_VERIFY_WITH_TIMEOUT( refreshButton->isEnabled(), 30000 );
  project->removeMapLayer( restoredLayer );
  QTRY_VERIFY_WITH_TIMEOUT( !refreshButton->isEnabled(), 30000 );
  dock.cancelPendingOperations();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
}

void TestMtplWidget::dockMixedPtpCredentialsRecoverAfterReselection()
{
  QgsMtplCredentialStore::clear();
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QStringList paths = {
    directory.filePath( QStringLiteral( "8-11-3-0-0.ptp" ) ),
    directory.filePath( QStringLiteral( "8-11-3-1-0.ptp" ) )
  };
  const QList<QgsMtpl::CryptoKeys> keys = { QgsMtplTest::fixtureKeys(), QgsMtplTest::differentFixtureKeys() };
  QString error;
  for ( int index = 0; index < paths.size(); ++index )
  {
    const quint32 x = static_cast<quint32>( index * 32 );
    const QByteArray image = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "png" ), 256, 256, index + 1, error );
    QVERIFY2( !image.isEmpty(), qPrintable( error ) );
    QVERIFY2( QgsMtplTest::writePtpFixture(
                paths.at( index ), 256, QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
                MTPL_STORAGE_ENCRYPTED, keys.at( index ), { { 8, x, x, 0, 0 } }, { { 8, x, 0, image } }, error ),
              qPrintable( error ) );
  }

  QgsMtplDockWidget dock;
  QPushButton *loadButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplLoadButton" ) );
  QPushButton *applyButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplApplyRememberedKeyButton" ) );
  QLineEdit *privateKey = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPrivateKeyEdit" ) );
  QLineEdit *deviceKey = dock.findChild<QLineEdit *>( QStringLiteral( "mtplDeviceKeyEdit" ) );
  QTreeWidget *packageTree = dock.findChild<QTreeWidget *>( QStringLiteral( "mtplPackageTree" ) );
  QVERIFY( loadButton );
  QVERIFY( applyButton );
  QVERIFY( privateKey );
  QVERIFY( deviceKey );
  QVERIFY( packageTree );
  QgsProject *project = QgsProject::instance();
  const QStringList originalLayerIds = project->mapLayers().keys();
  QSignalSpy loadedSpy( &dock, &QgsMtplDockWidget::layersLoaded );
  QSignalSpy partialSpy( &dock, &QgsMtplDockWidget::loadPartiallySucceeded );
  QSignalSpy failedSpy( &dock, &QgsMtplDockWidget::loadFailed );

  dock.setSelectedPath( directory.path() );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().packages.size() == 2 &&
                            dock.currentProbe().readyPackageCount() == 0 && applyButton->isEnabled(), 30000 );
  QVERIFY( !loadButton->isEnabled() );
  privateKey->setText( QString::fromLatin1( keys.constFirst().privateKey ) );
  deviceKey->setText( QString::fromLatin1( keys.constFirst().deviceKey ) );
  applyButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().readyPackageCount() == 1 && applyButton->isEnabled(), 30000 );
  QCOMPARE( dock.currentProbe().packages.size(), 2 );
  const QgsMtpl::ProbeResult partiallyUnlocked = dock.currentProbe();
  QVERIFY( std::any_of( partiallyUnlocked.packages.cbegin(), partiallyUnlocked.packages.cend(),
                       []( const QgsMtpl::PackageDescriptor &package )
                       { return package.readiness == QgsMtpl::ReadinessState::KeyRejectedOrCorrupt; } ) );
  QVERIFY( !loadButton->isEnabled() );
  loadButton->click();
  QCOMPARE( project->mapLayers().keys(), originalLayerIds );
  QCOMPARE( loadedSpy.count(), 0 );
  QCOMPARE( partialSpy.count(), 0 );
  privateKey->clear();
  deviceKey->clear();

  QStringList sidecarPaths;
  QStringList keyIds;
  for ( int index = 0; index < paths.size(); ++index )
  {
    QgsMtpl::KeyMaterial material;
    material.keyId = index == 0 ? QStringLiteral( "dba2fe86-64af-4c93-83ad-0ee1ecbe8b25" )
                                : QStringLiteral( "7bd104cb-174b-4ad2-a8b8-9a9de482c57c" );
    material.privateKeyBase64 = keys.at( index ).privateKey;
    material.deviceKeyHex = keys.at( index ).deviceKey;
    const QString sidecarPath = QgsMtpl::KeySidecarStore::singleSidecarPath( paths.at( index ) );
    QgsMtpl::KeySidecar sidecar;
    QVERIFY2( QgsMtpl::KeySidecarStore::createForPackages(
                sidecarPath, { paths.at( index ) }, material, sidecar, error ), qPrintable( error ) );
    QVERIFY2( QgsMtpl::KeySidecarStore::write( sidecarPath, sidecar, error ), qPrintable( error ) );
    sidecarPaths.append( sidecarPath );
    keyIds.append( material.keyId );
    if ( index == 0 )
    {
      dock.setSelectedPath( directory.path() );
      QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().readyPackageCount() == 1 && applyButton->isEnabled(), 30000 );
      QVERIFY( !loadButton->isEnabled() );
      QCOMPARE( project->mapLayers().keys(), originalLayerIds );
    }
  }

  // Replace in-flight checks with different selections before applying the final directory result.
  for ( int pass = 0; pass < 3; ++pass )
  {
    dock.setSelectedPath( paths.at( pass % paths.size() ) );
    dock.setSelectedPath( mPortablePtpPath );
    dock.setSelectedPath( directory.path() );
    QVERIFY( !loadButton->isEnabled() );
  }
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().isDirectorySelection && dock.currentProbe().packages.size() == 2 &&
                            dock.currentProbe().readyPackageCount() == 2 && loadButton->isEnabled(), 30000 );
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
  QCoreApplication::processEvents();
  QCOMPARE( QFileInfo( dock.selectedPath() ).absoluteFilePath(), QFileInfo( directory.path() ).absoluteFilePath() );
  QStringList displayedPackageNames;
  for ( int row = 0; row < packageTree->topLevelItemCount(); ++row )
  {
    const QTreeWidgetItem *item = packageTree->topLevelItem( row );
    if ( item->text( 1 ) == QgsMtpl::packageFormatName( QgsMtpl::PackageFormat::Ptp ) )
      displayedPackageNames.append( item->text( 0 ) );
  }
  displayedPackageNames.sort();
  QCOMPARE( displayedPackageNames,
            QStringList( { QStringLiteral( "8-11-3-0-0.ptp" ), QStringLiteral( "8-11-3-1-0.ptp" ) } ) );
  QCOMPARE( dock.currentProbe().readyPackageCount(), 2 );
  QVERIFY( loadButton->isEnabled() );
  for ( const QgsMtpl::PackageDescriptor &package : dock.currentProbe().packages )
  {
    const int index = paths.indexOf( package.path );
    QVERIFY( index >= 0 );
    QCOMPARE( package.credentialSource, QgsMtpl::CredentialSource::Sidecar );
    QCOMPARE( package.sidecarPath, sidecarPaths.at( index ) );
    QCOMPARE( package.keyId, keyIds.at( index ) );
  }

  loadButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( !loadedSpy.isEmpty() || !failedSpy.isEmpty() || !partialSpy.isEmpty(), 30000 );
  QVERIFY2( failedSpy.isEmpty(), failedSpy.isEmpty() ? "" : qPrintable( failedSpy.constFirst().constFirst().toString() ) );
  QCOMPARE( partialSpy.count(), 0 );
  QCOMPARE( loadedSpy.count(), 1 );
  QCOMPARE( loadedSpy.constFirst().constFirst().toInt(), 1 );
  QList<QgsMapLayer *> addedLayers;
  for ( QgsMapLayer *layer : project->mapLayers() )
  {
    if ( !originalLayerIds.contains( layer->id() ) )
      addedLayers.append( layer );
  }
  QCOMPARE( addedLayers.size(), 1 );
  QgsMtplPluginLayer *layer = qobject_cast<QgsMtplPluginLayer *>( addedLayers.constFirst() );
  QVERIFY( layer );
  QVERIFY( layer->isTileDataset() );
  QVERIFY( layer->hasCryptoKeys() );
  QCOMPARE( layer->tileDataset()->packages.size(), 2 );

  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( directory.filePath( QStringLiteral( "mixed-credentials.qgs" ) ) ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayer = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayer );
  QVERIFY( layer->writeLayerXml( mapLayer, document, context ) );
  const QByteArray savedXml = document.toByteArray();
  for ( const QgsMtpl::CryptoKeys &key : keys )
  {
    QVERIFY( !savedXml.contains( key.privateKey ) );
    QVERIFY( !savedXml.contains( key.deviceKey ) );
  }
  dock.cancelPendingOperations();
  project->removeMapLayer( layer );
  QgsMtplCredentialStore::clear();

  // A reopened layer recovers both independent credentials through its relative sidecar references.
  QgsMtplPluginLayer restored;
  QVERIFY( restored.readLayerXml( document.documentElement(), context ) );
  QVERIFY( restored.isValid() );
  QVERIFY( restored.isTileDataset() );
  QVERIFY( restored.hasCryptoKeys() );
  QCOMPARE( restored.tileDataset()->packages.size(), 2 );
  for ( const QgsMtpl::TileDatasetPackage &package : restored.tileDataset()->packages )
  {
    const int index = paths.indexOf( package.descriptor.path );
    QVERIFY( index >= 0 );
    QVERIFY( package.descriptor.isReady() );
    QCOMPARE( package.descriptor.credentialSource, QgsMtpl::CredentialSource::Sidecar );
    QCOMPARE( package.descriptor.keyId, keyIds.at( index ) );
  }
}

void TestMtplWidget::dockPtpSelectionPrefersSidecarOverSessionKeys()
{
  QgsMtplCredentialStore::clear();
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QString firstPath = directory.filePath( QStringLiteral( "first/0-7-0-0-0.ptp" ) );
  const QString secondDirectory = directory.filePath( QStringLiteral( "second" ) );
  const QString secondPath = QDir( secondDirectory ).filePath( QStringLiteral( "0-7-0-0-0.ptp" ) );
  const QgsMtpl::CryptoKeys firstKeys = QgsMtplTest::fixtureKeys();
  const QgsMtpl::CryptoKeys secondKeys = QgsMtplTest::differentFixtureKeys();
  QString error;
  const QByteArray image = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "png" ), 256, 256, 43, error );
  QVERIFY2( !image.isEmpty(), qPrintable( error ) );
  for ( const QString &path : { firstPath, secondPath } )
  {
    QVERIFY2( QgsMtplTest::writePtpFixture(
                path, 256, QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ), MTPL_STORAGE_ENCRYPTED,
                path == firstPath ? firstKeys : secondKeys,
                { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, image } }, error ), qPrintable( error ) );
  }
  QgsMtpl::KeyMaterial material;
  material.keyId = QStringLiteral( "ed5501e2-21e0-4e71-88c2-bdba048b72df" );
  material.privateKeyBase64 = secondKeys.privateKey;
  material.deviceKeyHex = secondKeys.deviceKey;
  const QString sidecarPath = QgsMtpl::KeySidecarStore::singleSidecarPath( secondPath );
  QgsMtpl::KeySidecar sidecar;
  QVERIFY2( QgsMtpl::KeySidecarStore::createForPackages(
              sidecarPath, { secondPath }, material, sidecar, error ), qPrintable( error ) );
  QVERIFY2( QgsMtpl::KeySidecarStore::write( sidecarPath, sidecar, error ), qPrintable( error ) );

  // The service option is opt-in. An explicit check must not silently use the sidecar instead.
  const QgsMtpl::ProbeResult explicitProbe = QgsMtplPackageService::probePath(
    secondPath, firstKeys, true, QgsMtpl::CredentialSource::Explicit );
  QVERIFY( explicitProbe.ok );
  QCOMPARE( explicitProbe.packages.size(), 1 );
  QCOMPARE( explicitProbe.packages.constFirst().readiness, QgsMtpl::ReadinessState::KeyRejectedOrCorrupt );
  const QgsMtpl::ProbeResult selectionProbe = QgsMtplPackageService::probePath(
    secondPath, firstKeys, true, QgsMtpl::CredentialSource::Explicit, {}, {}, true );
  QVERIFY( selectionProbe.ok );
  QCOMPARE( selectionProbe.readyPackageCount(), 1 );
  QCOMPARE( selectionProbe.packages.constFirst().credentialSource, QgsMtpl::CredentialSource::Sidecar );

  QgsMtplDockWidget dock;
  QLineEdit *privateKey = dock.findChild<QLineEdit *>( QStringLiteral( "mtplPrivateKeyEdit" ) );
  QLineEdit *deviceKey = dock.findChild<QLineEdit *>( QStringLiteral( "mtplDeviceKeyEdit" ) );
  QPushButton *applyButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplApplyRememberedKeyButton" ) );
  QPushButton *loadButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplLoadButton" ) );
  QVERIFY( privateKey && deviceKey && applyButton && loadButton );
  dock.setSelectedPath( firstPath );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().packages.size() == 1 &&
                            dock.currentProbe().packages.constFirst().readiness == QgsMtpl::ReadinessState::KeyRequired &&
                            applyButton->isEnabled(), 30000 );
  privateKey->setText( QString::fromLatin1( firstKeys.privateKey ) );
  deviceKey->setText( QString::fromLatin1( firstKeys.deviceKey ) );
  applyButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().readyPackageCount() == 1 && loadButton->isEnabled(), 30000 );
  QCOMPARE( dock.currentProbe().packages.constFirst().credentialSource, QgsMtpl::CredentialSource::Explicit );
  QVERIFY( privateKey->text().isEmpty() );
  QVERIFY( deviceKey->text().isEmpty() );
  QVERIFY( !QgsMtplCredentialStore::hasRememberedKeys() );

  dock.setSelectedPath( secondDirectory );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().isDirectorySelection && dock.currentProbe().readyPackageCount() == 1 &&
                            loadButton->isEnabled(), 30000 );
  QCOMPARE( dock.currentProbe().packages.constFirst().path, secondPath );
  QCOMPARE( dock.currentProbe().packages.constFirst().credentialSource, QgsMtpl::CredentialSource::Sidecar );
  QCOMPARE( dock.currentProbe().packages.constFirst().sidecarPath, sidecarPath );
  QCOMPARE( dock.currentProbe().packages.constFirst().keyId, material.keyId );

  // New explicit input still validates exactly those keys, even when a matching sidecar exists.
  privateKey->setText( QString::fromLatin1( firstKeys.privateKey ) );
  deviceKey->setText( QString::fromLatin1( firstKeys.deviceKey ) );
  applyButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().packages.constFirst().readiness == QgsMtpl::ReadinessState::KeyRejectedOrCorrupt &&
                            applyButton->isEnabled(), 30000 );
  QVERIFY( !loadButton->isEnabled() );
  privateKey->clear();
  deviceKey->clear();

  // Returning to a package without a sidecar may still use the previously validated session key.
  dock.setSelectedPath( firstPath );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().readyPackageCount() == 1 && loadButton->isEnabled(), 30000 );
  QCOMPARE( dock.currentProbe().packages.constFirst().path, firstPath );
  QCOMPARE( dock.currentProbe().packages.constFirst().credentialSource, QgsMtpl::CredentialSource::Explicit );
  dock.setSelectedPath( secondDirectory );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().isDirectorySelection && dock.currentProbe().readyPackageCount() == 1 &&
                            loadButton->isEnabled(), 30000 );
  QCOMPARE( dock.currentProbe().packages.constFirst().credentialSource, QgsMtpl::CredentialSource::Sidecar );
  QgsProject *project = QgsProject::instance();
  const QStringList originalLayerIds = project->mapLayers().keys();
  QSignalSpy loadedSpy( &dock, &QgsMtplDockWidget::layersLoaded );
  QSignalSpy partialSpy( &dock, &QgsMtplDockWidget::loadPartiallySucceeded );
  QSignalSpy failedSpy( &dock, &QgsMtplDockWidget::loadFailed );
  loadButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( !loadedSpy.isEmpty() || !failedSpy.isEmpty() || !partialSpy.isEmpty(), 30000 );
  QVERIFY2( failedSpy.isEmpty(), failedSpy.isEmpty() ? "" : qPrintable( failedSpy.constFirst().constFirst().toString() ) );
  QCOMPARE( partialSpy.count(), 0 );
  QCOMPARE( loadedSpy.count(), 1 );
  QCOMPARE( loadedSpy.constFirst().constFirst().toInt(), 1 );
  QList<QgsMapLayer *> addedLayers;
  for ( QgsMapLayer *layer : project->mapLayers() )
  {
    if ( !originalLayerIds.contains( layer->id() ) )
      addedLayers.append( layer );
  }
  QCOMPARE( addedLayers.size(), 1 );
  QgsMtplPluginLayer *layer = qobject_cast<QgsMtplPluginLayer *>( addedLayers.constFirst() );
  QVERIFY( layer && layer->isTileDataset() && layer->hasCryptoKeys() );
  QCOMPARE( layer->tileDataset()->packages.constFirst().descriptor.credentialSource, QgsMtpl::CredentialSource::Sidecar );
  dock.cancelPendingOperations();
  project->removeMapLayer( layer );
  QgsMtplCredentialStore::clear();
}

void TestMtplWidget::dockRejectsPtpReplacementBetweenProbeAndLoad()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QString path = directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) );
  QString error;
  const QByteArray image = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "png" ), 256, 256, 31, error );
  QVERIFY2( !image.isEmpty(), qPrintable( error ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              path, 256, QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
              MTPL_STORAGE_PLAIN, {}, { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, image } }, error ),
            qPrintable( error ) );
  QFile source( path );
  QVERIFY( source.open( QIODevice::ReadOnly ) );
  const QByteArray original = source.readAll();
  source.close();
  QVERIFY( !original.isEmpty() );

  QgsMtplDockWidget dock;
  QPushButton *loadButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplLoadButton" ) );
  QVERIFY( loadButton );
  QgsProject *project = QgsProject::instance();
  const QStringList originalLayerIds = project->mapLayers().keys();
  QSignalSpy loadedSpy( &dock, &QgsMtplDockWidget::layersLoaded );
  QSignalSpy partialSpy( &dock, &QgsMtplDockWidget::loadPartiallySucceeded );
  QSignalSpy failedSpy( &dock, &QgsMtplDockWidget::loadFailed );
  dock.setSelectedPath( path );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().readyPackageCount() == 1 &&
                            loadButton->isEnabled(), 30000 );

  // Change the source after the successful check without asking the dock to check it again.
  const QByteArray replacement = QByteArrayLiteral( "This replacement is not an MTPL package." );
  QVERIFY( source.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
  QCOMPARE( source.write( replacement ), static_cast<qint64>( replacement.size() ) );
  source.close();
  QCOMPARE( dock.currentProbe().readyPackageCount(), 1 );
  QVERIFY( loadButton->isEnabled() );
  loadButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( !failedSpy.isEmpty(), 30000 );
  QVERIFY( !failedSpy.constFirst().constFirst().toString().isEmpty() );
  QCOMPARE( loadedSpy.count(), 0 );
  QCOMPARE( partialSpy.count(), 0 );
  QCOMPARE( project->mapLayers().keys(), originalLayerIds );

  QVERIFY( source.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
  QCOMPARE( source.write( original ), static_cast<qint64>( original.size() ) );
  source.close();
  failedSpy.clear();
  dock.setSelectedPath( path );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().readyPackageCount() == 1 &&
                            loadButton->isEnabled(), 30000 );
  loadButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( !loadedSpy.isEmpty() || !failedSpy.isEmpty() || !partialSpy.isEmpty(), 30000 );
  QVERIFY2( failedSpy.isEmpty(), failedSpy.isEmpty() ? "" : qPrintable( failedSpy.constFirst().constFirst().toString() ) );
  QCOMPARE( partialSpy.count(), 0 );
  QCOMPARE( loadedSpy.count(), 1 );
  QCOMPARE( loadedSpy.constFirst().constFirst().toInt(), 1 );
  QList<QgsMapLayer *> addedLayers;
  for ( QgsMapLayer *layer : project->mapLayers() )
  {
    if ( !originalLayerIds.contains( layer->id() ) )
      addedLayers.append( layer );
  }
  QCOMPARE( addedLayers.size(), 1 );
  QgsMtplPluginLayer *layer = qobject_cast<QgsMtplPluginLayer *>( addedLayers.constFirst() );
  QVERIFY( layer );
  QVERIFY( layer->isTileDataset() );
  QCOMPARE( layer->tileDataset()->packages.size(), 1 );
  QCOMPARE( layer->tileDataset()->packages.constFirst().descriptor.path, path );
  dock.cancelPendingOperations();
  project->removeMapLayer( layer );
}

void TestMtplWidget::dockRejectsInvalidPtpDatasetAtomically()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );

  const QByteArray metadata = QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" );
  QString error;
  QVERIFY2( QgsMtplTest::writePtpFixture(
              directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 256, metadata,
              MTPL_STORAGE_PLAIN, {}, { { 0, 0, 0, 0, 0 } }, {}, error ),
            qPrintable( error ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              directory.filePath( QStringLiteral( "8-11-3-1-1.ptp" ) ), 129, metadata,
              MTPL_STORAGE_PLAIN, {}, { { 8, 32, 32, 32, 32 } }, {}, error ),
            qPrintable( error ) );

  QgsMtplDockWidget dock;
  QLabel *datasetSummary = dock.findChild<QLabel *>( QStringLiteral( "mtplTileDatasetSummaryLabel" ) );
  QLabel *batchSummary = dock.findChild<QLabel *>( QStringLiteral( "mtplBatchSummaryLabel" ) );
  QPushButton *loadButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplLoadButton" ) );
  QTreeWidget *packageTree = dock.findChild<QTreeWidget *>( QStringLiteral( "mtplPackageTree" ) );
  QVERIFY( datasetSummary );
  QVERIFY( batchSummary );
  QVERIFY( loadButton );
  QVERIFY( packageTree );

  dock.setSelectedPath( directory.path() );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().packages.size() == 2, 30000 );
  QTRY_VERIFY_WITH_TIMEOUT( datasetSummary->text().contains( QStringLiteral( "数据集状态：不可加载" ) ), 30000 );
  QCOMPARE( batchSummary->text(), QStringLiteral( "2 个可读取" ) );
  QCOMPARE( loadButton->text(), QStringLiteral( "当前 PTP 数据集不可加载" ) );
  QVERIFY( !loadButton->isEnabled() );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "首个问题包：" ) ) );
  QVERIFY( !loadButton->toolTip().isEmpty() );

  QTemporaryDir vectorDirectory;
  QVERIFY( vectorDirectory.isValid() );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              vectorDirectory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 256,
              QByteArrayLiteral( "{\"tile_file_ext\":\"pbf\"}" ), MTPL_STORAGE_PLAIN, {},
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, QgsMtplTest::minimalVectorTilePayload() } }, error ),
            qPrintable( error ) );
  dock.setSelectedPath( vectorDirectory.path() );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().packages.size() == 1, 30000 );
  QTRY_COMPARE_WITH_TIMEOUT( packageTree->topLevelItemCount(), 1, 30000 );
  QCOMPARE( packageTree->topLevelItem( 0 )->text( 4 ), QStringLiteral( "不可读取" ) );
  QCOMPARE( packageTree->topLevelItem( 0 )->checkState( 0 ), Qt::Unchecked );
  QVERIFY( !( packageTree->topLevelItem( 0 )->flags() & Qt::ItemIsUserCheckable ) );
  QCOMPARE( loadButton->text(), QStringLiteral( "当前 PTP 数据集不可加载" ) );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "VTP" ) ) );

  QTemporaryDir mixedDirectory;
  QVERIFY( mixedDirectory.isValid() );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              mixedDirectory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 256,
              QByteArrayLiteral( "{\"tile_file_ext\":\"pbf\"}" ), MTPL_STORAGE_PLAIN, {},
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, QgsMtplTest::minimalVectorTilePayload() } }, error ),
            qPrintable( error ) );
  const QString mixedVtpPath = mixedDirectory.filePath( QStringLiteral( "standalone.vtp" ) );
  QVERIFY2( QgsMtplTest::writeTileFixture(
              mixedVtpPath, QgsMtpl::PackageFormat::Vtp, MTPL_STORAGE_PLAIN, {}, error ),
            qPrintable( error ) );
  dock.setSelectedPath( mixedDirectory.path() );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().packages.size() == 2, 30000 );
  QTRY_COMPARE_WITH_TIMEOUT( loadButton->text(), QStringLiteral( "加载已选非 PTP 内容（PTP 已跳过）" ), 30000 );
  QVERIFY( loadButton->isEnabled() );
  QVERIFY( loadButton->toolTip().contains( QStringLiteral( "PTP" ) ) );

  QgsProject *project = QgsProject::instance();
  const QStringList layerIdsBeforeMixedLoad = project->mapLayers().keys();
  QSignalSpy mixedLoadedSpy( &dock, &QgsMtplDockWidget::layersLoaded );
  QSignalSpy mixedPartialSpy( &dock, &QgsMtplDockWidget::loadPartiallySucceeded );
  QSignalSpy mixedFailedSpy( &dock, &QgsMtplDockWidget::loadFailed );
  loadButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( !mixedLoadedSpy.isEmpty() || !mixedPartialSpy.isEmpty() || !mixedFailedSpy.isEmpty(), 30000 );
  QVERIFY2( mixedFailedSpy.isEmpty(),
            mixedFailedSpy.isEmpty() ? "" : qPrintable( mixedFailedSpy.constFirst().constFirst().toString() ) );
  QCOMPARE( mixedPartialSpy.count(), 0 );
  QCOMPARE( mixedLoadedSpy.count(), 1 );
  QList<QgsMapLayer *> mixedLayers;
  for ( QgsMapLayer *layer : project->mapLayers() )
  {
    if ( !layerIdsBeforeMixedLoad.contains( layer->id() ) )
      mixedLayers.append( layer );
  }
  QCOMPARE( mixedLayers.size(), 1 );
  QgsMtplPluginLayer *mixedLayer = qobject_cast<QgsMtplPluginLayer *>( mixedLayers.constFirst() );
  QVERIFY( mixedLayer );
  QVERIFY( !mixedLayer->isTileDataset() );
  QCOMPARE( mixedLayer->descriptor().format, QgsMtpl::PackageFormat::Vtp );
  project->removeMapLayer( mixedLayer->id() );

  QgsMtplCredentialStore::clear();
  QTemporaryDir encryptedDirectory;
  QVERIFY( encryptedDirectory.isValid() );
  const QgsMtpl::CryptoKeys encryptedKeys = QgsMtplTest::fixtureKeys();
  QVERIFY2( QgsMtplTest::writePtpFixture(
              encryptedDirectory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) ), 256, metadata,
              MTPL_STORAGE_ENCRYPTED, encryptedKeys, { { 0, 0, 0, 0, 0 } }, {}, error ),
            qPrintable( error ) );
  dock.setSelectedPath( encryptedDirectory.path() );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && dock.currentProbe().packages.size() == 1, 30000 );
  QTRY_COMPARE_WITH_TIMEOUT( packageTree->topLevelItemCount(), 1, 30000 );
  QCOMPARE( packageTree->topLevelItem( 0 )->text( 4 ), QStringLiteral( "需要密钥" ) );
  QCOMPARE( packageTree->topLevelItem( 0 )->checkState( 0 ), Qt::PartiallyChecked );
  QVERIFY( !( packageTree->topLevelItem( 0 )->flags() & Qt::ItemIsUserCheckable ) );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "数据集状态：需要解锁" ) ) );
  QCOMPARE( loadButton->text(), QStringLiteral( "请先解锁 PTP 数据集" ) );
  QVERIFY( !loadButton->isEnabled() );

  QTemporaryDir rootDirectory;
  QVERIFY( rootDirectory.isValid() );
  QVERIFY( QDir( rootDirectory.path() ).mkpath( QStringLiteral( "leaf" ) ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              rootDirectory.filePath( QStringLiteral( "leaf/0-7-0-0-0.ptp" ) ), 256, metadata,
              MTPL_STORAGE_PLAIN, {}, { { 0, 0, 0, 0, 0 } }, {}, error ),
            qPrintable( error ) );
  dock.setSelectedPath( rootDirectory.path() );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().isDirectorySelection &&
                            !dock.currentProbe().ok && dock.currentProbe().packages.isEmpty(), 30000 );
  QTRY_VERIFY_WITH_TIMEOUT( datasetSummary->text().contains( QStringLiteral( "直接存放 PTP 文件的子文件夹" ) ), 30000 );
  QLabel *probeSummary = dock.findChild<QLabel *>( QStringLiteral( "mtplSummaryLabel" ) );
  QVERIFY( probeSummary );
  QVERIFY( probeSummary->text().contains( QStringLiteral( "不会递归" ) ) );
  QVERIFY( !loadButton->isEnabled() );

  dock.cancelPendingOperations();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
}

void TestMtplWidget::dockDisablesLoadForUnsavedRuleDraft()
{
  QMainWindow window;
  QgsMtplDockWidget dock( &window );
  window.addDockWidget( Qt::RightDockWidgetArea, &dock );

  QgsMtplPartitionRuleWidget *ruleWidget = dock.findChild<QgsMtplPartitionRuleWidget *>( QStringLiteral( "mtplPartitionRuleWidget" ) );
  QLabel *datasetSummary = dock.findChild<QLabel *>( QStringLiteral( "mtplTileDatasetSummaryLabel" ) );
  QPushButton *loadButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplLoadButton" ) );
  QPushButton *copyButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleCopyButton" ) );
  QPushButton *saveButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleSaveButton" ) );
  QPushButton *deleteButton = dock.findChild<QPushButton *>( QStringLiteral( "mtplPartitionRuleDeleteButton" ) );
  QTableWidget *bandTable = dock.findChild<QTableWidget *>( QStringLiteral( "mtplPartitionRuleBandTable" ) );
  QVERIFY( ruleWidget );
  QVERIFY( datasetSummary );
  QVERIFY( loadButton );
  QVERIFY( copyButton );
  QVERIFY( saveButton );
  QVERIFY( deleteButton );
  QVERIFY( bandTable );

  dock.setSelectedPath( mPortablePtpPath );
  QTRY_VERIFY_WITH_TIMEOUT( dock.currentProbe().ok && loadButton->isEnabled(), 30000 );
  QCOMPARE( ruleWidget->currentRuleId(), QgsMtpl::PartitionRuleStore::ruleOneId() );

  copyButton->click();
  QVERIFY( ruleWidget->currentRuleIsDirty() );
  QVERIFY( !loadButton->isEnabled() );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "未保存修改" ) ) );

  saveButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( !ruleWidget->currentRuleIsDirty() && loadButton->isEnabled(), 30000 );

  QSpinBox *firstMinimum = bandSpinBox( bandTable, 0, 0 );
  QVERIFY( firstMinimum );
  firstMinimum->setValue( 8 );
  QVERIFY( ruleWidget->currentRuleIsDirty() );
  QVERIFY( !saveButton->isEnabled() );
  QVERIFY( !loadButton->isEnabled() );
  QVERIFY( datasetSummary->text().contains( QStringLiteral( "未保存修改" ) ) );

  firstMinimum->setValue( 0 );
  QVERIFY( saveButton->isEnabled() );
  saveButton->click();
  QTRY_VERIFY_WITH_TIMEOUT( !ruleWidget->currentRuleIsDirty() && loadButton->isEnabled(), 30000 );

  deleteButton->click();
  ruleWidget->setCurrentRuleId( QgsMtpl::PartitionRuleStore::ruleOneId() );
  QTRY_VERIFY_WITH_TIMEOUT( loadButton->isEnabled(), 30000 );
  dock.cancelPendingOperations();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
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
