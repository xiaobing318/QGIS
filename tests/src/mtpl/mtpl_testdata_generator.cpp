/***************************************************************************
  mtpl_testdata_generator.cpp
  --------------------------
  Explicit, reproducible PTP regression corpus generation.
 ***************************************************************************/

#include "mtpltestfixtures.h"
#include "qgsapplication.h"
#include "qgsmtplpackageoperation.h"
#include "qgsmtplpackageservice.h"
#include "qgsmtpltileset.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>
#include <QTemporaryDir>

#include <algorithm>

namespace
{
  QString digest( const QByteArray &bytes )
  {
    return QString::fromLatin1( QCryptographicHash::hash( bytes, QCryptographicHash::Sha256 ).toHex() );
  }

  QString fileDigest( const QString &path, QString &error )
  {
    QFile file( path );
    if ( !file.open( QIODevice::ReadOnly ) )
    {
      error = QStringLiteral( "Unable to read %1: %2" ).arg( path, file.errorString() );
      return {};
    }
    QCryptographicHash hash( QCryptographicHash::Sha256 );
    if ( !hash.addData( &file ) )
    {
      error = QStringLiteral( "Unable to hash %1." ).arg( path );
      return {};
    }
    return QString::fromLatin1( hash.result().toHex() );
  }

  QByteArray metadata( const QString &mapId, int rootWidth = 1, int rootHeight = 1, int maximumZoom = 19, int tileSize = 256 )
  {
    constexpr double edge = 20037508.342789244;
    const double span = 2.0 * edge / rootWidth;
    const QJsonObject matrix {
      { QStringLiteral( "version" ), 1 },
      { QStringLiteral( "crs" ), QStringLiteral( "EPSG:3857" ) },
      { QStringLiteral( "scheme" ), QStringLiteral( "xyz" ) },
      { QStringLiteral( "top_left" ), QJsonArray { -edge, edge } },
      { QStringLiteral( "z0_tile_span" ), span },
      { QStringLiteral( "z0_matrix_width" ), rootWidth },
      { QStringLiteral( "z0_matrix_height" ), rootHeight },
      { QStringLiteral( "min_zoom" ), 0 },
      { QStringLiteral( "max_zoom" ), maximumZoom },
      { QStringLiteral( "tile_size" ), tileSize },
      { QStringLiteral( "bounds" ), QJsonArray { -edge, edge - span * rootHeight, edge, edge } },
      { QStringLiteral( "bounds_crs" ), QStringLiteral( "EPSG:3857" ) }
    };
    return QJsonDocument( QJsonObject {
      { QStringLiteral( "map_id" ), mapId },
      { QStringLiteral( "tile_file_ext" ), QStringLiteral( "png" ) },
      { QStringLiteral( "mtpl_tile_matrix" ), matrix }
    } ).toJson( QJsonDocument::Compact );
  }

  class Corpus
  {
    public:
      explicit Corpus( const QString &root ) : mRoot( root ) {}

