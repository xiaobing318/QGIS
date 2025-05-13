#pragma region "包含头文件（减少重复）"
/***********************geo_algorithm_h**************************/
#include <vector/cse_vector_feature_matching.h>
/***********************geo_algorithm_h**************************/

/**********************STL***************************/
#include <cstdlib>
/**********************STL***************************/

#pragma endregion

#pragma region "公开API接口"

#pragma region "构造、析构函数"
vector_feature_matching::vector_feature_matching(
  OGRFeature* matched_feature,
  OGRFeature* match_feature,
  vector_feature_matching_args_t& vector_feature_matching_args)
{
  this->m_matched_feature = matched_feature;
  this->m_match_feature = match_feature;
  this->m_vector_feature_matching_args = vector_feature_matching_args;
}

vector_feature_matching::~vector_feature_matching()
{

}
#pragma endregion

#pragma region "get/set成员函数"
OGRFeature* vector_feature_matching::get_m_matched_feature()
{
  return this->m_matched_feature;
}
OGRFeature* vector_feature_matching::get_m_match_feature()
{
  return this->m_match_feature;
}
void vector_feature_matching::set_m_matched_feature(OGRFeature* feature)
{
  if (feature)
  {
    this->m_matched_feature = feature;
  }
}
void vector_feature_matching::set_m_match_feature(OGRFeature* feature)
{
  if (feature)
  {
    this->m_match_feature;
  }
}

OGRwkbGeometryType vector_feature_matching::GetOGRFeatureType(const OGRFeature* feature)
{
  if (feature == NULL)
  {
    // 如果输入的特征是空指针，返回未知类型
    return wkbUnknown;
  }

  // 获取特征的几何对象
  const OGRGeometry* geom = feature->GetGeometryRef();
  if (geom == NULL)
  {
    // 如果几何对象为空，返回未知类型
    return wkbUnknown;
  }

  // 返回几何类型
  return geom->getGeometryType();
}

#pragma endregion

