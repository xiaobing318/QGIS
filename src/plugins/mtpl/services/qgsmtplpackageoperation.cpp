/***************************************************************************
  qgsmtplpackageoperation.cpp
  ---------------------------
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

#include "qgsmtplpackageoperation.h"
#include "qgsmtplpackageservice.h"
#include "qgsmtplstatus.h"
#include "qgsmtpltileset.h"

#include <mtpl/mtpl.h>
#include "sfp_internal.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace
{
  QMutex sActiveTaskMutex;
  QSet<QgsMtplPackageOperationTask *> sActiveTasks;

  struct TileSource
  {
    mtpl_tile_coordinate_t coordinate = {};
    QString path;
  };

  struct SfpSource
  {
    QString packagePath;
    QString sourcePath;
    uint64_t logicalSize = 0;
  };

  class CryptoContext
  {
    public:
      explicit CryptoContext( const QgsMtpl::CryptoKeys &keys )
        : mPrivateKey( keys.privateKey )
        , mDeviceKey( keys.deviceKey )
      {
        mOptions.private_key = reinterpret_cast<const uint8_t *>( mPrivateKey.constData() );
        mOptions.private_key_size = static_cast<size_t>( mPrivateKey.size() );
        mOptions.device_key = reinterpret_cast<const uint8_t *>( mDeviceKey.constData() );
        mOptions.device_key_size = static_cast<size_t>( mDeviceKey.size() );
      }

      const mtpl_crypto_options_t *get() const
      {
        return mPrivateKey.isEmpty() || mDeviceKey.isEmpty() ? nullptr : &mOptions;
      }

    private:
      QByteArray mPrivateKey;
      QByteArray mDeviceKey;
      mtpl_crypto_options_t mOptions = {};
  };

  bool canceled( const QgsMtpl::PackageOperations::CancelCheck &check )
  {
    return check && check();
  }

  struct TranscodeCallbackContext
  {
    const QgsMtpl::PackageOperations::CancelCheck *cancelCheck = nullptr;
    const QgsMtpl::PackageOperations::ProgressCallback *progress = nullptr;
    double progressStart = 0.0;
    double progressEnd = 100.0;
  };

  bool transcodeCanceled( void *userData )
  {
    const auto *context = static_cast<const TranscodeCallbackContext *>( userData );
    return context && context->cancelCheck && canceled( *context->cancelCheck );
  }

  void transcodeProgress( uint64_t completed, uint64_t total, void *userData )
  {
    const auto *context = static_cast<const TranscodeCallbackContext *>( userData );
    if ( !context || !context->progress || !( *context->progress ) )
      return;
    const double fraction = total == 0 ? 1.0 : std::clamp( static_cast<double>( completed ) / static_cast<double>( total ), 0.0, 1.0 );
    ( *context->progress )( context->progressStart + ( context->progressEnd - context->progressStart ) * fraction );
  }

  void reportProgress( const QgsMtpl::PackageOperations::ProgressCallback &progress,
                       double progressStart,
                       double progressEnd,
                       uint64_t completed,
                       uint64_t total )
  {
    if ( !progress )
      return;
    const double fraction = total == 0 ? 1.0 : std::clamp( static_cast<double>( completed ) / static_cast<double>( total ), 0.0, 1.0 );
    progress( progressStart + ( progressEnd - progressStart ) * fraction );
  }

  QString statusError( const QString &action, mtpl_status_t status )
  {
    return QgsMtpl::statusError( action, status );
  }

  QgsMtpl::PackageFormat packageFormatFromMtpl( mtpl_package_format_t format )
  {
    switch ( format )
    {
      case MTPL_PACKAGE_FORMAT_PTP:
        return QgsMtpl::PackageFormat::Ptp;
      case MTPL_PACKAGE_FORMAT_DTP:
        return QgsMtpl::PackageFormat::Dtp;
      case MTPL_PACKAGE_FORMAT_VTP:
        return QgsMtpl::PackageFormat::Vtp;
      case MTPL_PACKAGE_FORMAT_SFP:
        return QgsMtpl::PackageFormat::Sfp;
      case MTPL_PACKAGE_FORMAT_UNKNOWN:
        return QgsMtpl::PackageFormat::Unknown;
    }
    return QgsMtpl::PackageFormat::Unknown;
  }

  bool probePackageFormat( const QString &path, QgsMtpl::PackageFormat &format, QString &error )
  {
    mtpl_package_info_t info = MTPL_PACKAGE_INFO_INIT;
    const QByteArray pathUtf8 = path.toUtf8();
    const mtpl_status_t status = mtpl_package_probe( pathUtf8.constData(), &info );
    format = status == MTPL_STATUS_OK ? packageFormatFromMtpl( info.format ) : QgsMtpl::PackageFormat::Unknown;
    if ( status == MTPL_STATUS_OK && format != QgsMtpl::PackageFormat::Unknown )
      return true;

    error = status == MTPL_STATUS_OK
      ? QStringLiteral( "不支持源数据包的格式。" )
      : statusError( QStringLiteral( "无法检查源数据包" ), status );
    return false;
  }

  QString stagingFilePath( const QString &outputPath )
  {
    const QFileInfo outputInfo( outputPath );
    return QDir( outputInfo.absolutePath() ).filePath(
      QStringLiteral( ".%1.mtpl-staging-%2" ).arg( outputInfo.fileName(), QUuid::createUuid().toString( QUuid::WithoutBraces ) ) );
  }

  bool prepareOutputFile( const QString &outputPath, QgsMtpl::PackageFormat format, QString &error )
  {
    const QFileInfo outputInfo( outputPath );
    if ( outputPath.trimmed().isEmpty() || outputInfo.fileName().isEmpty() )
    {
      error = QStringLiteral( "请选择输出数据包路径。" );
      return false;
    }
    if ( outputInfo.exists() )
    {
      error = QStringLiteral( "输出数据包已存在。" );
      return false;
    }
    if ( !QFileInfo( outputInfo.absolutePath() ).isDir() )
    {
      error = QStringLiteral( "输出路径的上级文件夹不存在。" );
      return false;
    }
    if ( QgsMtpl::packageFormatFromPath( outputInfo.fileName() ) != format )
    {
      error = QStringLiteral( "输出文件扩展名与数据包格式不匹配。" );
      return false;
    }
    return true;
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

  bool isWithinRoot( const QString &rootCanonical, const QString &candidateCanonical )
  {
    QString rootPrefix = QDir::cleanPath( QDir::fromNativeSeparators( rootCanonical ) );
    if ( !rootPrefix.endsWith( QLatin1Char( '/' ) ) )
      rootPrefix.append( QLatin1Char( '/' ) );
    const QString candidate = QDir::cleanPath( QDir::fromNativeSeparators( candidateCanonical ) );
#ifdef Q_OS_WIN
    return candidate.startsWith( rootPrefix, Qt::CaseInsensitive );
#else
    return candidate.startsWith( rootPrefix, Qt::CaseSensitive );
#endif
  }

  mtpl_status_t tileReaderOpen( QgsMtpl::PackageFormat format, const QByteArray &path, const mtpl_crypto_options_t *crypto, void **reader )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_open( path.constData(), crypto, reinterpret_cast<mtpl_ptp_reader_t **>( reader ) );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_open( path.constData(), crypto, reinterpret_cast<mtpl_dtp_reader_t **>( reader ) );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_open( path.constData(), crypto, reinterpret_cast<mtpl_vtp_reader_t **>( reader ) );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileReaderClose( QgsMtpl::PackageFormat format, void *reader )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_close( static_cast<mtpl_ptp_reader_t *>( reader ) );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_close( static_cast<mtpl_dtp_reader_t *>( reader ) );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_close( static_cast<mtpl_vtp_reader_t *>( reader ) );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileReaderMetadata( QgsMtpl::PackageFormat format, void *reader, mtpl_buffer_t *buffer )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_read_metadata( static_cast<mtpl_ptp_reader_t *>( reader ), buffer );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_read_metadata( static_cast<mtpl_dtp_reader_t *>( reader ), buffer );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_read_metadata( static_cast<mtpl_vtp_reader_t *>( reader ), buffer );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileReaderSize( QgsMtpl::PackageFormat format, void *reader, uint32_t *size )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_get_tile_size( static_cast<mtpl_ptp_reader_t *>( reader ), size );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_get_tile_size( static_cast<mtpl_dtp_reader_t *>( reader ), size );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_get_tile_size( static_cast<mtpl_vtp_reader_t *>( reader ), size );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileReaderMode( QgsMtpl::PackageFormat format, void *reader, mtpl_storage_mode_t *mode )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_get_storage_mode( static_cast<mtpl_ptp_reader_t *>( reader ), mode );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_get_storage_mode( static_cast<mtpl_dtp_reader_t *>( reader ), mode );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_get_storage_mode( static_cast<mtpl_vtp_reader_t *>( reader ), mode );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileReaderRead( QgsMtpl::PackageFormat format, void *reader, const mtpl_tile_coordinate_t *coordinate, mtpl_buffer_t *buffer )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_read_tile( static_cast<mtpl_ptp_reader_t *>( reader ), coordinate, buffer );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_read_tile( static_cast<mtpl_dtp_reader_t *>( reader ), coordinate, buffer );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_read_tile( static_cast<mtpl_vtp_reader_t *>( reader ), coordinate, buffer );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileRangeCount( QgsMtpl::PackageFormat format, void *reader, size_t *count )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_get_range_count( static_cast<mtpl_ptp_reader_t *>( reader ), count );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_get_range_count( static_cast<mtpl_dtp_reader_t *>( reader ), count );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_get_range_count( static_cast<mtpl_vtp_reader_t *>( reader ), count );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileRangeInfo( QgsMtpl::PackageFormat format, void *reader, size_t index, mtpl_tile_range_info_t *info )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_get_range_info( static_cast<mtpl_ptp_reader_t *>( reader ), index, info );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_get_range_info( static_cast<mtpl_dtp_reader_t *>( reader ), index, info );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_get_range_info( static_cast<mtpl_vtp_reader_t *>( reader ), index, info );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileEntryInfo( QgsMtpl::PackageFormat format, void *reader, size_t range, size_t slot, mtpl_tile_entry_info_t *entry )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_reader_get_entry_info( static_cast<mtpl_ptp_reader_t *>( reader ), range, slot, entry );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_reader_get_entry_info( static_cast<mtpl_dtp_reader_t *>( reader ), range, slot, entry );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_reader_get_entry_info( static_cast<mtpl_vtp_reader_t *>( reader ), range, slot, entry );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileWriterCreate( QgsMtpl::PackageFormat format,
                                  const QByteArray &path,
                                  uint32_t tileSize,
                                  mtpl_buffer_view_t metadata,
                                  mtpl_storage_mode_t mode,
                                  const mtpl_crypto_options_t *crypto,
                                  void **writer )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_writer_create( path.constData(), tileSize, metadata, mode, crypto, reinterpret_cast<mtpl_ptp_writer_t **>( writer ) );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_writer_create( path.constData(), tileSize, metadata, mode, crypto, reinterpret_cast<mtpl_dtp_writer_t **>( writer ) );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_writer_create( path.constData(), tileSize, metadata, mode, crypto, reinterpret_cast<mtpl_vtp_writer_t **>( writer ) );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileWriterAddRange( QgsMtpl::PackageFormat format, void *writer, const mtpl_tile_range_t *range )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_writer_add_range( static_cast<mtpl_ptp_writer_t *>( writer ), range );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_writer_add_range( static_cast<mtpl_dtp_writer_t *>( writer ), range );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_writer_add_range( static_cast<mtpl_vtp_writer_t *>( writer ), range );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileWriterAddData( QgsMtpl::PackageFormat format, void *writer, const mtpl_tile_coordinate_t *coordinate, mtpl_buffer_view_t data )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_writer_add_tile_data( static_cast<mtpl_ptp_writer_t *>( writer ), coordinate, data );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_writer_add_tile_data( static_cast<mtpl_dtp_writer_t *>( writer ), coordinate, data );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_writer_add_tile_data( static_cast<mtpl_vtp_writer_t *>( writer ), coordinate, data );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileWriterAddFile( QgsMtpl::PackageFormat format, void *writer, const mtpl_tile_coordinate_t *coordinate, const QByteArray &path )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_writer_add_tile_file( static_cast<mtpl_ptp_writer_t *>( writer ), coordinate, path.constData() );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_writer_add_tile_file( static_cast<mtpl_dtp_writer_t *>( writer ), coordinate, path.constData() );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_writer_add_tile_file( static_cast<mtpl_vtp_writer_t *>( writer ), coordinate, path.constData() );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  mtpl_status_t tileWriterClose( QgsMtpl::PackageFormat format, void *writer )
  {
    switch ( format )
    {
      case QgsMtpl::PackageFormat::Ptp:
        return mtpl_ptp_writer_close( static_cast<mtpl_ptp_writer_t *>( writer ) );
      case QgsMtpl::PackageFormat::Dtp:
        return mtpl_dtp_writer_close( static_cast<mtpl_dtp_writer_t *>( writer ) );
      case QgsMtpl::PackageFormat::Vtp:
        return mtpl_vtp_writer_close( static_cast<mtpl_vtp_writer_t *>( writer ) );
      default:
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
  }

  bool sameCoordinate( const mtpl_tile_coordinate_t &left, const mtpl_tile_coordinate_t &right )
  {
    return left.zoom == right.zoom && left.x == right.x && left.y == right.y;
  }

  bool sameRange( const mtpl_tile_range_info_t &left, const mtpl_tile_range_info_t &right )
  {
    return left.bounds.zoom == right.bounds.zoom && left.bounds.x_min == right.bounds.x_min &&
           left.bounds.x_max == right.bounds.x_max && left.bounds.y_min == right.bounds.y_min &&
           left.bounds.y_max == right.bounds.y_max && left.slot_count == right.slot_count &&
           left.present_count == right.present_count;
  }

  bool buffersEqual( const mtpl_buffer_t &left, const mtpl_buffer_t &right )
  {
    return left.size == right.size && ( left.size == 0 || memcmp( left.data, right.data, left.size ) == 0 );
  }

  bool verifyTileConversion( const QString &sourcePath,
                             const QString &destinationPath,
                             QgsMtpl::PackageFormat format,
                             const QgsMtpl::CryptoKeys &sourceKeys,
                             const QgsMtpl::CryptoKeys &destinationKeys,
                             mtpl_storage_mode_t expectedMode,
                             const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                             const QgsMtpl::PackageOperations::ProgressCallback &progress,
                             double progressStart,
                             double progressEnd,
                             QString &error )
  {
    if ( canceled( cancelCheck ) )
    {
      error = QStringLiteral( "数据包操作已取消。" );
      return false;
    }
    reportProgress( progress, progressStart, progressEnd, 0, 1 );

    const CryptoContext sourceCrypto( sourceKeys );
    const CryptoContext destinationCrypto( destinationKeys );
    const QByteArray sourceUtf8 = sourcePath.toUtf8();
    const QByteArray destinationUtf8 = destinationPath.toUtf8();
    void *source = nullptr;
    void *destination = nullptr;
    mtpl_status_t status = tileReaderOpen( format, sourceUtf8, sourceCrypto.get(), &source );
    if ( status == MTPL_STATUS_OK )
      status = tileReaderOpen( format, destinationUtf8, destinationCrypto.get(), &destination );
    if ( status != MTPL_STATUS_OK )
    {
      if ( source )
        tileReaderClose( format, source );
      error = statusError( QStringLiteral( "无法重新打开数据包进行验证" ), status );
      return false;
    }

    bool ok = true;
    uint32_t sourceSize = 0;
    uint32_t destinationSize = 0;
    mtpl_storage_mode_t destinationMode = MTPL_STORAGE_PLAIN;
    mtpl_buffer_t sourceMetadata = {};
    mtpl_buffer_t destinationMetadata = {};
    size_t sourceRangeCount = 0;
    size_t destinationRangeCount = 0;
    uint64_t totalSlotCount = 0;
    status = tileReaderSize( format, source, &sourceSize );
    if ( status == MTPL_STATUS_OK )
      status = tileReaderSize( format, destination, &destinationSize );
    if ( status == MTPL_STATUS_OK )
      status = tileReaderMode( format, destination, &destinationMode );
    if ( status == MTPL_STATUS_OK )
      status = tileReaderMetadata( format, source, &sourceMetadata );
    if ( status == MTPL_STATUS_OK )
      status = tileReaderMetadata( format, destination, &destinationMetadata );
    if ( status == MTPL_STATUS_OK )
      status = tileRangeCount( format, source, &sourceRangeCount );
    if ( status == MTPL_STATUS_OK )
      status = tileRangeCount( format, destination, &destinationRangeCount );

    if ( status != MTPL_STATUS_OK || sourceSize != destinationSize || destinationMode != expectedMode ||
         !buffersEqual( sourceMetadata, destinationMetadata ) || sourceRangeCount != destinationRangeCount )
      ok = false;

    for ( size_t rangeIndex = 0; ok && rangeIndex < sourceRangeCount; ++rangeIndex )
    {
      if ( canceled( cancelCheck ) )
      {
        ok = false;
        status = MTPL_STATUS_CANCELED;
        error = QStringLiteral( "数据包操作已取消。" );
        break;
      }
      mtpl_tile_range_info_t sourceRange = {};
      mtpl_tile_range_info_t destinationRange = {};
      status = tileRangeInfo( format, source, rangeIndex, &sourceRange );
      if ( status == MTPL_STATUS_OK )
        status = tileRangeInfo( format, destination, rangeIndex, &destinationRange );
      if ( status != MTPL_STATUS_OK || !sameRange( sourceRange, destinationRange ) )
      {
        ok = false;
        break;
      }

      if ( static_cast<uint64_t>( sourceRange.slot_count ) > std::numeric_limits<uint64_t>::max() - totalSlotCount )
      {
        ok = false;
        status = MTPL_STATUS_LIMIT_EXCEEDED;
        break;
      }
      totalSlotCount += static_cast<uint64_t>( sourceRange.slot_count );
    }

    uint64_t completedSlots = 0;
    for ( size_t rangeIndex = 0; ok && rangeIndex < sourceRangeCount; ++rangeIndex )
    {
      mtpl_tile_range_info_t sourceRange = {};
      status = tileRangeInfo( format, source, rangeIndex, &sourceRange );
      if ( status != MTPL_STATUS_OK )
      {
        ok = false;
        break;
      }
      for ( size_t slot = 0; ok && slot < sourceRange.slot_count; ++slot )
      {
        if ( canceled( cancelCheck ) )
        {
          ok = false;
          status = MTPL_STATUS_CANCELED;
          error = QStringLiteral( "数据包操作已取消。" );
          break;
        }
        mtpl_tile_entry_info_t sourceEntry = {};
        mtpl_tile_entry_info_t destinationEntry = {};
        status = tileEntryInfo( format, source, rangeIndex, slot, &sourceEntry );
        if ( status == MTPL_STATUS_OK )
          status = tileEntryInfo( format, destination, rangeIndex, slot, &destinationEntry );
        if ( status != MTPL_STATUS_OK || sourceEntry.present != destinationEntry.present ||
             !sameCoordinate( sourceEntry.coordinate, destinationEntry.coordinate ) )
        {
          ok = false;
          break;
        }
        if ( sourceEntry.present )
        {
          mtpl_buffer_t sourceData = {};
          mtpl_buffer_t destinationData = {};
          status = tileReaderRead( format, source, &sourceEntry.coordinate, &sourceData );
          if ( status == MTPL_STATUS_OK )
            status = tileReaderRead( format, destination, &destinationEntry.coordinate, &destinationData );
          if ( status != MTPL_STATUS_OK || !buffersEqual( sourceData, destinationData ) )
            ok = false;
          mtpl_buffer_release( &sourceData );
          mtpl_buffer_release( &destinationData );
        }
        if ( ok )
        {
          ++completedSlots;
          reportProgress( progress, progressStart, progressEnd, completedSlots, totalSlotCount );
        }
      }
    }

    mtpl_buffer_release( &sourceMetadata );
    mtpl_buffer_release( &destinationMetadata );
    const mtpl_status_t sourceClose = tileReaderClose( format, source );
    const mtpl_status_t destinationClose = tileReaderClose( format, destination );
    if ( status == MTPL_STATUS_OK )
      status = sourceClose;
    if ( status == MTPL_STATUS_OK )
      status = destinationClose;
    if ( !ok || status != MTPL_STATUS_OK )
    {
      if ( error.isEmpty() )
        error = status == MTPL_STATUS_OK
          ? QStringLiteral( "转换后的数据包未通过内容验证。" )
          : statusError( QStringLiteral( "转换后的数据包未通过内容验证" ), status );
      return false;
    }
    reportProgress( progress, progressStart, progressEnd, 1, 1 );
    return true;
  }

  bool convertTilePackage( const QString &sourcePath,
                           const QString &destinationPath,
                           QgsMtpl::PackageFormat format,
                           const QgsMtpl::CryptoKeys &sourceKeys,
                           const QgsMtpl::CryptoKeys &destinationKeys,
                           mtpl_storage_mode_t destinationMode,
                           const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                           QString &error )
  {
    const CryptoContext sourceCrypto( sourceKeys );
    const CryptoContext destinationCrypto( destinationKeys );
    const QByteArray sourceUtf8 = sourcePath.toUtf8();
    const QByteArray destinationUtf8 = destinationPath.toUtf8();
    void *reader = nullptr;
    void *writer = nullptr;
    mtpl_buffer_t metadata = {};
    mtpl_status_t status = tileReaderOpen( format, sourceUtf8, sourceCrypto.get(), &reader );
    uint32_t tileSize = 0;
    size_t rangeCount = 0;
    if ( status == MTPL_STATUS_OK )
      status = tileReaderSize( format, reader, &tileSize );
    if ( status == MTPL_STATUS_OK )
      status = tileReaderMetadata( format, reader, &metadata );
    if ( status == MTPL_STATUS_OK )
      status = tileRangeCount( format, reader, &rangeCount );
    if ( status == MTPL_STATUS_OK )
    {
      const mtpl_buffer_view_t metadataView = { metadata.data, metadata.size };
      status = tileWriterCreate( format, destinationUtf8, tileSize, metadataView, destinationMode, destinationCrypto.get(), &writer );
    }

    for ( size_t rangeIndex = 0; status == MTPL_STATUS_OK && rangeIndex < rangeCount; ++rangeIndex )
    {
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        status = MTPL_STATUS_CANCELED;
        break;
      }
      mtpl_tile_range_info_t range = {};
      status = tileRangeInfo( format, reader, rangeIndex, &range );
      if ( status == MTPL_STATUS_OK )
        status = tileWriterAddRange( format, writer, &range.bounds );
    }

    for ( size_t rangeIndex = 0; status == MTPL_STATUS_OK && rangeIndex < rangeCount; ++rangeIndex )
    {
      mtpl_tile_range_info_t range = {};
      status = tileRangeInfo( format, reader, rangeIndex, &range );
      for ( size_t slot = 0; status == MTPL_STATUS_OK && slot < range.slot_count; ++slot )
      {
        if ( canceled( cancelCheck ) )
        {
          error = QStringLiteral( "数据包操作已取消。" );
          status = MTPL_STATUS_CANCELED;
          break;
        }
        mtpl_tile_entry_info_t entry = {};
        status = tileEntryInfo( format, reader, rangeIndex, slot, &entry );
        if ( status == MTPL_STATUS_OK && entry.present )
        {
          mtpl_buffer_t data = {};
          status = tileReaderRead( format, reader, &entry.coordinate, &data );
          if ( status == MTPL_STATUS_OK )
          {
            const mtpl_buffer_view_t view = { data.data, data.size };
            status = tileWriterAddData( format, writer, &entry.coordinate, view );
          }
          mtpl_buffer_release( &data );
        }
      }
    }

    if ( writer )
    {
      const mtpl_status_t closeStatus = tileWriterClose( format, writer );
      if ( status == MTPL_STATUS_OK )
        status = closeStatus;
    }
    if ( reader )
    {
      const mtpl_status_t closeStatus = tileReaderClose( format, reader );
      if ( status == MTPL_STATUS_OK )
        status = closeStatus;
    }
    mtpl_buffer_release( &metadata );

    if ( status != MTPL_STATUS_OK )
    {
      if ( error.isEmpty() )
        error = statusError( QStringLiteral( "无法转换瓦片数据包" ), status );
      return false;
    }
    const QgsMtpl::PackageOperations::ProgressCallback noProgress;
    return verifyTileConversion( sourcePath, destinationPath, format, sourceKeys, destinationKeys,
                                 destinationMode, cancelCheck, noProgress, 0.0, 100.0, error );
  }

  constexpr size_t SFP_VERIFICATION_CHUNK_SIZE = 1024 * 1024;

  struct SfpDigestContext
  {
    QCryptographicHash *hash = nullptr;
    const QgsMtpl::PackageOperations::CancelCheck *cancelCheck = nullptr;
    const QgsMtpl::PackageOperations::ProgressCallback *progress = nullptr;
    uint64_t expectedSize = 0;
    uint64_t streamedSize = 0;
    uint64_t *completedSize = nullptr;
    uint64_t totalSize = 0;
    double progressStart = 0.0;
    double progressEnd = 100.0;
  };

  mtpl_status_t hashSfpChunk( mtpl_buffer_view_t data, void *userData )
  {
    auto *context = static_cast<SfpDigestContext *>( userData );
    if ( !context || !context->hash || !context->completedSize ||
         ( data.size > 0 && !data.data ) )
      return MTPL_STATUS_INVALID_ARGUMENT;
    if ( context->cancelCheck && canceled( *context->cancelCheck ) )
      return MTPL_STATUS_CANCELED;
    if ( data.size > SFP_VERIFICATION_CHUNK_SIZE )
      return MTPL_STATUS_LIMIT_EXCEEDED;

    const uint64_t chunkSize = static_cast<uint64_t>( data.size );
    if ( context->streamedSize > context->expectedSize ||
         chunkSize > context->expectedSize - context->streamedSize ||
         chunkSize > std::numeric_limits<uint64_t>::max() - *context->completedSize )
      return MTPL_STATUS_CORRUPT_DATA;

    if ( data.size > 0 )
    {
      context->hash->addData( QByteArray::fromRawData(
        reinterpret_cast<const char *>( data.data ), static_cast<int>( data.size ) ) );
    }
    context->streamedSize += chunkSize;
    *context->completedSize += chunkSize;
    if ( context->progress )
    {
      reportProgress( *context->progress, context->progressStart, context->progressEnd,
                      *context->completedSize, context->totalSize );
    }
    return MTPL_STATUS_OK;
  }

  bool streamSfpDigest( mtpl_sfp_reader_t *reader,
                        const char *packagePath,
                        uint64_t expectedSize,
                        const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                        const QgsMtpl::PackageOperations::ProgressCallback &progress,
                        uint64_t &completedSize,
                        uint64_t totalSize,
                        double progressStart,
                        double progressEnd,
                        QByteArray &digest,
                        QString &error )
  {
    if ( canceled( cancelCheck ) )
    {
      error = QStringLiteral( "数据包操作已取消。" );
      return false;
    }

    QCryptographicHash hash( QCryptographicHash::Sha256 );
    SfpDigestContext context;
    context.hash = &hash;
    context.cancelCheck = &cancelCheck;
    context.progress = &progress;
    context.expectedSize = expectedSize;
    context.completedSize = &completedSize;
    context.totalSize = totalSize;
    context.progressStart = progressStart;
    context.progressEnd = progressEnd;
    const mtpl_status_t status = mtpl_sfp_reader_read_file_chunks_internal(
      reader, packagePath, SFP_VERIFICATION_CHUNK_SIZE, hashSfpChunk, &context );
    if ( status != MTPL_STATUS_OK )
    {
      error = status == MTPL_STATUS_CANCELED
        ? QStringLiteral( "数据包操作已取消。" )
        : statusError( QStringLiteral( "无法流式验证 SFP 数据包内容" ), status );
      return false;
    }
    if ( context.streamedSize != expectedSize )
    {
      error = QStringLiteral( "转换后的 SFP 数据包条目长度与声明值不一致。" );
      return false;
    }
    digest = hash.result();
    return true;
  }

  bool streamFileDigest( const QString &path,
                         uint64_t expectedSize,
                         const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                         const QgsMtpl::PackageOperations::ProgressCallback &progress,
                         uint64_t &completedSize,
                         uint64_t totalSize,
                         double progressStart,
                         double progressEnd,
                         QByteArray &digest,
                         QString &error )
  {
    QFile file( path );
    if ( !file.open( QIODevice::ReadOnly ) )
    {
      error = QStringLiteral( "无法打开 SFP 源文件进行验证。" );
      return false;
    }

    QCryptographicHash hash( QCryptographicHash::Sha256 );
    QByteArray buffer( static_cast<int>( SFP_VERIFICATION_CHUNK_SIZE ), '\0' );
    uint64_t streamedSize = 0;
    while ( true )
    {
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        return false;
      }

      const qint64 bytesRead = file.read( buffer.data(), buffer.size() );
      if ( bytesRead < 0 )
      {
        error = QStringLiteral( "读取 SFP 源文件进行验证时失败。" );
        return false;
      }
      if ( bytesRead == 0 )
      {
        if ( !file.atEnd() )
        {
          error = QStringLiteral( "读取 SFP 源文件进行验证时未到达文件末尾。" );
          return false;
        }
        break;
      }

      const uint64_t chunkSize = static_cast<uint64_t>( bytesRead );
      if ( streamedSize > expectedSize || chunkSize > expectedSize - streamedSize ||
           chunkSize > std::numeric_limits<uint64_t>::max() - completedSize )
      {
        error = QStringLiteral( "SFP 源文件长度在创建期间发生变化。" );
        return false;
      }
      hash.addData( buffer.constData(), bytesRead );
      streamedSize += chunkSize;
      completedSize += chunkSize;
      reportProgress( progress, progressStart, progressEnd, completedSize, totalSize );
    }

    if ( streamedSize != expectedSize )
    {
      error = QStringLiteral( "SFP 源文件长度在创建期间发生变化。" );
      return false;
    }
    digest = hash.result();
    return true;
  }

  bool verifySfpConversion( const QString &sourcePath,
                            const QString &destinationPath,
                            const QgsMtpl::CryptoKeys &sourceKeys,
                            const QgsMtpl::CryptoKeys &destinationKeys,
                            mtpl_package_storage_t expectedStorage,
                            const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                            const QgsMtpl::PackageOperations::ProgressCallback &progress,
                            double progressStart,
                            double progressEnd,
                            QString &error )
  {
    if ( canceled( cancelCheck ) )
    {
      error = QStringLiteral( "数据包操作已取消。" );
      return false;
    }
    reportProgress( progress, progressStart, progressEnd, 0, 1 );

    const CryptoContext sourceCrypto( sourceKeys );
    const CryptoContext destinationCrypto( destinationKeys );
    const QByteArray sourceUtf8 = sourcePath.toUtf8();
    const QByteArray destinationUtf8 = destinationPath.toUtf8();
    mtpl_sfp_reader_t *source = nullptr;
    mtpl_sfp_reader_t *destination = nullptr;
    mtpl_status_t status = mtpl_sfp_reader_open( sourceUtf8.constData(), sourceCrypto.get(), &source );
    if ( status == MTPL_STATUS_OK )
      status = mtpl_sfp_reader_open( destinationUtf8.constData(), destinationCrypto.get(), &destination );
    size_t sourceCount = 0;
    size_t destinationCount = 0;
    if ( status == MTPL_STATUS_OK )
      status = mtpl_sfp_reader_get_entry_count( source, &sourceCount );
    if ( status == MTPL_STATUS_OK )
      status = mtpl_sfp_reader_get_entry_count( destination, &destinationCount );

    bool ok = status == MTPL_STATUS_OK && sourceCount == destinationCount &&
              ( expectedStorage == MTPL_PACKAGE_STORAGE_PLAIN ||
                expectedStorage == MTPL_PACKAGE_STORAGE_ENCRYPTED ||
                expectedStorage == MTPL_PACKAGE_STORAGE_MIXED );
    if ( ok && sourceCount == 0 && expectedStorage != MTPL_PACKAGE_STORAGE_PLAIN )
    {
      error = QStringLiteral( "空 SFP 数据包不包含可加密条目，无法生成加密输出。" );
      ok = false;
    }
    uint64_t totalLogicalSize = 0;
    for ( size_t index = 0; ok && index < sourceCount; ++index )
    {
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        status = MTPL_STATUS_CANCELED;
        ok = false;
        break;
      }
      mtpl_sfp_entry_info_t sourceEntry = {};
      mtpl_sfp_entry_info_t destinationEntry = {};
      status = mtpl_sfp_reader_get_entry_info( source, index, &sourceEntry );
      if ( status == MTPL_STATUS_OK )
        status = mtpl_sfp_reader_get_entry_info( destination, index, &destinationEntry );

      const bool sourceModeValid = sourceEntry.storage_mode == MTPL_STORAGE_PLAIN ||
                                   sourceEntry.storage_mode == MTPL_STORAGE_ENCRYPTED;
      const mtpl_storage_mode_t forcedMode = expectedStorage == MTPL_PACKAGE_STORAGE_ENCRYPTED
        ? MTPL_STORAGE_ENCRYPTED
        : MTPL_STORAGE_PLAIN;
      const bool modeMatches = expectedStorage == MTPL_PACKAGE_STORAGE_MIXED
        ? destinationEntry.storage_mode == sourceEntry.storage_mode
        : destinationEntry.storage_mode == forcedMode;
      if ( status != MTPL_STATUS_OK || !sourceEntry.path || !destinationEntry.path || !sourceModeValid ||
           QByteArray( sourceEntry.path ) != QByteArray( destinationEntry.path ) ||
           sourceEntry.logical_size != destinationEntry.logical_size || !modeMatches )
      {
        ok = false;
        break;
      }
      if ( sourceEntry.logical_size > std::numeric_limits<uint64_t>::max() - totalLogicalSize )
      {
        status = MTPL_STATUS_LIMIT_EXCEEDED;
        ok = false;
        break;
      }
      totalLogicalSize += sourceEntry.logical_size;
      if ( destinationEntry.logical_size > std::numeric_limits<uint64_t>::max() - totalLogicalSize )
      {
        status = MTPL_STATUS_LIMIT_EXCEEDED;
        ok = false;
        break;
      }
      totalLogicalSize += destinationEntry.logical_size;
    }

    uint64_t completedLogicalSize = 0;
    for ( size_t index = 0; ok && index < sourceCount; ++index )
    {
      mtpl_sfp_entry_info_t sourceEntry = {};
      mtpl_sfp_entry_info_t destinationEntry = {};
      status = mtpl_sfp_reader_get_entry_info( source, index, &sourceEntry );
      if ( status == MTPL_STATUS_OK )
        status = mtpl_sfp_reader_get_entry_info( destination, index, &destinationEntry );
      if ( status != MTPL_STATUS_OK )
      {
        ok = false;
        break;
      }

      QByteArray sourceDigest;
      QByteArray destinationDigest;
      if ( !streamSfpDigest( source, sourceEntry.path, sourceEntry.logical_size,
                             cancelCheck, progress, completedLogicalSize, totalLogicalSize,
                             progressStart, progressEnd, sourceDigest, error ) ||
           !streamSfpDigest( destination, destinationEntry.path, destinationEntry.logical_size,
                             cancelCheck, progress, completedLogicalSize, totalLogicalSize,
                             progressStart, progressEnd, destinationDigest, error ) ||
           sourceDigest != destinationDigest )
      {
        ok = false;
        break;
      }
    }

    if ( source )
    {
      const mtpl_status_t closeStatus = mtpl_sfp_reader_close( source );
      if ( status == MTPL_STATUS_OK )
        status = closeStatus;
    }
    if ( destination )
    {
      const mtpl_status_t closeStatus = mtpl_sfp_reader_close( destination );
      if ( status == MTPL_STATUS_OK )
        status = closeStatus;
    }
    if ( !ok || status != MTPL_STATUS_OK )
    {
      if ( error.isEmpty() )
        error = status == MTPL_STATUS_OK
          ? QStringLiteral( "转换后的 SFP 数据包未通过内容验证。" )
          : statusError( QStringLiteral( "转换后的 SFP 数据包未通过内容验证" ), status );
      return false;
    }
    reportProgress( progress, progressStart, progressEnd, 1, 1 );
    return true;
  }

  bool verifyPackageConversion( const QString &sourcePath,
                                const QString &destinationPath,
                                const QgsMtpl::CryptoKeys &sourceKeys,
                                const QgsMtpl::CryptoKeys &destinationKeys,
                                mtpl_package_storage_t expectedStorage,
                                const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                                const QgsMtpl::PackageOperations::ProgressCallback &progress,
                                double progressStart,
                                double progressEnd,
                                QString &error )
  {
    if ( canceled( cancelCheck ) )
    {
      error = QStringLiteral( "数据包操作已取消。" );
      return false;
    }

    mtpl_package_info_t sourceInfo = MTPL_PACKAGE_INFO_INIT;
    mtpl_package_info_t destinationInfo = MTPL_PACKAGE_INFO_INIT;
    const QByteArray sourceUtf8 = sourcePath.toUtf8();
    const QByteArray destinationUtf8 = destinationPath.toUtf8();
    mtpl_status_t status = mtpl_package_probe( sourceUtf8.constData(), &sourceInfo );
    if ( status == MTPL_STATUS_OK )
      status = mtpl_package_probe( destinationUtf8.constData(), &destinationInfo );
    if ( status != MTPL_STATUS_OK )
    {
      error = statusError( QStringLiteral( "无法检查转换后的数据包" ), status );
      return false;
    }
    if ( sourceInfo.format == MTPL_PACKAGE_FORMAT_UNKNOWN || sourceInfo.format != destinationInfo.format )
    {
      error = QStringLiteral( "转换后的数据包格式与源数据包不一致。" );
      return false;
    }

    const QgsMtpl::PackageFormat format = packageFormatFromMtpl( sourceInfo.format );
    if ( format == QgsMtpl::PackageFormat::Sfp )
    {
      return verifySfpConversion( sourcePath, destinationPath, sourceKeys, destinationKeys,
                                  expectedStorage, cancelCheck, progress,
                                  progressStart, progressEnd, error );
    }
    if ( format == QgsMtpl::PackageFormat::Ptp ||
         format == QgsMtpl::PackageFormat::Dtp ||
         format == QgsMtpl::PackageFormat::Vtp )
    {
      if ( expectedStorage == MTPL_PACKAGE_STORAGE_MIXED )
      {
        error = QStringLiteral( "瓦片数据包不支持混合存储方式。" );
        return false;
      }
      const mtpl_storage_mode_t expectedMode = expectedStorage == MTPL_PACKAGE_STORAGE_ENCRYPTED
        ? MTPL_STORAGE_ENCRYPTED
        : MTPL_STORAGE_PLAIN;
      return verifyTileConversion( sourcePath, destinationPath, format, sourceKeys, destinationKeys,
                                   expectedMode, cancelCheck, progress,
                                   progressStart, progressEnd, error );
    }

    error = QStringLiteral( "无法验证不受支持的数据包格式。" );
    return false;
  }

  bool convertSfpPackage( const QString &sourcePath,
                          const QString &destinationPath,
                          const QgsMtpl::CryptoKeys &sourceKeys,
                          const QgsMtpl::CryptoKeys &destinationKeys,
                          mtpl_storage_mode_t destinationMode,
                          const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                          QString &error )
  {
    const CryptoContext sourceCrypto( sourceKeys );
    const CryptoContext destinationCrypto( destinationKeys );
    const QByteArray sourceUtf8 = sourcePath.toUtf8();
    const QByteArray destinationUtf8 = destinationPath.toUtf8();
    mtpl_sfp_reader_t *reader = nullptr;
    mtpl_sfp_builder_t *builder = nullptr;
    mtpl_status_t status = mtpl_sfp_reader_open( sourceUtf8.constData(), sourceCrypto.get(), &reader );
    if ( status == MTPL_STATUS_OK )
      status = mtpl_sfp_builder_create( destinationCrypto.get(), &builder );
    size_t entryCount = 0;
    if ( status == MTPL_STATUS_OK )
      status = mtpl_sfp_reader_get_entry_count( reader, &entryCount );

    for ( size_t index = 0; status == MTPL_STATUS_OK && index < entryCount; ++index )
    {
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        status = MTPL_STATUS_CANCELED;
        break;
      }
      mtpl_sfp_entry_info_t entry = {};
      status = mtpl_sfp_reader_get_entry_info( reader, index, &entry );
      mtpl_buffer_t data = {};
      if ( status == MTPL_STATUS_OK )
        status = mtpl_sfp_reader_read_file( reader, entry.path, &data );
      if ( status == MTPL_STATUS_OK )
      {
        const mtpl_buffer_view_t view = { data.data, data.size };
        status = mtpl_sfp_builder_add_data( builder, entry.path, view, destinationMode );
      }
      mtpl_buffer_release( &data );
    }
    if ( status == MTPL_STATUS_OK )
      status = mtpl_sfp_builder_write( builder, destinationUtf8.constData() );
    if ( builder )
      mtpl_sfp_builder_destroy( builder );
    if ( reader )
    {
      const mtpl_status_t closeStatus = mtpl_sfp_reader_close( reader );
      if ( status == MTPL_STATUS_OK )
        status = closeStatus;
    }
    if ( status != MTPL_STATUS_OK )
    {
      if ( error.isEmpty() )
        error = statusError( QStringLiteral( "无法转换 SFP 数据包" ), status );
      return false;
    }
    const QgsMtpl::PackageOperations::ProgressCallback noProgress;
    const mtpl_package_storage_t expectedStorage = destinationMode == MTPL_STORAGE_ENCRYPTED
      ? MTPL_PACKAGE_STORAGE_ENCRYPTED
      : MTPL_PACKAGE_STORAGE_PLAIN;
    return verifySfpConversion( sourcePath, destinationPath, sourceKeys, destinationKeys,
                                expectedStorage, cancelCheck, noProgress, 0.0, 100.0, error );
  }

  bool scanTileTree( const QString &rootPath, QList<TileSource> &tiles, QString &error,
                     const QgsMtpl::PackageOperations::CancelCheck &cancelCheck )
  {
    const QFileInfo rootInfo( rootPath );
    if ( !rootInfo.isDir() || isReparseOrLink( rootInfo ) )
    {
      error = QStringLiteral( "瓦片源必须是常规文件夹，不能是链接或重解析点。" );
      return false;
    }
    const QString rootCanonical = rootInfo.canonicalFilePath();
    if ( rootCanonical.isEmpty() )
    {
      error = QStringLiteral( "无法解析瓦片源文件夹。" );
      return false;
    }

    QString commonSuffix;
    QSet<QString> coordinates;
    QDirIterator iterator( rootInfo.absoluteFilePath(),
                           QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                           QDirIterator::Subdirectories );
    while ( iterator.hasNext() )
    {
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        return false;
      }
      iterator.next();
      const QFileInfo info = iterator.fileInfo();
      if ( isReparseOrLink( info ) )
      {
        error = QStringLiteral( "瓦片目录树中包含链接或重解析点。" );
        return false;
      }
      if ( info.isDir() )
        continue;
      if ( !info.isFile() || !isWithinRoot( rootCanonical, info.canonicalFilePath() ) )
      {
        error = QStringLiteral( "瓦片目录树中包含不安全的文件系统条目。" );
        return false;
      }

      const QString relative = QDir( rootInfo.absoluteFilePath() ).relativeFilePath( info.absoluteFilePath() ).replace( '\\', '/' );
      const QStringList parts = relative.split( '/', Qt::KeepEmptyParts );
      if ( parts.size() != 3 )
      {
        error = QStringLiteral( "所有瓦片都必须使用 {z}/{x}/{y}.ext 目录结构。" );
        return false;
      }
      bool zoomOk = false;
      bool xOk = false;
      bool yOk = false;
      const qulonglong zoom = parts.at( 0 ).toULongLong( &zoomOk );
      const qulonglong x = parts.at( 1 ).toULongLong( &xOk );
      const QFileInfo tileName( parts.at( 2 ) );
      const qulonglong y = tileName.completeBaseName().toULongLong( &yOk );
      const QString suffix = tileName.suffix().toLower();
      if ( !zoomOk || !xOk || !yOk || zoom > std::numeric_limits<uint32_t>::max() ||
           x > std::numeric_limits<uint32_t>::max() || y > std::numeric_limits<uint32_t>::max() || suffix.isEmpty() )
      {
        error = QStringLiteral( "瓦片路径包含无效的数字坐标或扩展名。" );
        return false;
      }
      if ( commonSuffix.isEmpty() )
        commonSuffix = suffix;
      else if ( commonSuffix != suffix )
      {
        error = QStringLiteral( "瓦片目录树只能包含一种载荷文件扩展名。" );
        return false;
      }

      const QString coordinateKey = QStringLiteral( "%1/%2/%3" ).arg( zoom ).arg( x ).arg( y );
      if ( coordinates.contains( coordinateKey ) )
      {
        error = QStringLiteral( "瓦片目录树中包含重复坐标。" );
        return false;
      }
      coordinates.insert( coordinateKey );
      TileSource tile;
      tile.coordinate.zoom = static_cast<uint32_t>( zoom );
      tile.coordinate.x = static_cast<uint32_t>( x );
      tile.coordinate.y = static_cast<uint32_t>( y );
      tile.path = info.absoluteFilePath();
      tiles.append( tile );
    }

    if ( tiles.isEmpty() )
    {
      error = QStringLiteral( "瓦片源文件夹中没有任何瓦片。" );
      return false;
    }
    std::sort( tiles.begin(), tiles.end(), []( const TileSource &left, const TileSource &right )
    {
      if ( left.coordinate.zoom != right.coordinate.zoom )
        return left.coordinate.zoom < right.coordinate.zoom;
      if ( left.coordinate.x != right.coordinate.x )
        return left.coordinate.x < right.coordinate.x;
      if ( left.coordinate.y != right.coordinate.y )
        return left.coordinate.y < right.coordinate.y;
      return left.path < right.path;
    } );
    return true;
  }

  bool validatePtpTileMatrix( const QList<TileSource> &tiles,
                              int tileSize,
                              const QByteArray &metadata,
                              const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                              QString &error )
  {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson( metadata, &parseError );
    if ( parseError.error != QJsonParseError::NoError || !document.isObject() )
    {
      error = QStringLiteral( "创建 PTP 时，切片元数据必须是有效的 JSON 对象。" );
      return false;
    }
    // scanTileTree sorts by zoom. Check before narrowing the stored uint32 coordinate.
    if ( tiles.constLast().coordinate.zoom > static_cast<uint32_t>( std::numeric_limits<int>::max() ) )
    {
      error = QStringLiteral( "PTP 瓦片“%1”的缩放级别超出支持范围。" ).arg( tiles.constLast().path );
      return false;
    }
    QgsMtpl::TileMatrixDefinition matrix;
    if ( !QgsMtpl::TileMatrixDefinition::fromMetadata(
           document.object().toVariantMap(), tileSize,
           static_cast<int>( tiles.constFirst().coordinate.zoom ),
           static_cast<int>( tiles.constLast().coordinate.zoom ), matrix, &error ) )
    {
      error = QStringLiteral( "无法创建 PTP，瓦片矩阵无效：%1" ).arg( error );
      return false;
    }
    for ( const TileSource &tile : tiles )
    {
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        return false;
      }
      bool widthOk = false;
      bool heightOk = false;
      const quint64 width = matrix.matrixWidth( static_cast<int>( tile.coordinate.zoom ), &widthOk );
      const quint64 height = matrix.matrixHeight( static_cast<int>( tile.coordinate.zoom ), &heightOk );
      if ( !widthOk || !heightOk || tile.coordinate.x >= width || tile.coordinate.y >= height )
      {
        error = QStringLiteral( "PTP 瓦片“%1”的坐标超出第 %2 级瓦片矩阵。" )
                  .arg( tile.path ).arg( tile.coordinate.zoom );
        return false;
      }
    }
    return true;
  }

  bool verifyCreatedTilePackage( const QString &packagePath,
                                 QgsMtpl::PackageFormat format,
                                 int expectedTileSize,
                                 const QByteArray &expectedMetadata,
                                 const QList<TileSource> &tiles,
                                 const QgsMtpl::CryptoKeys &keys,
                                 mtpl_storage_mode_t expectedMode,
                                 const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                                 QString &error )
  {
    const CryptoContext crypto( keys );
    const QByteArray pathUtf8 = packagePath.toUtf8();
    void *reader = nullptr;
    mtpl_status_t status = tileReaderOpen( format, pathUtf8, crypto.get(), &reader );
    uint32_t actualTileSize = 0;
    mtpl_storage_mode_t actualMode = MTPL_STORAGE_PLAIN;
    mtpl_buffer_t metadata = {};
    size_t rangeCount = 0;
    if ( status == MTPL_STATUS_OK )
      status = tileReaderSize( format, reader, &actualTileSize );
    if ( status == MTPL_STATUS_OK )
      status = tileReaderMode( format, reader, &actualMode );
    if ( status == MTPL_STATUS_OK )
      status = tileReaderMetadata( format, reader, &metadata );
    if ( status == MTPL_STATUS_OK )
      status = tileRangeCount( format, reader, &rangeCount );
    bool ok = status == MTPL_STATUS_OK && actualTileSize == static_cast<uint32_t>( expectedTileSize ) &&
              actualMode == expectedMode && rangeCount == static_cast<size_t>( tiles.size() ) &&
              metadata.size == static_cast<size_t>( expectedMetadata.size() ) &&
              ( metadata.size == 0 || memcmp( metadata.data, expectedMetadata.constData(), metadata.size ) == 0 );

    for ( int index = 0; ok && index < tiles.size(); ++index )
    {
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        ok = false;
        break;
      }
      mtpl_tile_range_info_t range = {};
      mtpl_tile_entry_info_t entry = {};
      status = tileRangeInfo( format, reader, static_cast<size_t>( index ), &range );
      if ( status == MTPL_STATUS_OK )
        status = tileEntryInfo( format, reader, static_cast<size_t>( index ), 0, &entry );
      const TileSource &tile = tiles.at( index );
      if ( status != MTPL_STATUS_OK || range.slot_count != 1 || range.present_count != 1 || !entry.present ||
           !sameCoordinate( entry.coordinate, tile.coordinate ) || range.bounds.zoom != tile.coordinate.zoom ||
           range.bounds.x_min != tile.coordinate.x || range.bounds.x_max != tile.coordinate.x ||
           range.bounds.y_min != tile.coordinate.y || range.bounds.y_max != tile.coordinate.y )
      {
        ok = false;
        break;
      }

      QFile sourceFile( tile.path );
      mtpl_buffer_t packageData = {};
      if ( !sourceFile.open( QIODevice::ReadOnly ) )
      {
        ok = false;
        break;
      }
      const QByteArray sourceData = sourceFile.readAll();
      status = tileReaderRead( format, reader, &entry.coordinate, &packageData );
      if ( status != MTPL_STATUS_OK || packageData.size != static_cast<size_t>( sourceData.size() ) ||
           ( packageData.size != 0 && memcmp( packageData.data, sourceData.constData(), packageData.size ) != 0 ) )
        ok = false;
      mtpl_buffer_release( &packageData );
      if ( ok && format == QgsMtpl::PackageFormat::Ptp )
      {
        if ( canceled( cancelCheck ) )
        {
          error = QStringLiteral( "数据包操作已取消。" );
          ok = false;
        }
        else if ( !QgsMtplPackageService::validatePtpImagePayload( sourceData, expectedTileSize, error ) )
        {
          error = QStringLiteral( "PTP 瓦片“%1”无法用于影像数据集：%2" ).arg( tile.path, error );
          ok = false;
        }
      }
    }

    mtpl_buffer_release( &metadata );
    if ( reader )
    {
      const mtpl_status_t closeStatus = tileReaderClose( format, reader );
      if ( status == MTPL_STATUS_OK )
        status = closeStatus;
    }
    if ( !ok || status != MTPL_STATUS_OK )
    {
      if ( error.isEmpty() )
        error = status == MTPL_STATUS_OK
          ? QStringLiteral( "创建的瓦片数据包未通过内容验证。" )
          : statusError( QStringLiteral( "创建的瓦片数据包未通过内容验证" ), status );
      return false;
    }
    return true;
  }

  bool createTilePackage( const QString &sourcePath,
                          const QString &destinationPath,
                          QgsMtpl::PackageFormat format,
                          int tileSize,
                          const QByteArray &metadata,
                          const QgsMtpl::CryptoKeys &keys,
                          mtpl_storage_mode_t mode,
                          const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                          QString &error )
  {
    if ( format != QgsMtpl::PackageFormat::Ptp && format != QgsMtpl::PackageFormat::Dtp && format != QgsMtpl::PackageFormat::Vtp )
    {
      error = QStringLiteral( "创建瓦片数据包时，格式必须为 PTP、DTP 或 VTP。" );
      return false;
    }
    if ( tileSize != 33 && tileSize != 129 && tileSize != 256 )
    {
      error = QStringLiteral( "瓦片大小必须为 33、129 或 256。" );
      return false;
    }

    QList<TileSource> tiles;
    const QgsMtpl::PackageOperations::CancelCheck scanCancelCheck = format == QgsMtpl::PackageFormat::Ptp
      ? cancelCheck : QgsMtpl::PackageOperations::CancelCheck();
    if ( !scanTileTree( sourcePath, tiles, error, scanCancelCheck ) )
      return false;
    if ( format == QgsMtpl::PackageFormat::Ptp && !validatePtpTileMatrix( tiles, tileSize, metadata, cancelCheck, error ) )
      return false;

    const CryptoContext crypto( keys );
    const QByteArray destinationUtf8 = destinationPath.toUtf8();
    const mtpl_buffer_view_t metadataView = {
      reinterpret_cast<const uint8_t *>( metadata.constData() ), static_cast<size_t>( metadata.size() )
    };
    void *writer = nullptr;
    mtpl_status_t status = tileWriterCreate( format, destinationUtf8, static_cast<uint32_t>( tileSize ), metadataView, mode, crypto.get(), &writer );
    for ( const TileSource &tile : std::as_const( tiles ) )
    {
      if ( status != MTPL_STATUS_OK )
        break;
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        status = MTPL_STATUS_CANCELED;
        break;
      }
      const mtpl_tile_range_t range = {
        tile.coordinate.zoom, tile.coordinate.x, tile.coordinate.x, tile.coordinate.y, tile.coordinate.y
      };
      status = tileWriterAddRange( format, writer, &range );
    }
    for ( const TileSource &tile : std::as_const( tiles ) )
    {
      if ( status != MTPL_STATUS_OK )
        break;
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        status = MTPL_STATUS_CANCELED;
        break;
      }
      const QByteArray sourceUtf8 = tile.path.toUtf8();
      status = tileWriterAddFile( format, writer, &tile.coordinate, sourceUtf8 );
    }
    if ( writer )
    {
      const mtpl_status_t closeStatus = tileWriterClose( format, writer );
      if ( status == MTPL_STATUS_OK )
        status = closeStatus;
    }
    if ( status != MTPL_STATUS_OK )
    {
      if ( error.isEmpty() )
        error = statusError( QStringLiteral( "无法创建瓦片数据包" ), status );
      return false;
    }
    return verifyCreatedTilePackage( destinationPath, format, tileSize, metadata, tiles, keys, mode, cancelCheck, error );
  }

  bool verifyCreatedPtpDataset( const QString &stagingPath,
                                const QString &finalPath,
                                const QgsMtpl::CryptoKeys &keys,
                                const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                                QString &error )
  {
    QgsMtpl::PackageDescriptor descriptor;
    if ( !QgsMtplPackageService::probePackage(
           stagingPath, descriptor, error, keys, true,
           keys.isValid() ? QgsMtpl::CredentialSource::Explicit : QgsMtpl::CredentialSource::None,
           cancelCheck, {}, false, true ) )
      return false;
    if ( canceled( cancelCheck ) )
    {
      error = QStringLiteral( "数据包操作已取消。" );
      return false;
    }
    // Validate the name the user will load, not the staging file's temporary name.
    descriptor.path = finalPath;
    descriptor.displayName = QFileInfo( finalPath ).completeBaseName();
    QgsMtpl::TileDatasetBuildOptions options;
    QgsMtpl::TilePackageAddress address;
    if ( QgsMtpl::TileResolver::parsePackageFileName( QFileInfo( finalPath ).fileName(), address ) )
    {
      options.availableRules = QgsMtpl::PartitionRuleStore::loadAllRules( &error );
      if ( !error.isEmpty() )
        return false;
    }
    const QgsMtpl::TileDatasetBuildResult dataset = QgsMtpl::buildTileDataset( finalPath, false, { descriptor }, options );
    if ( !dataset.ok() )
    {
      error = QStringLiteral( "创建的 PTP 无法作为影像数据集加载：%1" ).arg( dataset.errorString() );
      return false;
    }
    return true;
  }

  bool scanSfpDirectory( const QString &rootPath, QList<SfpSource> &files, QString &error )
  {
    const QFileInfo rootInfo( rootPath );
    if ( !rootInfo.isDir() || isReparseOrLink( rootInfo ) )
    {
      error = QStringLiteral( "SFP 源必须是常规文件夹，不能是链接或重解析点。" );
      return false;
    }
    const QString rootCanonical = rootInfo.canonicalFilePath();
    if ( rootCanonical.isEmpty() )
    {
      error = QStringLiteral( "无法解析 SFP 源文件夹。" );
      return false;
    }

    QDirIterator iterator( rootInfo.absoluteFilePath(),
                           QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                           QDirIterator::Subdirectories );
    while ( iterator.hasNext() )
    {
      iterator.next();
      const QFileInfo info = iterator.fileInfo();
      if ( isReparseOrLink( info ) )
      {
        error = QStringLiteral( "SFP 源中包含链接或重解析点。" );
        return false;
      }
      if ( info.isDir() )
        continue;
      if ( !info.isFile() || !isWithinRoot( rootCanonical, info.canonicalFilePath() ) )
      {
        error = QStringLiteral( "SFP 源中包含不安全的文件系统条目。" );
        return false;
      }
      SfpSource file;
      file.packagePath = QDir( rootInfo.absoluteFilePath() ).relativeFilePath( info.absoluteFilePath() ).replace( '\\', '/' );
      file.sourcePath = info.absoluteFilePath();
      if ( info.size() < 0 )
      {
        error = QStringLiteral( "无法确定 SFP 源文件长度。" );
        return false;
      }
      file.logicalSize = static_cast<uint64_t>( info.size() );
      if ( file.packagePath.isEmpty() || file.packagePath == QLatin1String( "." ) ||
           file.packagePath.startsWith( QLatin1String( "../" ) ) || QDir::isAbsolutePath( file.packagePath ) )
      {
        error = QStringLiteral( "SFP 源中包含不安全的相对路径。" );
        return false;
      }
      files.append( file );
    }
    std::sort( files.begin(), files.end(), []( const SfpSource &left, const SfpSource &right )
    {
      const int folded = left.packagePath.toCaseFolded().compare( right.packagePath.toCaseFolded() );
      return folded == 0 ? left.packagePath < right.packagePath : folded < 0;
    } );
    for ( int index = 1; index < files.size(); ++index )
    {
      if ( files.at( index - 1 ).packagePath.compare( files.at( index ).packagePath, Qt::CaseInsensitive ) == 0 )
      {
        error = QStringLiteral( "SFP 源中包含仅字母大小写不同的路径。" );
        return false;
      }
    }
    return true;
  }

  bool verifyCreatedSfpPackage( const QString &packagePath,
                                const QList<SfpSource> &files,
                                const QgsMtpl::CryptoKeys &keys,
                                mtpl_storage_mode_t mode,
                                const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                                const QgsMtpl::PackageOperations::ProgressCallback &progress,
                                QString &error )
  {
    const CryptoContext crypto( keys );
    const QByteArray packageUtf8 = packagePath.toUtf8();
    mtpl_sfp_reader_t *reader = nullptr;
    mtpl_status_t status = mtpl_sfp_reader_open( packageUtf8.constData(), crypto.get(), &reader );
    size_t entryCount = 0;
    if ( status == MTPL_STATUS_OK )
      status = mtpl_sfp_reader_get_entry_count( reader, &entryCount );
    bool ok = status == MTPL_STATUS_OK && entryCount == static_cast<size_t>( files.size() );
    uint64_t totalLogicalSize = 0;
    for ( const SfpSource &file : files )
    {
      if ( file.logicalSize > std::numeric_limits<uint64_t>::max() - totalLogicalSize )
      {
        status = MTPL_STATUS_LIMIT_EXCEEDED;
        ok = false;
        break;
      }
      totalLogicalSize += file.logicalSize;
      if ( file.logicalSize > std::numeric_limits<uint64_t>::max() - totalLogicalSize )
      {
        status = MTPL_STATUS_LIMIT_EXCEEDED;
        ok = false;
        break;
      }
      totalLogicalSize += file.logicalSize;
    }
    uint64_t completedLogicalSize = 0;
    reportProgress( progress, 80.0, 100.0, 0, totalLogicalSize );
    for ( int index = 0; ok && index < files.size(); ++index )
    {
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        ok = false;
        break;
      }
      mtpl_sfp_entry_info_t entry = {};
      status = mtpl_sfp_reader_get_entry_info( reader, static_cast<size_t>( index ), &entry );
      const SfpSource &source = files.at( index );
      const QByteArray expectedPath = source.packagePath.toUtf8();
      if ( status != MTPL_STATUS_OK || !entry.path || QByteArray( entry.path ) != expectedPath ||
           entry.storage_mode != mode || entry.logical_size != source.logicalSize )
      {
        ok = false;
        break;
      }

      QByteArray sourceDigest;
      QByteArray packageDigest;
      if ( !streamFileDigest( source.sourcePath, source.logicalSize, cancelCheck, progress,
                              completedLogicalSize, totalLogicalSize, 80.0, 100.0,
                              sourceDigest, error ) ||
           !streamSfpDigest( reader, entry.path, entry.logical_size, cancelCheck, progress,
                             completedLogicalSize, totalLogicalSize, 80.0, 100.0,
                             packageDigest, error ) ||
           sourceDigest != packageDigest )
        ok = false;
    }
    if ( reader )
    {
      const mtpl_status_t closeStatus = mtpl_sfp_reader_close( reader );
      if ( status == MTPL_STATUS_OK )
        status = closeStatus;
    }
    if ( !ok || status != MTPL_STATUS_OK )
    {
      if ( error.isEmpty() )
        error = status == MTPL_STATUS_OK
          ? QStringLiteral( "创建的 SFP 数据包未通过内容验证。" )
          : statusError( QStringLiteral( "创建的 SFP 数据包未通过内容验证" ), status );
      return false;
    }
    reportProgress( progress, 80.0, 100.0, 1, 1 );
    return true;
  }

  bool createSfpPackage( const QString &sourcePath,
                         const QString &destinationPath,
                         const QgsMtpl::CryptoKeys &keys,
                         mtpl_storage_mode_t mode,
                         const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                         const QgsMtpl::PackageOperations::ProgressCallback &progress,
                         QString &error )
  {
    QList<SfpSource> files;
    if ( !scanSfpDirectory( sourcePath, files, error ) )
      return false;

    const CryptoContext crypto( keys );
    mtpl_sfp_builder_t *builder = nullptr;
    mtpl_status_t status = mtpl_sfp_builder_create( crypto.get(), &builder );
    for ( const SfpSource &file : std::as_const( files ) )
    {
      if ( status != MTPL_STATUS_OK )
        break;
      if ( canceled( cancelCheck ) )
      {
        error = QStringLiteral( "数据包操作已取消。" );
        status = MTPL_STATUS_CANCELED;
        break;
      }
      const QByteArray packageUtf8 = file.packagePath.toUtf8();
      const QByteArray sourceUtf8 = file.sourcePath.toUtf8();
      status = mtpl_sfp_builder_add_file( builder, packageUtf8.constData(), sourceUtf8.constData(), mode );
    }
    if ( status == MTPL_STATUS_OK )
    {
      const QByteArray destinationUtf8 = destinationPath.toUtf8();
      TranscodeCallbackContext callbackContext;
      callbackContext.cancelCheck = &cancelCheck;
      callbackContext.progress = &progress;
      callbackContext.progressStart = 0.0;
      callbackContext.progressEnd = 80.0;
      status = mtpl_sfp_builder_write_with_control(
        builder, destinationUtf8.constData(), transcodeProgress, transcodeCanceled, &callbackContext );
    }
    if ( builder )
      mtpl_sfp_builder_destroy( builder );
    if ( status != MTPL_STATUS_OK )
    {
      if ( error.isEmpty() )
        error = statusError( QStringLiteral( "无法创建 SFP 数据包" ), status );
      return false;
    }
    return verifyCreatedSfpPackage( destinationPath, files, keys, mode, cancelCheck, progress, error );
  }

  bool convertOne( const QgsMtpl::ConversionInput &input,
                   const QString &stagingPath,
                   const QgsMtpl::CryptoKeys &outputKeys,
                   mtpl_storage_mode_t mode,
                   bool preserveSfpMixedStorage,
                   const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                   const QgsMtpl::PackageOperations::ProgressCallback &progress,
                   double progressStart,
                   double progressEnd,
                   QString &error )
  {
    const CryptoContext sourceCrypto( input.keys );
    const CryptoContext destinationCrypto( outputKeys );
    const double verificationStart = progressStart + ( progressEnd - progressStart ) * 0.7;
    TranscodeCallbackContext callbackContext;
    callbackContext.cancelCheck = &cancelCheck;
    callbackContext.progress = &progress;
    callbackContext.progressStart = progressStart;
    callbackContext.progressEnd = verificationStart;

    mtpl_package_info_t sourceInfo = MTPL_PACKAGE_INFO_INIT;
    const QByteArray sourcePath = input.path.toUtf8();
    const mtpl_status_t probeStatus = mtpl_package_probe( sourcePath.constData(), &sourceInfo );
    if ( probeStatus != MTPL_STATUS_OK || sourceInfo.format == MTPL_PACKAGE_FORMAT_UNKNOWN )
    {
      error = probeStatus == MTPL_STATUS_OK
        ? QStringLiteral( "不支持源数据包的格式。" )
        : statusError( QStringLiteral( "无法检查源数据包" ), probeStatus );
      return false;
    }

    mtpl_transcode_options_t options = MTPL_TRANSCODE_OPTIONS_INIT;
    options.source_crypto = sourceCrypto.get();
    options.destination_crypto = destinationCrypto.get();
    options.destination_storage = mode == MTPL_STORAGE_ENCRYPTED
      ? MTPL_PACKAGE_STORAGE_ENCRYPTED
      : MTPL_PACKAGE_STORAGE_PLAIN;
    if ( preserveSfpMixedStorage && mode == MTPL_STORAGE_ENCRYPTED )
    {
      if ( sourceInfo.format == MTPL_PACKAGE_FORMAT_SFP && sourceInfo.storage == MTPL_PACKAGE_STORAGE_MIXED )
        options.destination_storage = MTPL_PACKAGE_STORAGE_MIXED;
    }
    options.cancel_callback = transcodeCanceled;
    options.progress_callback = transcodeProgress;
    options.user_data = &callbackContext;

    const QByteArray destinationPath = stagingPath.toUtf8();
    const mtpl_status_t status = mtpl_package_transcode( sourcePath.constData(), destinationPath.constData(), &options );
    if ( status == MTPL_STATUS_OK )
    {
      QString verificationError;
      if ( !verifyPackageConversion( input.path, stagingPath, input.keys, outputKeys,
                                     options.destination_storage, cancelCheck, progress,
                                     verificationStart, progressEnd, verificationError ) )
      {
        error = canceled( cancelCheck )
          ? QStringLiteral( "数据包操作已取消。" )
          : QStringLiteral( "转换完成但内容验证失败：%1" ).arg( verificationError );
        return false;
      }
      return true;
    }

    if ( status == MTPL_STATUS_CANCELED )
      error = QStringLiteral( "数据包操作已取消。" );
    else
      error = statusError( QStringLiteral( "无法转换数据包" ), status );
    return false;
  }

  bool saveSidecar( const QString &sidecarPath,
                    const QStringList &packagePaths,
                    const QgsMtpl::KeyMaterial &keys,
                    const QgsMtpl::PackageOperations::CancelCheck &cancelCheck,
                    QString &error )
  {
    if ( canceled( cancelCheck ) )
    {
      error = QStringLiteral( "数据包操作已取消。" );
      return false;
    }
    QgsMtpl::KeySidecar sidecar;
    if ( !QgsMtpl::KeySidecarStore::createForPackages( sidecarPath, packagePaths, keys, sidecar, error, cancelCheck ) )
      return false;
    if ( canceled( cancelCheck ) )
    {
      error = QStringLiteral( "数据包操作已取消。" );
      return false;
    }
    return QgsMtpl::KeySidecarStore::write( sidecarPath, sidecar, error );
  }

  bool removeOutputFile( const QString &path )
  {
    return path.isEmpty() || !QFileInfo::exists( path ) || QFile::remove( path ) || !QFileInfo::exists( path );
  }

  bool removeStagingFile( const QString &path, QString &error )
  {
    if ( removeOutputFile( path ) )
      return true;

    const QString cleanupError = QStringLiteral( "无法删除临时数据包：%1" ).arg( QDir::toNativeSeparators( path ) );
    error = error.isEmpty() ? cleanupError : QStringLiteral( "%1，%2" ).arg( error, cleanupError );
    return false;
  }

  void cleanupOutputsAfterSidecarFailure( const QStringList &packagePaths,
                                          const QString &sidecarPath,
                                          const QString &failure,
                                          QgsMtpl::PackageOperationResult &result )
  {
    result.ok = false;
    result.outputPaths.clear();
    result.sidecarPath.clear();
    QStringList retained;
    for ( const QString &packagePath : packagePaths )
    {
      if ( !removeOutputFile( packagePath ) )
      {
        result.outputPaths.append( packagePath );
        retained.append( QDir::toNativeSeparators( packagePath ) );
      }
    }
    if ( !removeOutputFile( sidecarPath ) )
    {
      result.sidecarPath = sidecarPath;
      retained.append( QDir::toNativeSeparators( sidecarPath ) );
    }
    result.error = retained.isEmpty()
      ? failure
      : QStringLiteral( "%1，且无法删除以下输出：%2" ).arg( failure, retained.join( QStringLiteral( "、" ) ) );
  }

  void cancelCommittedOutputs( const QString &packagePath,
                               const QString &sidecarPath,
                               QgsMtpl::PackageOperationResult &result )
  {
    const bool packageRemoved = removeOutputFile( packagePath );
    const bool sidecarRemoved = removeOutputFile( sidecarPath );
    result.ok = false;
    result.canceled = true;
    result.outputPaths.clear();
    result.sidecarPath.clear();
    QStringList retained;
    if ( !packageRemoved )
    {
      retained.append( QDir::toNativeSeparators( packagePath ) );
      result.outputPaths.append( packagePath );
    }
    if ( !sidecarRemoved )
    {
      retained.append( QDir::toNativeSeparators( sidecarPath ) );
      result.sidecarPath = sidecarPath;
    }
    result.error = retained.isEmpty()
      ? QStringLiteral( "数据包操作已取消。" )
      : QStringLiteral( "数据包操作已取消，但无法删除以下输出：%1" ).arg( retained.join( QStringLiteral( "、" ) ) );
  }
}

QgsMtpl::PackageOperationResult QgsMtpl::PackageOperations::execute( const PackageOperationRequest &request,
                                                                     const CancelCheck &cancelCheck,
                                                                     const ProgressCallback &progress )
{
  PackageOperationResult result;
  bool cancellationObserved = false;
  const CancelCheck effectiveCancelCheck = [&cancelCheck, &cancellationObserved]
  {
    if ( !cancellationObserved && cancelCheck )
      cancellationObserved = cancelCheck();
    return cancellationObserved;
  };
  if ( progress )
    progress( 0.0 );

  const mtpl_storage_mode_t outputMode = request.encryptOutput ? MTPL_STORAGE_ENCRYPTED : MTPL_STORAGE_PLAIN;
  KeyMaterial outputKeyMaterial = request.outputKeys;
  if ( request.encryptOutput && !outputKeyMaterial.isValid() )
  {
    if ( !request.generateOutputKeys )
    {
      result.error = QStringLiteral( "创建加密数据包需要有效的输出密钥。" );
      return result;
    }
    outputKeyMaterial = KeySidecarStore::generateKeys();
  }
  const CryptoKeys outputKeys = request.encryptOutput ? outputKeyMaterial.cryptoKeys() : CryptoKeys();
  result.outputKeys = outputKeyMaterial;

  if ( canceled( effectiveCancelCheck ) )
  {
    result.canceled = true;
    result.error = QStringLiteral( "数据包操作已取消。" );
    return result;
  }

  if ( request.type == PackageOperationType::Convert )
  {
    QList<ConversionInput> inputs = request.inputs;
    double conversionProgressStart = 0.0;
    if ( inputs.isEmpty() && !request.conversionSourcePath.trimmed().isEmpty() )
    {
      const ProbeResult probe = QgsMtplPackageService::probePath(
        request.conversionSourcePath,
        request.suppliedSourceKeys,
        false,
        request.suppliedCredentialSource,
        effectiveCancelCheck,
        [&progress]( double value )
        {
          if ( progress )
            progress( value * 0.1 );
        } );
      if ( !probe.ok )
      {
        result.canceled = probe.canceled || canceled( effectiveCancelCheck );
        result.error = probe.error.isEmpty()
          ? ( result.canceled ? QStringLiteral( "数据包操作已取消。" ) : QStringLiteral( "无法检查源数据包。" ) )
          : probe.error;
        return result;
      }

      conversionProgressStart = 10.0;
      for ( const PackageDescriptor &descriptor : probe.packages )
      {
        ConversionInput input;
        input.path = descriptor.path;
        const quint64 encryptedEntryCount = descriptor.metadata
                                              .value( QStringLiteral( "encryptedEntryCount" ) )
                                              .toULongLong();
        const quint64 plainEntryCount = descriptor.metadata.value( QStringLiteral( "plainEntryCount" ) ).toULongLong();
        const bool mixedSfp = descriptor.format == PackageFormat::Sfp && encryptedEntryCount > 0 && plainEntryCount > 0;
        const bool rekey = request.encryptOutput && request.preserveSfpMixedStorage;
        const bool encrypt = request.encryptOutput && !rekey;
        if ( encrypt && descriptor.encryption == EncryptionState::Encrypted && !mixedSfp )
          input.skipReason = QStringLiteral( "数据包已完全加密，无需重复加密。" );
        else if ( ( !request.encryptOutput || rekey ) && descriptor.encryption == EncryptionState::Plain )
          input.skipReason = !request.encryptOutput
            ? QStringLiteral( "数据包未加密，无需解密。" )
            : QStringLiteral( "数据包未加密，无法更换密钥。" );

        if ( descriptor.credentialSource == CredentialSource::Explicit )
        {
          input.keys = request.suppliedSourceKeys;
          if ( descriptor.encryption == EncryptionState::Encrypted &&
               descriptor.readiness == ReadinessState::KeyVerified )
            result.explicitSourceKeysVerified = true;
        }
        else if ( descriptor.credentialSource == CredentialSource::Remembered )
        {
          input.keys = request.suppliedSourceKeys;
        }
        else if ( descriptor.credentialSource == CredentialSource::Sidecar )
        {
          KeySidecar sidecar;
          QString sidecarPath;
          QString discoveryError;
          if ( KeySidecarStore::discover( input.path, sidecar, sidecarPath, discoveryError, effectiveCancelCheck ) )
            input.keys = sidecar.keys.cryptoKeys();
          else if ( canceled( effectiveCancelCheck ) )
          {
            result.canceled = true;
            result.error = QStringLiteral( "数据包操作已取消。" );
            return result;
          }
        }
        inputs.append( input );
      }
    }

    if ( inputs.isEmpty() )
    {
      result.error = QStringLiteral( "请至少选择一个源数据包。" );
      return result;
    }

    if ( inputs.size() == 1 && request.outputDirectory.trimmed().isEmpty() )
    {
      PackageOperationItemResult itemResult;
      itemResult.inputPath = inputs.constFirst().path;
      itemResult.outputPath = QFileInfo( request.outputPath ).absoluteFilePath();
      if ( !inputs.constFirst().skipReason.isEmpty() )
      {
        result.error = inputs.constFirst().skipReason;
        itemResult.status = PackageOperationItemStatus::Skipped;
        itemResult.message = result.error;
        result.items.append( itemResult );
        return result;
      }
      const QFileInfo inputInfo( itemResult.inputPath );
      PackageFormat inputFormat = PackageFormat::Unknown;
      QString probeError;
      if ( !inputInfo.isFile() || !probePackageFormat( itemResult.inputPath, inputFormat, probeError ) )
      {
        result.error = probeError.isEmpty() ? QStringLiteral( "源数据包必须存在且格式受支持。" ) : probeError;
        itemResult.status = inputInfo.isFile() ? PackageOperationItemStatus::Failed : PackageOperationItemStatus::Skipped;
        itemResult.message = result.error;
        result.items.append( itemResult );
        return result;
      }
      if ( !prepareOutputFile( request.outputPath, inputFormat, result.error ) )
      {
        itemResult.status = PackageOperationItemStatus::Skipped;
        itemResult.message = result.error;
        result.items.append( itemResult );
        return result;
      }
      const QString finalPath = QFileInfo( request.outputPath ).absoluteFilePath();
      itemResult.outputPath = finalPath;
      const QString sidecarPath = KeySidecarStore::singleSidecarPath( finalPath );
      if ( request.encryptOutput && request.writeSidecar && QFileInfo::exists( sidecarPath ) )
      {
        result.error = QStringLiteral( "输出密钥附属文件已存在。" );
        itemResult.status = PackageOperationItemStatus::Skipped;
        itemResult.message = result.error;
        result.items.append( itemResult );
        return result;
      }
      const QString stagingPath = stagingFilePath( finalPath );
      if ( QFileInfo::exists( stagingPath ) )
      {
        result.error = QStringLiteral( "无法分配唯一的临时数据包路径。" );
        itemResult.message = result.error;
        result.items.append( itemResult );
        return result;
      }
      if ( !convertOne( inputs.constFirst(), stagingPath, outputKeys, outputMode,
                        request.preserveSfpMixedStorage, effectiveCancelCheck, progress,
                        conversionProgressStart, 90.0, result.error ) )
      {
        removeStagingFile( stagingPath, result.error );
        result.canceled = canceled( effectiveCancelCheck );
        itemResult.status = result.canceled ? PackageOperationItemStatus::Skipped : PackageOperationItemStatus::Failed;
        itemResult.message = result.error;
        result.items.append( itemResult );
        return result;
      }
      if ( canceled( effectiveCancelCheck ) )
      {
        result.canceled = true;
        result.error = QStringLiteral( "数据包操作已取消。" );
        removeStagingFile( stagingPath, result.error );
        itemResult.status = PackageOperationItemStatus::Skipped;
        itemResult.message = result.error;
        result.items.append( itemResult );
        return result;
      }
      if ( !QFile::rename( stagingPath, finalPath ) )
      {
        result.error = QStringLiteral( "无法以原子方式提交转换后的数据包。" );
        removeStagingFile( stagingPath, result.error );
        itemResult.message = result.error;
        result.items.append( itemResult );
        return result;
      }
      if ( canceled( effectiveCancelCheck ) )
      {
        cancelCommittedOutputs( finalPath, QString(), result );
        itemResult.status = PackageOperationItemStatus::Skipped;
        itemResult.message = result.error;
        result.items.append( itemResult );
        return result;
      }
      if ( request.encryptOutput && request.writeSidecar )
      {
        QString sidecarError;
        if ( !saveSidecar( sidecarPath, QStringList() << finalPath, outputKeyMaterial, effectiveCancelCheck, sidecarError ) )
        {
          result.canceled = canceled( effectiveCancelCheck );
          cleanupOutputsAfterSidecarFailure( QStringList() << finalPath, sidecarPath, sidecarError, result );
          itemResult.status = result.canceled ? PackageOperationItemStatus::Skipped : PackageOperationItemStatus::Failed;
          itemResult.message = result.error;
          result.items.append( itemResult );
          return result;
        }
        result.sidecarPath = sidecarPath;
      }
      if ( canceled( effectiveCancelCheck ) )
      {
        cancelCommittedOutputs( finalPath, result.sidecarPath, result );
        itemResult.status = PackageOperationItemStatus::Skipped;
        itemResult.message = result.error;
        result.items.append( itemResult );
        return result;
      }
      result.outputPaths.append( finalPath );
      itemResult.status = PackageOperationItemStatus::Succeeded;
      itemResult.message = QStringLiteral( "转换成功。" );
      result.items.append( itemResult );
      result.ok = true;
    }
    else
    {
      const QFileInfo outputDirectoryInfo( request.outputDirectory );
      if ( request.outputDirectory.trimmed().isEmpty() ||
           ( outputDirectoryInfo.exists() && !outputDirectoryInfo.isDir() ) ||
           ( !outputDirectoryInfo.exists() && !QFileInfo( outputDirectoryInfo.absolutePath() ).isDir() ) )
      {
        result.error = QStringLiteral( "批量转换需要一个文件夹，或一个上级文件夹已存在的新路径。" );
        return result;
      }
      const QString finalDirectory = outputDirectoryInfo.absoluteFilePath();
      const bool createdDirectory = !outputDirectoryInfo.exists();
      if ( createdDirectory && !QDir().mkdir( finalDirectory ) )
      {
        result.error = QStringLiteral( "无法创建批量转换输出文件夹。" );
        return result;
      }
      const QString batchSidecarPath = request.encryptOutput && request.writeSidecar
        ? QDir( finalDirectory ).filePath( KeySidecarStore::batchSidecarName( outputKeyMaterial.keyId ) )
        : QString();
      if ( !batchSidecarPath.isEmpty() && QFileInfo::exists( batchSidecarPath ) )
      {
        result.error = QStringLiteral( "输出密钥附属文件已存在，未覆盖现有文件。" );
        if ( createdDirectory )
          QDir().rmdir( finalDirectory );
        return result;
      }

      QSet<QString> sources;
      QSet<QString> outputNames;
      auto appendCanceledItems = [&]( int startIndex )
      {
        for ( int remainingIndex = startIndex; remainingIndex < inputs.size(); ++remainingIndex )
        {
          PackageOperationItemResult canceledItem;
          canceledItem.inputPath = inputs.at( remainingIndex ).path;
          canceledItem.status = PackageOperationItemStatus::Skipped;
          canceledItem.message = QStringLiteral( "数据包操作已取消，未处理此项。" );
          result.items.append( canceledItem );
        }
      };
      for ( int index = 0; index < inputs.size(); ++index )
      {
        if ( canceled( effectiveCancelCheck ) )
        {
          result.canceled = true;
          result.error = QStringLiteral( "数据包操作已取消。" );
          appendCanceledItems( index );
          break;
        }

        const ConversionInput &input = inputs.at( index );
        const QFileInfo inputInfo( input.path );
        PackageOperationItemResult itemResult;
        itemResult.inputPath = input.path;
        const double conversionProgressSpan = 90.0 - conversionProgressStart;
        const double packageProgressStart = conversionProgressStart +
                                            conversionProgressSpan * static_cast<double>( index ) / inputs.size();
        const double packageProgressEnd = conversionProgressStart +
                                          conversionProgressSpan * static_cast<double>( index + 1 ) / inputs.size();
        auto finishWithoutOutput = [&]( PackageOperationItemStatus status, const QString &message )
        {
          itemResult.status = status;
          itemResult.message = message;
          result.items.append( itemResult );
          if ( progress )
            progress( packageProgressEnd );
        };

        if ( !input.skipReason.isEmpty() )
        {
          finishWithoutOutput( PackageOperationItemStatus::Skipped, input.skipReason );
          continue;
        }
        if ( !inputInfo.isFile() )
        {
          finishWithoutOutput( PackageOperationItemStatus::Skipped, QStringLiteral( "源数据包不存在或不是常规文件。" ) );
          continue;
        }
        PackageFormat inputFormat = PackageFormat::Unknown;
        QString probeError;
        if ( !probePackageFormat( input.path, inputFormat, probeError ) )
        {
          finishWithoutOutput( PackageOperationItemStatus::Failed, probeError );
          continue;
        }
        const QString canonical = inputInfo.canonicalFilePath().toCaseFolded();
        if ( canonical.isEmpty() || sources.contains( canonical ) )
        {
          finishWithoutOutput( PackageOperationItemStatus::Skipped, QStringLiteral( "源数据包路径无效或已重复。" ) );
          continue;
        }
        sources.insert( canonical );

        const QString fileName = inputInfo.fileName();
        if ( packageFormatFromPath( fileName ) != inputFormat )
        {
          finishWithoutOutput( PackageOperationItemStatus::Failed, QStringLiteral( "源文件扩展名与数据包格式不匹配。" ) );
          continue;
        }
        if ( outputNames.contains( fileName.toCaseFolded() ) )
        {
          finishWithoutOutput( PackageOperationItemStatus::Skipped, QStringLiteral( "输出文件名与批次中的其他数据包冲突。" ) );
          continue;
        }
        outputNames.insert( fileName.toCaseFolded() );
        const QString finalPath = QDir( finalDirectory ).filePath( fileName );
        itemResult.outputPath = finalPath;
        if ( QFileInfo::exists( finalPath ) )
        {
          finishWithoutOutput( PackageOperationItemStatus::Skipped, QStringLiteral( "输出数据包已存在，未覆盖目标文件。" ) );
          continue;
        }
        const QString stagingPath = stagingFilePath( finalPath );
        if ( QFileInfo::exists( stagingPath ) )
        {
          finishWithoutOutput( PackageOperationItemStatus::Failed, QStringLiteral( "无法分配唯一的临时数据包路径。" ) );
          continue;
        }
        QString conversionError;
        if ( !convertOne( input, stagingPath, outputKeys, outputMode,
                          request.preserveSfpMixedStorage, effectiveCancelCheck, progress,
                          packageProgressStart, packageProgressEnd, conversionError ) )
        {
          removeStagingFile( stagingPath, conversionError );
          if ( canceled( effectiveCancelCheck ) )
          {
            result.canceled = true;
            result.error = conversionError.isEmpty() ? QStringLiteral( "数据包操作已取消。" ) : conversionError;
            finishWithoutOutput( PackageOperationItemStatus::Skipped, result.error );
            appendCanceledItems( index + 1 );
            break;
          }
          finishWithoutOutput( PackageOperationItemStatus::Failed, conversionError );
          continue;
        }
        if ( canceled( effectiveCancelCheck ) )
        {
          result.canceled = true;
          result.error = QStringLiteral( "数据包操作已取消。" );
          removeStagingFile( stagingPath, result.error );
          finishWithoutOutput( PackageOperationItemStatus::Skipped, result.error );
          appendCanceledItems( index + 1 );
          break;
        }
        if ( QFileInfo::exists( finalPath ) || !QFile::rename( stagingPath, finalPath ) )
        {
          QString commitError = QStringLiteral( "无法以原子方式提交转换后的数据包，目标文件未被覆盖。" );
          removeStagingFile( stagingPath, commitError );
          finishWithoutOutput( PackageOperationItemStatus::Failed, commitError );
          continue;
        }
        itemResult.status = PackageOperationItemStatus::Succeeded;
        itemResult.message = QStringLiteral( "转换成功。" );
        result.items.append( itemResult );
        result.outputPaths.append( finalPath );
        if ( canceled( effectiveCancelCheck ) )
        {
          result.canceled = true;
          result.error = QStringLiteral( "数据包操作已取消。" );
          appendCanceledItems( index + 1 );
          break;
        }
      }

      if ( !result.canceled && !result.outputPaths.isEmpty() && canceled( effectiveCancelCheck ) )
      {
        result.canceled = true;
        result.error = QStringLiteral( "数据包操作已取消。" );
      }
      if ( result.canceled && !result.outputPaths.isEmpty() && request.encryptOutput && request.writeSidecar )
      {
        const QStringList generatedOutputs = result.outputPaths;
        cleanupOutputsAfterSidecarFailure(
          generatedOutputs,
          QString(),
          QStringLiteral( "数据包操作已取消，尚未生成密钥附属文件，已清理完成的加密输出。" ),
          result );
        for ( PackageOperationItemResult &item : result.items )
        {
          if ( item.status == PackageOperationItemStatus::Succeeded )
          {
            item.status = PackageOperationItemStatus::Skipped;
            item.message = QStringLiteral( "操作取消前尚未生成密钥附属文件，已清理此项输出。" );
          }
        }
        if ( result.outputPaths.isEmpty() && createdDirectory )
          QDir().rmdir( finalDirectory );
        return result;
      }

      if ( !result.outputPaths.isEmpty() && request.encryptOutput && request.writeSidecar )
      {
        const QString sidecarPath = batchSidecarPath;
        QString sidecarError;
        if ( !saveSidecar( sidecarPath, result.outputPaths, outputKeyMaterial, effectiveCancelCheck, sidecarError ) )
        {
          const QStringList generatedOutputs = result.outputPaths;
          result.canceled = canceled( effectiveCancelCheck );
          const QString failure = result.canceled
            ? QStringLiteral( "数据包操作已取消，尚未完成密钥附属文件，已清理完成的加密输出。" )
            : QStringLiteral( "无法创建批量密钥附属文件：%1" ).arg( sidecarError );
          cleanupOutputsAfterSidecarFailure( generatedOutputs, sidecarPath, failure, result );
          for ( PackageOperationItemResult &item : result.items )
          {
            if ( item.status == PackageOperationItemStatus::Succeeded )
            {
              item.status = result.canceled ? PackageOperationItemStatus::Skipped : PackageOperationItemStatus::Failed;
              item.message = failure;
            }
          }
          if ( result.outputPaths.isEmpty() && result.sidecarPath.isEmpty() && createdDirectory )
            QDir().rmdir( finalDirectory );
          return result;
        }
        result.sidecarPath = sidecarPath;
        if ( canceled( effectiveCancelCheck ) )
        {
          result.canceled = true;
          result.error = QStringLiteral( "数据包操作已取消，已保留完成的数据包和密钥附属文件。" );
        }
      }

      int skippedCount = 0;
      int failedCount = 0;
      for ( const PackageOperationItemResult &item : std::as_const( result.items ) )
      {
        skippedCount += item.status == PackageOperationItemStatus::Skipped ? 1 : 0;
        failedCount += item.status == PackageOperationItemStatus::Failed ? 1 : 0;
      }
      if ( result.outputPaths.isEmpty() && createdDirectory )
        QDir().rmdir( finalDirectory );
      if ( result.canceled )
      {
        const QString cancelSummary = QStringLiteral( "数据包操作已取消，已保留 %1 个完成项。" ).arg( result.outputPaths.size() );
        if ( result.error.isEmpty() || result.error == QLatin1String( "数据包操作已取消。" ) )
          result.error = cancelSummary;
        else if ( !result.error.contains( cancelSummary ) )
          result.error += QLatin1Char( '\n' ) + cancelSummary;
      }
      else if ( result.outputPaths.isEmpty() )
        result.error = QStringLiteral( "批量转换未生成输出，%1 项已跳过，%2 项失败。" ).arg( skippedCount ).arg( failedCount );
      else if ( skippedCount > 0 || failedCount > 0 )
        result.error = QStringLiteral( "批量转换部分完成，%1 项成功，%2 项跳过，%3 项失败。" )
                         .arg( result.outputPaths.size() ).arg( skippedCount ).arg( failedCount );
      result.ok = !result.canceled && !result.outputPaths.isEmpty();
    }
  }
  else
  {
    const PackageFormat format = request.type == PackageOperationType::CreateSfpPackage ? PackageFormat::Sfp : request.outputFormat;
    if ( !prepareOutputFile( request.outputPath, format, result.error ) )
      return result;
    const QString finalPath = QFileInfo( request.outputPath ).absoluteFilePath();
    const QString sidecarPath = KeySidecarStore::singleSidecarPath( finalPath );
    if ( request.encryptOutput && request.writeSidecar && QFileInfo::exists( sidecarPath ) )
    {
      result.error = QStringLiteral( "输出密钥附属文件已存在。" );
      return result;
    }
    const QString stagingPath = stagingFilePath( finalPath );
    bool created = false;
    if ( request.type == PackageOperationType::CreateSfpPackage )
      created = createSfpPackage( request.sourcePath, stagingPath, outputKeys, outputMode,
                                  effectiveCancelCheck, progress, result.error );
    else
      created = createTilePackage( request.sourcePath, stagingPath, format, request.tileSize,
                                   request.metadata, outputKeys, outputMode, effectiveCancelCheck, result.error );
    if ( created && format == PackageFormat::Ptp )
      created = verifyCreatedPtpDataset( stagingPath, finalPath, outputKeys, effectiveCancelCheck, result.error );
    if ( !created )
    {
      removeStagingFile( stagingPath, result.error );
      result.canceled = canceled( effectiveCancelCheck );
      return result;
    }
    if ( canceled( effectiveCancelCheck ) )
    {
      result.canceled = true;
      result.error = QStringLiteral( "数据包操作已取消。" );
      removeStagingFile( stagingPath, result.error );
      return result;
    }
    if ( !QFile::rename( stagingPath, finalPath ) )
    {
      result.error = QStringLiteral( "无法以原子方式提交创建的数据包。" );
      removeStagingFile( stagingPath, result.error );
      return result;
    }
    if ( canceled( effectiveCancelCheck ) )
    {
      cancelCommittedOutputs( finalPath, QString(), result );
      return result;
    }
    if ( request.encryptOutput && request.writeSidecar )
    {
      QString sidecarError;
      if ( !saveSidecar( sidecarPath, QStringList() << finalPath, outputKeyMaterial, effectiveCancelCheck, sidecarError ) )
      {
        result.canceled = canceled( effectiveCancelCheck );
        cleanupOutputsAfterSidecarFailure( QStringList() << finalPath, sidecarPath, sidecarError, result );
        return result;
      }
      result.sidecarPath = sidecarPath;
    }
    if ( canceled( effectiveCancelCheck ) )
    {
      cancelCommittedOutputs( finalPath, result.sidecarPath, result );
      return result;
    }
    result.outputPaths.append( finalPath );
    result.ok = true;
  }

  if ( progress && result.ok )
    progress( 100.0 );
  return result;
}

QgsMtplPackageOperationTask::QgsMtplPackageOperationTask( const QgsMtpl::PackageOperationRequest &request )
  : QgsTask( request.type == QgsMtpl::PackageOperationType::Convert
               ? tr( "转换 MTPL 数据包" )
               : tr( "创建 MTPL 数据包" ),
             QgsTask::CanCancel | QgsTask::CancelWithoutPrompt )
  , mRequest( request )
{
  QMutexLocker locker( &sActiveTaskMutex );
  sActiveTasks.insert( this );
}

QgsMtplPackageOperationTask::~QgsMtplPackageOperationTask()
{
  QMutexLocker locker( &sActiveTaskMutex );
  sActiveTasks.remove( this );
}

void QgsMtplPackageOperationTask::cancelAndWaitForAllActiveTasks()
{
  while ( true )
  {
    QList<QPointer<QgsMtplPackageOperationTask>> tasks;
    {
      QMutexLocker locker( &sActiveTaskMutex );
      tasks.reserve( sActiveTasks.size() );
      for ( QgsMtplPackageOperationTask *task : std::as_const( sActiveTasks ) )
        tasks.append( task );
    }

    if ( tasks.isEmpty() )
      return;

    for ( const QPointer<QgsMtplPackageOperationTask> &task : std::as_const( tasks ) )
    {
      if ( task )
        task->cancel();
    }
    for ( const QPointer<QgsMtplPackageOperationTask> &task : std::as_const( tasks ) )
    {
      if ( task )
        task->waitForFinished( 0 );
    }

    // Task completion is reported to QgsTaskManager through queued events. Drain
    // these events, followed by deferred deletes, while the plugin code is still
    // loaded. The destructor removes the task from sActiveTasks.
    QCoreApplication::sendPostedEvents( nullptr, 0 );
    QCoreApplication::sendPostedEvents( nullptr, QEvent::DeferredDelete );
  }
}

bool QgsMtplPackageOperationTask::run()
{
  mResult = QgsMtpl::PackageOperations::execute(
    mRequest,
    [this]() { return isCanceled(); },
    [this]( double value ) { setProgress( value ); } );
  for ( QgsMtpl::ConversionInput &input : mRequest.inputs )
    input.keys.clear();
  mRequest.suppliedSourceKeys.clear();
  mRequest.outputKeys.privateKeyBase64.fill( '\0' );
  mRequest.outputKeys.deviceKeyHex.fill( '\0' );
  mRequest.outputKeys = QgsMtpl::KeyMaterial();
  return mResult.ok;
}
