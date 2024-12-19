#ifndef CSE_VECTOR_FEATURE_MATCHING_H
#define CSE_VECTOR_FEATURE_MATCHING_H

#pragma region "包含头文件（减少重复）"
/**********************STL***************************/
#include <vector>
#include <string>
/**********************STL***************************/


/**********************GDAL***************************/
//  这个头文件提供了对各种矢量数据格式（包括Shapefile）的基本操作接口，如访问数据源、读写图层等API接口
#include <ogrsf_frmts.h>
//  这个头文件提供了对单个矢量要素的操作，包括获取和设置几何形状和属性字段
#include <ogr_feature.h>
//  这个头文件定义了`OGRGeometry`类及其相关的子类，如`OGRPolygon`等，这些类提供了广泛的几何操作方法，包括但不限于相交、并集、差集、缓冲区等几何计算
#include "ogr_geometry.h"
/**********************GDAL***************************/


/***********************geo_algorithm_h**************************/
#include <commontype/se_commondef.h>
#include <commontype/se_config.h>
/***********************geo_algorithm_h**************************/
#pragma endregion

#pragma region "自定义结构体"

#pragma region "单个要素和单个要素空间匹配算法种类"
/*
* 杨小兵-2024-05-28
* 结构体解释：单个要素和单个要素在空间上的匹配算法枚举类型，用来说明有哪些匹配算法
*/
typedef enum match_alg_type
{
  //  未指定的匹配算法
  UNSPECIFIED_MATCHING_ALG = 0,
  //  双缓冲区空间匹配算法
  DOUBLE_BUFFER_MATCHING_ALG = 1,
  //  单缓冲区空间匹配算法
  SINGLE_BUFFER_MATCHING_ALG = 2,
}match_alg_type_t;
#pragma endregion

#pragma region "错误管理：错误种类"
/*
* 杨小兵-2024-05-28
* 结构体解释：在这个基础算法库（单要素同单要素之间的空间匹配）中将会使用到的错误管理
  1、
*/
typedef enum error
{
  //  没有错误
  NO_ERRORS = 0,
  //  要素指针为空
  OGRFEATURE_POINTER_IS_NULL = 1,
  //  图层指针为空
  OGRLAYER_POINTER_IS_NULL = 2,
  //  GDAL中shp驱动获取失败
  GET_GDAL_SHP_DRIVER_UNSUCCESSFULL = 3,
  //  矢量要素匹配算法不是有效的（也就是没有指定矢量要素的匹配算法）
  OGRFEATURE_MATCHING_ALG_IS_NOT_VALID = 4,
  //  阈值小于0.0
  THRESHOLD_IS_LESS_THAN_ZERO = 5,
  //  创建具有缓冲区的新矢量要素几何信息失败
  CREATE_NEW_GEOMETRY_WITH_BUFF_IS_UNSUCCESSFULL = 6,
  //  矢量要素的几何信息指针为空
  OGRFEATURE_GEOMETRY_POINTER_IS_NULL = 7,
  //  矢量要素几何类型不匹配
  OGRFEATURE_GEOMETRY_TYPE_IS_NOT_MATCHED = 8,
  //  矢量要素几何信息动态转化为具体的几何类型失败了
  OGRFEATURE_GEOMETRY_TYPE_DYNAMIC_CAST_FAILED = 9
}error_t;
#pragma endregion

