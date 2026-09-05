/***************************************************************************
  qgsmtplpartitionrulewidget.cpp
  ------------------------------
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

#include "qgsmtplpartitionrulewidget.h"

#include "qgsapplication.h"

#include <QAbstractItemView>
#include <QFormLayout>
#include <QFont>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace
{
  constexpr int RULE_ID_ROLE = Qt::UserRole;
  constexpr int MAXIMUM_SUPPORTED_ZOOM = 30;

  using QgsMtplPartitionRuleEditorBand = QgsMtpl::PartitionBand;
  using QgsMtplPartitionRuleEditorRule = QgsMtpl::PartitionRule;

  class PartitionBandsSummaryWidget final : public QWidget
  {
    public:
      explicit PartitionBandsSummaryWidget( QWidget *parent = nullptr )
        : QWidget( parent )
      {
        setMinimumHeight( 42 );
        setAccessibleName( tr( "分包区间图示" ) );
      }

      void setBands( const QList<QgsMtplPartitionRuleEditorBand> &bands )
      {
        mBands = bands;
        update();
      }

      QSize sizeHint() const override { return QSize( 320, 42 ); }

    protected:
      void paintEvent( QPaintEvent *event ) override
      {
        Q_UNUSED( event )

        QPainter painter( this );
        painter.setRenderHint( QPainter::Antialiasing, true );
        const QRectF barRect = QRectF( rect() ).adjusted( 2.5, 4.5, -2.5, -4.5 );
        painter.setPen( palette().color( QPalette::Mid ) );
        painter.setBrush( palette().color( QPalette::Base ) );
        painter.drawRoundedRect( barRect, 3, 3 );

        if ( mBands.isEmpty() )
        {
          painter.setPen( palette().color( QPalette::PlaceholderText ) );
          painter.drawText( barRect, Qt::AlignCenter, tr( "尚无区间" ) );
          return;
        }

        int maximumZoom = 0;
        for ( const QgsMtplPartitionRuleEditorBand &band : std::as_const( mBands ) )
          maximumZoom = std::max( maximumZoom, band.maxZoom );
        const int zoomCount = std::max( 1, maximumZoom + 1 );
        const QColor highlight = palette().color( QPalette::Highlight );
        for ( int index = 0; index < mBands.size(); ++index )
        {
          const QgsMtplPartitionRuleEditorBand &band = mBands.at( index );
          const double leftRatio = std::clamp( static_cast<double>( band.minZoom ) / zoomCount, 0.0, 1.0 );
          const double rightRatio = std::clamp( static_cast<double>( band.maxZoom + 1 ) / zoomCount, 0.0, 1.0 );
          const double left = barRect.left() + leftRatio * barRect.width();
          const double right = barRect.left() + rightRatio * barRect.width();
          QRectF segment( left, barRect.top(), std::max( 1.0, right - left ), barRect.height() );

          QColor fill = highlight.lighter( 100 + index % 3 * 18 );
          fill.setAlpha( 165 );
          painter.setPen( palette().color( QPalette::Mid ) );
          painter.setBrush( fill );
          painter.drawRect( segment );

          if ( segment.width() >= 54 )
          {
            painter.setPen( palette().color( QPalette::HighlightedText ) );
            painter.drawText( segment.adjusted( 3, 0, -3, 0 ), Qt::AlignCenter, tr( "z%1–%2 · b%3" ).arg( band.minZoom ).arg( band.maxZoom ).arg( band.baseZoom ) );
          }
        }
      }

    private:
      QList<QgsMtplPartitionRuleEditorBand> mBands;
  };

  QSpinBox *createZoomSpinBox( QWidget *parent, const QString &objectName, int value )
  {
    auto *spinBox = new QSpinBox( parent );
    spinBox->setObjectName( objectName );
    spinBox->setRange( 0, MAXIMUM_SUPPORTED_ZOOM );
    spinBox->setValue( value );
    spinBox->setAlignment( Qt::AlignCenter );
    spinBox->setFrame( false );
    return spinBox;
  }
} // namespace

QgsMtplPartitionRuleWidget::QgsMtplPartitionRuleWidget( QWidget *parent )
  : QWidget( parent )
{
  setObjectName( QStringLiteral( "mtplPartitionRuleWidget" ) );

  auto *rootLayout = new QVBoxLayout( this );
  rootLayout->setContentsMargins( 0, 0, 0, 0 );

  auto *splitter = new QSplitter( Qt::Horizontal, this );
  splitter->setObjectName( QStringLiteral( "mtplPartitionRuleSplitter" ) );
  splitter->setChildrenCollapsible( false );
  rootLayout->addWidget( splitter );

  auto *libraryPanel = new QWidget( splitter );
  libraryPanel->setObjectName( QStringLiteral( "mtplPartitionRuleLibraryPanel" ) );
  libraryPanel->setMinimumWidth( 180 );
  auto *libraryLayout = new QVBoxLayout( libraryPanel );
  libraryLayout->setContentsMargins( 0, 0, 6, 0 );
  libraryLayout->setSpacing( 6 );

  auto *libraryHeading = new QLabel( tr( "规则库" ), libraryPanel );
  libraryHeading->setObjectName( QStringLiteral( "mtplPartitionRuleLibraryLabel" ) );
  QFont headingFont = libraryHeading->font();
  headingFont.setBold( true );
  libraryHeading->setFont( headingFont );
  libraryLayout->addWidget( libraryHeading );

  mRuleList = new QListWidget( libraryPanel );
  mRuleList->setObjectName( QStringLiteral( "mtplPartitionRuleList" ) );
  mRuleList->setSelectionMode( QAbstractItemView::SingleSelection );
  mRuleList->setAlternatingRowColors( true );
  mRuleList->setAccessibleName( tr( "分包规则库" ) );
  libraryLayout->addWidget( mRuleList, 1 );

  auto *libraryActions = new QHBoxLayout();
  libraryActions->setContentsMargins( 0, 0, 0, 0 );
  libraryActions->setSpacing( 4 );
  mAddRuleButton = new QPushButton( QgsApplication::getThemeIcon( QStringLiteral( "/mActionAdd.svg" ) ), tr( "添加" ), libraryPanel );
  mAddRuleButton->setObjectName( QStringLiteral( "mtplPartitionRuleAddButton" ) );
  mCopyRuleButton = new QPushButton( QgsApplication::getThemeIcon( QStringLiteral( "/mActionDuplicateLayer.svg" ) ), tr( "复制" ), libraryPanel );
  mCopyRuleButton->setObjectName( QStringLiteral( "mtplPartitionRuleCopyButton" ) );
  mDeleteRuleButton = new QPushButton( QgsApplication::getThemeIcon( QStringLiteral( "/mActionDeleteSelected.svg" ) ), tr( "删除" ), libraryPanel );
  mDeleteRuleButton->setObjectName( QStringLiteral( "mtplPartitionRuleDeleteButton" ) );
  libraryActions->addWidget( mAddRuleButton );
  libraryActions->addWidget( mCopyRuleButton );
  libraryActions->addWidget( mDeleteRuleButton );
  libraryLayout->addLayout( libraryActions );

  mDetailsStack = new QStackedWidget( splitter );
  mDetailsStack->setObjectName( QStringLiteral( "mtplPartitionRuleDetailsStack" ) );

  auto *emptyPanel = new QWidget( mDetailsStack );
  auto *emptyLayout = new QVBoxLayout( emptyPanel );
  auto *emptyLabel = new QLabel( tr( "请选择一条规则，或添加自定义规则。" ), emptyPanel );
  emptyLabel->setObjectName( QStringLiteral( "mtplPartitionRuleEmptyLabel" ) );
  emptyLabel->setAlignment( Qt::AlignCenter );
  emptyLabel->setWordWrap( true );
  emptyLayout->addStretch();
  emptyLayout->addWidget( emptyLabel );
  emptyLayout->addStretch();
  mDetailsStack->addWidget( emptyPanel );

  auto *detailsPanel = new QWidget( mDetailsStack );
  detailsPanel->setObjectName( QStringLiteral( "mtplPartitionRuleDetailsPanel" ) );
  auto *detailsLayout = new QVBoxLayout( detailsPanel );
  detailsLayout->setContentsMargins( 6, 0, 0, 0 );
  detailsLayout->setSpacing( 8 );

  auto *detailsHeading = new QLabel( tr( "规则详情" ), detailsPanel );
  detailsHeading->setObjectName( QStringLiteral( "mtplPartitionRuleDetailsLabel" ) );
  detailsHeading->setFont( headingFont );
  detailsLayout->addWidget( detailsHeading );

  auto *ruleForm = new QFormLayout();
  ruleForm->setFieldGrowthPolicy( QFormLayout::AllNonFixedFieldsGrow );
  ruleForm->setRowWrapPolicy( QFormLayout::WrapLongRows );
  mNameEdit = new QLineEdit( detailsPanel );
  mNameEdit->setObjectName( QStringLiteral( "mtplPartitionRuleNameEdit" ) );
  mNameEdit->setClearButtonEnabled( true );
  mNameEdit->setAccessibleName( tr( "分包规则名称" ) );
  ruleForm->addRow( tr( "规则名" ), mNameEdit );
  mRuleTypeLabel = new QLabel( detailsPanel );
  mRuleTypeLabel->setObjectName( QStringLiteral( "mtplPartitionRuleTypeLabel" ) );
  mRuleTypeLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
  ruleForm->addRow( tr( "类型" ), mRuleTypeLabel );
  detailsLayout->addLayout( ruleForm );

  auto *summaryGroup = new QGroupBox( tr( "区间摘要" ), detailsPanel );
  summaryGroup->setObjectName( QStringLiteral( "mtplPartitionRuleSummaryGroup" ) );
  auto *summaryLayout = new QVBoxLayout( summaryGroup );
  mSummaryBar = new PartitionBandsSummaryWidget( summaryGroup );
  mSummaryBar->setObjectName( QStringLiteral( "mtplPartitionRuleSummaryBar" ) );
  summaryLayout->addWidget( mSummaryBar );
  mSummaryLabel = new QLabel( summaryGroup );
  mSummaryLabel->setObjectName( QStringLiteral( "mtplPartitionRuleSummaryLabel" ) );
  mSummaryLabel->setWordWrap( true );
  mSummaryLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
  summaryLayout->addWidget( mSummaryLabel );
  detailsLayout->addWidget( summaryGroup );

  auto *bandsGroup = new QGroupBox( tr( "缩放区间" ), detailsPanel );
  bandsGroup->setObjectName( QStringLiteral( "mtplPartitionRuleBandsGroup" ) );
  auto *bandsLayout = new QVBoxLayout( bandsGroup );
  bandsLayout->setSpacing( 6 );
  mBandTable = new QTableWidget( bandsGroup );
  mBandTable->setObjectName( QStringLiteral( "mtplPartitionRuleBandTable" ) );
  mBandTable->setColumnCount( 3 );
  mBandTable->setHorizontalHeaderLabels( { tr( "最小级别" ), tr( "最大级别" ), tr( "基础级别" ) } );
  mBandTable->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
  mBandTable->verticalHeader()->setDefaultSectionSize( 28 );
  mBandTable->setSelectionBehavior( QAbstractItemView::SelectRows );
  mBandTable->setSelectionMode( QAbstractItemView::SingleSelection );
  mBandTable->setAlternatingRowColors( true );
  mBandTable->setMinimumHeight( 150 );
  mBandTable->setAccessibleName( tr( "分包规则缩放区间" ) );
  bandsLayout->addWidget( mBandTable, 1 );

  auto *bandActions = new QHBoxLayout();
  bandActions->setContentsMargins( 0, 0, 0, 0 );
  bandActions->setSpacing( 4 );
  mAddBandButton = new QPushButton( QgsApplication::getThemeIcon( QStringLiteral( "/mActionAdd.svg" ) ), tr( "添加区间" ), bandsGroup );
  mAddBandButton->setObjectName( QStringLiteral( "mtplPartitionRuleBandAddButton" ) );
  mDeleteBandButton = new QPushButton( QgsApplication::getThemeIcon( QStringLiteral( "/mActionDeleteSelected.svg" ) ), tr( "删除区间" ), bandsGroup );
  mDeleteBandButton->setObjectName( QStringLiteral( "mtplPartitionRuleBandDeleteButton" ) );
  mMoveBandUpButton = new QPushButton( QgsApplication::getThemeIcon( QStringLiteral( "/mActionArrowUp.svg" ) ), tr( "上移" ), bandsGroup );
  mMoveBandUpButton->setObjectName( QStringLiteral( "mtplPartitionRuleBandUpButton" ) );
  mMoveBandDownButton = new QPushButton( QgsApplication::getThemeIcon( QStringLiteral( "/mActionArrowDown.svg" ) ), tr( "下移" ), bandsGroup );
  mMoveBandDownButton->setObjectName( QStringLiteral( "mtplPartitionRuleBandDownButton" ) );
  bandActions->addWidget( mAddBandButton );
  bandActions->addWidget( mDeleteBandButton );
  bandActions->addStretch();
  bandActions->addWidget( mMoveBandUpButton );
  bandActions->addWidget( mMoveBandDownButton );
  bandsLayout->addLayout( bandActions );
  detailsLayout->addWidget( bandsGroup, 1 );

  mValidationLabel = new QLabel( detailsPanel );
  mValidationLabel->setObjectName( QStringLiteral( "mtplPartitionRuleValidationLabel" ) );
  mValidationLabel->setWordWrap( true );
  mValidationLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
  mValidationLabel->setAccessibleName( tr( "分包规则校验结果" ) );
  detailsLayout->addWidget( mValidationLabel );

  auto *saveActions = new QHBoxLayout();
  saveActions->setContentsMargins( 0, 0, 0, 0 );
  saveActions->addStretch();
  mSaveAsButton = new QPushButton( QgsApplication::getThemeIcon( QStringLiteral( "/mActionFileSaveAs.svg" ) ), tr( "另存为" ), detailsPanel );
  mSaveAsButton->setObjectName( QStringLiteral( "mtplPartitionRuleSaveAsButton" ) );
  mSaveButton = new QPushButton( QgsApplication::getThemeIcon( QStringLiteral( "/mActionSaveEdits.svg" ) ), tr( "保存" ), detailsPanel );
  mSaveButton->setObjectName( QStringLiteral( "mtplPartitionRuleSaveButton" ) );
  mSaveButton->setDefault( true );
  saveActions->addWidget( mSaveAsButton );
  saveActions->addWidget( mSaveButton );
  detailsLayout->addLayout( saveActions );
  mDetailsStack->addWidget( detailsPanel );

  splitter->setStretchFactor( 0, 0 );
  splitter->setStretchFactor( 1, 1 );
  splitter->setSizes( { 190, 410 } );

  connect( mRuleList, &QListWidget::currentRowChanged, this, &QgsMtplPartitionRuleWidget::ruleSelectionChanged );
  connect( mAddRuleButton, &QPushButton::clicked, this, &QgsMtplPartitionRuleWidget::addRule );
  connect( mCopyRuleButton, &QPushButton::clicked, this, &QgsMtplPartitionRuleWidget::copyRule );
  connect( mDeleteRuleButton, &QPushButton::clicked, this, &QgsMtplPartitionRuleWidget::deleteRule );
  connect( mNameEdit, &QLineEdit::textChanged, this, &QgsMtplPartitionRuleWidget::editorChanged );
  connect( mBandTable, &QTableWidget::currentCellChanged, this, &QgsMtplPartitionRuleWidget::bandSelectionChanged );
  connect( mAddBandButton, &QPushButton::clicked, this, &QgsMtplPartitionRuleWidget::addBand );
  connect( mDeleteBandButton, &QPushButton::clicked, this, &QgsMtplPartitionRuleWidget::deleteBand );
  connect( mMoveBandUpButton, &QPushButton::clicked, this, &QgsMtplPartitionRuleWidget::moveBandUp );
  connect( mMoveBandDownButton, &QPushButton::clicked, this, &QgsMtplPartitionRuleWidget::moveBandDown );
  connect( mSaveButton, &QPushButton::clicked, this, &QgsMtplPartitionRuleWidget::saveRule );
  connect( mSaveAsButton, &QPushButton::clicked, this, &QgsMtplPartitionRuleWidget::saveRuleAs );

  clearEditor();
}

QList<QgsMtplPartitionRuleEditorRule> QgsMtplPartitionRuleWidget::rules() const
{
  return mRules;
}

QgsMtplPartitionRuleEditorRule QgsMtplPartitionRuleWidget::currentRule() const
{
  return mCurrentRuleIndex >= 0 && mCurrentRuleIndex < mRules.size() ? mRules.at( mCurrentRuleIndex ) : QgsMtplPartitionRuleEditorRule();
}

QString QgsMtplPartitionRuleWidget::currentRuleId() const
{
  return currentRule().id;
}

bool QgsMtplPartitionRuleWidget::currentRuleIsDirty() const
{
  return mDirtyRuleIds.contains( currentRuleId() );
}

void QgsMtplPartitionRuleWidget::setRules( const QList<QgsMtplPartitionRuleEditorRule> &rules, const QString &currentRuleId )
{
  QString preferredRuleId = currentRuleId;
  if ( preferredRuleId.isEmpty() && mCurrentRuleIndex >= 0 && mCurrentRuleIndex < mRules.size() )
    preferredRuleId = mRules.at( mCurrentRuleIndex ).id;

  mRules = rules;
  mDirtyRuleIds.clear();
  mRecentlyDeletedRules.clear();
  mRecentlyDeletedRuleIndexes.clear();
  mRecentlyDeletedDirtyRuleIds.clear();

  QSet<QString> usedIds;
  for ( QgsMtplPartitionRuleEditorRule &rule : mRules )
  {
    if ( rule.id.isEmpty() || usedIds.contains( rule.id ) )
      rule.id = newRuleId();
    usedIds.insert( rule.id );
  }

  populateRuleList( preferredRuleId );
}

void QgsMtplPartitionRuleWidget::setCurrentRuleId( const QString &ruleId )
{
  const int ruleIndex = ruleIndexForId( ruleId );
  if ( ruleIndex >= 0 )
    mRuleList->setCurrentRow( ruleIndex );
}

void QgsMtplPartitionRuleWidget::ruleSaveFailed( const QString &ruleId )
{
  const int ruleIndex = ruleIndexForId( ruleId );
  if ( ruleIndex < 0 )
    return;
  mDirtyRuleIds.insert( ruleId );
  updateRuleListItem( ruleIndex );
  if ( ruleIndex == mCurrentRuleIndex )
    updateSummaryAndActions();
}

void QgsMtplPartitionRuleWidget::ruleDeleteSucceeded( const QString &ruleId )
{
  mRecentlyDeletedRules.remove( ruleId );
  mRecentlyDeletedRuleIndexes.remove( ruleId );
  mRecentlyDeletedDirtyRuleIds.remove( ruleId );
}

void QgsMtplPartitionRuleWidget::ruleDeleteFailed( const QString &ruleId )
{
  const auto deleted = mRecentlyDeletedRules.constFind( ruleId );
  if ( deleted == mRecentlyDeletedRules.constEnd() )
    return;
  const int restoredIndex = std::clamp( mRecentlyDeletedRuleIndexes.value( ruleId, mRules.size() ),
                                        0, static_cast<int>( mRules.size() ) );
  mRules.insert( restoredIndex, deleted.value() );
  if ( mRecentlyDeletedDirtyRuleIds.contains( ruleId ) )
    mDirtyRuleIds.insert( ruleId );
  mRecentlyDeletedRules.remove( ruleId );
  mRecentlyDeletedRuleIndexes.remove( ruleId );
  mRecentlyDeletedDirtyRuleIds.remove( ruleId );
  populateRuleList( ruleId );
}

void QgsMtplPartitionRuleWidget::addRule()
{
  QgsMtplPartitionRuleEditorRule rule;
  rule.id = newRuleId();
  rule.name = uniqueRuleName( tr( "新建规则" ) );
  rule.bands.append( QgsMtplPartitionRuleEditorBand { 0, 0, 0 } );
  mRules.append( rule );
  mDirtyRuleIds.insert( rule.id );
  populateRuleList( rule.id );
  mNameEdit->setFocus();
  mNameEdit->selectAll();
}

void QgsMtplPartitionRuleWidget::copyRule()
{
  if ( mCurrentRuleIndex < 0 || mCurrentRuleIndex >= mRules.size() )
    return;

  QgsMtplPartitionRuleEditorRule copy = mRules.at( mCurrentRuleIndex );
  copy.id = newRuleId();
  copy.name = uniqueRuleName( tr( "%1 副本" ).arg( copy.name ) );
  copy.builtIn = false;
  mRules.append( copy );
  mDirtyRuleIds.insert( copy.id );
  populateRuleList( copy.id );
  mNameEdit->setFocus();
  mNameEdit->selectAll();
}

void QgsMtplPartitionRuleWidget::deleteRule()
{
  if ( !currentRuleIsEditable() )
    return;

  const int removedIndex = mCurrentRuleIndex;
  const QString removedId = mRules.at( removedIndex ).id;
  mRecentlyDeletedRules.insert( removedId, mRules.at( removedIndex ) );
  mRecentlyDeletedRuleIndexes.insert( removedId, removedIndex );
  if ( mDirtyRuleIds.contains( removedId ) )
    mRecentlyDeletedDirtyRuleIds.insert( removedId );
  mRules.removeAt( removedIndex );
  mDirtyRuleIds.remove( removedId );

  const int nextIndex = std::min( removedIndex, static_cast<int>( mRules.size() ) - 1 );
  const QString nextId = nextIndex >= 0 ? mRules.at( nextIndex ).id : QString();
  populateRuleList( nextId );
  emit ruleDeleteRequested( removedId );
}

void QgsMtplPartitionRuleWidget::saveRule()
{
  if ( !currentRuleIsEditable() || mCurrentRuleIndex >= mRules.size() )
    return;

  const QgsMtplPartitionRuleEditorRule rule = mRules.at( mCurrentRuleIndex );
  if ( !validationError( rule ).isEmpty() )
    return;

  mDirtyRuleIds.remove( rule.id );
  updateRuleListItem( mCurrentRuleIndex );
  updateSummaryAndActions();
  emit ruleSaveRequested( rule );
}

void QgsMtplPartitionRuleWidget::saveRuleAs()
{
  if ( mCurrentRuleIndex < 0 || mCurrentRuleIndex >= mRules.size() )
    return;

  QgsMtplPartitionRuleEditorRule copy = mRules.at( mCurrentRuleIndex );
  if ( !validationError( copy ).isEmpty() )
    return;

  copy.id = newRuleId();
  copy.name = uniqueRuleName( tr( "%1 副本" ).arg( copy.name ) );
  copy.builtIn = false;
  mRules.append( copy );

  // Populate the new rule as a draft first so the selection change cannot
  // temporarily make an unpersisted rule available to dataset loading.
  mDirtyRuleIds.insert( copy.id );
  populateRuleList( copy.id );
  mDirtyRuleIds.remove( copy.id );
  updateRuleListItem( mCurrentRuleIndex );
  updateSummaryAndActions();
  emit ruleSaveRequested( copy );
}

void QgsMtplPartitionRuleWidget::addBand()
{
  if ( !currentRuleIsEditable() )
    return;

  QgsMtplPartitionRuleEditorRule &rule = mRules[mCurrentRuleIndex];
  QgsMtplPartitionRuleEditorBand band { 0, 0, 0 };
  if ( !rule.bands.isEmpty() )
  {
    const QgsMtplPartitionRuleEditorBand &last = rule.bands.constLast();
    band.minZoom = std::min( last.maxZoom + 1, MAXIMUM_SUPPORTED_ZOOM );
    band.maxZoom = band.minZoom;
    band.baseZoom = band.minZoom;
  }
  rule.bands.append( band );
  markCurrentRuleDirty();
  populateBandTable( rule.bands );
  mBandTable->setCurrentCell( rule.bands.size() - 1, 0 );
  updateSummaryAndActions();
  emit ruleDraftChanged( rule );
}

void QgsMtplPartitionRuleWidget::deleteBand()
{
  if ( !currentRuleIsEditable() )
    return;
  const int row = mBandTable->currentRow();
  if ( row < 0 || row >= mRules[mCurrentRuleIndex].bands.size() )
    return;

  QgsMtplPartitionRuleEditorRule &rule = mRules[mCurrentRuleIndex];
  rule.bands.removeAt( row );
  markCurrentRuleDirty();
  populateBandTable( rule.bands );
  if ( !rule.bands.isEmpty() )
    mBandTable->setCurrentCell( std::min( row, static_cast<int>( rule.bands.size() ) - 1 ), 0 );
  updateSummaryAndActions();
  emit ruleDraftChanged( rule );
}

void QgsMtplPartitionRuleWidget::moveBandUp()
{
  if ( !currentRuleIsEditable() )
    return;
  const int row = mBandTable->currentRow();
  if ( row <= 0 || row >= mRules[mCurrentRuleIndex].bands.size() )
    return;

  QgsMtplPartitionRuleEditorRule &rule = mRules[mCurrentRuleIndex];
  rule.bands.swapItemsAt( row, row - 1 );
  markCurrentRuleDirty();
  populateBandTable( rule.bands );
  mBandTable->setCurrentCell( row - 1, 0 );
  updateSummaryAndActions();
  emit ruleDraftChanged( rule );
}

void QgsMtplPartitionRuleWidget::moveBandDown()
{
  if ( !currentRuleIsEditable() )
    return;
  const int row = mBandTable->currentRow();
  if ( row < 0 || row + 1 >= mRules[mCurrentRuleIndex].bands.size() )
    return;

  QgsMtplPartitionRuleEditorRule &rule = mRules[mCurrentRuleIndex];
  rule.bands.swapItemsAt( row, row + 1 );
  markCurrentRuleDirty();
  populateBandTable( rule.bands );
  mBandTable->setCurrentCell( row + 1, 0 );
  updateSummaryAndActions();
  emit ruleDraftChanged( rule );
}

void QgsMtplPartitionRuleWidget::ruleSelectionChanged()
{
  mCurrentRuleIndex = mRuleList->currentRow();
  populateEditor();
  emit currentRuleChanged( currentRuleId() );
}

void QgsMtplPartitionRuleWidget::editorChanged()
{
  if ( mPopulating || !currentRuleIsEditable() )
    return;

  mRules[mCurrentRuleIndex] = ruleFromEditor();
  markCurrentRuleDirty();
  updateRuleListItem( mCurrentRuleIndex );
  updateSummaryAndActions();
  emit ruleDraftChanged( mRules.at( mCurrentRuleIndex ) );
}

void QgsMtplPartitionRuleWidget::bandSelectionChanged()
{
  updateBandActions();
}

void QgsMtplPartitionRuleWidget::populateRuleList( const QString &preferredRuleId )
{
  QString selectedId = preferredRuleId;
  if ( selectedId.isEmpty() && mCurrentRuleIndex >= 0 && mCurrentRuleIndex < mRules.size() )
    selectedId = mRules.at( mCurrentRuleIndex ).id;

  {
    const QSignalBlocker blocker( mRuleList );
    mRuleList->clear();
    for ( int i = 0; i < mRules.size(); ++i )
    {
      auto *item = new QListWidgetItem( mRuleList );
      item->setData( RULE_ID_ROLE, mRules.at( i ).id );
      updateRuleListItem( i );
    }

    int selectedIndex = ruleIndexForId( selectedId );
    if ( selectedIndex < 0 && !mRules.isEmpty() )
      selectedIndex = 0;
    mRuleList->setCurrentRow( selectedIndex );
  }

  mCurrentRuleIndex = mRuleList->currentRow();
  populateEditor();
  emit currentRuleChanged( currentRuleId() );
}

void QgsMtplPartitionRuleWidget::populateEditor()
{
  if ( mCurrentRuleIndex < 0 || mCurrentRuleIndex >= mRules.size() )
  {
    clearEditor();
    return;
  }

  mPopulating = true;
  const QgsMtplPartitionRuleEditorRule &rule = mRules.at( mCurrentRuleIndex );
  mDetailsStack->setCurrentIndex( 1 );
  mNameEdit->setText( rule.name );
  mRuleTypeLabel->setText( rule.builtIn ? tr( "内置规则，只读" ) : tr( "自定义规则" ) );
  populateBandTable( rule.bands );
  if ( !rule.bands.isEmpty() )
    mBandTable->setCurrentCell( 0, 0 );
  mPopulating = false;
  updateSummaryAndActions();
}

void QgsMtplPartitionRuleWidget::populateBandTable( const QList<QgsMtplPartitionRuleEditorBand> &bands )
{
  const bool previousPopulating = mPopulating;
  mPopulating = true;
  mBandTable->clearContents();
  mBandTable->setRowCount( bands.size() );

  for ( int row = 0; row < bands.size(); ++row )
  {
    const QgsMtplPartitionRuleEditorBand &band = bands.at( row );
    auto *minimumSpin = createZoomSpinBox( mBandTable, QStringLiteral( "mtplPartitionRuleBandMinimumSpinBox" ), band.minZoom );
    auto *maximumSpin = createZoomSpinBox( mBandTable, QStringLiteral( "mtplPartitionRuleBandMaximumSpinBox" ), band.maxZoom );
    auto *baseSpin = createZoomSpinBox( mBandTable, QStringLiteral( "mtplPartitionRuleBandBaseSpinBox" ), band.baseZoom );
    minimumSpin->setProperty( "bandRow", row );
    maximumSpin->setProperty( "bandRow", row );
    baseSpin->setProperty( "bandRow", row );
    mBandTable->setCellWidget( row, 0, minimumSpin );
    mBandTable->setCellWidget( row, 1, maximumSpin );
    mBandTable->setCellWidget( row, 2, baseSpin );
    connect( minimumSpin, qOverload<int>( &QSpinBox::valueChanged ), this, [this, row]( int ) {
      mBandTable->setCurrentCell( row, 0 );
      editorChanged();
    } );
    connect( maximumSpin, qOverload<int>( &QSpinBox::valueChanged ), this, [this, row]( int ) {
      mBandTable->setCurrentCell( row, 1 );
      editorChanged();
    } );
    connect( baseSpin, qOverload<int>( &QSpinBox::valueChanged ), this, [this, row]( int ) {
      mBandTable->setCurrentCell( row, 2 );
      editorChanged();
    } );
  }
  mPopulating = previousPopulating;
}

void QgsMtplPartitionRuleWidget::clearEditor()
{
  mCurrentRuleIndex = -1;
  mPopulating = true;
  mNameEdit->clear();
  mRuleTypeLabel->clear();
  static_cast<PartitionBandsSummaryWidget *>( mSummaryBar )->setBands( {} );
  mSummaryLabel->clear();
  mBandTable->clearContents();
  mBandTable->setRowCount( 0 );
  mValidationLabel->clear();
  mPopulating = false;
  mDetailsStack->setCurrentIndex( 0 );
  mCopyRuleButton->setEnabled( false );
  mDeleteRuleButton->setEnabled( false );
  mSaveButton->setEnabled( false );
  mSaveAsButton->setEnabled( false );
  updateBandActions();
}

void QgsMtplPartitionRuleWidget::updateRuleListItem( int ruleIndex )
{
  if ( ruleIndex < 0 || ruleIndex >= mRules.size() || ruleIndex >= mRuleList->count() )
    return;

  const QgsMtplPartitionRuleEditorRule &rule = mRules.at( ruleIndex );
  QListWidgetItem *item = mRuleList->item( ruleIndex );
  QString label = rule.name.trimmed().isEmpty() ? tr( "未命名规则" ) : rule.name.trimmed();
  if ( rule.builtIn )
    label += tr( "（内置）" );
  else if ( mDirtyRuleIds.contains( rule.id ) )
    label += QStringLiteral( " *" );
  item->setText( label );
  item->setToolTip( summaryText( rule ) );
  item->setData( RULE_ID_ROLE, rule.id );
}

void QgsMtplPartitionRuleWidget::updateSummaryAndActions()
{
  if ( mCurrentRuleIndex < 0 || mCurrentRuleIndex >= mRules.size() )
    return;

  const QgsMtplPartitionRuleEditorRule &rule = mRules.at( mCurrentRuleIndex );
  const bool editable = !rule.builtIn;
  const QString error = validationError( rule );

  mNameEdit->setReadOnly( !editable );
  mBandTable->setEnabled( editable );
  static_cast<PartitionBandsSummaryWidget *>( mSummaryBar )->setBands( rule.bands );
  mSummaryLabel->setText( summaryText( rule ) );
  mValidationLabel->setProperty( "valid", error.isEmpty() );
  mValidationLabel->setText( error.isEmpty() ? tr( "规则有效，可用于定位瓦片包。" ) : tr( "无法保存：%1" ).arg( error ) );

  mCopyRuleButton->setEnabled( true );
  mDeleteRuleButton->setEnabled( editable );
  mSaveButton->setEnabled( editable && error.isEmpty() && mDirtyRuleIds.contains( rule.id ) );
  mSaveAsButton->setEnabled( error.isEmpty() );
  updateBandActions();
}

void QgsMtplPartitionRuleWidget::updateBandActions()
{
  const bool editable = currentRuleIsEditable();
  const int row = mBandTable->currentRow();
  const int rowCount = mBandTable->rowCount();
  const bool canAppendBand = editable && ( mRules.at( mCurrentRuleIndex ).bands.isEmpty() || mRules.at( mCurrentRuleIndex ).bands.constLast().maxZoom < MAXIMUM_SUPPORTED_ZOOM );
  mAddBandButton->setEnabled( canAppendBand );
  mDeleteBandButton->setEnabled( editable && row >= 0 && row < rowCount );
  mMoveBandUpButton->setEnabled( editable && row > 0 && row < rowCount );
  mMoveBandDownButton->setEnabled( editable && row >= 0 && row + 1 < rowCount );
}

void QgsMtplPartitionRuleWidget::markCurrentRuleDirty()
{
  if ( mCurrentRuleIndex < 0 || mCurrentRuleIndex >= mRules.size() )
    return;
  mDirtyRuleIds.insert( mRules.at( mCurrentRuleIndex ).id );
}

QgsMtplPartitionRuleEditorRule QgsMtplPartitionRuleWidget::ruleFromEditor() const
{
  QgsMtplPartitionRuleEditorRule rule = currentRule();
  rule.name = mNameEdit->text();
  rule.bands.clear();
  for ( int row = 0; row < mBandTable->rowCount(); ++row )
  {
    const auto *minimumSpin = qobject_cast<QSpinBox *>( mBandTable->cellWidget( row, 0 ) );
    const auto *maximumSpin = qobject_cast<QSpinBox *>( mBandTable->cellWidget( row, 1 ) );
    const auto *baseSpin = qobject_cast<QSpinBox *>( mBandTable->cellWidget( row, 2 ) );
    if ( !minimumSpin || !maximumSpin || !baseSpin )
      continue;
    rule.bands.append( QgsMtplPartitionRuleEditorBand { minimumSpin->value(), maximumSpin->value(), baseSpin->value() } );
  }
  return rule;
}

QString QgsMtplPartitionRuleWidget::validationError( const QgsMtplPartitionRuleEditorRule &rule ) const
{
  QString error;
  if ( !rule.isValid( &error ) )
    return error;

  for ( const QgsMtplPartitionRuleEditorRule &candidate : mRules )
  {
    if ( candidate.id != rule.id && candidate.name.trimmed().compare( rule.name.trimmed(), Qt::CaseInsensitive ) == 0 )
      return tr( "规则名不能与现有规则重复。" );
  }
  return QString();
}

QString QgsMtplPartitionRuleWidget::summaryText( const QgsMtplPartitionRuleEditorRule &rule ) const
{
  if ( rule.bands.isEmpty() )
    return tr( "尚未添加区间。" );

  QStringList parts;
  parts.reserve( rule.bands.size() );
  for ( const QgsMtplPartitionRuleEditorBand &band : rule.bands )
    parts.append( tr( "z%1–%2，基础 z%3" ).arg( band.minZoom ).arg( band.maxZoom ).arg( band.baseZoom ) );
  return parts.join( tr( "  |  " ) );
}

QString QgsMtplPartitionRuleWidget::uniqueRuleName( const QString &baseName ) const
{
  QString candidate = baseName;
  int suffix = 2;
  const auto isUsed = [this]( const QString &name ) {
    return std::any_of( mRules.cbegin(), mRules.cend(), [&name]( const QgsMtplPartitionRuleEditorRule &rule ) { return rule.name.trimmed().compare( name.trimmed(), Qt::CaseInsensitive ) == 0; } );
  };
  while ( isUsed( candidate ) )
    candidate = tr( "%1 %2" ).arg( baseName ).arg( suffix++ );
  return candidate;
}

QString QgsMtplPartitionRuleWidget::newRuleId() const
{
  return QStringLiteral( "editor-%1" ).arg( QUuid::createUuid().toString( QUuid::WithoutBraces ) );
}

int QgsMtplPartitionRuleWidget::ruleIndexForId( const QString &ruleId ) const
{
  for ( int i = 0; i < mRules.size(); ++i )
  {
    if ( mRules.at( i ).id == ruleId )
      return i;
  }
  return -1;
}

bool QgsMtplPartitionRuleWidget::currentRuleIsEditable() const
{
  return mCurrentRuleIndex >= 0 && mCurrentRuleIndex < mRules.size() && !mRules.at( mCurrentRuleIndex ).builtIn;
}
