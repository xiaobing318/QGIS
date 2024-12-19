#pragma region "包含头文件（减少重复）"
#include "consistency_processing_base.h"
#include <vector/cse_vector_feature_matching.h>

#include "gdal_priv.h"
#include <ogrsf_frmts.h>
#include <ogr_feature.h>
#pragma endregion

#pragma region "public函数成员"
ogr_consistency_matching::ogr_consistency_matching(
  OGRLayer* matched_layer,
  OGRLayer* match_layer,
  consistency_processing_base::thresholds_t& thresholds)
  :m_matched_layer(matched_layer),
  m_match_layer(match_layer),
  m_thresholds(thresholds)
{
  //  这里假设的是参数都是有效的
}


ogr_consistency_matching::~ogr_consistency_matching()
{
  //  目前不需要释放资源
}

OGRLayer* ogr_consistency_matching::get_m_matched_layer()
{
  return this->m_matched_layer;
}

OGRLayer* ogr_consistency_matching::get_m_match_layer()
{
  return this->m_match_layer;
}

void ogr_consistency_matching::set_m_matched_layer(OGRLayer* layer)
{
  if (layer != nullptr)
  {
    this->m_matched_layer = layer;
  }
}

void ogr_consistency_matching::set_m_match_layer(OGRLayer* layer)
{
  if (layer != nullptr)
  {
    this->m_match_layer = layer;
  }
}

SE_Error ogr_consistency_matching::two_ogr_layer_consistency_matching(
  std::vector<consistency_processing_base::matching_form_t>& vmatching_form)
{
/*
* 一、解释
这里所做的假设条件是：
  1、私有数据成员m_matched_layer（本底数据图层）是有效的shapefile数据并且存在于memory中，也就是图层被正确打开并且可以访问修改
  2、私有数据成员m_match_layer（匹配数据图层）是有效的shapefile数据并且存在于memory中，也就是图层被正确打开并且可以访问修改

二、算法流程
  1、获取本底数据图层的第i个feature
  2、循环同匹配数据图层的第j个feature进行一致性匹配
*/
  //  创建两个临时的要素指针，用来指向具体的feature
  OGRFeature* matched_layer_ith_feature = nullptr;
  OGRFeature* match_layer_jth_feature = nullptr;
  bool flag = false;
  consistency_processing_base::post_process_input_arguments_t post_process_input_arguments;

  size_t number_features_matched_layer = get_m_matched_layer()->GetFeatureCount();
  size_t number_features_match_layer = get_m_match_layer()->GetFeatureCount();
  for (size_t i = 0; i < number_features_matched_layer; i++)
  {
    matched_layer_ith_feature = get_m_matched_layer()->GetFeature(i);
    if (!matched_layer_ith_feature)
    {
      //  说明没有获得到有效的要素，目前的解决方式就是跳过，处理下一个要素
      continue;
    }
    //  如果获取到的要素是有效的，则获取匹配数据图层中的要素
    for (size_t j = 0; j < number_features_match_layer; j++)
    {
      match_layer_jth_feature = get_m_match_layer()->GetFeature(j);
      if (!match_layer_jth_feature)
      {
        //  说明没有获得到有效的要素，目前的解决方式就是跳过，处理下一个要素
        continue;
      }
      //  单个要素对单个要素进行匹配
      single_feature_match_single_feature(matched_layer_ith_feature, match_layer_jth_feature, flag);
      //  创建post_process函数的所需要的输入参数
      post_process_input_arguments.threshold_level1 = this->m_thresholds.threshold_level1;
      post_process_input_arguments.threshold_level2 = this->m_thresholds.threshold_level2;
      post_process_input_arguments.threshold_level3 = this->m_thresholds.threshold_level3;
      post_process_input_arguments.threshold_level4 = this->m_thresholds.threshold_level4;
      post_process_input_arguments.matched_single_feature = matched_layer_ith_feature;
      post_process_input_arguments.match_single_feature = match_layer_jth_feature;
      //  如果flag是TRUE那么说明上述两个要素是匹配的（空间、属性），那么需要进行匹配后的处理post_process
      if (flag)
      {
        consistency_processing_base::matching_form_t matching_form;
        //  如果两个要素匹配上之后需要在合集表单中记录相应的内容：两个要素在各自图层中的“ID”、空间匹配度、属性相似度、同名要素确认度等等
        //post_process(post_process_input_arguments, matching_form);
        vmatching_form.push_back(matching_form);
      }
    }
  }
}

#pragma endregion


#pragma region "private函数成员"
SE_Error ogr_consistency_matching::single_feature_match_single_feature(
  const OGRFeature* matched_single_feature,
  const OGRFeature* match_single_feature,
  bool& flag)
{
  // 1、得到matched_single_feature、match_single_feature两者的类型
  OGRwkbGeometryType matchedType = wkbFlatten(matched_single_feature->GetGeometryRef()->getGeometryType());
  OGRwkbGeometryType matchType = wkbFlatten(match_single_feature->GetGeometryRef()->getGeometryType());

  // 2、根据不同的类型组合调用不同的函数接口(TODO：目前的接口存在问题，需要根据不同的算法类型来挑选接口，而不是这里写死的)
  if (matchedType == matchType)
  {
    switch (matchedType)
    {
    case wkbPoint:
    {
      const int argc = 1;
      //  目前是写死的
      const char* argv[] = { "20" };
      //return wkbPoint_double_buff_match_alg(matched_single_feature, match_single_feature, argc, argv, flag);
    }
    case wkbLineString:
    {
      const int argc = 1;
      //  目前是写死的
      const char* argv[] = { "20" };
      //return wkbLineString_double_buff_match_alg(matched_single_feature, match_single_feature, argc, argv, flag);
    }
      
    case wkbPolygon:
    {
      const int argc = 1;
      //  目前是写死的
      const char* argv[] = { "20" };
      //return wkbPolygon_double_buff_match_alg(matched_single_feature, match_single_feature, argc, argv, flag);
    }
      // 添加更多类型的匹配算法
    default:
    {
      break;
    }
    }
  }
  else
  {
    return SE_ERROR_OGRFEATURE_TYPE_NOT_MATCH;
  }
}


#pragma endregion

