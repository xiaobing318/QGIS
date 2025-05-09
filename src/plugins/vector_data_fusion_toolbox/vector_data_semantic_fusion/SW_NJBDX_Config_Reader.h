#ifndef SW_NJBDX_CONFIG_READER_H
#define SW_NJBDX_CONFIG_READER_H

#pragma region "0 包含头文件（减少重复）"
#include <string>

#include <QString>

// 使用 nlohmann/json
#include <nlohmann/json.hpp>
// 使用 spdlog
#include <spdlog/spdlog.h>
// 用于最简单的文件日志
#include <spdlog/sinks/basic_file_sink.h>

#include "SW_NJBDX_semantic_fusion_common.h"
#pragma endregion

#pragma region "1 读取五种配置文件到自定义结构体的接口"
// 1. 读取五种配置文件到自定义结构体的接口
SW_Layers_Fields_Info_Json_t readSW_Layers_Fields_Info_Json(
  const QString& filePath,
  std::shared_ptr<spdlog::logger> logger);
SW_Layers_Mapping_NJBDX_Layers_Json_t readSW_Layers_Mapping_NJBDX_Layers_Json(
  const QString& filePath,
  std::shared_ptr<spdlog::logger> logger);
SW_Classification_Code_Conditional_Mapping_Lists_Json_t readSW_Classification_Code_Conditional_Mapping_Lists_Json(
  const QString& filePath,
  std::shared_ptr<spdlog::logger> logger);
SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t readSW_Other_Fields_Mapping_NJBDX_Other_Fields_Json(
  const QString& filePath,
  std::shared_ptr<spdlog::logger> logger);
SW_NJBDX_Layers_Fields_Info_Json_t SW_readNJBDX_Layers_Fields_Info_Json(
  const QString& filePath,
  std::shared_ptr<spdlog::logger> logger);
#pragma endregion

#pragma region "2 对五种配置文件中读取到的结构体进行正确性检查的接口"
// 2. 对五种配置文件中读取到的结构体进行正确性检查的接口
bool checkSW_Layers_Fields_Info_Json(
  const SW_Layers_Fields_Info_Json_t& data,
  std::shared_ptr<spdlog::logger> logger);
bool checkSW_Layers_Mapping_NJBDX_Layers_Json(
  const SW_Layers_Mapping_NJBDX_Layers_Json_t& data,
  std::shared_ptr<spdlog::logger> logger);
bool checkSW_Classification_Code_Conditional_Mapping_Lists_Json(
  const SW_Classification_Code_Conditional_Mapping_Lists_Json_t& data,
  std::shared_ptr<spdlog::logger> logger);
bool checkSW_Other_Fields_Mapping_NJBDX_Other_Fields_Json(
  const SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t& data,
  std::shared_ptr<spdlog::logger> logger);
bool checkNJBDX_Layers_Fields_Info_Json(
  const SW_NJBDX_Layers_Fields_Info_Json_t& data,
  std::shared_ptr<spdlog::logger> logger);
#pragma endregion

#pragma region "3 辅助函数"
static OGRwkbGeometryType stringToOGRGeometryType(const std::string& typeStr);
#pragma endregion

#endif // SW_NJBDX_CONFIG_READER_H