      bool generate()
      {
        for ( const QByteArray &format : { QByteArrayLiteral( "png" ), QByteArrayLiteral( "jpeg" ), QByteArrayLiteral( "webp" ) } )
        {
          if ( !QImageWriter::supportedImageFormats().contains( format ) )
          {
            error = QStringLiteral( "Qt image writer %1 is unavailable. Use the QGIS runtime environment." ).arg( QString::fromLatin1( format ) );
            return false;
          }
        }
        if ( QFileInfo::exists( mRoot ) || !QDir().mkpath( mRoot ) )
        {
          error = QStringLiteral( "The output root must be a new directory: %1" ).arg( mRoot );
          return false;
        }
        const QByteArray png = QgsMtplTest::rasterTileImage( "png", 256, 256, 1, error );
        const QByteArray otherPng = QgsMtplTest::rasterTileImage( "png", 256, 256, 2, error );
        const QByteArray smallPng = QgsMtplTest::rasterTileImage( "png", 129, 129, 3, error );
        if ( png.isEmpty() || otherPng.isEmpty() || smallPng.isEmpty() )
          return false;

        const QgsMtpl::CryptoKeys keysA = QgsMtplTest::fixtureKeys();
        const QgsMtpl::CryptoKeys keysB = QgsMtplTest::differentFixtureKeys();
        if ( !credential( QStringLiteral( "a" ), keysA ) || !credential( QStringLiteral( "b" ), keysB ) )
          return false;

        const QList<QPair<QByteArray, QString>> formats {
          { QByteArrayLiteral( "png" ), QStringLiteral( "png" ) },
          { QByteArrayLiteral( "jpeg" ), QStringLiteral( "jpg" ) },
          { QByteArrayLiteral( "webp" ), QStringLiteral( "webp" ) },
          { QByteArrayLiteral( "png" ), QStringLiteral( "bin" ) }
        };
        for ( const auto &format : formats )
        {
          const QString path = QStringLiteral( "raw/%1" ).arg( format.second == QLatin1String( "bin" ) ? QStringLiteral( "bin-png" ) : format.second );
          const QByteArray bytes = QgsMtplTest::rasterTileImage( format.first, 256, 256, 1, error );
          if ( bytes.isEmpty() || !rawTree( path, format.second, bytes, bytes ) )
            return false;
          addCase( path, QStringLiteral( "raw-tiles" ), QStringLiteral( "create-and-load" ),
                   QStringLiteral( "Create plain and encrypted PTP with tile size 256 and metadata {}. Load the resulting file. Three tiles at Z0 and Z1 must survive conversion byte-for-byte." ) );
        }
        for ( const int tileSize : { 33, 129 } )
        {
          const QString path = QStringLiteral( "raw/png-%1" ).arg( tileSize );
          const QByteArray bytes = QgsMtplTest::rasterTileImage( "png", tileSize, tileSize, 1, error );
          if ( bytes.isEmpty() || !rawTree( path, QStringLiteral( "png" ), bytes, bytes ) )
            return false;
          addCase( path, QStringLiteral( "raw-tiles" ), QStringLiteral( "create-and-load" ),
                   QStringLiteral( "Create plain and encrypted PTP with tile size %1 and metadata {}. Matching image and header dimensions are valid." ).arg( tileSize ), QString(), tileSize );
        }
        const QList<QPair<QString, QByteArray>> invalidImages {
          { QStringLiteral( "bad-second-image" ), QByteArrayLiteral( "not an image" ) },
          { QStringLiteral( "second-size-mismatch" ), smallPng },
          { QStringLiteral( "truncated-png-tail" ), png.left( png.size() - 2 ) }
        };
        for ( const auto &item : invalidImages )
        {
          const QString path = QStringLiteral( "raw/%1" ).arg( item.first );
          if ( !rawTree( path, QStringLiteral( "png" ), png, item.second ) )
            return false;
          addCase( path, QStringLiteral( "raw-tiles" ), QStringLiteral( "reject-without-output" ),
                   QStringLiteral( "The first image is valid. Reject the later invalid image before committing a PTP or key sidecar." ) );
        }
        const QByteArray jpeg = QgsMtplTest::rasterTileImage( "jpeg", 256, 256, 1, error );
        if ( jpeg.isEmpty() || !rawTree( QStringLiteral( "raw/truncated-jpeg" ), QStringLiteral( "jpg" ), jpeg, jpeg.left( jpeg.size() / 3 ) ) )
          return false;
        addCase( QStringLiteral( "raw/truncated-jpeg" ), QStringLiteral( "raw-tiles" ), QStringLiteral( "reject-without-output" ),
                 QStringLiteral( "The later JPEG retains only its first third. Reject the incomplete image even if the Qt decoder returns recoverable pixels." ) );
        const QList<QPair<QString, QString>> invalidCoordinates {
          { QStringLiteral( "duplicate-coordinate" ), QStringLiteral( "0/00/00.png" ) },
          { QStringLiteral( "out-of-matrix-coordinate" ), QStringLiteral( "0/1/0.png" ) },
          { QStringLiteral( "zoom31" ), QStringLiteral( "31/0/0.png" ) },
          { QStringLiteral( "wrong-layout" ), QStringLiteral( "unrelated.txt" ) }
        };
        for ( const auto &item : invalidCoordinates )
        {
          const QString path = QStringLiteral( "raw/%1" ).arg( item.first );
          if ( !write( path + QStringLiteral( "/0/0/0.png" ), png ) || !write( path + QLatin1Char( '/' ) + item.second, otherPng ) )
            return false;
          addCase( path, QStringLiteral( "raw-tiles" ), QStringLiteral( "reject-without-output" ),
                   QStringLiteral( "Use default Web Mercator metadata {}. Reject the invalid layout or normalized coordinate." ) );
        }

        const QList<QPair<QString, QByteArray>> invalidMetadata {
          { QStringLiteral( "invalid-json" ), QByteArrayLiteral( "{broken" ) },
          { QStringLiteral( "non-object" ), QByteArrayLiteral( "[]" ) },
          { QStringLiteral( "declared-elevation" ), QByteArrayLiteral( "{\"type\":\"elevation\"}" ) },
          { QStringLiteral( "invalid-matrix" ), QByteArrayLiteral( "{\"mtpl_tile_matrix\":{\"version\":1,\"z0_matrix_width\":0}}" ) },
          { QStringLiteral( "size-conflict" ), metadata( QStringLiteral( "size-conflict" ), 1, 1, 19, 129 ) }
        };
        for ( const auto &item : invalidMetadata )
        {
          const QString path = QStringLiteral( "metadata/%1.json" ).arg( item.first );
          if ( !write( path, item.second ) )
            return false;
          addCase( path, QStringLiteral( "metadata" ), QStringLiteral( "reject-without-output" ),
                   QStringLiteral( "Use raw/png as input, tile size 256, and paste this metadata into the PTP creation form." ) );
        }
        addCase( QStringLiteral( "raw/png" ), QStringLiteral( "output-name" ), QStringLiteral( "reject-without-output" ),
                 QStringLiteral( "Create using output name 8-11-3-0-0.ptp. Its Z8-11 address conflicts with the Z0-1 source tiles." ) );

        const QByteArray common = metadata( QStringLiteral( "ptp-regression-v2" ) );
        if ( !package( QStringLiteral( "encrypted-empty/0-7-0-0-0.ptp" ), common, keysA, { { 0, 0, 0, 0, 0 } }, {} ) ||
             !package( QStringLiteral( "mixed-storage/0-7-0-0-0.ptp" ), common, {}, { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, png } } ) ||
             !package( QStringLiteral( "mixed-storage/8-11-3-1-0.ptp" ), common, keysA, { { 8, 32, 32, 0, 0 } }, { { 8, 32, 0, otherPng } } ) ||
             !package( QStringLiteral( "mixed-credentials/8-11-3-0-0.ptp" ), common, keysA, { { 8, 0, 0, 0, 0 } }, { { 8, 0, 0, png } } ) ||
             !package( QStringLiteral( "mixed-credentials/8-11-3-1-0.ptp" ), common, keysB, { { 8, 32, 32, 0, 0 } }, { { 8, 32, 0, otherPng } } ) )
          return false;
        addCase( QStringLiteral( "encrypted-empty" ), QStringLiteral( "dataset" ), QStringLiteral( "credentialed-empty" ),
                 QStringLiteral( "Without a key loading is disabled. Synthetic key a permits an empty transparent dataset with an unverifiable-empty explanation." ), QStringLiteral( "a" ) );
        addCase( QStringLiteral( "mixed-storage" ), QStringLiteral( "dataset" ), QStringLiteral( "one-layer-with-key" ),
                 QStringLiteral( "Key a unlocks the encrypted package. Without a key the PTP dataset must not load partially." ), QStringLiteral( "a" ) );
        addCase( QStringLiteral( "mixed-credentials" ), QStringLiteral( "dataset" ), QStringLiteral( "reject-without-partial-layer" ),
                 QStringLiteral( "Key a and key b each unlock only one package. Neither single manual key may create a partial dataset." ) );

