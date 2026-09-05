/***************************************************************************
  qgsauthmtpledit.h
  -----------------
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

#ifndef QGSAUTHMTPLEDIT_H
#define QGSAUTHMTPLEDIT_H

#include "qgsauthmethodedit.h"

class QLabel;
class QgsPasswordLineEdit;

class QgsAuthMtplEdit final : public QgsAuthMethodEdit
{
    Q_OBJECT

  public:
    explicit QgsAuthMtplEdit( QWidget *parent = nullptr );

    bool validateConfig() override;
    QgsStringMap configMap() const override;

  public slots:
    void loadConfig( const QgsStringMap &configMap ) override;
    void resetConfig() override;
    void clearConfig() override;

  private:
    void configChanged();
    QgsStringMap mLoadedConfig;
    QgsPasswordLineEdit *mPrivateKeyEdit = nullptr;
    QgsPasswordLineEdit *mDeviceKeyEdit = nullptr;
    QLabel *mValidationLabel = nullptr;
    bool mValid = false;
};

#endif // QGSAUTHMTPLEDIT_H
