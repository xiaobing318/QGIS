/***************************************************************************
  qgsmtpltileset.h
  ----------------
  MTPL tile matrix, package addressing, and dataset validation contracts.
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

#ifndef QGSMTPLTILESET_H
#define QGSMTPLTILESET_H

#include "../qgsmtplpackage.h"
#include "qgsmtplpartitionrule.h"

#include <QList>
#include <QHash>
#include <QString>
#include <QStringList>

namespace QgsMtpl
{

enum class TileScheme
{
  Xyz,
  Tms
};

/**
 * Tile matrix used by an MTPL tile dataset.
 *
 * Non-Web-Mercator datasets must provide an mtpl_tile_matrix version 1 object
 * containing crs, scheme, top_left, z0_tile_span, z0_matrix_width,
 * z0_matrix_height, and scale_to_zoom_method. Bounds and bounds_crs are
 * optional but must occur together. Old packages without this object receive
 * the standard EPSG:3857 global XYZ/TMS matrix only when their CRS is empty or
 * EPSG:3857.
 */
struct TileMatrixDefinition
{
  QString crsAuthId = QStringLiteral( "EPSG:3857" );
  TileScheme scheme = TileScheme::Xyz;
  int tileSize = 256;
  double topLeftX = -20037508.342789244;
  double topLeftY = 20037508.342789244;
  double z0TileSpan = 40075016.685578488;
  quint64 z0MatrixWidth = 1;
  quint64 z0MatrixHeight = 1;
  QString scaleToZoomMethod = QStringLiteral( "mapbox" );
  int minimumZoom = 0;
  int maximumZoom = 30;
  bool hasBounds = false;
  double boundsXMinimum = 0.0;
  double boundsYMinimum = 0.0;
  double boundsXMaximum = 0.0;
  double boundsYMaximum = 0.0;
  QString boundsCrsAuthId;
  bool zoomRangeExplicit = false;
  bool legacyFallbackApplied = false;

  bool isValid( QString *error = nullptr ) const;
  quint64 matrixWidth( int zoom, bool *ok = nullptr ) const;
  quint64 matrixHeight( int zoom, bool *ok = nullptr ) const;
  double tileSpan( int zoom, bool *ok = nullptr ) const;

  static bool fromMetadata( const QVariantMap &metadata,
                            int packageTileSize,
                            int observedMinimumZoom,
                            int observedMaximumZoom,
                            TileMatrixDefinition &definition,
                            QString *error = nullptr );
  static bool fromPackageDescriptor( const PackageDescriptor &package,
                                     TileMatrixDefinition &definition,
                                     QString *error = nullptr );
};

//! Address encoded by minZ-maxZ-baseZ-X-Y.ptp.
struct TilePackageAddress
{
  int minZoom = -1;
  int maxZoom = -1;
  int baseZoom = -1;
  quint32 packageX = 0;
  quint32 packageY = 0;

  PartitionBand band() const;
  QString fileName() const;
  QString key() const;
  bool isValid( QString *error = nullptr ) const;
  bool operator==( const TilePackageAddress &other ) const;
  bool operator!=( const TilePackageAddress &other ) const { return !( *this == other ); }
};

//! Probe-time range summary copied from PackageDescriptor metadata.
struct TileRangeRecord
{
  int zoom = -1;
  quint32 xMin = 0;
  quint32 xMax = 0;
  quint32 yMin = 0;
  quint32 yMax = 0;
  quint32 presentXMin = 0;
  quint32 presentXMax = 0;
  quint32 presentYMin = 0;
  quint32 presentYMax = 0;
  quint64 presentCount = 0;

  bool isValid( QString *error = nullptr ) const;
};

//! Structural metadata declarations captured from one PTP package.
struct TilePackageSourceContract
{
  //! FALSE for legacy project XML which predates source contract snapshots.
  bool available = false;
  bool zoomRangeExplicit = false;
  int minimumZoom = 0;
  int maximumZoom = 30;
  bool hasBounds = false;
  double boundsXMinimum = 0.0;
  double boundsYMinimum = 0.0;
  double boundsXMaximum = 0.0;
  double boundsYMaximum = 0.0;
  QString boundsCrsAuthId;
  bool legacyFallbackApplied = false;
  bool mapIdDeclared = false;
  QString mapId;
  bool payloadDeclared = false;
  PayloadType payload = PayloadType::Unknown;
};