        const QByteArray stressMetadata = metadata( QStringLiteral( "stress-81" ), 9, 9, 0 );
        for ( quint32 y = 0; y < 9; ++y )
        {
          for ( quint32 x = 0; x < 9; ++x )
          {
            const QByteArray bytes = QgsMtplTest::rasterTileImage( "png", 256, 256, 1 + x + 9 * y, error );
            if ( bytes.isEmpty() || !package( QStringLiteral( "stress-81-visible/0-7-0-%1-%2.ptp" ).arg( x ).arg( y ),
                                              stressMetadata, {}, { { 0, x, x, y, y } }, { { 0, x, y, bytes } } ) )
              return false;
          }
        }
        addCase( QStringLiteral( "stress-81-visible" ), QStringLiteral( "dataset" ), QStringLiteral( "one-layer-81-visible-packages" ),
                 QStringLiteral( "Built-in rule one, EPSG:3857, 9 by 9 root matrix, only Z0. Zoom to layer: all 81 colored tiles must render. Repeat pan, refresh and remove the layer." ) );

        QString longLeaf = QStringLiteral( "paths/Unicode 空格" );
        while ( QDir( mRoot ).filePath( longLeaf + QStringLiteral( "/0-7-0-0-0.ptp" ) ).size() <= 280 )
          longLeaf += QStringLiteral( "/层级_long_path_0123456789_0123456789" );
        if ( !package( longLeaf + QStringLiteral( "/0-7-0-0-0.ptp" ), common, {}, { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, png } } ) )
          return false;
        addCase( longLeaf, QStringLiteral( "dataset" ), QStringLiteral( "one-layer-long-unicode-path" ),
                 QStringLiteral( "The actual absolute PTP path exceeds 280 characters. Probe, load, save and reopen a project." ) );

