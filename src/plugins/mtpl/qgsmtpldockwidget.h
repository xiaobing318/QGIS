/***************************************************************************
  qgsmtpldockwidget.h
  -------------------
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

#ifndef QGSMTPLDOCKWIDGET_H
#define QGSMTPLDOCKWIDGET_H

#include "qgsdockwidget.h"
#include "qgsmtplpackage.h"
#include "services/qgsmtpltileset.h"
#include "qgis.h"

#include <QPointer>
#include <QSet>
#include <QStringList>
#include <memory>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QHideEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QPlainTextEdit;
class QResizeEvent;
class QScrollArea;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QToolButton;
class QTemporaryDir;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
class QgsMapLayer;
class QgsMtplPathWidget;
class QgsMtplPartitionRuleWidget;
class QgsMtplPackageToolsWidget;
class QgsMtplPluginLayer;
class QgsMtplProbeTask;
class QgsTask;

class QgsMtplDockWidget final : public QgsDockWidget
{
    Q_OBJECT

  public:
    explicit QgsMtplDockWidget( QWidget *parent = nullptr );
    ~QgsMtplDockWidget() override;

    QString selectedPath() const;
    QgsMtpl::ProbeResult currentProbe() const;
    void setSelectedPath( const QString &path );
    void cancelPendingOperations();

  signals:
    void layersLoaded( int count );
    void loadFailed( const QString &message );
    void loadPartiallySucceeded( int loadedCount, int failedItemCount, const QStringList &messages );
    void messageRequested( const QString &title, const QString &message, Qgis::MessageLevel level );
    void existingLayerActivated( const QString &layerId );

  protected:
    void resizeEvent( QResizeEvent *event ) override;
    void hideEvent( QHideEvent *event ) override;

  private slots:
    void scheduleProbe();
    void probeSelectedPath();
    void loadToProject();
    void refreshExistingDataset();
    void applyRememberedKeys();
    void clearRememberedKeys();
    void importVectorStyle();
    void showSelectedPackageMetadata();
    void applyOverrides();
    void openSelectedSfpEntry();
    void updateLoadButtonState();

  private:
    enum class MtplTab
    {
      Packages = 0,
      Details = 1,
      Tools = 2
    };

    enum class ProbePurpose
    {
      Selection,
      ValidateKeys,
      RefreshDataset
    };

    enum class LoadMode
    {
      Add,
      Refresh
    };
    struct SfpPopulationState;

    void startProbe( const QString &path,
                     const QgsMtpl::CryptoKeys &keys,
                     QgsMtpl::CredentialSource source,
                     ProbePurpose purpose,
                     bool rememberAfterValidation = false );
    void probeFinished( QgsMtplProbeTask *task, quint64 generation, ProbePurpose purpose );
    void cancelProbeWork( bool waitForTasks, const QString &summary = QString() );
    void cancelIoWork( bool waitForTasks );
    void cancelPreviewWork( bool showCanceled = false );
    void cancelCurrentProbe();
    void clearProbe( const QString &summary = QString() );
    void applyProbe( const QgsMtpl::ProbeResult &probe );
    void rebuildDetails();
    void populateSfpEntriesBatch();
    void finishSfpPopulation();
    void updateBatchSummary();
    void updateKeyStatus();
    void rebuildTileDataset( bool autoMatchRule = false );
    void updateTileDatasetSummary();
    void savePartitionRule( const QgsMtpl::PartitionRule &rule );
    void deletePartitionRule( const QString &ruleId );
    void updateImportStyleState();
    void populateOverrides();
    int currentPackageIndex() const;
    int maximumPackageNameColumnWidth() const;
    void updatePackageColumnWidths();
    void updateMetadataTableHeight();
    void updatePreviewTextHeight();
    void updateDetailsPageState();
    void currentTabChanged( int index );
    void packageToolOutputsCommitted( const QStringList &paths );
    void addCurrentSfpEntry( QTreeWidgetItem *packageItem,
                             const QgsMtpl::PackageDescriptor &descriptor,
                             const QgsMtpl::SfpEntryDescriptor &entry );
    void startSfpPreview( const QgsMtpl::PackageDescriptor &descriptor, const QString &entryPath );
    void previewFinished( QgsTask *task, quint64 generation, const QString &packagePath, const QString &entryPath );
    void startProjectLoad( LoadMode mode );
    void startLoadPreparationTask( QgsTask *task, bool singleSfpEntry, LoadMode mode = LoadMode::Add );
    void loadPreparationFinished( QgsTask *task, quint64 generation, bool singleSfpEntry, LoadMode mode );
    QgsMtplPluginLayer *existingTileDatasetLayer( const QString &sourcePath ) const;
    bool ensureSfpCache( QString &cacheRoot, QString &error );
    bool loadPreparedSfpEntry( const QString &entryPath,
                               const QString &extractedPath,
                               const QString &cacheDirectory,
                               QList<QgsMapLayer *> &addedLayers,
                               QString &error );
    void cleanupSfpCache();
    QgsMtpl::CryptoKeys suppliedKeysForDescriptor( const QgsMtpl::PackageDescriptor &descriptor ) const;
    QgsMtplPathWidget *mPathWidget = nullptr;
    QgsMtplPartitionRuleWidget *mPartitionRuleWidget = nullptr;
    QTabWidget *mTabs = nullptr;
    QWidget *mPackagesTab = nullptr;
    QWidget *mDetailsTab = nullptr;
    QWidget *mToolsTab = nullptr;
    QStackedWidget *mDetailsStack = nullptr;
    QLabel *mDetailsEmptyLabel = nullptr;
    QLabel *mDetailsSelectionLabel = nullptr;
    QPushButton *mBackToPackagesButton = nullptr;
    QScrollArea *mDetailsScrollArea = nullptr;
    QgsMtplPackageToolsWidget *mPackageToolsWidget = nullptr;
    QLabel *mSummaryLabel = nullptr;
    QGroupBox *mTileDatasetSummaryGroup = nullptr;
    QLabel *mTileDatasetSummaryLabel = nullptr;
    QLabel *mPackagesLabel = nullptr;
    QLabel *mBatchSummaryLabel = nullptr;
    QPushButton *mLoadButton = nullptr;
    QPushButton *mRefreshDatasetButton = nullptr;
    QPushButton *mCancelProbeButton = nullptr;
    QWidget *mKeyPanel = nullptr;
    QLineEdit *mPrivateKeyEdit = nullptr;
    QLineEdit *mDeviceKeyEdit = nullptr;
    QCheckBox *mRememberKeysCheckBox = nullptr;
    QLabel *mKeyStatusLabel = nullptr;
    QLabel *mKeyHelpLabel = nullptr;
    QPushButton *mApplyKeyButton = nullptr;
    QPushButton *mClearKeyButton = nullptr;
    QLineEdit *mLayerNameEdit = nullptr;
    QToolButton *mImportStyleButton = nullptr;
    QWidget *mDetailsPanel = nullptr;
    QTreeWidget *mPackageTree = nullptr;
    QTableWidget *mMetadataTable = nullptr;
    QGroupBox *mOverridesGroup = nullptr;
    QLabel *mPayloadOverrideLabel = nullptr;
    QComboBox *mPayloadOverride = nullptr;
    QLineEdit *mCrsOverride = nullptr;
    QComboBox *mSchemeOverride = nullptr;
    QWidget *mDtpOverrides = nullptr;
    QComboBox *mDataTypeOverride = nullptr;
    QComboBox *mEndiannessOverride = nullptr;
    QDoubleSpinBox *mScaleOverride = nullptr;
    QDoubleSpinBox *mOffsetOverride = nullptr;
    QLineEdit *mNoDataOverride = nullptr;
    QPushButton *mApplyOverridesButton = nullptr;
    QPlainTextEdit *mPreviewText = nullptr;
    QLabel *mPreviewImage = nullptr;
    QPushButton *mOpenInQgisButton = nullptr;
    QTimer *mProbeTimer = nullptr;
    QTimer *mSfpPopulationTimer = nullptr;
    QTimer *mToolOutputRefreshTimer = nullptr;

    QgsMtpl::ProbeResult mProbe;
    QgsMtpl::TileDatasetBuildResult mTileDatasetBuildResult;
    QPointer<QgsMtplProbeTask> mProbeTask;
    QPointer<QgsTask> mPreviewTask;
    QPointer<QgsTask> mLoadTask;
    std::unique_ptr<SfpPopulationState> mSfpPopulation;
    quint64 mProbeGeneration = 0;
    quint64 mPreviewGeneration = 0;
    quint64 mLoadGeneration = 0;
    QString mFinalProbeSummary;
    QgsMtpl::CryptoKeys mPendingKeys;
    bool mPendingShouldRemember = false;
    QgsMtpl::CryptoKeys mKeys;
    QgsMtpl::CryptoKeys mRememberedKeys;
    QgsMtpl::CredentialSource mKeySource = QgsMtpl::CredentialSource::None;
    std::unique_ptr<QTemporaryDir> mSfpCache;
    QSet<QString> mSfpNativeLayerIds;
    bool mLastFailureNeedsKeys = false;
    bool mStoredKeyUnlockAttempted = false;
    bool mUpdatingRuleSelection = false;
    int mPreviousTabIndex = 0;
};

#endif // QGSMTPLDOCKWIDGET_H