#pragma region "不同类型的矢量要素匹配"
/*
* 函数作用：判断wkbPoint类型与wkbPoint类型要素在空间上是否匹配
* 输入参数：
*   - wkbPointMatch_args    额外参数
*   - bFlag                 是否匹配标志
* 输出参数：
*   - error_t               错误管理
*/
error_t vector_feature_matching::wkbPointMatch(
  const wkbPointMatch_args_t& wkbPointMatch_args,
  bool& bFlag)
{
#pragma region "1、参数有效性检查"
  match_alg_type_t match_alg_type = wkbPointMatch_args.m_match_alg_type;
  double dthreshold = std::stod(wkbPointMatch_args.m_threshold);
  //  矢量要素1检查
  if (!(this->m_matched_feature))
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  矢量要素2检查
  if (!(this->m_match_feature))
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  获取得到本底要素的几何信息
  OGRGeometry* matched_feature_geometry = this->m_matched_feature->GetGeometryRef();
  if (!matched_feature_geometry)
  {
    return error_t::OGRFEATURE_GEOMETRY_POINTER_IS_NULL;
  }
  //  获取得到匹配要素的几何信息
  OGRGeometry* match_feature_geometry = this->m_match_feature->GetGeometryRef();
  if (!match_feature_geometry)
  {
    return error_t::OGRFEATURE_GEOMETRY_POINTER_IS_NULL;
  }
  //  获取得到本底要素、匹配要素的几何类型
  OGRwkbGeometryType matched_feature_geomType = matched_feature_geometry->getGeometryType();
  OGRwkbGeometryType match_feature_geomType = match_feature_geometry->getGeometryType();

  if ((wkbFlatten(matched_feature_geomType) != OGRwkbGeometryType::wkbPoint) ||
    (wkbFlatten(match_feature_geomType) != OGRwkbGeometryType::wkbPoint))
  {
    return error_t::OGRFEATURE_GEOMETRY_TYPE_IS_NOT_MATCHED;
  }

  //  空间匹配算法类型检查
  if (match_alg_type == match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
  {
    return error_t::OGRFEATURE_MATCHING_ALG_IS_NOT_VALID;
  }
  //  阈值参数检查
  if (!(0.0 < dthreshold))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }
#pragma endregion

#pragma region "2、根据不同的空间匹配算法调用不同的API接口"
  //  从矢量要素得到的要素几何信息转化为具体的几何类型
  OGRPoint* wkbPoint1 = dynamic_cast<OGRPoint*>(matched_feature_geometry);
  OGRPoint* wkbPoint2 = dynamic_cast<OGRPoint*>(match_feature_geometry);

  if (!wkbPoint1 || !wkbPoint2)
  {
    return error_t::OGRFEATURE_GEOMETRY_TYPE_DYNAMIC_CAST_FAILED;
  }

  switch (match_alg_type)
  {
  case match_alg_type_t::SINGLE_BUFFER_MATCHING_ALG:
  {
    error_t result_flag = wkbPoint_single_buff_match_alg(wkbPoint1, wkbPoint2, dthreshold, bFlag);
    if (result_flag != error_t::NO_ERRORS)
    {
      return result_flag;
    }
    break;
  }
  case match_alg_type_t::DOUBLE_BUFFER_MATCHING_ALG:
  {
    //  这里将两个阈值设置为一样（目前的观点：没有理由支持要将两个阈值设为不一样，因此将两个设置为一样）
    error_t result_flag = wkbPoint_double_buff_match_alg(wkbPoint1, wkbPoint2, dthreshold, dthreshold, bFlag);
    if (result_flag != error_t::NO_ERRORS)
    {
      return result_flag;
    }
    break;
  }
  default:
  {
    break;
  }
  } // switch end
#pragma endregion
  return error_t::NO_ERRORS;
}
/*
* 函数作用：判断wkbLineString类型与wkbLineString类型要素在空间上是否匹配
* 输入参数：
*   - wkbLineStringMatch_args                   额外参数列表
*   - bFlag                                     是否匹配标志
* 输出参数：
*   - error_t                                   错误管理
*/
error_t vector_feature_matching::wkbLineStringMatch(
  const wkbLineStringMatch_args_t& wkbLineStringMatch_args,
  bool& bFlag)
{
#pragma region "1、参数有效性检查"
  match_alg_type_t match_alg_type = wkbLineStringMatch_args.m_match_alg_type;
  double dthreshold = std::stod(wkbLineStringMatch_args.m_threshold);
  //  矢量要素1检查
  if (!(this->m_matched_feature))
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  矢量要素2检查
  if (!(this->m_match_feature))
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  获取得到本底要素的几何信息
  OGRGeometry* matched_feature_geometry = this->m_matched_feature->GetGeometryRef();
  if (!matched_feature_geometry)
  {
    return error_t::OGRFEATURE_GEOMETRY_POINTER_IS_NULL;
  }
  //  获取得到匹配要素的几何信息
  OGRGeometry* match_feature_geometry = this->m_match_feature->GetGeometryRef();
  if (!match_feature_geometry)
  {
    return error_t::OGRFEATURE_GEOMETRY_POINTER_IS_NULL;
  }
  //  获取得到本底要素、匹配要素的几何类型
  OGRwkbGeometryType matched_feature_geomType = matched_feature_geometry->getGeometryType();
  OGRwkbGeometryType match_feature_geomType = match_feature_geometry->getGeometryType();

  if ((wkbFlatten(matched_feature_geomType) != OGRwkbGeometryType::wkbLineString) ||
    (wkbFlatten(match_feature_geomType) != OGRwkbGeometryType::wkbLineString))
  {
    return error_t::OGRFEATURE_GEOMETRY_TYPE_IS_NOT_MATCHED;
  }

  //  空间匹配算法类型检查
  if (match_alg_type == match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
  {
    return error_t::OGRFEATURE_MATCHING_ALG_IS_NOT_VALID;
  }
  //  阈值参数检查
  if (!(0.0 < dthreshold))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }
#pragma endregion

#pragma region "2、根据不同的空间匹配算法调用不同的API接口"
  //  从矢量要素得到的要素几何信息转化为具体的几何类型
  OGRLineString* wkbLineString1 = dynamic_cast<OGRLineString*>(matched_feature_geometry);
  OGRLineString* wkbLineString2 = dynamic_cast<OGRLineString*>(match_feature_geometry);

  if (!wkbLineString1 || !wkbLineString2)
  {
    return error_t::OGRFEATURE_GEOMETRY_TYPE_DYNAMIC_CAST_FAILED;
  }

  switch (match_alg_type)
  {
  case match_alg_type_t::SINGLE_BUFFER_MATCHING_ALG:
  {
    error_t result_flag = wkbLineString_single_buff_match_alg(wkbLineString1, wkbLineString2, dthreshold, bFlag);
    if (result_flag != error_t::NO_ERRORS)
    {
      return result_flag;
    }
    break;
  }
  case match_alg_type_t::DOUBLE_BUFFER_MATCHING_ALG:
  {
    //  这里将两个阈值设置为一样（目前的观点：没有理由支持要将两个阈值设为不一样，因此将两个设置为一样）
    error_t result_flag = wkbLineString_double_buff_match_alg(wkbLineString1, wkbLineString2, dthreshold, dthreshold, bFlag);
    if (result_flag != error_t::NO_ERRORS)
    {
      return result_flag;
    }
    break;
  }
  default:
  {
    break;
  }
  } // switch end
#pragma endregion
  return error_t::NO_ERRORS;
}
/*
* 函数作用：判断wkbPolygon类型与wkbPolygon类型要素在空间上是否匹配
* 输入参数：
*   - wkbPolygonMatch_args                额外参数列表
*   - bFlag                               是否匹配标志
* 输出参数：
*   - error_t                             错误管理
*/
error_t vector_feature_matching::wkbPolygonMatch(
  const wkbPolygonMatch_args_t& wkbPolygonMatch_args,
  bool& bFlag)
{
#pragma region "1、参数有效性检查"
  match_alg_type_t match_alg_type = wkbPolygonMatch_args.m_match_alg_type;
  double dthreshold = std::stod(wkbPolygonMatch_args.m_threshold);
  //  矢量要素1检查
  if (!(this->m_matched_feature))
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  矢量要素2检查
  if (!(this->m_match_feature))
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  获取得到本底要素的几何信息
  OGRGeometry* matched_feature_geometry = this->m_matched_feature->GetGeometryRef();
  if (!matched_feature_geometry)
  {
    return error_t::OGRFEATURE_GEOMETRY_POINTER_IS_NULL;
  }
  //  获取得到匹配要素的几何信息
  OGRGeometry* match_feature_geometry = this->m_match_feature->GetGeometryRef();
  if (!match_feature_geometry)
  {
    return error_t::OGRFEATURE_GEOMETRY_POINTER_IS_NULL;
  }
  //  获取得到本底要素、匹配要素的几何类型
  OGRwkbGeometryType matched_feature_geomType = matched_feature_geometry->getGeometryType();
  OGRwkbGeometryType match_feature_geomType = match_feature_geometry->getGeometryType();

  if ((wkbFlatten(matched_feature_geomType) != OGRwkbGeometryType::wkbPolygon) ||
    (wkbFlatten(match_feature_geomType) != OGRwkbGeometryType::wkbPolygon))
  {
    return error_t::OGRFEATURE_GEOMETRY_TYPE_IS_NOT_MATCHED;
  }

  //  空间匹配算法类型检查
  if (match_alg_type == match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
  {
    return error_t::OGRFEATURE_MATCHING_ALG_IS_NOT_VALID;
  }
  //  阈值参数检查
  if (!(0.0 < dthreshold))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }
#pragma endregion

#pragma region "2、根据不同的空间匹配算法调用不同的API接口"
  //  从矢量要素得到的要素几何信息转化为具体的几何类型
  OGRPolygon* wkbPolygon1 = dynamic_cast<OGRPolygon*>(matched_feature_geometry);
  OGRPolygon* wkbPolygon2 = dynamic_cast<OGRPolygon*>(match_feature_geometry);

  if (!wkbPolygon1 || !wkbPolygon2)
  {
    return error_t::OGRFEATURE_GEOMETRY_TYPE_DYNAMIC_CAST_FAILED;
  }

  switch (match_alg_type)
  {
  case match_alg_type_t::SINGLE_BUFFER_MATCHING_ALG:
  {
    error_t result_flag = wkbPolygon_single_buff_match_alg(wkbPolygon1, wkbPolygon2, dthreshold, bFlag);
    if (result_flag != error_t::NO_ERRORS)
    {
      return result_flag;
    }
    break;
  }
  case match_alg_type_t::DOUBLE_BUFFER_MATCHING_ALG:
  {
    //  这里将两个阈值设置为一样（目前的观点：没有理由支持要将两个阈值设为不一样，因此将两个设置为一样）
    error_t result_flag = wkbPolygon_double_buff_match_alg(wkbPolygon1, wkbPolygon2, dthreshold, dthreshold, bFlag);
    if (result_flag != error_t::NO_ERRORS)
    {
      return result_flag;
    }
    break;
  }
  default:
  {
    break;
  }
  } // switch end
#pragma endregion
  return error_t::NO_ERRORS;
}