#pragma region "input_arguments:输入参数结构体"
typedef struct vector_feature_matching_args
{
  //  第一个矢量要素的缓冲区距离
  std::string m_OGRFeature1_buffer;
  //  第二个矢量要素的缓冲区距离
  std::string m_OGRFeature2_buffer;
  //  矢量要素进行匹配的算法类型
  match_alg_type_t m_match_alg_type;

  //  构造函数将会默认：1、第一个矢量要素的缓冲区距离为0；2、第二个矢量要素的缓冲区距离为0；3、矢量要素进行匹配的算法类型未指定
  vector_feature_matching_args(
    std::string OGRFeature1_buffer = "0",
    std::string OGRFeature2_buffer = "0",
    match_alg_type_t match_alg_type = match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
    : m_OGRFeature1_buffer(OGRFeature1_buffer),
    m_OGRFeature2_buffer(OGRFeature2_buffer),
    m_match_alg_type(match_alg_type)
  {

  }


}vector_feature_matching_args_t;

typedef struct wkbPointMatch_args
{
  //  阈值参数
  std::string m_threshold;
  //  空间匹配算法
  match_alg_type_t m_match_alg_type;

  wkbPointMatch_args(
    std::string threshold = "",
    match_alg_type_t match_alg_type = match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
    :m_threshold(threshold),
    m_match_alg_type(match_alg_type)
  {

  }
}wkbPointMatch_args_t;

typedef struct wkbLineStringMatch_args
{
  //  阈值参数
  std::string m_threshold;
  //  空间匹配算法
  match_alg_type_t m_match_alg_type;
  wkbLineStringMatch_args(
    std::string threshold = "",
    match_alg_type_t match_alg_type = match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
    :m_threshold(threshold),
    m_match_alg_type(match_alg_type)
  {

  }
}wkbLineStringMatch_args_t;

typedef struct wkbPolygonMatch_args
{
  //  阈值参数
  std::string m_threshold;
  //  空间匹配算法
  match_alg_type_t m_match_alg_type;
  wkbPolygonMatch_args(
    std::string threshold = "",
    match_alg_type_t match_alg_type = match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
    :m_threshold(threshold),
    m_match_alg_type(match_alg_type)
  {

  }
}wkbPolygonMatch_args_t;

typedef struct wkbMultiPointMatch_args
{
  //  阈值参数
  std::string m_threshold;
  //  空间匹配算法
  match_alg_type_t m_match_alg_type;
  wkbMultiPointMatch_args(
    std::string threshold = "",
    match_alg_type_t match_alg_type = match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
    :m_threshold(threshold),
    m_match_alg_type(match_alg_type)
  {

  }
}wkbMultiPointMatch_args_t;

typedef struct wkbMultiLineStringMatch_args
{
  //  阈值参数
  std::string m_threshold;
  //  空间匹配算法
  match_alg_type_t m_match_alg_type;
  wkbMultiLineStringMatch_args(
    std::string threshold = "",
    match_alg_type_t match_alg_type = match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
    :m_threshold(threshold),
    m_match_alg_type(match_alg_type)
  {

  }
}wkbMultiLineStringMatch_args_t;

typedef struct wkbMultiPolygonMatch_args
{
  //  阈值参数
  std::string m_threshold;
  //  空间匹配算法
  match_alg_type_t m_match_alg_type;
  wkbMultiPolygonMatch_args(
    std::string threshold = "",
    match_alg_type_t match_alg_type = match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
    :m_threshold(threshold),
    m_match_alg_type(match_alg_type)
  {

  }
}wkbMultiPolygonMatch_args_t;

typedef struct wkbGeometryCollectionMatch_args
{
  //  阈值参数
  std::string m_threshold;
  //  空间匹配算法
  match_alg_type_t m_match_alg_type;
  wkbGeometryCollectionMatch_args(
    std::string threshold = "",
    match_alg_type_t match_alg_type = match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
    :m_threshold(threshold),
    m_match_alg_type(match_alg_type)
  {

  }
}wkbGeometryCollectionMatch_args_t;

typedef struct wkbLinearRingMatch_args
{
  //  阈值参数
  std::string m_threshold;
  //  空间匹配算法
  match_alg_type_t m_match_alg_type;
  wkbLinearRingMatch_args(
    std::string threshold = "",
    match_alg_type_t match_alg_type = match_alg_type_t::UNSPECIFIED_MATCHING_ALG)
    :m_threshold(threshold),
    m_match_alg_type(match_alg_type)
  {

  }
}wkbLinearRingMatch_args_t;

#pragma endregion

#pragma region "output_arguments:输出参数结构体"




#pragma endregion

#pragma endregion

#pragma region "矢量要素匹配类（作为一个基础类，将会被其他类引用）"
class SE_API vector_feature_matching
{
#pragma region "公开API接口"
public:
  /*************************************************构造、析构*****************************************************/
  /*
  * 函数作用：用来构建类的实例
  * 输入参数：
  *   - matched_feature                               本底要素（默认为nullptr）
  *   - match_feature                                 匹配要素（默认为nullptr）
  *   - vector_feature_matching_args                  额外参数（“0”，“0”，“未指定匹配算法”）
  *   
  */
  vector_feature_matching(
    OGRFeature* matched_feature = nullptr,
    OGRFeature* match_feature = nullptr,
    vector_feature_matching_args_t& vector_feature_matching_args = vector_feature_matching_args_t());
  /*
  * 函数作用：用来析构类的实例
  * 输入参数：
  *   - no args
  */
  ~vector_feature_matching();
  /*************************************************构造、析构*****************************************************/


  /*************************************************get/set*****************************************************/
  OGRFeature* get_m_matched_feature();
  OGRFeature* get_m_match_feature();
  void set_m_matched_feature(OGRFeature* feature);
  void set_m_match_feature(OGRFeature* feature);
  OGRwkbGeometryType GetOGRFeatureType(const OGRFeature* feature);
  /*************************************************get/set*****************************************************/




  /*************************************************不同类型的矢量要素匹配*****************************************************/
  /*
  * 函数作用：判断wkbPoint类型与wkbPoint类型要素在空间上是否匹配
  * 输入参数：
  *   - wkbPointMatch_args    额外参数
  *   - bFlag                 是否匹配标志
  * 输出参数：
  *   - error_t               错误管理
  */
  error_t wkbPointMatch(
    const wkbPointMatch_args_t& wkbPointMatch_args,
    bool& bFlag);
  /*
  * 函数作用：判断wkbLineString类型与wkbLineString类型要素在空间上是否匹配
  * 输入参数：
  *   - wkbLineStringMatch_args                   额外参数列表
  *   - bFlag                                     是否匹配标志
  * 输出参数：
  *   - error_t                                   错误管理
  */
  error_t wkbLineStringMatch(
    const wkbLineStringMatch_args_t& wkbLineStringMatch_args,
    bool& bFlag);
  /*
  * 函数作用：判断wkbPolygon类型与wkbPolygon类型要素在空间上是否匹配
  * 输入参数：
  *   - wkbPolygonMatch_args                额外参数列表
  *   - bFlag                               是否匹配标志
  * 输出参数：
  *   - error_t                             错误管理
  */
  error_t wkbPolygonMatch(
    const wkbPolygonMatch_args_t& wkbPolygonMatch_args,
    bool& bFlag);

  /*
  * 函数作用：判断wkbMultiPoint类型与wkbMultiPoint类型要素在空间上是否匹配
  * 输入参数：
  *   - wkbMultiPointMatch_args                   额外参数列表
  *   - bFlag                                     是否匹配标志
  * 输出参数：
  *   - error_t                                   错误管理
  */
  error_t wkbMultiPointMatch(
    const wkbMultiPointMatch_args_t& wkbMultiPointMatch_args,
    bool& bFlag);
  /*
  * 函数作用：判断wkbMultiLineString类型与wkbMultiLineString类型要素在空间上是否匹配
  * 输入参数：
  *   - wkbMultiLineStringMatch_args                        额外参数列表
  *   - bFlag                                               是否匹配标志
  * 输出参数：
  *   - error_t                                             错误管理
  */
  error_t wkbMultiLineStringMatch(
    const wkbMultiLineStringMatch_args_t& wkbMultiLineStringMatch_args,
    bool& bFlag);
  /*
  * 函数作用：判断wkbMultiPolygon类型与wkbMultiPolygon类型要素在空间上是否匹配
  * 输入参数：
  *   - wkbMultiPolygonMatch_args                     额外参数列表
  *   - bFlag                                         是否匹配标志
  * 输出参数：
  *   - error_t                                       错误管理
  */
  error_t wkbMultiPolygonMatch(
    const wkbMultiPolygonMatch_args_t& wkbMultiPolygonMatch_args,
    bool& bFlag);
  /*
  * 函数作用：判断wkbGeometryCollection类型与wkbGeometryCollection类型要素在空间上是否匹配
  * 输入参数：
  *   - wkbGeometryCollectionMatch_args                           额外参数列表
  *   - bFlag                                                     是否匹配标志
  * 输出参数：
  *   - error_t                                                   错误管理
  */
  error_t wkbGeometryCollectionMatch(
    const wkbGeometryCollectionMatch_args_t& wkbGeometryCollectionMatch_args,
    bool& bFlag);
  /*
  * 函数作用：判断wkbLinearRing类型与wkbLinearRing类型要素在空间上是否匹配
  * 输入参数：
  *   - wkbLinearRingMatch_args                   额外参数列表
  *   - bFlag                                     是否匹配标志
  * 输出参数：
  *   - error_t                                   错误管理
  */
  error_t wkbLinearRingMatch(
    const wkbLinearRingMatch_args_t wkbLinearRingMatch_args,
    bool& bFlag);
  /*************************************************不同类型的矢量要素匹配*****************************************************/


  /*************************************************error_t--->string*****************************************************/
  std::string error_t2string(const error_t& error_type);
  /*************************************************error_t--->string*****************************************************/


#pragma endregion
  
#pragma region "私有API接口"
private:

#pragma region "单缓冲区要素空间匹配算法"
/*
一、解释
  使用双缓冲区算法进行空间匹配（单个矢量要素和单个矢量要素之间进行空间匹配）
*/

  /*
  * 函数作用：使用单缓冲区算法判断wkbPoint类型与wkbPoint类型要素在空间上是否匹配
  * 输入参数：
  *   - targetPoint             本底数据单点要素
  *   - testPoint               匹配数据单点要素
  *   - bufferRadius            缓冲区长度
  *   - bFlag                   是否匹配标志
  * 输出参数：
  *   - error_t               错误管理
  */
  error_t wkbPoint_single_buff_match_alg(
    const OGRPoint* targetPoint,
    const OGRPoint* testPoint,
    const double& bufferRadius,
    bool& bFlag);
  /*
  * 函数作用：使用单缓冲区算法判断wkbLineString类型与wkbLineString类型要素在空间上是否匹配
  * 输入参数：
  *   - targetLineString             本底数据单点要素
  *   - testLineString               匹配数据单点要素
  *   - bufferRadius                 缓冲区长度
  *   - bFlag                        是否匹配标志
  * 输出参数：
  *   - error_t               错误管理
  */
  error_t wkbLineString_single_buff_match_alg(
    const OGRLineString* targetLineString,
    const OGRLineString* testLineString,
    const double& bufferRadius,
    bool& bFlag);
  /*
  * 函数作用：使用单缓冲区算法判断wkbPolygon类型与wkbPolygon类型要素在空间上是否匹配
  * 输入参数：
  *   - targetPolygon             本底数据单点要素
  *   - testPolygon               匹配数据单点要素
  *   - bufferRadius              缓冲区长度
  *   - bFlag                     是否匹配标志
  * 输出参数：
  *   - error_t                   错误管理
  */
  error_t wkbPolygon_single_buff_match_alg(
    const OGRPolygon* targetPolygon,
    const OGRPolygon* testPolygon,
    const double& bufferRadius,
    bool& bFlag);
  /*
  * 函数作用：使用单缓冲区算法判断wkbMultiPoint类型与wkbMultiPoint类型要素在空间上是否匹配
  * 输入参数：
  *   - targetMultiPoint             本底数据单点要素
  *   - testMultiPoint               匹配数据单点要素
  *   - bufferRadius                 缓冲区长度
  *   - bFlag                        是否匹配标志
  * 输出参数：
  *   - error_t                      错误管理
  */
  error_t wkbMultiPoint_single_buff_match_alg(
    const OGRMultiPoint* targetMultiPoint,
    const OGRMultiPoint* testMultiPoint,
    const double& bufferRadius,
    bool& bFlag);
  /*
  * 函数作用：使用单缓冲区算法判断wkbMultiLineString类型与wkbMultiLineString类型要素在空间上是否匹配
  * 输入参数：
  *   - targetMultiLineString             本底数据单点要素
  *   - testMultiLineString               匹配数据单点要素
  *   - bufferRadius                      缓冲区长度
  *   - bFlag                             是否匹配标志
  * 输出参数：
  *   - error_t                           错误管理
  */
  error_t wkbMultiLineString_single_buff_match_alg(
    const OGRMultiLineString* targetMultiLineString,
    const OGRMultiLineString* testMultiLineString,
    const double& bufferRadius,
    bool& bFlag);
  /*
  * 函数作用：使用单缓冲区算法判断wkbMultiPolygon类型与wkbMultiPolygon类型要素在空间上是否匹配
  * 输入参数：
  *   - targetMultiPolygon                本底数据单点要素
  *   - testMultiPolygon                  匹配数据单点要素
  *   - bufferRadius                      缓冲区长度
  *   - bFlag                             是否匹配标志
  * 输出参数：
  *   - error_t                           错误管理
  */
  error_t wkbMultiPolygon_single_buff_match_alg(
    const OGRMultiPolygon* targetMultiPolygon,
    const OGRMultiPolygon* testMultiPolygon,
    const double& bufferRadius,
    bool& bFlag);
  /*
  * 函数作用：使用单缓冲区算法判断wkbGeometryCollection类型与wkbGeometryCollection类型要素在空间上是否匹配
  * 输入参数：
  *   - targetGeometryCollection                本底数据单点要素
  *   - testGeometryCollection                  匹配数据单点要素
  *   - bufferRadius                            缓冲区长度
  *   - bFlag                                   是否匹配标志
  * 输出参数：
  *   - error_t                                 错误管理
  */
  error_t wkbGeometryCollection_single_buff_match_alg(
    const OGRGeometryCollection* targetGeometryCollection,
    const OGRGeometryCollection* testGeometryCollection,
    const double& bufferRadius,
    bool& bFlag);
  /*
  * 函数作用：使用单缓冲区算法判断wkbLinearRing类型与wkbLinearRing类型要素在空间上是否匹配
  * 输入参数：
  *   - targetLinearRing                本底数据单点要素
  *   - testLinearRing                  匹配数据单点要素
  *   - bufferRadius                    缓冲区长度
  *   - bFlag                           是否匹配标志
  * 输出参数：
  *   - error_t                         错误管理
  */
  error_t wkbLinearRing_single_buff_match_alg(
    const OGRLinearRing* targetLinearRing,
    const OGRLinearRing* testLinearRing,
    const double& bufferRadius,
    bool& bFlag);
#pragma endregion

#pragma region "双缓冲区要素空间匹配算法"
/*
一、解释
  使用双缓冲区算法进行空间匹配（单个矢量要素和单个矢量要素之间进行空间匹配）
*/
  error_t wkbPoint_double_buff_match_alg(
    const OGRPoint* wkbPoint1,
    const OGRPoint* wkbPoint2,
    const double& wkbPoint1_buf,
    const double& wkbPoint2_buf,
    bool& bFlag);

  error_t wkbLineString_double_buff_match_alg(
    const OGRLineString* wkbLineString1,
    const OGRLineString* wkbLineString2,
    const double& wkbLineString1_buf,
    const double& wkbLineString2_buf,
    bool& bFlag);

  error_t wkbPolygon_double_buff_match_alg(
    const OGRPolygon* wkbPolygon1,
    const OGRPolygon* wkbPolygon2,
    const double& wkbPolygon1_buf,
    const double& wkbPolygon2_buf,
    bool& bFlag);

  error_t wkbMultiPoint_double_buff_match_alg(
    const OGRMultiPoint* wkbMultiPoint1,
    const OGRMultiPoint* wkbMultiPoint2,
    const double& wkbMultiPoint1_buf,
    const double& wkbMultiPoint2_buf,
    bool& bFlag);

  error_t wkbMultiLineString_double_buff_match_alg(
    const OGRMultiLineString* wkbMultiLineString1,
    const OGRMultiLineString* wkbMultiLineString2,
    const double& wkbMultiLineString1_buf,
    const double& wkbMultiLineString2_buf,
    bool& bFlag);

  error_t wkbMultiPolygon_double_buff_match_alg(
    const OGRMultiPolygon* wkbMultiPolygon1,
    const OGRMultiPolygon* wkbMultiPolygon2,
    const double& wkbMultiPolygon1_buf,
    const double& wkbMultiPolygon2_buf,
    bool& bFlag);

  error_t wkbGeometryCollection_double_buff_match_alg(
    const OGRGeometryCollection* wkbGeometryCollection1,
    const OGRGeometryCollection* wkbGeometryCollection2,
    const double& wkbGeometryCollection1_buf,
    const double& wkbGeometryCollection2_buf,
    bool& bFlag);

  error_t wkbLinearRing_double_buff_match_alg(
    const OGRLinearRing* wkbLinearRing1,
    const OGRLinearRing* wkbLinearRing2,
    const double& wkbLinearRing1_buf,
    const double& wkbLinearRing2_buf,
    bool& bFlag);

#pragma endregion

#pragma region "基于距离的空间匹配算法"
/*
一、解释
  可以在这里添加一些基于距离的空间匹配算法（单个矢量要素和单个矢量要素之间进行空间匹配）
*/
#pragma endregion

#pragma endregion

#pragma region "私有数据成员"
private:
  OGRFeature* m_matched_feature;
  OGRFeature* m_match_feature;
  vector_feature_matching_args_t m_vector_feature_matching_args;
#pragma endregion

};
#pragma endregion

#endif //CSE_VECTOR_FEATURE_MATCHING_H