        const QList<QPair<QString, QByteArray>> replacements {
          { QStringLiteral( "original" ), png }, { QStringLiteral( "replacement-image" ), otherPng },
          { QStringLiteral( "replacement-bad-image" ), QByteArrayLiteral( "invalid replacement image" ) }
        };
        for ( const auto &item : replacements )
        {
          if ( !package( QStringLiteral( "source-changes/%1/0-7-0-0-0.ptp" ).arg( item.first ), common, {},
                         { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, item.second } } ) )
            return false;
        }
        if ( !package( QStringLiteral( "source-changes/replacement-contract/0-7-0-0-0.ptp" ), metadata( QStringLiteral( "different-map" ) ), {},
                       { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, otherPng } } ) ||
             !package( QStringLiteral( "project-restore/encrypted/0-7-0-0-0.ptp" ), common, keysA,
                       { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, png } } ) ||
             !package( QStringLiteral( "invalid-package-name/wrong-name.ptp" ), common, {},
                       { { 0, 0, 0, 0, 0 } }, { { 0, 0, 0, png } } ) )
          return false;
        addCase( QStringLiteral( "source-changes" ), QStringLiteral( "source-replacement" ), QStringLiteral( "refresh-or-degrade-with-explanation" ),
                 QStringLiteral( "Work on copies. Load original, replace its file using each sibling, refresh and reopen a saved QGZ. An image-only change updates pixels. A changed contract or bad image must not silently reuse cached pixels. Delete and restore the copy as well." ) );
        addCase( QStringLiteral( "project-restore/encrypted" ), QStringLiteral( "project" ), QStringLiteral( "restorable-encrypted-layer" ),
                 QStringLiteral( "Save QGZ with and without remembering key a, close and reopen QGIS, then move a copy of the project and dataset together. No plaintext key may enter project XML. Migration requires an actual legacy mtpl-package XML fixture. Current single-file saves use the dataset format." ), QStringLiteral( "a" ) );
        addCase( QStringLiteral( "invalid-package-name" ), QStringLiteral( "dataset" ), QStringLiteral( "reject-directory-allow-single-file" ),
                 QStringLiteral( "Selecting the directory is invalid. Selecting wrong-name.ptp directly uses single-file compatibility mode." ) );
        return finish();
      }

      QString error;

    private:
      bool write( const QString &relative, const QByteArray &bytes )
      {
        const QString path = QDir( mRoot ).filePath( relative );
        if ( !QDir().mkpath( QFileInfo( path ).absolutePath() ) )
        {
          error = QStringLiteral( "Unable to create parent directory for %1." ).arg( path );
          return false;
        }
        QFile file( path );
        if ( !file.open( QIODevice::WriteOnly | QIODevice::NewOnly ) || file.write( bytes ) != bytes.size() )
        {
          error = QStringLiteral( "Unable to write %1: %2" ).arg( path, file.errorString() );
          return false;
        }
        return true;
      }

      bool credential( const QString &id, const QgsMtpl::CryptoKeys &keys )
      {
        const QString path = QStringLiteral( "credentials/%1.json" ).arg( id );
        return write( path, QJsonDocument( QJsonObject {
          { QStringLiteral( "scope" ), QStringLiteral( "synthetic-only" ) },
          { QStringLiteral( "id" ), id },
          { QStringLiteral( "privateKeyBase64" ), QString::fromLatin1( keys.privateKey ) },
          { QStringLiteral( "deviceKeyHex" ), QString::fromLatin1( keys.deviceKey ) }
        } ).toJson( QJsonDocument::Indented ) );
      }

      bool rawTree( const QString &path, const QString &suffix, const QByteArray &first, const QByteArray &second )
      {
        return write( path + QStringLiteral( "/0/0/0.%1" ).arg( suffix ), first ) &&
               write( path + QStringLiteral( "/1/0/0.%1" ).arg( suffix ), first ) &&
               write( path + QStringLiteral( "/1/1/1.%1" ).arg( suffix ), second );
      }

      bool package( const QString &relative, const QByteArray &packageMetadata, const QgsMtpl::CryptoKeys &keys,
                    const QList<QgsMtplTest::TileFixtureRange> &ranges, const QList<QgsMtplTest::TileFixtureEntry> &tiles )
      {
        if ( !QgsMtplTest::writePtpFixture( QDir( mRoot ).filePath( relative ), 256, packageMetadata,
                                           keys.isEmpty() ? MTPL_STORAGE_PLAIN : MTPL_STORAGE_ENCRYPTED, keys, ranges, tiles, error ) )
          return false;
        QJsonArray tileRecords;
        for ( const QgsMtplTest::TileFixtureEntry &tile : tiles )
        {
          QJsonObject record {
            { QStringLiteral( "z" ), static_cast<int>( tile.zoom ) },
            { QStringLiteral( "x" ), static_cast<int>( tile.x ) },
            { QStringLiteral( "y" ), static_cast<int>( tile.y ) },
            { QStringLiteral( "encoded_sha256" ), digest( tile.data ) }
          };
          const QImage image = QImage::fromData( tile.data ).convertToFormat( QImage::Format_RGBA8888 );
          if ( !image.isNull() )
          {
            QCryptographicHash hash( QCryptographicHash::Sha256 );
            for ( int row = 0; row < image.height(); ++row )
              hash.addData( reinterpret_cast<const char *>( image.constScanLine( row ) ), image.width() * 4 );
            record.insert( QStringLiteral( "decoded_rgba_sha256" ), QString::fromLatin1( hash.result().toHex() ) );
          }
          tileRecords.append( record );
        }
        mPackages.append( QJsonObject {
          { QStringLiteral( "path" ), relative },
          { QStringLiteral( "encrypted" ), !keys.isEmpty() },
          { QStringLiteral( "tile_size" ), 256 },
          { QStringLiteral( "metadata_sha256" ), digest( packageMetadata ) },
          { QStringLiteral( "tiles" ), tileRecords }
        } );
        return true;
      }

      void addCase( const QString &path, const QString &kind, const QString &expected, const QString &notes,
                    const QString &credentialId = QString(), int tileSize = 0 )
      {
        QJsonObject item {
          { QStringLiteral( "id" ), QStringLiteral( "case-%1" ).arg( mCases.size() + 1, 2, 10, QLatin1Char( '0' ) ) },
          { QStringLiteral( "path" ), path }, { QStringLiteral( "kind" ), kind },
          { QStringLiteral( "expected" ), expected }, { QStringLiteral( "notes" ), notes }
        };
        if ( !credentialId.isEmpty() )
          item.insert( QStringLiteral( "credential_id" ), credentialId );
        if ( tileSize > 0 )
          item.insert( QStringLiteral( "tile_size" ), tileSize );
        mCases.append( item );
      }

      bool finish()
      {
        QByteArray guide = QByteArrayLiteral( "# PTP regression corpus v2\n\nGenerated only on explicit request. Each dataset leaf is independent. The root is not a loadable dataset. Original synthetic fixtures are untouched.\n\nWork on copies for conversion, replacement and project relocation. Credentials a and b are test-only and unrelated to user data. No PBF samples are generated.\n\n| Path | Kind | Expected | Procedure |\n| --- | --- | --- | --- |\n" );
        for ( const QJsonValue &value : mCases )
        {
          const QJsonObject item = value.toObject();
          guide += QStringLiteral( "| `%1` | %2 | %3 | %4 |\n" )
                     .arg( item.value( QStringLiteral( "path" ) ).toString(), item.value( QStringLiteral( "kind" ) ).toString(),
                           item.value( QStringLiteral( "expected" ) ).toString(), item.value( QStringLiteral( "notes" ) ).toString() ).toUtf8();
        }
        if ( !write( QStringLiteral( "EXPECTED_RESULTS.md" ), guide ) )
          return false;
        QStringList paths;
        QDirIterator iterator( mRoot, QDir::Files, QDirIterator::Subdirectories );
        while ( iterator.hasNext() )
          paths.append( QDir( mRoot ).relativeFilePath( iterator.next() ).replace( '\\', '/' ) );
        std::sort( paths.begin(), paths.end() );
        QJsonArray files;
        for ( const QString &relative : paths )
        {
          const QString path = QDir( mRoot ).filePath( relative );
          const QString hash = fileDigest( path, error );
          if ( hash.isEmpty() )
            return false;
          files.append( QJsonObject {
            { QStringLiteral( "path" ), relative },
            { QStringLiteral( "size" ), QFileInfo( path ).size() },
            { QStringLiteral( "sha256" ), hash }
          } );
        }
        return write( QStringLiteral( "MANIFEST.json" ), QJsonDocument( QJsonObject {
          { QStringLiteral( "format_version" ), 2 },
          { QStringLiteral( "generator" ), QStringLiteral( "QGIS mtpl_testdata_generator using MTPL writer API and QImageWriter" ) },
          { QStringLiteral( "credential_scope" ), QStringLiteral( "synthetic-only" ) },
          { QStringLiteral( "cases" ), mCases }, { QStringLiteral( "packages" ), mPackages }, { QStringLiteral( "files" ), files }
        } ).toJson( QJsonDocument::Indented ) );
      }

      QString mRoot;
      QJsonArray mCases;
      QJsonArray mPackages;
  };

  bool verifyDataset( const QString &path, bool directory, const QgsMtpl::CryptoKeys &keys,
                       bool expectedValid, int expectedPackages, QString &error )
  {
    const QgsMtpl::ProbeResult probe = QgsMtplPackageService::probePath(
      path, keys, true, keys.isEmpty() ? QgsMtpl::CredentialSource::None : QgsMtpl::CredentialSource::Explicit );
    const QgsMtpl::TileDatasetBuildResult dataset = QgsMtpl::buildTileDataset( path, directory, probe.packages );
    const bool valid = probe.ok && dataset.ok() && dataset.dataset.isValid();
    if ( valid != expectedValid || ( valid && dataset.dataset.packages.size() != expectedPackages ) )
    {
      error = QStringLiteral( "Unexpected dataset result for %1 (expected %2, packages %3): %4 %5" )
                .arg( path, expectedValid ? QStringLiteral( "valid" ) : QStringLiteral( "invalid" ) )
                .arg( expectedPackages ).arg( probe.error, dataset.errorString() );
      return false;
    }
    if ( valid && QFileInfo( path ).fileName() == QLatin1String( "stress-81-visible" ) &&
         ( dataset.dataset.matrix.minimumZoom != 0 || dataset.dataset.matrix.maximumZoom != 0 ||
           dataset.dataset.matrix.z0MatrixWidth != 9 || dataset.dataset.matrix.z0MatrixHeight != 9 ) )
    {
      error = QStringLiteral( "The stress dataset must retain its explicit Z0-only 9 by 9 matrix." );
      return false;
    }
    return true;
  }

  bool verifySemantics( const QString &root, const QJsonObject &manifest, QString &error )
  {
    const QgsMtpl::CryptoKeys keysA = QgsMtplTest::fixtureKeys();
    const QgsMtpl::CryptoKeys keysB = QgsMtplTest::differentFixtureKeys();
    const QDir directory( root );
    QSet<QString> recordedFiles;
    for ( const QJsonValue &value : manifest.value( QStringLiteral( "files" ) ).toArray() )
      recordedFiles.insert( value.toObject().value( QStringLiteral( "path" ) ).toString() );
    if ( manifest.value( QStringLiteral( "packages" ) ).toArray().isEmpty() || manifest.value( QStringLiteral( "cases" ) ).toArray().isEmpty() )
    {
      error = QStringLiteral( "The corpus has no semantic package or case records." );
      return false;
    }
    for ( const QJsonValue &value : manifest.value( QStringLiteral( "packages" ) ).toArray() )
    {
      const QJsonObject record = value.toObject();
      const QString relative = record.value( QStringLiteral( "path" ) ).toString();
      if ( !recordedFiles.contains( relative ) )
      {
        error = QStringLiteral( "An unrecorded package was requested for semantic validation." );
        return false;
      }
      const bool valid = !relative.startsWith( QLatin1String( "source-changes/replacement-bad-image/" ) );
      const QgsMtpl::CryptoKeys keys = !record.value( QStringLiteral( "encrypted" ) ).toBool() ? QgsMtpl::CryptoKeys() :
        relative == QLatin1String( "mixed-credentials/8-11-3-1-0.ptp" ) ? keysB : keysA;
      if ( !verifyDataset( directory.filePath( relative ), false, keys, valid, 1, error ) )
        return false;
    }

    QTemporaryDir scratch( QDir::tempPath() + QStringLiteral( "/mtpl-corpus-check-XXXXXX" ) );
    if ( !scratch.isValid() )
    {
      error = QStringLiteral( "Unable to create the isolated corpus verification workspace." );
      return false;
    }
    int operationNumber = 0;
    for ( const QJsonValue &value : manifest.value( QStringLiteral( "cases" ) ).toArray() )
    {
      const QJsonObject item = value.toObject();
      const QString relative = item.value( QStringLiteral( "path" ) ).toString();
      const QString path = directory.filePath( relative );
      const QString kind = item.value( QStringLiteral( "kind" ) ).toString();
      if ( !QFileInfo( path ).canonicalFilePath().startsWith( QFileInfo( root ).canonicalFilePath() + QLatin1Char( '/' ) ) )
      {
        error = QStringLiteral( "A semantic case is missing or lies outside the corpus: %1" ).arg( relative );
        return false;
      }
      if ( kind == QLatin1String( "raw-tiles" ) || kind == QLatin1String( "metadata" ) || kind == QLatin1String( "output-name" ) )
      {
        QByteArray creationMetadata = QByteArrayLiteral( "{}" );
        if ( kind == QLatin1String( "metadata" ) )
        {
          QFile file( path );
          if ( !file.open( QIODevice::ReadOnly ) )
          {
            error = QStringLiteral( "Unable to read metadata case %1." ).arg( path );
            return false;
          }
          creationMetadata = file.readAll();
        }
        for ( const bool encrypted : { false, true } )
        {
          const QString outputDirectory = scratch.filePath( QStringLiteral( "operation-%1" ).arg( ++operationNumber ) );
          if ( !QDir().mkpath( outputDirectory ) )
          {
            error = QStringLiteral( "Unable to prepare the isolated creation output." );
            return false;
          }
          QgsMtpl::PackageOperationRequest request;
          request.type = QgsMtpl::PackageOperationType::CreateTilePackage;
          request.outputFormat = QgsMtpl::PackageFormat::Ptp;
          request.sourcePath = kind == QLatin1String( "metadata" ) ? directory.filePath( QStringLiteral( "raw/png" ) ) : path;
          request.outputPath = QDir( outputDirectory ).filePath( kind == QLatin1String( "output-name" )
            ? QStringLiteral( "8-11-3-0-0.ptp" ) : QStringLiteral( "created.ptp" ) );
          request.tileSize = item.value( QStringLiteral( "tile_size" ) ).toInt( 256 );
          request.metadata = creationMetadata;
          request.encryptOutput = encrypted;
          request.generateOutputKeys = false;
          request.writeSidecar = false;
          request.outputKeys.keyId = QStringLiteral( "dba2fe86-64af-4c93-83ad-0ee1ecbe8b25" );
          request.outputKeys.privateKeyBase64 = keysA.privateKey;
          request.outputKeys.deviceKeyHex = keysA.deviceKey;
          const QgsMtpl::PackageOperationResult result = QgsMtpl::PackageOperations::execute( request );
          const bool expectedValid = item.value( QStringLiteral( "expected" ) ).toString() == QLatin1String( "create-and-load" );
          if ( result.ok != expectedValid || result.canceled ||
               ( !expectedValid && ( QFileInfo::exists( request.outputPath ) || !result.outputPaths.isEmpty() || !result.sidecarPath.isEmpty() ) ) )
          {
            error = QStringLiteral( "Unexpected PTP creation result for %1 (encrypted=%2): %3" )
                      .arg( relative ).arg( encrypted ).arg( result.error );
            return false;
          }
          if ( expectedValid && !verifyDataset( request.outputPath, false, encrypted ? keysA : QgsMtpl::CryptoKeys(), true, 1, error ) )
            return false;
          if ( !expectedValid && !QDir( outputDirectory ).entryList( QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot ).isEmpty() )
          {
            error = QStringLiteral( "A rejected creation left partial files for %1." ).arg( relative );
            return false;
          }
        }
      }
      else if ( kind == QLatin1String( "dataset" ) )
      {
        if ( relative == QLatin1String( "mixed-credentials" ) )
        {
          if ( !verifyDataset( path, true, keysA, false, 0, error ) || !verifyDataset( path, true, keysB, false, 0, error ) )
            return false;
        }
        else if ( relative == QLatin1String( "invalid-package-name" ) )
        {
          if ( !verifyDataset( path, true, {}, false, 0, error ) )
            return false;
        }
        else
        {
          const bool encrypted = !item.value( QStringLiteral( "credential_id" ) ).toString().isEmpty();
          const int expectedCount = relative == QLatin1String( "stress-81-visible" ) ? 81 : relative == QLatin1String( "mixed-storage" ) ? 2 : 1;
          if ( encrypted && !verifyDataset( path, true, {}, false, 0, error ) )
            return false;
          if ( !verifyDataset( path, true, encrypted ? keysA : QgsMtpl::CryptoKeys(), true, expectedCount, error ) )
            return false;
        }
      }
    }
    if ( !verifyDataset( root, true, {}, false, 0, error ) )
      return false;
    return true;
  }

  bool verify( const QString &root, QString &error )
  {
    QFile manifest( QDir( root ).filePath( QStringLiteral( "MANIFEST.json" ) ) );
    if ( !manifest.open( QIODevice::ReadOnly ) )
    {
      error = QStringLiteral( "Unable to open MANIFEST.json." );
      return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson( manifest.readAll(), &parseError );
    const QJsonArray files = document.object().value( QStringLiteral( "files" ) ).toArray();
    if ( parseError.error != QJsonParseError::NoError || document.object().value( QStringLiteral( "format_version" ) ).toInt() != 2 || files.isEmpty() )
    {
      error = QStringLiteral( "Invalid corpus manifest." );
      return false;
    }
    QSet<QString> expectedPaths;
    for ( const QJsonValue &value : files )
    {
      const QJsonObject record = value.toObject();
      const QString relative = record.value( QStringLiteral( "path" ) ).toString();
      const QString path = QDir( root ).filePath( relative );
      const QString canonical = QFileInfo( path ).canonicalFilePath();
      const QString rootCanonical = QFileInfo( root ).canonicalFilePath() + QLatin1Char( '/' );
      if ( relative.isEmpty() || QDir::isAbsolutePath( relative ) || relative.contains( '\\' ) ||
           QDir::cleanPath( relative ) != relative || relative.startsWith( QLatin1String( "../" ) ) ||
           expectedPaths.contains( relative ) || !canonical.startsWith( rootCanonical ) ||
           QFileInfo( path ).size() != record.value( QStringLiteral( "size" ) ).toInteger() ||
           fileDigest( path, error ) != record.value( QStringLiteral( "sha256" ) ).toString() )
      {
        error = QStringLiteral( "Corpus file is missing, changed, or outside the root: %1" ).arg( relative );
        return false;
      }
      expectedPaths.insert( relative );
    }
    QDirIterator iterator( root, QDir::Files, QDirIterator::Subdirectories );
    while ( iterator.hasNext() )
    {
      const QString relative = QDir( root ).relativeFilePath( iterator.next() ).replace( '\\', '/' );
      if ( relative != QLatin1String( "MANIFEST.json" ) && !expectedPaths.contains( relative ) )
      {
        error = QStringLiteral( "Unrecorded corpus file: %1" ).arg( relative );
        return false;
      }
    }
    return verifySemantics( root, document.object(), error );
  }
}