/*
* 函数作用：判断wkbMultiPoint类型与wkbMultiPoint类型要素在空间上是否匹配
* 输入参数：
*   - wkbMultiPointMatch_args                   额外参数列表
*   - bFlag                                     是否匹配标志
* 输出参数：
*   - error_t                                   错误管理
*/
error_t vector_feature_matching::wkbMultiPointMatch(
  const wkbMultiPointMatch_args_t& wkbMultiPointMatch_args,
  bool& bFlag)
{
  return error_t::NO_ERRORS;
}
/*
* 函数作用：判断wkbMultiLineString类型与wkbMultiLineString类型要素在空间上是否匹配
* 输入参数：
*   - wkbMultiLineStringMatch_args                        额外参数列表
*   - bFlag                                               是否匹配标志
* 输出参数：
*   - error_t                                             错误管理
*/
error_t vector_feature_matching::wkbMultiLineStringMatch(
  const wkbMultiLineStringMatch_args_t& wkbMultiLineStringMatch_args,
  bool& bFlag)
{
  return error_t::NO_ERRORS;
}
/*
* 函数作用：判断wkbMultiPolygon类型与wkbMultiPolygon类型要素在空间上是否匹配
* 输入参数：
*   - wkbMultiPolygonMatch_args                     额外参数列表
*   - bFlag                                         是否匹配标志
* 输出参数：
*   - error_t                                       错误管理
*/
error_t vector_feature_matching::wkbMultiPolygonMatch(
  const wkbMultiPolygonMatch_args_t& wkbMultiPolygonMatch_args,
  bool& bFlag)
{
  return error_t::NO_ERRORS;
}
/*
* 函数作用：判断wkbGeometryCollection类型与wkbGeometryCollection类型要素在空间上是否匹配
* 输入参数：
*   - wkbGeometryCollectionMatch_args                           额外参数列表
*   - bFlag                                                     是否匹配标志
* 输出参数：
*   - error_t                                                   错误管理
*/
error_t vector_feature_matching::wkbGeometryCollectionMatch(
  const wkbGeometryCollectionMatch_args_t& wkbGeometryCollectionMatch_args,
  bool& bFlag)
{
  return error_t::NO_ERRORS;
}
/*
* 函数作用：判断wkbLinearRing类型与wkbLinearRing类型要素在空间上是否匹配
* 输入参数：
*   - wkbLinearRingMatch_args                   额外参数列表
*   - bFlag                                     是否匹配标志
* 输出参数：
*   - error_t                                   错误管理
*/
error_t vector_feature_matching::wkbLinearRingMatch(
  const wkbLinearRingMatch_args_t wkbLinearRingMatch_args,
  bool& bFlag)
{
  return error_t::NO_ERRORS;
}
#pragma endregion

