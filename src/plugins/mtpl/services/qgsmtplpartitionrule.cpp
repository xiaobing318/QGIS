/***************************************************************************
  qgsmtplpartitionrule.cpp
  ------------------------
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

#include "qgsmtplpartitionrule.h"

#include "qgssettings.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>

#include <limits>

namespace
{
  constexpr int sJsonVersion = 1;
  constexpr int sMinimumZoom = 0;
  constexpr int sMaximumZoom = 30;

  QString translatedRuleName( const char *source )
  {
    return QCoreApplication::translate( "QgsMtplPartitionRuleStore", source );
  }

  void setError( QString *error, const QString &message )
  {
    if ( error )
      *error = message;
  }

  bool jsonInteger( const QJsonObject &object, const QString &key, int &value )
  {
    const QJsonValue jsonValue = object.value( key );
    if ( !jsonValue.isDouble() )
      return false;
    const double number = jsonValue.toDouble();
    const int integer = jsonValue.toInt( std::numeric_limits<int>::min() );
    if ( static_cast<double>( integer ) != number )
      return false;
    value = integer;
    return true;
  }

  QJsonObject bandToJson( const QgsMtpl::PartitionBand &band )
  {
    QJsonObject object;
    object.insert( QStringLiteral( "minZoom" ), band.minZoom );
    object.insert( QStringLiteral( "maxZoom" ), band.maxZoom );
    object.insert( QStringLiteral( "baseZoom" ), band.baseZoom );
    return object;
  }

  bool bandFromJson( const QJsonValue &value, QgsMtpl::PartitionBand &band, QString *error )
  {
    if ( !value.isObject() )
    {
      setError( error, QStringLiteral( "分包区间必须是 JSON 对象。" ) );
      return false;
    }

    const QJsonObject object = value.toObject();
    if ( !jsonInteger( object, QStringLiteral( "minZoom" ), band.minZoom ) ||
         !jsonInteger( object, QStringLiteral( "maxZoom" ), band.maxZoom ) ||
         !jsonInteger( object, QStringLiteral( "baseZoom" ), band.baseZoom ) )
    {
      setError( error, QStringLiteral( "分包区间的 minZoom、maxZoom 和 baseZoom 必须是整数。" ) );
      return false;
    }
    return band.isValid( error );
  }

  QJsonObject ruleToJson( const QgsMtpl::PartitionRule &rule )
  {
    QJsonArray bands;
    for ( const QgsMtpl::PartitionBand &band : rule.bands )
      bands.append( bandToJson( band ) );

    QJsonObject object;
    object.insert( QStringLiteral( "id" ), rule.id );
    object.insert( QStringLiteral( "name" ), rule.name );
    object.insert( QStringLiteral( "bands" ), bands );
    return object;
  }

  bool ruleFromJson( const QJsonValue &value, QgsMtpl::PartitionRule &rule, QString *error )
  {
    if ( !value.isObject() )
    {
      setError( error, QStringLiteral( "分包规则必须是 JSON 对象。" ) );
      return false;
    }

    const QJsonObject object = value.toObject();
    if ( !object.value( QStringLiteral( "id" ) ).isString() ||
         !object.value( QStringLiteral( "name" ) ).isString() ||
         !object.value( QStringLiteral( "bands" ) ).isArray() )
    {
      setError( error, QStringLiteral( "分包规则缺少有效的 id、name 或 bands。" ) );
      return false;
    }

    rule = QgsMtpl::PartitionRule();
    rule.id = object.value( QStringLiteral( "id" ) ).toString();
    rule.name = object.value( QStringLiteral( "name" ) ).toString();
    rule.builtIn = false;
    const QJsonArray bands = object.value( QStringLiteral( "bands" ) ).toArray();
    for ( qsizetype index = 0; index < bands.size(); ++index )
    {
      QgsMtpl::PartitionBand band;
      QString bandError;
      if ( !bandFromJson( bands.at( index ), band, &bandError ) )
      {
        setError( error, QStringLiteral( "规则“%1”的第 %2 个区间无效：%3" )
                   .arg( rule.name.isEmpty() ? rule.id : rule.name )
                   .arg( index + 1 )
                   .arg( bandError ) );
        return false;
      }
      rule.bands.append( band );
    }
    return rule.isValid( error );
  }
}

bool QgsMtpl::PartitionBand::isValid( QString *error ) const
{
  if ( minZoom < sMinimumZoom || minZoom > sMaximumZoom ||
       maxZoom < sMinimumZoom || maxZoom > sMaximumZoom ||
       baseZoom < sMinimumZoom || baseZoom > sMaximumZoom )
  {
    setError( error, QStringLiteral( "缩放级别必须位于 0 至 30。" ) );
    return false;
  }
  if ( minZoom > maxZoom )
  {
    setError( error, QStringLiteral( "最小缩放级别不能大于最大缩放级别。" ) );
    return false;
  }
  if ( baseZoom > minZoom )
  {
    setError( error, QStringLiteral( "基础缩放级别不能大于区间的最小缩放级别。" ) );
    return false;
  }
  if ( error )
    error->clear();
  return true;
}

bool QgsMtpl::PartitionBand::operator==( const PartitionBand &other ) const
{
  return minZoom == other.minZoom && maxZoom == other.maxZoom && baseZoom == other.baseZoom;
}

bool QgsMtpl::PartitionRule::isValid( QString *error ) const
{
  static const QRegularExpression idPattern( QStringLiteral( "^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$" ) );
  if ( !idPattern.match( id ).hasMatch() )
  {
    setError( error, QStringLiteral( "规则 ID 必须由 1 至 128 个字母、数字、点、下划线或连字符组成。" ) );
    return false;
  }
  if ( name.trimmed().isEmpty() )
  {
    setError( error, QStringLiteral( "规则名称不能为空。" ) );
    return false;
  }
  if ( bands.isEmpty() )
  {
    setError( error, QStringLiteral( "规则必须至少包含一个分包区间。" ) );
    return false;
  }
  int expectedMinimum = bands.constFirst().minZoom;
  for ( qsizetype index = 0; index < bands.size(); ++index )
  {
    QString bandError;
    if ( !bands.at( index ).isValid( &bandError ) )
    {
      setError( error, QStringLiteral( "第 %1 个分包区间无效：%2" ).arg( index + 1 ).arg( bandError ) );
      return false;
    }
    if ( bands.at( index ).minZoom != expectedMinimum )
    {
      setError( error, QStringLiteral( "分包区间必须按缩放级别连续排列，不能重叠或留空。" ) );
      return false;
    }
    expectedMinimum = bands.at( index ).maxZoom + 1;
  }

  if ( error )
    error->clear();
  return true;
}

const QgsMtpl::PartitionBand *QgsMtpl::PartitionRule::bandForZoom( int zoom ) const
{
  for ( const PartitionBand &band : bands )
  {
    if ( zoom >= band.minZoom && zoom <= band.maxZoom )
      return &band;
  }
  return nullptr;
}

bool QgsMtpl::PartitionRule::operator==( const PartitionRule &other ) const
{
  return id == other.id && name == other.name && builtIn == other.builtIn && bands == other.bands;
}

QString QgsMtpl::PartitionRuleStore::ruleOneId()
{
  return QStringLiteral( "org.qgis.mtpl.partition-rule.1" );
}

QString QgsMtpl::PartitionRuleStore::ruleTwoId()
{
  return QStringLiteral( "org.qgis.mtpl.partition-rule.2" );
}

QList<QgsMtpl::PartitionRule> QgsMtpl::PartitionRuleStore::builtInRules()
{
  PartitionRule first;
  first.id = ruleOneId();
  first.name = translatedRuleName( "分包规则一" );
  first.builtIn = true;
  first.bands = { { 0, 7, 0 }, { 8, 11, 3 }, { 12, 15, 7 }, { 16, 19, 11 } };

  PartitionRule second;
  second.id = ruleTwoId();
  second.name = translatedRuleName( "分包规则二" );
  second.builtIn = true;
  second.bands = { { 0, 9, 0 }, { 10, 14, 7 }, { 15, 19, 11 } };
  return { first, second };
}

bool QgsMtpl::PartitionRuleStore::validateCustomRules( const QList<PartitionRule> &rules, QString *error )
{
  QSet<QString> identifiers;
  QSet<QString> names;
  for ( const PartitionRule &rule : builtInRules() )
  {
    identifiers.insert( rule.id.toCaseFolded() );
    names.insert( rule.name.trimmed().toCaseFolded() );
  }

  for ( qsizetype index = 0; index < rules.size(); ++index )
  {
    const PartitionRule &rule = rules.at( index );
    if ( rule.builtIn )
    {
      setError( error, QStringLiteral( "自定义规则“%1”不能标记为内置规则。" ).arg( rule.name ) );
      return false;
    }
    QString ruleError;
    if ( !rule.isValid( &ruleError ) )
    {
      setError( error, QStringLiteral( "第 %1 个自定义规则无效：%2" ).arg( index + 1 ).arg( ruleError ) );
      return false;
    }
    const QString foldedId = rule.id.toCaseFolded();
    if ( identifiers.contains( foldedId ) )
    {
      setError( error, QStringLiteral( "规则 ID“%1”重复或与内置规则冲突。" ).arg( rule.id ) );
      return false;
    }
    identifiers.insert( foldedId );
    const QString foldedName = rule.name.trimmed().toCaseFolded();
    if ( names.contains( foldedName ) )
    {
      setError( error, QStringLiteral( "规则名称“%1”重复或与内置规则冲突。" ).arg( rule.name ) );
      return false;
    }
    names.insert( foldedName );
  }
  if ( error )
    error->clear();
  return true;
}

QList<QgsMtpl::PartitionRule> QgsMtpl::PartitionRuleStore::combineWithBuiltIns(
  const QList<PartitionRule> &customRules, QString *error )
{
  if ( !validateCustomRules( customRules, error ) )
    return builtInRules();

  QList<PartitionRule> result = builtInRules();
  result.append( customRules );
  if ( error )
    error->clear();
  return result;
}

bool QgsMtpl::PartitionRuleStore::findRule( const QList<PartitionRule> &rules,
                                            const QString &id,
                                            PartitionRule &rule )
{
  for ( const PartitionRule &candidate : rules )
  {
    if ( candidate.id == id )
    {
      rule = candidate;
      return true;
    }
  }
  return false;
}

QByteArray QgsMtpl::PartitionRuleStore::toJson( const QList<PartitionRule> &customRules,
                                                QString *error )
{
  if ( !validateCustomRules( customRules, error ) )
    return QByteArray();

  QJsonArray rules;
  for ( const PartitionRule &rule : customRules )
    rules.append( ruleToJson( rule ) );

  QJsonObject root;
  root.insert( QStringLiteral( "version" ), sJsonVersion );
  root.insert( QStringLiteral( "rules" ), rules );
  if ( error )
    error->clear();
  return QJsonDocument( root ).toJson( QJsonDocument::Compact );
}

bool QgsMtpl::PartitionRuleStore::fromJson( const QByteArray &json,
                                            QList<PartitionRule> &customRules,
                                            QString *error )
{
  customRules.clear();
  if ( json.trimmed().isEmpty() )
  {
    if ( error )
      error->clear();
    return true;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson( json, &parseError );
  if ( parseError.error != QJsonParseError::NoError || !document.isObject() )
  {
    setError( error, QStringLiteral( "无法解析分包规则 JSON：%1" ).arg( parseError.errorString() ) );
    return false;
  }
  const QJsonObject root = document.object();
  int version = -1;
  if ( !jsonInteger( root, QStringLiteral( "version" ), version ) || version != sJsonVersion )
  {
    setError( error, QStringLiteral( "不支持此分包规则 JSON 版本。" ) );
    return false;
  }
  if ( !root.value( QStringLiteral( "rules" ) ).isArray() )
  {
    setError( error, QStringLiteral( "分包规则 JSON 缺少 rules 数组。" ) );
    return false;
  }

  QList<PartitionRule> parsed;
  const QJsonArray rules = root.value( QStringLiteral( "rules" ) ).toArray();
  for ( qsizetype index = 0; index < rules.size(); ++index )
  {
    PartitionRule rule;
    QString ruleError;
    if ( !ruleFromJson( rules.at( index ), rule, &ruleError ) )
    {
      setError( error, QStringLiteral( "第 %1 个规则无法读取：%2" ).arg( index + 1 ).arg( ruleError ) );
      return false;
    }
    parsed.append( rule );
  }
  if ( !validateCustomRules( parsed, error ) )
    return false;

  customRules = parsed;
  if ( error )
    error->clear();
  return true;
}

QString QgsMtpl::PartitionRuleStore::settingsKey()
{
  return QStringLiteral( "mtpl/partitionRules/v1" );
}

QList<QgsMtpl::PartitionRule> QgsMtpl::PartitionRuleStore::loadCustomRules( QgsSettings &settings,
                                                                           QString *error )
{
  const QByteArray json = settings.value( settingsKey(), QByteArray(), QgsSettings::Section::Plugins ).toByteArray();
  QList<PartitionRule> rules;
  if ( !fromJson( json, rules, error ) )
    return {};
  return rules;
}

bool QgsMtpl::PartitionRuleStore::saveCustomRules( QgsSettings &settings,
                                                   const QList<PartitionRule> &customRules,
                                                   QString *error )
{
  const QByteArray json = toJson( customRules, error );
  if ( json.isEmpty() && !customRules.isEmpty() )
    return false;
  const QByteArray previousJson = settings.value( settingsKey(), QByteArray(), QgsSettings::Section::Plugins ).toByteArray();
  if ( previousJson != json )
    settings.setValue( settingsKey(), json, QgsSettings::Section::Plugins );
  settings.sync();
  if ( settings.value( settingsKey(), QByteArray(), QgsSettings::Section::Plugins ).toByteArray() != json )
  {
    setError( error, QStringLiteral( "分包规则写入 QGIS 用户设置后无法读回。" ) );
    return false;
  }

  // QgsSettings intentionally hides the underlying QSettings status. Reopen
  // the target store to verify both the backend status and the persisted bytes
  // instead of treating an in-memory cache update as a successful save.
  const QString settingsFileName = settings.fileName();
  const bool iniStore = QFileInfo( settingsFileName ).suffix().compare( QLatin1String( "ini" ), Qt::CaseInsensitive ) == 0 ||
                        QFileInfo::exists( settingsFileName );
  QSettings verifier( settingsFileName, iniStore ? QSettings::IniFormat : QSettings::NativeFormat );
  verifier.sync();
  const QString rawSettingsKey = QStringLiteral( "plugins/%1" ).arg( settingsKey() );
  if ( verifier.status() != QSettings::NoError || verifier.value( rawSettingsKey ).toByteArray() != json )
  {
    setError( error, QStringLiteral( "分包规则未能持久化到 QGIS 用户设置。" ) );
    return false;
  }
  if ( error )
    error->clear();
  return true;
}

QList<QgsMtpl::PartitionRule> QgsMtpl::PartitionRuleStore::loadCustomRules( QString *error )
{
  QgsSettings settings;
  return loadCustomRules( settings, error );
}

bool QgsMtpl::PartitionRuleStore::saveCustomRules( const QList<PartitionRule> &customRules,
                                                   QString *error )
{
  QgsSettings settings;
  return saveCustomRules( settings, customRules, error );
}

QList<QgsMtpl::PartitionRule> QgsMtpl::PartitionRuleStore::loadAllRules( QString *error )
{
  QString loadError;
  const QList<PartitionRule> customRules = loadCustomRules( &loadError );
  if ( !loadError.isEmpty() )
  {
    setError( error, loadError );
    return builtInRules();
  }
  return combineWithBuiltIns( customRules, error );
}