int main( int argc, char **argv )
{
  QTemporaryDir profile( QDir::tempPath() + QStringLiteral( "/mtpl-corpus-profile-XXXXXX" ) );
  if ( !profile.isValid() )
  {
    QTextStream( stderr ) << "Unable to create the isolated QGIS profile.\n";
    return 1;
  }
  QgsApplication application( argc, argv, false, profile.path() );
  QCoreApplication::setApplicationName( QStringLiteral( "mtpl_testdata_generator" ) );
  QCommandLineParser parser;
  parser.setApplicationDescription( QStringLiteral( "Generate an explicit new PTP regression corpus, or verify an existing corpus without changes." ) );
  parser.addHelpOption();
  parser.addOption( QCommandLineOption( QStringLiteral( "output-root" ), QStringLiteral( "Explicit corpus directory. Generation refuses an existing path." ), QStringLiteral( "directory" ) ) );
  parser.addOption( QCommandLineOption( QStringLiteral( "verify" ), QStringLiteral( "Verify hashes, dataset contracts and creation results without modifying the corpus." ) ) );
  parser.process( application );
  const QString requested = parser.value( QStringLiteral( "output-root" ) ).trimmed();
  if ( requested.isEmpty() || !parser.positionalArguments().isEmpty() )
    parser.showHelp( 2 );
  const QString root = QFileInfo( requested ).absoluteFilePath();
  QgsApplication::init( profile.path() );
  QgsApplication::initQgis();
  QString error;
  if ( parser.isSet( QStringLiteral( "verify" ) ) )
  {
    if ( !verify( root, error ) )
    {
      QTextStream( stderr ) << error << '\n';
      QgsApplication::exitQgis();
      return 1;
    }
  }
  else
  {
    Corpus corpus( root );
    if ( !corpus.generate() || !verify( root, error ) )
    {
      QTextStream( stderr ) << ( corpus.error.isEmpty() ? error : corpus.error )
                           << "\nGeneration did not finish. Any partial new output is retained for inspection.\n";
      QgsApplication::exitQgis();
      return 1;
    }
  }
  QTextStream( stdout ) << ( parser.isSet( QStringLiteral( "verify" ) ) ? "Verified " : "Generated and verified " ) << root << '\n';
  QgsApplication::exitQgis();
  return 0;
}