std::string vector_feature_matching::error_t2string(const error_t& error_type)
{
  switch (error_type)
  {
  case error_t::NO_ERRORS:
  {
    return "vector_feature_matching:没有错误";
    break;
  }
  case error_t::OGRFEATURE_POINTER_IS_NULL:
  {
    return "vector_feature_matching:要素指针为空";
    break;
  }
  case error_t::OGRLAYER_POINTER_IS_NULL:
  {
    return "vector_feature_matching:图层指针为空";
    break;
  }
  case error_t::GET_GDAL_SHP_DRIVER_UNSUCCESSFULL:
  {
    return "vector_feature_matching:GDAL中shp驱动获取失败";
    break;
  }
  case error_t::OGRFEATURE_MATCHING_ALG_IS_NOT_VALID:
  {
    return "vector_feature_matching:矢量要素匹配算法不是有效的（也就是没有指定矢量要素的匹配算法）";
    break;
  }
  case error_t::THRESHOLD_IS_LESS_THAN_ZERO:
  {
    return "vector_feature_matching:阈值小于0.0";
    break;
  }
  case error_t::CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL:
  {
    return "vector_feature_matching:创建具有缓冲区的新矢量要素几何信息失败";
    break;
  }
  default:
  {
    return "未知错误类型";
  }
  } // switch end
}

