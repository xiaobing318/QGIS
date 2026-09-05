/***************************************************************************
  test_mtpl_layer.cpp
  -------------------
  Contract tests for logical MTPL dataset layers and raster rendering.
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

#include "qgsmtplpluginlayer.h"
#include "qgsmtplrasterinterface.h"
#include "services/qgsmtplcredentialstore.h"
#include "services/qgsmtplpackageservice.h"
#include "qgsapplication.h"
#include "qgscoordinatetransform.h"
#include "qgsmaplayerrenderer.h"
#include "qgsmaprendererjob.h"
#include "qgsmaprenderersequentialjob.h"
#include "qgsmapsettings.h"
#include "qgspathresolver.h"
#include "qgsproject.h"
#include "qgsrasterblock.h"
#include "qgsrasterdataprovider.h"
#include "qgsrasterprojector.h"
#include "qgsreadwritecontext.h"
#include "qgsrendercontext.h"
#include "qgstest.h"

#include <QApplication>
#include <QBuffer>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QDialog>
#include <QDomDocument>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <future>
#include <limits>
#include <memory>
#include <vector>

namespace
{
  class ScopedMemoryCredentialBackend final : public QgsMtplCredentialBackend
  {
    public:
      ScopedMemoryCredentialBackend()
      {
        QgsMtplCredentialStore::setBackendForTesting( this );
      }

      ~ScopedMemoryCredentialBackend() override
      {
        QgsMtplCredentialStore::clear();
        QgsMtplCredentialStore::setBackendForTesting( nullptr );
      }

      bool contains( const QString &credentialId ) const override
      {
        return mCredentials.contains( credentialId );
      }

      LoadResult load( const QString &credentialId, bool allowUnlock ) override
      {
        if ( allowUnlock )
          ++unlockAllowedLoadCount;
        else
          ++silentLoadCount;
        LoadResult result;
        if ( !allowUnlock && !mUnlocked )
        {
          result.status = LoadStatus::UnlockFailed;
          return result;
        }
        if ( !mCredentials.contains( credentialId ) )
          return result;
        result.status = LoadStatus::Success;
        result.keys = mCredentials.value( credentialId );
        return result;
      }

      bool store( const QgsMtpl::CryptoKeys &keys, QString &credentialId, QString &error ) override
      {
        credentialId = QStringLiteral( "fake001" );
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

      void setUnlocked( bool unlocked ) { mUnlocked = unlocked; }
      void resetLoadCounts()
      {
        unlockAllowedLoadCount = 0;
        silentLoadCount = 0;
      }

      int unlockAllowedLoadCount = 0;
      int silentLoadCount = 0;

    private:
      QHash<QString, QgsMtpl::CryptoKeys> mCredentials;
      bool mUnlocked = true;
  };

  constexpr int sTileSize = 33;

  class RecordingMtplRasterFilter final : public QgsRasterInterface
  {
    public:
      RecordingMtplRasterFilter( QgsRasterInterface *input, const QgsRectangle &reportedExtent )
        : QgsRasterInterface( input )
        , mReportedExtent( reportedExtent )
      {
      }

      RecordingMtplRasterFilter *clone() const override
      {
        return new RecordingMtplRasterFilter( input(), mReportedExtent );
      }

      int bandCount() const override { return input()->bandCount(); }
      Qgis::DataType dataType( int bandNo ) const override { return input()->dataType( bandNo ); }
      QgsRectangle extent() const override { return mReportedExtent; }

      QgsRasterBlock *block( int bandNo, const QgsRectangle &extent, int width, int height,
                             QgsRasterBlockFeedback *feedback = nullptr ) override
      {
        requestedExtents.append( extent );
        requestedSizes.append( QSize( width, height ) );
        forwardedFeedback.append( feedback );
        if ( cancelBeforeForward && feedback )
          feedback->cancel();
        return input()->block( bandNo, extent, width, height, feedback );
      }

      QList<QgsRectangle> requestedExtents;
      QList<QSize> requestedSizes;
      QList<QgsRasterBlockFeedback *> forwardedFeedback;
      bool cancelBeforeForward = false;

    private:
      QgsRectangle mReportedExtent;
  };

  class ScopedPluginSettingsRestore
  {
    public:
      explicit ScopedPluginSettingsRestore( const QStringList &keys )
      {
        mSnapshot.capture( keys );
      }

      ~ScopedPluginSettingsRestore()
      {
        mSnapshot.restore();
      }

      ScopedPluginSettingsRestore( const ScopedPluginSettingsRestore & ) = delete;
      ScopedPluginSettingsRestore &operator=( const ScopedPluginSettingsRestore & ) = delete;

    private:
      QgsMtplTest::PluginSettingsSnapshot mSnapshot;
  };

  QByteArray solidPng( const QColor &color )
  {
    QImage image( sTileSize, sTileSize, QImage::Format_ARGB32 );
    image.fill( color );
    QByteArray bytes;
    QBuffer buffer( &bytes );
    if ( !buffer.open( QIODevice::WriteOnly ) || !image.save( &buffer, "PNG" ) )
      return QByteArray();
    return bytes;
  }

  QByteArray tileMetadata( QgsMtpl::TileScheme scheme, int maximumZoom, quint64 rootWidth )
  {
    return QStringLiteral(
      R"({"map_id":"logical-test-map","tile_file_ext":"png","privateKeyBase64":"package-private-secret","nested":{"password":"package-nested-secret","label":"nested-visible"},"mtpl_tile_matrix":{"version":1,"crs":"EPSG:4326","scheme":"%1","top_left":[-180.0,90.0],"z0_tile_span":180.0,"z0_matrix_width":%2,"z0_matrix_height":1,"scale_to_zoom_method":"mapbox","minimum_zoom":0,"maximum_zoom":%3,"tile_size":%4,"bounds":[-180.0,-90.0,%5,90.0],"bounds_crs":"EPSG:4326"}})" )
      .arg( scheme == QgsMtpl::TileScheme::Tms ? QStringLiteral( "tms" ) : QStringLiteral( "xyz" ) )
      .arg( rootWidth )
      .arg( maximumZoom )
      .arg( sTileSize )
      .arg( rootWidth == 2 ? QStringLiteral( "180.0" ) : QStringLiteral( "0.0" ) )
      .toUtf8();
  }

  QgsMtpl::TileRangeRecord rangeRecord( int zoom,
                                        quint32 xMinimum,
                                        quint32 xMaximum,
                                        quint32 yMinimum,
                                        quint32 yMaximum )
  {
    const quint64 count = ( static_cast<quint64>( xMaximum ) - xMinimum + 1ULL ) *
                          ( static_cast<quint64>( yMaximum ) - yMinimum + 1ULL );
    return QgsMtpl::TileRangeRecord {
      zoom,
      xMinimum,
      xMaximum,
      yMinimum,
      yMaximum,
      xMinimum,
      xMaximum,
      yMinimum,
      yMaximum,
      count
    };
  }

  QgsMtpl::TileDatasetPackage datasetPackage( const QString &path,
                                              const QgsMtpl::TilePackageAddress &address,
                                              const QList<QgsMtpl::TileRangeRecord> &ranges,
                                              int minimumZoom,
                                              int maximumZoom,
                                              QgsMtpl::TileScheme scheme )
  {
    const QFileInfo fileInfo( path );
    QgsMtpl::TileDatasetPackage package;
    package.descriptor.path = fileInfo.absoluteFilePath();
    package.descriptor.displayName = fileInfo.completeBaseName();
    package.descriptor.format = QgsMtpl::PackageFormat::Ptp;
    package.descriptor.payload = QgsMtpl::PayloadType::RasterImage;
    package.descriptor.encryption = QgsMtpl::EncryptionState::Plain;
    package.descriptor.readiness = QgsMtpl::ReadinessState::PlainReady;
    package.descriptor.crsAuthId = QStringLiteral( "EPSG:4326" );
    package.descriptor.scheme = scheme == QgsMtpl::TileScheme::Tms ? QStringLiteral( "tms" ) : QStringLiteral( "xyz" );
    package.descriptor.fileSize = static_cast<quint64>( fileInfo.size() );
    package.descriptor.fileLastModifiedMs = fileInfo.lastModified().toMSecsSinceEpoch();
    package.descriptor.tileSize = sTileSize;
    package.descriptor.minimumZoom = minimumZoom;
    package.descriptor.maximumZoom = maximumZoom;
    package.address = address;
    package.relativePath = fileInfo.fileName();
    package.fileLastModifiedMs = package.descriptor.fileLastModifiedMs;
    package.ranges = ranges;
    return package;
  }

  QgsMtpl::TileMatrixDefinition matrixDefinition( QgsMtpl::TileScheme scheme,
                                                  int maximumZoom,
                                                  quint64 rootWidth )
  {
    QgsMtpl::TileMatrixDefinition matrix;
    matrix.crsAuthId = QStringLiteral( "EPSG:4326" );
    matrix.scheme = scheme;
    matrix.tileSize = sTileSize;
    matrix.topLeftX = -180.0;
    matrix.topLeftY = 90.0;
    matrix.z0TileSpan = 180.0;
    matrix.z0MatrixWidth = rootWidth;
    matrix.z0MatrixHeight = 1;
    matrix.minimumZoom = 0;
    matrix.maximumZoom = maximumZoom;
    matrix.zoomRangeExplicit = true;
    matrix.hasBounds = true;
    matrix.boundsXMinimum = -180.0;
    matrix.boundsYMinimum = -90.0;
    matrix.boundsXMaximum = rootWidth == 2 ? 180.0 : 0.0;
    matrix.boundsYMaximum = 90.0;
    matrix.boundsCrsAuthId = QStringLiteral( "EPSG:4326" );
    return matrix;
  }

  QByteArray customTileMetadata( const QString &crsAuthId,
                                 QgsMtpl::TileScheme scheme,
                                 int maximumZoom,
                                 double topLeftX,
                                 double topLeftY,
                                 double z0TileSpan,
                                 quint64 rootWidth,
                                 quint64 rootHeight )
  {
    const double xMaximum = topLeftX + z0TileSpan * static_cast<double>( rootWidth );
    const double yMinimum = topLeftY - z0TileSpan * static_cast<double>( rootHeight );
    return QStringLiteral(
      R"({"map_id":"logical-test-map","tile_file_ext":"png","mtpl_tile_matrix":{"version":1,"crs":"%1","scheme":"%2","top_left":[%3,%4],"z0_tile_span":%5,"z0_matrix_width":%6,"z0_matrix_height":%7,"scale_to_zoom_method":"mapbox","minimum_zoom":0,"maximum_zoom":%8,"tile_size":%9,"bounds":[%3,%10,%11,%4],"bounds_crs":"%1"}})" )
      .arg( crsAuthId )
      .arg( scheme == QgsMtpl::TileScheme::Tms ? QStringLiteral( "tms" ) : QStringLiteral( "xyz" ) )
      .arg( QString::number( topLeftX, 'g', 17 ) )
      .arg( QString::number( topLeftY, 'g', 17 ) )
      .arg( QString::number( z0TileSpan, 'g', 17 ) )
      .arg( QString::number( rootWidth ) )
      .arg( QString::number( rootHeight ) )
      .arg( QString::number( maximumZoom ) )
      .arg( QString::number( sTileSize ) )
      .arg( QString::number( yMinimum, 'g', 17 ) )
      .arg( QString::number( xMaximum, 'g', 17 ) )
      .toUtf8();
  }

  QgsMtpl::TileMatrixDefinition customMatrixDefinition( const QString &crsAuthId,
                                                        QgsMtpl::TileScheme scheme,
                                                        int maximumZoom,
                                                        double topLeftX,
                                                        double topLeftY,
                                                        double z0TileSpan,
                                                        quint64 rootWidth,
                                                        quint64 rootHeight )
  {
    QgsMtpl::TileMatrixDefinition matrix;
    matrix.crsAuthId = crsAuthId;
    matrix.scheme = scheme;
    matrix.tileSize = sTileSize;
    matrix.topLeftX = topLeftX;
    matrix.topLeftY = topLeftY;
    matrix.z0TileSpan = z0TileSpan;
    matrix.z0MatrixWidth = rootWidth;
    matrix.z0MatrixHeight = rootHeight;
    matrix.minimumZoom = 0;
    matrix.maximumZoom = maximumZoom;
    matrix.zoomRangeExplicit = true;
    matrix.hasBounds = true;
    matrix.boundsXMinimum = topLeftX;
    matrix.boundsYMinimum = topLeftY - z0TileSpan * static_cast<double>( rootHeight );
    matrix.boundsXMaximum = topLeftX + z0TileSpan * static_cast<double>( rootWidth );
    matrix.boundsYMaximum = topLeftY;
    matrix.boundsCrsAuthId = crsAuthId;
    return matrix;
  }

  QgsMtpl::TileDatasetPackage customDatasetPackage( const QString &path,
                                                    const QgsMtpl::TilePackageAddress &address,
                                                    const QList<QgsMtpl::TileRangeRecord> &ranges,
                                                    int minimumZoom,
                                                    int maximumZoom,
                                                    const QgsMtpl::TileMatrixDefinition &matrix,
                                                    bool encrypted = false )
  {
    QgsMtpl::TileDatasetPackage package = datasetPackage(
      path, address, ranges, minimumZoom, maximumZoom, matrix.scheme );
    package.descriptor.crsAuthId = matrix.crsAuthId;
    package.descriptor.scheme = matrix.scheme == QgsMtpl::TileScheme::Tms
                                  ? QStringLiteral( "tms" )
                                  : QStringLiteral( "xyz" );
    package.descriptor.encryption = encrypted
                                      ? QgsMtpl::EncryptionState::Encrypted
                                      : QgsMtpl::EncryptionState::Plain;
    package.descriptor.readiness = encrypted
                                     ? QgsMtpl::ReadinessState::KeyVerified
                                     : QgsMtpl::ReadinessState::PlainReady;
    package.descriptor.credentialSource = encrypted
                                           ? QgsMtpl::CredentialSource::Explicit
                                           : QgsMtpl::CredentialSource::None;
    return package;
  }

  QgsMtpl::PartitionRule partitionRule( int maximumZoom,
                                        const QString &id = QStringLiteral( "test.dataset.rule" ),
                                        const QString &name = QStringLiteral( "测试数据集规则" ) )
  {
    QgsMtpl::PartitionRule rule;
    rule.id = id;
    rule.name = name;
    rule.bands = { { 0, maximumZoom, 0 } };
    return rule;
  }

  QgsMtpl::TileDatasetDescriptor datasetDescriptor( const QString &sourcePath,
                                                    QgsMtpl::TileScheme scheme,
                                                    int minimumZoom,
                                                    int maximumZoom,
                                                    quint64 rootWidth,
                                                    const QList<QgsMtpl::TileDatasetPackage> &packages )
  {
    QgsMtpl::TileDatasetDescriptor dataset;
    dataset.sourcePath = QDir( sourcePath ).absolutePath();
    dataset.directorySource = true;
    dataset.partitionRule = partitionRule( maximumZoom );
    dataset.matrix = matrixDefinition( scheme, maximumZoom, rootWidth );
    dataset.payload = QgsMtpl::PayloadType::RasterImage;
    dataset.mapId = QStringLiteral( "logical-test-map" );
    dataset.packages = packages;
    for ( int index = 0; index < dataset.packages.size(); ++index )
      dataset.packageIndexByAddress.insert( dataset.packages.at( index ).address.key(), index );
    dataset.minimumZoom = minimumZoom;
    dataset.maximumZoom = maximumZoom;
    return dataset;
  }

  bool hasColorNear( const QImage &image,
                     const QgsPointXY &devicePoint,
                     const QColor &expected,
                     int radius = 6,
                     int tolerance = 35 )
  {
    const int centerX = qRound( devicePoint.x() );
    const int centerY = qRound( devicePoint.y() );
    for ( int y = std::max( 0, centerY - radius ); y <= std::min( image.height() - 1, centerY + radius ); ++y )
    {
      for ( int x = std::max( 0, centerX - radius ); x <= std::min( image.width() - 1, centerX + radius ); ++x )
      {
        const QColor actual = image.pixelColor( x, y );
        if ( actual.alpha() >= 200 &&
             std::abs( actual.red() - expected.red() ) <= tolerance &&
             std::abs( actual.green() - expected.green() ) <= tolerance &&
             std::abs( actual.blue() - expected.blue() ) <= tolerance )
          return true;
      }
    }
    return false;
  }

  int opaquePixelCount( const QImage &image )
  {
    int count = 0;
    for ( int y = 0; y < image.height(); ++y )
    {
      const QRgb *line = reinterpret_cast<const QRgb *>( image.constScanLine( y ) );
      for ( int x = 0; x < image.width(); ++x )
      {
        if ( qAlpha( line[x] ) > 0 )
          ++count;
      }
    }
    return count;
  }

  bool createXyzDataset( const QString &root,
                         QgsMtpl::TileDatasetDescriptor &dataset,
                         QString &error )
  {
    const QString leftPath = QDir( root ).filePath( QStringLiteral( "0-0-0-0-0.ptp" ) );
    const QString rightPath = QDir( root ).filePath( QStringLiteral( "0-0-0-1-0.ptp" ) );
    const QByteArray leftTile = solidPng( QColor( 220, 30, 40 ) );
    const QByteArray rightTile = solidPng( QColor( 20, 70, 230 ) );
    if ( leftTile.isEmpty() || rightTile.isEmpty() )
    {
      error = QStringLiteral( "Unable to encode test PNG tiles." );
      return false;
    }

    if ( !QgsMtplTest::writePtpFixture(
           leftPath,
           sTileSize,
           tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 ),
           MTPL_STORAGE_PLAIN,
           QgsMtpl::CryptoKeys(),
           { { 0, 0, 0, 0, 0 } },
           { { 0, 0, 0, leftTile } },
           error ) ||
         !QgsMtplTest::writePtpFixture(
           rightPath,
           sTileSize,
           tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 ),
           MTPL_STORAGE_PLAIN,
           QgsMtpl::CryptoKeys(),
           { { 0, 1, 1, 0, 0 } },
           { { 0, 1, 0, rightTile } },
           error ) )
    {
      return false;
    }

    const QgsMtpl::TilePackageAddress leftAddress { 0, 0, 0, 0, 0 };
    const QgsMtpl::TilePackageAddress rightAddress { 0, 0, 0, 1, 0 };
    dataset = datasetDescriptor(
      root,
      QgsMtpl::TileScheme::Xyz,
      0,
      0,
      2,
      {
        datasetPackage( leftPath, leftAddress, { rangeRecord( 0, 0, 0, 0, 0 ) }, 0, 0, QgsMtpl::TileScheme::Xyz ),
        datasetPackage( rightPath, rightAddress, { rangeRecord( 0, 1, 1, 0, 0 ) }, 0, 0, QgsMtpl::TileScheme::Xyz ),
      } );
    error.clear();
    return true;
  }

  bool createTmsDataset( const QString &root,
                         QgsMtpl::TileDatasetDescriptor &dataset,
                         QString &error )
  {
    const QString path = QDir( root ).filePath( QStringLiteral( "1-1-0-0-0.ptp" ) );
    const QByteArray storedTopTile = solidPng( QColor( 30, 190, 70 ) );
    const QByteArray storedBottomTile = solidPng( QColor( 240, 180, 20 ) );
    if ( storedTopTile.isEmpty() || storedBottomTile.isEmpty() )
    {
      error = QStringLiteral( "Unable to encode test PNG tiles." );
      return false;
    }

    if ( !QgsMtplTest::writePtpFixture(
           path,
           sTileSize,
           tileMetadata( QgsMtpl::TileScheme::Tms, 1, 1 ),
           MTPL_STORAGE_PLAIN,
           QgsMtpl::CryptoKeys(),
           { { 1, 0, 0, 0, 1 } },
           {
             { 1, 0, 1, storedTopTile },
             { 1, 0, 0, storedBottomTile },
           },
           error ) )
    {
      return false;
    }

    const QgsMtpl::TilePackageAddress address { 0, 1, 0, 0, 0 };
    dataset = datasetDescriptor(
      root,
      QgsMtpl::TileScheme::Tms,
      0,
      1,
      1,
      { datasetPackage( path, address, { rangeRecord( 1, 0, 0, 0, 1 ) }, 1, 1, QgsMtpl::TileScheme::Tms ) } );
    error.clear();
    return true;
  }

  bool createWebMercatorPixelDataset( const QString &root,
                                      QgsMtpl::TileDatasetDescriptor &dataset,
                                      QString &error )
  {
    constexpr double halfWorld = 20037508.342789244;
    const QgsMtpl::TileMatrixDefinition matrix = customMatrixDefinition(
      QStringLiteral( "EPSG:3857" ), QgsMtpl::TileScheme::Xyz, 1,
      -halfWorld, halfWorld, 2.0 * halfWorld, 1, 1 );
    const QString path = QDir( root ).filePath( QStringLiteral( "0-1-0-0-0.ptp" ) );
    const QList<QgsMtplTest::TileFixtureEntry> tiles {
      { 1, 0, 0, solidPng( QColor( 220, 30, 40 ) ) },
      { 1, 1, 0, solidPng( QColor( 20, 70, 230 ) ) },
      { 1, 0, 1, solidPng( QColor( 30, 190, 70 ) ) },
      { 1, 1, 1, solidPng( QColor( 240, 180, 20 ) ) },
    };
    if ( std::any_of( tiles.cbegin(), tiles.cend(), []( const QgsMtplTest::TileFixtureEntry &tile ) { return tile.data.isEmpty(); } ) )
    {
      error = QStringLiteral( "Unable to encode Web Mercator test PNG tiles." );
      return false;
    }

    if ( !QgsMtplTest::writePtpFixture(
           path,
           sTileSize,
           customTileMetadata( matrix.crsAuthId, matrix.scheme, 1, matrix.topLeftX, matrix.topLeftY,
                               matrix.z0TileSpan, matrix.z0MatrixWidth, matrix.z0MatrixHeight ),
           MTPL_STORAGE_PLAIN,
           QgsMtpl::CryptoKeys(),
           { { 1, 0, 1, 0, 1 } },
           tiles,
           error ) )
    {
      return false;
    }

    const QgsMtpl::TilePackageAddress address { 1, 1, 0, 0, 0 };
    dataset = datasetDescriptor(
      root,
      matrix.scheme,
      1,
      1,
      matrix.z0MatrixWidth,
      { customDatasetPackage( path, address, { rangeRecord( 1, 0, 1, 0, 1 ) }, 1, 1, matrix ) } );
    dataset.matrix = matrix;
    dataset.partitionRule.id = QStringLiteral( "test.web-mercator.rule" );
    dataset.partitionRule.name = QStringLiteral( "Web Mercator 像素规则" );
    dataset.partitionRule.bands = { { 1, 1, 0 } };
    error.clear();
    return true;
  }

  bool createPartialProviderDataset( const QString &root,
                                      QgsMtpl::TileDatasetDescriptor &dataset,
                                      QImage &referenceImage,
                                      QString &error )
  {
    const QgsMtpl::TileMatrixDefinition matrix = customMatrixDefinition(
      QStringLiteral( "EPSG:3857" ), QgsMtpl::TileScheme::Xyz, 0,
      -2000.0, 1000.0, 2000.0, 2, 1 );
    const QByteArray tile = QgsMtplTest::rasterTileImage( QByteArrayLiteral( "PNG" ), sTileSize, sTileSize, 7, error );
    if ( tile.isEmpty() )
      return false;
    referenceImage = QImage::fromData( tile );
    const QString path = QDir( root ).filePath( QStringLiteral( "0-0-0-0-0.ptp" ) );
    if ( !QgsMtplTest::writePtpFixture(
           path, sTileSize,
           customTileMetadata( matrix.crsAuthId, matrix.scheme, 0, matrix.topLeftX, matrix.topLeftY,
                               matrix.z0TileSpan, matrix.z0MatrixWidth, matrix.z0MatrixHeight ),
           MTPL_STORAGE_PLAIN, QgsMtpl::CryptoKeys(),
           { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, tile } }, error ) )
      return false;

    dataset = datasetDescriptor(
      root, matrix.scheme, 0, 0, matrix.z0MatrixWidth,
      { customDatasetPackage( path, { 0, 0, 0, 0, 0 }, { rangeRecord( 0, 0, 0, 0, 0 ) }, 0, 0, matrix ) } );
    dataset.matrix = matrix;
    dataset.hasExtent = true;
    dataset.extentXMinimum = -2000.0;
    dataset.extentYMinimum = -1000.0;
    dataset.extentXMaximum = 0.0;
    dataset.extentYMaximum = 1000.0;
    return dataset.isValid( &error );
  }

  bool createPolarTmsPixelDataset( const QString &root,
                                   QgsMtpl::TileDatasetDescriptor &dataset,
                                   QString &error )
  {
    const QgsMtpl::TileMatrixDefinition matrix = customMatrixDefinition(
      QStringLiteral( "EPSG:3413" ), QgsMtpl::TileScheme::Tms, 0,
      -4000000.0, 4000000.0, 4000000.0, 2, 2 );
    const QColor colors[2][2] = {
      { QColor( 30, 190, 70 ), QColor( 240, 180, 20 ) },
      { QColor( 190, 35, 180 ), QColor( 20, 190, 210 ) },
    };
    QList<QgsMtpl::TileDatasetPackage> packages;
    for ( quint32 xyzY = 0; xyzY < 2; ++xyzY )
    {
      for ( quint32 x = 0; x < 2; ++x )
      {
        const quint32 storedY = 1U - xyzY;
        const QgsMtpl::TilePackageAddress address { 0, 0, 0, x, storedY };
        const QString path = QDir( root ).filePath( address.fileName() );
        const QByteArray image = solidPng( colors[xyzY][x] );
        if ( image.isEmpty() ||
             !QgsMtplTest::writePtpFixture(
               path,
               sTileSize,
               customTileMetadata( matrix.crsAuthId, matrix.scheme, 0, matrix.topLeftX, matrix.topLeftY,
                                   matrix.z0TileSpan, matrix.z0MatrixWidth, matrix.z0MatrixHeight ),
               MTPL_STORAGE_PLAIN,
               QgsMtpl::CryptoKeys(),
               { { 0, x, x, storedY, storedY } },
               { { 0, x, storedY, image } },
               error ) )
        {
          if ( error.isEmpty() )
            error = QStringLiteral( "Unable to encode polar test PNG tiles." );
          return false;
        }
        packages.append( customDatasetPackage(
          path, address, { rangeRecord( 0, x, x, storedY, storedY ) }, 0, 0, matrix ) );
      }
    }

    dataset = datasetDescriptor( root, matrix.scheme, 0, 0, matrix.z0MatrixWidth, packages );
    dataset.matrix = matrix;
    dataset.partitionRule = partitionRule( 0, QStringLiteral( "test.polar-tms.rule" ), QStringLiteral( "极区 TMS 像素规则" ) );
    error.clear();
    return true;
  }

  bool createEncryptedAggregateDataset( const QString &root,
                                        const QgsMtpl::CryptoKeys &keys,
                                        QgsMtpl::TileDatasetDescriptor &dataset,
                                        QString &error )
  {
    const QgsMtpl::TileMatrixDefinition matrix = matrixDefinition( QgsMtpl::TileScheme::Xyz, 0, 2 );
    const QString leftPath = QDir( root ).filePath( QStringLiteral( "0-0-0-0-0.ptp" ) );
    const QString rightPath = QDir( root ).filePath( QStringLiteral( "0-0-0-1-0.ptp" ) );
    const QByteArray leftTile = solidPng( QColor( 220, 30, 40 ) );
    const QByteArray rightTile = solidPng( QColor( 20, 70, 230 ) );
    if ( leftTile.isEmpty() || rightTile.isEmpty() )
    {
      error = QStringLiteral( "Unable to encode encrypted test PNG tiles." );
      return false;
    }
    if ( !QgsMtplTest::writePtpFixture(
           leftPath, sTileSize, tileMetadata( matrix.scheme, 0, 2 ), MTPL_STORAGE_ENCRYPTED, keys,
           { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, leftTile } }, error ) ||
         !QgsMtplTest::writePtpFixture(
           rightPath, sTileSize, tileMetadata( matrix.scheme, 0, 2 ), MTPL_STORAGE_ENCRYPTED, keys,
           { { 0, 1, 1, 0, 0 } }, { { 0, 1, 0, rightTile } }, error ) )
    {
      return false;
    }

    const QgsMtpl::TilePackageAddress leftAddress { 0, 0, 0, 0, 0 };
    const QgsMtpl::TilePackageAddress rightAddress { 0, 0, 0, 1, 0 };
    dataset = datasetDescriptor(
      root,
      matrix.scheme,
      0,
      0,
      2,
      {
        customDatasetPackage( leftPath, leftAddress, { rangeRecord( 0, 0, 0, 0, 0 ) }, 0, 0, matrix, true ),
        customDatasetPackage( rightPath, rightAddress, { rangeRecord( 0, 1, 1, 0, 0 ) }, 0, 0, matrix, true ),
      } );
    dataset.partitionRule = partitionRule( 0, QStringLiteral( "test.encrypted.rule" ), QStringLiteral( "合成加密规则" ) );
    error.clear();
    return true;
  }
}

class TestMtplLayer : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void datasetCreatesSingleLayer();
    void datasetReplacementIsAtomic();
    void rasterInterfaceXyzTwoByOne();
    void rasterInterfaceTmsRowFlip();
    void rasterProviderProjectorClipsPartialCoverage_data();
    void rasterProviderProjectorClipsPartialCoverage();
    void rasterProviderCloneSurvivesOriginal();
    void rasterProviderCancellationReachesSource();
    void mapRendererJobReprojectsAndRotates();
    void rendererClampsScaleBoundaries_data();
    void rendererClampsScaleBoundaries();
    void webMercatorXyzPixelReference();
    void polarTmsPixelReferenceReprojectsAndRotates();
    void encryptedAggregateKeysRenderOrFailTransparent();
    void pluginLayerTypeLifecycle();
    void rendererConcurrencyCancellationAndThrottle();
    void structureValidationCacheRefreshesChangedSources();
    void projectRestoredSourceContractRejectsMutation_data();
    void projectRestoredSourceContractRejectsMutation();
    void runtimeRejectsNonWhitelistedPtpTile();
    void rasterInterfaceRejectsExcessiveTileRequest();
    void eightyOnePackagesRenderAcrossReaderLimit();
    void legacyPackageXmlRoundTrip();
    void legacyPackageXmlRestoresRememberedKeys();
    void legacyPackageXmlPixelReference_data();
    void legacyPackageXmlPixelReference();
    void legacyPackageXmlBadImages_data();
    void legacyPackageXmlBadImages();
    void legacyPackageXmlCompleteImages_data();
    void legacyPackageXmlCompleteImages();
    void projectMovePreservesRelativePtpPixels_data();
    void projectMovePreservesRelativePtpPixels();
    void datasetXmlV1RoundTrip();
    void datasetXmlClippedPartitionBandRoundTrip();
    void datasetXmlLockedRoundTripDoesNotRequestUnlock();
    void datasetXmlBadImageSnapshotRoundTrip();
    void malformedDatasetXml_data();
    void malformedDatasetXml();
    void datasetXmlOptionalFieldsDefault();
    void emptyPlainDatasetXmlRoundTrip();
    void ptpElevationIsRejected();
};

void TestMtplLayer::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
}

void TestMtplLayer::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestMtplLayer::datasetCreatesSingleLayer()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );
  QVERIFY2( dataset.isValid( &error ), qPrintable( error ) );

  QgsProject project;
  auto *layer = new QgsMtplPluginLayer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer->isValid() );
  QVERIFY( layer->isTileDataset() );
  QVERIFY( layer->tileDataset() );
  QCOMPARE( layer->tileDataset()->packages.size(), 2 );
  project.addMapLayer( layer );
  QCOMPARE( project.mapLayers().size(), 1 );
  QCOMPARE( project.mapLayer( layer->id() ), static_cast<QgsMapLayer *>( layer ) );

  std::unique_ptr<QgsMtplPluginLayer> cloned( layer->clone() );
  QVERIFY( cloned->isTileDataset() );
  QVERIFY( cloned->tileDataset() );
  QCOMPARE( cloned->tileDataset()->packages.size(), 2 );
  QCOMPARE( cloned->tileDataset()->partitionRule, dataset.partitionRule );
}

void TestMtplLayer::datasetReplacementIsAtomic()
{
  QTemporaryDir originalDirectory;
  QTemporaryDir replacementDirectory;
  QVERIFY( originalDirectory.isValid() );
  QVERIFY( replacementDirectory.isValid() );

  QgsMtpl::TileDatasetDescriptor originalDataset;
  QgsMtpl::TileDatasetDescriptor replacementDataset;
  QString error;
  QVERIFY2( createXyzDataset( originalDirectory.path(), originalDataset, error ), qPrintable( error ) );
  QVERIFY2( createXyzDataset( replacementDirectory.path(), replacementDataset, error ), qPrintable( error ) );

  QgsMtplPluginLayer layer( originalDataset, QgsMtpl::CryptoKeys() );
  layer.setName( QStringLiteral( "用户命名的 PTP 图层" ) );
  QVERIFY( layer.isValid() );
  const QgsMtpl::TileDatasetDescriptor *originalSnapshot = layer.tileDataset();
  QVERIFY( originalSnapshot );
  const QString originalSource = layer.source();
  const QgsRectangle originalExtent = layer.extent();

  QList<QgsMtpl::CryptoKeys> wrongKeyCount;
  wrongKeyCount.append( QgsMtpl::CryptoKeys() );
  QVERIFY( !layer.replaceTileDataset( replacementDataset, wrongKeyCount, &error ) );
  QVERIFY( !error.isEmpty() );
  QCOMPARE( layer.tileDataset(), originalSnapshot );
  QCOMPARE( layer.source(), originalSource );
  QCOMPARE( layer.extent(), originalExtent );

  QList<QgsMtpl::CryptoKeys> replacementKeys;
  while ( replacementKeys.size() < replacementDataset.packages.size() )
    replacementKeys.append( QgsMtpl::CryptoKeys() );
  QgsMtpl::TileDatasetDescriptor invalidDataset = replacementDataset;
  invalidDataset.matrix.tileSize = 0;
  QVERIFY( !layer.replaceTileDataset( invalidDataset, replacementKeys, &error ) );
  QVERIFY( !error.isEmpty() );
  QCOMPARE( layer.tileDataset(), originalSnapshot );
  QCOMPARE( layer.source(), originalSource );
  QCOMPARE( layer.extent(), originalExtent );

  QVERIFY2( layer.replaceTileDataset( replacementDataset, replacementKeys, &error ), qPrintable( error ) );
  QVERIFY( layer.isValid() );
  QVERIFY( layer.tileDataset() );
  QVERIFY( layer.tileDataset() != originalSnapshot );
  QCOMPARE( QDir::cleanPath( layer.source() ), QDir::cleanPath( replacementDataset.sourcePath ) );
  QCOMPARE( layer.tileDataset()->packages.constFirst().descriptor.path,
            replacementDataset.packages.constFirst().descriptor.path );
  QCOMPARE( layer.name(), QStringLiteral( "用户命名的 PTP 图层" ) );
}

void TestMtplLayer::rasterInterfaceXyzTwoByOne()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  QCOMPARE( layer.crs().authid(), QStringLiteral( "EPSG:4326" ) );
  const QgsTileMatrix matrix = layer.tileMatrixSet().tileMatrix( 0 );
  QCOMPARE( matrix.matrixWidth(), 2 );
  QCOMPARE( matrix.matrixHeight(), 1 );

  QgsMtplRasterInterface raster( dataset, layer.tileMatrixSet(), {}, 0 );
  QCOMPARE( raster.xSize(), 2 * sTileSize );
  QCOMPARE( raster.ySize(), sTileSize );
  std::unique_ptr<QgsRasterBlock> block( raster.block( 1, matrix.extent(), 2 * sTileSize, sTileSize ) );
  QVERIFY( block );
  QVERIFY( block->isValid() );
  QCOMPARE( block->color( sTileSize / 2, sTileSize / 2 ), qRgb( 220, 30, 40 ) );
  QCOMPARE( block->color( sTileSize / 2, sTileSize + sTileSize / 2 ), qRgb( 20, 70, 230 ) );
  QVERIFY2( raster.errors().isEmpty(), qPrintable( raster.errors().join( QLatin1Char( '\n' ) ) ) );
}

void TestMtplLayer::rasterInterfaceTmsRowFlip()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createTmsDataset( directory.path(), dataset, error ), qPrintable( error ) );

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const QgsTileMatrix matrix = layer.tileMatrixSet().tileMatrix( 1 );
  QCOMPARE( matrix.matrixWidth(), 2 );
  QCOMPARE( matrix.matrixHeight(), 2 );

  QgsMtplRasterInterface raster( dataset, layer.tileMatrixSet(), {}, 1 );
  const QgsRectangle xyzTopExtent = matrix.tileExtent( QgsTileXYZ( 0, 0, 1 ) );
  std::unique_ptr<QgsRasterBlock> topBlock( raster.block( 1, xyzTopExtent, sTileSize, sTileSize ) );
  QVERIFY( topBlock );
  QCOMPARE( topBlock->color( sTileSize / 2, sTileSize / 2 ), qRgb( 30, 190, 70 ) );

  const QgsRectangle xyzBottomExtent = matrix.tileExtent( QgsTileXYZ( 0, 1, 1 ) );
  std::unique_ptr<QgsRasterBlock> bottomBlock( raster.block( 1, xyzBottomExtent, sTileSize, sTileSize ) );
  QVERIFY( bottomBlock );
  QCOMPARE( bottomBlock->color( sTileSize / 2, sTileSize / 2 ), qRgb( 240, 180, 20 ) );

  const QgsRectangle missingExtent = matrix.tileExtent( QgsTileXYZ( 1, 0, 1 ) );
  std::unique_ptr<QgsRasterBlock> missingBlock( raster.block( 1, missingExtent, sTileSize, sTileSize ) );
  QVERIFY( missingBlock );
  QCOMPARE( qAlpha( missingBlock->color( sTileSize / 2, sTileSize / 2 ) ), 0 );
  QVERIFY2( raster.errors().isEmpty(), qPrintable( raster.errors().join( QLatin1Char( '\n' ) ) ) );
}

void TestMtplLayer::rasterProviderProjectorClipsPartialCoverage_data()
{
  QTest::addColumn<int>( "precision" );
  QTest::newRow( "exact" ) << static_cast<int>( QgsRasterProjector::Exact );
  QTest::newRow( "approximate" ) << static_cast<int>( QgsRasterProjector::Approximate );
}

void TestMtplLayer::rasterProviderProjectorClipsPartialCoverage()
{
  QFETCH( int, precision );
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QImage referenceImage;
  QString error;
  QVERIFY2( createPartialProviderDataset( directory.path(), dataset, referenceImage, error ), qPrintable( error ) );
  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const auto validationCache = std::make_shared<QgsMtplStructureValidationCache>();
  QgsMtplRasterSourceProvider provider( dataset, layer.tileMatrixSet(), {}, 0, validationCache );
  QVERIFY( provider.isValid() );
  QCOMPARE( provider.input(), static_cast<QgsRasterInterface *>( nullptr ) );
  QCOMPARE( provider.sourceInput(), static_cast<QgsRasterInterface *>( &provider ) );
  QCOMPARE( provider.crs(), layer.crs() );
  QCOMPARE( provider.extent(), QgsRectangle( -2000.0, -1000.0, 0.0, 1000.0 ) );
  QCOMPARE( provider.xSize(), 2 * sTileSize );
  QCOMPARE( provider.ySize(), sTileSize );
  QCOMPARE( provider.xBlockSize(), sTileSize );
  QCOMPARE( provider.yBlockSize(), sTileSize );
  QCOMPARE( provider.bandCount(), 1 );
  QCOMPARE( provider.dataType( 1 ), Qgis::DataType::ARGB32_Premultiplied );
  QCOMPARE( provider.sourceDataType( 1 ), provider.dataType( 1 ) );
  QVERIFY( provider.extent() != layer.tileMatrixSet().rootMatrix().extent() );
  QCOMPARE( provider.capabilities(), Qgis::RasterInterfaceCapabilities( Qgis::RasterInterfaceCapability::NoCapabilities ) );
  QVERIFY( !( provider.capabilities() & Qgis::RasterInterfaceCapability::Size ) );

  // A filter may report a larger extent. The native projector must find the
  // provider through sourceInput() and clip to the actual package coverage.
  const QgsRectangle requestedSourceExtent( -2500.0, -1500.0, 500.0, 1500.0 );
  RecordingMtplRasterFilter filter( &provider, requestedSourceExtent );
  QCOMPARE( dynamic_cast<QgsRasterDataProvider *>( filter.sourceInput() ),
            static_cast<QgsRasterDataProvider *>( &provider ) );
  const QgsCoordinateReferenceSystem destinationCrs( QStringLiteral( "EPSG:4326" ) );
  QgsCoordinateTransform toDestination( provider.crs(), destinationCrs, QgsProject::instance()->transformContext() );
  const QgsRectangle destinationExtent = toDestination.transformBoundingBox( requestedSourceExtent );
  QgsRasterProjector projector;
  QVERIFY( projector.setInput( &filter ) );
  projector.setCrs( provider.crs(), destinationCrs, QgsProject::instance()->transformContext() );
  projector.setPrecision( static_cast<QgsRasterProjector::Precision>( precision ) );
  constexpr int outputSize = 7 * sTileSize;
  QgsRasterBlockFeedback feedback;
  std::unique_ptr<QgsRasterBlock> block( projector.block( 1, destinationExtent, outputSize, outputSize, &feedback ) );
  QVERIFY( block );
  QVERIFY( block->isValid() );
  QCOMPARE( filter.requestedExtents.size(), 1 );
  QCOMPARE( filter.requestedExtents.constFirst(), provider.extent() );
  QVERIFY( filter.requestedSizes.constFirst().width() > 0 );
  QVERIFY( filter.requestedSizes.constFirst().height() > 0 );
  QCOMPARE( filter.forwardedFeedback.constFirst(), &feedback );
  QVERIFY( !feedback.isCanceled() );
  QCOMPARE( validationCache->validationAttemptCount(), quint64( 1 ) );

  const QList<QPoint> referencePixels {
    QPoint( 2, 2 ), QPoint( sTileSize - 3, 2 ),
    QPoint( 2, sTileSize - 3 ), QPoint( sTileSize - 3, sTileSize - 3 )
  };
  for ( const QPoint &pixel : referencePixels )
  {
    const QgsPointXY sourcePoint(
      provider.extent().xMinimum() + ( pixel.x() + 0.5 ) * provider.extent().width() / sTileSize,
      provider.extent().yMaximum() - ( pixel.y() + 0.5 ) * provider.extent().height() / sTileSize );
    const QgsPointXY destinationPoint = toDestination.transform( sourcePoint );
    const int column = static_cast<int>( std::floor( ( destinationPoint.x() - destinationExtent.xMinimum() ) / destinationExtent.width() * outputSize ) );
    const int row = static_cast<int>( std::floor( ( destinationExtent.yMaximum() - destinationPoint.y() ) / destinationExtent.height() * outputSize ) );
    QVERIFY( column >= 0 && column < outputSize && row >= 0 && row < outputSize );
    QCOMPARE( block->color( row, column ), referenceImage.pixel( pixel ) );
  }
  QCOMPARE( qAlpha( block->color( 0, 0 ) ), 0 );
  QCOMPARE( qAlpha( block->color( 0, outputSize - 1 ) ), 0 );
  QCOMPARE( qAlpha( block->color( outputSize - 1, 0 ) ), 0 );
  QCOMPARE( qAlpha( block->color( outputSize - 1, outputSize - 1 ) ), 0 );
  QVERIFY2( provider.errors().isEmpty(), qPrintable( provider.errors().join( QLatin1Char( '\n' ) ) ) );
}

void TestMtplLayer::rasterProviderCloneSurvivesOriginal()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QImage referenceImage;
  QString error;
  QVERIFY2( createPartialProviderDataset( directory.path(), dataset, referenceImage, error ), qPrintable( error ) );
  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const auto validationCache = std::make_shared<QgsMtplStructureValidationCache>();
  std::unique_ptr<QgsMtplRasterSourceProvider> cloned;
  const QgsRectangle extent = layer.extent();
  {
    QgsMtplRasterSourceProvider original( dataset, layer.tileMatrixSet(), {}, 0, validationCache );
    std::unique_ptr<QgsRasterBlock> initial( original.block( 1, extent, sTileSize, sTileSize ) );
    QVERIFY( initial );
    QVERIFY( initial->isValid() );
    QCOMPARE( initial->color( 2, 2 ), referenceImage.pixel( 2, 2 ) );
    QCOMPARE( validationCache->validationAttemptCount(), quint64( 1 ) );
    cloned.reset( original.clone() );
    QVERIFY( cloned );
    QVERIFY( cloned.get() != &original );
    QCOMPARE( cloned->crs(), original.crs() );
    QCOMPARE( cloned->extent(), original.extent() );
    QCOMPARE( cloned->xSize(), original.xSize() );
    QCOMPARE( cloned->ySize(), original.ySize() );
    QCOMPARE( cloned->capabilities(), original.capabilities() );
  }

  // The original has already released its reader and image cache.
  QVERIFY( cloned->isValid() );
  QCOMPARE( cloned->input(), static_cast<QgsRasterInterface *>( nullptr ) );
  QCOMPARE( cloned->sourceInput(), static_cast<QgsRasterInterface *>( cloned.get() ) );
  std::unique_ptr<QgsRasterBlock> block( cloned->block( 1, extent, sTileSize, sTileSize ) );
  QVERIFY( block );
  QVERIFY( block->isValid() );
  for ( int row = 0; row < sTileSize; ++row )
    for ( int column = 0; column < sTileSize; ++column )
      QCOMPARE( block->color( row, column ), referenceImage.pixel( column, row ) );
  QCOMPARE( validationCache->validationAttemptCount(), quint64( 1 ) );
  QVERIFY2( cloned->errors().isEmpty(), qPrintable( cloned->errors().join( QLatin1Char( '\n' ) ) ) );
}

void TestMtplLayer::rasterProviderCancellationReachesSource()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QImage referenceImage;
  QString error;
  QVERIFY2( createPartialProviderDataset( directory.path(), dataset, referenceImage, error ), qPrintable( error ) );
  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const auto validationCache = std::make_shared<QgsMtplStructureValidationCache>();
  QgsMtplRasterSourceProvider provider( dataset, layer.tileMatrixSet(), {}, 0, validationCache );
  QgsRasterBlockFeedback alreadyCanceled;
  alreadyCanceled.cancel();
  std::unique_ptr<QgsRasterBlock> canceledBlock(
    provider.block( 1, provider.extent(), sTileSize, sTileSize, &alreadyCanceled ) );
  QVERIFY( canceledBlock );
  QVERIFY( canceledBlock->isValid() );
  QCOMPARE( qAlpha( canceledBlock->color( 2, 2 ) ), 0 );
  QCOMPARE( validationCache->validationAttemptCount(), quint64( 0 ) );

  RecordingMtplRasterFilter filter( &provider, provider.extent() );
  filter.cancelBeforeForward = true;
  const QgsCoordinateReferenceSystem destinationCrs( QStringLiteral( "EPSG:4326" ) );
  QgsCoordinateTransform toDestination( provider.crs(), destinationCrs, QgsProject::instance()->transformContext() );
  QgsRasterProjector projector;
  QVERIFY( projector.setInput( &filter ) );
  projector.setCrs( provider.crs(), destinationCrs, QgsProject::instance()->transformContext() );
  QgsRasterBlockFeedback feedback;
  std::unique_ptr<QgsRasterBlock> projected(
    projector.block( 1, toDestination.transformBoundingBox( provider.extent() ), sTileSize, sTileSize, &feedback ) );
  QVERIFY( projected );
  QCOMPARE( filter.requestedExtents.size(), 1 );
  QCOMPARE( filter.forwardedFeedback.constFirst(), &feedback );
  QVERIFY( feedback.isCanceled() );
  QCOMPARE( validationCache->validationAttemptCount(), quint64( 0 ) );
  QVERIFY2( provider.errors().isEmpty(), qPrintable( provider.errors().join( QLatin1Char( '\n' ) ) ) );

  QgsRasterBlockFeedback retryFeedback;
  std::unique_ptr<QgsRasterBlock> retried(
    provider.block( 1, provider.extent(), sTileSize, sTileSize, &retryFeedback ) );
  QVERIFY( retried );
  QVERIFY( retried->isValid() );
  QCOMPARE( retried->color( 2, 2 ), referenceImage.pixel( 2, 2 ) );
  QCOMPARE( validationCache->validationAttemptCount(), quint64( 1 ) );
  QVERIFY( !retryFeedback.isCanceled() );
}

void TestMtplLayer::mapRendererJobReprojectsAndRotates()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );
  dataset.hasExtent = true;
  dataset.extentXMinimum = -135.0;
  dataset.extentYMinimum = 45.0;
  dataset.extentXMaximum = 90.0;
  dataset.extentYMaximum = 90.0;

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  QCOMPARE( layer.crs().authid(), QStringLiteral( "EPSG:4326" ) );

  const QgsCoordinateReferenceSystem destinationCrs( QStringLiteral( "EPSG:3413" ) );
  QVERIFY( destinationCrs.isValid() );
  QgsCoordinateTransform toDestination( layer.crs(), destinationCrs, QgsProject::instance()->transformContext() );
  toDestination.setBallparkTransformsAreAppropriate( true );

  const QgsPointXY redMapPoint = toDestination.transform( QgsPointXY( -90.0, 75.0 ) );
  const QgsPointXY blueMapPoint = toDestination.transform( QgsPointXY( 30.0, 75.0 ) );
  const QgsPointXY center( ( redMapPoint.x() + blueMapPoint.x() ) * 0.5,
                           ( redMapPoint.y() + blueMapPoint.y() ) * 0.5 );
  const double pointSpan = std::max( std::abs( redMapPoint.x() - blueMapPoint.x() ),
                                     std::abs( redMapPoint.y() - blueMapPoint.y() ) );
  QVERIFY( std::isfinite( pointSpan ) );
  QVERIFY( pointSpan > 0 );

  QgsMapSettings settings;
  settings.setLayers( QList<QgsMapLayer *>() << &layer );
  settings.setDestinationCrs( destinationCrs );
  settings.setTransformContext( QgsProject::instance()->transformContext() );
  settings.setOutputSize( QSize( 640, 480 ) );
  settings.setOutputDpi( 96 );
  settings.setBackgroundColor( QColor( 0, 0, 0, 0 ) );
  settings.setOutputImageFormat( QImage::Format_ARGB32_Premultiplied );
  settings.setExtent( QgsRectangle( center.x() - pointSpan,
                                    center.y() - pointSpan * 0.75,
                                    center.x() + pointSpan,
                                    center.y() + pointSpan * 0.75 ) );
  settings.setRotation( 27.0 );
  QVERIFY( settings.hasValidSettings() );
  QCOMPARE( settings.mapToPixel().mapRotation(), 27.0 );

  const QgsPointXY redDevicePoint = settings.mapToPixel().transform( redMapPoint );
  const QgsPointXY blueDevicePoint = settings.mapToPixel().transform( blueMapPoint );
  QVERIFY( redDevicePoint.x() >= 0 && redDevicePoint.x() < settings.outputSize().width() );
  QVERIFY( redDevicePoint.y() >= 0 && redDevicePoint.y() < settings.outputSize().height() );
  QVERIFY( blueDevicePoint.x() >= 0 && blueDevicePoint.x() < settings.outputSize().width() );
  QVERIFY( blueDevicePoint.y() >= 0 && blueDevicePoint.y() < settings.outputSize().height() );

  QgsMapRendererSequentialJob job( settings );
  job.start();
  job.waitForFinished();
  QVERIFY2( job.errors().isEmpty(),
            qPrintable( job.errors().isEmpty() ? QString() : job.errors().constFirst().message ) );

  const QImage rendered = job.renderedImage();
  QVERIFY( !rendered.isNull() );
  QCOMPARE( rendered.size(), settings.outputSize() );
  const int opaquePixels = opaquePixelCount( rendered );
  QVERIFY2( opaquePixels > rendered.width() * rendered.height() / 200,
            qPrintable( QStringLiteral( "Only %1 pixels were rendered opaque." ).arg( opaquePixels ) ) );
  QVERIFY2( hasColorNear( rendered, redDevicePoint, QColor( 220, 30, 40 ) ),
            "The reprojected western tile is not aligned with its known EPSG:3413 location." );
  QVERIFY2( hasColorNear( rendered, blueDevicePoint, QColor( 20, 70, 230 ) ),
            "The reprojected eastern tile is not aligned with its known EPSG:3413 location." );
  QCOMPARE( layer.structuralValidationAttemptCount(), 2ULL );
}

void TestMtplLayer::rendererClampsScaleBoundaries_data()
{
  QTest::addColumn<QString>( "method" );
  QTest::addColumn<int>( "scenario" );
  QTest::addColumn<int>( "maximumZoom" );
  struct Scenario
  {
    const char *name;
    int value;
  };
  const Scenario scenarios[] = {
    { "exact-binary-overzoom", 0 }, { "before-binary-overzoom", 1 }, { "after-binary-overzoom", 2 },
    { "finest-boundary", 3 }, { "before-finest-boundary", 4 }, { "after-finest-boundary", 5 },
    { "coarsest-boundary", 6 }, { "before-coarsest-boundary", 7 }, { "after-coarsest-boundary", 8 },
    { "zero-scale", 9 }, { "negative-scale", 10 }, { "nan-scale", 11 }, { "infinite-scale", 12 }
  };
  for ( const QString &method : { QStringLiteral( "mapbox" ), QStringLiteral( "esri" ) } )
  {
    for ( const Scenario &scenario : scenarios )
    {
      const QByteArray rowName = method.toLatin1() + '-' + scenario.name;
      QTest::newRow( rowName.constData() ) << method << scenario.value << 1;
    }
    const QByteArray singleLevelName = method.toLatin1() + QByteArrayLiteral( "-single-level-exact-overzoom" );
    QTest::newRow( singleLevelName.constData() ) << method << 0 << 0;
  }
}

void TestMtplLayer::rendererClampsScaleBoundaries()
{
  QFETCH( QString, method );
  QFETCH( int, scenario );
  QFETCH( int, maximumZoom );
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileMatrixDefinition definition = customMatrixDefinition(
    QStringLiteral( "EPSG:3857" ), QgsMtpl::TileScheme::Xyz, maximumZoom, 0.0, 1024.0, 1024.0, 1, 1 );
  definition.scaleToZoomMethod = method;
  QByteArray metadata = customTileMetadata( definition.crsAuthId, definition.scheme, maximumZoom,
                                           definition.topLeftX, definition.topLeftY, definition.z0TileSpan, 1, 1 );
  metadata.replace( "\"minimum_zoom\"", "\"min_zoom\"" );
  metadata.replace( "\"maximum_zoom\"", "\"max_zoom\"" );
  const QByteArray methodFragment = QByteArrayLiteral( "\"scale_to_zoom_method\":\"" ) + method.toLatin1() + QByteArrayLiteral( "\"" );
  metadata.replace( QByteArrayLiteral( "\"scale_to_zoom_method\":\"mapbox\"" ), methodFragment );
  const QgsMtpl::TilePackageAddress address { 0, maximumZoom, 0, 0, 0 };
  const QString packagePath = directory.filePath( address.fileName() );
  const QColor coarseColor( 220, 30, 40 );
  const QColor fineColor( 20, 70, 230 );
  const QByteArray coarseImage = solidPng( coarseColor );
  const QByteArray fineImage = solidPng( fineColor );
  QVERIFY( !coarseImage.isEmpty() && !fineImage.isEmpty() );
  QList<QgsMtplTest::TileFixtureRange> ranges { { 0, 0, 0, 0, 0 } };
  QList<QgsMtplTest::TileFixtureEntry> tiles { { 0, 0, 0, coarseImage } };
  QList<QgsMtpl::TileRangeRecord> summaries { rangeRecord( 0, 0, 0, 0, 0 ) };
  if ( maximumZoom == 1 )
  {
    ranges.push_back( { 1, 0, 1, 0, 1 } );
    summaries.append( rangeRecord( 1, 0, 1, 0, 1 ) );
    for ( quint32 row = 0; row < 2; ++row )
    {
      for ( quint32 column = 0; column < 2; ++column )
        tiles.append( { 1, column, row, fineImage } );
    }
  }
  QString error;
  QVERIFY2( QgsMtplTest::writePtpFixture( packagePath, sTileSize, metadata, MTPL_STORAGE_PLAIN,
                                         {}, ranges, tiles, error ), qPrintable( error ) );
  QgsMtpl::TileDatasetDescriptor dataset = datasetDescriptor(
    directory.path(), definition.scheme, 0, maximumZoom, 1,
    { customDatasetPackage( packagePath, address, summaries, 0, maximumZoom, definition ) } );
  dataset.matrix = definition;
  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const QgsTileMatrixSet &matrices = layer.tileMatrixSet();
  const double divisor = method == QLatin1String( "mapbox" ) ? 2.0 : 1.0;
  const double finestScale = matrices.tileMatrix( maximumZoom ).scale() / divisor;
  const double coarsestScale = matrices.tileMatrix( 0 ).scale() / divisor;
  double tileScale = scenario < 3 ? matrices.tileMatrix( maximumZoom ).scale() / 32.0
                                 : scenario < 6 ? finestScale : coarsestScale;
  if ( scenario == 1 || scenario == 4 || scenario == 7 )
    tileScale = std::nextafter( tileScale, 0.0 );
  else if ( scenario == 2 || scenario == 5 || scenario == 8 )
    tileScale = std::nextafter( tileScale, std::numeric_limits<double>::infinity() );
  else if ( scenario == 9 )
    tileScale = 0.0;
  else if ( scenario == 10 )
    tileScale = -1.0;
  else if ( scenario == 11 )
    tileScale = std::numeric_limits<double>::quiet_NaN();
  else if ( scenario == 12 )
    tileScale = std::numeric_limits<double>::infinity();

  QgsMapSettings settings;
  settings.setDestinationCrs( layer.crs() );
  settings.setOutputSize( QSize( 2 * sTileSize, 2 * sTileSize ) );
  settings.setExtent( matrices.rootMatrix().extent() );
  QVERIFY( settings.hasValidSettings() );
  QImage rendered( settings.outputSize(), QImage::Format_ARGB32_Premultiplied );
  rendered.fill( Qt::transparent );
  QPainter painter( &rendered );
  QgsRenderContext context = QgsRenderContext::fromMapSettings( settings );
  context.setPainter( &painter );
  context.setCoordinateTransform( QgsCoordinateTransform( layer.crs(), layer.crs(), QgsProject::instance() ) );
  if ( method == QLatin1String( "mapbox" ) )
  {
    constexpr double referenceDpi = 0.0254 / ( 2.8 / 10000.0 );
    // This DPI multiple preserves this fixture's boundaries and adjacent doubles
    // through the MapBox normalization. Assert that below to avoid a false regression.
    context.setScaleFactor( referenceDpi * 25.0 / 25.4 );
    context.setRendererScale( tileScale * 25.0 );
  }
  else
    context.setRendererScale( tileScale );
  if ( scenario < 9 )
    QVERIFY2( matrices.scaleForRenderContext( context ) == tileScale,
              "The regression must preserve the exact boundary or adjacent scale." );
  std::unique_ptr<QgsMapLayerRenderer> renderer( layer.createMapRenderer( context ) );
  QVERIFY( renderer );
  QElapsedTimer timer;
  timer.start();
  const bool result = renderer->render();
  QVERIFY2( timer.elapsed() < 5000, "PTP scale boundary rendering did not finish promptly." );
  painter.end();
  if ( scenario >= 9 )
  {
    QVERIFY( !result );
    QCOMPARE( opaquePixelCount( rendered ), 0 );
    QVERIFY( renderer->errors().join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "比例尺" ) ) );
  }
  else
  {
    QVERIFY( result );
    QVERIFY2( renderer->errors().isEmpty(), qPrintable( renderer->errors().join( QLatin1Char( '\n' ) ) ) );
    QImage expected( rendered.size(), rendered.format() );
    const bool useCoarseColor = maximumZoom == 0 || scenario >= 6 ||
                               ( method == QLatin1String( "esri" ) && scenario == 5 );
    expected.fill( useCoarseColor ? coarseColor : fineColor );
    QCOMPARE( rendered, expected );
  }
}

void TestMtplLayer::webMercatorXyzPixelReference()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createWebMercatorPixelDataset( directory.path(), dataset, error ), qPrintable( error ) );
  QVERIFY2( dataset.isValid( &error ), qPrintable( error ) );

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  QCOMPARE( layer.crs().authid(), QStringLiteral( "EPSG:3857" ) );
  const QgsTileMatrix matrix = layer.tileMatrixSet().tileMatrix( 1 );
  QCOMPARE( matrix.matrixWidth(), 2 );
  QCOMPARE( matrix.matrixHeight(), 2 );

  QgsMtplRasterInterface raster( dataset, layer.tileMatrixSet(), {}, 1 );
  std::unique_ptr<QgsRasterBlock> block(
    raster.block( 1, matrix.extent(), 2 * sTileSize, 2 * sTileSize ) );
  QVERIFY( block );
  QVERIFY( block->isValid() );
  QCOMPARE( block->color( sTileSize / 2, sTileSize / 2 ), qRgb( 220, 30, 40 ) );
  QCOMPARE( block->color( sTileSize / 2, sTileSize + sTileSize / 2 ), qRgb( 20, 70, 230 ) );
  QCOMPARE( block->color( sTileSize + sTileSize / 2, sTileSize / 2 ), qRgb( 30, 190, 70 ) );
  QCOMPARE( block->color( sTileSize + sTileSize / 2, sTileSize + sTileSize / 2 ), qRgb( 240, 180, 20 ) );
  QVERIFY2( raster.errors().isEmpty(), qPrintable( raster.errors().join( QLatin1Char( '\n' ) ) ) );

  QgsMapSettings settings;
  settings.setLayers( QList<QgsMapLayer *>() << &layer );
  settings.setDestinationCrs( layer.crs() );
  settings.setTransformContext( QgsProject::instance()->transformContext() );
  settings.setOutputSize( QSize( 2 * sTileSize, 2 * sTileSize ) );
  settings.setOutputDpi( 96 );
  settings.setBackgroundColor( QColor( 0, 0, 0, 0 ) );
  settings.setOutputImageFormat( QImage::Format_ARGB32_Premultiplied );
  settings.setExtent( matrix.extent() );
  QVERIFY( settings.hasValidSettings() );

  QgsMapRendererSequentialJob job( settings );
  job.start();
  job.waitForFinished();
  QVERIFY2( job.errors().isEmpty(),
            qPrintable( job.errors().isEmpty() ? QString() : job.errors().constFirst().message ) );
  const QImage rendered = job.renderedImage();
  QCOMPARE( rendered.size(), settings.outputSize() );
  QVERIFY( opaquePixelCount( rendered ) > rendered.width() * rendered.height() * 9 / 10 );

  const QColor expectedColors[2][2] = {
    { QColor( 220, 30, 40 ), QColor( 20, 70, 230 ) },
    { QColor( 30, 190, 70 ), QColor( 240, 180, 20 ) },
  };
  for ( int row = 0; row < 2; ++row )
  {
    for ( int column = 0; column < 2; ++column )
    {
      const QgsPointXY devicePoint = settings.mapToPixel().transform(
        matrix.tileExtent( QgsTileXYZ( column, row, 1 ) ).center() );
      QVERIFY2( hasColorNear( rendered, devicePoint, expectedColors[row][column], 2, 5 ),
                qPrintable( QStringLiteral( "Web Mercator tile %1/%2 missed its pixel reference." )
                              .arg( column ).arg( row ) ) );
    }
  }
}

void TestMtplLayer::polarTmsPixelReferenceReprojectsAndRotates()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createPolarTmsPixelDataset( directory.path(), dataset, error ), qPrintable( error ) );
  QVERIFY2( dataset.isValid( &error ), qPrintable( error ) );

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  QCOMPARE( layer.crs().authid(), QStringLiteral( "EPSG:3413" ) );
  const QgsTileMatrix matrix = layer.tileMatrixSet().tileMatrix( 0 );
  QCOMPARE( matrix.matrixWidth(), 2 );
  QCOMPARE( matrix.matrixHeight(), 2 );

  QgsMtplRasterInterface raster( dataset, layer.tileMatrixSet(), {}, 0 );
  std::unique_ptr<QgsRasterBlock> block(
    raster.block( 1, matrix.extent(), 2 * sTileSize, 2 * sTileSize ) );
  QVERIFY( block );
  QVERIFY( block->isValid() );
  QCOMPARE( block->color( sTileSize / 2, sTileSize / 2 ), qRgb( 30, 190, 70 ) );
  QCOMPARE( block->color( sTileSize / 2, sTileSize + sTileSize / 2 ), qRgb( 240, 180, 20 ) );
  QCOMPARE( block->color( sTileSize + sTileSize / 2, sTileSize / 2 ), qRgb( 190, 35, 180 ) );
  QCOMPARE( block->color( sTileSize + sTileSize / 2, sTileSize + sTileSize / 2 ), qRgb( 20, 190, 210 ) );
  QVERIFY2( raster.errors().isEmpty(), qPrintable( raster.errors().join( QLatin1Char( '\n' ) ) ) );

  const QgsCoordinateReferenceSystem destinationCrs( QStringLiteral( "EPSG:3857" ) );
  QVERIFY( destinationCrs.isValid() );
  QgsCoordinateTransform toDestination( layer.crs(), destinationCrs, QgsProject::instance()->transformContext() );
  toDestination.setBallparkTransformsAreAppropriate( true );
  const QgsPointXY greenMapPoint = toDestination.transform( QgsPointXY( -1000000.0, 1000000.0 ) );
  const QgsPointXY yellowMapPoint = toDestination.transform( QgsPointXY( 1000000.0, 1000000.0 ) );
  const QgsPointXY center( ( greenMapPoint.x() + yellowMapPoint.x() ) * 0.5,
                           ( greenMapPoint.y() + yellowMapPoint.y() ) * 0.5 );
  const double pointSpan = std::max( std::abs( greenMapPoint.x() - yellowMapPoint.x() ),
                                     std::abs( greenMapPoint.y() - yellowMapPoint.y() ) );
  QVERIFY( std::isfinite( pointSpan ) );
  QVERIFY( pointSpan > 0 );

  QgsMapSettings settings;
  settings.setLayers( QList<QgsMapLayer *>() << &layer );
  settings.setDestinationCrs( destinationCrs );
  settings.setTransformContext( QgsProject::instance()->transformContext() );
  settings.setOutputSize( QSize( 640, 480 ) );
  settings.setOutputDpi( 96 );
  settings.setBackgroundColor( QColor( 0, 0, 0, 0 ) );
  settings.setOutputImageFormat( QImage::Format_ARGB32_Premultiplied );
  settings.setExtent( QgsRectangle( center.x() - pointSpan,
                                    center.y() - pointSpan * 0.75,
                                    center.x() + pointSpan,
                                    center.y() + pointSpan * 0.75 ) );
  settings.setRotation( 23.0 );
  QVERIFY( settings.hasValidSettings() );
  QCOMPARE( settings.mapToPixel().mapRotation(), 23.0 );

  const QgsPointXY greenDevicePoint = settings.mapToPixel().transform( greenMapPoint );
  const QgsPointXY yellowDevicePoint = settings.mapToPixel().transform( yellowMapPoint );
  QVERIFY( greenDevicePoint.x() >= 0 && greenDevicePoint.x() < settings.outputSize().width() );
  QVERIFY( greenDevicePoint.y() >= 0 && greenDevicePoint.y() < settings.outputSize().height() );
  QVERIFY( yellowDevicePoint.x() >= 0 && yellowDevicePoint.x() < settings.outputSize().width() );
  QVERIFY( yellowDevicePoint.y() >= 0 && yellowDevicePoint.y() < settings.outputSize().height() );

  QgsMapRendererSequentialJob job( settings );
  job.start();
  job.waitForFinished();
  QVERIFY2( job.errors().isEmpty(),
            qPrintable( job.errors().isEmpty() ? QString() : job.errors().constFirst().message ) );
  const QImage rendered = job.renderedImage();
  QVERIFY( !rendered.isNull() );
  QCOMPARE( rendered.size(), settings.outputSize() );
  QVERIFY( opaquePixelCount( rendered ) > rendered.width() * rendered.height() / 200 );
  QVERIFY2( hasColorNear( rendered, greenDevicePoint, QColor( 30, 190, 70 ), 8, 40 ),
            "The EPSG:3413 TMS upper-left tile missed its reprojected pixel reference." );
  QVERIFY2( hasColorNear( rendered, yellowDevicePoint, QColor( 240, 180, 20 ), 8, 40 ),
            "The EPSG:3413 TMS upper-right tile missed its reprojected pixel reference." );
}

void TestMtplLayer::encryptedAggregateKeysRenderOrFailTransparent()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::CryptoKeys correctKeys = QgsMtplTest::fixtureKeys();
  QgsMtpl::CryptoKeys wrongKeys = QgsMtplTest::differentFixtureKeys();
  QVERIFY( correctKeys.isValid() );
  QVERIFY( wrongKeys.isValid() );

  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createEncryptedAggregateDataset( directory.path(), correctKeys, dataset, error ), qPrintable( error ) );
  QVERIFY2( dataset.isValid( &error ), qPrintable( error ) );

  QgsMtplPluginLayer correctLayer( dataset, correctKeys );
  QVERIFY( correctLayer.isValid() );
  QVERIFY( correctLayer.hasCryptoKeys() );
  const QgsTileMatrix matrix = correctLayer.tileMatrixSet().tileMatrix( 0 );

  QgsMapSettings settings;
  settings.setLayers( QList<QgsMapLayer *>() << &correctLayer );
  settings.setDestinationCrs( correctLayer.crs() );
  settings.setTransformContext( QgsProject::instance()->transformContext() );
  settings.setOutputSize( QSize( 2 * sTileSize, sTileSize ) );
  settings.setOutputDpi( 96 );
  settings.setBackgroundColor( QColor( 0, 0, 0, 0 ) );
  settings.setOutputImageFormat( QImage::Format_ARGB32_Premultiplied );
  settings.setExtent( matrix.extent() );
  QVERIFY( settings.hasValidSettings() );

  QgsMapRendererSequentialJob correctJob( settings );
  correctJob.start();
  correctJob.waitForFinished();
  QVERIFY2( correctJob.errors().isEmpty(),
            qPrintable( correctJob.errors().isEmpty() ? QString() : correctJob.errors().constFirst().message ) );
  const QImage correctImage = correctJob.renderedImage();
  QVERIFY2( hasColorNear( correctImage, QgsPointXY( sTileSize / 2.0, sTileSize / 2.0 ), QColor( 220, 30, 40 ), 2, 5 ),
            "The first encrypted aggregate package did not render with the fixture key." );
  QVERIFY2( hasColorNear( correctImage, QgsPointXY( sTileSize + sTileSize / 2.0, sTileSize / 2.0 ), QColor( 20, 70, 230 ), 2, 5 ),
            "The second encrypted aggregate package did not render with the fixture key." );

  QgsMtplPluginLayer wrongLayer( dataset, wrongKeys );
  QVERIFY( wrongLayer.isValid() );
  QgsMtplRasterInterface wrongRaster(
    dataset, wrongLayer.tileMatrixSet(), { wrongKeys, wrongKeys }, 0 );
  std::unique_ptr<QgsRasterBlock> wrongBlock(
    wrongRaster.block( 1, matrix.extent(), 2 * sTileSize, sTileSize ) );
  QVERIFY( wrongBlock );
  QVERIFY( wrongBlock->isValid() );
  for ( int row = 0; row < wrongBlock->height(); ++row )
  {
    for ( int column = 0; column < wrongBlock->width(); ++column )
      QCOMPARE( qAlpha( wrongBlock->color( row, column ) ), 0 );
  }
  QVERIFY( !wrongRaster.errors().isEmpty() );
  QVERIFY2( wrongRaster.errors().join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "图层已降级：" ) ),
            qPrintable( wrongRaster.errors().join( QLatin1Char( '\n' ) ) ) );
  QVERIFY2( wrongRaster.errors().join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "无法解密" ) ),
            qPrintable( wrongRaster.errors().join( QLatin1Char( '\n' ) ) ) );

  correctKeys.clear();
  wrongKeys.clear();
}

void TestMtplLayer::pluginLayerTypeLifecycle()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );

  QgsProject::instance()->removeAllMapLayers();
  QgsPluginLayerRegistry registry;
  QVERIFY( registry.addPluginLayerType( new QgsMtplPluginLayerType() ) );
  QVERIFY( registry.pluginLayerTypes().contains( QgsMtplPluginLayer::layerTypeKey() ) );
  QCOMPARE( registry.pluginLayerType( QgsMtplPluginLayer::layerTypeKey() )->name(),
            QgsMtplPluginLayer::layerTypeKey() );

  std::unique_ptr<QgsPluginLayer> defaultLayer(
    registry.createLayer( QgsMtplPluginLayer::layerTypeKey() ) );
  QVERIFY( defaultLayer );
  QCOMPARE( defaultLayer->pluginLayerType(), QgsMtplPluginLayer::layerTypeKey() );

  QgsPluginLayer *uriLayer = registry.createLayer(
    QgsMtplPluginLayer::layerTypeKey(), dataset.packages.constFirst().descriptor.path );
  QVERIFY( uriLayer );
  QCOMPARE( uriLayer->pluginLayerType(), QgsMtplPluginLayer::layerTypeKey() );
  auto *typedLayer = dynamic_cast<QgsMtplPluginLayer *>( uriLayer );
  QVERIFY( typedLayer );
  QCOMPARE( QDir::cleanPath( typedLayer->descriptor().path ),
            QDir::cleanPath( dataset.packages.constFirst().descriptor.path ) );

  QgsMtpl::PackageDescriptor metadataDescriptor = dataset.packages.constFirst().descriptor;
  metadataDescriptor.metadata.insert( QStringLiteral( "publicLabel" ), QStringLiteral( "public-value" ) );
  metadataDescriptor.metadata.insert( QStringLiteral( "privateKeyBase64" ), QStringLiteral( "top-level-secret" ) );
  metadataDescriptor.metadata.insert( QStringLiteral( "_MtPlSpoof" ), QStringLiteral( "internal-secret" ) );
  metadataDescriptor.metadata.insert(
    QStringLiteral( "nested" ),
    QVariantMap {
      { QStringLiteral( "password" ), QStringLiteral( "nested-secret" ) },
      { QStringLiteral( "label" ), QStringLiteral( "nested-visible" ) }
    } );
  metadataDescriptor.metadata.insert(
    QStringLiteral( "items" ),
    QVariantList {
      QVariantMap {
        { QStringLiteral( "device_key_hex" ), QStringLiteral( "list-secret" ) },
        { QStringLiteral( "name" ), QStringLiteral( "list-visible" ) }
      }
    } );
  QgsMtplPluginLayer metadataLayer( metadataDescriptor );
  QString propertiesText;
  QTimer::singleShot( 0, [&propertiesText]()
  {
    QDialog *dialog = qobject_cast<QDialog *>( QApplication::activeModalWidget() );
    if ( !dialog )
      return;
    if ( QTableWidget *table = dialog->findChild<QTableWidget *>( QStringLiteral( "mtplMetadataTable" ) ) )
    {
      for ( int row = 0; row < table->rowCount(); ++row )
      {
        if ( table->item( row, 0 ) )
          propertiesText += table->item( row, 0 )->text() + QLatin1Char( '\n' );
        if ( table->item( row, 1 ) )
          propertiesText += table->item( row, 1 )->text() + QLatin1Char( '\n' );
      }
    }
    dialog->reject();
  } );
  QVERIFY( registry.pluginLayerType( QgsMtplPluginLayer::layerTypeKey() )->showLayerProperties( &metadataLayer ) );
  QVERIFY( propertiesText.contains( QStringLiteral( "public-value" ) ) );
  QVERIFY( propertiesText.contains( QStringLiteral( "nested-visible" ) ) );
  QVERIFY( propertiesText.contains( QStringLiteral( "list-visible" ) ) );
  QVERIFY( !propertiesText.contains( QStringLiteral( "top-level-secret" ) ) );
  QVERIFY( !propertiesText.contains( QStringLiteral( "nested-secret" ) ) );
  QVERIFY( !propertiesText.contains( QStringLiteral( "list-secret" ) ) );
  QVERIFY( !propertiesText.contains( QStringLiteral( "internal-secret" ) ) );

  QgsMtplPluginLayer datasetPropertiesLayer( dataset, QgsMtpl::CryptoKeys() );
  QString datasetPropertiesText;
  QTimer::singleShot( 0, [&datasetPropertiesText]()
  {
    QDialog *dialog = qobject_cast<QDialog *>( QApplication::activeModalWidget() );
    if ( !dialog )
      return;
    for ( QLabel *label : dialog->findChildren<QLabel *>() )
      datasetPropertiesText += label->text() + QLatin1Char( '\n' );
    dialog->reject();
  } );
  QVERIFY( registry.pluginLayerType( QgsMtplPluginLayer::layerTypeKey() )->showLayerProperties( &datasetPropertiesLayer ) );
  QVERIFY( datasetPropertiesText.contains( QStringLiteral( "根矩阵" ) ) );
  QVERIFY( datasetPropertiesText.contains( QStringLiteral( "左上角" ) ) );
  QVERIFY( datasetPropertiesText.contains( QStringLiteral( "Z0 瓦片跨度" ) ) );
  QVERIFY( datasetPropertiesText.contains( QStringLiteral( "比例尺转级别" ) ) );
  QVERIFY( datasetPropertiesText.contains( QStringLiteral( "稀疏统计" ) ) );
  QVERIFY( datasetPropertiesText.contains( QStringLiteral( "总槽位" ) ) );
  QVERIFY( datasetPropertiesText.contains( QStringLiteral( "存在瓦片" ) ) );
  QVERIFY( datasetPropertiesText.contains( QStringLiteral( "空槽" ) ) );

  QPointer<QgsPluginLayer> trackedLayer( uriLayer );
  QCOMPARE( QgsProject::instance()->addMapLayer( uriLayer ), static_cast<QgsMapLayer *>( uriLayer ) );

  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  auto *encryptedLayer = new QgsMtplPluginLayer( dataset, keys );
  QVERIFY( encryptedLayer->hasCryptoKeys() );
  QPointer<QgsMtplPluginLayer> trackedEncryptedLayer( encryptedLayer );
  QCOMPARE( QgsProject::instance()->addMapLayer( encryptedLayer ), static_cast<QgsMapLayer *>( encryptedLayer ) );
  QCOMPARE( QgsProject::instance()->mapLayers().size(), 2 );

  QVERIFY( registry.removePluginLayerType( QgsMtplPluginLayer::layerTypeKey() ) );
  QVERIFY( trackedLayer.isNull() );
  QVERIFY( trackedEncryptedLayer.isNull() );
  QVERIFY( !registry.pluginLayerTypes().contains( QgsMtplPluginLayer::layerTypeKey() ) );
  QVERIFY( registry.pluginLayerType( QgsMtplPluginLayer::layerTypeKey() ) == nullptr );
  QVERIFY( QgsProject::instance()->mapLayers().isEmpty() );
  keys.clear();
}

void TestMtplLayer::rendererConcurrencyCancellationAndThrottle()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const QgsTileMatrix matrix = layer.tileMatrixSet().tileMatrix( 0 );
  const auto validationCache = std::make_shared<QgsMtplStructureValidationCache>();

  std::vector<std::future<bool>> renders;
  for ( int index = 0; index < 4; ++index )
  {
    renders.emplace_back( std::async( std::launch::async, [dataset, matrixSet = layer.tileMatrixSet(), extent = matrix.extent(), validationCache]()
    {
      QgsMtplRasterInterface raster( dataset, matrixSet, {}, 0, validationCache );
      std::unique_ptr<QgsRasterBlock> block( raster.block( 1, extent, 2 * sTileSize, sTileSize ) );
      return block && block->isValid() &&
             block->color( sTileSize / 2, sTileSize / 2 ) == qRgb( 220, 30, 40 ) &&
             block->color( sTileSize / 2, sTileSize + sTileSize / 2 ) == qRgb( 20, 70, 230 ) &&
             raster.errors().isEmpty();
    } ) );
  }
  for ( std::future<bool> &render : renders )
    QVERIFY( render.get() );
  QCOMPARE( validationCache->validationAttemptCount(), 2ULL );

  QgsMtplRasterInterface canceledRaster( dataset, layer.tileMatrixSet(), {}, 0 );
  QgsRasterBlockFeedback canceledFeedback;
  canceledFeedback.cancel();
  std::unique_ptr<QgsRasterBlock> canceledBlock(
    canceledRaster.block( 1, matrix.extent(), 2 * sTileSize, sTileSize, &canceledFeedback ) );
  QVERIFY( canceledBlock );
  QVERIFY( canceledBlock->isValid() );
  QCOMPARE( qAlpha( canceledBlock->color( sTileSize / 2, sTileSize / 2 ) ), 0 );

  QgsMapSettings settings;
  settings.setLayers( QList<QgsMapLayer *>() << &layer );
  settings.setDestinationCrs( layer.crs() );
  settings.setTransformContext( QgsProject::instance()->transformContext() );
  settings.setOutputSize( QSize( 2 * sTileSize, sTileSize ) );
  settings.setOutputDpi( 96 );
  settings.setBackgroundColor( QColor( 0, 0, 0, 0 ) );
  settings.setOutputImageFormat( QImage::Format_ARGB32_Premultiplied );
  settings.setExtent( matrix.extent() );
  QVERIFY( settings.hasValidSettings() );

  QImage canceledImage( settings.outputSize(), QImage::Format_ARGB32_Premultiplied );
  canceledImage.fill( Qt::transparent );
  QPainter canceledPainter( &canceledImage );
  QgsRenderContext canceledContext = QgsRenderContext::fromMapSettings( settings );
  canceledContext.setPainter( &canceledPainter );
  std::unique_ptr<QgsMapLayerRenderer> canceledRenderer( layer.createMapRenderer( canceledContext ) );
  QVERIFY( canceledRenderer );
  QVERIFY( canceledRenderer->feedback() );
  canceledRenderer->feedback()->cancel();
  QVERIFY( !canceledRenderer->render() );
  canceledPainter.end();

  QVERIFY( QFile::remove( dataset.packages.constFirst().descriptor.path ) );
  settings.setFlag( Qgis::MapSettingsFlag::RenderPreviewJob, true );
  QgsMapRendererSequentialJob previewJob( settings );
  previewJob.start();
  previewJob.waitForFinished();
  QVERIFY( previewJob.errors().isEmpty() );

  settings.setFlag( Qgis::MapSettingsFlag::RenderPreviewJob, false );
  QgsMapRendererSequentialJob firstMainJob( settings );
  firstMainJob.start();
  firstMainJob.waitForFinished();
  QVERIFY( !firstMainJob.errors().isEmpty() );
  QStringList firstErrors;
  for ( const QgsMapRendererJob::Error &renderError : firstMainJob.errors() )
    firstErrors << renderError.message;
  QVERIFY2( firstErrors.join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "图层已降级：" ) ),
            qPrintable( firstErrors.join( QLatin1Char( '\n' ) ) ) );
  QVERIFY2( firstErrors.join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "已缺失" ) ),
            qPrintable( firstErrors.join( QLatin1Char( '\n' ) ) ) );

  QgsMapRendererSequentialJob repeatedMainJob( settings );
  repeatedMainJob.start();
  repeatedMainJob.waitForFinished();
  QVERIFY( repeatedMainJob.errors().isEmpty() );
}

void TestMtplLayer::structureValidationCacheRefreshesChangedSources()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );
  dataset.hasExtent = true;
  dataset.extentXMinimum = dataset.matrix.boundsXMinimum;
  dataset.extentYMinimum = dataset.matrix.boundsYMinimum;
  dataset.extentXMaximum = dataset.matrix.boundsXMaximum;
  dataset.extentYMaximum = dataset.matrix.boundsYMaximum;

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const QgsTileMatrix matrix = layer.tileMatrixSet().tileMatrix( 0 );
  QgsMapSettings settings;
  settings.setLayers( QList<QgsMapLayer *>() << &layer );
  settings.setDestinationCrs( layer.crs() );
  settings.setTransformContext( QgsProject::instance()->transformContext() );
  settings.setOutputSize( QSize( 2 * sTileSize, sTileSize ) );
  settings.setOutputDpi( 96 );
  settings.setBackgroundColor( QColor( 0, 0, 0, 0 ) );
  settings.setOutputImageFormat( QImage::Format_ARGB32_Premultiplied );
  settings.setExtent( matrix.extent() );
  QVERIFY( settings.hasValidSettings() );

  struct RenderResult
  {
    QImage image;
    QStringList errors;
  };
  const auto render = []( const QgsMapSettings &mapSettings )
  {
    QgsMapRendererSequentialJob job( mapSettings );
    job.start();
    job.waitForFinished();
    RenderResult result;
    result.image = job.renderedImage();
    for ( const QgsMapRendererJob::Error &error : job.errors() )
      result.errors.append( error.message );
    return result;
  };

  const RenderResult initial = render( settings );
  QVERIFY2( initial.errors.isEmpty(), qPrintable( initial.errors.join( QLatin1Char( '\n' ) ) ) );
  QVERIFY( hasColorNear( initial.image, QgsPointXY( sTileSize / 2.0, sTileSize / 2.0 ),
                         QColor( 220, 30, 40 ), 2, 5 ) );
  QCOMPARE( layer.structuralValidationAttemptCount(), 2ULL );

  std::unique_ptr<QgsMtplPluginLayer> clonedLayer( layer.clone() );
  QCOMPARE( clonedLayer->structuralValidationAttemptCount(), 2ULL );
  QgsMapSettings clonedSettings( settings );
  clonedSettings.setLayers( QList<QgsMapLayer *>() << clonedLayer.get() );
  const RenderResult cloned = render( clonedSettings );
  QVERIFY2( cloned.errors.isEmpty(), qPrintable( cloned.errors.join( QLatin1Char( '\n' ) ) ) );
  QCOMPARE( clonedLayer->structuralValidationAttemptCount(), 2ULL );
  QCOMPARE( layer.structuralValidationAttemptCount(), 2ULL );

  const RenderResult cached = render( settings );
  QVERIFY( cached.errors.isEmpty() );
  QCOMPARE( layer.structuralValidationAttemptCount(), 2ULL );

  const QString leftPath = dataset.packages.constFirst().descriptor.path;
  const qint64 initialModified = QFileInfo( leftPath ).lastModified().toMSecsSinceEpoch();
  QVERIFY( QFile::remove( leftPath ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              leftPath,
              sTileSize,
              tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 ),
              MTPL_STORAGE_PLAIN,
              QgsMtpl::CryptoKeys(),
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, solidPng( QColor( 30, 190, 70 ) ) } },
              error ),
            qPrintable( error ) );
  QFile updatedFile( leftPath );
  QVERIFY( updatedFile.open( QIODevice::ReadWrite ) );
  QVERIFY( updatedFile.setFileTime( QDateTime::fromMSecsSinceEpoch( initialModified + 2000, Qt::UTC ),
                                   QFileDevice::FileModificationTime ) );
  updatedFile.close();

  const RenderResult changed = render( settings );
  QVERIFY2( changed.errors.isEmpty(), qPrintable( changed.errors.join( QLatin1Char( '\n' ) ) ) );
  QVERIFY2( hasColorNear( changed.image, QgsPointXY( sTileSize / 2.0, sTileSize / 2.0 ),
                          QColor( 30, 190, 70 ), 2, 5 ),
            "A structurally compatible PTP replacement was not rendered after its file stamp changed." );
  QVERIFY( hasColorNear( changed.image, QgsPointXY( sTileSize + sTileSize / 2.0, sTileSize / 2.0 ),
                         QColor( 20, 70, 230 ), 2, 5 ) );
  QCOMPARE( layer.structuralValidationAttemptCount(), 3ULL );

  layer.reload();
  const RenderResult refreshed = render( settings );
  QVERIFY( refreshed.errors.isEmpty() );
  QCOMPARE( layer.structuralValidationAttemptCount(), 5ULL );

  const QString projectPath = directory.filePath( QStringLiteral( "updated-source-project.qgs" ) );
  QgsReadWriteContext projectContext;
  projectContext.setPathResolver( QgsPathResolver( projectPath ) );
  QDomDocument projectDocument( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = projectDocument.createElement( QStringLiteral( "maplayer" ) );
  projectDocument.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, projectDocument, projectContext ) );
  const QDomElement savedLeftPackage = mapLayerElement
                                          .firstChildElement( QStringLiteral( "mtpl-dataset" ) )
                                          .firstChildElement( QStringLiteral( "packages" ) )
                                          .firstChildElement( QStringLiteral( "package" ) );
  QVERIFY( !savedLeftPackage.isNull() );
  const QFileInfo compatibleFileInfo( leftPath );
  QCOMPARE( savedLeftPackage.attribute( QStringLiteral( "fileSize" ) ).toULongLong(),
            static_cast<qulonglong>( compatibleFileInfo.size() ) );
  QCOMPARE( savedLeftPackage.attribute( QStringLiteral( "lastModifiedMs" ) ).toLongLong(),
            compatibleFileInfo.lastModified().toMSecsSinceEpoch() );
  QVERIFY( !projectDocument.toByteArray().contains( "structureValidation" ) );

  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( projectDocument.documentElement(), projectContext ) );
  QVERIFY( restoredLayer.isValid() );
  QgsMapSettings restoredSettings( settings );
  restoredSettings.setLayers( QList<QgsMapLayer *>() << &restoredLayer );
  const RenderResult restored = render( restoredSettings );
  QVERIFY2( restored.errors.isEmpty(), qPrintable( restored.errors.join( QLatin1Char( '\n' ) ) ) );
  QVERIFY( hasColorNear( restored.image, QgsPointXY( sTileSize / 2.0, sTileSize / 2.0 ),
                         QColor( 30, 190, 70 ), 2, 5 ) );
  QCOMPARE( restoredLayer.structuralValidationAttemptCount(), 2ULL );

  const qint64 compatibleModified = QFileInfo( leftPath ).lastModified().toMSecsSinceEpoch();
  QVERIFY( QFile::remove( leftPath ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              leftPath,
              sTileSize,
              tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 ),
              MTPL_STORAGE_PLAIN,
              QgsMtpl::CryptoKeys(),
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, QByteArrayLiteral( "not-an-image" ) } },
              error ),
            qPrintable( error ) );
  QFile corruptTileFile( leftPath );
  QVERIFY( corruptTileFile.open( QIODevice::ReadWrite ) );
  QVERIFY( corruptTileFile.setFileTime( QDateTime::fromMSecsSinceEpoch( compatibleModified + 2000, Qt::UTC ),
                                       QFileDevice::FileModificationTime ) );
  corruptTileFile.close();

  const RenderResult corruptTile = render( settings );
  QVERIFY( !corruptTile.errors.isEmpty() );
  QVERIFY2( corruptTile.errors.join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "图层已降级：" ) ),
            qPrintable( corruptTile.errors.join( QLatin1Char( '\n' ) ) ) );
  const QImage corruptTileImage = corruptTile.image;
  QCOMPARE( corruptTileImage.pixelColor( sTileSize / 2, sTileSize / 2 ).alpha(), 0 );
  QVERIFY( hasColorNear( corruptTileImage, QgsPointXY( sTileSize + sTileSize / 2.0, sTileSize / 2.0 ),
                         QColor( 20, 70, 230 ), 2, 5 ) );
  QCOMPARE( layer.structuralValidationAttemptCount(), 6ULL );

  const RenderResult throttled = render( settings );
  QVERIFY( throttled.errors.isEmpty() );
  QCOMPARE( layer.structuralValidationAttemptCount(), 6ULL );

  const qint64 corruptTileModified = QFileInfo( leftPath ).lastModified().toMSecsSinceEpoch();
  QVERIFY( QFile::remove( leftPath ) );
  QByteArray incompatibleMetadata = tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 );
  incompatibleMetadata.replace( "logical-test-map", "incompatible-map" );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              leftPath,
              sTileSize,
              incompatibleMetadata,
              MTPL_STORAGE_PLAIN,
              QgsMtpl::CryptoKeys(),
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, solidPng( QColor( 30, 190, 70 ) ) } },
              error ),
            qPrintable( error ) );
  QFile incompatibleFile( leftPath );
  QVERIFY( incompatibleFile.open( QIODevice::ReadWrite ) );
  QVERIFY( incompatibleFile.setFileTime( QDateTime::fromMSecsSinceEpoch( corruptTileModified + 2000, Qt::UTC ),
                                        QFileDevice::FileModificationTime ) );
  incompatibleFile.close();

  const RenderResult incompatible = render( settings );
  QVERIFY( !incompatible.errors.isEmpty() );
  QVERIFY2( incompatible.errors.join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "图层已降级：" ) ),
            qPrintable( incompatible.errors.join( QLatin1Char( '\n' ) ) ) );
  QCOMPARE( incompatible.image.pixelColor( sTileSize / 2, sTileSize / 2 ).alpha(), 0 );
  QCOMPARE( layer.structuralValidationAttemptCount(), 7ULL );

  QDomDocument incompatibleProjectDocument( QStringLiteral( "qgis" ) );
  QDomElement incompatibleMapLayerElement = incompatibleProjectDocument.createElement( QStringLiteral( "maplayer" ) );
  incompatibleProjectDocument.appendChild( incompatibleMapLayerElement );
  QVERIFY( layer.writeLayerXml( incompatibleMapLayerElement, incompatibleProjectDocument, projectContext ) );
  const QDomElement incompatibleSavedPackage = incompatibleMapLayerElement
                                                 .firstChildElement( QStringLiteral( "mtpl-dataset" ) )
                                                 .firstChildElement( QStringLiteral( "packages" ) )
                                                 .firstChildElement( QStringLiteral( "package" ) );
  QVERIFY( !incompatibleSavedPackage.isNull() );
  QCOMPARE( incompatibleSavedPackage.attribute( QStringLiteral( "fileSize" ) ).toULongLong(),
            static_cast<qulonglong>( dataset.packages.constFirst().descriptor.fileSize ) );
  QCOMPARE( incompatibleSavedPackage.attribute( QStringLiteral( "lastModifiedMs" ) ).toLongLong(),
            dataset.packages.constFirst().fileLastModifiedMs );

  QgsMtplPluginLayer incompatibleRestoredLayer;
  QVERIFY( incompatibleRestoredLayer.readLayerXml( incompatibleProjectDocument.documentElement(), projectContext ) );
  QVERIFY( incompatibleRestoredLayer.isValid() );
  QgsMapSettings incompatibleRestoredSettings( settings );
  incompatibleRestoredSettings.setLayers( QList<QgsMapLayer *>() << &incompatibleRestoredLayer );
  const RenderResult incompatibleRestored = render( incompatibleRestoredSettings );
  QVERIFY( !incompatibleRestored.errors.isEmpty() );
  QVERIFY2( incompatibleRestored.errors.join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "图层已降级：" ) ),
            qPrintable( incompatibleRestored.errors.join( QLatin1Char( '\n' ) ) ) );
  const QImage incompatibleRestoredImage = incompatibleRestored.image;
  QCOMPARE( incompatibleRestoredImage.pixelColor( sTileSize / 2, sTileSize / 2 ).alpha(), 0 );
  QVERIFY( hasColorNear( incompatibleRestoredImage,
                         QgsPointXY( sTileSize + sTileSize / 2.0, sTileSize / 2.0 ),
                         QColor( 20, 70, 230 ), 2, 5 ) );
}

void TestMtplLayer::projectRestoredSourceContractRejectsMutation_data()
{
  QTest::addColumn<QString>( "mutation" );
  QTest::addColumn<bool>( "accepted" );
  QTest::addColumn<bool>( "displayOverride" );

  QTest::newRow( "zoom-range-removed" ) << QStringLiteral( "zoom-range-removed" ) << false << false;
  QTest::newRow( "zoom-range-changed" ) << QStringLiteral( "zoom-range-changed" ) << false << false;
  QTest::newRow( "matrix-removed" ) << QStringLiteral( "matrix-removed" ) << false << false;
  QTest::newRow( "matrix-method-changed" ) << QStringLiteral( "matrix-method-changed" ) << false << false;
  QTest::newRow( "bounds-removed" ) << QStringLiteral( "bounds-removed" ) << false << false;
  QTest::newRow( "bounds-changed" ) << QStringLiteral( "bounds-changed" ) << false << false;
  QTest::newRow( "map-id-removed" ) << QStringLiteral( "map-id-removed" ) << false << false;
  QTest::newRow( "map-id-changed" ) << QStringLiteral( "map-id-changed" ) << false << false;
  QTest::newRow( "payload-removed" ) << QStringLiteral( "payload-removed" ) << false << false;
  QTest::newRow( "payload-changed" ) << QStringLiteral( "payload-changed" ) << false << false;
  QTest::newRow( "display-scheme-override" ) << QStringLiteral( "display-scheme-override" ) << true << true;
}

void TestMtplLayer::projectRestoredSourceContractRejectsMutation()
{
  QFETCH( QString, mutation );
  QFETCH( bool, accepted );
  QFETCH( bool, displayOverride );

  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor fixtureDataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), fixtureDataset, error ), qPrintable( error ) );

  QByteArray sourceMetadata = tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 );
  sourceMetadata.replace( "minimum_zoom", "min_zoom" );
  sourceMetadata.replace( "maximum_zoom", "max_zoom" );
  for ( int packageIndex = 0; packageIndex < fixtureDataset.packages.size(); ++packageIndex )
  {
    const QgsMtpl::TileDatasetPackage &package = fixtureDataset.packages.at( packageIndex );
    const QColor color = packageIndex == 0 ? QColor( 220, 30, 40 ) : QColor( 20, 70, 230 );
    QVERIFY( QFile::remove( package.descriptor.path ) );
    QVERIFY2( QgsMtplTest::writePtpFixture(
                package.descriptor.path,
                sTileSize,
                sourceMetadata,
                MTPL_STORAGE_PLAIN,
                QgsMtpl::CryptoKeys(),
                { { 0, package.address.packageX, package.address.packageX, 0, 0 } },
                { { 0, package.address.packageX, 0, solidPng( color ) } },
                error ),
              qPrintable( error ) );
  }

  const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
    directory.path(), QgsMtpl::CryptoKeys(), true, QgsMtpl::CredentialSource::None );
  QVERIFY2( probe.ok, qPrintable( probe.error ) );
  QCOMPARE( probe.packages.size(), 2 );
  QgsMtpl::TileDatasetBuildOptions buildOptions;
  buildOptions.ruleSnapshot = partitionRule(
    0, QStringLiteral( "test.source-contract.rule" ), QStringLiteral( "来源契约测试规则" ) );
  const QgsMtpl::TileDatasetBuildResult build = QgsMtpl::buildTileDataset(
    directory.path(), true, probe.packages, buildOptions );
  QVERIFY2( build.ok(), qPrintable( build.errorString() ) );
  QgsMtpl::TileDatasetDescriptor dataset = build.dataset;
  QCOMPARE( dataset.packages.size(), 2 );
  for ( QgsMtpl::TileDatasetPackage &package : dataset.packages )
  {
    QVERIFY( package.sourceContract.available );
    QVERIFY( package.sourceContract.zoomRangeExplicit );
    QVERIFY( package.sourceContract.hasBounds );
    QVERIFY( package.sourceContract.mapIdDeclared );
    QVERIFY( package.sourceContract.payloadDeclared );
    if ( displayOverride )
    {
      package.descriptor.displayOverridesApplied = true;
      package.descriptor.crsAuthId = dataset.matrix.crsAuthId;
      package.descriptor.scheme = QStringLiteral( "xyz" );
    }
  }

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const QString projectPath = directory.filePath( QStringLiteral( "source-contract-project.qgs" ) );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( projectPath ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, document, context ) );
  const QByteArray xml = document.toByteArray( 2 );
  QCOMPARE( mapLayerElement.elementsByTagName( QStringLiteral( "source-contract" ) ).size(), 2 );
  QVERIFY( !xml.contains( "_mtpl" ) );
  QVERIFY( !xml.contains( "package-private-secret" ) );
  QVERIFY( !xml.contains( "package-nested-secret" ) );
  QFile projectFile( projectPath );
  QVERIFY( projectFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
  QCOMPARE( projectFile.write( xml ), static_cast<qint64>( xml.size() ) );
  projectFile.close();

  const QString changedPath = dataset.packages.constFirst().descriptor.path;
  const qint64 savedModified = QFileInfo( changedPath ).lastModified().toMSecsSinceEpoch();
  QJsonDocument metadataDocument = QJsonDocument::fromJson( sourceMetadata );
  QVERIFY( metadataDocument.isObject() );
  QJsonObject metadata = metadataDocument.object();
  QJsonObject matrix = metadata.value( QStringLiteral( "mtpl_tile_matrix" ) ).toObject();
  QVERIFY( !matrix.isEmpty() );
  if ( mutation == QLatin1String( "zoom-range-removed" ) )
  {
    matrix.remove( QStringLiteral( "min_zoom" ) );
    matrix.remove( QStringLiteral( "max_zoom" ) );
    metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), matrix );
  }
  else if ( mutation == QLatin1String( "zoom-range-changed" ) )
  {
    matrix.insert( QStringLiteral( "max_zoom" ), 1 );
    metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), matrix );
  }
  else if ( mutation == QLatin1String( "matrix-removed" ) )
  {
    metadata.remove( QStringLiteral( "mtpl_tile_matrix" ) );
  }
  else if ( mutation == QLatin1String( "matrix-method-changed" ) )
  {
    matrix.insert( QStringLiteral( "scale_to_zoom_method" ), QStringLiteral( "esri" ) );
    metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), matrix );
  }
  else if ( mutation == QLatin1String( "bounds-removed" ) )
  {
    matrix.remove( QStringLiteral( "bounds" ) );
    matrix.remove( QStringLiteral( "bounds_crs" ) );
    metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), matrix );
  }
  else if ( mutation == QLatin1String( "bounds-changed" ) )
  {
    matrix.insert( QStringLiteral( "bounds" ), QJsonArray { -170.0, -80.0, 0.0, 80.0 } );
    metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), matrix );
  }
  else if ( mutation == QLatin1String( "map-id-removed" ) )
  {
    metadata.remove( QStringLiteral( "map_id" ) );
  }
  else if ( mutation == QLatin1String( "map-id-changed" ) )
  {
    metadata.insert( QStringLiteral( "map_id" ), QStringLiteral( "changed-map" ) );
  }
  else if ( mutation == QLatin1String( "payload-removed" ) )
  {
    metadata.remove( QStringLiteral( "tile_file_ext" ) );
  }
  else if ( mutation == QLatin1String( "payload-changed" ) )
  {
    metadata.insert( QStringLiteral( "tile_file_ext" ), QStringLiteral( "elevation" ) );
  }
  else if ( mutation == QLatin1String( "display-scheme-override" ) )
  {
    matrix.insert( QStringLiteral( "scheme" ), QStringLiteral( "tms" ) );
    metadata.insert( QStringLiteral( "mtpl_tile_matrix" ), matrix );
  }
  else
  {
    QFAIL( "Unhandled source contract mutation." );
  }

  QVERIFY( QFile::remove( changedPath ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              changedPath,
              sTileSize,
              QJsonDocument( metadata ).toJson( QJsonDocument::Compact ),
              MTPL_STORAGE_PLAIN,
              QgsMtpl::CryptoKeys(),
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, solidPng( QColor( 220, 30, 40 ) ) } },
              error ),
            qPrintable( error ) );
  QFile changedFile( changedPath );
  QVERIFY( changedFile.open( QIODevice::ReadWrite ) );
  QVERIFY( changedFile.setFileTime(
    QDateTime::fromMSecsSinceEpoch( savedModified + 2000, Qt::UTC ),
    QFileDevice::FileModificationTime ) );
  changedFile.close();
  QCOMPARE( QFileInfo( changedPath ).lastModified().toMSecsSinceEpoch(), savedModified + 2000 );

  QVERIFY( projectFile.open( QIODevice::ReadOnly ) );
  QDomDocument restoredDocument;
  QVERIFY( restoredDocument.setContent( &projectFile ) );
  projectFile.close();
  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( restoredDocument.documentElement(), context ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.tileDataset() );
  const QgsTileMatrix renderMatrix = restoredLayer.tileMatrixSet().tileMatrix( 0 );
  QgsMtplRasterInterface raster(
    *restoredLayer.tileDataset(), restoredLayer.tileMatrixSet(), {}, 0 );
  std::unique_ptr<QgsRasterBlock> block(
    raster.block( 1, renderMatrix.extent(), 2 * sTileSize, sTileSize ) );
  QVERIFY( block );
  QVERIFY( block->isValid() );
  const QString diagnostics = raster.errors().join( QLatin1Char( '\n' ) );
  if ( accepted )
  {
    QVERIFY2( diagnostics.isEmpty(), qPrintable( diagnostics ) );
    QCOMPARE( block->color( sTileSize / 2, sTileSize / 2 ), qRgb( 220, 30, 40 ) );
  }
  else
  {
    QVERIFY2( diagnostics.contains( QStringLiteral( "图层已降级：" ) ), qPrintable( diagnostics ) );
    QVERIFY2( diagnostics.contains( QStringLiteral( "快照不一致" ) ), qPrintable( diagnostics ) );
    QCOMPARE( qAlpha( block->color( sTileSize / 2, sTileSize / 2 ) ), 0 );
  }
  QCOMPARE( block->color( sTileSize / 2, sTileSize + sTileSize / 2 ), qRgb( 20, 70, 230 ) );
}

void TestMtplLayer::runtimeRejectsNonWhitelistedPtpTile()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );

  const QByteArray pngTile = solidPng( QColor( 25, 170, 70 ) );
  QVERIFY( !pngTile.isEmpty() );
  QImage bmpImage( sTileSize, sTileSize, QImage::Format_RGB32 );
  bmpImage.fill( qRgb( 190, 40, 150 ) );
  QByteArray bmpTile;
  QBuffer bmpBuffer( &bmpTile );
  QVERIFY( bmpBuffer.open( QIODevice::WriteOnly ) );
  QVERIFY( bmpImage.save( &bmpBuffer, "BMP" ) );

  const QString packagePath = directory.filePath( QStringLiteral( "0-7-0-0-0.ptp" ) );
  QString error;
  QVERIFY2( QgsMtplTest::writePtpFixture(
              packagePath,
              sTileSize,
              QByteArrayLiteral( "{\"tile_file_ext\":\"png\"}" ),
              MTPL_STORAGE_PLAIN,
              {},
              { { 1, 0, 1, 0, 0 } },
              { { 1, 0, 0, pngTile }, { 1, 1, 0, bmpTile } },
              error ),
            qPrintable( error ) );

  const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
    directory.path(), {}, true, QgsMtpl::CredentialSource::None );
  QVERIFY2( probe.ok, qPrintable( probe.error ) );
  QCOMPARE( probe.packages.size(), 1 );
  QVERIFY( probe.packages.constFirst().isReady() );
  QCOMPARE( probe.packages.constFirst().payload, QgsMtpl::PayloadType::RasterImage );

  const QgsMtpl::TileDatasetBuildResult build = QgsMtpl::buildTileDataset(
    directory.path(), true, probe.packages );
  QVERIFY2( build.ok(), qPrintable( build.errorString() ) );
  QgsMtplPluginLayer layer( build.dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const QgsTileMatrix matrix = layer.tileMatrixSet().tileMatrix( 1 );
  QCOMPARE( matrix.zoomLevel(), 1 );

  QgsMtplRasterInterface raster( build.dataset, layer.tileMatrixSet(), {}, 1 );
  QgsRectangle topRowExtent = matrix.tileExtent( QgsTileXYZ( 0, 0, 1 ) );
  topRowExtent.combineExtentWith( matrix.tileExtent( QgsTileXYZ( 1, 0, 1 ) ) );
  std::unique_ptr<QgsRasterBlock> block(
    raster.block( 1, topRowExtent, 2 * sTileSize, sTileSize ) );
  QVERIFY( block );
  QVERIFY( block->isValid() );
  QCOMPARE( block->color( sTileSize / 2, sTileSize / 2 ), qRgb( 25, 170, 70 ) );
  QCOMPARE( qAlpha( block->color( sTileSize / 2, sTileSize + sTileSize / 2 ) ), 0 );
  const QString diagnostics = raster.errors().join( QLatin1Char( '\n' ) );
  QVERIFY2( diagnostics.contains( QStringLiteral( "图层已降级：" ) ), qPrintable( diagnostics ) );
  QVERIFY2( diagnostics.contains( QStringLiteral( "PNG、JPEG 或 WebP" ) ), qPrintable( diagnostics ) );
}

void TestMtplLayer::rasterInterfaceRejectsExcessiveTileRequest()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );

  constexpr quint64 hugeRootSize = 1000ULL;
  dataset.matrix.z0MatrixWidth = hugeRootSize;
  dataset.matrix.z0MatrixHeight = hugeRootSize;
  dataset.matrix.hasBounds = false;
  dataset.matrix.boundsCrsAuthId.clear();

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const QgsTileMatrix matrix = layer.tileMatrixSet().tileMatrix( 0 );
  QCOMPARE( matrix.matrixWidth(), static_cast<int>( hugeRootSize ) );
  QCOMPARE( matrix.matrixHeight(), static_cast<int>( hugeRootSize ) );

  QgsMtplRasterInterface raster( dataset, layer.tileMatrixSet(), {}, 0 );
  QElapsedTimer timer;
  timer.start();
  std::unique_ptr<QgsRasterBlock> block( raster.block( 1, matrix.extent(), 16, 16 ) );
  const qint64 elapsedMs = timer.elapsed();

  QVERIFY( block );
  QVERIFY( block->isValid() );
  QCOMPARE( block->width(), 16 );
  QCOMPARE( block->height(), 16 );
  for ( int row = 0; row < block->height(); ++row )
  {
    for ( int column = 0; column < block->width(); ++column )
      QCOMPARE( qAlpha( block->color( row, column ) ), 0 );
  }
  QVERIFY2( elapsedMs < 2000, qPrintable( QStringLiteral( "Excessive tile request took %1 ms." ).arg( elapsedMs ) ) );
  QVERIFY( !raster.errors().isEmpty() );
  QVERIFY( raster.errors().size() <= 16 );
  QVERIFY2( raster.errors().join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "图层已降级：" ) ),
            qPrintable( raster.errors().join( QLatin1Char( '\n' ) ) ) );
  QVERIFY2( raster.errors().join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "请求的瓦片数量过大" ) ),
            qPrintable( raster.errors().join( QLatin1Char( '\n' ) ) ) );
}

void TestMtplLayer::eightyOnePackagesRenderAcrossReaderLimit()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  constexpr int gridSize = 9;
  constexpr int packageCount = gridSize * gridSize;
  const QgsMtpl::TileMatrixDefinition matrix = customMatrixDefinition(
    QStringLiteral( "EPSG:3857" ), QgsMtpl::TileScheme::Xyz, 0,
    0.0, 9000.0, 1000.0, gridSize, gridSize );
  QByteArray metadata = customTileMetadata(
    matrix.crsAuthId, matrix.scheme, 0, matrix.topLeftX, matrix.topLeftY,
    matrix.z0TileSpan, gridSize, gridSize );
  metadata.replace( "\"minimum_zoom\"", "\"min_zoom\"" );
  metadata.replace( "\"maximum_zoom\"", "\"max_zoom\"" );
  QgsMtpl::TileDatasetDescriptor dataset;
  dataset.sourcePath = directory.path();
  dataset.directorySource = true;
  dataset.partitionRule = partitionRule( 0 );
  dataset.matrix = matrix;
  dataset.minimumZoom = 0;
  dataset.maximumZoom = 0;
  dataset.payload = QgsMtpl::PayloadType::RasterImage;
  dataset.mapId = QStringLiteral( "logical-test-map" );
  QImage expected( gridSize * sTileSize, gridSize * sTileSize, QImage::Format_ARGB32_Premultiplied );
  QPainter expectedPainter( &expected );
  QString error;
  for ( int row = 0; row < gridSize; ++row )
  {
    for ( int column = 0; column < gridSize; ++column )
    {
      const QColor color( 20 + column * 24, 20 + row * 24, 40 + ( column + row ) * 8 );
      expectedPainter.fillRect( column * sTileSize, row * sTileSize, sTileSize, sTileSize, color );
      const QgsMtpl::TilePackageAddress address {
        0, 0, 0, static_cast<quint32>( column ), static_cast<quint32>( row )
      };
      const QString path = directory.filePath( address.fileName() );
      const uint32_t x = static_cast<uint32_t>( column );
      const uint32_t y = static_cast<uint32_t>( row );
      QVERIFY2( QgsMtplTest::writePtpFixture(
                  path, sTileSize, metadata, MTPL_STORAGE_PLAIN, QgsMtpl::CryptoKeys(),
                  { { 0, x, x, y, y } }, { { 0, x, y, solidPng( color ) } }, error ), qPrintable( error ) );
      dataset.packageIndexByAddress.insert( address.key(), static_cast<int>( dataset.packages.size() ) );
      dataset.packages.append( customDatasetPackage(
        path, address, { rangeRecord( 0, x, x, y, y ) }, 0, 0, matrix ) );
    }
  }
  expectedPainter.end();
  QCOMPARE( dataset.packages.size(), packageCount );
  QVERIFY2( dataset.isValid( &error ), qPrintable( error ) );
  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  const auto validationCache = std::make_shared<QgsMtplStructureValidationCache>();
  QgsMtplRasterInterface raster( dataset, layer.tileMatrixSet(), {}, 0, validationCache );
  for ( int pass = 0; pass < 3; ++pass )
  {
    std::unique_ptr<QgsRasterBlock> block(
      raster.block( 1, layer.extent(), expected.width(), expected.height() ) );
    QVERIFY( block->isValid() );
    // Compare every pixel, including tile boundaries and packages evicted
    // from the 64-reader LRU before the following block request.
    QCOMPARE( block->image(), expected );
    QVERIFY2( raster.errors().isEmpty(), qPrintable( raster.errors().join( QLatin1Char( '\n' ) ) ) );
    QCOMPARE( validationCache->validationAttemptCount(), static_cast<quint64>( packageCount ) );
  }

  QgsMapSettings settings;
  settings.setLayers( { &layer } );
  settings.setDestinationCrs( layer.crs() );
  settings.setTransformContext( QgsProject::instance()->transformContext() );
  settings.setOutputSize( expected.size() );
  settings.setOutputDpi( 96 );
  settings.setBackgroundColor( QColor( 0, 0, 0, 0 ) );
  settings.setOutputImageFormat( QImage::Format_ARGB32_Premultiplied );
  settings.setExtent( layer.extent() );
  for ( int pass = 0; pass < 2; ++pass )
  {
    QgsMapRendererSequentialJob job( settings );
    job.start();
    job.waitForFinished();
    QVERIFY2( job.errors().isEmpty(),
              qPrintable( job.errors().isEmpty() ? QString() : job.errors().constFirst().message ) );
    QCOMPARE( job.renderedImage(), expected );
    QCOMPARE( layer.structuralValidationAttemptCount(), static_cast<quint64>( packageCount ) );
  }
}

void TestMtplLayer::legacyPackageXmlRoundTrip()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );

  QgsMtpl::CryptoKeys syntheticKeys = QgsMtplTest::fixtureKeys();
  QgsMtpl::PackageDescriptor descriptor = dataset.packages.constFirst().descriptor;
  descriptor.displayName = QStringLiteral( "Saved single-package layer" );
  descriptor.hasExtent = true;
  descriptor.extentXMinimum = -170.0;
  descriptor.extentYMinimum = -80.0;
  descriptor.extentXMaximum = -10.0;
  descriptor.extentYMaximum = 80.0;
  descriptor.stylePath = directory.filePath( QStringLiteral( "saved-style.json" ) );
  descriptor.sidecarPath = directory.filePath( QStringLiteral( "saved-keys.json" ) );
  descriptor.keyId = QStringLiteral( "saved-key-reference" );
  descriptor.scale = 2.0;
  descriptor.offset = 3.0;
  descriptor.hasNoData = true;
  descriptor.noData = -9999.0;
  QgsMtplPluginLayer layer( descriptor, syntheticKeys );
  QVERIFY( layer.isValid() );
  QVERIFY( !layer.isTileDataset() );

  const QString projectPath = directory.filePath( QStringLiteral( "legacy-project.qgs" ) );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( projectPath ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, document, context ) );

  const QByteArray xml = document.toByteArray( 2 );
  QVERIFY( xml.contains( "<mtpl-package" ) );
  QVERIFY( !xml.contains( "<mtpl-dataset" ) );
  QVERIFY( !xml.contains( syntheticKeys.privateKey ) );
  QVERIFY( !xml.contains( syntheticKeys.deviceKey ) );

  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( document.documentElement(), context ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.isTileDataset() );
  QVERIFY( restoredLayer.tileDataset()->compatibilityMode );
  QVERIFY( !restoredLayer.tileDataset()->directorySource );
  QCOMPARE( restoredLayer.tileDataset()->packages.size(), 1 );
  QCOMPARE( restoredLayer.tileDataset()->minimumZoom, 0 );
  QCOMPARE( restoredLayer.tileDataset()->maximumZoom, 0 );
  QCOMPARE( restoredLayer.name(), layer.name() );
  QCOMPARE( restoredLayer.extent(), layer.extent() );
  QCOMPARE( restoredLayer.descriptor().stylePath, descriptor.stylePath );
  QCOMPARE( restoredLayer.descriptor().sidecarPath, descriptor.sidecarPath );
  QCOMPARE( restoredLayer.descriptor().keyId, descriptor.keyId );
  QCOMPARE( restoredLayer.descriptor().scale, descriptor.scale );
  QCOMPARE( restoredLayer.descriptor().offset, descriptor.offset );
  QCOMPARE( restoredLayer.descriptor().hasNoData, descriptor.hasNoData );
  QCOMPARE( restoredLayer.descriptor().noData, descriptor.noData );
  QCOMPARE( QDir::cleanPath( restoredLayer.descriptor().path ),
            QDir::cleanPath( dataset.packages.constFirst().descriptor.path ) );
  QCOMPARE( restoredLayer.descriptor().payload, QgsMtpl::PayloadType::RasterImage );

  QDomDocument migratedDocument( QStringLiteral( "qgis" ) );
  QDomElement migratedElement = migratedDocument.createElement( QStringLiteral( "maplayer" ) );
  migratedDocument.appendChild( migratedElement );
  QVERIFY( restoredLayer.writeLayerXml( migratedElement, migratedDocument, context ) );
  const QByteArray migratedXml = migratedDocument.toByteArray();
  QVERIFY( migratedXml.contains( "<mtpl-dataset" ) );
  QVERIFY( !migratedXml.contains( "<mtpl-package" ) );
  QVERIFY( !migratedXml.contains( syntheticKeys.privateKey ) );
  QVERIFY( !migratedXml.contains( syntheticKeys.deviceKey ) );
  QgsMtplPluginLayer reopenedLayer;
  QVERIFY( reopenedLayer.readLayerXml( migratedDocument.documentElement(), context ) );
  QVERIFY( reopenedLayer.isValid() );
  QVERIFY( reopenedLayer.tileDataset()->compatibilityMode );
  QCOMPARE( reopenedLayer.tileDataset()->packages.size(), 1 );
  QCOMPARE( reopenedLayer.extent(), layer.extent() );
  QCOMPARE( reopenedLayer.name(), layer.name() );
  QCOMPARE( reopenedLayer.descriptor().sidecarPath, descriptor.sidecarPath );
  QCOMPARE( reopenedLayer.descriptor().keyId, descriptor.keyId );
  QCOMPARE( reopenedLayer.descriptor().stylePath, descriptor.stylePath );
  QCOMPARE( reopenedLayer.descriptor().scale, descriptor.scale );
  QCOMPARE( reopenedLayer.descriptor().offset, descriptor.offset );
  QCOMPARE( reopenedLayer.descriptor().hasNoData, descriptor.hasNoData );
  QCOMPARE( reopenedLayer.descriptor().noData, descriptor.noData );

  QgsMtpl::PackageDescriptor directoryPackage;
  QVERIFY2( QgsMtplPackageService::probePackage( descriptor.path, directoryPackage, error ), qPrintable( error ) );
  QgsMtpl::TileDatasetBuildOptions directoryOptions;
  directoryOptions.singleFileCompatibility = true;
  directoryOptions.ruleSnapshot = dataset.partitionRule;
  const QgsMtpl::TileDatasetBuildResult directoryResult = QgsMtpl::buildTileDataset(
    directory.path(), true, { directoryPackage }, directoryOptions );
  QVERIFY2( directoryResult.ok(), qPrintable( directoryResult.errorString() ) );
  QVERIFY( !directoryResult.dataset.compatibilityMode );
  syntheticKeys.clear();
}

void TestMtplLayer::legacyPackageXmlRestoresRememberedKeys()
{
  ScopedPluginSettingsRestore settingsRestore( {
    QStringLiteral( "mtpl/credentials/authcfg" ),
    QStringLiteral( "mtpl/credentials/remember" ),
    QStringLiteral( "mtpl/credentials/privateKey" ),
    QStringLiteral( "mtpl/credentials/deviceKey" ),
    QStringLiteral( "mtpl/lastPath" )
  } );

  ScopedMemoryCredentialBackend credentialBackend;
  QgsMtplCredentialStore::clear();
  QgsMtpl::CryptoKeys syntheticKeys = QgsMtplTest::fixtureKeys();
  QString error;
  QVERIFY2( QgsMtplCredentialStore::saveManualKeys( syntheticKeys, QString(), error ), qPrintable( error ) );
  credentialBackend.resetLoadCounts();

  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QVERIFY2( createEncryptedAggregateDataset( directory.path(), syntheticKeys, dataset, error ), qPrintable( error ) );

  QgsMtpl::PackageDescriptor descriptor = dataset.packages.constFirst().descriptor;
  descriptor.sidecarPath.clear();
  descriptor.keyId.clear();
  QgsMtplPluginLayer layer( descriptor, syntheticKeys );
  QVERIFY( layer.isValid() );

  const QString projectPath = directory.filePath( QStringLiteral( "legacy-encrypted-project.qgs" ) );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( projectPath ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, document, context ) );

  const QByteArray xml = document.toByteArray( 2 );
  QVERIFY( xml.contains( "<mtpl-package" ) );
  QVERIFY( !xml.contains( syntheticKeys.privateKey ) );
  QVERIFY( !xml.contains( syntheticKeys.deviceKey ) );

  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( document.documentElement(), context ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.isTileDataset() );
  QCOMPARE( restoredLayer.tileDataset()->packages.size(), 1 );
  QVERIFY( restoredLayer.hasCryptoKeys() );
  QCOMPARE( restoredLayer.descriptor().encryption, QgsMtpl::EncryptionState::Encrypted );
  QCOMPARE( restoredLayer.descriptor().readiness, QgsMtpl::ReadinessState::KeyVerified );
  QCOMPARE( restoredLayer.descriptor().credentialSource, QgsMtpl::CredentialSource::Remembered );
  QCOMPARE( credentialBackend.unlockAllowedLoadCount, 0 );
  QCOMPARE( credentialBackend.silentLoadCount, 1 );

  credentialBackend.setUnlocked( false );
  credentialBackend.resetLoadCounts();
  QgsMtplPluginLayer lockedLayer;
  QVERIFY( lockedLayer.readLayerXml( document.documentElement(), context ) );
  QVERIFY( lockedLayer.isValid() );
  QVERIFY( lockedLayer.isTileDataset() );
  QVERIFY( !lockedLayer.hasCryptoKeys() );
  QCOMPARE( lockedLayer.descriptor().encryption, QgsMtpl::EncryptionState::Locked );
  QCOMPARE( lockedLayer.descriptor().readiness, QgsMtpl::ReadinessState::KeyRequired );
  QCOMPARE( credentialBackend.unlockAllowedLoadCount, 0 );
  QCOMPARE( credentialBackend.silentLoadCount, 1 );
  QgsMtplRasterInterface lockedRaster( *lockedLayer.tileDataset(), lockedLayer.tileMatrixSet(), {}, 0 );
  std::unique_ptr<QgsRasterBlock> lockedBlock(
    lockedRaster.block( 1, lockedLayer.extent(), 2 * sTileSize, sTileSize ) );
  QVERIFY( lockedBlock->isValid() );
  QCOMPARE( qAlpha( lockedBlock->color( sTileSize / 2, sTileSize / 2 ) ), 0 );
  QVERIFY( lockedRaster.errors().join( QLatin1Char( '\n' ) ).contains( QStringLiteral( "需要匹配的密钥" ) ) );
  syntheticKeys.clear();
}

void TestMtplLayer::legacyPackageXmlPixelReference_data()
{
  QTest::addColumn<QString>( "destinationAuthId" );
  QTest::addColumn<double>( "rotation" );
  QTest::addColumn<QString>( "savedScheme" );
  QTest::newRow( "rotated-mercator" ) << QStringLiteral( "EPSG:3857" ) << 31.0 << QStringLiteral( "xyz" );
  QTest::newRow( "reprojected-rotated-geographic" ) << QStringLiteral( "EPSG:4326" ) << 23.0 << QStringLiteral( "xyz" );
  QTest::newRow( "saved-tms-override" ) << QStringLiteral( "EPSG:4326" ) << 23.0 << QStringLiteral( "tms" );
}

void TestMtplLayer::legacyPackageXmlPixelReference()
{
  QFETCH( QString, destinationAuthId );
  QFETCH( double, rotation );
  QFETCH( QString, savedScheme );
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  // A legacy single package must not acquire a partition-address requirement.
  const QString path = directory.filePath( QStringLiteral( "0-7-0-99-99.ptp" ) );
  QImage sourceImage( sTileSize, sTileSize, QImage::Format_ARGB32 );
  for ( int row = 0; row < sTileSize; ++row )
  {
    for ( int column = 0; column < sTileSize; ++column )
      sourceImage.setPixelColor( column, row, QColor( 10 + column * 7, 10 + row * 7, 80 ) );
  }
  QByteArray tileBytes;
  QBuffer buffer( &tileBytes );
  QVERIFY( buffer.open( QIODevice::WriteOnly ) );
  QVERIFY( sourceImage.save( &buffer, "PNG" ) );
  buffer.close();
  QString error;
  QVERIFY2( QgsMtplTest::writePtpFixture(
              path, sTileSize,
              QByteArrayLiteral( "{\"tile_file_ext\":\"png\",\"crs\":\"EPSG:3857\",\"scheme\":\"xyz\"}" ),
              MTPL_STORAGE_PLAIN, QgsMtpl::CryptoKeys(),
              { { 2, 1, 1, 1, 1 } }, { { 2, 1, 1, tileBytes } }, error ), qPrintable( error ) );
  QgsMtpl::PackageDescriptor descriptor;
  QVERIFY2( QgsMtplPackageService::probePackage( path, descriptor, error ), qPrintable( error ) );
  descriptor.scheme = savedScheme;
  const int displayRow = savedScheme == QLatin1String( "tms" ) ? 2 : 1;
  const QgsRectangle tileExtent = QgsTileMatrix::fromWebMercator( 2 ).tileExtent( QgsTileXYZ( 1, displayRow, 2 ) );
  descriptor.hasExtent = true;
  descriptor.extentXMinimum = tileExtent.xMinimum();
  descriptor.extentYMinimum = tileExtent.yMinimum();
  descriptor.extentXMaximum = tileExtent.xMaximum();
  descriptor.extentYMaximum = tileExtent.yMaximum();
  QgsMtplPluginLayer legacyLayer( descriptor );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( directory.filePath( QStringLiteral( "legacy.qgs" ) ) ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement element = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( element );
  QVERIFY( legacyLayer.writeLayerXml( element, document, context ) );
  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( element, context ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.isTileDataset() );
  QVERIFY( restoredLayer.tileDataset()->compatibilityMode );
  QCOMPARE( restoredLayer.tileDataset()->packages.size(), 1 );
  QCOMPARE( restoredLayer.tileDataset()->minimumZoom, 2 );
  QCOMPARE( restoredLayer.tileDataset()->maximumZoom, 2 );
  QCOMPARE( restoredLayer.descriptor().scheme, savedScheme );
  QCOMPARE( restoredLayer.extent(), tileExtent );

  const QgsCoordinateReferenceSystem destinationCrs( destinationAuthId );
  QgsCoordinateTransform transform( restoredLayer.crs(), destinationCrs, QgsProject::instance()->transformContext() );
  QgsRectangle viewExtent = transform.transformBoundingBox( tileExtent );
  viewExtent.scale( 1.5 );
  QgsMapSettings settings;
  settings.setLayers( { &restoredLayer } );
  settings.setDestinationCrs( destinationCrs );
  settings.setTransformContext( QgsProject::instance()->transformContext() );
  settings.setOutputSize( QSize( 640, 640 ) );
  settings.setOutputDpi( 96 );
  settings.setBackgroundColor( QColor( 0, 0, 0, 0 ) );
  settings.setOutputImageFormat( QImage::Format_ARGB32_Premultiplied );
  settings.setExtent( viewExtent );
  settings.setRotation( rotation );
  QgsMapRendererSequentialJob job( settings );
  job.start();
  job.waitForFinished();
  QVERIFY2( job.errors().isEmpty(),
            qPrintable( job.errors().isEmpty() ? QString() : job.errors().constFirst().message ) );
  const QImage rendered = job.renderedImage();
  QVERIFY( opaquePixelCount( rendered ) > 1000 );
  for ( const int row : { 6, 16, 26 } )
  {
    for ( const int column : { 6, 16, 26 } )
    {
      const QgsPointXY sourcePoint( tileExtent.xMinimum() + ( column + 0.5 ) * tileExtent.width() / sTileSize,
                                    tileExtent.yMaximum() - ( row + 0.5 ) * tileExtent.height() / sTileSize );
      const QgsPointXY devicePoint = settings.mapToPixel().transform( transform.transform( sourcePoint ) );
      QVERIFY2( hasColorNear( rendered, devicePoint, sourceImage.pixelColor( column, row ), 2, 15 ),
                qPrintable( QStringLiteral( "Restored legacy PTP pixel %1/%2 missed its projected reference." )
                              .arg( column ).arg( row ) ) );
    }
  }
}

void TestMtplLayer::legacyPackageXmlBadImages_data()
{
  QTest::addColumn<QString>( "imageCase" );
  QTest::newRow( "damaged-png" ) << QStringLiteral( "damaged-png" );
  QTest::newRow( "wrong-image-size" ) << QStringLiteral( "wrong-image-size" );
  QTest::newRow( "truncated-png-tail" ) << QStringLiteral( "truncated-png-tail" );
  QTest::newRow( "truncated-jpeg-eoi" ) << QStringLiteral( "truncated-jpeg-eoi" );
  QTest::newRow( "truncated-jpeg-scan" ) << QStringLiteral( "truncated-jpeg-scan" );
}

void TestMtplLayer::legacyPackageXmlBadImages()
{
  QFETCH( QString, imageCase );
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );
  const QgsMtpl::PackageDescriptor descriptor = dataset.packages.constFirst().descriptor;
  QgsMtplPluginLayer legacyLayer( descriptor );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( directory.filePath( QStringLiteral( "legacy-bad-image.qgs" ) ) ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement element = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( element );
  QVERIFY( legacyLayer.writeLayerXml( element, document, context ) );

  QByteArray bytes = solidPng( QColor( 220, 30, 40 ) );
  QByteArray metadata = tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 );
  if ( imageCase == QLatin1String( "wrong-image-size" ) )
  {
    QImage image( sTileSize + 1, sTileSize + 1, QImage::Format_ARGB32 );
    image.fill( Qt::red );
    QBuffer buffer( &bytes );
    QVERIFY( buffer.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
    QVERIFY( image.save( &buffer, "PNG" ) );
  }
  else if ( imageCase.startsWith( QLatin1String( "truncated-jpeg" ) ) )
  {
    QImage image( sTileSize, sTileSize, QImage::Format_RGB32 );
    for ( int row = 0; row < sTileSize; ++row )
    {
      for ( int column = 0; column < sTileSize; ++column )
        image.setPixelColor( column, row, QColor( ( row * 37 + column * 17 ) % 256,
                                                 ( row * 11 + column * 43 ) % 256,
                                                 ( row * 53 + column * 29 ) % 256 ) );
    }
    QBuffer buffer( &bytes );
    QVERIFY( buffer.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
    QVERIFY( image.save( &buffer, "JPEG", 100 ) );
    buffer.close();
    QVERIFY( bytes.endsWith( QByteArray::fromHex( "ffd9" ) ) );
    bytes.chop( imageCase.endsWith( QLatin1String( "eoi" ) ) ? 2 : bytes.size() / 3 );
    metadata.replace( "\"tile_file_ext\":\"png\"", "\"tile_file_ext\":\"jpeg\"" );
  }
  else if ( imageCase == QLatin1String( "truncated-png-tail" ) )
    bytes.chop( 2 );
  else
    bytes.truncate( 24 );
  if ( imageCase != QLatin1String( "wrong-image-size" ) )
    QVERIFY( !QgsMtplPackageService::ptpImageHasCompleteStructure( bytes ) );
  QVERIFY( QFile::remove( descriptor.path ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              descriptor.path, sTileSize, metadata,
              MTPL_STORAGE_PLAIN, QgsMtpl::CryptoKeys(),
              { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, bytes } }, error ), qPrintable( error ) );
  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( element, context ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.isTileDataset() );
  QCOMPARE( restoredLayer.tileDataset()->packages.size(), 1 );

  QgsMapSettings settings;
  settings.setLayers( { &restoredLayer } );
  settings.setDestinationCrs( restoredLayer.crs() );
  settings.setOutputSize( QSize( 2 * sTileSize, sTileSize ) );
  settings.setOutputDpi( 96 );
  settings.setBackgroundColor( QColor( 0, 0, 0, 0 ) );
  settings.setOutputImageFormat( QImage::Format_ARGB32_Premultiplied );
  settings.setExtent( restoredLayer.extent() );
  QgsMapRendererSequentialJob job( settings );
  job.start();
  job.waitForFinished();
  QCOMPARE( opaquePixelCount( job.renderedImage() ), 0 );
  QVERIFY( !job.errors().isEmpty() );
  QVERIFY2( job.errors().constFirst().message.contains( QStringLiteral( "图层已降级：" ) ),
            qPrintable( job.errors().constFirst().message ) );

  QDomDocument migratedDocument( QStringLiteral( "qgis" ) );
  QDomElement migratedElement = migratedDocument.createElement( QStringLiteral( "maplayer" ) );
  migratedDocument.appendChild( migratedElement );
  QVERIFY( restoredLayer.writeLayerXml( migratedElement, migratedDocument, context ) );
  QgsMtplPluginLayer reopenedLayer;
  QVERIFY( reopenedLayer.readLayerXml( migratedElement, context ) );
  QVERIFY( reopenedLayer.isValid() );
  QVERIFY( reopenedLayer.isTileDataset() );
  settings.setLayers( { &reopenedLayer } );
  QgsMapRendererSequentialJob reopenedJob( settings );
  reopenedJob.start();
  reopenedJob.waitForFinished();
  QCOMPARE( opaquePixelCount( reopenedJob.renderedImage() ), 0 );
  QVERIFY( !reopenedJob.errors().isEmpty() );
  QVERIFY( reopenedJob.errors().constFirst().message.contains( QStringLiteral( "图层已降级：" ) ) );
}

void TestMtplLayer::legacyPackageXmlCompleteImages_data()
{
  QTest::addColumn<QByteArray>( "format" );
  QTest::addColumn<bool>( "progressive" );
  QTest::addColumn<bool>( "padding" );
  QTest::newRow( "png-padding" ) << QByteArrayLiteral( "png" ) << false << true;
  QTest::newRow( "jpeg-padding" ) << QByteArrayLiteral( "jpeg" ) << false << true;
  QTest::newRow( "webp-padding" ) << QByteArrayLiteral( "webp" ) << false << true;
  QTest::newRow( "progressive-jpeg" ) << QByteArrayLiteral( "jpeg" ) << true << false;
  QTest::newRow( "progressive-jpeg-padding" ) << QByteArrayLiteral( "jpeg" ) << true << true;
}

void TestMtplLayer::legacyPackageXmlCompleteImages()
{
  QFETCH( QByteArray, format );
  QFETCH( bool, progressive );
  QFETCH( bool, padding );
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );
  const QgsMtpl::PackageDescriptor descriptor = dataset.packages.constFirst().descriptor;
  QgsMtplPluginLayer legacyLayer( descriptor );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( directory.filePath( QStringLiteral( "legacy-complete-image.qgs" ) ) ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement element = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( element );
  QVERIFY( legacyLayer.writeLayerXml( element, document, context ) );

  const QColor expectedColor( 220, 30, 40 );
  QImage sourceImage( sTileSize, sTileSize, QImage::Format_RGB32 );
  sourceImage.fill( expectedColor );
  QByteArray bytes;
  QBuffer buffer( &bytes );
  QVERIFY( buffer.open( QIODevice::WriteOnly ) );
  QImageWriter writer( &buffer, format );
  writer.setQuality( 100 );
  writer.setProgressiveScanWrite( progressive );
  QVERIFY2( writer.write( sourceImage ), qPrintable( writer.errorString() ) );
  buffer.close();
  if ( progressive )
    QVERIFY( bytes.contains( QByteArray::fromHex( "ffc2" ) ) );
  if ( padding )
    bytes.append( QByteArray( 16, '\0' ) );
  QVERIFY( QgsMtplPackageService::ptpImageHasCompleteStructure( bytes ) );
  QVERIFY2( QgsMtplPackageService::validatePtpImagePayload( bytes, sTileSize, error ), qPrintable( error ) );

  QByteArray metadata = tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 );
  metadata.replace( QByteArrayLiteral( "\"tile_file_ext\":\"png\"" ),
                    QByteArray( QByteArrayLiteral( "\"tile_file_ext\":\"" ) + format + QByteArrayLiteral( "\"" ) ) );
  QVERIFY( QFile::remove( descriptor.path ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              descriptor.path, sTileSize, metadata, MTPL_STORAGE_PLAIN, QgsMtpl::CryptoKeys(),
              { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, bytes } }, error ), qPrintable( error ) );
  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( element, context ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.isTileDataset() );
  QgsMtplRasterInterface raster( *restoredLayer.tileDataset(), restoredLayer.tileMatrixSet(), {}, 0 );
  const QgsTileMatrix matrix = restoredLayer.tileMatrixSet().tileMatrix( 0 );
  std::unique_ptr<QgsRasterBlock> block( raster.block( 1, matrix.extent(), 2 * sTileSize, sTileSize ) );
  QVERIFY( block );
  QVERIFY2( raster.errors().isEmpty(), qPrintable( raster.errors().join( QLatin1Char( '\n' ) ) ) );
  const QImage rendered = block->image();
  QCOMPARE( opaquePixelCount( rendered ), sTileSize * sTileSize );
  const QColor actualColor = rendered.pixelColor( sTileSize / 2, sTileSize / 2 );
  QVERIFY( std::abs( actualColor.red() - expectedColor.red() ) <= 3 );
  QVERIFY( std::abs( actualColor.green() - expectedColor.green() ) <= 3 );
  QVERIFY( std::abs( actualColor.blue() - expectedColor.blue() ) <= 3 );
  QCOMPARE( rendered.pixelColor( sTileSize + sTileSize / 2, sTileSize / 2 ).alpha(), 0 );
}

void TestMtplLayer::projectMovePreservesRelativePtpPixels_data()
{
  QTest::addColumn<bool>( "legacy" );
  QTest::newRow( "legacy-single-package" ) << true;
  QTest::newRow( "current-directory-dataset" ) << false;
}

void TestMtplLayer::projectMovePreservesRelativePtpPixels()
{
  QFETCH( bool, legacy );
  QTemporaryDir workspace;
  QVERIFY( workspace.isValid() );
  const QString originalName = QStringLiteral( "original project" );
  const QString movedName = QStringLiteral( "移动后的工程 copy" );
  const QString originalRoot = workspace.filePath( originalName );
  const QString movedRoot = workspace.filePath( movedName );
  const QString dataPath = QDir( originalRoot ).filePath( QStringLiteral( "data" ) );
  QVERIFY( QDir().mkpath( dataPath ) );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( dataPath, dataset, error ), qPrintable( error ) );

  struct RegistryCleanup
  {
    bool removeType = false;
    ~RegistryCleanup()
    {
      if ( removeType )
        QgsApplication::pluginLayerRegistry()->removePluginLayerType( QgsMtplPluginLayer::layerTypeKey() );
    }
  } registryCleanup;
  if ( !QgsApplication::pluginLayerRegistry()->pluginLayerType( QgsMtplPluginLayer::layerTypeKey() ) )
  {
    QVERIFY( QgsApplication::pluginLayerRegistry()->addPluginLayerType( new QgsMtplPluginLayerType() ) );
    registryCleanup.removeType = true;
  }

  const QString originalProjectPath = QDir( originalRoot ).filePath( QStringLiteral( "map.qgs" ) );
  {
    QgsProject project;
    project.setFileName( originalProjectPath );
    project.setFilePathStorage( Qgis::FilePathType::Relative );
    QgsMtplPluginLayer *layer = nullptr;
    if ( legacy )
    {
      QgsMtpl::PackageDescriptor descriptor = dataset.packages.constFirst().descriptor;
      descriptor.hasExtent = true;
      descriptor.extentXMinimum = -180.0;
      descriptor.extentYMinimum = -90.0;
      descriptor.extentXMaximum = 0.0;
      descriptor.extentYMaximum = 90.0;
      layer = new QgsMtplPluginLayer( descriptor );
    }
    else
    {
      layer = new QgsMtplPluginLayer( dataset, QgsMtpl::CryptoKeys() );
    }
    QVERIFY( layer->isValid() );
    QVERIFY( project.addMapLayer( layer ) );
    QVERIFY( project.write() );
  }
  QFile originalProjectFile( originalProjectPath );
  QVERIFY( originalProjectFile.open( QIODevice::ReadOnly ) );
  const QByteArray originalXml = originalProjectFile.readAll();
  originalProjectFile.close();
  QDomDocument originalDocument;
  QVERIFY( originalDocument.setContent( originalXml ) );
  const QDomElement layerElement = originalDocument.documentElement()
                                    .firstChildElement( QStringLiteral( "projectlayers" ) )
                                    .firstChildElement( QStringLiteral( "maplayer" ) );
  QVERIFY( !layerElement.isNull() );
  const QString expectedSource = legacy ? QStringLiteral( "./data/0-0-0-0-0.ptp" ) : QStringLiteral( "./data" );
  // QGIS also writes an absolute publicSource in its providerless layer-tree
  // snapshot. Check the actual layer references used to restore the PTP data.
  QCOMPARE( layerElement.firstChildElement( QStringLiteral( "datasource" ) ).text(), expectedSource );
  if ( legacy )
  {
    QVERIFY( !layerElement.firstChildElement( QStringLiteral( "mtpl-package" ) ).isNull() );
    QVERIFY( layerElement.firstChildElement( QStringLiteral( "mtpl-dataset" ) ).isNull() );
  }
  else
  {
    const QDomElement datasetElement = layerElement.firstChildElement( QStringLiteral( "mtpl-dataset" ) );
    QVERIFY( !datasetElement.isNull() );
    QCOMPARE( datasetElement.attribute( QStringLiteral( "source" ) ), expectedSource );
    const QDomNodeList packageElements = datasetElement.firstChildElement( QStringLiteral( "packages" ) )
                                          .elementsByTagName( QStringLiteral( "package" ) );
    QCOMPARE( packageElements.size(), 2 );
    for ( int packageIndex = 0; packageIndex < packageElements.size(); ++packageIndex )
    {
      const QDomElement packageElement = packageElements.at( packageIndex ).toElement();
      const QString fileName = QFileInfo( dataset.packages.at( packageIndex ).descriptor.path ).fileName();
      QCOMPARE( packageElement.attribute( QStringLiteral( "path" ) ), QStringLiteral( "./data/%1" ).arg( fileName ) );
      QCOMPARE( packageElement.attribute( QStringLiteral( "relativePath" ) ), fileName );
    }
  }

  // Move the real project and its relative data directory together. The old
  // absolute paths no longer exist, so a stale source cannot satisfy the test.
  QVERIFY( QDir( workspace.path() ).rename( originalName, movedName ) );
  QVERIFY( !QFileInfo::exists( originalProjectPath ) );
  QVERIFY( !QFileInfo::exists( dataset.packages.constFirst().descriptor.path ) );
  const QString movedProjectPath = QDir( movedRoot ).filePath( QStringLiteral( "map.qgs" ) );
  QVERIFY( QFileInfo::exists( movedProjectPath ) );
  QImage expected( 2 * sTileSize, sTileSize, QImage::Format_ARGB32_Premultiplied );
  expected.fill( Qt::transparent );
  QPainter expectedPainter( &expected );
  expectedPainter.fillRect( 0, 0, sTileSize, sTileSize, QColor( 220, 30, 40 ) );
  if ( !legacy )
    expectedPainter.fillRect( sTileSize, 0, sTileSize, sTileSize, QColor( 20, 70, 230 ) );
  expectedPainter.end();

  for ( int pass = 0; pass < 2; ++pass )
  {
    QgsProject project;
    QVERIFY( project.read( movedProjectPath ) );
    QCOMPARE( project.mapLayers().size(), 1 );
    auto *layer = dynamic_cast<QgsMtplPluginLayer *>( project.mapLayers().constBegin().value() );
    QVERIFY( layer );
    QVERIFY( layer->isValid() );
    QVERIFY( layer->isTileDataset() );
    const QString expectedMovedSource = QDir( movedRoot ).filePath(
      legacy ? QStringLiteral( "data/0-0-0-0-0.ptp" ) : QStringLiteral( "data" ) );
    QCOMPARE( QDir::cleanPath( layer->source() ), QDir::cleanPath( expectedMovedSource ) );
    QCOMPARE( layer->tileDataset()->packages.size(), legacy ? 1 : 2 );
    QCOMPARE( layer->tileDataset()->compatibilityMode, legacy );
    for ( const QgsMtpl::TileDatasetPackage &package : layer->tileDataset()->packages )
    {
      const QString expectedPath = QDir( movedRoot ).filePath(
        QStringLiteral( "data/%1" ).arg( QFileInfo( package.descriptor.path ).fileName() ) );
      QCOMPARE( QDir::cleanPath( package.descriptor.path ), QDir::cleanPath( expectedPath ) );
      QVERIFY( QFileInfo::exists( package.descriptor.path ) );
    }
    QgsMapSettings settings;
    settings.setLayers( { layer } );
    settings.setDestinationCrs( layer->crs() );
    settings.setTransformContext( project.transformContext() );
    settings.setOutputSize( expected.size() );
    settings.setOutputDpi( 96 );
    settings.setBackgroundColor( QColor( 0, 0, 0, 0 ) );
    settings.setOutputImageFormat( QImage::Format_ARGB32_Premultiplied );
    settings.setExtent( QgsRectangle( -180.0, -90.0, 180.0, 90.0 ) );
    QgsMapRendererSequentialJob job( settings );
    job.start();
    job.waitForFinished();
    QVERIFY2( job.errors().isEmpty(),
              qPrintable( job.errors().isEmpty() ? QString() : job.errors().constFirst().message ) );
    QCOMPARE( job.renderedImage(), expected );
    // The migrated legacy project must remain portable after its next save.
    QVERIFY( project.write() );
  }
}

void TestMtplLayer::datasetXmlV1RoundTrip()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QString datasetPath = QDir( directory.path() ).filePath( QStringLiteral( "data" ) );
  QVERIFY( QDir().mkpath( datasetPath ) );

  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( datasetPath, dataset, error ), qPrintable( error ) );
  QFile directoryTileJson( QDir( datasetPath ).filePath( QStringLiteral( "tilejson.json" ) ) );
  QVERIFY( directoryTileJson.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
  const QByteArray directoryTileJsonBytes =
    QByteArrayLiteral( R"({"description":"directory-description","attribution":"directory-attribution"})" );
  QCOMPARE( directoryTileJson.write( directoryTileJsonBytes ),
            static_cast<qint64>( directoryTileJsonBytes.size() ) );
  directoryTileJson.close();

  const QFileInfo firstPackageInfo( dataset.packages.constFirst().descriptor.path );
  QFile packageTileJson( firstPackageInfo.absoluteDir().filePath(
    firstPackageInfo.completeBaseName() + QStringLiteral( ".json" ) ) );
  QVERIFY( packageTileJson.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
  const QByteArray packageTileJsonBytes =
    QByteArrayLiteral( R"({"description":"wrong-per-package-description"})" );
  QCOMPARE( packageTileJson.write( packageTileJsonBytes ),
            static_cast<qint64>( packageTileJsonBytes.size() ) );
  packageTileJson.close();

  dataset.hasExtent = true;
  dataset.extentXMinimum = dataset.matrix.boundsXMinimum;
  dataset.extentYMinimum = dataset.matrix.boundsYMinimum;
  dataset.extentXMaximum = dataset.matrix.boundsXMaximum;
  dataset.extentYMaximum = dataset.matrix.boundsYMaximum;
  dataset.partitionRule = partitionRule(
    0,
    QStringLiteral( "test.snapshot.rule" ),
    QStringLiteral( "项目内规则快照" ) );
  for ( QgsMtpl::TileDatasetPackage &package : dataset.packages )
  {
    package.sidecarPath = QDir( datasetPath ).filePath( QStringLiteral( "synthetic-reference.keys.json" ) );
    package.descriptor.sidecarPath = package.sidecarPath;
  }

  QgsMtpl::CryptoKeys syntheticKeys = QgsMtplTest::fixtureKeys();
  QgsMtplPluginLayer layer( dataset, syntheticKeys );
  QVERIFY( layer.isValid() );
  QVERIFY( layer.hasCryptoKeys() );

  dataset.partitionRule.id = QStringLiteral( "mutated-after-layer-created" );
  dataset.partitionRule.name = QStringLiteral( "不应保存" );

  const QString projectPath = QDir( directory.path() ).filePath( QStringLiteral( "dataset-project.qgs" ) );
  QgsReadWriteContext writeContext;
  writeContext.setPathResolver( QgsPathResolver( projectPath ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, document, writeContext ) );

  const QByteArray xml = document.toByteArray( 2 );
  QVERIFY( xml.contains( "<mtpl-dataset" ) );
  QVERIFY( !xml.contains( "<mtpl-package" ) );
  QVERIFY( !xml.contains( syntheticKeys.privateKey ) );
  QVERIFY( !xml.contains( syntheticKeys.deviceKey ) );
  QVERIFY( !xml.contains( directory.path().toUtf8() ) );
  QVERIFY( !xml.contains( "structureValidation" ) );
  QVERIFY( !xml.contains( "validationAttempt" ) );

  const QDomElement datasetElement = mapLayerElement.firstChildElement( QStringLiteral( "mtpl-dataset" ) );
  QCOMPARE( datasetElement.attribute( QStringLiteral( "version" ) ), QStringLiteral( "1" ) );
  QCOMPARE( datasetElement.attribute( QStringLiteral( "source" ) ), QStringLiteral( "./data" ) );
  const QDomElement ruleElement = datasetElement.firstChildElement( QStringLiteral( "partition-rule" ) );
  QCOMPARE( ruleElement.attribute( QStringLiteral( "id" ) ), QStringLiteral( "test.snapshot.rule" ) );
  QCOMPARE( ruleElement.attribute( QStringLiteral( "name" ) ), QStringLiteral( "项目内规则快照" ) );
  QCOMPARE( ruleElement.elementsByTagName( QStringLiteral( "band" ) ).size(), 1 );

  const QDomNodeList packageElements = datasetElement.elementsByTagName( QStringLiteral( "package" ) );
  QCOMPARE( packageElements.size(), 2 );
  for ( int index = 0; index < packageElements.size(); ++index )
  {
    const QDomElement packageElement = packageElements.at( index ).toElement();
    QVERIFY( !packageElement.attribute( QStringLiteral( "relativePath" ) ).isEmpty() );
    QVERIFY( packageElement.attribute( QStringLiteral( "path" ) ).startsWith( QStringLiteral( "./data/" ) ) );
    QVERIFY( packageElement.attribute( QStringLiteral( "sidecar" ) ).startsWith( QStringLiteral( "./data/" ) ) );
    QVERIFY( packageElement.hasAttribute( QStringLiteral( "fileSize" ) ) );
    QVERIFY( packageElement.hasAttribute( QStringLiteral( "lastModifiedMs" ) ) );
  }

  QFile projectFile( projectPath );
  QVERIFY( projectFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
  QCOMPARE( projectFile.write( xml ), static_cast<qint64>( xml.size() ) );
  projectFile.close();
  QVERIFY( projectFile.open( QIODevice::ReadOnly ) );
  QDomDocument restoredDocument;
  QVERIFY( restoredDocument.setContent( &projectFile ) );
  projectFile.close();

  QgsReadWriteContext readContext;
  readContext.setPathResolver( QgsPathResolver( projectPath ) );
  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( restoredDocument.documentElement(), readContext ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.isTileDataset() );
  QVERIFY( !restoredLayer.hasCryptoKeys() );
  QVERIFY( restoredLayer.tileDataset() );
  QCOMPARE( restoredLayer.tileDataset()->partitionRule.id, QStringLiteral( "test.snapshot.rule" ) );
  QCOMPARE( restoredLayer.tileDataset()->partitionRule.name, QStringLiteral( "项目内规则快照" ) );
  QCOMPARE( restoredLayer.tileDataset()->packages.size(), 2 );
  QCOMPARE( QDir::cleanPath( restoredLayer.tileDataset()->sourcePath ), QDir::cleanPath( datasetPath ) );
  QCOMPARE( restoredLayer.tileMatrixSet().tileMatrix( 0 ).matrixWidth(), 2 );
  for ( int index = 0; index < restoredLayer.tileDataset()->packages.size(); ++index )
  {
    const QgsMtpl::TileDatasetPackage &restoredPackage = restoredLayer.tileDataset()->packages.at( index );
    QCOMPARE( QDir::cleanPath( restoredPackage.descriptor.path ),
              QDir::cleanPath( QDir( datasetPath ).filePath( restoredPackage.relativePath ) ) );
    QCOMPARE( restoredPackage.fileLastModifiedMs,
              layer.tileDataset()->packages.at( index ).fileLastModifiedMs );
    QCOMPARE( restoredPackage.descriptor.readiness, QgsMtpl::ReadinessState::PlainReady );
    QCOMPARE( restoredPackage.descriptor.metadata.value( QStringLiteral( "map_id" ) ).toString(),
              QStringLiteral( "logical-test-map" ) );
    QCOMPARE( restoredPackage.descriptor.metadata.value( QStringLiteral( "description" ) ).toString(),
              QStringLiteral( "directory-description" ) );
    QCOMPARE( restoredPackage.descriptor.metadata.value( QStringLiteral( "attribution" ) ).toString(),
              QStringLiteral( "directory-attribution" ) );
    QVERIFY( !restoredPackage.descriptor.metadata.contains( QStringLiteral( "privateKeyBase64" ) ) );
    QVERIFY( !restoredPackage.descriptor.metadata.contains( QStringLiteral( "deviceKeyHex" ) ) );
    const QVariantMap restoredNested = restoredPackage.descriptor.metadata.value(
      QStringLiteral( "nested" ) ).toMap();
    QCOMPARE( restoredNested.value( QStringLiteral( "label" ) ).toString(),
              QStringLiteral( "nested-visible" ) );
    QVERIFY( !restoredNested.contains( QStringLiteral( "password" ) ) );
    for ( auto it = restoredPackage.descriptor.metadata.constBegin();
          it != restoredPackage.descriptor.metadata.constEnd(); ++it )
      QVERIFY( !it.key().startsWith( QLatin1String( "_mtpl" ), Qt::CaseInsensitive ) );
  }

  QDomDocument missingSchemeDocument;
  QVERIFY( missingSchemeDocument.setContent( xml ) );
  missingSchemeDocument.documentElement()
    .firstChildElement( QStringLiteral( "mtpl-dataset" ) )
    .firstChildElement( QStringLiteral( "tile-matrix" ) )
    .removeAttribute( QStringLiteral( "scheme" ) );
  QgsMtplPluginLayer missingSchemeLayer;
  QVERIFY( !missingSchemeLayer.readLayerXml( missingSchemeDocument.documentElement(), readContext ) );

  QDomDocument tamperedMapIdDocument;
  QVERIFY( tamperedMapIdDocument.setContent( xml ) );
  tamperedMapIdDocument.documentElement()
    .firstChildElement( QStringLiteral( "mtpl-dataset" ) )
    .setAttribute( QStringLiteral( "mapId" ), QStringLiteral( "tampered-map" ) );
  QgsMtplPluginLayer tamperedMapIdLayer;
  QVERIFY( !tamperedMapIdLayer.readLayerXml( tamperedMapIdDocument.documentElement(), readContext ) );

  QVERIFY( QFile::remove( layer.tileDataset()->packages.constFirst().descriptor.path ) );
  QgsMtplPluginLayer missingHistoricalPackageLayer;
  QVERIFY( missingHistoricalPackageLayer.readLayerXml( restoredDocument.documentElement(), readContext ) );
  QVERIFY( missingHistoricalPackageLayer.isValid() );
  QVERIFY( missingHistoricalPackageLayer.tileDataset() );
  QCOMPARE( missingHistoricalPackageLayer.tileDataset()->packages.size(), 2 );
  for ( const QgsMtpl::TileDatasetPackage &package : missingHistoricalPackageLayer.tileDataset()->packages )
    QCOMPARE( package.descriptor.readiness, QgsMtpl::ReadinessState::PlainReady );

  syntheticKeys.clear();
}

void TestMtplLayer::datasetXmlClippedPartitionBandRoundTrip()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QString datasetPath = QDir( directory.path() ).filePath( QStringLiteral( "data" ) );
  QVERIFY( QDir().mkpath( datasetPath ) );

  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( datasetPath, dataset, error ), qPrintable( error ) );
  dataset.partitionRule = partitionRule(
    7,
    QStringLiteral( "test.clipped.rule" ),
    QStringLiteral( "矩阵裁剪规则" ) );

  dataset.packageIndexByAddress.clear();
  for ( qsizetype index = 0; index < dataset.packages.size(); ++index )
  {
    QgsMtpl::TileDatasetPackage &package = dataset.packages[index];
    const QString oldPath = package.descriptor.path;
    const QString newName = QFileInfo( oldPath ).fileName().replace(
      QStringLiteral( "0-0-0-" ), QStringLiteral( "0-7-0-" ) );
    const QString newPath = QDir( datasetPath ).filePath( newName );
    QVERIFY( QFile::rename( oldPath, newPath ) );
    package.descriptor.path = newPath;
    package.descriptor.displayName = QFileInfo( newPath ).completeBaseName();
    package.address.maxZoom = 7;
    package.relativePath = newName;
    package.fileLastModifiedMs = QFileInfo( newPath ).lastModified().toMSecsSinceEpoch();
    package.descriptor.fileLastModifiedMs = package.fileLastModifiedMs;
    dataset.packageIndexByAddress.insert( package.address.key(), static_cast<int>( index ) );
  }
  QVERIFY2( dataset.isValid( &error ), qPrintable( error ) );

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  QCOMPARE( layer.tileMatrixSet().minimumZoom(), 0 );
  QCOMPARE( layer.tileMatrixSet().maximumZoom(), 0 );

  const QString projectPath = directory.filePath( QStringLiteral( "clipped-dataset-project.qgs" ) );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( projectPath ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, document, context ) );

  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( document.documentElement(), context ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.tileDataset() );
  QCOMPARE( restoredLayer.tileDataset()->minimumZoom, 0 );
  QCOMPARE( restoredLayer.tileDataset()->maximumZoom, 0 );
  QCOMPARE( restoredLayer.tileDataset()->partitionRule.bands.constFirst().minZoom, 0 );
  QCOMPARE( restoredLayer.tileDataset()->partitionRule.bands.constFirst().maxZoom, 7 );
  QCOMPARE( restoredLayer.tileMatrixSet().minimumZoom(), 0 );
  QCOMPARE( restoredLayer.tileMatrixSet().maximumZoom(), 0 );

  QDomDocument tamperedDocument;
  QVERIFY( tamperedDocument.setContent( document.toByteArray() ) );
  QDomElement packageElement = tamperedDocument.documentElement()
                                 .firstChildElement( QStringLiteral( "mtpl-dataset" ) )
                                 .firstChildElement( QStringLiteral( "packages" ) )
                                 .firstChildElement( QStringLiteral( "package" ) );
  packageElement.setAttribute( QStringLiteral( "addressMinZoom" ), 8 );
  packageElement.setAttribute( QStringLiteral( "addressMaxZoom" ), 11 );
  packageElement.setAttribute( QStringLiteral( "addressBaseZoom" ), 3 );
  QgsMtplPluginLayer tamperedLayer;
  QVERIFY( !tamperedLayer.readLayerXml( tamperedDocument.documentElement(), context ) );
}

void TestMtplLayer::datasetXmlLockedRoundTripDoesNotRequestUnlock()
{
  ScopedPluginSettingsRestore settingsRestore( {
    QStringLiteral( "mtpl/credentials/authcfg" ),
    QStringLiteral( "mtpl/credentials/remember" ),
    QStringLiteral( "mtpl/credentials/privateKey" ),
    QStringLiteral( "mtpl/credentials/deviceKey" ),
    QStringLiteral( "mtpl/lastPath" )
  } );
  ScopedMemoryCredentialBackend credentialBackend;
  QgsMtplCredentialStore::clear();

  QgsMtpl::CryptoKeys keys = QgsMtplTest::fixtureKeys();
  QString error;
  QVERIFY2( QgsMtplCredentialStore::saveManualKeys( keys, QString(), error ), qPrintable( error ) );
  credentialBackend.setUnlocked( false );
  credentialBackend.resetLoadCounts();

  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QVERIFY2( createEncryptedAggregateDataset( directory.path(), keys, dataset, error ), qPrintable( error ) );
  QgsMtplPluginLayer layer( dataset, keys );
  QVERIFY( layer.isValid() );

  const QString projectPath = directory.filePath( QStringLiteral( "locked-dataset-project.qgs" ) );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( projectPath ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, document, context ) );

  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( document.documentElement(), context ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.isTileDataset() );
  QVERIFY( !restoredLayer.hasCryptoKeys() );
  QCOMPARE( credentialBackend.unlockAllowedLoadCount, 0 );
  QCOMPARE( credentialBackend.silentLoadCount, 1 );
  QVERIFY( restoredLayer.tileDataset() );
  for ( const QgsMtpl::TileDatasetPackage &package : restoredLayer.tileDataset()->packages )
  {
    QCOMPARE( package.descriptor.encryption, QgsMtpl::EncryptionState::Locked );
    QCOMPARE( package.descriptor.readiness, QgsMtpl::ReadinessState::KeyRequired );
    QCOMPARE( package.descriptor.credentialSource, QgsMtpl::CredentialSource::None );
  }

  const QString changedPath = dataset.packages.constFirst().descriptor.path;
  QFile changedFile( changedPath );
  QVERIFY( changedFile.open( QIODevice::ReadWrite ) );
  QVERIFY( changedFile.setFileTime(
    QDateTime::fromMSecsSinceEpoch(
      QFileInfo( changedPath ).lastModified().toMSecsSinceEpoch() + 2000, Qt::UTC ),
    QFileDevice::FileModificationTime ) );
  changedFile.close();
  credentialBackend.setUnlocked( true );
  credentialBackend.resetLoadCounts();

  QgsMtplPluginLayer changedSourceLayer;
  QVERIFY( changedSourceLayer.readLayerXml( document.documentElement(), context ) );
  QVERIFY( changedSourceLayer.isValid() );
  QVERIFY( changedSourceLayer.hasCryptoKeys() );
  QCOMPARE( credentialBackend.unlockAllowedLoadCount, 0 );
  QCOMPARE( credentialBackend.silentLoadCount, 1 );
  QVERIFY( changedSourceLayer.tileDataset() );
  QCOMPARE( changedSourceLayer.tileDataset()->packages.at( 0 ).descriptor.readiness,
            QgsMtpl::ReadinessState::KeyRequired );
  QCOMPARE( changedSourceLayer.tileDataset()->packages.at( 1 ).descriptor.readiness,
            QgsMtpl::ReadinessState::KeyVerified );
  keys.clear();
}

void TestMtplLayer::datasetXmlBadImageSnapshotRoundTrip()
{
  ScopedPluginSettingsRestore settingsRestore( {
    QStringLiteral( "mtpl/credentials/authcfg" ),
    QStringLiteral( "mtpl/credentials/remember" ),
    QStringLiteral( "mtpl/credentials/privateKey" ),
    QStringLiteral( "mtpl/credentials/deviceKey" ),
    QStringLiteral( "mtpl/lastPath" )
  } );
  ScopedMemoryCredentialBackend credentialBackend;
  QgsMtplCredentialStore::clear();

  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( directory.path(), dataset, error ), qPrintable( error ) );
  dataset.hasExtent = true;
  dataset.extentXMinimum = dataset.matrix.boundsXMinimum;
  dataset.extentYMinimum = dataset.matrix.boundsYMinimum;
  dataset.extentXMaximum = dataset.matrix.boundsXMaximum;
  dataset.extentYMaximum = dataset.matrix.boundsYMaximum;
  const QString damagedPath = dataset.packages.constFirst().descriptor.path;

  QVERIFY( QFile::remove( damagedPath ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              damagedPath,
              sTileSize,
              tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 ),
              MTPL_STORAGE_PLAIN,
              QgsMtpl::CryptoKeys(),
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, QByteArrayLiteral( "not-an-image" ) } },
              error ),
            qPrintable( error ) );
  const QFileInfo damagedInfo( damagedPath );
  dataset.packages[0].descriptor.fileSize = static_cast<quint64>( damagedInfo.size() );
  dataset.packages[0].descriptor.fileLastModifiedMs = damagedInfo.lastModified().toMSecsSinceEpoch();
  dataset.packages[0].fileLastModifiedMs = dataset.packages[0].descriptor.fileLastModifiedMs;

  const QString projectPath = directory.filePath( QStringLiteral( "bad-image-project.qgs" ) );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( projectPath ) );
  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, document, context ) );

  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( document.documentElement(), context ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.isTileDataset() );
  QCOMPARE( restoredLayer.tileDataset()->packages.constFirst().descriptor.readiness,
            QgsMtpl::ReadinessState::PlainReady );

  QByteArray elevationMetadata = tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 );
  elevationMetadata.replace( "\"tile_file_ext\":\"png\"", "\"tile_file_ext\":\"elevation\"" );
  QVERIFY( QFile::remove( damagedPath ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              damagedPath,
              sTileSize,
              elevationMetadata,
              MTPL_STORAGE_PLAIN,
              QgsMtpl::CryptoKeys(),
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, solidPng( QColor( 220, 30, 40 ) ) } },
              error ),
            qPrintable( error ) );
  const QFileInfo elevationInfo( damagedPath );
  dataset.packages[0].descriptor.fileSize = static_cast<quint64>( elevationInfo.size() );
  dataset.packages[0].descriptor.fileLastModifiedMs = elevationInfo.lastModified().toMSecsSinceEpoch();
  dataset.packages[0].fileLastModifiedMs = dataset.packages[0].descriptor.fileLastModifiedMs;

  QgsMtplPluginLayer declaredElevationLayer( dataset, QgsMtpl::CryptoKeys() );
  QDomDocument elevationDocument( QStringLiteral( "qgis" ) );
  QDomElement elevationMapLayer = elevationDocument.createElement( QStringLiteral( "maplayer" ) );
  elevationDocument.appendChild( elevationMapLayer );
  QVERIFY( declaredElevationLayer.writeLayerXml( elevationMapLayer, elevationDocument, context ) );
  QgsMtplPluginLayer rejectedLayer;
  QVERIFY( !rejectedLayer.readLayerXml( elevationDocument.documentElement(), context ) );

  QTemporaryDir encryptedDirectory;
  QVERIFY( encryptedDirectory.isValid() );
  QgsMtpl::CryptoKeys encryptedKeys = QgsMtplTest::fixtureKeys();
  QgsMtpl::TileDatasetDescriptor encryptedDataset;
  QVERIFY2( createEncryptedAggregateDataset(
              encryptedDirectory.path(), encryptedKeys, encryptedDataset, error ),
            qPrintable( error ) );
  const QString encryptedDamagedPath = encryptedDataset.packages.constFirst().descriptor.path;
  QVERIFY( QFile::remove( encryptedDamagedPath ) );
  QVERIFY2( QgsMtplTest::writePtpFixture(
              encryptedDamagedPath,
              sTileSize,
              tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 2 ),
              MTPL_STORAGE_ENCRYPTED,
              encryptedKeys,
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, QByteArrayLiteral( "not-an-image" ) } },
              error ),
            qPrintable( error ) );
  const QFileInfo encryptedDamagedInfo( encryptedDamagedPath );
  encryptedDataset.packages[0].descriptor.fileSize = static_cast<quint64>( encryptedDamagedInfo.size() );
  encryptedDataset.packages[0].descriptor.fileLastModifiedMs =
    encryptedDamagedInfo.lastModified().toMSecsSinceEpoch();
  encryptedDataset.packages[0].fileLastModifiedMs =
    encryptedDataset.packages[0].descriptor.fileLastModifiedMs;
  QVERIFY2( QgsMtplCredentialStore::saveManualKeys( encryptedKeys, QString(), error ), qPrintable( error ) );

  const QString encryptedProjectPath = encryptedDirectory.filePath( QStringLiteral( "encrypted-bad-image.qgs" ) );
  QgsReadWriteContext encryptedContext;
  encryptedContext.setPathResolver( QgsPathResolver( encryptedProjectPath ) );
  QgsMtplPluginLayer encryptedLayer( encryptedDataset, encryptedKeys );
  QVERIFY( encryptedLayer.isValid() );
  QDomDocument encryptedDocument( QStringLiteral( "qgis" ) );
  QDomElement encryptedMapLayer = encryptedDocument.createElement( QStringLiteral( "maplayer" ) );
  encryptedDocument.appendChild( encryptedMapLayer );
  QVERIFY( encryptedLayer.writeLayerXml( encryptedMapLayer, encryptedDocument, encryptedContext ) );

  QgsMtplPluginLayer restoredEncryptedLayer;
  QVERIFY( restoredEncryptedLayer.readLayerXml( encryptedDocument.documentElement(), encryptedContext ) );
  QVERIFY( restoredEncryptedLayer.isValid() );
  QVERIFY( restoredEncryptedLayer.hasCryptoKeys() );
  QCOMPARE( restoredEncryptedLayer.tileDataset()->packages.constFirst().descriptor.readiness,
            QgsMtpl::ReadinessState::KeyVerified );
  QCOMPARE( restoredEncryptedLayer.tileDataset()->packages.constFirst().descriptor.credentialSource,
            QgsMtpl::CredentialSource::Remembered );
  encryptedKeys.clear();
}

void TestMtplLayer::malformedDatasetXml_data()
{
  QTest::addColumn<QString>( "mutation" );

  QTest::newRow( "file-size-not-a-number" ) << QStringLiteral( "fileSize" );
  QTest::newRow( "address-x-not-a-number" ) << QStringLiteral( "addressX" );
  QTest::newRow( "present-count-not-a-number" ) << QStringLiteral( "presentCount" );
  QTest::newRow( "top-left-x-not-finite" ) << QStringLiteral( "topLeftX" );
  QTest::newRow( "minimum-zoom-not-a-number" ) << QStringLiteral( "minimumZoom" );
  QTest::newRow( "package-zoom-unpaired" ) << QStringLiteral( "packageZoomUnpaired" );
  QTest::newRow( "package-zoom-reversed" ) << QStringLiteral( "packageZoomReversed" );
  QTest::newRow( "package-zoom-invalid-sentinel" ) << QStringLiteral( "packageZoomInvalidSentinel" );
  QTest::newRow( "has-bounds-not-boolean" ) << QStringLiteral( "hasBounds" );
  QTest::newRow( "scale-not-finite" ) << QStringLiteral( "scale" );
  QTest::newRow( "has-no-data-not-boolean" ) << QStringLiteral( "hasNoData" );
  QTest::newRow( "missing-ranges" ) << QStringLiteral( "ranges" );
  QTest::newRow( "range-outside-tile-matrix" ) << QStringLiteral( "rangeBounds" );
  QTest::newRow( "payload-does-not-match-package" ) << QStringLiteral( "payloadMismatch" );
  QTest::newRow( "bounds-enabled-with-missing-value" ) << QStringLiteral( "boundsFields" );
  QTest::newRow( "no-data-enabled-with-missing-value" ) << QStringLiteral( "noData" );
}

void TestMtplLayer::malformedDatasetXml()
{
  QFETCH( QString, mutation );

  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QString datasetPath = QDir( directory.path() ).filePath( QStringLiteral( "data" ) );
  QVERIFY( QDir().mkpath( datasetPath ) );

  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( datasetPath, dataset, error ), qPrintable( error ) );
  dataset.hasExtent = true;
  dataset.extentXMinimum = dataset.matrix.boundsXMinimum;
  dataset.extentYMinimum = dataset.matrix.boundsYMinimum;
  dataset.extentXMaximum = dataset.matrix.boundsXMaximum;
  dataset.extentYMaximum = dataset.matrix.boundsYMaximum;

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );

  const QString projectPath = QDir( directory.path() ).filePath( QStringLiteral( "malformed-dataset-project.qgs" ) );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( projectPath ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, document, context ) );

  QDomElement datasetElement = mapLayerElement.firstChildElement( QStringLiteral( "mtpl-dataset" ) );
  QDomElement matrixElement = datasetElement.firstChildElement( QStringLiteral( "tile-matrix" ) );
  QDomElement packageElement = datasetElement.firstChildElement( QStringLiteral( "packages" ) )
                                 .firstChildElement( QStringLiteral( "package" ) );
  QDomElement rangesElement = packageElement.firstChildElement( QStringLiteral( "ranges" ) );
  QDomElement rangeElement = rangesElement.firstChildElement( QStringLiteral( "range" ) );
  QVERIFY( !datasetElement.isNull() );
  QVERIFY( !matrixElement.isNull() );
  QVERIFY( !packageElement.isNull() );
  QVERIFY( !rangesElement.isNull() );
  QVERIFY( !rangeElement.isNull() );

  if ( mutation == QLatin1String( "fileSize" ) )
    packageElement.setAttribute( QStringLiteral( "fileSize" ), QStringLiteral( "not-a-number" ) );
  else if ( mutation == QLatin1String( "addressX" ) )
    packageElement.setAttribute( QStringLiteral( "addressX" ), QStringLiteral( "not-a-number" ) );
  else if ( mutation == QLatin1String( "presentCount" ) )
    rangeElement.setAttribute( QStringLiteral( "presentCount" ), QStringLiteral( "not-a-number" ) );
  else if ( mutation == QLatin1String( "topLeftX" ) )
    matrixElement.setAttribute( QStringLiteral( "topLeftX" ), QStringLiteral( "nan" ) );
  else if ( mutation == QLatin1String( "minimumZoom" ) )
    datasetElement.setAttribute( QStringLiteral( "minimumZoom" ), QStringLiteral( "bad" ) );
  else if ( mutation == QLatin1String( "packageZoomUnpaired" ) )
  {
    packageElement.setAttribute( QStringLiteral( "minimumZoom" ), QStringLiteral( "-1" ) );
    packageElement.setAttribute( QStringLiteral( "maximumZoom" ), QStringLiteral( "0" ) );
  }
  else if ( mutation == QLatin1String( "packageZoomReversed" ) )
  {
    packageElement.setAttribute( QStringLiteral( "minimumZoom" ), QStringLiteral( "1" ) );
    packageElement.setAttribute( QStringLiteral( "maximumZoom" ), QStringLiteral( "0" ) );
  }
  else if ( mutation == QLatin1String( "packageZoomInvalidSentinel" ) )
  {
    packageElement.setAttribute( QStringLiteral( "minimumZoom" ), QStringLiteral( "-2" ) );
    packageElement.setAttribute( QStringLiteral( "maximumZoom" ), QStringLiteral( "-2" ) );
  }
  else if ( mutation == QLatin1String( "hasBounds" ) )
    matrixElement.setAttribute( QStringLiteral( "hasBounds" ), QStringLiteral( "2" ) );
  else if ( mutation == QLatin1String( "scale" ) )
    packageElement.setAttribute( QStringLiteral( "scale" ), QStringLiteral( "nan" ) );
  else if ( mutation == QLatin1String( "hasNoData" ) )
    packageElement.setAttribute( QStringLiteral( "hasNoData" ), QStringLiteral( "2" ) );
  else if ( mutation == QLatin1String( "ranges" ) )
    packageElement.removeChild( rangesElement );
  else if ( mutation == QLatin1String( "rangeBounds" ) )
  {
    rangeElement.setAttribute( QStringLiteral( "xMax" ), QStringLiteral( "2" ) );
    rangeElement.setAttribute( QStringLiteral( "presentXMax" ), QStringLiteral( "2" ) );
  }
  else if ( mutation == QLatin1String( "payloadMismatch" ) )
  {
    datasetElement.setAttribute( QStringLiteral( "payload" ), QStringLiteral( "elevation" ) );
    for ( QDomElement element = datasetElement.firstChildElement( QStringLiteral( "packages" ) )
                                  .firstChildElement( QStringLiteral( "package" ) );
          !element.isNull();
          element = element.nextSiblingElement( QStringLiteral( "package" ) ) )
    {
      element.setAttribute( QStringLiteral( "payload" ), QStringLiteral( "elevation" ) );
    }
  }
  else if ( mutation == QLatin1String( "boundsFields" ) )
  {
    matrixElement.setAttribute( QStringLiteral( "hasBounds" ), QStringLiteral( "1" ) );
    matrixElement.removeAttribute( QStringLiteral( "boundsXMinimum" ) );
  }
  else if ( mutation == QLatin1String( "noData" ) )
  {
    packageElement.setAttribute( QStringLiteral( "hasNoData" ), QStringLiteral( "1" ) );
    packageElement.removeAttribute( QStringLiteral( "noData" ) );
  }
  else
  {
    QFAIL( "Unhandled malformed XML mutation." );
  }

  QgsMtplPluginLayer restoredLayer;
  QVERIFY2( !restoredLayer.readLayerXml( document.documentElement(), context ), qPrintable( mutation ) );
}

void TestMtplLayer::datasetXmlOptionalFieldsDefault()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QString datasetPath = QDir( directory.path() ).filePath( QStringLiteral( "data" ) );
  QVERIFY( QDir().mkpath( datasetPath ) );

  QgsMtpl::TileDatasetDescriptor dataset;
  QString error;
  QVERIFY2( createXyzDataset( datasetPath, dataset, error ), qPrintable( error ) );
  dataset.hasExtent = true;
  dataset.extentXMinimum = dataset.matrix.boundsXMinimum;
  dataset.extentYMinimum = dataset.matrix.boundsYMinimum;
  dataset.extentXMaximum = dataset.matrix.boundsXMaximum;
  dataset.extentYMaximum = dataset.matrix.boundsYMaximum;

  QgsMtplPluginLayer layer( dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );

  const QString projectPath = QDir( directory.path() ).filePath( QStringLiteral( "optional-fields-project.qgs" ) );
  QgsReadWriteContext context;
  context.setPathResolver( QgsPathResolver( projectPath ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, document, context ) );

  QDomElement datasetElement = mapLayerElement.firstChildElement( QStringLiteral( "mtpl-dataset" ) );
  QDomElement matrixElement = datasetElement.firstChildElement( QStringLiteral( "tile-matrix" ) );
  QDomElement packagesElement = datasetElement.firstChildElement( QStringLiteral( "packages" ) );
  QVERIFY( !datasetElement.isNull() );
  QVERIFY( !matrixElement.isNull() );
  QVERIFY( !packagesElement.isNull() );

  datasetElement.removeAttribute( QStringLiteral( "mapId" ) );
  datasetElement.setAttribute( QStringLiteral( "hasExtent" ), QStringLiteral( "0" ) );
  datasetElement.removeAttribute( QStringLiteral( "extentXMinimum" ) );
  datasetElement.removeAttribute( QStringLiteral( "extentYMinimum" ) );
  datasetElement.removeAttribute( QStringLiteral( "extentXMaximum" ) );
  datasetElement.removeAttribute( QStringLiteral( "extentYMaximum" ) );

  matrixElement.setAttribute( QStringLiteral( "hasBounds" ), QStringLiteral( "0" ) );
  matrixElement.removeAttribute( QStringLiteral( "boundsXMinimum" ) );
  matrixElement.removeAttribute( QStringLiteral( "boundsYMinimum" ) );
  matrixElement.removeAttribute( QStringLiteral( "boundsXMaximum" ) );
  matrixElement.removeAttribute( QStringLiteral( "boundsYMaximum" ) );
  matrixElement.removeAttribute( QStringLiteral( "boundsCrs" ) );

  for ( QDomElement packageElement = packagesElement.firstChildElement( QStringLiteral( "package" ) );
        !packageElement.isNull();
        packageElement = packageElement.nextSiblingElement( QStringLiteral( "package" ) ) )
  {
    packageElement.removeAttribute( QStringLiteral( "sidecar" ) );
    packageElement.removeAttribute( QStringLiteral( "keyId" ) );
    packageElement.removeAttribute( QStringLiteral( "style" ) );
    packageElement.setAttribute( QStringLiteral( "hasNoData" ), QStringLiteral( "0" ) );
    packageElement.removeAttribute( QStringLiteral( "noData" ) );
  }

  for ( const QgsMtpl::TileDatasetPackage &package : dataset.packages )
    QVERIFY( QFile::remove( package.descriptor.path ) );

  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( document.documentElement(), context ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.tileDataset() );
  QVERIFY( restoredLayer.tileDataset()->mapId.isEmpty() );
  QVERIFY( !restoredLayer.tileDataset()->hasExtent );
  QVERIFY( !restoredLayer.tileDataset()->matrix.hasBounds );
  for ( const QgsMtpl::TileDatasetPackage &package : restoredLayer.tileDataset()->packages )
  {
    QVERIFY( package.sidecarPath.isEmpty() );
    QVERIFY( package.descriptor.keyId.isEmpty() );
    QVERIFY( package.descriptor.stylePath.isEmpty() );
    QVERIFY( !package.descriptor.hasNoData );
  }
}

void TestMtplLayer::emptyPlainDatasetXmlRoundTrip()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QString datasetPath = QDir( directory.path() ).filePath( QStringLiteral( "empty-data" ) );
  QVERIFY( QDir().mkpath( datasetPath ) );
  const QString packagePath = QDir( datasetPath ).filePath( QStringLiteral( "0-0-0-0-0.ptp" ) );

  QString error;
  QVERIFY2( QgsMtplTest::writePtpFixture(
              packagePath,
              sTileSize,
              tileMetadata( QgsMtpl::TileScheme::Xyz, 0, 1 ),
              MTPL_STORAGE_PLAIN,
              QgsMtpl::CryptoKeys(),
              { { 0, 0, 0, 0, 0 } },
              {},
              error ),
            qPrintable( error ) );

  QgsMtpl::PackageDescriptor descriptor;
  QVERIFY2( QgsMtplPackageService::probePackage(
              packagePath,
              descriptor,
              error,
              QgsMtpl::CryptoKeys(),
              true,
              QgsMtpl::CredentialSource::None ),
            qPrintable( error ) );
  QCOMPARE( descriptor.format, QgsMtpl::PackageFormat::Ptp );
  QCOMPARE( descriptor.encryption, QgsMtpl::EncryptionState::Plain );
  QCOMPARE( descriptor.readiness, QgsMtpl::ReadinessState::PlainReady );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "rangeCount" ) ).toULongLong(), qulonglong( 1 ) );
  QCOMPARE( descriptor.metadata.value( QStringLiteral( "presentTileCount" ) ).toULongLong(), qulonglong( 0 ) );
  const QVariantList emptyRangeBounds = descriptor.metadata.value( QStringLiteral( "_mtplRangeBounds" ) ).toList();
  QCOMPARE( emptyRangeBounds.size(), 1 );
  QCOMPARE( emptyRangeBounds.constFirst().toMap().value( QStringLiteral( "presentCount" ) ).toULongLong(), 0ULL );
  QCOMPARE( descriptor.minimumZoom, -1 );
  QCOMPARE( descriptor.maximumZoom, -1 );

  QgsMtpl::TileDatasetBuildOptions buildOptions;
  buildOptions.ruleSnapshot = partitionRule(
    0,
    QStringLiteral( "test.empty-plain.rule" ),
    QStringLiteral( "空明文包规则" ) );
  const QgsMtpl::TileDatasetBuildResult buildResult = QgsMtpl::buildTileDataset(
    datasetPath,
    true,
    { descriptor },
    buildOptions );
  QVERIFY2( buildResult.ok(), qPrintable( buildResult.errorString() ) );
  QCOMPARE( buildResult.dataset.packages.size(), 1 );
  QCOMPARE( buildResult.dataset.packages.constFirst().ranges.size(), 1 );
  QCOMPARE( buildResult.dataset.packages.constFirst().ranges.constFirst().presentCount, 0ULL );
  QCOMPARE( buildResult.dataset.minimumZoom, 0 );
  QCOMPARE( buildResult.dataset.maximumZoom, 0 );

  QgsMtplPluginLayer layer( buildResult.dataset, QgsMtpl::CryptoKeys() );
  QVERIFY( layer.isValid() );
  QVERIFY( layer.isTileDataset() );
  QVERIFY( layer.tileDataset() );
  QCOMPARE( layer.tileDataset()->packages.size(), 1 );
  QCOMPARE( layer.tileDataset()->packages.constFirst().ranges.size(), 1 );

  const QgsTileMatrix emptyMatrix = layer.tileMatrixSet().tileMatrix( 0 );
  QgsMtplRasterInterface emptyRaster( buildResult.dataset, layer.tileMatrixSet(), {}, 0 );
  std::unique_ptr<QgsRasterBlock> emptyBlock(
    emptyRaster.block( 1, emptyMatrix.extent(), sTileSize, sTileSize ) );
  QVERIFY( emptyBlock );
  QVERIFY( emptyBlock->isValid() );
  QCOMPARE( qAlpha( emptyBlock->color( sTileSize / 2, sTileSize / 2 ) ), 0 );
  QVERIFY2( emptyRaster.errors().isEmpty(),
            qPrintable( emptyRaster.errors().join( QLatin1Char( '\n' ) ) ) );

  QgsMtpl::CryptoKeys syntheticKeys = QgsMtplTest::fixtureKeys();
  QVERIFY( syntheticKeys.isValid() );
  layer.setCryptoKeys( syntheticKeys );
  QVERIFY( layer.hasCryptoKeys() );

  const QString projectPath = QDir( directory.path() ).filePath( QStringLiteral( "empty-dataset-project.qgs" ) );
  QgsReadWriteContext writeContext;
  writeContext.setPathResolver( QgsPathResolver( projectPath ) );
  QDomDocument document( QStringLiteral( "qgis" ) );
  QDomElement mapLayerElement = document.createElement( QStringLiteral( "maplayer" ) );
  document.appendChild( mapLayerElement );
  QVERIFY( layer.writeLayerXml( mapLayerElement, document, writeContext ) );

  const QByteArray xml = document.toByteArray( 2 );
  QVERIFY( xml.contains( "<mtpl-dataset" ) );
  QVERIFY( !xml.contains( syntheticKeys.privateKey ) );
  QVERIFY( !xml.contains( syntheticKeys.deviceKey ) );
  QVERIFY( !xml.contains( "privateKey" ) );
  QVERIFY( !xml.contains( "deviceKey" ) );

  const QDomElement packageElement = mapLayerElement
                                       .firstChildElement( QStringLiteral( "mtpl-dataset" ) )
                                       .firstChildElement( QStringLiteral( "packages" ) )
                                       .firstChildElement( QStringLiteral( "package" ) );
  QVERIFY( !packageElement.isNull() );
  QCOMPARE( packageElement.attribute( QStringLiteral( "storage" ) ), QStringLiteral( "plain" ) );
  QCOMPARE( packageElement.attribute( QStringLiteral( "minimumZoom" ) ), QStringLiteral( "-1" ) );
  QCOMPARE( packageElement.attribute( QStringLiteral( "maximumZoom" ) ), QStringLiteral( "-1" ) );
  const QDomElement emptyRangeElement = packageElement.firstChildElement( QStringLiteral( "ranges" ) )
                                         .firstChildElement( QStringLiteral( "range" ) );
  QVERIFY( !emptyRangeElement.isNull() );
  QCOMPARE( emptyRangeElement.attribute( QStringLiteral( "presentCount" ) ), QStringLiteral( "0" ) );

  QFile projectFile( projectPath );
  QVERIFY( projectFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
  QCOMPARE( projectFile.write( xml ), static_cast<qint64>( xml.size() ) );
  projectFile.close();
  QVERIFY( projectFile.open( QIODevice::ReadOnly ) );
  QDomDocument restoredDocument;
  QVERIFY( restoredDocument.setContent( &projectFile ) );
  projectFile.close();

  QgsReadWriteContext readContext;
  readContext.setPathResolver( QgsPathResolver( projectPath ) );
  QgsMtplPluginLayer restoredLayer;
  QVERIFY( restoredLayer.readLayerXml( restoredDocument.documentElement(), readContext ) );
  QVERIFY( restoredLayer.isValid() );
  QVERIFY( restoredLayer.isTileDataset() );
  QVERIFY( !restoredLayer.hasCryptoKeys() );
  QVERIFY( restoredLayer.tileDataset() );
  QCOMPARE( restoredLayer.tileDataset()->packages.size(), 1 );
  QCOMPARE( restoredLayer.tileDataset()->packages.constFirst().ranges.size(), 1 );
  QCOMPARE( restoredLayer.tileDataset()->packages.constFirst().ranges.constFirst().presentCount, 0ULL );
  QCOMPARE( restoredLayer.tileDataset()->packages.constFirst().descriptor.minimumZoom, -1 );
  QCOMPARE( restoredLayer.tileDataset()->packages.constFirst().descriptor.maximumZoom, -1 );
  QCOMPARE( restoredLayer.tileDataset()->minimumZoom, 0 );
  QCOMPARE( restoredLayer.tileDataset()->maximumZoom, 0 );
  QCOMPARE( QDir::cleanPath( restoredLayer.tileDataset()->sourcePath ), QDir::cleanPath( datasetPath ) );

  syntheticKeys.clear();
}

void TestMtplLayer::ptpElevationIsRejected()
{
  QTemporaryDir directory;
  QVERIFY( directory.isValid() );
  const QString packagePath = directory.filePath( QStringLiteral( "0-0-0-0-0.ptp" ) );
  const QByteArray metadata = QByteArrayLiteral(
    R"({"tile_file_ext":"elevation","dataType":"int16","endianness":"big","scale":2.0,"offset":5.0,"noData":-32768,"mtpl_tile_matrix":{"version":1,"crs":"EPSG:4326","scheme":"xyz","top_left":[-180.0,90.0],"z0_tile_span":180.0,"z0_matrix_width":1,"z0_matrix_height":1,"scale_to_zoom_method":"mapbox","minimum_zoom":0,"maximum_zoom":0,"tile_size":33,"bounds":[-180.0,-90.0,0.0,90.0],"bounds_crs":"EPSG:4326"}})" );
  const QByteArray samples( 33 * 33 * 2, '\0' );

  QString error;
  QVERIFY2( QgsMtplTest::writePtpFixture(
              packagePath,
              33,
              metadata,
              MTPL_STORAGE_PLAIN,
              QgsMtpl::CryptoKeys(),
              { { 0, 0, 0, 0, 0 } },
              { { 0, 0, 0, samples } },
              error ),
            qPrintable( error ) );

  QgsMtpl::PackageDescriptor descriptor;
  QVERIFY( !QgsMtplPackageService::probePackage(
    packagePath,
    descriptor,
    error,
    QgsMtpl::CryptoKeys(),
    true,
    QgsMtpl::CredentialSource::None ) );
  QVERIFY2( error.contains( QStringLiteral( "DTP" ), Qt::CaseInsensitive ), qPrintable( error ) );
  QCOMPARE( descriptor.format, QgsMtpl::PackageFormat::Ptp );
  QCOMPARE( descriptor.payload, QgsMtpl::PayloadType::Elevation );
  QCOMPARE( descriptor.readiness, QgsMtpl::ReadinessState::Unsupported );

  // Legacy callers and project XML must not bypass the PTP image-only
  // contract by constructing a package layer directly.
  descriptor.readiness = QgsMtpl::ReadinessState::PlainReady;
  descriptor.encryption = QgsMtpl::EncryptionState::Plain;
  QgsMtplPluginLayer elevationLayer( descriptor, QgsMtpl::CryptoKeys() );
  QVERIFY( !elevationLayer.isValid() );

  descriptor.payload = QgsMtpl::PayloadType::VectorTile;
  QgsMtplPluginLayer vectorLayer( descriptor, QgsMtpl::CryptoKeys() );
  QVERIFY( !vectorLayer.isValid() );

  descriptor.payload = QgsMtpl::PayloadType::Unknown;
  QgsMtplPluginLayer unknownLayer( descriptor, QgsMtpl::CryptoKeys() );
  QVERIFY( !unknownLayer.isValid() );
}

QGSTEST_MAIN( TestMtplLayer )
#include "test_mtpl_layer.moc"
