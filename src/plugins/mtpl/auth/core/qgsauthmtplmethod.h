/***************************************************************************
  qgsauthmtplmethod.h
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

#ifndef QGSAUTHMTPLMETHOD_H
#define QGSAUTHMTPLMETHOD_H

#include "qgsauthmethod.h"
#include "qgsauthmethodmetadata.h"

/**
 * Authentication method used solely as an encrypted container for the MTPL
 * plugin's one saved key pair. MTPL consumes the values directly, so this
 * method deliberately has no network or data-source expansion points.
 */
class QgsAuthMtplMethod final : public QgsAuthMethod
{
    Q_OBJECT

  public:
    static const QString AUTH_METHOD_KEY;
    static const QString AUTH_METHOD_DESCRIPTION;
    static const QString AUTH_METHOD_DISPLAY_DESCRIPTION;

    QgsAuthMtplMethod();

    QString key() const override;
    QString description() const override;
    QString displayDescription() const override;
    void clearCachedConfig( const QString &authcfg ) override;
    void updateMethodConfig( QgsAuthMethodConfig &config ) override;

#ifdef HAVE_GUI
    QWidget *editWidget( QWidget *parent ) const override;
#endif
};

class QgsAuthMtplMethodMetadata final : public QgsAuthMethodMetadata
{
  public:
    QgsAuthMtplMethodMetadata();
    QgsAuthMtplMethod *createAuthMethod() const override;
};

#endif // QGSAUTHMTPLMETHOD_H