#pragma endregion

#pragma region "私有空间匹配算法API接口"

#pragma region "单缓冲区空间匹配算法"
error_t vector_feature_matching::wkbPoint_single_buff_match_alg(
  const OGRPoint* targetPoint,
  const OGRPoint* testPoint,
  const double& bufferRadius,
  bool& bFlag)
{
#pragma region "1、参数有效性检查"
  //  矢量要素1检查
  if (!targetPoint)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  矢量要素2检查
  if (!testPoint)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  缓冲区长度有效性检查
  if (!(0.0 < bufferRadius))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }

#pragma endregion

#pragma region "2、使用单缓冲区进行空间匹配"

  //  在之前的代码中（WX项目：没有将创建的几何要素使用完之后在内存中释放掉）
  OGRGeometry* buffer_geom = targetPoint->Buffer(bufferRadius);
  if (!buffer_geom)
  {
    //  创建具有缓冲区的新矢量要素几何信息失败
    bFlag = false;
    return error_t::CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL;
  }

  // 检查测试点是否在缓冲区内
  bFlag = buffer_geom->Contains(testPoint);

  // 释放创建的缓冲区几何对象的内存
  delete buffer_geom;
  return error_t::NO_ERRORS;
#pragma endregion
}

error_t vector_feature_matching::wkbLineString_single_buff_match_alg(
  const OGRLineString* targetLineString,
  const OGRLineString* testLineString,
  const double& bufferRadius,
  bool& bFlag)
{
#pragma region "1、参数有效性检查"
  //  矢量要素1检查
  if (!targetLineString)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  矢量要素2检查
  if (!testLineString)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  缓冲区长度有效性检查
  if (!(0.0 < bufferRadius))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }

#pragma endregion

