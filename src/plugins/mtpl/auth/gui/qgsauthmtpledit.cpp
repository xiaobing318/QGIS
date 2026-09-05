/***************************************************************************
  qgsauthmtpledit.cpp
  -------------------
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

#include "qgsauthmtpledit.h"

#include "qgspasswordlineedit.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "moc_qgsauthmtpledit.cpp"

namespace
{
  bool validPrivateKey( const QByteArray &value )
  {
    const QByteArray decoded = QByteArray::fromBase64( value );
    return decoded.size() >= 16 && decoded.size() <= 4096 && decoded.toBase64() == value;
  }

  bool validDeviceKey( const QByteArray &value )
  {
    static const QRegularExpression sLowerHex( QStringLiteral( "^[0-9a-f]+$" ) );
    return value.size() >= 2 && value.size() <= 512 && value.size() % 2 == 0 &&
           sLowerHex.match( QString::fromLatin1( value ) ).hasMatch();
  }
}

QgsAuthMtplEdit::QgsAuthMtplEdit( QWidget *parent )
  : QgsAuthMethodEdit( parent )
{
  auto *layout = new QVBoxLayout( this );
  layout->setContentsMargins( 0, 0, 0, 0 );

  auto *description = new QLabel(
    tr( "此配置由 MTPL 插件用于安全保存一组数据包密钥。" ), this );
  description->setWordWrap( true );
  layout->addWidget( description );

  auto *form = new QFormLayout();
  mPrivateKeyEdit = new QgsPasswordLineEdit( this );
  mPrivateKeyEdit->setEchoMode( QLineEdit::Password );
  mPrivateKeyEdit->setPlaceholderText( tr( "规范的 Base64 私钥" ) );
  form->addRow( tr( "私钥" ), mPrivateKeyEdit );
  mDeviceKeyEdit = new QgsPasswordLineEdit( this );
  mDeviceKeyEdit->setEchoMode( QLineEdit::Password );
  mDeviceKeyEdit->setPlaceholderText( tr( "小写十六进制设备密钥" ) );
  form->addRow( tr( "设备密钥" ), mDeviceKeyEdit );
  layout->addLayout( form );

  mValidationLabel = new QLabel( this );
  mValidationLabel->setWordWrap( true );
  layout->addWidget( mValidationLabel );

  connect( mPrivateKeyEdit, &QLineEdit::textChanged, this, &QgsAuthMtplEdit::configChanged );
  connect( mDeviceKeyEdit, &QLineEdit::textChanged, this, &QgsAuthMtplEdit::configChanged );
  validateConfig();
}

bool QgsAuthMtplEdit::validateConfig()
{
  const QByteArray privateKey = mPrivateKeyEdit->text().trimmed().toLatin1();
  const QByteArray deviceKey = mDeviceKeyEdit->text().trimmed().toLatin1();
  const bool currentValid = validPrivateKey( privateKey ) && validDeviceKey( deviceKey );
  mValidationLabel->setText( currentValid
                              ? tr( "密钥格式有效。" )
                              : tr( "请输入规范的 Base64 私钥和偶数位小写十六进制设备密钥。" ) );
  if ( currentValid != mValid )
  {
    mValid = currentValid;
    emit validityChanged( mValid );
  }
  return mValid;
}

QgsStringMap QgsAuthMtplEdit::configMap() const
{
  return {
    { QStringLiteral( "privateKeyBase64" ), mPrivateKeyEdit->text().trimmed() },
    { QStringLiteral( "deviceKeyHex" ), mDeviceKeyEdit->text().trimmed() }
  };
}

void QgsAuthMtplEdit::loadConfig( const QgsStringMap &configMap )
{
  mLoadedConfig = configMap;
  mPrivateKeyEdit->setText( configMap.value( QStringLiteral( "privateKeyBase64" ) ) );
  mDeviceKeyEdit->setText( configMap.value( QStringLiteral( "deviceKeyHex" ) ) );
  validateConfig();
}

void QgsAuthMtplEdit::resetConfig()
{
  loadConfig( mLoadedConfig );
}

void QgsAuthMtplEdit::clearConfig()
{
  mPrivateKeyEdit->clear();
  mDeviceKeyEdit->clear();
}

void QgsAuthMtplEdit::configChanged()
{
  validateConfig();
}
