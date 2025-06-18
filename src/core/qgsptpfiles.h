/***************************************************************************
  qgsptpfiles.h
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

#ifndef QGSPTPFILES_H
#define QGSPTPFILES_H


#include "qgis_core.h"

#define SIP_NO_FILE

/*------------PTP include-----------------*/

extern "C"
{
#include <ptp/common.h>
#include <ptp/typedef.h>
#include <ptp/mtp_packet_reader.h>
}

/*----------------------------------------*/

#include <qbytearray.h>
#include <qstring.h>

class QImage;

using namespace std;


/*
* PTP瓦片包读取类，提供PTP瓦片包的读取功能。
*/
class CORE_EXPORT QgsPTPFiles
{
  public:

     /*@brief 构造函数
    *
    * @param filename:				    ptp文件全路径
    * @param private_key:				  私有key
    * @param dev_key:					    设备key
    *
    */
    QgsPTPFiles(const QString& filename,
      const QString& private_key,
      const QString& dev_key);

    /*@brief 打开ptp瓦片包
    *
    * @return 打开成功返回true，失败返回false
    */
    bool open();

    /*@brief 获取ptp包元数据
    *
    * @param iDataLength：元数据长度
    * @param iStatus：操作状态
    * 
    * @return
    * 		获取成功，返回数据
    * 		获取失败，返回NULL，查看 status
    */
    unsigned char* metadataValue(uint32* iDataLength, uint32* iStatus);

    /*@brief 获取单张瓦片
    *
    * @param z    			  瓦片级别
    * @param x 			      瓦片x序号
    * @param y 			      瓦片y序号
    *
    * @return
    * 		获取成功，返回数据
    * 		获取失败，返回空字节串
    */
    QByteArray tileData( int z, int x, int y );

    /* @brief 获取单张瓦片，并以QImage对象返回
    *
    *@param z    			  瓦片级别
    * @param x 			      瓦片x序号
    * @param y 			      瓦片y序号
    *
    * @return
    *       获取成功，返回QImage对象
    *       获取失败，返回空QImage对象
    */
    QImage tileDataAsImage( int z, int x, int y );


    /*@brief 关闭ptp瓦片包
    */
    void close();

  private:

    // ptp文件全路径
    QString m_qstrFilename;

    // ptp读写私有key
    QString m_qstrPrivateKey;

    // ptp读写设备key
    QString m_qstrDevKey;

    // ptp瓦片包读取句柄
    HANDLE64_PTP m_ptpReader;
};


#endif // QGSPTPFILES_H