#pragma region "2、使用单缓冲区进行空间匹配"

  //  在之前的代码中（WX项目：没有将创建的几何要素使用完之后在内存中释放掉）
  OGRGeometry* buffer_geom = targetLineString->Buffer(bufferRadius);
  if (!buffer_geom)
  {
    //  创建具有缓冲区的新矢量要素几何信息失败
    bFlag = false;
    return error_t::CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL;
  }

  // 检查测试点是否在缓冲区内
  bFlag = buffer_geom->Contains(testLineString);

  // 释放创建的缓冲区几何对象的内存
  delete buffer_geom;
  return error_t::NO_ERRORS;
#pragma endregion
}

error_t vector_feature_matching::wkbPolygon_single_buff_match_alg(
  const OGRPolygon* targetPolygon,
  const OGRPolygon* testPolygon,
  const double& bufferRadius,
  bool& bFlag)
{
#pragma region "1、参数有效性检查"
  //  矢量要素1检查
  if (!targetPolygon)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  矢量要素2检查
  if (!testPolygon)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  缓冲区长度有效性检查
  if (!(0.0 < bufferRadius))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }

#pragma endregion

#pragma region "2、使用单缓冲区进行空间匹配"

  //  在之前的代码中（WX项目：没有将创建的几何要素使用完之后在内存中释放掉）
  OGRGeometry* buffer_geom = targetPolygon->Buffer(bufferRadius);
  if (!buffer_geom)
  {
    //  创建具有缓冲区的新矢量要素几何信息失败
    bFlag = false;
    return error_t::CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL;
  }

  // 检查测试点是否在缓冲区内
  bFlag = buffer_geom->Contains(testPolygon);

  // 释放创建的缓冲区几何对象的内存
  delete buffer_geom;
  return error_t::NO_ERRORS;
#pragma endregion
}

error_t vector_feature_matching::wkbMultiPoint_single_buff_match_alg(
  const OGRMultiPoint* targetMultiPoint,
  const OGRMultiPoint* testMultiPoint,
  const double& bufferRadius,
  bool& bFlag)
{
  /*目前只是留下接口，具体的算法不进行实现*/
  bFlag = false;
  return error_t::NO_ERRORS;
}

error_t vector_feature_matching::wkbMultiLineString_single_buff_match_alg(
  const OGRMultiLineString* targetMultiLineString,
  const OGRMultiLineString* testMultiLineString,
  const double& bufferRadius,
  bool& bFlag)
{
  /*目前只是留下接口，具体的算法不进行实现*/
  bFlag = false;
  return error_t::NO_ERRORS;
}

error_t vector_feature_matching::wkbMultiPolygon_single_buff_match_alg(
  const OGRMultiPolygon* targetMultiPolygon,
  const OGRMultiPolygon* testMultiPolygon,
  const double& bufferRadius,
  bool& bFlag)
{
  /*目前只是留下接口，具体的算法不进行实现*/
  bFlag = false;
  return error_t::NO_ERRORS;
}

error_t vector_feature_matching::wkbGeometryCollection_single_buff_match_alg(
  const OGRGeometryCollection* targetGeometryCollection,
  const OGRGeometryCollection* testGeometryCollection,
  const double& bufferRadius,
  bool& bFlag)
{
  /*目前只是留下接口，具体的算法不进行实现*/
  bFlag = false;
  return error_t::NO_ERRORS;
}

error_t vector_feature_matching::wkbLinearRing_single_buff_match_alg(
  const OGRLinearRing* targetLinearRing,
  const OGRLinearRing* testLinearRing,
  const double& bufferRadius,
  bool& bFlag)
{
  /*目前只是留下接口，具体的算法不进行实现*/
  bFlag = false;
  return error_t::NO_ERRORS;
}

#pragma endregion

#pragma region "双缓冲区空间匹配算法"
error_t vector_feature_matching::wkbPoint_double_buff_match_alg(
  const OGRPoint* wkbPoint1,
  const OGRPoint* wkbPoint2,
  const double& wkbPoint1_buf,
  const double& wkbPoint2_buf,
  bool& bFlag)
{
#pragma region "1、参数有效性检查"
  //  矢量要素1检查
  if (!wkbPoint1)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  矢量要素2检查
  if (!wkbPoint2)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  缓冲区长度有效性检查
  if (!(0.0 < wkbPoint1_buf))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }
  if (!(0.0 < wkbPoint2_buf))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }
