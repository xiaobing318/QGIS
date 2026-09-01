/***************************************************************************
  qgsmtplstatus.h
  ----------------
  User-facing status text for the built-in MTPL plugin.
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

#ifndef QGSMTPLSTATUS_H
#define QGSMTPLSTATUS_H

#include <mtpl/status.h>

#include <QString>

namespace QgsMtpl
{

inline QString statusText( mtpl_status_t status )
{
  switch ( status )
  {
    case MTPL_STATUS_OK:
      return QStringLiteral( "操作成功" );
    case MTPL_STATUS_INVALID_ARGUMENT:
      return QStringLiteral( "参数无效" );
    case MTPL_STATUS_OUT_OF_MEMORY:
      return QStringLiteral( "内存不足" );
    case MTPL_STATUS_IO_ERROR:
      return QStringLiteral( "输入或输出错误" );
    case MTPL_STATUS_FILE_NOT_FOUND:
      return QStringLiteral( "文件不存在" );
    case MTPL_STATUS_FORMAT_ERROR:
      return QStringLiteral( "数据包格式错误" );
    case MTPL_STATUS_FORMAT_MISMATCH:
      return QStringLiteral( "数据包格式不匹配" );
    case MTPL_STATUS_UNSUPPORTED_VERSION:
      return QStringLiteral( "不支持的数据包版本" );
    case MTPL_STATUS_KEY_REQUIRED:
      return QStringLiteral( "需要密钥" );
    case MTPL_STATUS_CRYPTO_ERROR:
      return QStringLiteral( "加密或解密失败" );
    case MTPL_STATUS_COMPRESSION_ERROR:
      return QStringLiteral( "压缩失败" );
    case MTPL_STATUS_DECOMPRESSION_ERROR:
      return QStringLiteral( "解压失败" );
    case MTPL_STATUS_NOT_FOUND:
      return QStringLiteral( "未找到请求的内容" );
    case MTPL_STATUS_ALREADY_EXISTS:
      return QStringLiteral( "目标已存在" );
    case MTPL_STATUS_OUT_OF_RANGE:
      return QStringLiteral( "数值超出有效范围" );
    case MTPL_STATUS_CORRUPT_DATA:
      return QStringLiteral( "数据已损坏" );
    case MTPL_STATUS_PATH_ERROR:
      return QStringLiteral( "路径无效" );
    case MTPL_STATUS_PATH_TRAVERSAL:
      return QStringLiteral( "检测到不安全的路径" );
    case MTPL_STATUS_LIMIT_EXCEEDED:
      return QStringLiteral( "超出安全限制" );
    case MTPL_STATUS_UNSUPPORTED:
      return QStringLiteral( "不支持此操作" );
    case MTPL_STATUS_INTERNAL_ERROR:
      return QStringLiteral( "内部错误" );
    case MTPL_STATUS_CANCELED:
      return QStringLiteral( "操作已取消" );
  }
  return QStringLiteral( "未知错误（状态码：%1）" ).arg( static_cast<int>( status ) );
}

inline QString statusError( const QString &action, mtpl_status_t status )
{
  return QStringLiteral( "%1：%2" ).arg( action, statusText( status ) );
}

} // namespace QgsMtpl

#endif // QGSMTPLSTATUS_H
