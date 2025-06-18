/***************************************************************************
  qgsptpfiles.cpp
  --------------------------------------
  Date                 : February 2023
  Copyright            : (C) 2023 by StarEarth
  Email                : pippo6376087@126.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#define _HAS_STD_BYTE 0


#include "qgsptpfiles.h"

#include <QFile>
#include <QImage>


QgsPTPFiles::QgsPTPFiles(const QString& filename,
  const QString& private_key,
  const QString& dev_key) : m_qstrFilename(filename),
  m_qstrPrivateKey(private_key),
  m_qstrDevKey(dev_key)  
{
 
}

bool QgsPTPFiles::open()
{
  // ptp瓦片包名
  QByteArray qFileName = m_qstrFilename.toLocal8Bit();
  string strFileName = string(qFileName);

  // 私有key
  QByteArray qPrivateKey = m_qstrPrivateKey.toLocal8Bit();
  string strPrivateKey = string(qPrivateKey);

  // 设备key
  QByteArray qDevKey = m_qstrDevKey.toLocal8Bit();
  string strDevKey = string(qDevKey);

  // 操作状态
  uint32 status = 0;

  m_ptpReader = mtp_open(
    strFileName.c_str(),
    strPrivateKey.c_str(),
    strlen(strPrivateKey.c_str()),
    strDevKey.c_str(),
    strlen(strDevKey.c_str()),
    &status);

  if (m_ptpReader == 0)
  {
    return false;
  }

  return true;
}

unsigned char* QgsPTPFiles::metadataValue(uint32* iDataLength, uint32* iStatus)
{
  byte* metadata = mtp_get_metadata(m_ptpReader, iDataLength, iStatus);

  if (metadata == 0)
  {
    return nullptr;
  }
  else
  {
    return metadata;
  }
}

QByteArray QgsPTPFiles::tileData(int z, int x, int y)
{
  uint32 data_len = 0;
  uint32 status = 0;
  byte *data = mtp_get_tile(
    m_ptpReader,
    z,
    x,
    y,
    &data_len,
    TRUE,
    &status);

  if (data == NULL)
  {
    return QByteArray();
  }

  return QByteArray(reinterpret_cast<const char*>(data), data_len);

}

void QgsPTPFiles::close()
{
  mtp_close(m_ptpReader);
}


QImage QgsPTPFiles::tileDataAsImage( int z, int x, int y )
{

  uint32 data_len = 0;
  uint32 status = 0;
  byte* data = mtp_get_tile(
    m_ptpReader,
    z,
    x,
    y,
    &data_len,
    TRUE,
    &status);

  if (data == NULL)
  {
    return QImage();
  }

  QImage tileImage;

  bool bResult = tileImage.loadFromData(data, data_len);
  if (!bResult)
  {
    internal_free((void*)data);
    return QImage();
  }


  internal_free((void*)data);
  return tileImage;
}