#pragma endregion

#pragma region "2、使用双缓冲区进行空间匹配"

  //  在之前的代码中（WX项目：没有将创建的几何要素使用完之后在内存中释放掉）
  OGRGeometry* new_geometry_matched_feature = wkbPoint1->Buffer(wkbPoint1_buf);
  OGRGeometry* new_geometry_match_feature = wkbPoint2->Buffer(wkbPoint2_buf);
  if (!new_geometry_matched_feature)
  {
    //  创建具有缓冲区的新矢量要素几何信息失败
    bFlag = false;
    return error_t::CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL;
  }
  if (!new_geometry_match_feature)
  {
    //  创建具有缓冲区的新矢量要素几何信息失败
    bFlag = false;
    return error_t::CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL;
  }

  //  目前对于wkbPoint类型的几何类型来说，只要两个wkbPoint要素的缓冲区之间是相交的，那么就认为两个要素在空间上是匹配的
  OGRBoolean is_intersect = new_geometry_matched_feature->Intersect(new_geometry_match_feature);
  if (is_intersect)
  {
    bFlag = true;
  }

  delete new_geometry_matched_feature;
  delete new_geometry_match_feature;

  return error_t::NO_ERRORS;
#pragma endregion
}

error_t vector_feature_matching::wkbLineString_double_buff_match_alg(
  const OGRLineString* wkbLineString1,
  const OGRLineString* wkbLineString2,
  const double& wkbLineString1_buf,
  const double& wkbLineString2_buf,
  bool& bFlag)
{
#pragma region "1、参数有效性检查"
  //  矢量要素1检查
  if (!wkbLineString1)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  矢量要素2检查
  if (!wkbLineString2)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  缓冲区长度有效性检查
  if (!(0.0 < wkbLineString1_buf))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }
  if (!(0.0 < wkbLineString2_buf))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }
#pragma endregion

#pragma region "2、使用双缓冲区进行空间匹配"

  //  在之前的代码中（WX项目：没有将创建的几何要素使用完之后在内存中释放掉）
  OGRGeometry* new_geometry_matched_feature = wkbLineString1->Buffer(wkbLineString1_buf);
  OGRGeometry* new_geometry_match_feature = wkbLineString2->Buffer(wkbLineString2_buf);
  if (!new_geometry_matched_feature)
  {
    //  创建具有缓冲区的新矢量要素几何信息失败
    bFlag = false;
    return error_t::CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL;
  }
  if (!new_geometry_match_feature)
  {
    //  创建具有缓冲区的新矢量要素几何信息失败
    bFlag = false;
    return error_t::CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL;
  }

  //  目前对于wkbPoint类型的几何类型来说，只要两个wkbPoint要素的缓冲区之间是相交的，那么就认为两个要素在空间上是匹配的
  OGRBoolean is_intersect = new_geometry_matched_feature->Intersect(new_geometry_match_feature);
  if (is_intersect)
  {
    bFlag = true;
  }

  delete new_geometry_matched_feature;
  delete new_geometry_match_feature;

  return error_t::NO_ERRORS;
#pragma endregion
}

error_t vector_feature_matching::wkbPolygon_double_buff_match_alg(
  const OGRPolygon* wkbPolygon1,
  const OGRPolygon* wkbPolygon2,
  const double& wkbPolygon1_buf,
  const double& wkbPolygon2_buf,
  bool& bFlag)
{
#pragma region "1、参数有效性检查"
  //  矢量要素1检查
  if (!wkbPolygon1)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  矢量要素2检查
  if (!wkbPolygon2)
  {
    return error_t::OGRFEATURE_POINTER_IS_NULL;
  }
  //  缓冲区长度有效性检查
  if (!(0.0 < wkbPolygon1_buf))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }
  if (!(0.0 < wkbPolygon2_buf))
  {
    return error_t::THRESHOLD_IS_LESS_THAN_ZERO;
  }
