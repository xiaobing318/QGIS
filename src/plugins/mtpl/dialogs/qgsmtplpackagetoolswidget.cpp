/***************************************************************************
  qgsmtplpackagetoolswidget.cpp
  -----------------------------
  begin                : August 2026
  copyright            : (C) 2026 QGIS contributors
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmtplpackagetoolswidget.h"

#include "../services/qgsmtplcredentialstore.h"
#include "../services/qgsmtplpackageoperation.h"

#include "qgsapplication.h"
#include "qgspasswordlineedit.h"

#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
  enum OperationIndex
  {
    EncryptOperation = 0,
    DecryptOperation = 1,
    RekeyOperation = 2,
    CreatePtpOperation = 3,
    CreateDtpOperation = 4,
    CreateVtpOperation = 5,
    CreateSfpOperation = 6
  };

  bool isTranscodeOperation( int operation )
  {
    return operation == EncryptOperation || operation == DecryptOperation || operation == RekeyOperation;
  }

  bool isCreateTileOperation( int operation )
  {
    return operation == CreatePtpOperation || operation == CreateDtpOperation || operation == CreateVtpOperation;
  }

  QAction *passwordVisibilityAction( QLineEdit *edit )
  {
    const QList<QAction *> actions = edit->actions();
    for ( QAction *action : actions )
    {
      if ( action->isCheckable() )
        return action;
    }
    return nullptr;
  }

  void localizePasswordVisibilityAction( QLineEdit *edit, const QString &showText, const QString &hideText )
  {
    QAction *action = passwordVisibilityAction( edit );
    if ( !action )
      return;

    action->setProperty( "mtplShowText", showText );
    action->setProperty( "mtplHideText", hideText );
    const auto updateText = [action, showText, hideText]( bool visible )
    {
      const QString text = visible ? hideText : showText;
      action->setText( text );
      action->setToolTip( text );
    };
    updateText( action->isChecked() );
    QObject::connect( action, &QAction::triggered, edit, updateText );
  }

  void hidePassword( QLineEdit *edit )
  {
    auto *passwordEdit = qobject_cast<QgsPasswordLineEdit *>( edit );
    if ( !passwordEdit )
      return;

    if ( QAction *action = passwordVisibilityAction( passwordEdit ) )
    {
      action->setChecked( false );
      passwordEdit->setPasswordVisibility( false );
      const QString showText = action->property( "mtplShowText" ).toString();
      if ( !showText.isEmpty() )
      {
        action->setText( showText );
        action->setToolTip( showText );
      }
      return;
    }
    passwordEdit->setPasswordVisibility( false );
  }

  QgsMtpl::PackageFormat creationFormat( int operation )
  {
    if ( operation == CreatePtpOperation )
      return QgsMtpl::PackageFormat::Ptp;
    if ( operation == CreateDtpOperation )
      return QgsMtpl::PackageFormat::Dtp;
    if ( operation == CreateVtpOperation )
      return QgsMtpl::PackageFormat::Vtp;
    return QgsMtpl::PackageFormat::Unknown;
  }

  QString mtplPackageFilter()
  {
    return QObject::tr( "MTPL 数据包 (*.ptp *.dtp *.vtp *.sfp);;所有文件 (*)" );
  }

  void localizeFileDialog( QFileDialog &dialog, const QString &acceptText, const QString &fileNameLabel = QObject::tr( "文件名" ) )
  {
    dialog.setLabelText( QFileDialog::LookIn, QObject::tr( "位置" ) );
    dialog.setLabelText( QFileDialog::FileName, fileNameLabel );
    dialog.setLabelText( QFileDialog::FileType, QObject::tr( "文件类型" ) );
    dialog.setLabelText( QFileDialog::Accept, acceptText );
    dialog.setLabelText( QFileDialog::Reject, QObject::tr( "取消" ) );
    if ( QDialogButtonBox *buttonBox = dialog.findChild<QDialogButtonBox *>() )
    {
      if ( QPushButton *cancelButton = buttonBox->button( QDialogButtonBox::Cancel ) )
        cancelButton->setText( QObject::tr( "取消" ) );
    }
  }

  QString comparablePath( const QString &path )
  {
    const QString trimmed = path.trimmed();
    if ( trimmed.isEmpty() )
      return QString();
    return QDir::cleanPath( QFileInfo( QDir::fromNativeSeparators( trimmed ) ).absoluteFilePath() );
  }

  QString normalizedPtpCreationPath( const QString &path )
  {
    QString normalized = path.trimmed();
    if ( normalized.size() >= 2 &&
         ( ( normalized.startsWith( QLatin1Char( '"' ) ) && normalized.endsWith( QLatin1Char( '"' ) ) ) ||
           ( normalized.startsWith( QLatin1Char( '\'' ) ) && normalized.endsWith( QLatin1Char( '\'' ) ) ) ) )
      normalized = normalized.mid( 1, normalized.size() - 2 ).trimmed();
    return normalized;
  }

  bool pathsMatch( const QString &left, const QString &right )
  {
    const Qt::CaseSensitivity sensitivity =
#ifdef Q_OS_WIN
      Qt::CaseInsensitive;
#else
      Qt::CaseSensitive;
#endif
    return comparablePath( left ).compare( comparablePath( right ), sensitivity ) == 0;
  }

  QString sourceKeysValidationError( const QgsMtpl::CryptoKeys &keys )
  {
    if ( keys.isEmpty() )
      return QString();

    QString error;
    return keys.isValid( &error ) ? QString() : error;
  }

  QWidget *pathRow( QLineEdit *edit, QPushButton *button, QWidget *parent, QPushButton *secondaryButton = nullptr )
  {
    QWidget *container = new QWidget( parent );
    QHBoxLayout *layout = new QHBoxLayout( container );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->addWidget( edit, 1 );
    layout->addWidget( button );
    if ( secondaryButton )
      layout->addWidget( secondaryButton );
    return container;
  }
}

QgsMtplPackageToolsWidget::QgsMtplPackageToolsWidget( QWidget *parent )
  : QWidget( parent )
{
  setObjectName( QStringLiteral( "mtplPackageToolsWidget" ) );
  auto *outerLayout = new QVBoxLayout( this );
  outerLayout->setContentsMargins( 0, 0, 0, 0 );
  auto *scrollArea = new QScrollArea( this );
  scrollArea->setObjectName( QStringLiteral( "mtplPackageToolsScrollArea" ) );
  scrollArea->setWidgetResizable( true );
  scrollArea->setFrameShape( QFrame::NoFrame );
  scrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  auto *page = new QWidget( scrollArea );
  auto *mainLayout = new QVBoxLayout( page );
  mainLayout->setContentsMargins( 4, 4, 4, 4 );
  mainLayout->setSpacing( 8 );
  scrollArea->setWidget( page );
  outerLayout->addWidget( scrollArea );

  QFormLayout *form = new QFormLayout();
  form->setRowWrapPolicy( QFormLayout::WrapLongRows );
  form->setFieldGrowthPolicy( QFormLayout::AllNonFixedFieldsGrow );
  mainLayout->addLayout( form );

  mOperationCombo = new QComboBox( page );
  mOperationCombo->setObjectName( QStringLiteral( "mtplPackageToolAction" ) );
  mOperationCombo->addItem( tr( "加密数据包" ), EncryptOperation );
  mOperationCombo->addItem( tr( "解密数据包" ), DecryptOperation );
  mOperationCombo->addItem( tr( "更换数据包密钥" ), RekeyOperation );
  mOperationCombo->addItem( tr( "创建 PTP 数据包" ), CreatePtpOperation );
  mOperationCombo->addItem( tr( "创建 DTP 数据包" ), CreateDtpOperation );
  mOperationCombo->addItem( tr( "创建 VTP 数据包" ), CreateVtpOperation );
  mOperationCombo->addItem( tr( "创建 SFP 数据包" ), CreateSfpOperation );
  form->addRow( tr( "操作" ), mOperationCombo );

  mSourceEdit = new QLineEdit( page );
  mSourceEdit->setObjectName( QStringLiteral( "mtplPackageToolSource" ) );
  mSourceEdit->setClearButtonEnabled( true );
  mSourceBrowseButton = new QPushButton( tr( "浏览…" ), page );
  mSourceBrowseButton->setObjectName( QStringLiteral( "mtplPackageToolSourceBrowse" ) );
  mRestoreSourceButton = new QPushButton( tr( "使用当前数据包路径" ), page );
  mRestoreSourceButton->setObjectName( QStringLiteral( "mtplPackageToolRestoreSource" ) );
  mRestoreSourceButton->setToolTip( tr( "重新使用“数据包”页当前选择的路径，并恢复自动跟随。" ) );
  form->addRow( tr( "源路径" ), pathRow( mSourceEdit, mSourceBrowseButton, page, mRestoreSourceButton ) );

  mOutputEdit = new QLineEdit( page );
  mOutputEdit->setObjectName( QStringLiteral( "mtplPackageToolOutput" ) );
  mOutputEdit->setClearButtonEnabled( true );
  mOutputBrowseButton = new QPushButton( tr( "浏览…" ), page );
  mOutputBrowseButton->setObjectName( QStringLiteral( "mtplPackageToolOutputBrowse" ) );
  form->addRow( tr( "输出路径" ), pathRow( mOutputEdit, mOutputBrowseButton, page ) );

  mPtpCreateHelpLabel = new QLabel(
    tr( "选择包含 {z}/{x}/{y}.ext 图片树的原始影像目录，支持 PNG、JPEG 和 WebP。"
        "每张图片的宽、高都须与“切片大小”一致，输出为单个 PTP 数据包。" ), page );
  mPtpCreateHelpLabel->setObjectName( QStringLiteral( "mtplPackageToolPtpCreateHelp" ) );
  mPtpCreateHelpLabel->setWordWrap( true );
  form->addRow( mPtpCreateHelpLabel );

  mTileOptions = new QWidget( page );
  QFormLayout *tileForm = new QFormLayout( mTileOptions );
  tileForm->setContentsMargins( 0, 0, 0, 0 );
  mFormatCombo = new QComboBox( mTileOptions );
  mFormatCombo->setObjectName( QStringLiteral( "mtplPackageToolFormat" ) );
  mFormatCombo->addItem( QStringLiteral( "PTP" ), static_cast<int>( QgsMtpl::PackageFormat::Ptp ) );
  mFormatCombo->addItem( QStringLiteral( "DTP" ), static_cast<int>( QgsMtpl::PackageFormat::Dtp ) );
  mFormatCombo->addItem( QStringLiteral( "VTP" ), static_cast<int>( QgsMtpl::PackageFormat::Vtp ) );
  tileForm->addRow( tr( "格式" ), mFormatCombo );
  mTileSizeCombo = new QComboBox( mTileOptions );
  mTileSizeCombo->setObjectName( QStringLiteral( "mtplPackageToolTileSize" ) );
  mTileSizeCombo->addItem( QStringLiteral( "256" ), 256 );
  mTileSizeCombo->addItem( QStringLiteral( "129" ), 129 );
  mTileSizeCombo->addItem( QStringLiteral( "33" ), 33 );
  tileForm->addRow( tr( "切片大小" ), mTileSizeCombo );
  mMetadataEdit = new QPlainTextEdit( QStringLiteral( "{}" ), mTileOptions );
  mMetadataEdit->setObjectName( QStringLiteral( "mtplPackageToolMetadata" ) );
  mMetadataEdit->setMaximumHeight( 100 );
  mMetadataEdit->setPlaceholderText( tr( "JSON 元数据" ) );
  tileForm->addRow( tr( "元数据" ), mMetadataEdit );
  form->addRow( mTileOptions );

  mEncryptCheck = new QCheckBox( tr( "加密创建的数据包并保存密钥附属文件" ), page );
  mEncryptCheck->setObjectName( QStringLiteral( "mtplPackageToolEncrypt" ) );
  form->addRow( mEncryptCheck );

  mSourceKeysBox = new QGroupBox( tr( "源密钥" ), page );
  mSourceKeysBox->setObjectName( QStringLiteral( "mtplPackageToolSourceKeys" ) );
  QFormLayout *keyForm = new QFormLayout( mSourceKeysBox );
  auto *keyHelpLabel = new QLabel(
    tr( "有匹配的密钥附属文件时可留空。输入的密钥只用于本次操作，不会自动保存。"
        "如果已通过插件安全保存一组密钥，输入留空时会在运行操作时请求解锁。" ),
    mSourceKeysBox );
  keyHelpLabel->setWordWrap( true );
  keyForm->addRow( keyHelpLabel );
  mPrivateKeyEdit = new QgsPasswordLineEdit( mSourceKeysBox );
  mPrivateKeyEdit->setObjectName( QStringLiteral( "mtplPackageToolPrivateKey" ) );
  mPrivateKeyEdit->setEchoMode( QLineEdit::Password );
  mPrivateKeyEdit->setClearButtonEnabled( true );
  mPrivateKeyEdit->setPlaceholderText( tr( "Base64 私钥" ) );
  mPrivateKeyEdit->setAccessibleName( tr( "源私钥" ) );
  mPrivateKeyEdit->setAccessibleDescription( tr( "请输入规范的 Base64 私钥。末尾按钮用于显示或隐藏内容。" ) );
  localizePasswordVisibilityAction( mPrivateKeyEdit, tr( "显示私钥" ), tr( "隐藏私钥" ) );
  keyForm->addRow( tr( "私钥" ), mPrivateKeyEdit );
  mDeviceKeyEdit = new QgsPasswordLineEdit( mSourceKeysBox );
  mDeviceKeyEdit->setObjectName( QStringLiteral( "mtplPackageToolDeviceKey" ) );
  mDeviceKeyEdit->setEchoMode( QLineEdit::Password );
  mDeviceKeyEdit->setClearButtonEnabled( true );
  mDeviceKeyEdit->setPlaceholderText( tr( "小写十六进制设备密钥" ) );
  mDeviceKeyEdit->setAccessibleName( tr( "源设备密钥" ) );
  mDeviceKeyEdit->setAccessibleDescription( tr( "请输入偶数长度的小写十六进制设备密钥。末尾按钮用于显示或隐藏内容。" ) );
  localizePasswordVisibilityAction( mDeviceKeyEdit, tr( "显示设备密钥" ), tr( "隐藏设备密钥" ) );
  keyForm->addRow( tr( "设备密钥" ), mDeviceKeyEdit );
  auto *keyStorageWarning = new QLabel( tr( "需要长期保存密钥时，请在“数据包”页明确选择使用 QGIS 认证管理器安全记住。" ), mSourceKeysBox );
  keyStorageWarning->setWordWrap( true );
  keyStorageWarning->setAccessibleName( tr( "MTPL 密钥存储说明" ) );
  keyForm->addRow( keyStorageWarning );
  mainLayout->addWidget( mSourceKeysBox );

  mStatusLabel = new QLabel( page );
  mStatusLabel->setObjectName( QStringLiteral( "mtplPackageToolStatus" ) );
  mStatusLabel->setAccessibleName( tr( "数据包工具状态" ) );
  mStatusLabel->setWordWrap( true );
  mStatusLabel->setText( QgsMtplCredentialStore::hasRememberedKeys()
                           ? tr( "已安全保存一组密钥。输入留空时，将在运行操作后请求解锁。" )
                           : tr( "请选择源路径和输出路径，然后运行数据包操作。" ) );
  mainLayout->addWidget( mStatusLabel );

  mProgressBar = new QProgressBar( page );
  mProgressBar->setObjectName( QStringLiteral( "mtplPackageToolProgress" ) );
  mProgressBar->setRange( 0, 100 );
  mProgressBar->hide();
  mainLayout->addWidget( mProgressBar );

  mResultDetails = new QPlainTextEdit( page );
  mResultDetails->setObjectName( QStringLiteral( "mtplPackageToolResult" ) );
  mResultDetails->setAccessibleName( tr( "数据包工具结果" ) );
  mResultDetails->setReadOnly( true );
  mResultDetails->setMaximumHeight( 220 );
  mResultDetails->hide();
  mainLayout->addWidget( mResultDetails );

  mSidecarActions = new QWidget( page );
  mSidecarActions->setObjectName( QStringLiteral( "mtplPackageToolSidecarActions" ) );
  auto *sidecarLayout = new QHBoxLayout( mSidecarActions );
  sidecarLayout->setContentsMargins( 0, 0, 0, 0 );
  mCopySidecarButton = new QPushButton( tr( "复制附属文件路径" ), mSidecarActions );
  mCopySidecarButton->setObjectName( QStringLiteral( "mtplPackageToolCopySidecar" ) );
  mOpenSidecarFolderButton = new QPushButton( tr( "打开所在文件夹" ), mSidecarActions );
  mOpenSidecarFolderButton->setObjectName( QStringLiteral( "mtplPackageToolOpenSidecarFolder" ) );
  sidecarLayout->addWidget( mCopySidecarButton );
  sidecarLayout->addWidget( mOpenSidecarFolderButton );
  sidecarLayout->addStretch();
  mSidecarActions->hide();
  mainLayout->addWidget( mSidecarActions );

  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->setContentsMargins( 0, 0, 0, 0 );
  buttonLayout->addStretch();
  mRunButton = new QPushButton( tr( "运行" ), page );
  mRunButton->setObjectName( QStringLiteral( "mtplPackageToolRun" ) );
  mRunButton->setAccessibleName( tr( "运行数据包操作" ) );
  mCancelButton = new QPushButton( tr( "取消操作" ), page );
  mCancelButton->setObjectName( QStringLiteral( "mtplPackageToolCancel" ) );
  mCancelButton->setEnabled( false );
  buttonLayout->addWidget( mRunButton );
  buttonLayout->addWidget( mCancelButton );
  mainLayout->addLayout( buttonLayout );
  mainLayout->addStretch();

  connect( mOperationCombo, qOverload<int>( &QComboBox::currentIndexChanged ), this, [this] { updateOperationUi(); } );
  connect( mSourceEdit, &QLineEdit::textEdited, this, &QgsMtplPackageToolsWidget::sourceTextEdited );
  connect( mSourceBrowseButton, &QPushButton::clicked, this, [this] { browseSource(); } );
  connect( mOutputBrowseButton, &QPushButton::clicked, this, [this] { browseOutput(); } );
  connect( mRestoreSourceButton, &QPushButton::clicked, this, &QgsMtplPackageToolsWidget::restoreSuggestedSource );
  connect( mRunButton, &QPushButton::clicked, this, [this] { startOperation(); } );
  connect( mCancelButton, &QPushButton::clicked, this, &QgsMtplPackageToolsWidget::cancelOperation );
  connect( mCopySidecarButton, &QPushButton::clicked, this, [this]
  {
    if ( !mSidecarPath.isEmpty() )
      QGuiApplication::clipboard()->setText( QDir::toNativeSeparators( mSidecarPath ) );
  } );
  connect( mOpenSidecarFolderButton, &QPushButton::clicked, this, [this]
  {
    if ( !mSidecarPath.isEmpty() )
      QDesktopServices::openUrl( QUrl::fromLocalFile( QFileInfo( mSidecarPath ).absolutePath() ) );
  } );
  updateOperationUi();
  setOperationUiState( OperationUiState::Idle );
  updateSourceFollowUi();
}

QgsMtplPackageToolsWidget::~QgsMtplPackageToolsWidget()
{
  shutdown();
}

void QgsMtplPackageToolsWidget::setSuggestedSourcePath( const QString &path )
{
  const QString normalized = path.trimmed().isEmpty()
    ? QString()
    : QDir::toNativeSeparators( QDir::cleanPath( QDir::fromNativeSeparators( path.trimmed() ) ) );
  mSuggestedSourcePath = normalized;
  if ( isOperationRunning() )
  {
    mDeferredSuggestedSourcePath = normalized;
    mHasDeferredSuggestedSource = true;
    updateSourceFollowUi();
    return;
  }
  if ( mFollowsSuggestedSource && !normalized.isEmpty() )
    mSourceEdit->setText( normalized );
  updateSourceFollowUi();
}

void QgsMtplPackageToolsWidget::reloadRememberedKeys()
{
  if ( mShuttingDown )
    return;
  if ( isOperationRunning() )
  {
    mReloadRememberedKeysDeferred = true;
    return;
  }

  mReloadRememberedKeysDeferred = false;
  mStatusLabel->setText( QgsMtplCredentialStore::hasRememberedKeys()
                           ? tr( "已安全保存一组密钥。输入留空时，将在运行操作后请求解锁。" )
                           : tr( "未安全保存密钥。这里输入的密钥只用于本次操作。" ) );
}

void QgsMtplPackageToolsWidget::cancelOperation()
{
  concealSecrets();
  if ( !mTask )
    return;
  setOperationUiState( OperationUiState::Canceling, tr( "正在取消数据包操作…" ) );
  mTask->cancel();
}

bool QgsMtplPackageToolsWidget::isOperationRunning() const
{
  return !mTask.isNull();
}

void QgsMtplPackageToolsWidget::concealSecrets()
{
  hidePassword( mPrivateKeyEdit );
  hidePassword( mDeviceKeyEdit );
}

void QgsMtplPackageToolsWidget::shutdown()
{
  if ( mShuttingDown )
    return;

  mShuttingDown = true;
  mReloadRememberedKeysDeferred = false;
  ++mOperationGeneration;
  const QPointer<QgsMtplPackageOperationTask> task = mTask;
  mTask = nullptr;
  if ( task )
    task->cancel();
  concealSecrets();
  mPrivateKeyEdit->clear();
  mDeviceKeyEdit->clear();
  setEnabled( false );
}

void QgsMtplPackageToolsWidget::sourceTextEdited( const QString &text )
{
  mFollowsSuggestedSource = !mSuggestedSourcePath.isEmpty() && pathsMatch( text, mSuggestedSourcePath );
  updateSourceFollowUi();
}

void QgsMtplPackageToolsWidget::restoreSuggestedSource()
{
  if ( mSuggestedSourcePath.isEmpty() || isOperationRunning() )
    return;

  mFollowsSuggestedSource = true;
  mSourceEdit->setText( QDir::toNativeSeparators( mSuggestedSourcePath ) );
  updateSourceFollowUi();
}

void QgsMtplPackageToolsWidget::updateSourceFollowUi()
{
  if ( !mRestoreSourceButton )
    return;

  const bool synchronized = !mSuggestedSourcePath.isEmpty() && pathsMatch( mSourceEdit->text(), mSuggestedSourcePath );
  mRestoreSourceButton->setEnabled( !isOperationRunning() && !mSuggestedSourcePath.isEmpty() && !synchronized );
  mRestoreSourceButton->setVisible( !mSuggestedSourcePath.isEmpty() );
}

void QgsMtplPackageToolsWidget::applyDeferredSuggestedSource()
{
  if ( !mHasDeferredSuggestedSource )
    return;

  const QString deferred = mDeferredSuggestedSourcePath;
  mDeferredSuggestedSourcePath.clear();
  mHasDeferredSuggestedSource = false;
  mSuggestedSourcePath = deferred;
  if ( mFollowsSuggestedSource && !deferred.isEmpty() )
    mSourceEdit->setText( QDir::toNativeSeparators( deferred ) );
  updateSourceFollowUi();
}

void QgsMtplPackageToolsWidget::setOperationUiState( OperationUiState state, const QString &status )
{
  QString stateName;
  switch ( state )
  {
    case OperationUiState::Idle:
      stateName = QStringLiteral( "idle" );
      break;
    case OperationUiState::Running:
      stateName = QStringLiteral( "running" );
      break;
    case OperationUiState::Canceling:
      stateName = QStringLiteral( "canceling" );
      break;
    case OperationUiState::Succeeded:
      stateName = QStringLiteral( "succeeded" );
      break;
    case OperationUiState::PartiallySucceeded:
      stateName = QStringLiteral( "partiallySucceeded" );
      break;
    case OperationUiState::Failed:
      stateName = QStringLiteral( "failed" );
      break;
    case OperationUiState::Canceled:
      stateName = QStringLiteral( "canceled" );
      break;
  }
  setProperty( "mtplOperationState", stateName );

  const bool running = state == OperationUiState::Running || state == OperationUiState::Canceling;
  const bool canceling = state == OperationUiState::Canceling;
  mOperationCombo->setEnabled( !running );
  mSourceEdit->setEnabled( !running );
  mSourceBrowseButton->setEnabled( !running );
  mOutputEdit->setEnabled( !running );
  mOutputBrowseButton->setEnabled( !running );
  mFormatCombo->setEnabled( false );
  mTileSizeCombo->setEnabled( !running );
  mMetadataEdit->setEnabled( !running );
  mEncryptCheck->setEnabled( !running );
  mPrivateKeyEdit->setEnabled( !running );
  mDeviceKeyEdit->setEnabled( !running );
  mRunButton->setEnabled( !running );
  mCancelButton->setEnabled( running && !canceling );
  mProgressBar->setVisible( running );
  if ( !status.isNull() )
    mStatusLabel->setText( status );
  updateSourceFollowUi();
}

void QgsMtplPackageToolsWidget::showInlineValidationError( const QString &message )
{
  setOperationUiState( OperationUiState::Failed, message );
  setResultDetails( message );
  emit messageRequested( tr( "MTPL 数据包工具" ), message, Qgis::MessageLevel::Warning );
}

void QgsMtplPackageToolsWidget::setResultDetails( const QString &message, const QString &sidecarPath )
{
  mSidecarPath = sidecarPath;
  mResultDetails->setPlainText( message );
  mResultDetails->setVisible( !message.isEmpty() );
  mSidecarActions->setVisible( !sidecarPath.isEmpty() );
}

void QgsMtplPackageToolsWidget::updateOperationUi()
{
  const int operation = mOperationCombo->currentData().toInt();
  const bool createTile = isCreateTileOperation( operation );
  const bool createPtp = operation == CreatePtpOperation;
  mPtpCreateHelpLabel->setVisible( createPtp );
  mOutputEdit->setPlaceholderText( createPtp ? tr( "单个输出数据包，例如 imagery.ptp" ) : QString() );
  mTileSizeCombo->setToolTip( createPtp ? tr( "与每张原始影像的宽、高像素数一致。" ) : QString() );
  mTileOptions->setVisible( createTile );
  if ( createTile )
  {
    const int formatIndex = mFormatCombo->findData( static_cast<int>( creationFormat( operation ) ) );
    mFormatCombo->setCurrentIndex( formatIndex );
    mFormatCombo->setEnabled( false );
  }
  mSourceKeysBox->setVisible( isTranscodeOperation( operation ) );
  if ( !isTranscodeOperation( operation ) )
  {
    hidePassword( mPrivateKeyEdit );
    hidePassword( mDeviceKeyEdit );
  }
  mEncryptCheck->setVisible( createTile || operation == CreateSfpOperation );
  if ( isTranscodeOperation( operation ) )
    mSourceEdit->setPlaceholderText( tr( "数据包文件或文件夹" ) );
  else if ( createPtp )
    mSourceEdit->setPlaceholderText( tr( "原始影像目录，内含 {z}/{x}/{y}.png 等图片" ) );
  else if ( createTile )
    mSourceEdit->setPlaceholderText( tr( "包含 {z}/{x}/{y}.ext 的文件夹" ) );
  else
    mSourceEdit->setPlaceholderText( tr( "要递归打包的文件夹" ) );
}

void QgsMtplPackageToolsWidget::browseSource()
{
  const int operation = mOperationCombo->currentData().toInt();
  if ( !isTranscodeOperation( operation ) )
  {
    QFileDialog dialog( this, tr( "选择源文件夹" ), mSourceEdit->text(), tr( "所有文件 (*)" ) );
    dialog.setAcceptMode( QFileDialog::AcceptOpen );
    dialog.setFileMode( QFileDialog::Directory );
    dialog.setOption( QFileDialog::ShowDirsOnly, true );
    dialog.setOption( QFileDialog::DontUseNativeDialog, true );
    dialog.setOption( QFileDialog::ReadOnly, true );
    localizeFileDialog( dialog, tr( "选择" ), tr( "文件夹名称" ) );
    if ( dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty() )
    {
      mSourceEdit->setText( QDir::toNativeSeparators( dialog.selectedFiles().constFirst() ) );
      sourceTextEdited( mSourceEdit->text() );
    }
    return;
  }

  QFileInfo currentInfo( mSourceEdit->text() );
  const QString initialDirectory = currentInfo.isDir() ? currentInfo.absoluteFilePath() : currentInfo.absolutePath();
  QFileDialog dialog( this, tr( "选择 MTPL 数据包或文件夹" ), initialDirectory, mtplPackageFilter() );
  dialog.setAcceptMode( QFileDialog::AcceptOpen );
  dialog.setFileMode( QFileDialog::ExistingFile );
  dialog.setOption( QFileDialog::DontUseNativeDialog, true );
  dialog.setOption( QFileDialog::ReadOnly, true );
  localizeFileDialog( dialog, tr( "选择" ) );

  QString selectedFolder;
  if ( QDialogButtonBox *buttonBox = dialog.findChild<QDialogButtonBox *>() )
  {
    auto *chooseFolder = new QPushButton( tr( "选择当前文件夹" ), &dialog );
    buttonBox->addButton( chooseFolder, QDialogButtonBox::ActionRole );
    connect( chooseFolder, &QPushButton::clicked, &dialog, [&dialog, &selectedFolder]
    {
      selectedFolder = dialog.directory().absolutePath();
      dialog.QDialog::accept();
    } );
  }

  if ( dialog.exec() != QDialog::Accepted )
    return;

  const QString selectedPath = selectedFolder.isEmpty()
    ? ( dialog.selectedFiles().isEmpty() ? QString() : dialog.selectedFiles().constFirst() )
    : selectedFolder;
  if ( !selectedPath.isEmpty() )
  {
    mSourceEdit->setText( QDir::toNativeSeparators( selectedPath ) );
    sourceTextEdited( mSourceEdit->text() );
  }
}

void QgsMtplPackageToolsWidget::browseOutput()
{
  const int operation = mOperationCombo->currentData().toInt();
  const QFileInfo sourceInfo( mSourceEdit->text() );
  QString output;
  if ( isTranscodeOperation( operation ) && sourceInfo.isDir() )
  {
    QFileDialog dialog( this, tr( "选择新的输出文件夹" ), mOutputEdit->text(), tr( "所有文件 (*)" ) );
    dialog.setAcceptMode( QFileDialog::AcceptSave );
    dialog.setFileMode( QFileDialog::AnyFile );
    dialog.setOption( QFileDialog::DontUseNativeDialog, true );
    localizeFileDialog( dialog, tr( "选择" ), tr( "文件夹名称" ) );
    if ( dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty() )
      output = dialog.selectedFiles().constFirst();
  }
  else
  {
    QString filter;
    if ( isTranscodeOperation( operation ) )
      filter = mtplPackageFilter();
    else if ( operation == CreateSfpOperation )
      filter = tr( "SFP 数据包 (*.sfp)" );
    else
      filter = tr( "%1 数据包 (*.%2)" ).arg( mFormatCombo->currentText(), mFormatCombo->currentText().toLower() );
    QFileDialog dialog( this, tr( "选择输出数据包" ), mOutputEdit->text(), filter );
    dialog.setAcceptMode( QFileDialog::AcceptSave );
    dialog.setFileMode( QFileDialog::AnyFile );
    dialog.setOption( QFileDialog::DontUseNativeDialog, true );
    localizeFileDialog( dialog, tr( "保存" ) );
    if ( dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty() )
      output = dialog.selectedFiles().constFirst();
  }
  if ( !output.isEmpty() )
    mOutputEdit->setText( QDir::toNativeSeparators( output ) );
}

QString QgsMtplPackageToolsWidget::normalizedOutputPath( const QString &sourcePath ) const
{
  const int operation = mOperationCombo->currentData().toInt();
  const QString outputText = operation == CreatePtpOperation
                               ? normalizedPtpCreationPath( mOutputEdit->text() )
                               : mOutputEdit->text();
  QString output = QFileInfo( outputText ).absoluteFilePath();
  if ( isTranscodeOperation( operation ) && QFileInfo( sourcePath ).isDir() )
    return output;

  QString suffix;
  if ( isTranscodeOperation( operation ) )
    suffix = QFileInfo( sourcePath ).suffix().toLower();
  else if ( operation == CreateSfpOperation )
    suffix = QStringLiteral( "sfp" );
  else
    suffix = mFormatCombo->currentText().toLower();
  if ( QFileInfo( output ).suffix().isEmpty() )
    output += QLatin1Char( '.' ) + suffix;
  return output;
}

void QgsMtplPackageToolsWidget::startOperation()
{
  if ( mTask || mShuttingDown )
    return;

  const int operation = mOperationCombo->currentData().toInt();
  const bool createPtp = operation == CreatePtpOperation;
  const QString sourceText = createPtp ? normalizedPtpCreationPath( mSourceEdit->text() ) : mSourceEdit->text();
  if ( createPtp && sourceText.isEmpty() )
  {
    showInlineValidationError( tr( "请选择原始影像目录。" ) );
    return;
  }
  const QString sourcePath = QFileInfo( sourceText ).absoluteFilePath();
  const QFileInfo sourceInfo( sourcePath );
  if ( !sourceInfo.exists() )
  {
    showInlineValidationError( tr( "源路径不存在。" ) );
    return;
  }
  if ( createPtp && !sourceInfo.isDir() )
  {
    showInlineValidationError( tr( "请选择包含 {z}/{x}/{y} 图片树的原始影像目录。" ) );
    return;
  }
  const QString outputText = createPtp ? normalizedPtpCreationPath( mOutputEdit->text() ) : mOutputEdit->text().trimmed();
  if ( outputText.isEmpty() )
  {
    showInlineValidationError( tr( "请选择输出路径。" ) );
    return;
  }

  QgsMtpl::PackageOperationRequest request;
  request.encryptOutput = operation == EncryptOperation || operation == RekeyOperation ||
                          ( ( isCreateTileOperation( operation ) || operation == CreateSfpOperation ) && mEncryptCheck->isChecked() );
  request.preserveSfpMixedStorage = operation == RekeyOperation;
  request.generateOutputKeys = request.encryptOutput;
  request.writeSidecar = request.encryptOutput;
  const QString outputPath = normalizedOutputPath( sourcePath );

  if ( isTranscodeOperation( operation ) )
  {
    request.type = QgsMtpl::PackageOperationType::Convert;
    QgsMtpl::CryptoKeys suppliedKeys;
    suppliedKeys.privateKey = mPrivateKeyEdit->text().trimmed().toLatin1();
    suppliedKeys.deviceKey = mDeviceKeyEdit->text().trimmed().toLatin1();
    bool loadedFromSecureStore = false;
    if ( suppliedKeys.hasAnyValue() )
    {
      const QString validationError = sourceKeysValidationError( suppliedKeys );
      if ( !validationError.isEmpty() )
      {
        suppliedKeys.clear();
        showInlineValidationError( validationError );
        return;
      }
    }
    else if ( QgsMtplCredentialStore::hasRememberedKeys() )
    {
      QString loadError;
      suppliedKeys = QgsMtplCredentialStore::rememberedKeys( &loadError );
      if ( !suppliedKeys.isValid() )
      {
        suppliedKeys.clear();
        showInlineValidationError( loadError.isEmpty()
                                     ? tr( "无法读取安全保存的 MTPL 密钥。" )
                                     : loadError );
        return;
      }
      loadedFromSecureStore = true;
    }

    const QgsMtpl::CredentialSource suppliedSource = suppliedKeys.hasAnyValue()
      ? ( loadedFromSecureStore ? QgsMtpl::CredentialSource::Remembered : QgsMtpl::CredentialSource::Explicit )
      : QgsMtpl::CredentialSource::None;
    request.conversionSourcePath = sourcePath;
    request.suppliedSourceKeys = suppliedKeys;
    request.suppliedCredentialSource = suppliedSource;
    if ( sourceInfo.isDir() )
      request.outputDirectory = outputPath;
    else
      request.outputPath = outputPath;
    suppliedKeys.clear();
  }
  else if ( isCreateTileOperation( operation ) )
  {
    QJsonParseError parseError;
    const QByteArray metadata = mMetadataEdit->toPlainText().toUtf8();
    const QJsonDocument metadataDocument = QJsonDocument::fromJson( metadata, &parseError );
    if ( parseError.error != QJsonParseError::NoError || !metadataDocument.isObject() )
    {
      showInlineValidationError( tr( "切片元数据必须是 JSON 对象。" ) );
      return;
    }
    request.type = QgsMtpl::PackageOperationType::CreateTilePackage;
    request.sourcePath = sourcePath;
    request.outputPath = outputPath;
    request.outputFormat = creationFormat( operation );
    request.tileSize = mTileSizeCombo->currentData().toInt();
    request.metadata = metadataDocument.toJson( QJsonDocument::Compact );
  }
  else
  {
    request.type = QgsMtpl::PackageOperationType::CreateSfpPackage;
    request.sourcePath = sourcePath;
    request.outputPath = outputPath;
    request.outputFormat = QgsMtpl::PackageFormat::Sfp;
  }

  auto *task = new QgsMtplPackageOperationTask( request );
  mTask = task;
  mRunningOperation = operation;
  const quint64 generation = ++mOperationGeneration;
  connect( task, &QgsTask::taskCompleted, this, [this, task, generation]
  {
    operationFinished( task, generation, true );
  } );
  connect( task, &QgsTask::taskTerminated, this, [this, task, generation]
  {
    operationFinished( task, generation, false );
  } );
  connect( task, &QgsTask::progressChanged, this, [this, task, generation]( double progress )
  {
    if ( generation == mOperationGeneration && mTask == task )
      mProgressBar->setValue( qRound( progress ) );
  } );
  concealSecrets();
  mProgressBar->setValue( 0 );
  setResultDetails( QString() );
  setOperationUiState( OperationUiState::Running, tr( "数据包操作正在后台运行…" ) );
  QgsApplication::taskManager()->addTask( task );
}

void QgsMtplPackageToolsWidget::operationFinished( QgsMtplPackageOperationTask *task, quint64 generation, bool successful )
{
  if ( mShuttingDown || !task || generation != mOperationGeneration || mTask != task )
    return;

  concealSecrets();
  const QgsMtpl::PackageOperationResult result = task->result();
  int succeededCount = 0;
  int skippedCount = 0;
  int failedCount = 0;
  QStringList itemProblems;
  for ( const QgsMtpl::PackageOperationItemResult &item : result.items )
  {
    if ( item.status == QgsMtpl::PackageOperationItemStatus::Succeeded )
    {
      ++succeededCount;
      continue;
    }
    const bool skipped = item.status == QgsMtpl::PackageOperationItemStatus::Skipped;
    if ( skipped )
      ++skippedCount;
    else
      ++failedCount;
    if ( itemProblems.size() < 8 )
    {
      const QString itemName = QFileInfo( item.inputPath ).fileName().isEmpty()
        ? QDir::toNativeSeparators( item.inputPath )
        : QFileInfo( item.inputPath ).fileName();
      itemProblems.append( tr( "%1：%2（%3）" )
                             .arg( skipped ? tr( "跳过" ) : tr( "失败" ),
                                   itemName,
                                   item.message.isEmpty() ? tr( "未提供原因" ) : item.message ) );
    }
  }
  auto appendItemSummary = [&]( QString &message )
  {
    if ( result.items.isEmpty() )
      return;
    message += QLatin1Char( '\n' ) + tr( "逐项结果：%1 项成功，%2 项跳过，%3 项失败。" )
                                      .arg( succeededCount ).arg( skippedCount ).arg( failedCount );
    if ( !itemProblems.isEmpty() )
      message += QLatin1Char( '\n' ) + itemProblems.join( QLatin1Char( '\n' ) );
    const int omittedCount = skippedCount + failedCount - itemProblems.size();
    if ( omittedCount > 0 )
      message += QLatin1Char( '\n' ) + tr( "另有 %1 项未展开。" ).arg( omittedCount );
  };
  auto appendRetainedOutputPaths = [&]( QString &message )
  {
    if ( result.outputPaths.isEmpty() )
      return;
    message += QLatin1Char( '\n' ) + tr( "仍保留的输出：" );
    const qsizetype visibleCount = std::min<qsizetype>( result.outputPaths.size(), 8 );
    for ( qsizetype index = 0; index < visibleCount; ++index )
      message += QLatin1Char( '\n' ) + QDir::toNativeSeparators( result.outputPaths.at( index ) );
    if ( result.outputPaths.size() > visibleCount )
      message += QLatin1Char( '\n' ) + tr( "另有 %1 个输出未展开。" ).arg( result.outputPaths.size() - visibleCount );
  };
  const int operation = mRunningOperation;
  mTask = nullptr;
  mRunningOperation = -1;
  if ( mReloadRememberedKeysDeferred )
    reloadRememberedKeys();

  QString message;
  QString notification;
  Qgis::MessageLevel level = Qgis::MessageLevel::Info;
  OperationUiState state = OperationUiState::Failed;
  if ( successful && result.ok )
  {
    if ( operation == EncryptOperation )
      message = tr( "已加密 %1 个数据包。" ).arg( result.outputPaths.size() );
    else if ( operation == DecryptOperation )
      message = tr( "已解密 %1 个数据包。" ).arg( result.outputPaths.size() );
    else if ( operation == RekeyOperation )
      message = tr( "已为 %1 个数据包更换密钥。" ).arg( result.outputPaths.size() );
    else
      message = tr( "已创建 %1 个数据包。" ).arg( result.outputPaths.size() );
    notification = message;
    appendItemSummary( message );
    appendRetainedOutputPaths( message );
    if ( !result.sidecarPath.isEmpty() )
    {
      message += QLatin1Char( '\n' ) + tr( "密钥附属文件：%1" ).arg( QDir::toNativeSeparators( result.sidecarPath ) );
      message += QLatin1Char( '\n' ) + tr( "警告：该附属文件包含可直接读取的加密密钥。请妥善保存，并仅通过安全方式共享。" );
    }
    if ( skippedCount > 0 || failedCount > 0 )
    {
      state = OperationUiState::PartiallySucceeded;
      level = Qgis::MessageLevel::Warning;
      notification += tr( " %1 项跳过，%2 项失败。" ).arg( skippedCount ).arg( failedCount );
    }
    else
    {
      state = OperationUiState::Succeeded;
      level = Qgis::MessageLevel::Success;
    }
  }
  else if ( result.canceled )
  {
    message = result.outputPaths.isEmpty()
      ? tr( "数据包操作已取消，未保留最终输出。" )
      : tr( "数据包操作已取消，已保留 %1 个完成的数据包。" ).arg( result.outputPaths.size() );
    notification = message;
    appendItemSummary( message );
    if ( !result.sidecarPath.isEmpty() )
      message += QLatin1Char( '\n' ) + tr( "密钥附属文件：%1" ).arg( QDir::toNativeSeparators( result.sidecarPath ) );
    if ( !result.error.isEmpty() && result.outputPaths.isEmpty() && result.error != QLatin1String( "数据包操作已取消。" ) )
      message += QLatin1Char( '\n' ) + result.error;
    appendRetainedOutputPaths( message );
    state = OperationUiState::Canceled;
    level = result.outputPaths.isEmpty() && failedCount == 0 ? Qgis::MessageLevel::Info : Qgis::MessageLevel::Warning;
  }
  else
  {
    message = result.error.isEmpty() ? tr( "数据包操作失败。" ) : result.error;
    notification = message;
    appendItemSummary( message );
    appendRetainedOutputPaths( message );
    state = OperationUiState::Failed;
    level = Qgis::MessageLevel::Critical;
  }

  setOperationUiState( state, notification );
  setResultDetails( message, result.sidecarPath );
  applyDeferredSuggestedSource();
  emit messageRequested( tr( "MTPL 数据包工具" ), notification, level );
  if ( !result.outputPaths.isEmpty() )
    emit outputsCommitted( result.outputPaths );
}
