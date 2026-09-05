/***************************************************************************
  qgsmtplpartitionrule.h
  ----------------------
  Partition-rule value types and persistence for the built-in MTPL plugin.
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

#ifndef QGSMTPLPARTITIONRULE_H
#define QGSMTPLPARTITIONRULE_H

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>

class QgsSettings;

namespace QgsMtpl
{

//! Inclusive zoom interval stored in packages rooted at baseZoom tiles.
struct PartitionBand
{
  int minZoom = -1;
  int maxZoom = -1;
  int baseZoom = -1;

  bool isValid( QString *error = nullptr ) const;
  bool operator==( const PartitionBand &other ) const;
  bool operator!=( const PartitionBand &other ) const { return !( *this == other ); }
};

//! Named partition rule. Built-in rules have stable, non-localized IDs.
struct PartitionRule
{
  QString id;
  QString name;
  bool builtIn = false;
  QList<PartitionBand> bands;

  bool isValid( QString *error = nullptr ) const;
  const PartitionBand *bandForZoom( int zoom ) const;
  bool operator==( const PartitionRule &other ) const;
  bool operator!=( const PartitionRule &other ) const { return !( *this == other ); }
};

/**
 * Provides the two predefined rules and profile-scoped custom-rule storage.
 *
 * Only custom rules are serialized. Built-in rules are supplied by code and
 * cannot be shadowed or removed by profile data. The JSON codec is public so
 * callers can import, export, and unit-test rule sets without touching a user
 * profile.
 */
class PartitionRuleStore final
{
  public:
    static QString ruleOneId();
    static QString ruleTwoId();
    static QList<PartitionRule> builtInRules();

    //! Validates rule structure, unique IDs/names, and built-in protection.
    static bool validateCustomRules( const QList<PartitionRule> &rules, QString *error = nullptr );

    //! Returns built-in rules followed by validated custom rules.
    static QList<PartitionRule> combineWithBuiltIns( const QList<PartitionRule> &customRules,
                                                     QString *error = nullptr );

    static bool findRule( const QList<PartitionRule> &rules,
                          const QString &id,
                          PartitionRule &rule );

    static QByteArray toJson( const QList<PartitionRule> &customRules,
                              QString *error = nullptr );
    static bool fromJson( const QByteArray &json,
                          QList<PartitionRule> &customRules,
                          QString *error = nullptr );

    static QString settingsKey();
    static QList<PartitionRule> loadCustomRules( QgsSettings &settings,
                                                 QString *error = nullptr );
    static bool saveCustomRules( QgsSettings &settings,
                                 const QList<PartitionRule> &customRules,
                                 QString *error = nullptr );

    //! Convenience overloads using the active QGIS profile.
    static QList<PartitionRule> loadCustomRules( QString *error = nullptr );
    static bool saveCustomRules( const QList<PartitionRule> &customRules,
                                 QString *error = nullptr );
    static QList<PartitionRule> loadAllRules( QString *error = nullptr );
};

} // namespace QgsMtpl

Q_DECLARE_METATYPE( QgsMtpl::PartitionBand )
Q_DECLARE_METATYPE( QgsMtpl::PartitionRule )

#endif // QGSMTPLPARTITIONRULE_H