#pragma endregion

#pragma region "2、使用双缓冲区进行空间匹配"

  //  在之前的代码中（WX项目：没有将创建的几何要素使用完之后在内存中释放掉）
  OGRGeometry* new_geometry_matched_feature = wkbPolygon1->Buffer(wkbPolygon1_buf);
  OGRGeometry* new_geometry_match_feature = wkbPolygon2->Buffer(wkbPolygon2_buf);
  if (!new_geometry_matched_feature)
  {
    //  创建具有缓冲区的新矢量要素几何信息失败
    bFlag = false;
    return error_t::CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL;
  }
  if (!new_geometry_match_feature)
  {
    //  创建具有缓冲区的新矢量要素几何信息失败
    bFlag = false;
    return error_t::CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL;
  }

  //  目前对于wkbPoint类型的几何类型来说，只要两个wkbPoint要素的缓冲区之间是相交的，那么就认为两个要素在空间上是匹配的
  OGRBoolean is_intersect = new_geometry_matched_feature->Intersect(new_geometry_match_feature);
  if (is_intersect)
  {
    bFlag = true;
  }

  delete new_geometry_matched_feature;
  delete new_geometry_match_feature;

  return error_t::NO_ERRORS;
#pragma endregion
}

error_t vector_feature_matching::wkbMultiPoint_double_buff_match_alg(
  const OGRMultiPoint* wkbMultiPoint1,
  const OGRMultiPoint* wkbMultiPoint2,
  const double& wkbMultiPoint1_buf,
  const double& wkbMultiPoint2_buf,
  bool& bFlag)
{
  /*目前只是留下接口，具体的算法不进行实现*/
  bFlag = false;
  return error_t::NO_ERRORS;
}

error_t vector_feature_matching::wkbMultiLineString_double_buff_match_alg(
  const OGRMultiLineString* wkbMultiLineString1,
  const OGRMultiLineString* wkbMultiLineString2,
  const double& wkbMultiLineString1_buf,
  const double& wkbMultiLineString2_buf,
  bool& bFlag)
{
  /*目前只是留下接口，具体的算法不进行实现*/
  bFlag = false;
  return error_t::NO_ERRORS;
}

error_t vector_feature_matching::wkbMultiPolygon_double_buff_match_alg(
  const OGRMultiPolygon* wkbMultiPolygon1,
  const OGRMultiPolygon* wkbMultiPolygon2,
  const double& wkbMultiPolygon1_buf,
  const double& wkbMultiPolygon2_buf,
  bool& bFlag)
{
  /*目前只是留下接口，具体的算法不进行实现*/
  bFlag = false;
  return error_t::NO_ERRORS;
}

error_t vector_feature_matching::wkbGeometryCollection_double_buff_match_alg(
  const OGRGeometryCollection* wkbGeometryCollection1,
  const OGRGeometryCollection* wkbGeometryCollection2,
  const double& wkbGeometryCollection1_buf,
  const double& wkbGeometryCollection2_buf,
  bool& bFlag)
{
  /*目前只是留下接口，具体的算法不进行实现*/
  bFlag = false;
  return error_t::NO_ERRORS;
}

error_t vector_feature_matching::wkbLinearRing_double_buff_match_alg(
  const OGRLinearRing* wkbLinearRing1,
  const OGRLinearRing* wkbLinearRing2,
  const double& wkbLinearRing1_buf,
  const double& wkbLinearRing2_buf,
  bool& bFlag)
{
  /*目前只是留下接口，具体的算法不进行实现*/
  bFlag = false;
  return error_t::NO_ERRORS;
}

#pragma endregion

#pragma region "基于距离的空间匹配算法"

#pragma endregion

#pragma endregion