struct TileDatasetPackage
{
  PackageDescriptor descriptor;
  TilePackageAddress address;
  QString relativePath;
  qint64 fileLastModifiedMs = -1;
  QString sidecarPath;
  QList<TileRangeRecord> ranges;
  TilePackageSourceContract sourceContract;
};

struct TileDatasetDescriptor
{
  QString sourcePath;
  bool directorySource = false;
  PartitionRule partitionRule;
  TileMatrixDefinition matrix;
  PayloadType payload = PayloadType::Unknown;
  QString mapId;
  QList<TileDatasetPackage> packages;
  //! Direct address-to-package lookup used by renderers in aggregate mode.
  QHash<QString, int> packageIndexByAddress;
  int minimumZoom = -1;
  int maximumZoom = -1;
  bool compatibilityMode = false;
  bool hasExtent = false;
  double extentXMinimum = 0.0;
  double extentYMinimum = 0.0;
  double extentXMaximum = 0.0;
  double extentYMaximum = 0.0;

  bool isValid( QString *error = nullptr ) const;
};

struct ResolvedTile
{
  int packageIndex = -1;
  TilePackageAddress packageAddress;
  quint32 storageX = 0;
  quint32 storageY = 0;

  bool isValid() const { return packageIndex >= 0; }
};

class TileResolver final
{
  public:
    static bool parsePackageFileName( const QString &fileName,
                                      TilePackageAddress &address,
                                      QString *error = nullptr );

    static bool storageCoordinate( const TileMatrixDefinition &matrix,
                                   int zoom,
                                   quint64 requestX,
                                   quint64 requestY,
                                   quint32 &storageX,
                                   quint32 &storageY,
                                   QString *error = nullptr );

    static bool packageAddressForTile( const PartitionRule &rule,
                                       const TileMatrixDefinition &matrix,
                                       int zoom,
                                       quint64 requestX,
                                       quint64 requestY,
                                       TilePackageAddress &address,
                                       quint32 *storageX = nullptr,
                                       quint32 *storageY = nullptr,
                                       QString *error = nullptr );

    static bool resolve( const TileDatasetDescriptor &dataset,
                         int zoom,
                         quint64 requestX,
                         quint64 requestY,
                         ResolvedTile &resolved,
                         QString *error = nullptr );
};

enum class TileDatasetIssueSeverity
{
  Warning,
  Error
};

struct TileDatasetIssue
{
  TileDatasetIssueSeverity severity = TileDatasetIssueSeverity::Error;
  QString code;
  QString message;
  QString packagePath;
};

struct TileDatasetBuildOptions
{
  //! Preserve legacy single-package addressing without interpreting its file name.
  //! Directory sources always require the normal partition-rule contract.
  bool singleFileCompatibility = false;

  //! Optional stable ID. Empty means infer the rule from package names.
  QString selectedRuleId;

  //! Optional custom/built-in snapshot. A nonempty ID gives it precedence.
  PartitionRule ruleSnapshot;

  //! Candidate custom rules. Built-ins are always considered as well.
  QList<PartitionRule> availableRules;
};

struct TileDatasetBuildResult
{
  TileDatasetDescriptor dataset;
  QList<TileDatasetIssue> issues;

  bool ok() const;
  QString errorString() const;
  QStringList warningMessages() const;
};

/**
 * Builds one logical XYZ-like raster dataset from already probed PTP packages.
 *
 * The builder validates package names, rule membership, metadata/matrix
 * agreement, tile size, payload, map ID, duplicate addresses, and any range
 * summaries exposed by the probe service. It performs no package I/O. The
 * returned dataset remains empty when any input package fails validation.
 */
TileDatasetBuildResult buildTileDataset( const QString &sourcePath,
                                         bool isDirectory,
                                         const QList<PackageDescriptor> &packages,
                                         const TileDatasetBuildOptions &options = TileDatasetBuildOptions() );

} // namespace QgsMtpl

#endif // QGSMTPLTILESET_H
