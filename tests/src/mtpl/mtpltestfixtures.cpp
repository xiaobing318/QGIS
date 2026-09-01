/***************************************************************************
  mtpltestfixtures.cpp
  --------------------
  Self-contained fixtures for the built-in MTPL plugin tests.
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

#include <mtpl/mtpl.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace
{

class CryptoContext
{
  public:
    explicit CryptoContext( const QgsMtpl::CryptoKeys &keys )
      : mKeys( keys )
    {
      mOptions.private_key = reinterpret_cast<const uint8_t *>( mKeys.privateKey.constData() );
      mOptions.private_key_size = static_cast<size_t>( mKeys.privateKey.size() );
      mOptions.device_key = reinterpret_cast<const uint8_t *>( mKeys.deviceKey.constData() );
      mOptions.device_key_size = static_cast<size_t>( mKeys.deviceKey.size() );
    }

    const mtpl_crypto_options_t *get() const
    {
      return mKeys.isEmpty() ? nullptr : &mOptions;
    }

  private:
    QgsMtpl::CryptoKeys mKeys;
    mtpl_crypto_options_t mOptions = {};
};

QString statusError( const QString &operation, mtpl_status_t status )
{
  const char *statusText = mtpl_status_string( status );
  return QStringLiteral( "%1: %2 (%3)." )
    .arg( operation,
          statusText ? QString::fromUtf8( statusText ) : QStringLiteral( "unknown MTPL status" ) )
    .arg( static_cast<int>( status ) );
}

bool isStorageModeSupported( mtpl_storage_mode_t storageMode )
{
  return storageMode == MTPL_STORAGE_PLAIN || storageMode == MTPL_STORAGE_ENCRYPTED;
}

bool prepareOutputPath( const QString &path, QString &error )
{
  if ( path.trimmed().isEmpty() )
  {
    error = QStringLiteral( "The fixture output path is empty." );
    return false;
  }

  const QFileInfo outputInfo( path );
  if ( outputInfo.exists() )
  {
    error = QStringLiteral( "The fixture output path already exists." );
    return false;
  }

  const QString parentPath = outputInfo.absolutePath();
  if ( !QDir().mkpath( parentPath ) )
  {
    error = QStringLiteral( "Unable to create the fixture output directory." );
    return false;
  }

  error.clear();
  return true;
}

void removePartialOutput( const QString &path, QString &error )
{
  if ( QFileInfo::exists( path ) && !QFile::remove( path ) )
    error += QStringLiteral( " Unable to remove the partial fixture." );
}

mtpl_status_t tileWriterCreate( QgsMtpl::PackageFormat format,
                                const QByteArray &path,
                                uint32_t tileSize,
                                mtpl_buffer_view_t metadata,
                                mtpl_storage_mode_t storageMode,
                                const mtpl_crypto_options_t *crypto,
                                void **writer )
{
  switch ( format )
  {
    case QgsMtpl::PackageFormat::Ptp:
      return mtpl_ptp_writer_create( path.constData(), tileSize, metadata, storageMode, crypto,
                                     reinterpret_cast<mtpl_ptp_writer_t **>( writer ) );
    case QgsMtpl::PackageFormat::Dtp:
      return mtpl_dtp_writer_create( path.constData(), tileSize, metadata, storageMode, crypto,
                                     reinterpret_cast<mtpl_dtp_writer_t **>( writer ) );
    case QgsMtpl::PackageFormat::Vtp:
      return mtpl_vtp_writer_create( path.constData(), tileSize, metadata, storageMode, crypto,
                                     reinterpret_cast<mtpl_vtp_writer_t **>( writer ) );
    default:
      return MTPL_STATUS_FORMAT_MISMATCH;
  }
}

mtpl_status_t tileWriterAddRange( QgsMtpl::PackageFormat format,
                                  void *writer,
                                  const mtpl_tile_range_t *range )
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

mtpl_status_t tileWriterAddData( QgsMtpl::PackageFormat format,
                                 void *writer,
                                 const mtpl_tile_coordinate_t *coordinate,
                                 mtpl_buffer_view_t data )
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

struct TileFixtureData
{
  uint32_t tileSize = 256;
  QByteArray metadata;
  QByteArray payload;
};

bool tileFixtureData( QgsMtpl::PackageFormat format, TileFixtureData &fixture, QString &error )
{
  switch ( format )
  {
    case QgsMtpl::PackageFormat::Ptp:
      fixture.tileSize = 256;
      fixture.metadata = QByteArrayLiteral( "{\"tile_file_ext\":\"png\",\"source\":\"qgis-mtpl-test\"}" );
      fixture.payload = QByteArray::fromBase64( QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=" ) );
      break;
    case QgsMtpl::PackageFormat::Dtp:
      fixture.tileSize = 33;
      fixture.metadata = QByteArrayLiteral( "{\"data_type\":\"uint16\",\"endianness\":\"little\",\"source\":\"qgis-mtpl-test\"}" );
      fixture.payload = QByteArray( 33 * 33 * 2, '\0' );
      break;
    case QgsMtpl::PackageFormat::Vtp:
      fixture.tileSize = 256;
      fixture.metadata = QByteArrayLiteral( "{\"tile_file_ext\":\"pbf\",\"source\":\"qgis-mtpl-test\"}" );
      fixture.payload = QgsMtplTest::minimalVectorTilePayload();
      break;
    default:
      error = QStringLiteral( "Tile fixtures support only PTP, DTP, and VTP formats." );
      return false;
  }

  error.clear();
  return true;
}

bool prepareNewDirectory( const QString &root, QString &absoluteRoot, QString &error )
{
  if ( root.trimmed().isEmpty() )
  {
    error = QStringLiteral( "The fixture source directory path is empty." );
    return false;
  }

  const QFileInfo rootInfo( root );
  if ( rootInfo.exists() )
  {
    error = QStringLiteral( "The fixture source directory already exists." );
    return false;
  }

  absoluteRoot = rootInfo.absoluteFilePath();
  if ( !QDir().mkpath( absoluteRoot ) )
  {
    absoluteRoot.clear();
    error = QStringLiteral( "Unable to create the fixture source directory." );
    return false;
  }

  error.clear();
  return true;
}

bool writeNewFile( const QString &path, const QByteArray &data, QString &error )
{
  QFile file( path );
  if ( !file.open( QIODevice::WriteOnly | QIODevice::NewOnly ) )
  {
    error = QStringLiteral( "Unable to create a fixture source file." );
    return false;
  }

  if ( !data.isEmpty() && file.write( data ) != data.size() )
  {
    error = QStringLiteral( "Unable to write a complete fixture source file." );
    file.close();
    return false;
  }

  file.close();
  error.clear();
  return true;
}

} // namespace

QgsMtpl::CryptoKeys QgsMtplTest::fixtureKeys()
{
  QgsMtpl::CryptoKeys keys;
  keys.privateKey = QByteArray( 32, 'p' ).toBase64();
  keys.deviceKey = QByteArrayLiteral( "00112233445566778899aabbccddeeff" );
  return keys;
}

QgsMtpl::CryptoKeys QgsMtplTest::differentFixtureKeys()
{
  QgsMtpl::CryptoKeys keys;
  keys.privateKey = QByteArray( 32, 'q' ).toBase64();
  keys.deviceKey = QByteArrayLiteral( "ffeeddccbbaa99887766554433221100" );
  return keys;
}

QByteArray QgsMtplTest::minimalVectorTilePayload()
{
  return QByteArray::fromHex( "1a0b0a07666978747572657802" );
}

bool QgsMtplTest::writeTileFixture( const QString &path,
                                    QgsMtpl::PackageFormat format,
                                    mtpl_storage_mode_t storageMode,
                                    const QgsMtpl::CryptoKeys &keys,
                                    QString &error )
{
  if ( !isStorageModeSupported( storageMode ) )
  {
    error = QStringLiteral( "The tile fixture storage mode is not supported." );
    return false;
  }

  if ( storageMode == MTPL_STORAGE_ENCRYPTED && !keys.isValid( &error ) )
    return false;

  TileFixtureData fixture;
  if ( !tileFixtureData( format, fixture, error ) || !prepareOutputPath( path, error ) )
    return false;

  const CryptoContext crypto( keys );
  const QByteArray pathUtf8 = QFileInfo( path ).absoluteFilePath().toUtf8();
  const mtpl_buffer_view_t metadataView = {
    reinterpret_cast<const uint8_t *>( fixture.metadata.constData() ),
    static_cast<size_t>( fixture.metadata.size() )
  };
  const mtpl_buffer_view_t payloadView = {
    reinterpret_cast<const uint8_t *>( fixture.payload.constData() ),
    static_cast<size_t>( fixture.payload.size() )
  };
  const mtpl_tile_range_t range = { 0, 0, 0, 0, 0 };
  const mtpl_tile_coordinate_t coordinate = { 0, 0, 0 };

  void *writer = nullptr;
  mtpl_status_t status = tileWriterCreate(
    format, pathUtf8, fixture.tileSize, metadataView, storageMode,
    storageMode == MTPL_STORAGE_ENCRYPTED ? crypto.get() : nullptr, &writer );
  if ( status == MTPL_STATUS_OK && !writer )
    status = MTPL_STATUS_INTERNAL_ERROR;
  if ( status == MTPL_STATUS_OK )
    status = tileWriterAddRange( format, writer, &range );
  if ( status == MTPL_STATUS_OK )
    status = tileWriterAddData( format, writer, &coordinate, payloadView );

  if ( writer )
  {
    const mtpl_status_t closeStatus = tileWriterClose( format, writer );
    writer = nullptr;
    if ( status == MTPL_STATUS_OK )
      status = closeStatus;
  }

  if ( status != MTPL_STATUS_OK )
  {
    error = statusError( QStringLiteral( "Unable to write the tile fixture" ), status );
    removePartialOutput( path, error );
    return false;
  }

  error.clear();
  return true;
}

bool QgsMtplTest::writeSfpFixture( const QString &path,
                                   const QList<SfpFixtureEntry> &entries,
                                   const QgsMtpl::CryptoKeys &keys,
                                   QString &error )
{
  bool requiresKeys = false;
  for ( const SfpFixtureEntry &entry : entries )
  {
    if ( !isStorageModeSupported( entry.storageMode ) )
    {
      error = QStringLiteral( "An SFP fixture entry has an unsupported storage mode." );
      return false;
    }
    requiresKeys = requiresKeys || entry.storageMode == MTPL_STORAGE_ENCRYPTED;
  }

  if ( requiresKeys && !keys.isValid( &error ) )
    return false;
  if ( !prepareOutputPath( path, error ) )
    return false;

  const CryptoContext crypto( keys );
  mtpl_sfp_builder_t *builder = nullptr;
  mtpl_status_t status = mtpl_sfp_builder_create( requiresKeys ? crypto.get() : nullptr, &builder );
  if ( status == MTPL_STATUS_OK && !builder )
    status = MTPL_STATUS_INTERNAL_ERROR;

  for ( const SfpFixtureEntry &entry : entries )
  {
    if ( status != MTPL_STATUS_OK )
      break;

    const QByteArray entryPathUtf8 = entry.path.toUtf8();
    const mtpl_buffer_view_t dataView = {
      entry.data.isEmpty() ? nullptr : reinterpret_cast<const uint8_t *>( entry.data.constData() ),
      static_cast<size_t>( entry.data.size() )
    };
    status = mtpl_sfp_builder_add_data(
      builder, entryPathUtf8.constData(), dataView, entry.storageMode );
  }

  if ( status == MTPL_STATUS_OK )
  {
    const QByteArray pathUtf8 = QFileInfo( path ).absoluteFilePath().toUtf8();
    status = mtpl_sfp_builder_write( builder, pathUtf8.constData() );
  }

  if ( builder )
  {
    mtpl_sfp_builder_destroy( builder );
    builder = nullptr;
  }

  if ( status != MTPL_STATUS_OK )
  {
    error = statusError( QStringLiteral( "Unable to write the SFP fixture" ), status );
    removePartialOutput( path, error );
    return false;
  }

  error.clear();
  return true;
}

bool QgsMtplTest::createTileSourceTree( const QString &root, QString &error )
{
  QString absoluteRoot;
  if ( !prepareNewDirectory( root, absoluteRoot, error ) )
    return false;

  const QString tileDirectory = QDir( absoluteRoot ).filePath( QStringLiteral( "0/0" ) );
  if ( !QDir().mkpath( tileDirectory ) ||
       !writeNewFile( QDir( tileDirectory ).filePath( QStringLiteral( "0.bin" ) ),
                      minimalVectorTilePayload(), error ) )
  {
    if ( error.isEmpty() )
      error = QStringLiteral( "Unable to create the tile fixture directory tree." );
    if ( !QDir( absoluteRoot ).removeRecursively() )
      error += QStringLiteral( " Unable to remove the partial fixture source directory." );
    return false;
  }

  error.clear();
  return true;
}

bool QgsMtplTest::createSfpSourceDirectory( const QString &root, QString &error )
{
  QString absoluteRoot;
  if ( !prepareNewDirectory( root, absoluteRoot, error ) )
    return false;

  const QString textPath = QDir( absoluteRoot ).filePath( QStringLiteral( "地图-Delta.txt" ) );
  const QString nestedPath = QDir( absoluteRoot ).filePath( QStringLiteral( "nested" ) );
  const QString emptyPath = QDir( nestedPath ).filePath( QStringLiteral( "empty.dat" ) );
  if ( !QDir().mkpath( nestedPath ) ||
       !writeNewFile( textPath, QByteArrayLiteral( "spatial package payload" ), error ) ||
       !writeNewFile( emptyPath, QByteArray(), error ) )
  {
    if ( error.isEmpty() )
      error = QStringLiteral( "Unable to create the nested SFP fixture directory." );
    if ( !QDir( absoluteRoot ).removeRecursively() )
      error += QStringLiteral( " Unable to remove the partial fixture source directory." );
    return false;
  }

  error.clear();
  return true;
}
