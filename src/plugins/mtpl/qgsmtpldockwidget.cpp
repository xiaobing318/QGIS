/***************************************************************************
  qgsmtpldockwidget.cpp
  ---------------------
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

#include "qgsmtpldockwidget.h"

#include "qgsmtplpathwidget.h"
#include "dialogs/qgsmtplpackagetoolswidget.h"
#include "qgsmtplpluginlayer.h"
#include "services/qgsmtplcredentialstore.h"
#include "services/qgsmtplkeysidecar.h"
#include "services/qgsmtplpackageoperation.h"
#include "services/qgsmtplpackageservice.h"
#include "services/qgsmtplstatus.h"

#include "qgslayertree.h"
#include "qgsapplication.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsmaplayer.h"
#include "qgspasswordlineedit.h"
#include "qgsproject.h"
#include "qgsprovidermetadata.h"
#include "qgsproviderregistry.h"
#include "qgsprovidersublayerdetails.h"
#include "qgsvectorlayer.h"

#include <mtpl/mtpl.h>
extern "C"
{
#include "sfp_internal.h"
}

#include <QAbstractItemView>
#include <QAction>
#include <QBuffer>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QFormLayout>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHideEvent>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMutex>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStorageInfo>
#include <QTableWidget>
#include <QTabWidget>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "moc_qgsmtpldockwidget.cpp"

namespace
{

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

QString humanSize( quint64 bytes )
{
  static const char *suffixes[] = { "B", "KiB", "MiB", "GiB", "TiB" };
  double value = static_cast<double>( bytes );
  int suffix = 0;
  while ( value >= 1024.0 && suffix < 4 )
  {
    value /= 1024.0;
    ++suffix;
  }
  return suffix == 0 ? QStringLiteral( "%1 %2" ).arg( bytes ).arg( QString::fromLatin1( suffixes[suffix] ) )
                     : QStringLiteral( "%1 %2" ).arg( value, 0, 'f', 1 ).arg( QString::fromLatin1( suffixes[suffix] ) );
}

bool isReparseOrLink( const QFileInfo &info )
{
  if ( info.isSymLink() )
    return true;
#ifdef Q_OS_WIN
  const QString nativePath = QDir::toNativeSeparators( info.absoluteFilePath() );
  const DWORD attributes = GetFileAttributesW( reinterpret_cast<LPCWSTR>( nativePath.utf16() ) );
  return attributes != INVALID_FILE_ATTRIBUTES && ( attributes & FILE_ATTRIBUTE_REPARSE_POINT ) != 0;
#else
  return false;
#endif
}

bool isWithinRoot( const QString &rootPath, const QString &candidatePath )
{
  QString root = QDir::cleanPath( QDir::fromNativeSeparators( rootPath ) );
  QString candidate = QDir::cleanPath( QDir::fromNativeSeparators( candidatePath ) );
#ifdef Q_OS_WIN
  if ( candidate.compare( root, Qt::CaseInsensitive ) == 0 )
    return true;
#else
  if ( candidate == root )
    return true;
#endif
  if ( !root.endsWith( QLatin1Char( '/' ) ) )
    root.append( QLatin1Char( '/' ) );
#ifdef Q_OS_WIN
  return candidate.startsWith( root, Qt::CaseInsensitive );
#else
  return candidate.startsWith( root, Qt::CaseSensitive );
#endif
}

bool isReservedWindowsName( const QString &component )
{
  const QString stem = component.section( QLatin1Char( '.' ), 0, 0 ).toUpper();
  if ( stem == QLatin1String( "CON" ) || stem == QLatin1String( "PRN" ) ||
       stem == QLatin1String( "AUX" ) || stem == QLatin1String( "NUL" ) )
    return true;
  if ( stem.size() == 4 && ( stem.startsWith( QLatin1String( "COM" ) ) || stem.startsWith( QLatin1String( "LPT" ) ) ) )
    return stem.at( 3 ) >= QLatin1Char( '1' ) && stem.at( 3 ) <= QLatin1Char( '9' );
  return false;
}

bool normalizedSafeSfpPath( const QString &path, QString &normalized )
{
  normalized.clear();
  if ( path.isEmpty() || path.contains( QLatin1Char( '\\' ) ) || path.contains( QLatin1Char( ':' ) ) ||
       QDir::isAbsolutePath( path ) || path.startsWith( QLatin1Char( '/' ) ) )
    return false;

  const QString cleaned = QDir::cleanPath( path );
  if ( cleaned != path || cleaned == QLatin1String( "." ) || cleaned.startsWith( QLatin1String( "../" ) ) )
    return false;

  const QStringList components = cleaned.split( QLatin1Char( '/' ), Qt::KeepEmptyParts );
  for ( const QString &component : components )
  {
    if ( component.isEmpty() || component == QLatin1String( "." ) || component == QLatin1String( ".." ) ||
         component.endsWith( QLatin1Char( '.' ) ) || component.endsWith( QLatin1Char( ' ' ) ) ||
         isReservedWindowsName( component ) )
      return false;
    for ( const QChar character : component )
    {
      if ( character.isNull() || character.unicode() < 0x20 )
        return false;
    }
  }

  normalized = cleaned;
  return true;
}

bool isShapefileSidecar( const QString &entryPath )
{
  const QFileInfo info( entryPath );
  if ( info.fileName().endsWith( QLatin1String( ".shp.xml" ), Qt::CaseInsensitive ) )
    return true;

  static const QSet<QString> extensions = {
    QStringLiteral( "shx" ), QStringLiteral( "dbf" ), QStringLiteral( "prj" ), QStringLiteral( "cpg" ),
    QStringLiteral( "qix" ), QStringLiteral( "sbn" ), QStringLiteral( "sbx" )
  };
  return extensions.contains( info.suffix().toLower() );
}

QString shapefileDatasetKey( const QString &entryPath )
{
  QString normalized = QDir::fromNativeSeparators( entryPath );
  if ( normalized.endsWith( QLatin1String( ".shp.xml" ), Qt::CaseInsensitive ) )
    normalized.chop( 8 );
  else
    normalized = QDir( QFileInfo( normalized ).path() ).filePath( QFileInfo( normalized ).completeBaseName() );
  return QDir::cleanPath( normalized ).toCaseFolded();
}

bool isShapefileCompanion( const QString &selectedPath, const QString &candidatePath )
{
  const QFileInfo selectedInfo( selectedPath );
  const QFileInfo candidateInfo( candidatePath );
  if ( selectedInfo.path() != candidateInfo.path() )
    return false;

  const QString baseName = selectedInfo.completeBaseName();
  const QString candidateName = candidateInfo.fileName();
  if ( candidateName.compare( baseName + QStringLiteral( ".shp.xml" ), Qt::CaseInsensitive ) == 0 )
    return true;
  if ( candidateInfo.completeBaseName().compare( baseName, Qt::CaseInsensitive ) != 0 )
    return false;

  static const QSet<QString> extensions = {
    QStringLiteral( "shp" ), QStringLiteral( "shx" ), QStringLiteral( "dbf" ), QStringLiteral( "prj" ),
    QStringLiteral( "cpg" ), QStringLiteral( "qix" ), QStringLiteral( "sbn" ), QStringLiteral( "sbx" )
  };
  return extensions.contains( candidateInfo.suffix().toLower() );
}

bool isRasterDatasetEntry( const QString &entryPath )
{
  static const QSet<QString> extensions = {
    QStringLiteral( "jpg" ), QStringLiteral( "jpeg" ), QStringLiteral( "png" )
  };
  return extensions.contains( QFileInfo( entryPath ).suffix().toLower() );
}

bool isRasterCompanion( const QString &selectedPath, const QString &candidatePath )
{
  const QFileInfo selectedInfo( selectedPath );
  const QFileInfo candidateInfo( candidatePath );
  if ( selectedInfo.path() != candidateInfo.path() || !isRasterDatasetEntry( selectedPath ) )
    return false;

  if ( candidateInfo.fileName().compare( selectedInfo.fileName() + QStringLiteral( ".aux.xml" ), Qt::CaseInsensitive ) == 0 )
    return true;
  if ( candidateInfo.completeBaseName().compare( selectedInfo.completeBaseName(), Qt::CaseInsensitive ) != 0 )
    return false;

  const QString selectedExtension = selectedInfo.suffix().toLower();
  const QString candidateExtension = candidateInfo.suffix().toLower();
  if ( candidateExtension == QLatin1String( "wld" ) || candidateExtension == QLatin1String( "prj" ) )
    return true;
  if ( selectedExtension == QLatin1String( "png" ) )
    return candidateExtension == QLatin1String( "pgw" ) || candidateExtension == QLatin1String( "pngw" );
  return candidateExtension == QLatin1String( "jgw" ) || candidateExtension == QLatin1String( "jpgw" ) ||
         candidateExtension == QLatin1String( "jpegw" );
}

QString normalizedSfpPathKey( const QString &path )
{
  return QDir::cleanPath( QDir::fromNativeSeparators( path ) ).toCaseFolded();
}

QStringList rasterCompanionPathKeys( const QString &datasetPath )
{
  const QFileInfo datasetInfo( datasetPath );
  const QDir directory( datasetInfo.path() );
  const QString baseName = datasetInfo.completeBaseName();
  QStringList names = {
    datasetInfo.fileName() + QStringLiteral( ".aux.xml" ),
    baseName + QStringLiteral( ".wld" ),
    baseName + QStringLiteral( ".prj" )
  };

  if ( datasetInfo.suffix().compare( QLatin1String( "png" ), Qt::CaseInsensitive ) == 0 )
  {
    names << baseName + QStringLiteral( ".pgw" )
          << baseName + QStringLiteral( ".pngw" );
  }
  else
  {
    names << baseName + QStringLiteral( ".jgw" )
          << baseName + QStringLiteral( ".jpgw" )
          << baseName + QStringLiteral( ".jpegw" );
  }

  QStringList keys;
  keys.reserve( names.size() );
  for ( const QString &name : std::as_const( names ) )
    keys.append( normalizedSfpPathKey( directory.filePath( name ) ) );
  keys.removeDuplicates();
  return keys;
}

struct SfpEntryRecord
{
  QByteArray packagePath;
  QString safePath;
  quint64 logicalSize = 0;
  quint64 storedSize = 0;
  bool encrypted = false;
};

struct SfpReaderCloser
{
  void operator()( mtpl_sfp_reader_t *reader ) const
  {
    if ( reader )
      mtpl_sfp_reader_close( reader );
  }
};

enum class PackageTreeItemKind
{
  Package,
  SfpEntry,
  Folder,
  Companion,
  Informational
};

constexpr int PACKAGE_INDEX_ROLE = Qt::UserRole;
constexpr int SFP_ENTRY_PATH_ROLE = Qt::UserRole + 1;
constexpr int ITEM_KIND_ROLE = Qt::UserRole + 2;
constexpr int ITEM_READY_ROLE = Qt::UserRole + 3;
constexpr int SFP_ENTRY_ENCRYPTED_ROLE = Qt::UserRole + 4;
constexpr int SFP_ENTRY_LOGICAL_SIZE_ROLE = Qt::UserRole + 5;
constexpr int SFP_ENTRY_STORED_SIZE_ROLE = Qt::UserRole + 6;

QString metadataValueText( const QVariant &value )
{
  auto jsonText = []( const QJsonValue &json ) -> QString
  {
    if ( json.isObject() )
      return QString::fromUtf8( QJsonDocument( json.toObject() ).toJson( QJsonDocument::Indented ) ).trimmed();
    if ( json.isArray() )
      return QString::fromUtf8( QJsonDocument( json.toArray() ).toJson( QJsonDocument::Indented ) ).trimmed();
    return QString();
  };

  if ( !value.isValid() || value.isNull() )
    return QObject::tr( "（空值）" );

  if ( value.metaType().id() == QMetaType::QVariantMap || value.metaType().id() == QMetaType::QVariantList )
    return jsonText( QJsonValue::fromVariant( value ) );

  if ( value.metaType().id() == QMetaType::QByteArray )
  {
    const QByteArray bytes = value.toByteArray();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson( bytes, &parseError );
    if ( parseError.error == QJsonParseError::NoError && ( document.isObject() || document.isArray() ) )
      return QString::fromUtf8( document.toJson( QJsonDocument::Indented ) ).trimmed();

    const QString text = QString::fromUtf8( bytes );
    if ( text.toUtf8() == bytes && !text.contains( QChar::Null ) )
      return text;
    return QObject::tr( "二进制数据 · %1 字节 · 前缀 %2" )
      .arg( bytes.size() )
      .arg( QString::fromLatin1( bytes.left( 16 ).toHex( ' ' ) ) );
  }

  const QString text = value.toString();
  const QByteArray candidate = text.trimmed().toUtf8();
  if ( candidate.startsWith( '{' ) || candidate.startsWith( '[' ) )
  {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson( candidate, &parseError );
    if ( parseError.error == QJsonParseError::NoError && ( document.isObject() || document.isArray() ) )
      return QString::fromUtf8( document.toJson( QJsonDocument::Indented ) ).trimmed();
  }
  return text;
}

QString keyStatusText( const QgsMtpl::PackageDescriptor &descriptor )
{
  switch ( descriptor.readiness )
  {
    case QgsMtpl::ReadinessState::PlainReady:
      return QObject::tr( "无需密钥" );
    case QgsMtpl::ReadinessState::KeyVerified:
      if ( descriptor.credentialSource == QgsMtpl::CredentialSource::Sidecar )
        return QObject::tr( "已验证 · 配套密钥文件" );
      if ( descriptor.credentialSource == QgsMtpl::CredentialSource::Remembered )
        return QObject::tr( "已验证 · 已记住" );
      return QObject::tr( "已验证 · 手动输入" );
    case QgsMtpl::ReadinessState::KeyRequired:
      return QObject::tr( "需要密钥" );
    case QgsMtpl::ReadinessState::KeyRejectedOrCorrupt:
      return QObject::tr( "验证失败" );
    case QgsMtpl::ReadinessState::UnverifiableEmpty:
      return QObject::tr( "无法验证" );
    case QgsMtpl::ReadinessState::Unsupported:
    case QgsMtpl::ReadinessState::Unreadable:
      return QObject::tr( "不可用" );
  }
  return QObject::tr( "不可用" );
}

QString readyStateText( const QgsMtpl::PackageDescriptor &descriptor )
{
  switch ( descriptor.readiness )
  {
    case QgsMtpl::ReadinessState::PlainReady:
    case QgsMtpl::ReadinessState::KeyVerified:
      return QObject::tr( "就绪" );
    case QgsMtpl::ReadinessState::KeyRequired:
      return QObject::tr( "需要密钥" );
    case QgsMtpl::ReadinessState::KeyRejectedOrCorrupt:
      return QObject::tr( "密钥不匹配，或数据包已损坏" );
    case QgsMtpl::ReadinessState::UnverifiableEmpty:
      return QObject::tr( "空包，无法验证密钥" );
    case QgsMtpl::ReadinessState::Unsupported:
      return QObject::tr( "不支持" );
    case QgsMtpl::ReadinessState::Unreadable:
      return QObject::tr( "无法读取" );
  }
  return QObject::tr( "无法读取" );
}

QString payloadTypeText( QgsMtpl::PayloadType payload )
{
  switch ( payload )
  {
    case QgsMtpl::PayloadType::RasterImage:
      return QObject::tr( "栅格图像" );
    case QgsMtpl::PayloadType::VectorTile:
      return QObject::tr( "矢量瓦片" );
    case QgsMtpl::PayloadType::Elevation:
      return QObject::tr( "高程数据" );
    case QgsMtpl::PayloadType::Files:
      return QObject::tr( "文件" );
    case QgsMtpl::PayloadType::Unknown:
      return QObject::tr( "未知" );
  }
  return QObject::tr( "未知" );
}

QString encryptionStateText( QgsMtpl::EncryptionState state )
{
  switch ( state )
  {
    case QgsMtpl::EncryptionState::Plain:
      return QObject::tr( "未加密" );
    case QgsMtpl::EncryptionState::Encrypted:
      return QObject::tr( "已加密" );
    case QgsMtpl::EncryptionState::Locked:
      return QObject::tr( "已锁定" );
    case QgsMtpl::EncryptionState::Unknown:
      return QObject::tr( "未知" );
  }
  return QObject::tr( "未知" );
}

bool validateEnteredKeys( const QgsMtpl::CryptoKeys &keys, QString &error )
{
  return keys.isValid( &error );
}

bool isPotentialGisDatasetEntry( const QString &entryPath )
{
  static const QSet<QString> extensions = {
    QStringLiteral( "shp" ), QStringLiteral( "geojson" ), QStringLiteral( "json" ),
    QStringLiteral( "gpkg" ), QStringLiteral( "fgb" ), QStringLiteral( "kml" ),
    QStringLiteral( "gml" ), QStringLiteral( "gpx" ), QStringLiteral( "csv" ),
    QStringLiteral( "tab" ), QStringLiteral( "mif" ), QStringLiteral( "dxf" ),
    QStringLiteral( "sqlite" ), QStringLiteral( "mbtiles" ), QStringLiteral( "tif" ),
    QStringLiteral( "tiff" ), QStringLiteral( "vrt" ), QStringLiteral( "img" ),
    QStringLiteral( "asc" ), QStringLiteral( "dem" ), QStringLiteral( "png" ),
    QStringLiteral( "jpg" ), QStringLiteral( "jpeg" ), QStringLiteral( "webp" ),
    QStringLiteral( "jp2" ), QStringLiteral( "grd" ), QStringLiteral( "nc" )
  };
  return extensions.contains( QFileInfo( entryPath ).suffix().toLower() );
}

using IoCancelCheck = std::function<bool()>;
using IoProgressCallback = std::function<void( double )>;

struct SfpFileWriteContext
{
  QSaveFile *output = nullptr;
  const IoCancelCheck *cancelCheck = nullptr;
  const IoProgressCallback *progress = nullptr;
  qint64 written = 0;
  qint64 expected = 0;
};

mtpl_status_t writeSfpFileChunk( mtpl_buffer_view_t data, void *userData )
{
  auto *context = static_cast<SfpFileWriteContext *>( userData );
  if ( !context || !context->output )
    return MTPL_STATUS_INVALID_ARGUMENT;
  if ( context->cancelCheck && *context->cancelCheck && ( *context->cancelCheck )() )
    return MTPL_STATUS_CANCELED;
  if ( data.size > static_cast<size_t>( std::numeric_limits<qint64>::max() ) )
    return MTPL_STATUS_LIMIT_EXCEEDED;

  const qint64 chunkSize = static_cast<qint64>( data.size );
  if ( context->written < 0 || context->expected < context->written ||
       chunkSize > context->expected - context->written )
    return MTPL_STATUS_CORRUPT_DATA;
  if ( chunkSize > 0 && context->output->write( reinterpret_cast<const char *>( data.data ), chunkSize ) != chunkSize )
    return MTPL_STATUS_IO_ERROR;

  context->written += chunkSize;
  if ( context->progress && *context->progress )
  {
    const double fraction = context->expected == 0
                              ? 1.0
                              : static_cast<double>( context->written ) / static_cast<double>( context->expected );
    ( *context->progress )( fraction );
  }
  return context->cancelCheck && *context->cancelCheck && ( *context->cancelCheck )()
           ? MTPL_STATUS_CANCELED
           : MTPL_STATUS_OK;
}

struct SfpPreviewReadContext
{
  QByteArray *bytes = nullptr;
  const IoCancelCheck *cancelCheck = nullptr;
  qsizetype limit = 0;
  bool stopAtLimit = false;
  bool userCanceled = false;
  bool limitReached = false;
};

mtpl_status_t collectSfpPreviewChunk( mtpl_buffer_view_t data, void *userData )
{
  auto *context = static_cast<SfpPreviewReadContext *>( userData );
  if ( !context || !context->bytes || context->limit < 0 )
    return MTPL_STATUS_INVALID_ARGUMENT;
  if ( context->cancelCheck && *context->cancelCheck && ( *context->cancelCheck )() )
  {
    context->userCanceled = true;
    return MTPL_STATUS_CANCELED;
  }

  const qsizetype remaining = context->limit - context->bytes->size();
  if ( remaining < 0 )
    return MTPL_STATUS_CORRUPT_DATA;
  const size_t accepted = std::min( data.size, static_cast<size_t>( remaining ) );
  if ( accepted > 0 )
    context->bytes->append( reinterpret_cast<const char *>( data.data ), static_cast<qsizetype>( accepted ) );
  if ( accepted != data.size || ( context->stopAtLimit && context->bytes->size() == context->limit ) )
  {
    context->limitReached = true;
    return MTPL_STATUS_CANCELED;
  }
  if ( context->cancelCheck && *context->cancelCheck && ( *context->cancelCheck )() )
  {
    context->userCanceled = true;
    return MTPL_STATUS_CANCELED;
  }
  return MTPL_STATUS_OK;
}

struct MtplTileLoadInput
{
  QgsMtpl::PackageDescriptor descriptor;
  QgsMtpl::CryptoKeys suppliedKeys;
  QgsMtpl::CredentialSource suppliedSource = QgsMtpl::CredentialSource::None;
};

struct MtplSfpLoadInput
{
  QgsMtpl::PackageDescriptor descriptor;
  QString entryPath;
  QgsMtpl::CryptoKeys suppliedKeys;
};

struct MtplLoadPreparationRequest
{
  QList<MtplTileLoadInput> tilePackages;
  QList<MtplSfpLoadInput> sfpEntries;
  QString cacheRoot;
};

struct MtplPreparedTile
{
  QgsMtpl::PackageDescriptor descriptor;
  QgsMtpl::CryptoKeys keys;
  QString error;
  bool ok = false;
};

struct MtplPreparedSfpEntry
{
  QString entryPath;
  QString extractedPath;
  QString cacheDirectory;
  QString error;
  bool ok = false;
  bool canceled = false;
};

struct MtplLoadPreparationResult
{
  QList<MtplPreparedTile> tilePackages;
  QList<MtplPreparedSfpEntry> sfpEntries;
  bool canceled = false;
};

struct MtplPreviewResult
{
  QByteArray bytes;
  QImage image;
  quint64 logicalSize = 0;
  QString error;
  bool ok = false;
  bool canceled = false;
};

QMutex sMtplIoTaskMutex;
QSet<QgsTask *> sActiveMtplIoTasks;

void registerMtplIoTask( QgsTask *task )
{
  QMutexLocker locker( &sMtplIoTaskMutex );
  sActiveMtplIoTasks.insert( task );
}

void unregisterMtplIoTask( QgsTask *task )
{
  QMutexLocker locker( &sMtplIoTaskMutex );
  sActiveMtplIoTasks.remove( task );
}

void cancelAndWaitForAllMtplIoTasks()
{
  while ( true )
  {
    QList<QPointer<QgsTask>> tasks;
    {
      QMutexLocker locker( &sMtplIoTaskMutex );
      tasks.reserve( sActiveMtplIoTasks.size() );
      for ( QgsTask *task : std::as_const( sActiveMtplIoTasks ) )
        tasks.append( task );
    }
    if ( tasks.isEmpty() )
      return;

    for ( const QPointer<QgsTask> &task : std::as_const( tasks ) )
    {
      if ( task )
        task->cancel();
    }
    for ( const QPointer<QgsTask> &task : std::as_const( tasks ) )
    {
      if ( task )
        task->waitForFinished( 0 );
    }
    QCoreApplication::sendPostedEvents( nullptr, 0 );
    QCoreApplication::sendPostedEvents( nullptr, QEvent::DeferredDelete );
  }
}

void wipeSidecarKeys( QgsMtpl::KeySidecar &sidecar )
{
  sidecar.keys.privateKeyBase64.fill( '\0' );
  sidecar.keys.deviceKeyHex.fill( '\0' );
  sidecar.keys.privateKeyBase64.clear();
  sidecar.keys.deviceKeyHex.clear();
}

bool resolveDescriptorKeys( const QgsMtpl::PackageDescriptor &descriptor,
                            const QgsMtpl::CryptoKeys &suppliedKeys,
                            const IoCancelCheck &cancelCheck,
                            QgsMtpl::CryptoKeys &keys,
                            QString &error )
{
  keys.clear();
  error.clear();
  if ( cancelCheck && cancelCheck() )
  {
    error = QStringLiteral( "操作已取消。" );
    return false;
  }
  if ( descriptor.encryption == QgsMtpl::EncryptionState::Plain )
    return true;

  if ( descriptor.credentialSource == QgsMtpl::CredentialSource::Sidecar )
  {
    QgsMtpl::KeySidecar sidecar;
    if ( descriptor.sidecarPath.isEmpty() ||
         !QgsMtpl::KeySidecarStore::readForPackage( descriptor.sidecarPath, descriptor.path, sidecar, error, cancelCheck ) )
    {
      wipeSidecarKeys( sidecar );
      if ( error.isEmpty() )
        error = QStringLiteral( "无法读取或验证数据包的密钥附属文件。" );
      return false;
    }
    keys = sidecar.keys.cryptoKeys();
    wipeSidecarKeys( sidecar );
  }
  else
  {
    keys = suppliedKeys;
  }

  if ( cancelCheck && cancelCheck() )
  {
    keys.clear();
    error = QStringLiteral( "操作已取消。" );
    return false;
  }
  if ( !keys.isValid( &error ) )
  {
    keys.clear();
    if ( error.isEmpty() )
      error = QStringLiteral( "数据包需要有效的密钥。" );
    return false;
  }
  return true;
}

void preserveDisplaySettings( const QgsMtpl::PackageDescriptor &source, QgsMtpl::PackageDescriptor &target )
{
  target.displayName = source.displayName;
  target.stylePath = source.stylePath;
  if ( !source.displayOverridesApplied )
    return;

  target.displayOverridesApplied = true;
  target.payload = source.payload;
  target.crsAuthId = source.crsAuthId;
  target.scheme = source.scheme;
  target.scale = source.scale;
  target.offset = source.offset;
  target.hasNoData = source.hasNoData;
  target.noData = source.noData;
  if ( source.format == QgsMtpl::PackageFormat::Dtp )
  {
    target.metadata.insert( QStringLiteral( "dataType" ), source.metadata.value( QStringLiteral( "dataType" ) ) );
    target.metadata.insert( QStringLiteral( "endianness" ), source.metadata.value( QStringLiteral( "endianness" ) ) );
  }
}

bool packageStampMatches( const QgsMtpl::PackageDescriptor &descriptor )
{
  const QFileInfo info( descriptor.path );
  if ( !info.isFile() )
    return false;
  if ( descriptor.fileSize > 0 && static_cast<quint64>( info.size() ) != descriptor.fileSize )
    return false;
  if ( descriptor.fileLastModifiedMs >= 0 && info.lastModified().toMSecsSinceEpoch() != descriptor.fileLastModifiedMs )
    return false;
  return true;
}

bool samePath( const QString &left, const QString &right )
{
#ifdef Q_OS_WIN
  return QDir::cleanPath( left ).compare( QDir::cleanPath( right ), Qt::CaseInsensitive ) == 0;
#else
  return QDir::cleanPath( left ) == QDir::cleanPath( right );
#endif
}

void removePreparedDirectory( const QString &cacheRoot, const QString &directory )
{
  if ( cacheRoot.isEmpty() || directory.isEmpty() || samePath( cacheRoot, directory ) ||
       !isWithinRoot( cacheRoot, QFileInfo( directory ).absoluteFilePath() ) )
    return;
  QDir( directory ).removeRecursively();
}

MtplPreparedSfpEntry extractSfpDataset( const QgsMtpl::PackageDescriptor &descriptor,
                                        const QString &entryPath,
                                        const QgsMtpl::CryptoKeys &keys,
                                        const QString &cacheRoot,
                                        const IoCancelCheck &cancelCheck,
                                        const IoProgressCallback &progress )
{
  MtplPreparedSfpEntry result;
  result.entryPath = entryPath;
  const auto canceled = [&cancelCheck]() { return cancelCheck && cancelCheck(); };
  const auto reportProgress = [&progress]( double value )
  {
    if ( progress )
      progress( std::clamp( value, 0.0, 100.0 ) );
  };
  const auto cancelResult = [&result, &cacheRoot]()
  {
    result.canceled = true;
    result.error = QStringLiteral( "SFP 数据集加载已取消。" );
    removePreparedDirectory( cacheRoot, result.cacheDirectory );
    result.extractedPath.clear();
    result.cacheDirectory.clear();
  };

  reportProgress( 0.0 );
  if ( canceled() )
  {
    cancelResult();
    return result;
  }

  QString selectedSafePath;
  if ( !normalizedSafeSfpPath( entryPath, selectedSafePath ) )
  {
    result.error = QStringLiteral( "所选 SFP 条目的路径不安全。" );
    return result;
  }

  const QFileInfo cacheInfo( cacheRoot );
  const QString canonicalCacheRoot = cacheInfo.canonicalFilePath();
  if ( canonicalCacheRoot.isEmpty() || !cacheInfo.isDir() || isReparseOrLink( cacheInfo ) )
  {
    result.error = QStringLiteral( "临时缓存已不再安全，无法继续使用。" );
    return result;
  }

  mtpl_crypto_options_t crypto = {};
  const mtpl_crypto_options_t *cryptoPtr = nullptr;
  if ( keys.isValid() )
  {
    crypto.private_key = reinterpret_cast<const uint8_t *>( keys.privateKey.constData() );
    crypto.private_key_size = static_cast<size_t>( keys.privateKey.size() );
    crypto.device_key = reinterpret_cast<const uint8_t *>( keys.deviceKey.constData() );
    crypto.device_key_size = static_cast<size_t>( keys.deviceKey.size() );
    cryptoPtr = &crypto;
  }

  mtpl_sfp_reader_t *rawReader = nullptr;
  const QByteArray packageFilePath = descriptor.path.toUtf8();
  if ( mtpl_sfp_reader_open( packageFilePath.constData(), cryptoPtr, &rawReader ) != MTPL_STATUS_OK )
  {
    result.error = QStringLiteral( "无法使用当前密钥打开 SFP 数据包。" );
    return result;
  }
  std::unique_ptr<mtpl_sfp_reader_t, SfpReaderCloser> reader( rawReader );

  size_t entryCount = 0;
  if ( mtpl_sfp_reader_get_entry_count( reader.get(), &entryCount ) != MTPL_STATUS_OK )
  {
    result.error = QStringLiteral( "无法读取 SFP 条目列表。" );
    return result;
  }

  const bool includeShapefileCompanions = QFileInfo( selectedSafePath ).suffix().compare( QLatin1String( "shp" ), Qt::CaseInsensitive ) == 0;
  const bool includeRasterCompanions = isRasterDatasetEntry( selectedSafePath );
  QList<SfpEntryRecord> entries;
  QSet<QString> outputPaths;
  quint64 totalLogicalSize = 0;
  bool foundSelectedEntry = false;
  for ( size_t index = 0; index < entryCount; ++index )
  {
    if ( canceled() )
    {
      cancelResult();
      return result;
    }
    reportProgress( 5.0 + 25.0 * static_cast<double>( index ) / static_cast<double>( std::max<size_t>( entryCount, 1 ) ) );
    mtpl_sfp_entry_info_t entry = {};
    if ( mtpl_sfp_reader_get_entry_info( reader.get(), index, &entry ) != MTPL_STATUS_OK || !entry.path )
    {
      result.error = QStringLiteral( "SFP 条目列表无效。" );
      return result;
    }

    const QByteArray rawEntryPath( entry.path );
    const QString decodedEntryPath = QString::fromUtf8( rawEntryPath );
    QString safeEntryPath;
    const bool pathIsSafe = decodedEntryPath.toUtf8() == rawEntryPath && normalizedSafeSfpPath( decodedEntryPath, safeEntryPath );
    const bool isSelectedEntry = pathIsSafe && safeEntryPath == selectedSafePath;
    const bool isDatasetCompanion = pathIsSafe &&
                                    ( ( includeShapefileCompanions && isShapefileCompanion( selectedSafePath, safeEntryPath ) ) ||
                                      ( includeRasterCompanions && isRasterCompanion( selectedSafePath, safeEntryPath ) ) );
    if ( !isSelectedEntry && !isDatasetCompanion )
      continue;
    if ( !pathIsSafe )
    {
      result.error = QStringLiteral( "所选 SFP 条目包含不安全的路径。" );
      return result;
    }

    const QString foldedOutputPath = safeEntryPath.toCaseFolded();
    if ( outputPaths.contains( foldedOutputPath ) )
    {
      result.error = QStringLiteral( "所选 SFP 数据集中存在冲突的文件名。" );
      return result;
    }
    outputPaths.insert( foldedOutputPath );
    if ( std::numeric_limits<quint64>::max() - totalLogicalSize < entry.logical_size )
    {
      result.error = QStringLiteral( "所选 SFP 数据集的总大小无效。" );
      return result;
    }
    totalLogicalSize += entry.logical_size;

    SfpEntryRecord record;
    record.packagePath = rawEntryPath;
    record.safePath = safeEntryPath;
    record.logicalSize = entry.logical_size;
    entries.append( record );
    foundSelectedEntry = foundSelectedEntry || isSelectedEntry;
  }

  if ( !foundSelectedEntry )
  {
    result.error = QStringLiteral( "所选 SFP 条目已不存在。" );
    return result;
  }
  std::sort( entries.begin(), entries.end(), []( const SfpEntryRecord &left, const SfpEntryRecord &right )
  {
    return left.safePath < right.safePath;
  } );

  const QStorageInfo cacheStorage( canonicalCacheRoot );
  if ( cacheStorage.isValid() && cacheStorage.isReady() && cacheStorage.bytesAvailable() >= 0 &&
       totalLogicalSize > static_cast<quint64>( cacheStorage.bytesAvailable() ) )
  {
    result.error = QStringLiteral( "临时缓存的可用空间不足，无法存放此 SFP 数据集。" );
    return result;
  }

  const QString actionName = QUuid::createUuid().toString( QUuid::WithoutBraces );
  QDir cacheRootDir( canonicalCacheRoot );
  if ( !cacheRootDir.mkdir( actionName ) )
  {
    result.error = QStringLiteral( "无法创建临时数据集目录。" );
    return result;
  }
  result.cacheDirectory = cacheRootDir.filePath( actionName );
  const QFileInfo actionInfo( result.cacheDirectory );
  const QString actionRoot = actionInfo.canonicalFilePath();
  if ( actionRoot.isEmpty() || !actionInfo.isDir() || isReparseOrLink( actionInfo ) || !isWithinRoot( canonicalCacheRoot, actionRoot ) )
  {
    removePreparedDirectory( canonicalCacheRoot, result.cacheDirectory );
    result.cacheDirectory.clear();
    result.error = QStringLiteral( "临时数据集目录不安全，无法使用。" );
    return result;
  }

  for ( qsizetype entryIndex = 0; entryIndex < entries.size(); ++entryIndex )
  {
    if ( canceled() )
    {
      cancelResult();
      return result;
    }
    const SfpEntryRecord &entry = entries.at( entryIndex );
    reportProgress( 30.0 + 70.0 * static_cast<double>( entryIndex ) / static_cast<double>( std::max<qsizetype>( entries.size(), 1 ) ) );
    const QString targetPath = QDir( actionRoot ).filePath( entry.safePath );
    const QFileInfo targetInfo( targetPath );
    if ( !isWithinRoot( actionRoot, targetInfo.absoluteFilePath() ) || targetInfo.exists() || targetInfo.isSymLink() )
    {
      result.error = QStringLiteral( "无法将 SFP 条目安全映射到临时缓存。" );
      removePreparedDirectory( canonicalCacheRoot, result.cacheDirectory );
      result.cacheDirectory.clear();
      return result;
    }

    if ( !QDir().mkpath( targetInfo.absolutePath() ) )
    {
      result.error = QStringLiteral( "无法创建临时数据集目录。" );
      removePreparedDirectory( canonicalCacheRoot, result.cacheDirectory );
      result.cacheDirectory.clear();
      return result;
    }
    const QFileInfo parentInfo( targetInfo.absolutePath() );
    const QString parentCanonicalPath = parentInfo.canonicalFilePath();
    if ( parentCanonicalPath.isEmpty() || !parentInfo.isDir() || isReparseOrLink( parentInfo ) || !isWithinRoot( actionRoot, parentCanonicalPath ) )
    {
      result.error = QStringLiteral( "临时数据集目录未通过安全检查。" );
      removePreparedDirectory( canonicalCacheRoot, result.cacheDirectory );
      result.cacheDirectory.clear();
      return result;
    }

    if ( entry.logicalSize > static_cast<quint64>( std::numeric_limits<qint64>::max() ) )
    {
      result.error = QStringLiteral( "SFP 条目超过当前平台可安全写入的大小。" );
      removePreparedDirectory( canonicalCacheRoot, result.cacheDirectory );
      result.cacheDirectory.clear();
      return result;
    }

    const qint64 expectedSize = static_cast<qint64>( entry.logicalSize );
    bool committed = false;
    QString outputError;
    mtpl_status_t readStatus = MTPL_STATUS_IO_ERROR;
    {
      QSaveFile output( targetPath );
      output.setDirectWriteFallback( false );
      const bool opened = output.open( QIODevice::WriteOnly );
      if ( opened )
      {
        const IoProgressCallback streamProgress = [&reportProgress, entryIndex, entryCount = entries.size()]( double entryFraction )
        {
          reportProgress( 30.0 + 70.0 * ( static_cast<double>( entryIndex ) + entryFraction ) /
                                      static_cast<double>( std::max<qsizetype>( entryCount, 1 ) ) );
        };
        SfpFileWriteContext streamContext;
        streamContext.output = &output;
        streamContext.cancelCheck = &cancelCheck;
        streamContext.progress = &streamProgress;
        streamContext.expected = expectedSize;
        readStatus = mtpl_sfp_reader_read_file_chunks_internal(
          reader.get(), entry.packagePath.constData(), 1024 * 1024,
          writeSfpFileChunk, &streamContext );
        if ( readStatus != MTPL_STATUS_OK && output.error() != QFileDevice::NoError )
          outputError = output.errorString();
        if ( readStatus == MTPL_STATUS_OK && streamContext.written == expectedSize && !canceled() )
        {
          committed = output.commit();
          if ( !committed )
            outputError = output.errorString();
        }
        else
          output.cancelWriting();
      }
      else
      {
        outputError = output.errorString();
      }
    }
    if ( readStatus == MTPL_STATUS_CANCELED || canceled() )
    {
      cancelResult();
      return result;
    }
    if ( readStatus != MTPL_STATUS_OK || !committed )
    {
      if ( !outputError.isEmpty() )
        result.error = QStringLiteral( "无法将所选 SFP 数据集写入临时缓存：%1。" ).arg( outputError );
      else if ( readStatus != MTPL_STATUS_OK )
        result.error = QStringLiteral( "无法读取所选 SFP 数据集：%1。" ).arg( QgsMtpl::statusText( readStatus ) );
      else
        result.error = QStringLiteral( "无法将所选 SFP 数据集写入临时缓存。" );
      removePreparedDirectory( canonicalCacheRoot, result.cacheDirectory );
      result.cacheDirectory.clear();
      return result;
    }
    QFile::setPermissions( targetPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner );

    const QFileInfo writtenInfo( targetPath );
    const QString writtenCanonicalPath = writtenInfo.canonicalFilePath();
    if ( writtenCanonicalPath.isEmpty() || !writtenInfo.isFile() || isReparseOrLink( writtenInfo ) || !isWithinRoot( actionRoot, writtenCanonicalPath ) )
    {
      result.error = QStringLiteral( "已提取的 SFP 条目未通过安全检查。" );
      removePreparedDirectory( canonicalCacheRoot, result.cacheDirectory );
      result.cacheDirectory.clear();
      return result;
    }

    if ( entry.safePath == selectedSafePath )
      result.extractedPath = writtenCanonicalPath;
  }

  if ( result.extractedPath.isEmpty() )
  {
    result.error = QStringLiteral( "无法提取所选 SFP 条目。" );
    removePreparedDirectory( canonicalCacheRoot, result.cacheDirectory );
    result.cacheDirectory.clear();
    return result;
  }
  result.ok = true;
  reportProgress( 100.0 );
  return result;
}

class MtplSfpPreviewTask final : public QgsTask
{
  public:
    MtplSfpPreviewTask( const QgsMtpl::PackageDescriptor &descriptor,
                        const QString &entryPath,
                        const QgsMtpl::CryptoKeys &suppliedKeys )
      : QgsTask( QObject::tr( "读取 MTPL SFP 预览" ), QgsTask::CanCancel | QgsTask::CancelWithoutPrompt )
      , mDescriptor( descriptor )
      , mEntryPath( entryPath )
      , mSuppliedKeys( suppliedKeys )
    {
      registerMtplIoTask( this );
    }

    ~MtplSfpPreviewTask() override
    {
      mSuppliedKeys.clear();
      mResult.bytes.fill( '\0' );
      mResult.bytes.clear();
      mResult.image = QImage();
      unregisterMtplIoTask( this );
    }

    const MtplPreviewResult &result() const { return mResult; }
    bool cancellationRequested() const { return isCanceled(); }

  protected:
    bool run() override
    {
      const IoCancelCheck cancelCheck = [this]() { return isCanceled(); };
      if ( isCanceled() )
      {
        mResult.canceled = true;
        return false;
      }
      if ( !packageStampMatches( mDescriptor ) )
      {
        mResult.error = QStringLiteral( "SFP 数据包在选择后发生变化，请重新检查。" );
        return false;
      }

      QgsMtpl::CryptoKeys keys;
      if ( !resolveDescriptorKeys( mDescriptor, mSuppliedKeys, cancelCheck, keys, mResult.error ) )
      {
        mResult.canceled = isCanceled();
        mSuppliedKeys.clear();
        keys.clear();
        return false;
      }
      mSuppliedKeys.clear();
      setProgress( 5.0 );

      QString safePath;
      if ( !normalizedSafeSfpPath( mEntryPath, safePath ) )
      {
        mResult.error = QStringLiteral( "所选 SFP 条目的路径不安全。" );
        keys.clear();
        return false;
      }

      mtpl_crypto_options_t crypto = {};
      const mtpl_crypto_options_t *cryptoPtr = nullptr;
      if ( keys.isValid() )
      {
        crypto.private_key = reinterpret_cast<const uint8_t *>( keys.privateKey.constData() );
        crypto.private_key_size = static_cast<size_t>( keys.privateKey.size() );
        crypto.device_key = reinterpret_cast<const uint8_t *>( keys.deviceKey.constData() );
        crypto.device_key_size = static_cast<size_t>( keys.deviceKey.size() );
        cryptoPtr = &crypto;
      }

      mtpl_sfp_reader_t *rawReader = nullptr;
      const QByteArray packagePath = mDescriptor.path.toUtf8();
      const mtpl_status_t openStatus = mtpl_sfp_reader_open( packagePath.constData(), cryptoPtr, &rawReader );
      if ( openStatus != MTPL_STATUS_OK )
      {
        mResult.error = QStringLiteral( "无法使用当前密钥打开 SFP 数据包：%1。" ).arg( QgsMtpl::statusText( openStatus ) );
        keys.clear();
        return false;
      }
      std::unique_ptr<mtpl_sfp_reader_t, SfpReaderCloser> reader( rawReader );
      setProgress( 15.0 );

      const QByteArray entryUtf8 = safePath.toUtf8();
      const auto entryIt = std::find_if( mDescriptor.sfpEntries.cbegin(), mDescriptor.sfpEntries.cend(), [&safePath]( const QgsMtpl::SfpEntryDescriptor &entry )
      {
        return entry.path == safePath;
      } );
      if ( entryIt == mDescriptor.sfpEntries.cend() )
      {
        mResult.error = QStringLiteral( "所选 SFP 条目已不存在，请重新检查。" );
        keys.clear();
        return false;
      }

      mResult.logicalSize = entryIt->logicalSize;
      const QString suffix = QFileInfo( safePath ).suffix().toLower();
      const bool imagePreview = suffix == QLatin1String( "png" ) || suffix == QLatin1String( "jpg" ) ||
                                suffix == QLatin1String( "jpeg" ) || suffix == QLatin1String( "webp" );
      constexpr quint64 textPreviewLimit = 1024 * 1024;
      constexpr quint64 imagePreviewByteLimit = 16 * 1024 * 1024;
      if ( imagePreview && mResult.logicalSize > imagePreviewByteLimit )
      {
        mResult.error = QStringLiteral( "图像条目超过 16 MiB 安全预览上限，可直接加载为图层。" );
        keys.clear();
        return false;
      }
      const quint64 byteLimit = imagePreview ? imagePreviewByteLimit : textPreviewLimit;
      const quint64 acceptedSize = std::min( mResult.logicalSize, byteLimit );
      if ( acceptedSize > static_cast<quint64>( std::numeric_limits<qsizetype>::max() ) )
      {
        mResult.error = QStringLiteral( "SFP 预览大小超过当前平台上限。" );
        keys.clear();
        return false;
      }

      mResult.bytes.reserve( static_cast<qsizetype>( acceptedSize ) );
      SfpPreviewReadContext previewContext;
      previewContext.bytes = &mResult.bytes;
      previewContext.cancelCheck = &cancelCheck;
      previewContext.limit = static_cast<qsizetype>( acceptedSize );
      previewContext.stopAtLimit = mResult.logicalSize > acceptedSize;
      const mtpl_status_t status = mtpl_sfp_reader_read_file_chunks_internal(
        reader.get(), entryUtf8.constData(), 256 * 1024,
        collectSfpPreviewChunk, &previewContext );
      keys.clear();
      if ( previewContext.userCanceled || isCanceled() )
      {
        mResult.canceled = true;
        mResult.error = QStringLiteral( "SFP 预览已取消。" );
        return false;
      }
      if ( status != MTPL_STATUS_OK && !( status == MTPL_STATUS_CANCELED && previewContext.limitReached ) )
      {
        mResult.error = QStringLiteral( "无法读取所选 SFP 条目：%1。" ).arg( QgsMtpl::statusText( status ) );
        return false;
      }
      if ( !packageStampMatches( mDescriptor ) )
      {
        mResult.error = QStringLiteral( "SFP 数据包在预览读取期间发生变化。" );
        return false;
      }

      if ( imagePreview )
      {
        QBuffer imageBuffer( &mResult.bytes );
        if ( !imageBuffer.open( QIODevice::ReadOnly ) )
        {
          mResult.error = QStringLiteral( "无法读取图像预览数据。" );
          return false;
        }

        QImageReader imageReader( &imageBuffer );
        imageReader.setDecideFormatFromContent( true );
        imageReader.setAutoTransform( true );
        if ( !imageReader.canRead() )
        {
          mResult.error = QStringLiteral( "无法识别所选图像预览。" );
          return false;
        }

        const QSize sourceSize = imageReader.size();
        constexpr int maxPreviewSourceDimension = 65535;
        constexpr quint64 maxPreviewSourcePixels = 64ULL * 1024ULL * 1024ULL;
        if ( !sourceSize.isValid() || sourceSize.width() <= 0 || sourceSize.height() <= 0 ||
             sourceSize.width() > maxPreviewSourceDimension || sourceSize.height() > maxPreviewSourceDimension )
        {
          mResult.error = QStringLiteral( "图像预览尺寸无效或超过安全上限。" );
          return false;
        }

        const quint64 sourcePixels = static_cast<quint64>( sourceSize.width() ) *
                                     static_cast<quint64>( sourceSize.height() );
        const int allocationLimitMiB = QImageReader::allocationLimit();
        const quint64 allocationPixelLimit = allocationLimitMiB > 0
                                               ? static_cast<quint64>( allocationLimitMiB ) * 1024ULL * 1024ULL / 4ULL
                                               : std::numeric_limits<quint64>::max();
        if ( sourcePixels > maxPreviewSourcePixels || sourcePixels > allocationPixelLimit )
        {
          mResult.error = QStringLiteral( "图像预览像素数超过安全或 Qt 图像分配上限。" );
          return false;
        }

        constexpr int previewWidth = 520;
        constexpr int previewHeight = 220;
        const QSize previewBounds( previewWidth, previewHeight );
        imageReader.setScaledSize( sourceSize.scaled( previewBounds, Qt::KeepAspectRatio ) );
        QImage image = imageReader.read();
        if ( isCanceled() )
        {
          image = QImage();
          mResult.canceled = true;
          mResult.error = QStringLiteral( "SFP 预览已取消。" );
          return false;
        }
        if ( image.isNull() )
        {
          mResult.error = QStringLiteral( "无法解码所选图像预览。" );
          return false;
        }
        if ( image.width() > previewWidth || image.height() > previewHeight )
          image = image.scaled( previewBounds, Qt::KeepAspectRatio, Qt::SmoothTransformation );
        mResult.image = std::move( image );
        mResult.bytes.fill( '\0' );
        mResult.bytes.clear();
      }
      if ( isCanceled() )
      {
        mResult.bytes.fill( '\0' );
        mResult.bytes.clear();
        mResult.image = QImage();
        mResult.canceled = true;
        mResult.error = QStringLiteral( "SFP 预览已取消。" );
        return false;
      }
      mResult.ok = true;
      setProgress( 100.0 );
      return true;
    }

  private:
    QgsMtpl::PackageDescriptor mDescriptor;
    QString mEntryPath;
    QgsMtpl::CryptoKeys mSuppliedKeys;
    MtplPreviewResult mResult;
};

class MtplLoadPreparationTask final : public QgsTask
{
  public:
    explicit MtplLoadPreparationTask( const MtplLoadPreparationRequest &request )
      : QgsTask( QObject::tr( "准备加载 MTPL 数据" ), QgsTask::CanCancel | QgsTask::CancelWithoutPrompt )
      , mRequest( request )
    {
      registerMtplIoTask( this );
    }

    ~MtplLoadPreparationTask() override
    {
      clearRequestKeys();
      clearResultKeys();
      if ( !mOutputsClaimed )
        cleanupPreparedOutputs();
      unregisterMtplIoTask( this );
    }

    MtplLoadPreparationResult takeResult()
    {
      mOutputsClaimed = true;
      return std::move( mResult );
    }

    bool cancellationRequested() const { return isCanceled(); }

  protected:
    bool run() override
    {
      const qsizetype totalItems = mRequest.tilePackages.size() + mRequest.sfpEntries.size();
      if ( totalItems == 0 )
        return true;

      qsizetype completedItems = 0;
      const auto itemProgress = [this, &completedItems, totalItems]( double value )
      {
        setProgress( ( static_cast<double>( completedItems ) + std::clamp( value, 0.0, 100.0 ) / 100.0 ) *
                     100.0 / static_cast<double>( totalItems ) );
      };

      for ( MtplTileLoadInput &input : mRequest.tilePackages )
      {
        if ( isCanceled() )
          return finishCanceled();

        MtplPreparedTile prepared;
        prepared.descriptor = input.descriptor;
        if ( !packageStampMatches( input.descriptor ) )
        {
          prepared.error = QStringLiteral( "%1 在选择后发生变化，请重新检查。" )
                             .arg( QFileInfo( input.descriptor.path ).fileName() );
          input.suppliedKeys.clear();
          mResult.tilePackages.append( std::move( prepared ) );
          ++completedItems;
          itemProgress( 0.0 );
          continue;
        }
        const QgsMtpl::ProbeResult freshProbe = QgsMtplPackageService::probePath(
          input.descriptor.path, input.suppliedKeys, true, input.suppliedSource,
          [this]() { return isCanceled(); }, itemProgress );
        if ( isCanceled() || freshProbe.canceled )
          return finishCanceled();
        if ( !freshProbe.ok || freshProbe.packages.size() != 1 ||
             !freshProbe.packages.constFirst().isReady() ||
             freshProbe.packages.constFirst().format != input.descriptor.format )
        {
          prepared.error = QStringLiteral( "%1 在检查后发生变化，或已不可用。" )
                             .arg( QFileInfo( input.descriptor.path ).fileName() );
        }
        else
        {
          prepared.descriptor = freshProbe.packages.constFirst();
          if ( !packageStampMatches( prepared.descriptor ) )
          {
            prepared.error = QStringLiteral( "%1 在后台检查期间发生变化。" )
                               .arg( QFileInfo( input.descriptor.path ).fileName() );
          }
          else
          {
            preserveDisplaySettings( input.descriptor, prepared.descriptor );
            prepared.ok = resolveDescriptorKeys(
              prepared.descriptor, input.suppliedKeys, [this]() { return isCanceled(); }, prepared.keys, prepared.error );
          }
          if ( isCanceled() )
            return finishCanceled();
        }
        input.suppliedKeys.clear();
        mResult.tilePackages.append( std::move( prepared ) );
        ++completedItems;
        itemProgress( 0.0 );
      }

      for ( MtplSfpLoadInput &input : mRequest.sfpEntries )
      {
        if ( isCanceled() )
          return finishCanceled();

        QgsMtpl::CryptoKeys keys;
        QString keyError;
        if ( !packageStampMatches( input.descriptor ) )
        {
          MtplPreparedSfpEntry prepared;
          prepared.entryPath = input.entryPath;
          prepared.error = QStringLiteral( "%1 在选择后发生变化，请重新检查。" )
                             .arg( QFileInfo( input.descriptor.path ).fileName() );
          mResult.sfpEntries.append( std::move( prepared ) );
        }
        else if ( !resolveDescriptorKeys( input.descriptor, input.suppliedKeys,
                                     [this]() { return isCanceled(); }, keys, keyError ) )
        {
          MtplPreparedSfpEntry prepared;
          prepared.entryPath = input.entryPath;
          prepared.error = keyError;
          prepared.canceled = isCanceled();
          mResult.sfpEntries.append( std::move( prepared ) );
        }
        else
        {
          MtplPreparedSfpEntry prepared = extractSfpDataset(
            input.descriptor, input.entryPath, keys, mRequest.cacheRoot,
            [this]() { return isCanceled(); }, itemProgress );
          keys.clear();
          if ( prepared.ok && !packageStampMatches( input.descriptor ) )
          {
            removePreparedDirectory( mRequest.cacheRoot, prepared.cacheDirectory );
            prepared.ok = false;
            prepared.extractedPath.clear();
            prepared.cacheDirectory.clear();
            prepared.error = QStringLiteral( "%1 在后台提取期间发生变化。" )
                               .arg( QFileInfo( input.descriptor.path ).fileName() );
          }
          mResult.sfpEntries.append( std::move( prepared ) );
        }
        input.suppliedKeys.clear();
        if ( isCanceled() || mResult.sfpEntries.constLast().canceled )
          return finishCanceled();
        ++completedItems;
        itemProgress( 0.0 );
      }

      setProgress( 100.0 );
      return true;
    }

  private:
    bool finishCanceled()
    {
      mResult.canceled = true;
      cleanupPreparedOutputs();
      clearRequestKeys();
      clearResultKeys();
      return false;
    }

    void cleanupPreparedOutputs()
    {
      for ( MtplPreparedSfpEntry &entry : mResult.sfpEntries )
      {
        removePreparedDirectory( mRequest.cacheRoot, entry.cacheDirectory );
        entry.extractedPath.clear();
        entry.cacheDirectory.clear();
      }
    }

    void clearRequestKeys()
    {
      for ( MtplTileLoadInput &input : mRequest.tilePackages )
        input.suppliedKeys.clear();
      for ( MtplSfpLoadInput &input : mRequest.sfpEntries )
        input.suppliedKeys.clear();
    }

    void clearResultKeys()
    {
      for ( MtplPreparedTile &item : mResult.tilePackages )
        item.keys.clear();
    }

    MtplLoadPreparationRequest mRequest;
    MtplLoadPreparationResult mResult;
    bool mOutputsClaimed = false;
};

} // namespace

struct QgsMtplDockWidget::SfpPopulationState
{
  quint64 generation = 0;
  QList<int> packageIndexes;
  qsizetype packageListIndex = 0;
  qsizetype nextEntry = 0;
  qsizetype processedEntries = 0;
  qsizetype totalEntries = 0;
  qsizetype phaseEntry = 0;
  int phase = 0;
  quint64 completedWork = 0;
  quint64 totalWork = 0;
  QSet<QString> shapefileDatasets;
  QHash<QString, QStringList> rasterCompanionDatasets;
  QHash<QString, bool> shapefileDatasetReady;
  QHash<QString, bool> rasterDatasetReady;
  QHash<QString, QTreeWidgetItem *> treeItems;
};

QgsMtplDockWidget::QgsMtplDockWidget( QWidget *parent )
  : QgsDockWidget( tr( "MTPL 数据包" ), parent )
{
  setObjectName( QStringLiteral( "MtplPackagesDock" ) );
  setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );
  setMinimumWidth( 620 );

  auto *content = new QWidget( this );
  auto *rootLayout = new QVBoxLayout( content );
  rootLayout->setContentsMargins( 0, 0, 0, 0 );
  mTabs = new QTabWidget( content );
  mTabs->setObjectName( QStringLiteral( "mtplMainTabs" ) );
  rootLayout->addWidget( mTabs );

  mPackagesTab = new QWidget( mTabs );
  mPackagesTab->setObjectName( QStringLiteral( "mtplPackagesTab" ) );
  auto *layout = new QVBoxLayout( mPackagesTab );
  layout->setContentsMargins( 8, 8, 8, 8 );
  layout->setSpacing( 8 );

  auto *pathLabel = new QLabel( tr( "数据包或文件夹" ), content );
  pathLabel->setObjectName( QStringLiteral( "mtplPathLabel" ) );
  layout->addWidget( pathLabel );

  mPathWidget = new QgsMtplPathWidget( content );
  pathLabel->setBuddy( mPathWidget );
  layout->addWidget( mPathWidget );

  mSummaryLabel = new QLabel( tr( "请选择数据包或文件夹。" ), content );
  mSummaryLabel->setObjectName( QStringLiteral( "mtplSummaryLabel" ) );
  mSummaryLabel->setWordWrap( true );
  mSummaryLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
  auto *summaryLayout = new QHBoxLayout();
  summaryLayout->setContentsMargins( 0, 0, 0, 0 );
  summaryLayout->addWidget( mSummaryLabel, 1 );
  mCancelProbeButton = new QPushButton( tr( "取消检查" ), content );
  mCancelProbeButton->setObjectName( QStringLiteral( "mtplCancelProbeButton" ) );
  mCancelProbeButton->setAccessibleName( tr( "取消 MTPL 数据包检查" ) );
  mCancelProbeButton->hide();
  summaryLayout->addWidget( mCancelProbeButton );
  layout->addLayout( summaryLayout );

  auto *packagesHeading = new QHBoxLayout();
  packagesHeading->setContentsMargins( 0, 0, 0, 0 );
  mPackagesLabel = new QLabel( tr( "数据包（0）" ), content );
  mPackagesLabel->setObjectName( QStringLiteral( "mtplPackagesLabel" ) );
  mBatchSummaryLabel = new QLabel( tr( "0 个可用" ), content );
  mBatchSummaryLabel->setObjectName( QStringLiteral( "mtplBatchSummaryLabel" ) );
  mBatchSummaryLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
  mBatchSummaryLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
  packagesHeading->addWidget( mPackagesLabel );
  packagesHeading->addStretch();
  packagesHeading->addWidget( mBatchSummaryLabel );
  layout->addLayout( packagesHeading );

  mPackageTree = new QTreeWidget( content );
  mPackageTree->setObjectName( QStringLiteral( "mtplPackageTree" ) );
  mPackageTree->setColumnCount( 5 );
  mPackageTree->setHeaderLabels( { tr( "名称" ), tr( "格式" ), tr( "加密状态" ), tr( "密钥状态" ), tr( "可用状态" ) } );
  mPackageTree->setTextElideMode( Qt::ElideMiddle );
  mPackageTree->header()->setStretchLastSection( true );
  mPackageTree->header()->setSectionResizeMode( 0, QHeaderView::Interactive );
  for ( int column = 1; column < mPackageTree->columnCount() - 1; ++column )
    mPackageTree->header()->setSectionResizeMode( column, QHeaderView::ResizeToContents );
  mPackageTree->header()->setSectionResizeMode( mPackageTree->columnCount() - 1, QHeaderView::Stretch );
  connect( mPackageTree->header(), &QHeaderView::sectionResized, this, [this]( int logicalIndex, int, int newSize )
  {
    if ( logicalIndex == 0 && newSize > maximumPackageNameColumnWidth() )
      mPackageTree->header()->resizeSection( 0, maximumPackageNameColumnWidth() );
  } );
  mPackageTree->setSelectionMode( QAbstractItemView::SingleSelection );
  mPackageTree->setMinimumHeight( 180 );
  layout->addWidget( mPackageTree, 1 );

  mKeyPanel = new QWidget( content );
  mKeyPanel->setObjectName( QStringLiteral( "mtplKeyPanel" ) );
  auto *keyLayout = new QVBoxLayout( mKeyPanel );
  keyLayout->setContentsMargins( 0, 0, 0, 0 );
  keyLayout->setSpacing( 6 );
  auto *keyHeading = new QLabel( tr( "输入密钥" ), mKeyPanel );
  keyHeading->setObjectName( QStringLiteral( "mtplKeyHeading" ) );
  QFont keyHeadingFont = keyHeading->font();
  keyHeadingFont.setBold( true );
  keyHeading->setFont( keyHeadingFont );
  keyLayout->addWidget( keyHeading );
  mKeyHelpLabel = new QLabel(
    tr( "未加密的数据无需填写，加密数据需要同时提供 Base64 私钥和小写十六进制设备密钥。"
        "点击“保存并应用密钥”后，密钥将以未加密形式保存在 QGIS 设置中，请仅在可信设备上使用。" ),
    mKeyPanel );
  mKeyHelpLabel->setObjectName( QStringLiteral( "mtplKeyHelpLabel" ) );
  mKeyHelpLabel->setWordWrap( true );
  mKeyHelpLabel->setTextFormat( Qt::PlainText );
  mKeyHelpLabel->setAccessibleName( tr( "MTPL 密钥保存说明" ) );
  mKeyHelpLabel->setAccessibleDescription( mKeyHelpLabel->text() );
  mKeyHelpLabel->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Minimum );
  mKeyHelpLabel->setMinimumWidth( 0 );
  keyLayout->addWidget( mKeyHelpLabel );
  auto *keyForm = new QFormLayout();
  keyForm->setRowWrapPolicy( QFormLayout::WrapLongRows );
  keyForm->setFieldGrowthPolicy( QFormLayout::AllNonFixedFieldsGrow );
  mPrivateKeyEdit = new QgsPasswordLineEdit( mKeyPanel );
  mPrivateKeyEdit->setObjectName( QStringLiteral( "mtplPrivateKeyEdit" ) );
  mPrivateKeyEdit->setEchoMode( QLineEdit::Password );
  mPrivateKeyEdit->setPlaceholderText( tr( "Base64 私钥" ) );
  mPrivateKeyEdit->setAccessibleName( tr( "MTPL 私钥" ) );
  mPrivateKeyEdit->setAccessibleDescription( tr( "规范的 Base64 私钥。使用输入框末尾的按钮显示或隐藏内容。" ) );
  mPrivateKeyEdit->setToolTip( tr( "请输入规范的 Base64 私钥。" ) );
  mPrivateKeyEdit->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
  localizePasswordVisibilityAction( mPrivateKeyEdit, tr( "显示私钥" ), tr( "隐藏私钥" ) );
  mDeviceKeyEdit = new QgsPasswordLineEdit( mKeyPanel );
  mDeviceKeyEdit->setObjectName( QStringLiteral( "mtplDeviceKeyEdit" ) );
  mDeviceKeyEdit->setEchoMode( QLineEdit::Password );
  mDeviceKeyEdit->setPlaceholderText( tr( "小写十六进制设备密钥" ) );
  mDeviceKeyEdit->setAccessibleName( tr( "MTPL 设备密钥" ) );
  mDeviceKeyEdit->setAccessibleDescription( tr( "仅包含小写十六进制字符的设备密钥。使用输入框末尾的按钮显示或隐藏内容。" ) );
  mDeviceKeyEdit->setToolTip( tr( "请输入偶数位的小写十六进制设备密钥。" ) );
  mDeviceKeyEdit->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
  localizePasswordVisibilityAction( mDeviceKeyEdit, tr( "显示设备密钥" ), tr( "隐藏设备密钥" ) );
  keyForm->addRow( tr( "私钥" ), mPrivateKeyEdit );
  keyForm->addRow( tr( "设备密钥" ), mDeviceKeyEdit );
  keyLayout->addLayout( keyForm );
  auto *keyActions = new QHBoxLayout();
  keyActions->setContentsMargins( 0, 0, 0, 0 );
  mApplyKeyButton = new QPushButton( tr( "保存并应用密钥" ), mKeyPanel );
  mApplyKeyButton->setObjectName( QStringLiteral( "mtplApplyRememberedKeyButton" ) );
  mApplyKeyButton->setAccessibleName( tr( "保存并应用密钥" ) );
  mApplyKeyButton->setAccessibleDescription( tr( "将输入值以未加密形式保存到 QGIS 设置，并应用于当前批次。" ) );
  mClearKeyButton = new QPushButton( tr( "清除已记住的密钥" ), mKeyPanel );
  mClearKeyButton->setObjectName( QStringLiteral( "mtplClearRememberedKeyButton" ) );
  mClearKeyButton->setAccessibleName( tr( "清除已记住的密钥" ) );
  mClearKeyButton->setAccessibleDescription( tr( "从 QGIS 设置和当前输入区域中删除已保存的密钥。" ) );
  keyActions->addWidget( mApplyKeyButton );
  keyActions->addWidget( mClearKeyButton );
  keyActions->addStretch();
  keyLayout->addLayout( keyActions );
  layout->addWidget( mKeyPanel );

  mLoadButton = new QPushButton( tr( "加载可用数据包和已选 SFP 条目" ), content );
  mLoadButton->setObjectName( QStringLiteral( "mtplLoadButton" ) );
  mLoadButton->setIcon( QgsApplication::getThemeIcon( QStringLiteral( "/mActionAddLayer.svg" ) ) );
  mLoadButton->setEnabled( false );
  layout->addWidget( mLoadButton );

  mTabs->addTab( mPackagesTab, tr( "数据包" ) );

  mDetailsTab = new QWidget( mTabs );
  mDetailsTab->setObjectName( QStringLiteral( "mtplPackageDetailsTab" ) );
  auto *detailsTabLayout = new QVBoxLayout( mDetailsTab );
  detailsTabLayout->setContentsMargins( 0, 0, 0, 0 );
  mDetailsStack = new QStackedWidget( mDetailsTab );
  mDetailsStack->setObjectName( QStringLiteral( "mtplDetailsStack" ) );
  detailsTabLayout->addWidget( mDetailsStack );

  mDetailsEmptyLabel = new QLabel( tr( "请先在“数据包”页选择一个数据包或 SFP 数据集。" ), mDetailsStack );
  mDetailsEmptyLabel->setObjectName( QStringLiteral( "mtplDetailsEmptyLabel" ) );
  mDetailsEmptyLabel->setAlignment( Qt::AlignCenter );
  mDetailsEmptyLabel->setWordWrap( true );
  mDetailsStack->addWidget( mDetailsEmptyLabel );

  mDetailsScrollArea = new QScrollArea( mDetailsStack );
  mDetailsScrollArea->setObjectName( QStringLiteral( "mtplDetailsScrollArea" ) );
  mDetailsScrollArea->setWidgetResizable( true );
  mDetailsScrollArea->setFrameShape( QFrame::NoFrame );
  mDetailsScrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  mDetailsStack->addWidget( mDetailsScrollArea );

  mDetailsPanel = new QWidget( mDetailsScrollArea );
  mDetailsPanel->setObjectName( QStringLiteral( "mtplDetailsPanel" ) );
  auto *detailsLayout = new QVBoxLayout( mDetailsPanel );
  detailsLayout->setContentsMargins( 8, 8, 8, 8 );
  detailsLayout->setSpacing( 6 );

  auto *detailsSelectionLayout = new QHBoxLayout();
  detailsSelectionLayout->setContentsMargins( 0, 0, 0, 0 );
  mDetailsSelectionLabel = new QLabel( mDetailsPanel );
  mDetailsSelectionLabel->setObjectName( QStringLiteral( "mtplDetailsSelectionLabel" ) );
  mDetailsSelectionLabel->setWordWrap( true );
  mDetailsSelectionLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
  detailsSelectionLayout->addWidget( mDetailsSelectionLabel, 1 );
  mBackToPackagesButton = new QPushButton( tr( "返回数据包" ), mDetailsPanel );
  mBackToPackagesButton->setObjectName( QStringLiteral( "mtplBackToPackagesButton" ) );
  mBackToPackagesButton->setAccessibleName( tr( "返回数据包页" ) );
  detailsSelectionLayout->addWidget( mBackToPackagesButton, 0, Qt::AlignTop );
  detailsLayout->addLayout( detailsSelectionLayout );

  mImportStyleButton = new QToolButton( mDetailsPanel );
  mImportStyleButton->setObjectName( QStringLiteral( "mtplImportStyleButton" ) );
  mImportStyleButton->setText( tr( "导入 Mapbox 样式…" ) );
  mImportStyleButton->setIcon( QgsApplication::getThemeIcon( QStringLiteral( "/mActionStyleManager.svg" ) ) );
  mImportStyleButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
  mImportStyleButton->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
  mImportStyleButton->setEnabled( false );
  detailsLayout->addWidget( mImportStyleButton );

  mOpenInQgisButton = new QPushButton( tr( "在 QGIS 中打开" ), mDetailsPanel );
  mOpenInQgisButton->setObjectName( QStringLiteral( "mtplOpenSfpEntryButton" ) );
  mOpenInQgisButton->setToolTip( tr( "将所选 GIS 条目提取到受管理的临时缓存，并使用 QGIS 打开。" ) );
  mOpenInQgisButton->hide();
  detailsLayout->addWidget( mOpenInQgisButton );

  mOverridesGroup = new QGroupBox( tr( "显示参数覆盖" ), mDetailsPanel );
  mOverridesGroup->setObjectName( QStringLiteral( "mtplOverridesGroup" ) );
  auto *overridesLayout = new QFormLayout( mOverridesGroup );
  mPayloadOverride = new QComboBox( mOverridesGroup );
  mPayloadOverride->setObjectName( QStringLiteral( "mtplPayloadOverride" ) );
  mPayloadOverride->addItem( tr( "自动检测" ), static_cast<int>( QgsMtpl::PayloadType::Unknown ) );
  mPayloadOverride->addItem( tr( "栅格图像" ), static_cast<int>( QgsMtpl::PayloadType::RasterImage ) );
  mPayloadOverride->addItem( tr( "矢量瓦片" ), static_cast<int>( QgsMtpl::PayloadType::VectorTile ) );
  mPayloadOverrideLabel = new QLabel( tr( "载荷类型" ), mOverridesGroup );
  mPayloadOverrideLabel->setBuddy( mPayloadOverride );
  overridesLayout->addRow( mPayloadOverrideLabel, mPayloadOverride );

  mCrsOverride = new QLineEdit( mOverridesGroup );
  mCrsOverride->setObjectName( QStringLiteral( "mtplCrsOverride" ) );
  mCrsOverride->setPlaceholderText( QStringLiteral( "EPSG:3857" ) );
  overridesLayout->addRow( tr( "坐标参考系（CRS）" ), mCrsOverride );

  mSchemeOverride = new QComboBox( mOverridesGroup );
  mSchemeOverride->setObjectName( QStringLiteral( "mtplSchemeOverride" ) );
  mSchemeOverride->addItem( QStringLiteral( "XYZ" ), QStringLiteral( "xyz" ) );
  mSchemeOverride->addItem( QStringLiteral( "TMS" ), QStringLiteral( "tms" ) );
  overridesLayout->addRow( tr( "瓦片方案" ), mSchemeOverride );

  mDtpOverrides = new QWidget( mOverridesGroup );
  mDtpOverrides->setObjectName( QStringLiteral( "mtplDtpOverrides" ) );
  auto *dtpLayout = new QFormLayout( mDtpOverrides );
  dtpLayout->setContentsMargins( 0, 0, 0, 0 );
  mDataTypeOverride = new QComboBox( mDtpOverrides );
  mDataTypeOverride->setObjectName( QStringLiteral( "mtplDataTypeOverride" ) );
  mDataTypeOverride->addItems( { QStringLiteral( "uint16" ), QStringLiteral( "int16" ), QStringLiteral( "float32" ) } );
  dtpLayout->addRow( tr( "数据类型" ), mDataTypeOverride );
  mEndiannessOverride = new QComboBox( mDtpOverrides );
  mEndiannessOverride->setObjectName( QStringLiteral( "mtplEndiannessOverride" ) );
  mEndiannessOverride->addItem( tr( "小端序" ), QStringLiteral( "little" ) );
  mEndiannessOverride->addItem( tr( "大端序" ), QStringLiteral( "big" ) );
  dtpLayout->addRow( tr( "字节序" ), mEndiannessOverride );
  mScaleOverride = new QDoubleSpinBox( mDtpOverrides );
  mScaleOverride->setObjectName( QStringLiteral( "mtplScaleOverride" ) );
  mScaleOverride->setDecimals( 8 );
  mScaleOverride->setRange( -1.0e12, 1.0e12 );
  dtpLayout->addRow( tr( "比例系数" ), mScaleOverride );
  mOffsetOverride = new QDoubleSpinBox( mDtpOverrides );
  mOffsetOverride->setObjectName( QStringLiteral( "mtplOffsetOverride" ) );
  mOffsetOverride->setDecimals( 8 );
  mOffsetOverride->setRange( -1.0e12, 1.0e12 );
  dtpLayout->addRow( tr( "偏移量" ), mOffsetOverride );
  mNoDataOverride = new QLineEdit( mDtpOverrides );
  mNoDataOverride->setObjectName( QStringLiteral( "mtplNoDataOverride" ) );
  mNoDataOverride->setPlaceholderText( tr( "不使用时留空" ) );
  dtpLayout->addRow( tr( "NoData 值" ), mNoDataOverride );
  overridesLayout->addRow( mDtpOverrides );

  mApplyOverridesButton = new QPushButton( tr( "应用显示参数" ), mOverridesGroup );
  mApplyOverridesButton->setObjectName( QStringLiteral( "mtplApplyOverridesButton" ) );
  overridesLayout->addRow( mApplyOverridesButton );
  mOverridesGroup->hide();
  detailsLayout->addWidget( mOverridesGroup );

  mMetadataTable = new QTableWidget( mDetailsPanel );
  mMetadataTable->setObjectName( QStringLiteral( "mtplMetadataTable" ) );
  mMetadataTable->setColumnCount( 2 );
  mMetadataTable->setHorizontalHeaderLabels( { tr( "元数据" ), tr( "值" ) } );
  mMetadataTable->horizontalHeader()->setStretchLastSection( true );
  mMetadataTable->verticalHeader()->hide();
  mMetadataTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
  mMetadataTable->setSelectionBehavior( QAbstractItemView::SelectRows );
  mMetadataTable->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  mMetadataTable->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
  detailsLayout->addWidget( mMetadataTable );

  mPreviewText = new QPlainTextEdit( mDetailsPanel );
  mPreviewText->setObjectName( QStringLiteral( "mtplSfpTextPreview" ) );
  mPreviewText->setReadOnly( true );
  mPreviewText->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  mPreviewText->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
  mPreviewText->hide();
  detailsLayout->addWidget( mPreviewText );

  mPreviewImage = new QLabel( mDetailsPanel );
  mPreviewImage->setObjectName( QStringLiteral( "mtplSfpImagePreview" ) );
  mPreviewImage->setAlignment( Qt::AlignCenter );
  mPreviewImage->setMinimumHeight( 120 );
  mPreviewImage->setMaximumHeight( 240 );
  mPreviewImage->hide();
  detailsLayout->addWidget( mPreviewImage );

  detailsLayout->addStretch();
  mDetailsScrollArea->setWidget( mDetailsPanel );
  mDetailsStack->setCurrentWidget( mDetailsEmptyLabel );
  mTabs->addTab( mDetailsTab, tr( "数据包详情" ) );

  mToolsTab = new QWidget( mTabs );
  mToolsTab->setObjectName( QStringLiteral( "mtplPackageToolsTab" ) );
  auto *toolsLayout = new QVBoxLayout( mToolsTab );
  toolsLayout->setContentsMargins( 8, 8, 8, 8 );
  mPackageToolsWidget = new QgsMtplPackageToolsWidget( mToolsTab );
  toolsLayout->addWidget( mPackageToolsWidget );
  mTabs->addTab( mToolsTab, tr( "数据包工具" ) );
  mTabs->setCurrentIndex( static_cast<int>( MtplTab::Packages ) );

  setWidget( content );

  QgsMtplCredentialStore::setRememberEnabled( true );
  QString rememberedKeyError;
  mRememberedKeys = QgsMtplCredentialStore::rememberedKeys( &rememberedKeyError );
  if ( mRememberedKeys.isValid() )
  {
    mPrivateKeyEdit->setText( QString::fromUtf8( mRememberedKeys.privateKey ) );
    mDeviceKeyEdit->setText( QString::fromUtf8( mRememberedKeys.deviceKey ) );
    mKeys = mRememberedKeys;
    mKeySource = QgsMtpl::CredentialSource::Remembered;
  }
  mClearKeyButton->setEnabled( mRememberedKeys.isValid() );
  const QString rememberedPath = QgsMtplCredentialStore::lastPath();
  if ( !rememberedPath.isEmpty() )
    mPathWidget->setPath( rememberedPath );

  mProbeTimer = new QTimer( this );
  mProbeTimer->setSingleShot( true );
  mProbeTimer->setInterval( 350 );
  mSfpPopulationTimer = new QTimer( this );
  mSfpPopulationTimer->setSingleShot( true );
  mSfpPopulationTimer->setInterval( 0 );
  mToolOutputRefreshTimer = new QTimer( this );
  mToolOutputRefreshTimer->setSingleShot( true );
  mToolOutputRefreshTimer->setInterval( 250 );

  connect( mPathWidget, &QgsMtplPathWidget::pathChanged, this, &QgsMtplDockWidget::scheduleProbe );
  connect( mPathWidget, &QgsMtplPathWidget::pathSelected, this, &QgsMtplDockWidget::probeSelectedPath );
  connect( mProbeTimer, &QTimer::timeout, this, &QgsMtplDockWidget::probeSelectedPath );
  connect( mSfpPopulationTimer, &QTimer::timeout, this, &QgsMtplDockWidget::populateSfpEntriesBatch );
  connect( mToolOutputRefreshTimer, &QTimer::timeout, this, [this]
  {
    if ( mLoadTask || mPreviewTask || mProbeTask || mSfpPopulation )
    {
      mToolOutputRefreshTimer->start();
      return;
    }
    if ( !selectedPath().isEmpty() )
      probeSelectedPath();
  } );
  connect( mCancelProbeButton, &QPushButton::clicked, this, &QgsMtplDockWidget::cancelCurrentProbe );
  connect( mLoadButton, &QPushButton::clicked, this, &QgsMtplDockWidget::loadToProject );
  connect( mApplyKeyButton, &QPushButton::clicked, this, &QgsMtplDockWidget::applyRememberedKeys );
  connect( mClearKeyButton, &QPushButton::clicked, this, &QgsMtplDockWidget::clearRememberedKeys );
  connect( mPackageTree, &QTreeWidget::itemSelectionChanged, this, &QgsMtplDockWidget::showSelectedPackageMetadata );
  connect( mPackageTree, &QTreeWidget::itemChanged, this, &QgsMtplDockWidget::updateLoadButtonState );
  connect( mApplyOverridesButton, &QPushButton::clicked, this, &QgsMtplDockWidget::applyOverrides );
  connect( mOpenInQgisButton, &QPushButton::clicked, this, &QgsMtplDockWidget::openSelectedSfpEntry );
  connect( mImportStyleButton, &QToolButton::clicked, this, &QgsMtplDockWidget::importVectorStyle );
  connect( mPreviewText, &QPlainTextEdit::textChanged, this, &QgsMtplDockWidget::updatePreviewTextHeight );
  connect( mBackToPackagesButton, &QPushButton::clicked, this, [this]
  {
    mTabs->setCurrentIndex( static_cast<int>( MtplTab::Packages ) );
    mPackageTree->setFocus( Qt::ShortcutFocusReason );
  } );
  connect( mTabs, &QTabWidget::currentChanged, this, &QgsMtplDockWidget::currentTabChanged );
  connect( mPackageToolsWidget, &QgsMtplPackageToolsWidget::outputsCommitted,
           this, &QgsMtplDockWidget::packageToolOutputsCommitted );
  connect( mPackageToolsWidget, &QgsMtplPackageToolsWidget::messageRequested,
           this, &QgsMtplDockWidget::messageRequested );
  mPackageToolsWidget->setSuggestedSourcePath( selectedPath() );
  updateDetailsPageState();
  if ( !selectedPath().isEmpty() )
    probeSelectedPath();
}

QgsMtplDockWidget::~QgsMtplDockWidget()
{
  cancelPendingOperations();
  cleanupSfpCache();
}

void QgsMtplDockWidget::resizeEvent( QResizeEvent *event )
{
  QgsDockWidget::resizeEvent( event );
  updatePackageColumnWidths();
  updateMetadataTableHeight();
  updatePreviewTextHeight();
}

void QgsMtplDockWidget::hideEvent( QHideEvent *event )
{
  hidePassword( mPrivateKeyEdit );
  hidePassword( mDeviceKeyEdit );
  if ( mPackageToolsWidget )
    mPackageToolsWidget->concealSecrets();
  QgsDockWidget::hideEvent( event );
}

void QgsMtplDockWidget::currentTabChanged( int index )
{
  if ( mPreviousTabIndex == static_cast<int>( MtplTab::Packages ) )
  {
    hidePassword( mPrivateKeyEdit );
    hidePassword( mDeviceKeyEdit );
  }
  else if ( mPreviousTabIndex == static_cast<int>( MtplTab::Tools ) && mPackageToolsWidget )
  {
    mPackageToolsWidget->concealSecrets();
  }

  mPreviousTabIndex = index;
  if ( index == static_cast<int>( MtplTab::Packages ) )
  {
    QTimer::singleShot( 0, this, &QgsMtplDockWidget::updatePackageColumnWidths );
  }
  else if ( index == static_cast<int>( MtplTab::Details ) )
  {
    updateDetailsPageState();
    QTimer::singleShot( 0, this, &QgsMtplDockWidget::updateMetadataTableHeight );
  }
  else if ( index == static_cast<int>( MtplTab::Tools ) && mPackageToolsWidget )
  {
    mPackageToolsWidget->setSuggestedSourcePath( selectedPath() );
  }
}

void QgsMtplDockWidget::packageToolOutputsCommitted( const QStringList &paths )
{
  if ( paths.isEmpty() || selectedPath().isEmpty() )
    return;
  mToolOutputRefreshTimer->start();
}

QString QgsMtplDockWidget::selectedPath() const
{
  return mPathWidget->path();
}

QgsMtpl::ProbeResult QgsMtplDockWidget::currentProbe() const
{
  return mProbe;
}

void QgsMtplDockWidget::setSelectedPath( const QString &path )
{
  mPathWidget->setPath( path );
  probeSelectedPath();
}

void QgsMtplDockWidget::cancelPendingOperations()
{
  if ( mToolOutputRefreshTimer )
    mToolOutputRefreshTimer->stop();
  cancelProbeWork( false );
  cancelIoWork( false );
  if ( mPackageToolsWidget )
    mPackageToolsWidget->shutdown();
  QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
  cancelAndWaitForAllMtplIoTasks();
  QgsMtplPackageOperationTask::cancelAndWaitForAllActiveTasks();
  mKeys.clear();
  mKeySource = QgsMtpl::CredentialSource::None;
  mRememberedKeys.clear();
  if ( mPrivateKeyEdit )
    mPrivateKeyEdit->clear();
  if ( mDeviceKeyEdit )
    mDeviceKeyEdit->clear();
}

void QgsMtplDockWidget::cancelProbeWork( bool waitForTasks, const QString &summary )
{
  if ( mProbeTimer )
    mProbeTimer->stop();
  if ( mSfpPopulationTimer )
    mSfpPopulationTimer->stop();
  if ( mProbeTask )
    mProbeTask->cancel();
  mProbeTask = nullptr;
  mSfpPopulation.reset();
  mPendingKeys.clear();
  mPendingMatchesRememberedKey = false;
  ++mProbeGeneration;
  if ( mCancelProbeButton )
  {
    mCancelProbeButton->setEnabled( true );
    mCancelProbeButton->setText( tr( "取消检查" ) );
    mCancelProbeButton->hide();
  }
  if ( mApplyKeyButton )
    mApplyKeyButton->setEnabled( true );
  if ( waitForTasks )
    QgsMtplProbeTask::cancelAndWaitForAllActiveTasks();
  if ( !summary.isNull() )
    clearProbe( summary );
}

void QgsMtplDockWidget::cancelIoWork( bool waitForTasks )
{
  const QPointer<QgsTask> previewTask = mPreviewTask;
  const QPointer<QgsTask> loadTask = mLoadTask;
  mPreviewTask = nullptr;
  mLoadTask = nullptr;
  ++mPreviewGeneration;
  ++mLoadGeneration;
  if ( previewTask )
    previewTask->cancel();
  if ( loadTask )
    loadTask->cancel();
  if ( mPackageTree )
    mPackageTree->setEnabled( true );
  if ( mOpenInQgisButton )
    mOpenInQgisButton->setEnabled( true );
  if ( loadTask && mLoadButton )
    updateLoadButtonState();
  if ( mCancelProbeButton )
  {
    mCancelProbeButton->setEnabled( true );
    mCancelProbeButton->setText( tr( "取消检查" ) );
    mCancelProbeButton->hide();
  }
  if ( waitForTasks )
    cancelAndWaitForAllMtplIoTasks();
}

void QgsMtplDockWidget::cancelPreviewWork( bool showCanceled )
{
  if ( mPreviewTask )
    mPreviewTask->cancel();
  mPreviewTask = nullptr;
  ++mPreviewGeneration;
  if ( !mLoadTask && !mProbeTask && !mSfpPopulation )
  {
    mCancelProbeButton->setEnabled( true );
    mCancelProbeButton->setText( tr( "取消检查" ) );
    mCancelProbeButton->hide();
  }
  if ( showCanceled )
  {
    mPreviewImage->clear();
    mPreviewImage->hide();
    mPreviewText->setPlainText( tr( "SFP 预览已取消。" ) );
    mPreviewText->show();
  }
}

void QgsMtplDockWidget::cancelCurrentProbe()
{
  if ( mLoadTask )
  {
    // Invalidate the task before asking it to stop. A worker may already have
    // finished while its completion event is still queued on the GUI thread.
    // Keeping the old generation in that window would apply a canceled load.
    cancelIoWork( false );
    mSummaryLabel->setText( tr( "数据加载已取消，未应用未完成的结果。" ) );
    return;
  }
  if ( mPreviewTask )
  {
    cancelPreviewWork( true );
    return;
  }
  if ( mToolOutputRefreshTimer )
    mToolOutputRefreshTimer->stop();
  cancelProbeWork( false, tr( "检查已取消。未应用未完成的结果。" ) );
}

void QgsMtplDockWidget::scheduleProbe()
{
  cancelIoWork( false );
  cancelProbeWork( false );
  const bool keepManualKeys = mKeySource == QgsMtpl::CredentialSource::Explicit && mKeys.isValid();
  if ( !keepManualKeys )
  {
    mKeys.clear();
    mKeySource = QgsMtpl::CredentialSource::None;
    if ( mRememberedKeys.isValid() )
    {
      mKeys = mRememberedKeys;
      mKeySource = QgsMtpl::CredentialSource::Remembered;
    }
  }
  clearProbe();
  const QString path = selectedPath();
  if ( mPackageToolsWidget )
    mPackageToolsWidget->setSuggestedSourcePath( path );
  if ( path.isEmpty() )
  {
    mSummaryLabel->setText( tr( "请选择数据包或文件夹。" ) );
    return;
  }
  mSummaryLabel->setText( tr( "正在检查所选内容…" ) );
  mProbeTimer->start();
}

void QgsMtplDockWidget::probeSelectedPath()
{
  mProbeTimer->stop();
  hidePassword( mPrivateKeyEdit );
  hidePassword( mDeviceKeyEdit );
  const QString path = selectedPath();
  if ( mPackageToolsWidget )
    mPackageToolsWidget->setSuggestedSourcePath( path );
  if ( path.isEmpty() )
  {
    clearProbe( tr( "请选择数据包或文件夹。" ) );
    return;
  }

  QgsMtplCredentialStore::setLastPath( path );
  startProbe( path, mKeys, mKeySource, ProbePurpose::Selection );
}

void QgsMtplDockWidget::startProbe( const QString &path,
                                    const QgsMtpl::CryptoKeys &keys,
                                    QgsMtpl::CredentialSource source,
                                    ProbePurpose purpose )
{
  cancelIoWork( false );
  cancelProbeWork( false );
  const quint64 generation = mProbeGeneration;
  if ( purpose == ProbePurpose::ValidateAndRememberKeys )
    mPendingKeys = keys;

  auto *task = new QgsMtplProbeTask( path, keys, true, source );
  mProbeTask = task;
  mCancelProbeButton->setEnabled( true );
  mCancelProbeButton->setText( tr( "取消检查" ) );
  mCancelProbeButton->show();
  mApplyKeyButton->setEnabled( false );
  mLoadButton->setEnabled( false );
  mSummaryLabel->setText( tr( "正在检查所选内容… 0%" ) );
  connect( task, &QgsTask::progressChanged, this, [this, generation]( double value )
  {
    if ( generation != mProbeGeneration || !mProbeTask )
      return;
    mSummaryLabel->setText( tr( "正在检查所选内容… %1%" ).arg( qRound( value ) ) );
  } );
  const auto finished = [this, task, generation, purpose]
  {
    probeFinished( task, generation, purpose );
  };
  connect( task, &QgsTask::taskCompleted, this, finished );
  connect( task, &QgsTask::taskTerminated, this, finished );
  QgsApplication::taskManager()->addTask( task );
}

void QgsMtplDockWidget::probeFinished( QgsMtplProbeTask *task, quint64 generation, ProbePurpose purpose )
{
  if ( generation != mProbeGeneration || !task )
    return;

  const QgsMtpl::ProbeResult result = task->result();
  mProbeTask = nullptr;
  mCancelProbeButton->hide();
  mApplyKeyButton->setEnabled( true );
  if ( result.canceled )
  {
    mPendingKeys.clear();
    clearProbe( tr( "检查已取消。未应用未完成的结果。" ) );
    return;
  }

  if ( purpose == ProbePurpose::ValidateAndRememberKeys )
  {
    const bool rejected = !result.ok || std::any_of(
      result.packages.cbegin(), result.packages.cend(), []( const QgsMtpl::PackageDescriptor &descriptor )
      {
        return descriptor.readiness == QgsMtpl::ReadinessState::KeyRequired ||
               descriptor.readiness == QgsMtpl::ReadinessState::KeyRejectedOrCorrupt;
      } );
    if ( rejected )
    {
      mPendingKeys.clear();
      applyProbe( result );
      return;
    }

    QString saveError;
    QgsMtplCredentialStore::setRememberEnabled( true );
    if ( !QgsMtplCredentialStore::saveManualKeys( mPendingKeys, selectedPath(), saveError ) )
    {
      mPendingKeys.clear();
      emit loadFailed( saveError );
      return;
    }
    mRememberedKeys.clear();
    mRememberedKeys = mPendingKeys;
    mKeys.clear();
    mKeys = mPendingKeys;
    mKeySource = mPendingMatchesRememberedKey ? QgsMtpl::CredentialSource::Remembered
                                              : QgsMtpl::CredentialSource::Explicit;
    mPendingKeys.clear();
    mPendingMatchesRememberedKey = false;
    mClearKeyButton->setEnabled( true );
    hidePassword( mPrivateKeyEdit );
    hidePassword( mDeviceKeyEdit );
    if ( mPackageToolsWidget )
      mPackageToolsWidget->reloadRememberedKeys();
  }
  applyProbe( result );
}

void QgsMtplDockWidget::loadToProject()
{
  if ( !mProbe.ok || mProbe.packages.isEmpty() )
    return;

  cancelPreviewWork();
  MtplLoadPreparationRequest request;
  for ( int row = 0; row < mPackageTree->topLevelItemCount(); ++row )
  {
    QTreeWidgetItem *item = mPackageTree->topLevelItem( row );
    if ( static_cast<PackageTreeItemKind>( item->data( 0, ITEM_KIND_ROLE ).toInt() ) != PackageTreeItemKind::Package ||
         item->checkState( 0 ) != Qt::Checked )
      continue;

    bool indexOk = false;
    const int packageIndex = item->data( 0, PACKAGE_INDEX_ROLE ).toInt( &indexOk );
    if ( !indexOk || packageIndex < 0 || packageIndex >= mProbe.packages.size() )
      continue;

    QgsMtpl::PackageDescriptor descriptor = mProbe.packages.at( packageIndex );
    if ( descriptor.format == QgsMtpl::PackageFormat::Sfp || !descriptor.isReady() )
      continue;

    MtplTileLoadInput input;
    input.descriptor = descriptor;
    input.suppliedKeys = suppliedKeysForDescriptor( descriptor );
    if ( descriptor.credentialSource == QgsMtpl::CredentialSource::Explicit )
      input.suppliedSource = QgsMtpl::CredentialSource::Explicit;
    else if ( descriptor.credentialSource == QgsMtpl::CredentialSource::Remembered )
      input.suppliedSource = QgsMtpl::CredentialSource::Remembered;
    request.tilePackages.append( std::move( input ) );
  }

  QTreeWidgetItemIterator iterator( mPackageTree );
  while ( *iterator )
  {
    QTreeWidgetItem *item = *iterator;
    ++iterator;
    if ( static_cast<PackageTreeItemKind>( item->data( 0, ITEM_KIND_ROLE ).toInt() ) != PackageTreeItemKind::SfpEntry ||
         item->checkState( 0 ) != Qt::Checked )
      continue;

    bool indexOk = false;
    const int packageIndex = item->data( 0, PACKAGE_INDEX_ROLE ).toInt( &indexOk );
    const QString entryPath = item->data( 0, SFP_ENTRY_PATH_ROLE ).toString();
    if ( !indexOk || packageIndex < 0 || packageIndex >= mProbe.packages.size() || entryPath.isEmpty() )
      continue;

    const QgsMtpl::PackageDescriptor &descriptor = mProbe.packages.at( packageIndex );
    if ( descriptor.format != QgsMtpl::PackageFormat::Sfp || !item->data( 0, ITEM_READY_ROLE ).toBool() )
      continue;

    MtplSfpLoadInput input;
    input.descriptor = descriptor;
    input.entryPath = entryPath;
    input.suppliedKeys = suppliedKeysForDescriptor( descriptor );
    request.sfpEntries.append( std::move( input ) );
  }

  if ( request.tilePackages.isEmpty() && request.sfpEntries.isEmpty() )
  {
    emit loadFailed( tr( "请至少选择一个可用数据包或 SFP 条目。" ) );
    return;
  }

  if ( !request.sfpEntries.isEmpty() )
  {
    QString cacheError;
    if ( !ensureSfpCache( request.cacheRoot, cacheError ) )
    {
      for ( MtplTileLoadInput &input : request.tilePackages )
        input.suppliedKeys.clear();
      for ( MtplSfpLoadInput &input : request.sfpEntries )
        input.suppliedKeys.clear();
      emit loadFailed( cacheError );
      return;
    }
  }

  startLoadPreparationTask( new MtplLoadPreparationTask( request ), false );
  for ( MtplTileLoadInput &input : request.tilePackages )
    input.suppliedKeys.clear();
  for ( MtplSfpLoadInput &input : request.sfpEntries )
    input.suppliedKeys.clear();
}

void QgsMtplDockWidget::startLoadPreparationTask( QgsTask *task, bool singleSfpEntry )
{
  if ( !task )
    return;

  if ( mLoadTask )
    mLoadTask->cancel();
  mLoadTask = nullptr;
  ++mLoadGeneration;
  const quint64 generation = mLoadGeneration;
  mLoadTask = task;
  mPackageTree->setEnabled( false );
  mLoadButton->setEnabled( false );
  mOpenInQgisButton->setEnabled( false );
  mCancelProbeButton->setEnabled( true );
  mCancelProbeButton->setText( tr( "取消加载" ) );
  mCancelProbeButton->show();
  mSummaryLabel->setText( tr( "正在后台准备所选 MTPL 数据… 0%" ) );

  connect( task, &QgsTask::progressChanged, this, [this, task, generation]( double value )
  {
    if ( generation != mLoadGeneration || mLoadTask != task )
      return;
    mSummaryLabel->setText( tr( "正在后台准备所选 MTPL 数据… %1%" ).arg( qRound( value ) ) );
  } );
  const auto finished = [this, task, generation, singleSfpEntry]
  {
    loadPreparationFinished( task, generation, singleSfpEntry );
  };
  connect( task, &QgsTask::taskCompleted, this, finished );
  connect( task, &QgsTask::taskTerminated, this, finished );
  QgsApplication::taskManager()->addTask( task );
}

void QgsMtplDockWidget::loadPreparationFinished( QgsTask *task, quint64 generation, bool singleSfpEntry )
{
  Q_ASSERT( QThread::currentThread() == QgsApplication::instance()->thread() );
  if ( generation != mLoadGeneration || mLoadTask != task || !task )
    return;

  auto *preparationTask = static_cast<MtplLoadPreparationTask *>( task );
  const bool canceledByTaskManager = preparationTask->cancellationRequested();
  mLoadTask = nullptr;
  mPackageTree->setEnabled( true );
  mOpenInQgisButton->setEnabled( true );
  mCancelProbeButton->setEnabled( true );
  mCancelProbeButton->setText( tr( "取消检查" ) );
  mCancelProbeButton->hide();
  updateLoadButtonState();

  if ( canceledByTaskManager )
  {
    mSummaryLabel->setText( tr( "数据加载已取消，未应用未完成的结果。" ) );
    return;
  }

  MtplLoadPreparationResult result = preparationTask->takeResult();

  if ( result.canceled )
  {
    for ( MtplPreparedTile &item : result.tilePackages )
      item.keys.clear();
    mSummaryLabel->setText( tr( "数据加载已取消，未应用未完成的结果。" ) );
    return;
  }

  QList<QgsMapLayer *> packageLayers;
  QStringList failures;
  int failedItemCount = 0;
  for ( MtplPreparedTile &prepared : result.tilePackages )
  {
    if ( !prepared.ok )
    {
      ++failedItemCount;
      failures.append( prepared.error.isEmpty() ? tr( "%1 无法加载。" ).arg( QFileInfo( prepared.descriptor.path ).fileName() )
                                                 : prepared.error );
      prepared.keys.clear();
      continue;
    }

    const QFileInfo currentInfo( prepared.descriptor.path );
    if ( !packageStampMatches( prepared.descriptor ) )
    {
      ++failedItemCount;
      failures.append( tr( "%1 在后台检查完成后又发生变化。" ).arg( currentInfo.fileName() ) );
      prepared.keys.clear();
      continue;
    }

    auto *layer = new QgsMtplPluginLayer( prepared.descriptor, prepared.keys );
    prepared.keys.clear();
    if ( !layer->isValid() )
    {
      delete layer;
      ++failedItemCount;
      failures.append( tr( "%1 已不可用。" ).arg( currentInfo.fileName() ) );
      continue;
    }
    packageLayers.append( layer );
  }

  QgsProject *project = QgsProject::instance();
  QList<QgsMapLayer *> addedPackageLayers;
  if ( !packageLayers.isEmpty() && mProbe.isDirectorySelection )
  {
    addedPackageLayers = project->addMapLayers( packageLayers, false );
    if ( !addedPackageLayers.isEmpty() )
    {
      const QString groupName = QFileInfo( selectedPath() ).fileName().isEmpty()
                                ? QDir( selectedPath() ).dirName()
                                : QFileInfo( selectedPath() ).fileName();
      QgsLayerTreeGroup *group = project->layerTreeRoot()->addGroup( groupName );
      for ( QgsMapLayer *layer : std::as_const( addedPackageLayers ) )
        group->addLayer( layer );
    }
  }
  else if ( !packageLayers.isEmpty() )
  {
    addedPackageLayers = project->addMapLayers( packageLayers, true );
  }

  if ( addedPackageLayers.size() != packageLayers.size() )
  {
    const int notAddedCount = packageLayers.size() - addedPackageLayers.size();
    failures.append( tr( "%1 个可用数据包图层无法添加到工程。" ).arg( notAddedCount ) );
    failedItemCount += notAddedCount;
    for ( QgsMapLayer *layer : std::as_const( packageLayers ) )
    {
      if ( !addedPackageLayers.contains( layer ) )
        delete layer;
    }
  }

  int loadedLayerCount = addedPackageLayers.size();
  for ( MtplPreparedSfpEntry &prepared : result.sfpEntries )
  {
    if ( !prepared.ok )
    {
      ++failedItemCount;
      failures.append( tr( "%1: %2" ).arg( prepared.entryPath,
                                            prepared.error.isEmpty() ? tr( "无法提取所选 SFP 数据集。" ) : prepared.error ) );
      continue;
    }

    QList<QgsMapLayer *> addedEntryLayers;
    QString error;
    if ( loadPreparedSfpEntry( prepared.entryPath, prepared.extractedPath,
                               prepared.cacheDirectory, addedEntryLayers, error ) )
    {
      loadedLayerCount += addedEntryLayers.size();
    }
    else
    {
      ++failedItemCount;
      failures.append( tr( "%1: %2" ).arg( prepared.entryPath, error ) );
    }
  }

  mSummaryLabel->setText( mFinalProbeSummary );
  if ( loadedLayerCount == 0 )
  {
    emit loadFailed( failures.isEmpty() ? tr( "无法加载任何所选内容。" ) : failures.join( QLatin1Char( '\n' ) ) );
    return;
  }

  Q_UNUSED( singleSfpEntry )
  if ( failedItemCount > 0 )
    emit loadPartiallySucceeded( loadedLayerCount, failedItemCount, failures );
  else
    emit layersLoaded( loadedLayerCount );
}

void QgsMtplDockWidget::applyRememberedKeys()
{
  QgsMtpl::CryptoKeys candidate;
  candidate.privateKey = mPrivateKeyEdit->text().trimmed().toUtf8();
  candidate.deviceKey = mDeviceKeyEdit->text().trimmed().toUtf8();
  const bool matchesRememberedKey = mRememberedKeys.isValid() &&
                                    candidate.privateKey == mRememberedKeys.privateKey &&
                                    candidate.deviceKey == mRememberedKeys.deviceKey;
  QString validationError;
  if ( !validateEnteredKeys( candidate, validationError ) )
  {
    candidate.clear();
    emit loadFailed( validationError );
    return;
  }

  startProbe( selectedPath(), candidate, QgsMtpl::CredentialSource::Explicit,
              ProbePurpose::ValidateAndRememberKeys );
  mPendingMatchesRememberedKey = matchesRememberedKey;
  candidate.clear();
  hidePassword( mPrivateKeyEdit );
  hidePassword( mDeviceKeyEdit );
}

void QgsMtplDockWidget::clearRememberedKeys()
{
  QgsMtplCredentialStore::clearRememberedKeys();
  mRememberedKeys.clear();
  mKeys.clear();
  mKeySource = QgsMtpl::CredentialSource::None;
  mPrivateKeyEdit->clear();
  mDeviceKeyEdit->clear();
  mClearKeyButton->setEnabled( false );
  if ( mPackageToolsWidget )
    mPackageToolsWidget->reloadRememberedKeys();
  probeSelectedPath();
}

void QgsMtplDockWidget::importVectorStyle()
{
  if ( !mProbe.ok || mProbe.packages.isEmpty() )
    return;

  QTreeWidgetItem *item = mPackageTree->currentItem();
  bool indexOk = false;
  const int packageIndex = item ? item->data( 0, PACKAGE_INDEX_ROLE ).toInt( &indexOk ) : -1;
  if ( !indexOk || packageIndex < 0 || packageIndex >= mProbe.packages.size() )
    return;

  QFileDialog dialog( this, tr( "导入 Mapbox 样式" ), QString(), tr( "Mapbox 样式 (*.json);;JSON 文件 (*.json)" ) );
  dialog.setAcceptMode( QFileDialog::AcceptOpen );
  dialog.setFileMode( QFileDialog::ExistingFile );
  dialog.setOption( QFileDialog::DontUseNativeDialog, true );
  dialog.setOption( QFileDialog::ReadOnly, true );
  dialog.setLabelText( QFileDialog::LookIn, tr( "位置" ) );
  dialog.setLabelText( QFileDialog::FileName, tr( "文件名" ) );
  dialog.setLabelText( QFileDialog::FileType, tr( "文件类型" ) );
  dialog.setLabelText( QFileDialog::Accept, tr( "导入" ) );
  dialog.setLabelText( QFileDialog::Reject, tr( "取消" ) );
  if ( QDialogButtonBox *buttonBox = dialog.findChild<QDialogButtonBox *>() )
  {
    if ( QPushButton *cancelButton = buttonBox->button( QDialogButtonBox::Cancel ) )
      cancelButton->setText( tr( "取消" ) );
  }
  if ( dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty() )
    return;

  const QString stylePath = dialog.selectedFiles().constFirst();
  if ( stylePath.isEmpty() )
    return;

  mProbe.packages[packageIndex].stylePath = QFileInfo( stylePath ).absoluteFilePath();
  mImportStyleButton->setToolTip( tr( "样式已就绪：%1" ).arg( QFileInfo( stylePath ).fileName() ) );
  showSelectedPackageMetadata();
}

void QgsMtplDockWidget::showSelectedPackageMetadata()
{
  cancelPreviewWork();
  updateImportStyleState();
  populateOverrides();
  mOpenInQgisButton->hide();
  QTreeWidgetItem *item = mPackageTree->currentItem();
  if ( !item )
  {
    mDetailsSelectionLabel->clear();
    mMetadataTable->setRowCount( 0 );
    mPreviewText->clear();
    mPreviewText->hide();
    mPreviewImage->clear();
    mPreviewImage->hide();
    mOpenInQgisButton->hide();
    updateDetailsPageState();
    return;
  }

  bool ok = false;
  const int packageIndex = item->data( 0, PACKAGE_INDEX_ROLE ).toInt( &ok );
  if ( !ok || packageIndex < 0 || packageIndex >= mProbe.packages.size() )
  {
    mDetailsSelectionLabel->clear();
    mMetadataTable->setRowCount( 0 );
    mPreviewText->clear();
    mPreviewText->hide();
    mPreviewImage->clear();
    mPreviewImage->hide();
    updateDetailsPageState();
    return;
  }

  const QgsMtpl::PackageDescriptor &descriptor = mProbe.packages.at( packageIndex );
  const QString entryPath = item->data( 0, SFP_ENTRY_PATH_ROLE ).toString();
  const QString currentObject = entryPath.isEmpty() ? item->text( 0 ) : entryPath;
  mDetailsSelectionLabel->setText( tr( "当前对象：%1\n数据包：%2" )
                                    .arg( currentObject, QDir::toNativeSeparators( descriptor.path ) ) );
  QList<QPair<QString, QString>> values;
  values.append( { tr( "路径" ), QDir::toNativeSeparators( descriptor.path ) } );
  values.append( { tr( "格式" ), QgsMtpl::packageFormatName( descriptor.format ) } );
  values.append( { tr( "载荷类型" ), payloadTypeText( descriptor.payload ) } );
  values.append( { tr( "加密状态" ), encryptionStateText( descriptor.encryption ) } );
  values.append( { tr( "密钥状态" ), keyStatusText( descriptor ) } );
  values.append( { tr( "可用状态" ), readyStateText( descriptor ) } );
  if ( !descriptor.readinessMessage.isEmpty() )
    values.append( { tr( "状态说明" ), descriptor.readinessMessage } );
  if ( descriptor.isSpatial() )
  {
    values.append( { tr( "坐标参考系（CRS）" ), descriptor.crsAuthId } );
    values.append( { tr( "瓦片方案" ), descriptor.scheme.toUpper() } );
    values.append( { tr( "瓦片大小" ), QString::number( descriptor.tileSize ) } );
  }
  if ( !descriptor.stylePath.isEmpty() )
    values.append( { tr( "样式" ), QDir::toNativeSeparators( descriptor.stylePath ) } );
  for ( auto it = descriptor.metadata.constBegin(); it != descriptor.metadata.constEnd(); ++it )
    values.append( { it.key(), metadataValueText( it.value() ) } );

  if ( !entryPath.isEmpty() )
  {
    values.append( { tr( "SFP 条目" ), entryPath } );
    values.append( { tr( "条目加密状态" ), item->data( 0, SFP_ENTRY_ENCRYPTED_ROLE ).toBool() ? tr( "已加密" ) : tr( "未加密" ) } );
    values.append( { tr( "条目原始大小" ), humanSize( item->data( 0, SFP_ENTRY_LOGICAL_SIZE_ROLE ).toULongLong() ) } );
    values.append( { tr( "条目存储大小" ), humanSize( item->data( 0, SFP_ENTRY_STORED_SIZE_ROLE ).toULongLong() ) } );
  }

  mMetadataTable->setRowCount( values.size() );
  for ( int row = 0; row < values.size(); ++row )
  {
    mMetadataTable->setItem( row, 0, new QTableWidgetItem( values.at( row ).first ) );
    mMetadataTable->setItem( row, 1, new QTableWidgetItem( values.at( row ).second ) );
  }
  mMetadataTable->resizeRowsToContents();
  updateDetailsPageState();

  if ( descriptor.format == QgsMtpl::PackageFormat::Sfp && !entryPath.isEmpty() &&
       static_cast<PackageTreeItemKind>( item->data( 0, ITEM_KIND_ROLE ).toInt() ) == PackageTreeItemKind::SfpEntry )
  {
    startSfpPreview( descriptor, entryPath );
    mOpenInQgisButton->setVisible( item->data( 0, ITEM_READY_ROLE ).toBool() );
  }
  else
  {
    mPreviewText->clear();
    mPreviewText->hide();
    mPreviewImage->clear();
    mPreviewImage->hide();
    mOpenInQgisButton->hide();
  }
}

void QgsMtplDockWidget::updateImportStyleState()
{
  QTreeWidgetItem *item = mPackageTree->currentItem();
  if ( !item )
  {
    mImportStyleButton->setEnabled( false );
    mImportStyleButton->setToolTip( tr( "请选择可用的 PTP 矢量数据包或 VTP 数据包。" ) );
    return;
  }

  bool ok = false;
  const int packageIndex = item->data( 0, PACKAGE_INDEX_ROLE ).toInt( &ok );
  if ( !ok || packageIndex < 0 || packageIndex >= mProbe.packages.size() )
  {
    mImportStyleButton->setEnabled( false );
    mImportStyleButton->setToolTip( tr( "所选行不是数据包。" ) );
    return;
  }

  const QgsMtpl::PackageDescriptor &descriptor = mProbe.packages.at( packageIndex );
  const bool supported = descriptor.isReady() &&
                         ( descriptor.format == QgsMtpl::PackageFormat::Vtp ||
                           ( descriptor.format == QgsMtpl::PackageFormat::Ptp && descriptor.payload == QgsMtpl::PayloadType::VectorTile ) );
  mImportStyleButton->setEnabled( supported );
  if ( supported )
    mImportStyleButton->setToolTip( tr( "为所选数据包导入 Mapbox 样式。" ) );
  else if ( !descriptor.isReady() )
    mImportStyleButton->setToolTip( tr( "所选数据包可用后才能导入样式。" ) );
  else
    mImportStyleButton->setToolTip( tr( "Mapbox 样式仅适用于 PTP 矢量数据和 VTP 数据包。" ) );
}

void QgsMtplDockWidget::applyOverrides()
{
  const int packageIndex = currentPackageIndex();
  if ( !mProbe.ok || packageIndex < 0 )
    return;

  QgsMtpl::PackageDescriptor &descriptor = mProbe.packages[packageIndex];
  if ( !descriptor.isSpatial() )
  {
    populateOverrides();
    return;
  }

  const QString crsText = mCrsOverride->text().trimmed();
  const QgsCoordinateReferenceSystem crs( crsText );
  if ( crsText.isEmpty() || !crs.isValid() )
  {
    emit loadFailed( tr( "请输入有效的坐标参考系，例如 EPSG:3857。" ) );
    return;
  }

  bool noDataOk = true;
  double noDataValue = 0.0;
  const QString noDataText = mNoDataOverride->text().trimmed();
  if ( descriptor.format == QgsMtpl::PackageFormat::Dtp && !noDataText.isEmpty() )
    noDataValue = noDataText.toDouble( &noDataOk );
  if ( !noDataOk )
  {
    emit loadFailed( tr( "NoData 值必须留空或填写数字。" ) );
    return;
  }

  const QgsMtpl::PayloadType payload = static_cast<QgsMtpl::PayloadType>( mPayloadOverride->currentData().toInt() );
  const QString scheme = mSchemeOverride->currentData().toString();
  if ( descriptor.format == QgsMtpl::PackageFormat::Ptp )
    descriptor.payload = payload;
  descriptor.crsAuthId = crs.authid().isEmpty() ? crsText : crs.authid();
  descriptor.scheme = scheme;
  if ( descriptor.format == QgsMtpl::PackageFormat::Dtp )
  {
    descriptor.metadata.insert( QStringLiteral( "dataType" ), mDataTypeOverride->currentText() );
    descriptor.metadata.insert( QStringLiteral( "endianness" ), mEndiannessOverride->currentData().toString() );
    descriptor.scale = mScaleOverride->value();
    descriptor.offset = mOffsetOverride->value();
    descriptor.hasNoData = !noDataText.isEmpty();
    descriptor.noData = noDataValue;
  }
  descriptor.displayOverridesApplied = true;

  showSelectedPackageMetadata();
}

void QgsMtplDockWidget::clearProbe( const QString &summary )
{
  if ( mSfpPopulationTimer )
    mSfpPopulationTimer->stop();
  mSfpPopulation.reset();
  mFinalProbeSummary.clear();
  mProbe = QgsMtpl::ProbeResult();
  mLastFailureNeedsKeys = false;
  mLoadButton->setEnabled( false );
  mImportStyleButton->setEnabled( false );
  mOverridesGroup->hide();
  mPackageTree->clear();
  updateBatchSummary();
  mDetailsSelectionLabel->clear();
  mMetadataTable->setRowCount( 0 );
  mPreviewText->clear();
  mPreviewText->hide();
  mPreviewImage->clear();
  mPreviewImage->hide();
  mOpenInQgisButton->hide();
  updateDetailsPageState();
  if ( !summary.isNull() )
    mSummaryLabel->setText( summary );
}

void QgsMtplDockWidget::applyProbe( const QgsMtpl::ProbeResult &probe )
{
  mProbe = probe;
  mImportStyleButton->setEnabled( false );
  mImportStyleButton->setToolTip( tr( "请选择可用的 PTP 矢量数据包或 VTP 数据包。" ) );
  mLastFailureNeedsKeys = std::any_of( probe.packages.cbegin(), probe.packages.cend(), []( const QgsMtpl::PackageDescriptor &descriptor )
  {
    return descriptor.readiness == QgsMtpl::ReadinessState::KeyRequired ||
           descriptor.readiness == QgsMtpl::ReadinessState::KeyRejectedOrCorrupt;
  } );
  if ( !probe.ok )
  {
    mSummaryLabel->setText( probe.error.isEmpty() ? tr( "无法读取所选内容。" ) : probe.error );
    mPackageTree->clear();
    mDetailsSelectionLabel->clear();
    mMetadataTable->setRowCount( 0 );
    mOverridesGroup->hide();
    mPreviewText->clear();
    mPreviewText->hide();
    mPreviewImage->clear();
    mPreviewImage->hide();
    mOpenInQgisButton->hide();
    updateDetailsPageState();
    updateBatchSummary();
    updateLoadButtonState();
    return;
  }

  int plainCount = 0;
  int encryptedCount = 0;
  int lockedCount = 0;
  int unknownCount = 0;
  QStringList formatNames;
  for ( const QgsMtpl::PackageDescriptor &descriptor : probe.packages )
  {
    const QString formatName = QgsMtpl::packageFormatName( descriptor.format );
    if ( !formatNames.contains( formatName ) )
      formatNames.append( formatName );
    if ( descriptor.encryption == QgsMtpl::EncryptionState::Plain )
      ++plainCount;
    else if ( descriptor.encryption == QgsMtpl::EncryptionState::Encrypted )
      ++encryptedCount;
    else if ( descriptor.encryption == QgsMtpl::EncryptionState::Locked )
      ++lockedCount;
    else
      ++unknownCount;
  }

  QStringList securityParts;
  if ( plainCount > 0 )
    securityParts << tr( "%1 个未加密" ).arg( plainCount );
  if ( encryptedCount > 0 )
    securityParts << tr( "%1 个已加密" ).arg( encryptedCount );
  if ( lockedCount > 0 )
    securityParts << tr( "%1 个已锁定" ).arg( lockedCount );
  if ( unknownCount > 0 )
    securityParts << tr( "%1 个不可用" ).arg( unknownCount );
  const QString security = securityParts.join( QStringLiteral( "，" ) );

  const QString formatSummary = formatNames.size() == 1
    ? formatNames.constFirst()
    : tr( "混合格式（%1）" ).arg( formatNames.join( QStringLiteral( "、" ) ) );
  QString summary = tr( "%1 · 共 %2 个数据包 · %3" )
                      .arg( formatSummary )
                      .arg( probe.packages.size() )
                      .arg( security );
  mFinalProbeSummary = summary;
  mSummaryLabel->setText( mFinalProbeSummary );
  rebuildDetails();
  updateBatchSummary();
  updateLoadButtonState();
}

void QgsMtplDockWidget::updateBatchSummary()
{
  const int packageCount = mProbe.packages.size();
  int readyCount = 0;
  int keyRequiredCount = 0;
  int keyRejectedCount = 0;
  int unavailableCount = 0;
  for ( const QgsMtpl::PackageDescriptor &descriptor : std::as_const( mProbe.packages ) )
  {
    if ( descriptor.isReady() )
      ++readyCount;
    else if ( descriptor.readiness == QgsMtpl::ReadinessState::KeyRequired )
      ++keyRequiredCount;
    else if ( descriptor.readiness == QgsMtpl::ReadinessState::KeyRejectedOrCorrupt )
      ++keyRejectedCount;
    else
      ++unavailableCount;
  }

  mPackagesLabel->setText( tr( "数据包（%1）" ).arg( packageCount ) );
  QStringList summaryParts;
  summaryParts << tr( "%1 个可用" ).arg( readyCount );
  if ( keyRequiredCount > 0 )
    summaryParts << tr( "%1 个需要密钥" ).arg( keyRequiredCount );
  if ( keyRejectedCount > 0 )
    summaryParts << tr( "%1 个密钥验证失败" ).arg( keyRejectedCount );
  if ( unavailableCount > 0 )
    summaryParts << tr( "%1 个不可用" ).arg( unavailableCount );
  mBatchSummaryLabel->setText( summaryParts.join( QStringLiteral( " · " ) ) );
}

void QgsMtplDockWidget::populateOverrides()
{
  const int packageIndex = currentPackageIndex();
  if ( !mProbe.ok || packageIndex < 0 )
  {
    mOverridesGroup->hide();
    return;
  }

  const QgsMtpl::PackageDescriptor &descriptor = mProbe.packages.at( packageIndex );
  const bool spatialFormat = descriptor.isSpatial();
  mOverridesGroup->setVisible( spatialFormat );
  if ( !spatialFormat )
    return;

  const bool payloadCanBeChanged = descriptor.format == QgsMtpl::PackageFormat::Ptp;
  mPayloadOverrideLabel->setVisible( payloadCanBeChanged );
  mPayloadOverride->setVisible( payloadCanBeChanged );
  const int payloadIndex = mPayloadOverride->findData( static_cast<int>( descriptor.payload ) );
  mPayloadOverride->setCurrentIndex( payloadIndex < 0 ? 0 : payloadIndex );
  mCrsOverride->setText( descriptor.crsAuthId );
  const int schemeIndex = mSchemeOverride->findData( descriptor.scheme.toLower() );
  mSchemeOverride->setCurrentIndex( schemeIndex < 0 ? 0 : schemeIndex );

  const bool dtp = descriptor.format == QgsMtpl::PackageFormat::Dtp;
  mDtpOverrides->setVisible( dtp );
  if ( dtp )
  {
    const QString dataType = descriptor.metadata.value( QStringLiteral( "dataType" ), descriptor.metadata.value( QStringLiteral( "data_type" ), QStringLiteral( "uint16" ) ) ).toString().toLower();
    const int dataTypeIndex = mDataTypeOverride->findText( dataType );
    mDataTypeOverride->setCurrentIndex( dataTypeIndex < 0 ? 0 : dataTypeIndex );
    const QString endianness = descriptor.metadata.value( QStringLiteral( "endianness" ), QStringLiteral( "little" ) ).toString().toLower();
    const int endiannessIndex = mEndiannessOverride->findData( endianness );
    mEndiannessOverride->setCurrentIndex( endiannessIndex < 0 ? 0 : endiannessIndex );
    mScaleOverride->setValue( descriptor.scale );
    mOffsetOverride->setValue( descriptor.offset );
    mNoDataOverride->setText( descriptor.hasNoData ? QString::number( descriptor.noData, 'g', 17 ) : QString() );
  }
}

int QgsMtplDockWidget::currentPackageIndex() const
{
  if ( !mPackageTree )
    return -1;
  QTreeWidgetItem *item = mPackageTree->currentItem();
  bool ok = false;
  const int packageIndex = item ? item->data( 0, PACKAGE_INDEX_ROLE ).toInt( &ok ) : -1;
  return ok && packageIndex >= 0 && packageIndex < mProbe.packages.size() ? packageIndex : -1;
}

int QgsMtplDockWidget::maximumPackageNameColumnWidth() const
{
  if ( !mPackageTree || !mPackageTree->header() )
    return 0;
  return std::max( mPackageTree->header()->minimumSectionSize(), mPackageTree->viewport()->width() * 2 / 5 );
}

void QgsMtplDockWidget::updatePackageColumnWidths()
{
  if ( !mPackageTree || !mPackageTree->header() )
    return;
  mPackageTree->resizeColumnToContents( 0 );
  if ( mPackageTree->header()->sectionSize( 0 ) > maximumPackageNameColumnWidth() )
    mPackageTree->header()->resizeSection( 0, maximumPackageNameColumnWidth() );
}

void QgsMtplDockWidget::updateMetadataTableHeight()
{
  if ( !mMetadataTable || !mMetadataTable->horizontalHeader() )
    return;

  mMetadataTable->resizeRowsToContents();
  int height = mMetadataTable->horizontalHeader()->height() + 2 * mMetadataTable->frameWidth();
  for ( int row = 0; row < mMetadataTable->rowCount(); ++row )
    height += mMetadataTable->rowHeight( row );
  mMetadataTable->setFixedHeight( std::max( height, mMetadataTable->horizontalHeader()->height() + 2 * mMetadataTable->frameWidth() ) );
}

void QgsMtplDockWidget::updatePreviewTextHeight()
{
  if ( !mPreviewText )
    return;

  const QFontMetrics metrics( mPreviewText->font() );
  const int availableWidth = std::max( 1, mPreviewText->viewport()->width() );
  int textHeight = 0;
  const QStringList lines = mPreviewText->toPlainText().split( QLatin1Char( '\n' ), Qt::KeepEmptyParts );
  for ( const QString &line : lines )
  {
    const QString measuredLine = line.isEmpty() ? QStringLiteral( " " ) : line;
    const QRect bounds = metrics.boundingRect( QRect( 0, 0, availableWidth, std::numeric_limits<int>::max() ),
                                               Qt::AlignLeft | Qt::AlignTop | Qt::TextWrapAnywhere,
                                               measuredLine );
    textHeight += std::max( metrics.lineSpacing(), bounds.height() );
  }
  const int chromeHeight = 2 * mPreviewText->frameWidth() + 8;
  mPreviewText->setFixedHeight( std::max( 2 * metrics.lineSpacing() + chromeHeight, textHeight + chromeHeight ) );
}

void QgsMtplDockWidget::updateDetailsPageState()
{
  if ( !mDetailsStack )
    return;

  const bool hasSelection = currentPackageIndex() >= 0 && mMetadataTable && mMetadataTable->rowCount() > 0;
  mDetailsStack->setCurrentWidget( hasSelection ? static_cast<QWidget *>( mDetailsScrollArea )
                                                 : static_cast<QWidget *>( mDetailsEmptyLabel ) );
  if ( hasSelection )
    updateMetadataTableHeight();
}

void QgsMtplDockWidget::rebuildDetails()
{
  mSfpPopulationTimer->stop();
  mSfpPopulation.reset();
  const QSignalBlocker blocker( mPackageTree );
  mPackageTree->clear();
  auto population = std::make_unique<SfpPopulationState>();
  population->generation = mProbeGeneration;
  for ( int index = 0; index < mProbe.packages.size(); ++index )
  {
    const QgsMtpl::PackageDescriptor &descriptor = mProbe.packages.at( index );
    auto *item = new QTreeWidgetItem( mPackageTree, {
      QFileInfo( descriptor.path ).fileName(),
      QgsMtpl::packageFormatName( descriptor.format ),
      encryptionStateText( descriptor.encryption ),
      keyStatusText( descriptor ),
      readyStateText( descriptor )
    } );
    item->setData( 0, PACKAGE_INDEX_ROLE, index );
    item->setData( 0, ITEM_KIND_ROLE, static_cast<int>( PackageTreeItemKind::Package ) );
    item->setData( 0, ITEM_READY_ROLE, descriptor.isReady() );
    item->setToolTip( 0, QDir::toNativeSeparators( descriptor.path ) );
    item->setToolTip( 4, descriptor.readinessMessage );
    item->setFlags( item->flags() & ~Qt::ItemIsUserCheckable );
    if ( descriptor.isReady() && descriptor.isSpatial() )
    {
      item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
      item->setCheckState( 0, Qt::Checked );
    }
    if ( descriptor.format == QgsMtpl::PackageFormat::Sfp )
    {
      item->setExpanded( true );
      if ( !descriptor.sfpEntries.isEmpty() )
      {
        population->packageIndexes.append( index );
        population->totalEntries += descriptor.sfpEntries.size();
      }
      else if ( descriptor.isReady() )
      {
        auto *empty = new QTreeWidgetItem( item, { tr( "SFP 不含可显示的安全条目" ) } );
        empty->setData( 0, ITEM_KIND_ROLE, static_cast<int>( PackageTreeItemKind::Informational ) );
        empty->setDisabled( true );
      }
      const quint64 unsafeCount = descriptor.metadata.value( QStringLiteral( "unsafeEntryCount" ) ).toULongLong();
      if ( unsafeCount > 0 )
      {
        auto *unsafe = new QTreeWidgetItem( item, { tr( "已忽略 %1 个路径不安全的条目" ).arg( unsafeCount ) } );
        unsafe->setData( 0, ITEM_KIND_ROLE, static_cast<int>( PackageTreeItemKind::Informational ) );
        unsafe->setDisabled( true );
      }
    }
  }
  if ( mProbe.ignoredFileCount > 0 )
  {
    auto *ignored = new QTreeWidgetItem( mPackageTree, { tr( "已忽略 %1 个无关文件" ).arg( mProbe.ignoredFileCount ) } );
    ignored->setData( 0, ITEM_KIND_ROLE, static_cast<int>( PackageTreeItemKind::Informational ) );
    ignored->setToolTip( 0, ignored->text( 0 ) );
    ignored->setDisabled( true );
  }
  if ( mPackageTree->topLevelItemCount() > 0 )
  {
    mPackageTree->setCurrentItem( mPackageTree->topLevelItem( 0 ) );
    showSelectedPackageMetadata();
  }
  updatePackageColumnWidths();
  if ( population->totalEntries > 0 )
  {
    const qsizetype totalEntries = population->totalEntries;
    population->totalWork = static_cast<quint64>( totalEntries ) * 3;
    mSfpPopulation = std::move( population );
    mCancelProbeButton->setEnabled( true );
    mCancelProbeButton->show();
    mSummaryLabel->setText( tr( "数据包检查完成，正在整理 %1 个 SFP 条目… 0%" ).arg( totalEntries ) );
    mSfpPopulationTimer->start();
  }
  else
  {
    mCancelProbeButton->hide();
  }
}

void QgsMtplDockWidget::updateLoadButtonState()
{
  bool hasSelection = false;
  int selectedSfpDatasetCount = 0;
  QTreeWidgetItemIterator iterator( mPackageTree );
  while ( *iterator )
  {
    QTreeWidgetItem *item = *iterator;
    ++iterator;
    const PackageTreeItemKind kind = static_cast<PackageTreeItemKind>( item->data( 0, ITEM_KIND_ROLE ).toInt() );
    if ( ( kind == PackageTreeItemKind::Package || kind == PackageTreeItemKind::SfpEntry ) &&
         item->data( 0, ITEM_READY_ROLE ).toBool() && item->checkState( 0 ) == Qt::Checked )
    {
      hasSelection = true;
      if ( kind == PackageTreeItemKind::SfpEntry )
        ++selectedSfpDatasetCount;
    }
  }
  const bool sfpSelection = !mProbe.packages.isEmpty() &&
                            std::all_of( mProbe.packages.cbegin(), mProbe.packages.cend(), []( const QgsMtpl::PackageDescriptor &descriptor )
  {
    return descriptor.format == QgsMtpl::PackageFormat::Sfp;
  } );
  if ( sfpSelection )
    mLoadButton->setText( tr( "加载已选数据集（%1）" ).arg( selectedSfpDatasetCount ) );
  else if ( mProbe.isDirectorySelection )
    mLoadButton->setText( tr( "加载可用数据包" ) );
  else
    mLoadButton->setText( tr( "加载到地图" ) );
  mLoadButton->setEnabled( mProbe.ok && hasSelection && !mSfpPopulation );
}

void QgsMtplDockWidget::populateSfpEntriesBatch()
{
  if ( !mSfpPopulation || mSfpPopulation->generation != mProbeGeneration )
    return;

  QElapsedTimer budget;
  budget.start();
  const QSignalBlocker blocker( mPackageTree );
  while ( mSfpPopulation && mSfpPopulation->packageListIndex < mSfpPopulation->packageIndexes.size() && budget.elapsed() < 12 )
  {
    SfpPopulationState &state = *mSfpPopulation;
    const int packageIndex = state.packageIndexes.at( state.packageListIndex );
    if ( packageIndex < 0 || packageIndex >= mProbe.packages.size() || packageIndex >= mPackageTree->topLevelItemCount() )
    {
      ++state.packageListIndex;
      state.phase = 0;
      state.phaseEntry = 0;
      state.nextEntry = 0;
      continue;
    }

    const QgsMtpl::PackageDescriptor &descriptor = mProbe.packages.at( packageIndex );
    QTreeWidgetItem *packageItem = mPackageTree->topLevelItem( packageIndex );
    if ( state.phase == 0 )
    {
      while ( state.phaseEntry < descriptor.sfpEntries.size() && budget.elapsed() < 12 )
      {
        const QString &path = descriptor.sfpEntries.at( state.phaseEntry ).path;
        if ( QFileInfo( path ).suffix().compare( QLatin1String( "shp" ), Qt::CaseInsensitive ) == 0 )
          state.shapefileDatasets.insert( shapefileDatasetKey( path ) );
        if ( isRasterDatasetEntry( path ) )
        {
          const QString datasetKey = normalizedSfpPathKey( path );
          state.rasterDatasetReady.insert( datasetKey, true );
          const QStringList companionKeys = rasterCompanionPathKeys( path );
          for ( const QString &companionKey : companionKeys )
          {
            QStringList &datasets = state.rasterCompanionDatasets[companionKey];
            if ( !datasets.contains( datasetKey ) )
              datasets.append( datasetKey );
          }
        }
        ++state.phaseEntry;
        ++state.completedWork;
      }
      if ( state.phaseEntry == descriptor.sfpEntries.size() )
      {
        state.phase = 1;
        state.phaseEntry = 0;
      }
    }
    else if ( state.phase == 1 )
    {
      while ( state.phaseEntry < descriptor.sfpEntries.size() && budget.elapsed() < 12 )
      {
        const QgsMtpl::SfpEntryDescriptor &entry = descriptor.sfpEntries.at( state.phaseEntry );
        const bool entryReady = !entry.encrypted || descriptor.readiness == QgsMtpl::ReadinessState::KeyVerified;
        const QString shapefileKey = shapefileDatasetKey( entry.path );
        if ( state.shapefileDatasets.contains( shapefileKey ) )
          state.shapefileDatasetReady.insert( shapefileKey, state.shapefileDatasetReady.value( shapefileKey, true ) && entryReady );
        const QString entryKey = normalizedSfpPathKey( entry.path );
        if ( state.rasterDatasetReady.contains( entryKey ) )
          state.rasterDatasetReady.insert( entryKey, state.rasterDatasetReady.value( entryKey, true ) && entryReady );
        const QStringList companionDatasets = state.rasterCompanionDatasets.value( entryKey );
        for ( const QString &datasetKey : companionDatasets )
        {
          state.rasterDatasetReady.insert( datasetKey, state.rasterDatasetReady.value( datasetKey, true ) && entryReady );
        }
        ++state.phaseEntry;
        ++state.completedWork;
      }
      if ( state.phaseEntry == descriptor.sfpEntries.size() )
      {
        state.phase = 2;
        state.nextEntry = 0;
      }
    }
    else
    {
      while ( state.nextEntry < descriptor.sfpEntries.size() && budget.elapsed() < 12 )
      {
        addCurrentSfpEntry( packageItem, descriptor, descriptor.sfpEntries.at( state.nextEntry ) );
        ++state.nextEntry;
        ++state.processedEntries;
        ++state.completedWork;
      }
      if ( state.nextEntry == descriptor.sfpEntries.size() )
      {
        ++state.packageListIndex;
        state.phase = 0;
        state.phaseEntry = 0;
        state.nextEntry = 0;
        state.shapefileDatasets.clear();
        state.rasterCompanionDatasets.clear();
        state.shapefileDatasetReady.clear();
        state.rasterDatasetReady.clear();
        state.treeItems.clear();
      }
    }
  }

  if ( !mSfpPopulation )
    return;
  if ( mSfpPopulation->packageListIndex >= mSfpPopulation->packageIndexes.size() )
  {
    finishSfpPopulation();
    return;
  }

  const int percent = mSfpPopulation->totalWork == 0
    ? 100
    : static_cast<int>( std::min<quint64>( 100, mSfpPopulation->completedWork * 100 / mSfpPopulation->totalWork ) );
  mSummaryLabel->setText( tr( "数据包检查完成，正在整理 %1 个 SFP 条目… %2%" )
                            .arg( mSfpPopulation->totalEntries ).arg( percent ) );
  mSfpPopulationTimer->start();
}

void QgsMtplDockWidget::finishSfpPopulation()
{
  mSfpPopulationTimer->stop();
  mSfpPopulation.reset();
  mCancelProbeButton->hide();
  mSummaryLabel->setText( mFinalProbeSummary );
  updatePackageColumnWidths();
  updateLoadButtonState();
}

void QgsMtplDockWidget::addCurrentSfpEntry( QTreeWidgetItem *packageItem,
                                            const QgsMtpl::PackageDescriptor &descriptor,
                                            const QgsMtpl::SfpEntryDescriptor &entry )
{
  const QString &safePath = entry.path;
  const QStringList parts = safePath.split( QLatin1Char( '/' ), Qt::SkipEmptyParts );
  QTreeWidgetItem *parent = packageItem;
  QString itemPath;
  for ( int partIndex = 0; partIndex < parts.size(); ++partIndex )
  {
    if ( !itemPath.isEmpty() )
      itemPath.append( QLatin1Char( '/' ) );
    itemPath.append( parts.at( partIndex ) );
    QTreeWidgetItem *child = mSfpPopulation->treeItems.value( itemPath );
    if ( !child )
    {
      child = new QTreeWidgetItem( parent, { parts.at( partIndex ) } );
      child->setFlags( child->flags() & ~Qt::ItemIsUserCheckable );
      mSfpPopulation->treeItems.insert( itemPath, child );
    }
    parent = child;
    parent->setData( 0, PACKAGE_INDEX_ROLE, packageItem->data( 0, PACKAGE_INDEX_ROLE ) );
    parent->setToolTip( 0, parts.mid( 0, partIndex + 1 ).join( QLatin1Char( '/' ) ) );
    if ( partIndex != parts.size() - 1 )
    {
      if ( !parent->data( 0, ITEM_KIND_ROLE ).isValid() )
        parent->setData( 0, ITEM_KIND_ROLE, static_cast<int>( PackageTreeItemKind::Folder ) );
      continue;
    }

    const bool shapefileCompanion = isShapefileSidecar( safePath ) &&
                                     mSfpPopulation->shapefileDatasets.contains( shapefileDatasetKey( safePath ) );
    const QString rasterPathKey = normalizedSfpPathKey( safePath );
    const bool rasterCompanion = mSfpPopulation->rasterCompanionDatasets.contains( rasterPathKey );
    const bool companion = shapefileCompanion || rasterCompanion;
    bool entryReady = !entry.encrypted || descriptor.readiness == QgsMtpl::ReadinessState::KeyVerified;
    if ( QFileInfo( safePath ).suffix().compare( QLatin1String( "shp" ), Qt::CaseInsensitive ) == 0 )
      entryReady = mSfpPopulation->shapefileDatasetReady.value( shapefileDatasetKey( safePath ), entryReady );
    else if ( isRasterDatasetEntry( safePath ) )
      entryReady = mSfpPopulation->rasterDatasetReady.value( rasterPathKey, entryReady );
    const bool gisCandidate = isPotentialGisDatasetEntry( safePath );
    if ( !companion && !gisCandidate )
      entryReady = false;

    parent->setText( 1, QFileInfo( safePath ).suffix().toUpper() );
    parent->setText( 2, entry.encrypted
                          ? ( descriptor.readiness == QgsMtpl::ReadinessState::KeyVerified ? tr( "已加密" ) : tr( "已锁定" ) )
                          : tr( "未加密" ) );
    parent->setText( 3, entry.encrypted ? keyStatusText( descriptor ) : tr( "无需密钥" ) );
    if ( companion )
      parent->setText( 4, rasterCompanion ? tr( "随栅格一同加载" ) : tr( "随 .shp 一同加载" ) );
    else if ( entryReady )
      parent->setText( 4, tr( "待加载验证 · %1" ).arg( humanSize( entry.logicalSize ) ) );
    else if ( !gisCandidate )
      parent->setText( 4, tr( "不是已知 GIS 数据集" ) );
    else
      parent->setText( 4, readyStateText( descriptor ) );
    parent->setData( 0, SFP_ENTRY_PATH_ROLE, safePath );
    parent->setData( 0, ITEM_KIND_ROLE, static_cast<int>( companion ? PackageTreeItemKind::Companion : PackageTreeItemKind::SfpEntry ) );
    parent->setData( 0, ITEM_READY_ROLE, entryReady && !companion );
    parent->setData( 0, SFP_ENTRY_ENCRYPTED_ROLE, entry.encrypted );
    parent->setData( 0, SFP_ENTRY_LOGICAL_SIZE_ROLE, QVariant::fromValue<qulonglong>( entry.logicalSize ) );
    parent->setData( 0, SFP_ENTRY_STORED_SIZE_ROLE, QVariant::fromValue<qulonglong>( entry.storedSize ) );
    parent->setFlags( parent->flags() & ~Qt::ItemIsUserCheckable );
    if ( entryReady && !companion )
    {
      parent->setFlags( parent->flags() | Qt::ItemIsUserCheckable );
      parent->setCheckState( 0, Qt::Unchecked );
      parent->setToolTip( 4, tr( "实际加载时将由 QGIS 数据提供程序验证此条目。" ) );
    }
    else if ( companion )
    {
      parent->setToolTip( 4, rasterCompanion
                                ? tr( "此配套文件会与匹配的栅格条目一同提取并加载。" )
                                : tr( "此配套文件会与匹配的 .shp 条目一同提取并加载。" ) );
    }
  }
}

void QgsMtplDockWidget::startSfpPreview( const QgsMtpl::PackageDescriptor &descriptor, const QString &entryPath )
{
  const QString suffix = QFileInfo( entryPath ).suffix().toLower();
  const bool textPreview = suffix == QLatin1String( "json" ) || suffix == QLatin1String( "txt" ) ||
                           suffix == QLatin1String( "xml" ) || suffix == QLatin1String( "csv" ) ||
                           suffix == QLatin1String( "md" ) || suffix == QLatin1String( "yaml" ) ||
                           suffix == QLatin1String( "yml" );
  const bool imagePreview = suffix == QLatin1String( "png" ) || suffix == QLatin1String( "jpg" ) ||
                            suffix == QLatin1String( "jpeg" ) || suffix == QLatin1String( "webp" );
  if ( !textPreview && !imagePreview )
  {
    mPreviewText->clear();
    mPreviewText->hide();
    mPreviewImage->clear();
    mPreviewImage->hide();
    return;
  }

  QgsMtpl::CryptoKeys suppliedKeys = suppliedKeysForDescriptor( descriptor );
  auto *task = new MtplSfpPreviewTask( descriptor, entryPath, suppliedKeys );
  suppliedKeys.clear();
  const quint64 generation = mPreviewGeneration;
  mPreviewTask = task;
  mPreviewImage->clear();
  mPreviewImage->hide();
  mPreviewText->setPlainText( tr( "正在后台读取 SFP 预览… 0%" ) );
  mPreviewText->show();
  mCancelProbeButton->setEnabled( true );
  mCancelProbeButton->setText( tr( "取消预览" ) );
  mCancelProbeButton->show();

  connect( task, &QgsTask::progressChanged, this, [this, task, generation]( double value )
  {
    if ( generation != mPreviewGeneration || mPreviewTask != task )
      return;
    mPreviewText->setPlainText( tr( "正在后台读取 SFP 预览… %1%" ).arg( qRound( value ) ) );
  } );
  const auto finished = [this, task, generation, packagePath = descriptor.path, entryPath]
  {
    previewFinished( task, generation, packagePath, entryPath );
  };
  connect( task, &QgsTask::taskCompleted, this, finished );
  connect( task, &QgsTask::taskTerminated, this, finished );
  QgsApplication::taskManager()->addTask( task );
}

void QgsMtplDockWidget::previewFinished( QgsTask *task,
                                         quint64 generation,
                                         const QString &packagePath,
                                         const QString &entryPath )
{
  Q_ASSERT( QThread::currentThread() == QgsApplication::instance()->thread() );
  if ( generation != mPreviewGeneration || mPreviewTask != task || !task )
    return;

  auto *previewTask = static_cast<MtplSfpPreviewTask *>( task );
  const MtplPreviewResult &result = previewTask->result();
  const bool canceled = previewTask->cancellationRequested() || result.canceled;
  mPreviewTask = nullptr;
  mCancelProbeButton->setEnabled( true );
  mCancelProbeButton->setText( tr( "取消检查" ) );
  mCancelProbeButton->hide();

  QTreeWidgetItem *item = mPackageTree->currentItem();
  bool indexOk = false;
  const int packageIndex = item ? item->data( 0, PACKAGE_INDEX_ROLE ).toInt( &indexOk ) : -1;
  const QString currentEntryPath = item ? item->data( 0, SFP_ENTRY_PATH_ROLE ).toString() : QString();
  if ( !indexOk || packageIndex < 0 || packageIndex >= mProbe.packages.size() ||
       mProbe.packages.at( packageIndex ).path != packagePath || currentEntryPath != entryPath )
    return;

  mPreviewText->clear();
  mPreviewText->hide();
  mPreviewImage->clear();
  mPreviewImage->hide();
  if ( canceled )
  {
    mPreviewText->setPlainText( tr( "SFP 预览已取消。" ) );
    mPreviewText->show();
    return;
  }
  if ( !result.ok )
  {
    mPreviewText->setPlainText( result.error.isEmpty() ? tr( "无法生成 SFP 预览。" ) : result.error );
    mPreviewText->show();
    return;
  }

  const QString suffix = QFileInfo( entryPath ).suffix().toLower();
  const bool textPreview = suffix == QLatin1String( "json" ) || suffix == QLatin1String( "txt" ) ||
                           suffix == QLatin1String( "xml" ) || suffix == QLatin1String( "csv" ) ||
                           suffix == QLatin1String( "md" ) || suffix == QLatin1String( "yaml" ) ||
                           suffix == QLatin1String( "yml" );
  if ( textPreview )
  {
    QByteArray bytes = result.bytes;
    const bool truncated = result.logicalSize > static_cast<quint64>( bytes.size() );
    mPreviewText->setPlainText( QString::fromUtf8( bytes ) +
                                ( truncated ? tr( "\n\n预览内容已截断为前 1 MiB。" ) : QString() ) );
    mPreviewText->show();
    bytes.fill( '\0' );
    bytes.clear();
  }
  else
  {
    if ( !result.image.isNull() )
    {
      mPreviewImage->setPixmap( QPixmap::fromImage( result.image ) );
      mPreviewImage->show();
    }
    else
    {
      mPreviewText->setPlainText( tr( "无法解码所选图像预览。" ) );
      mPreviewText->show();
    }
  }
}

bool QgsMtplDockWidget::ensureSfpCache( QString &cacheRoot, QString &error )
{
  cacheRoot.clear();
  error.clear();
  if ( !mSfpCache )
  {
    const QString cacheTemplate = QDir( QDir::tempPath() ).filePath( QStringLiteral( "qgis-mtpl-sfp-XXXXXX" ) );
    auto cache = std::make_unique<QTemporaryDir>( cacheTemplate );
    if ( !cache->isValid() || isReparseOrLink( QFileInfo( cache->path() ) ) )
    {
      error = tr( "无法创建安全的临时缓存。" );
      return false;
    }
    cache->setAutoRemove( true );
    mSfpCache = std::move( cache );
  }

  const QFileInfo cacheInfo( mSfpCache->path() );
  cacheRoot = cacheInfo.canonicalFilePath();
  if ( cacheRoot.isEmpty() || !cacheInfo.isDir() || isReparseOrLink( cacheInfo ) )
  {
    cacheRoot.clear();
    error = tr( "临时缓存已不再安全，无法继续使用。" );
    return false;
  }
  return true;
}
void QgsMtplDockWidget::openSelectedSfpEntry()
{
  QTreeWidgetItem *item = mPackageTree->currentItem();
  if ( !item )
    return;

  bool ok = false;
  const int packageIndex = item->data( 0, PACKAGE_INDEX_ROLE ).toInt( &ok );
  const QString entryPath = item->data( 0, SFP_ENTRY_PATH_ROLE ).toString();
  if ( !ok || packageIndex < 0 || packageIndex >= mProbe.packages.size() || entryPath.isEmpty() ||
       static_cast<PackageTreeItemKind>( item->data( 0, ITEM_KIND_ROLE ).toInt() ) != PackageTreeItemKind::SfpEntry )
    return;

  const QgsMtpl::PackageDescriptor &descriptor = mProbe.packages.at( packageIndex );
  if ( descriptor.format != QgsMtpl::PackageFormat::Sfp || !item->data( 0, ITEM_READY_ROLE ).toBool() )
    return;

  cancelPreviewWork();
  MtplLoadPreparationRequest request;
  QString cacheError;
  if ( !ensureSfpCache( request.cacheRoot, cacheError ) )
  {
    emit loadFailed( cacheError );
    return;
  }
  MtplSfpLoadInput input;
  input.descriptor = descriptor;
  input.entryPath = entryPath;
  input.suppliedKeys = suppliedKeysForDescriptor( descriptor );
  request.sfpEntries.append( std::move( input ) );
  startLoadPreparationTask( new MtplLoadPreparationTask( request ), true );
  request.sfpEntries[0].suppliedKeys.clear();
}

bool QgsMtplDockWidget::loadPreparedSfpEntry( const QString &entryPath,
                                              const QString &extractedPath,
                                              const QString &cacheDirectory,
                                              QList<QgsMapLayer *> &addedLayers,
                                              QString &error )
{
  Q_ASSERT( QThread::currentThread() == QgsApplication::instance()->thread() );
  addedLayers.clear();
  error.clear();
  if ( extractedPath.isEmpty() || cacheDirectory.isEmpty() )
  {
    error = tr( "后台任务未返回可用的 SFP 提取结果。" );
    return false;
  }

  const QString cacheRoot = mSfpCache ? QFileInfo( mSfpCache->path() ).canonicalFilePath() : QString();

  const QStringList providerKeys { QStringLiteral( "ogr" ), QStringLiteral( "gdal" ) };

  QList<QgsMapLayer *> layers;
  QSet<QString> seenSublayers;
  const QgsProviderSublayerDetails::LayerOptions layerOptions( QgsProject::instance()->transformContext() );
  for ( const QString &providerKey : std::as_const( providerKeys ) )
  {
    QgsProviderMetadata *metadata = QgsProviderRegistry::instance()->providerMetadata( providerKey );
    if ( !metadata )
      continue;
    const QList<QgsProviderSublayerDetails> sublayers = metadata->querySublayers( extractedPath, Qgis::SublayerQueryFlag::ResolveGeometryType );
    for ( const QgsProviderSublayerDetails &sublayer : sublayers )
    {
      if ( sublayer.type() != Qgis::LayerType::Vector && sublayer.type() != Qgis::LayerType::Raster )
        continue;
      const QString identity = QStringLiteral( "%1\n%2\n%3" ).arg( sublayer.providerKey(), sublayer.uri() ).arg( static_cast<int>( sublayer.type() ) );
      if ( seenSublayers.contains( identity ) )
        continue;
      seenSublayers.insert( identity );

      QgsMapLayer *layer = sublayer.toLayer( layerOptions );
      if ( !layer || !layer->isValid() )
      {
        delete layer;
        continue;
      }
      if ( layer->name().trimmed().isEmpty() )
        layer->setName( QFileInfo( entryPath ).completeBaseName() );
      if ( QgsVectorLayer *vectorLayer = qobject_cast<QgsVectorLayer *>( layer ) )
        vectorLayer->setReadOnly( true );
      layers.append( layer );
    }
  }

  if ( layers.isEmpty() )
  {
    removePreparedDirectory( cacheRoot, cacheDirectory );
    error = tr( "QGIS 数据提供程序未将提取的 SFP 条目标识为受支持的 GIS 数据集。" );
    return false;
  }

  addedLayers = QgsProject::instance()->addMapLayers( layers );
  for ( QgsMapLayer *layer : addedLayers )
    mSfpNativeLayerIds.insert( layer->id() );
  if ( addedLayers.isEmpty() )
  {
    removePreparedDirectory( cacheRoot, cacheDirectory );
    error = tr( "无法将提取的 SFP 数据集添加到工程。" );
    return false;
  }
  return true;
}

void QgsMtplDockWidget::cleanupSfpCache()
{
  if ( !mSfpCache )
    return;

  const QString cacheRoot = QDir::cleanPath( QDir::fromNativeSeparators( mSfpCache->path() ) );
  QStringList layersToRemove;
  const QMap<QString, QgsMapLayer *> projectLayers = QgsProject::instance()->mapLayers();
  for ( auto iterator = projectLayers.constBegin(); iterator != projectLayers.constEnd(); ++iterator )
  {
    const QString source = QDir::cleanPath( QDir::fromNativeSeparators( iterator.value()->source() ) );
    if ( mSfpNativeLayerIds.contains( iterator.key() ) || isWithinRoot( cacheRoot, source ) )
      layersToRemove.append( iterator.key() );
  }
  if ( !layersToRemove.isEmpty() )
    QgsProject::instance()->removeMapLayers( layersToRemove );

  mSfpNativeLayerIds.clear();
  mSfpCache.reset();
}

QgsMtpl::CryptoKeys QgsMtplDockWidget::suppliedKeysForDescriptor( const QgsMtpl::PackageDescriptor &descriptor ) const
{
  if ( descriptor.credentialSource == QgsMtpl::CredentialSource::Explicit && mKeys.isValid() )
    return mKeys;
  if ( descriptor.credentialSource == QgsMtpl::CredentialSource::Remembered && mRememberedKeys.isValid() )
    return mRememberedKeys;
  return QgsMtpl::CryptoKeys();
}
