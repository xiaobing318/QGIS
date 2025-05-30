#pragma region "0 包含头文件（减少重复）"
#include <fstream>   // ifstream
#include <exception> // exception
#include <unordered_map>
#include <stdexcept>
#include <string>

#include "QQCT_NJBDX_Config_Reader.h"
#pragma endregion

//  方便书写，使用别名
using json = nlohmann::json;

/**
 * @brief 读取文件并解析为 nlohmann::json 对象的工具函数
 * @param filePath 配置文件路径
 * @return 解析后的 json 对象
 */
static json loadJsonFromFile(
  const std::string& filePath,
  std::shared_ptr<spdlog::logger> logger)
{
  std::ifstream ifs(filePath);
  if (!ifs.is_open())
  {
    logger->error("打开文件[{}]失败。", filePath);
    throw std::runtime_error("无法打开文件: " + filePath);
  }

  json j;
  try
  {
    // 直接读取并解析 json
    ifs >> j;
  }
  catch (const std::exception& e)
  {
    logger->error("解析文件[{}]中的JSON内容的时候出现了错误，异常：{}", filePath, e.what());
    // 抛出异常，让上层处理
    throw; 
  }

  // 可在此处添加更多的文件完整性校验或json结构校验
  return j;
}

#pragma region "1 读取五种配置文件到自定义结构体的接口"
/*---------------------------------------------*
 * 1. 读取 QQCT_Layers_Fields_Info.json
 *---------------------------------------------*/
QQCT_Layers_Fields_Info_Json_t readQQCT_Layers_Fields_Info_Json(
  const QString& filePath,
  std::shared_ptr<spdlog::logger> logger)
{
  //  日志开始
  logger->info("******************************************************************读取 QQCT_Layers_Fields_Info.json 配置文件开始******************************************************************");
  logger->info("QQCT_Layers_Fields_Info.json 配置文件路径: {}", filePath.toUtf8().toStdString());

  //  最终要返回的结构体
  QQCT_Layers_Fields_Info_Json_t data;

  //  1. 加载 JSON
  json j = loadJsonFromFile(filePath.toLocal8Bit().toStdString(), logger);
  //  2. 解析 "Note"
  if (j.contains("Note") && j["Note"].is_string())
  {
    data.Note = j["Note"].get<std::string>();
    logger->info("已读取配置文件备注（Note): {}", data.Note);
  }
  else
  {
    logger->warn("配置文件中未找到 'Note' 字段或字段类型不正确。");
  }
  //  3. 解析 "QQCT_Layers_Fields" 数组
  if (!j.contains("QQCT_Layers_Fields") || !j["QQCT_Layers_Fields"].is_array())
  {
    logger->error("配置文件中未找到 'QQCT_Layers_Fields' 字段或字段类型不正确，返回空结构体。");
    // 根据需求：也可以抛异常
    return data;
  }

  // 遍历 "QQCT_Layers_Fields"
  for (const auto& layerObj : j["QQCT_Layers_Fields"])
  {
    QQCT_Layers_Fields_t layerData;

    // 3.1 解析 "QQCT_Layer_Name"
    if (layerObj.contains("QQCT_Layer_Name") && layerObj["QQCT_Layer_Name"].is_string())
    {
      layerData.QQCT_Layer_Name = layerObj["QQCT_Layer_Name"].get<std::string>();
    }
    else
    {
      logger->warn("某个图层信息中缺少 'QQCT_Layer_Name' 字段或类型错误。");
    }

    // 3.2 解析 "Size_Of_Fields"
    if (layerObj.contains("Size_Of_Fields") && layerObj["Size_Of_Fields"].is_number_integer())
    {
      layerData.Size_Of_Fields = layerObj["Size_Of_Fields"].get<int>();
    }
    else
    {
      logger->warn("图层 '{}' 中缺少 'Size_Of_Fields' 字段或类型错误，默认为 0。", layerData.QQCT_Layer_Name);
      layerData.Size_Of_Fields = 0;
    }

    // 3.3 解析 "Fields" 数组
    if (layerObj.contains("Fields") && layerObj["Fields"].is_array())
    {
      for (const auto& fld : layerObj["Fields"])
      {
        QQCT_Field_t field;

        // (1) 中文字段名
        if (fld.contains("chinese_field_name") && fld["chinese_field_name"].is_string())
        {
          field.chinese_field_name = fld["chinese_field_name"].get<std::string>();
        }
        else
        {
          logger->warn("图层 '{}' 有字段未提供 'chinese_field_name'，将使用空字符串。", layerData.QQCT_Layer_Name);
        }

        // (2) 英文字段名
        if (fld.contains("english_field_name") && fld["english_field_name"].is_string())
        {
          field.english_field_name = fld["english_field_name"].get<std::string>();
        }
        else
        {
          logger->warn("图层 '{}' 有字段未提供 'english_field_name'，将使用空字符串。", layerData.QQCT_Layer_Name);
        }

        // (3) 字段类型
        if (fld.contains("field_type") && fld["field_type"].is_string())
        {
          std::string typeStr = fld["field_type"].get<std::string>();
          if (typeStr == "OFTInteger")
            field.field_type = OFTInteger;
          else if (typeStr == "OFTInteger64")
            field.field_type = OFTInteger64;
          else if (typeStr == "OFTReal")
            field.field_type = OFTReal;
          else if (typeStr == "OFTString")
            field.field_type = OFTString;
          else
          {
            logger->warn("图层 '{}' 中未知的字段类型 '{}', 将默认设为 OFTString。", layerData.QQCT_Layer_Name, typeStr);
            field.field_type = OFTString; // 默认
          }
        }
        else
        {
          logger->warn("图层 '{}' 有字段未提供 'field_type'，默认为 OFTString。", layerData.QQCT_Layer_Name);
          field.field_type = OFTString;
        }

        // (4) 字段长度
        if (fld.contains("field_width"))
        {
          // 兼容数字或字符串（两种类型都是可以读取）
          if (fld["field_width"].is_number_integer())
          {
            field.field_width = fld["field_width"].get<int>();
          }
          else if (fld["field_width"].is_string())
          {
            std::string lenStr = fld["field_width"].get<std::string>();
            try
            {
              field.field_width = std::stoi(lenStr);
            }
            catch (...)
            {
              logger->warn("图层 '{}' 中字段 'field_width' 无法解析为整数，使用默认值 0。", layerData.QQCT_Layer_Name);
              field.field_width = 0;
            }
          }
          else
          {
            logger->warn("图层 '{}' 中 'field_width' 类型不符合预期，使用默认值 0。", layerData.QQCT_Layer_Name);
            field.field_width = 0;
          }
        }
        else
        {
          field.field_width = 0; // 默认值
        }

        // (5) 字段精度
        if (fld.contains("field_precision"))
        {
          if (fld["field_precision"].is_number_integer())
          {
            field.field_precision = fld["field_precision"].get<int>();
          }
          else if (fld["field_precision"].is_string())
          {
            std::string precStr = fld["field_precision"].get<std::string>();
            try
            {
              field.field_precision = std::stoi(precStr);
            }
            catch (...)
            {
              logger->warn("图层 '{}' 中字段 'field_precision' 无法解析为整数，使用默认值 0。", layerData.QQCT_Layer_Name);
              field.field_precision = 0;
            }
          }
          else
          {
            logger->warn("图层 '{}' 中 'field_precision' 类型不符合预期，使用默认值 0。", layerData.QQCT_Layer_Name);
            field.field_precision = 0;
          }
        }
        else
        {
          // 默认值
          field.field_precision = 0; 
        }

        // (6) 字段备注
        if (fld.contains("field_note") && fld["field_note"].is_string())
        {
          field.field_note = fld["field_note"].get<std::string>();
        }
        else
        {
          // 可视需求是否要警告，这里仅简单示例
          field.field_note = "";
        }

        // 将此字段加入当前图层的字段集合
        layerData.vFields.push_back(field);
      }

      //  验证 Size_Of_Fields 是否一致
      if (layerData.Size_Of_Fields != layerData.vFields.size())
      {
        logger->warn("图层 '{}' 中 'Size_Of_Fields' 为 {}, 但实际解析到 {} 个字段。",
          layerData.QQCT_Layer_Name,
          layerData.Size_Of_Fields,
          layerData.vFields.size());
      }
    }
    else
    {
      logger->warn("图层 '{}' 缺少 'Fields' 字段或其类型不是数组，vFields 将为空。", layerData.QQCT_Layer_Name);
    }

    // 将处理好的图层信息放入 data 中
    data.vQQCT_Layers_Fields.push_back(layerData);
  }


  logger->info("已完成对 QQCT_Layers_Fields_Info.json 的解析，共解析到 {} 个图层。", data.vQQCT_Layers_Fields.size());
  //  日志结束
  logger->info("******************************************************************读取 QQCT_Layers_Fields_Info.json 配置文件结束******************************************************************");

  return data;
}

/*---------------------------------------------*
 * 2. 读取 QQCT_Layers_Mapping_NJBDX_Layers.json
 *---------------------------------------------*/
QQCT_Layers_Mapping_NJBDX_Layers_Json_t readQQCT_Layers_Mapping_NJBDX_Layers_Json(
  const QString& filePath,
  std::shared_ptr<spdlog::logger> logger)
{
  //  日志开始
  logger->info("******************************************************************读取 QQCT_Layers_Mapping_NJBDX_Layers.json 配置文件开始******************************************************************");
  logger->info("QQCT_Layers_Mapping_NJBDX_Layers.json 配置文件路径: {}", filePath.toUtf8().toStdString());

  //  最终要返回的结构体
  QQCT_Layers_Mapping_NJBDX_Layers_Json_t data;

  //  1. 加载 JSON
  json j = loadJsonFromFile(filePath.toLocal8Bit().toStdString(), logger);

  //  2. 解析 "Note"
  if (j.contains("Note") && j["Note"].is_string())
  {
    data.Note = j["Note"].get<std::string>();
    logger->info("已读取配置文件备注(Note): {}", data.Note);
  }
  else
  {
    logger->warn("配置文件中未找到 'Note' 字段或类型不正确。");
  }

  //  3. 解析 "QQCT_layers_mapping_NJBDX_layers" 数组
  if (!j.contains("QQCT_layers_mapping_NJBDX_layers") || !j["QQCT_layers_mapping_NJBDX_layers"].is_array())
  {
    logger->error("配置文件中缺少 'QQCT_layers_mapping_NJBDX_layers' 数组或格式不正确，返回空结构体。");
    return data; // 这里直接返回空结构体，也可根据需求抛异常
  }

  //  遍历 "QQCT_layers_mapping_NJBDX_layers"
  for (const auto& mappingObj : j["QQCT_layers_mapping_NJBDX_layers"])
  {
    // 用于存放单个映射关系
    QQCT_layers_mapping_NJBDX_layers_t oneMapping; 

    // 3.1 解析 "QQCT_Layer" 对象
    if (mappingObj.contains("QQCT_Layer") && mappingObj["QQCT_Layer"].is_object())
    {
      auto QQCTLayerObj = mappingObj["QQCT_Layer"];

      // 1、解析 "QQCT_layer_english_name"
      if (QQCTLayerObj.contains("QQCT_layer_english_name") && QQCTLayerObj["QQCT_layer_english_name"].is_string())
      {
        oneMapping.QQCT_Layer.QQCT_layer_english_name = QQCTLayerObj["QQCT_layer_english_name"].get<std::string>();
      }
      else
      {
        logger->warn("某个映射信息中缺少 'QQCT_layer_english_name' 字段或类型错误。");
      }

      // 2、解析 "QQCT_layer_chinese_name"
      if (QQCTLayerObj.contains("QQCT_layer_chinese_name") && QQCTLayerObj["QQCT_layer_chinese_name"].is_string())
      {
        oneMapping.QQCT_Layer.QQCT_layer_chinese_name = QQCTLayerObj["QQCT_layer_chinese_name"].get<std::string>();
      }
      else
      {
        logger->warn("某个映射信息中缺少 'QQCT_layer_chinese_name' 字段或类型错误。");
      }

      // 3、解析 "QQCT_layer_notes"
      if (QQCTLayerObj.contains("QQCT_layer_notes") && QQCTLayerObj["QQCT_layer_notes"].is_string())
      {
        oneMapping.QQCT_Layer.QQCT_layer_notes = QQCTLayerObj["QQCT_layer_notes"].get<std::string>();
      }
      else
      {
        logger->warn("某个映射信息中缺少 'QQCT_layer_notes' 字段或类型错误。");
      }

      // 4、解析 "QQCT_layer_type"
      if (QQCTLayerObj.contains("QQCT_layer_type") && QQCTLayerObj["QQCT_layer_type"].is_string())
      {
        std::string typeStr = QQCTLayerObj["QQCT_layer_type"].get<std::string>();
        try
        {
          oneMapping.QQCT_Layer.QQCT_layer_type = stringToOGRGeometryType(typeStr);
        }
        catch (const std::invalid_argument& e)
        {
          logger->error("QQCT_layer_type 转换失败: {}", e.what());
          //  根据需求，可以选择抛出异常或设置为默认类型
          oneMapping.QQCT_Layer.QQCT_layer_type = wkbUnknown;
        }
      }
      else
      {
        logger->warn("某个映射信息中缺少 'QQCT_layer_type' 字段或类型错误。");
      }

      // 5、解析 "QQCT_layer_field_number"
      if (QQCTLayerObj.contains("QQCT_layer_field_number") && QQCTLayerObj["QQCT_layer_field_number"].is_number_integer())
      {
        oneMapping.QQCT_Layer.QQCT_layer_field_number = QQCTLayerObj["QQCT_layer_field_number"].get<int>();
      }
      else
      {
        logger->warn("某个映射信息中缺少 'QQCT_layer_field_number' 字段或类型错误，默认为 0。");
        oneMapping.QQCT_Layer.QQCT_layer_field_number = 0;
      }

      // 6、解析 "QQCT_layer_extention" (目前结构体内为空，可按需扩展)
      if (QQCTLayerObj.contains("QQCT_layer_extention") && QQCTLayerObj["QQCT_layer_extention"].is_object())
      {
        // 暂时无字段可解析，如需后续扩展可在此处添加解析逻辑
      }
      else
      {
        logger->warn("某个映射信息中缺少 'QQCT_layer_extention' 字段或类型错误（对象类型），但结构体中目前也没有扩展字段。");
      }
    }
    else
    {
      logger->warn("缺少 'QQCT_Layer' 对象或类型错误，该映射将仅保留默认空层信息。");
    }

    // 3.2 解析 "NJBDX_Mapping_Layers" 数组
    if (mappingObj.contains("NJBDX_Mapping_Layers") && mappingObj["NJBDX_Mapping_Layers"].is_array())
    {
      for (const auto& njbdxLayerObj : mappingObj["NJBDX_Mapping_Layers"])
      {
        QQCT_NJBDX_Mapping_Layer_t oneNJBDXLayer;

        // 1、解析 "NJBDX_layer_english_name"
        if (njbdxLayerObj.contains("NJBDX_layer_english_name") && njbdxLayerObj["NJBDX_layer_english_name"].is_string())
        {
          oneNJBDXLayer.NJBDX_layer_english_name = njbdxLayerObj["NJBDX_layer_english_name"].get<std::string>();
        }
        else
        {
          logger->warn("有某个 NJBDX_Mapping_Layer 缺少 'NJBDX_layer_english_name'。");
        }

        // 2、解析 "NJBDX_layer_chinese_name"
        if (njbdxLayerObj.contains("NJBDX_layer_chinese_name") && njbdxLayerObj["NJBDX_layer_chinese_name"].is_string())
        {
          oneNJBDXLayer.NJBDX_layer_chinese_name = njbdxLayerObj["NJBDX_layer_chinese_name"].get<std::string>();
        }
        else
        {
          logger->warn("有某个 NJBDX_Mapping_Layer 缺少 'NJBDX_layer_chinese_name'。");
        }

        // 3、解析 "NJBDX_layer_notes"
        if (njbdxLayerObj.contains("NJBDX_layer_notes") && njbdxLayerObj["NJBDX_layer_notes"].is_string())
        {
          oneNJBDXLayer.NJBDX_layer_notes = njbdxLayerObj["NJBDX_layer_notes"].get<std::string>();
        }
        else
        {
          logger->warn("有某个 NJBDX_Mapping_Layer 缺少 'NJBDX_layer_notes'。");
        }

        // 4、解析 "NJBDX_layer_type"
        if (njbdxLayerObj.contains("NJBDX_layer_type") && njbdxLayerObj["NJBDX_layer_type"].is_string())
        {
          std::string typeStr = njbdxLayerObj["NJBDX_layer_type"].get<std::string>();
          try
          {
            oneNJBDXLayer.NJBDX_layer_type = stringToOGRGeometryType(typeStr);
          }
          catch (const std::invalid_argument& e)
          {
            logger->error("NJBDX_layer_type 转换失败: {}", e.what());
            // 根据需求，可以选择抛出异常或设置为默认类型
            oneNJBDXLayer.NJBDX_layer_type = wkbUnknown;
          }
        }
        else
        {
          logger->warn("有某个 NJBDX_Mapping_Layer 缺少 'NJBDX_layer_type'。");
        }

        // 5、解析 "NJBDX_layer_field_number"
        if (njbdxLayerObj.contains("NJBDX_layer_field_number") && njbdxLayerObj["NJBDX_layer_field_number"].is_number_integer())
        {
          oneNJBDXLayer.NJBDX_layer_field_number = njbdxLayerObj["NJBDX_layer_field_number"].get<int>();
        }
        else
        {
          logger->warn("有某个 NJBDX_Mapping_Layer 缺少 'NJBDX_layer_field_number'，默认为 0。");
          oneNJBDXLayer.NJBDX_layer_field_number = 0;
        }

        // 6、解析 "NJBDX_layer_extention" (目前结构体内为空，可按需扩展)
        if (njbdxLayerObj.contains("NJBDX_layer_extention") && njbdxLayerObj["NJBDX_layer_extention"].is_object())
        {
          // 暂时无字段可解析，如需后续扩展可在此处添加解析逻辑
        }
        else
        {
          logger->warn("有某个 NJBDX_Mapping_Layer 缺少 'NJBDX_layer_extention' 字段或类型错误（对象类型）。");
        }

        // 添加到 oneMapping 的 NJBDX 列表
        oneMapping.vNJBDX_Mapping_Layers.push_back(oneNJBDXLayer);
      }
    }
    else
    {
      logger->warn("缺少 'NJBDX_Mapping_Layers' 数组或类型错误，该映射将仅保留默认空 NJBDX 信息。");
    }

    // 将该映射结果放入最终 data 中
    data.vQQCT_layers_mapping_NJBDX_layers.push_back(oneMapping);
  }


  //  日志：完成解析
  logger->info("已完成对 QQCT_Layers_Mapping_NJBDX_Layers.json 的解析，共解析到 {} 条映射关系。", data.vQQCT_layers_mapping_NJBDX_layers.size());
  //  日志结束
  logger->info("******************************************************************读取 QQCT_Layers_Mapping_NJBDX_Layers.json 配置文件结束******************************************************************");

  return data;
}

/*---------------------------------------------*
 * 3. 读取 QQCT_Classification_Code_Conditional_Mapping_Lists.json
 *---------------------------------------------*/
QQCT_Classification_Code_Conditional_Mapping_Lists_Json_t readQQCT_Classification_Code_Conditional_Mapping_Lists_Json(
  const QString& filePath,
  std::shared_ptr<spdlog::logger> logger)
{
  //  日志开始
  logger->info("******************************************************************读取 QQCT_Classification_Code_Conditional_Mapping_Lists.json 配置文件开始******************************************************************");
  logger->info("QQCT_Classification_Code_Conditional_Mapping_Lists.json 配置文件路径: {}", filePath.toUtf8().toStdString());

  // 最终要返回的结构体
  QQCT_Classification_Code_Conditional_Mapping_Lists_Json_t data;

  // 1. 加载 JSON
  json j = loadJsonFromFile(filePath.toLocal8Bit().toStdString(), logger);

  // 2. 解析 "Note"
  if (j.contains("Note") && j["Note"].is_string())
  {
    data.Note = j["Note"].get<std::string>();
    logger->info("已读取配置文件备注(Note): {}", data.Note);
  }
  else
  {
    logger->warn("配置文件中未找到 'Note' 字段或字段类型不正确。");
  }

  // 3. 解析 "QQCT_Classification_Code_Conditional_Mapping_Lists" 数组
  if (!j.contains("QQCT_Classification_Code_Conditional_Mapping_Lists") || !j["QQCT_Classification_Code_Conditional_Mapping_Lists"].is_array())
  {
    logger->error("配置文件中缺少 'QQCT_Classification_Code_Conditional_Mapping_Lists' 数组或格式不正确，返回空结构体。");
    return data; // 根据需求：也可以抛异常
  }

  // 遍历 "QQCT_Classification_Code_Conditional_Mapping_Lists"
  for (const auto& itemObj : j["QQCT_Classification_Code_Conditional_Mapping_Lists"])
  {
    QQCT_Classification_Code_Conditional_Mapping_Lists_t oneItem;

    // (1) Note
    if (itemObj.contains("Note") && itemObj["Note"].is_string())
    {
      oneItem.Note = itemObj["Note"].get<std::string>();
    }
    else
    {
      logger->warn("有某个映射项缺少 'Note' 字段或类型错误，将使用空字符串。");
    }

    // (2) QQCT_Classification_Code_Mapping_Condition
    if (itemObj.contains("QQCT_Classification_Code_Mapping_Condition") && itemObj["QQCT_Classification_Code_Mapping_Condition"].is_string())
    {
      oneItem.QQCT_Classification_Code_Mapping_Condition = itemObj["QQCT_Classification_Code_Mapping_Condition"].get<std::string>();
    }
    else
    {
      logger->warn("有某个映射项缺少 'QQCT_Classification_Code_Mapping_Condition' 字段或类型错误，将使用空字符串。");
    }

    // (3) QQCT_Classification_Code_Mapping_Condition_Flag
    if (itemObj.contains("QQCT_Classification_Code_Mapping_Condition_Flag") && itemObj["QQCT_Classification_Code_Mapping_Condition_Flag"].is_string())
    {
      oneItem.QQCT_Classification_Code_Mapping_Condition_Flag = itemObj["QQCT_Classification_Code_Mapping_Condition_Flag"].get<std::string>();
    }
    else
    {
      logger->warn("有某个映射项缺少 'QQCT_Classification_Code_Mapping_Condition_Flag' 字段或类型错误，将使用空字符串。");
    }

    // (4) NJBDX_Classification_Code_Mapping_Condition
    if (itemObj.contains("NJBDX_Classification_Code_Mapping_Condition") && itemObj["NJBDX_Classification_Code_Mapping_Condition"].is_string())
    {
      oneItem.NJBDX_Classification_Code_Mapping_Condition = itemObj["NJBDX_Classification_Code_Mapping_Condition"].get<std::string>();
    }
    else
    {
      logger->warn("有某个映射项缺少 'NJBDX_Classification_Code_Mapping_Condition' 字段或类型错误，将使用空字符串。");
    }

    // 将解析到的结果加入集合
    data.vQQCT_Classification_Code_Conditional_Mapping_Lists.push_back(oneItem);
  }

  // 日志：完成解析
  logger->info("已完成对 QQCT_Classification_Code_Conditional_Mapping_Lists.json 的解析，共解析到 {} 条映射规则。", data.vQQCT_Classification_Code_Conditional_Mapping_Lists.size());

  //  日志结束
  logger->info("******************************************************************读取 QQCT_Classification_Code_Conditional_Mapping_Lists.json 配置文件结束******************************************************************");

  return data;
}

/*---------------------------------------------*
 * 4. 读取 QQCT_Other_Fields_Mapping_NJBDX_Other_Fields.json
 *---------------------------------------------*/
QQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t readQQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json(
  const QString& filePath,
  std::shared_ptr<spdlog::logger> logger)
{
  //  日志开始
  logger->info("******************************************************************读取 QQCT_Other_Fields_Mapping_NJBDX_Other_Fields.json 配置文件开始******************************************************************");
  logger->info("QQCT_Other_Fields_Mapping_NJBDX_Other_Fields.json 配置文件路径: {}", filePath.toUtf8().toStdString());

  // 最终要返回的结构体
  QQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t data;

  // 1. 加载 JSON
  json j = loadJsonFromFile(filePath.toLocal8Bit().toStdString(), logger);

  // 2. 解析 "Note"
  if (j.contains("Note") && j["Note"].is_string())
  {
    data.Note = j["Note"].get<std::string>();
    logger->info("已读取配置文件备注(Note): {}", data.Note);
  }
  else
  {
    logger->warn("配置文件中未找到 'Note' 字段或字段类型不正确。");
  }

  // 3. 解析 "QQCT_Layers_Other_Fields_Mapping" 数组
  if (!j.contains("QQCT_Layers_Other_Fields_Mapping") || !j["QQCT_Layers_Other_Fields_Mapping"].is_array())
  {
    logger->error("配置文件中缺少 'QQCT_Layers_Other_Fields_Mapping' 数组或格式不正确，返回空结构体。");
    return data; // 可按需求改为抛异常
  }

  //  遍历 "QQCT_Layers_Other_Fields_Mapping"
  for (const auto& layerObj : j["QQCT_Layers_Other_Fields_Mapping"])
  {
    QQCT_Layers_Other_Fields_Mapping_t layerMapping;

    // (1) 解析 "NJBDX_Layer_English_Name"
    if (layerObj.contains("NJBDX_Layer_English_Name") && layerObj["NJBDX_Layer_English_Name"].is_string())
    {
      layerMapping.NJBDX_Layer_English_Name = layerObj["NJBDX_Layer_English_Name"].get<std::string>();
    }
    else
    {
      logger->warn("有某个图层映射信息缺少 'NJBDX_Layer_English_Name' 字段或类型错误，使用空字符串。");
    }

    // (2) 解析 "NJBDX_Layer_Chinese_Name"
    if (layerObj.contains("NJBDX_Layer_Chinese_Name") && layerObj["NJBDX_Layer_Chinese_Name"].is_string())
    {
      layerMapping.NJBDX_Layer_Chinese_Name = layerObj["NJBDX_Layer_Chinese_Name"].get<std::string>();
    }
    else
    {
      logger->warn("有某个图层映射信息缺少 'NJBDX_Layer_Chinese_Name' 字段或类型错误，使用空字符串。");
    }

    // (3) 解析 "NJBDX_Layer_Numerical_Identification"
    if (layerObj.contains("NJBDX_Layer_Numerical_Identification") && layerObj["NJBDX_Layer_Numerical_Identification"].is_string())
    {
      layerMapping.NJBDX_Layer_Numerical_Identification = layerObj["NJBDX_Layer_Numerical_Identification"].get<std::string>();
    }
    else
    {
      logger->warn("有某个图层映射信息缺少 'NJBDX_Layer_Numerical_Identification' 字段或类型错误，使用空字符串。");
    }

    // (4) 解析 "Other_Fields_Mapping" 数组
    if (layerObj.contains("Other_Fields_Mapping") && layerObj["Other_Fields_Mapping"].is_array())
    {
      for (const auto& fieldObj : layerObj["Other_Fields_Mapping"])
      {
        QQCT_Other_Fields_Mapping_t fieldMapping;

        // 1、解析 "NJBDX_field_name"
        if (fieldObj.contains("NJBDX_field_name") && fieldObj["NJBDX_field_name"].is_string())
        {
          fieldMapping.NJBDX_field_name = fieldObj["NJBDX_field_name"].get<std::string>();
        }
        else
        {
          logger->warn("某个 Other_Fields_Mapping 缺少 'NJBDX_field_name' 字段或类型错误，使用空字符串。");
        }

        // 2、解析 "QQCT_field_name"
        if (fieldObj.contains("QQCT_field_name") && fieldObj["QQCT_field_name"].is_string())
        {
          fieldMapping.QQCT_field_name = fieldObj["QQCT_field_name"].get<std::string>();
        }
        else
        {
          logger->warn("某个 Other_Fields_Mapping 缺少 'QQCT_field_name' 字段或类型错误，使用空字符串。");
        }

        // 3、解析 "field_mapping_codition_flag"
        if (fieldObj.contains("field_mapping_codition_flag") && fieldObj["field_mapping_codition_flag"].is_string())
        {
          fieldMapping.field_mapping_codition_flag = fieldObj["field_mapping_codition_flag"].get<std::string>();
        }
        else
        {
          logger->warn("某个 Other_Fields_Mapping 缺少 'field_mapping_codition_flag' 字段或类型错误，使用空字符串。");
        }

        // 4、解析 "field_mapping_codition"
        if (fieldObj.contains("field_mapping_codition") && fieldObj["field_mapping_codition"].is_string())
        {
          fieldMapping.field_mapping_codition = fieldObj["field_mapping_codition"].get<std::string>();
        }
        else
        {
          logger->warn("某个 Other_Fields_Mapping 缺少 'field_mapping_codition' 字段或类型错误，使用空字符串。");
        }

        // 将此字段映射信息加入当前图层的映射列表
        layerMapping.vOther_Fields_Mapping.push_back(fieldMapping);
      }
    }
    else
    {
      logger->warn("图层 '{}' 缺少 'Other_Fields_Mapping' 字段或类型错误。", layerMapping.NJBDX_Layer_English_Name);
    }

    // 将处理好的图层映射信息放入 data
    data.vQQCT_Layers_Other_Fields_Mapping.push_back(layerMapping);
  }

  //  日志：完成解析
  logger->info("已完成对 QQCT_Other_Fields_Mapping_NJBDX_Other_Fields.json 的解析，共解析到 {} 个图层。", data.vQQCT_Layers_Other_Fields_Mapping.size());

  //  日志结束
  logger->info("******************************************************************读取 QQCT_Other_Fields_Mapping_NJBDX_Other_Fields.json 配置文件结束******************************************************************");


  return data;
}

/*---------------------------------------------*
 * 5. 读取 NJBDX_Layers_Fields_Info.json
 *---------------------------------------------*/
QQCT_NJBDX_Layers_Fields_Info_Json_t QQCT_readNJBDX_Layers_Fields_Info_Json(
  const QString& filePath,
  std::shared_ptr<spdlog::logger> logger)
{
  //  日志开始
  logger->info("******************************************************************读取 NJBDX_Layers_Fields_Info.json 配置文件开始******************************************************************");
  logger->info("NJBDX_Layers_Fields_Info.json 配置文件路径: {}", filePath.toUtf8().toStdString());

  //  最终要返回的结构体
  QQCT_NJBDX_Layers_Fields_Info_Json_t data;

  //  1. 加载 JSON
  json j = loadJsonFromFile(filePath.toLocal8Bit().toStdString(), logger);
  //  2. 解析 "Note"
  if (j.contains("Note") && j["Note"].is_string())
  {
    data.Note = j["Note"].get<std::string>();
    logger->info("已读取配置文件备注（Note): {}", data.Note);
  }
  else
  {
    logger->warn("配置文件中未找到 'Note' 字段或字段类型不正确。");
  }
  //  3. 解析 "NJBDX_Layers_Fields" 数组
  if (!j.contains("NJBDX_Layers_Fields") || !j["NJBDX_Layers_Fields"].is_array())
  {
    logger->error("配置文件中未找到 'NJBDX_Layers_Fields' 字段或字段类型不正确，返回空结构体。");
    // 根据需求：也可以抛异常
    return data;
  }

  // 遍历 "NJBDX_Layers_Fields"
  for (const auto& layerObj : j["NJBDX_Layers_Fields"])
  {
    QQCT_NJBDX_Layers_Fields_t layerData;

    // 3.1 解析 "NJBDX_Layer_English_Name"
    if (layerObj.contains("NJBDX_Layer_English_Name") && layerObj["NJBDX_Layer_English_Name"].is_string())
    {
      layerData.NJBDX_Layer_English_Name = layerObj["NJBDX_Layer_English_Name"].get<std::string>();
    }
    else
    {
      logger->warn("某个图层信息中缺少 'NJBDX_Layer_English_Name' 字段或类型错误。");
    }

    // 3.2 解析 "NJBDX_Layer_Chinese_Name"
    if (layerObj.contains("NJBDX_Layer_Chinese_Name") && layerObj["NJBDX_Layer_Chinese_Name"].is_string())
    {
      layerData.NJBDX_Layer_Chinese_Name = layerObj["NJBDX_Layer_Chinese_Name"].get<std::string>();
    }
    else
    {
      logger->warn("某个图层信息中缺少 'NJBDX_Layer_Chinese_Name' 字段或类型错误。");
    }

    // 3.3 解析 "NJBDX_Layer_Numerical_Identification"
    if (layerObj.contains("NJBDX_Layer_Numerical_Identification") && layerObj["NJBDX_Layer_Numerical_Identification"].is_string())
    {
      layerData.NJBDX_Layer_Numerical_Identification = layerObj["NJBDX_Layer_Numerical_Identification"].get<std::string>();
    }
    else
    {
      logger->warn("某个图层信息中缺少 'NJBDX_Layer_Numerical_Identification' 字段或类型错误。");
    }

    // 3.4 解析 "Size_Of_Fields"
    if (layerObj.contains("Size_Of_Fields") && layerObj["Size_Of_Fields"].is_number_integer())
    {
      layerData.Size_Of_Fields = layerObj["Size_Of_Fields"].get<int>();
    }
    else
    {
      logger->warn("图层 '{}' 中缺少 'Size_Of_Fields' 字段或类型错误，默认为 0。", layerData.NJBDX_Layer_English_Name);
      layerData.Size_Of_Fields = 0;
    }

    // 3.5 解析 "Fields" 数组
    if (layerObj.contains("Fields") && layerObj["Fields"].is_array())
    {
      for (const auto& fld : layerObj["Fields"])
      {
        QQCT_NJBDX_Field_t field;

        // (1) 中文字段名
        if (fld.contains("chinese_field_name") && fld["chinese_field_name"].is_string())
        {
          field.chinese_field_name = fld["chinese_field_name"].get<std::string>();
        }
        else
        {
          logger->warn("图层 '{}' 有字段未提供 'chinese_field_name'，将使用空字符串。", layerData.NJBDX_Layer_English_Name);
        }

        // (2) 英文字段名
        if (fld.contains("english_field_name") && fld["english_field_name"].is_string())
        {
          field.english_field_name = fld["english_field_name"].get<std::string>();
        }
        else
        {
          logger->warn("图层 '{}' 有字段未提供 'english_field_name'，将使用空字符串。", layerData.NJBDX_Layer_English_Name);
        }

        // (3) 字段类型
        if (fld.contains("field_type") && fld["field_type"].is_string())
        {
          std::string typeStr = fld["field_type"].get<std::string>();
          if (typeStr == "OFTInteger")
            field.field_type = OFTInteger;
          else if (typeStr == "OFTInteger64")
            field.field_type = OFTInteger64;
          else if (typeStr == "OFTReal")
            field.field_type = OFTReal;
          else if (typeStr == "OFTString")
            field.field_type = OFTString;
          else
          {
            logger->warn("图层 '{}' 中未知的字段类型 '{}', 将默认设为 OFTString。", layerData.NJBDX_Layer_English_Name, typeStr);
            field.field_type = OFTString; // 默认
          }
        }
        else
        {
          logger->warn("图层 '{}' 有字段未提供 'field_type'，默认为 OFTString。", layerData.NJBDX_Layer_English_Name);
          field.field_type = OFTString;
        }

        // (4) 字段长度
        if (fld.contains("field_width"))
        {
          // 兼容数字或字符串（两种类型都是可以读取）
          if (fld["field_width"].is_number_integer())
          {
            field.field_width = fld["field_width"].get<int>();
          }
          else if (fld["field_width"].is_string())
          {
            std::string lenStr = fld["field_width"].get<std::string>();
            try
            {
              field.field_width = std::stoi(lenStr);
            }
            catch (...)
            {
              logger->warn("图层 '{}' 中字段 'field_width' 无法解析为整数，使用默认值 0。", layerData.NJBDX_Layer_English_Name);
              field.field_width = 0;
            }
          }
          else
          {
            logger->warn("图层 '{}' 中 'field_width' 类型不符合预期，使用默认值 0。", layerData.NJBDX_Layer_English_Name);
            field.field_width = 0;
          }
        }
        else
        {
          field.field_width = 0; // 默认值
        }

        // (5) 字段精度
        if (fld.contains("field_precision"))
        {
          if (fld["field_precision"].is_number_integer())
          {
            field.field_precision = fld["field_precision"].get<int>();
          }
          else if (fld["field_precision"].is_string())
          {
            std::string precStr = fld["field_precision"].get<std::string>();
            try
            {
              field.field_precision = std::stoi(precStr);
            }
            catch (...)
            {
              logger->warn("图层 '{}' 中字段 'field_precision' 无法解析为整数，使用默认值 0。", layerData.NJBDX_Layer_English_Name);
              field.field_precision = 0;
            }
          }
          else
          {
            logger->warn("图层 '{}' 中 'field_precision' 类型不符合预期，使用默认值 0。", layerData.NJBDX_Layer_English_Name);
            field.field_precision = 0;
          }
        }
        else
        {
          // 默认值
          field.field_precision = 0;
        }

        // (6) 字段备注
        if (fld.contains("field_note") && fld["field_note"].is_string())
        {
          field.field_note = fld["field_note"].get<std::string>();
        }
        else
        {
          // 可视需求是否要警告，这里仅简单示例
          field.field_note = "";
        }

        // 将此字段加入当前图层的字段集合
        layerData.vFields.push_back(field);
      }

      //  验证 Size_Of_Fields 是否一致
      if (layerData.Size_Of_Fields != layerData.vFields.size())
      {
        logger->warn("图层 '{}' 中 'Size_Of_Fields' 为 {}, 但实际解析到 {} 个字段。",
          layerData.NJBDX_Layer_English_Name,
          layerData.Size_Of_Fields,
          layerData.vFields.size());
      }
    }
    else
    {
      logger->warn("图层 '{}' 缺少 'Fields' 字段或其类型不是数组，vFields 将为空。", layerData.NJBDX_Layer_English_Name);
    }

    // 将处理好的图层信息放入 data 中
    data.vNJBDX_Layers_Fields.push_back(layerData);
  }


  logger->info("已完成对 NJBDX_Layers_Fields_Info.json 的解析，共解析到 {} 个图层。", data.vNJBDX_Layers_Fields.size());
  //  日志结束
  logger->info("******************************************************************读取 NJBDX_Layers_Fields_Info.json 配置文件结束******************************************************************");

  return data;
}
#pragma endregion

#pragma region "2 对五种配置文件中读取到的结构体进行正确性检查的接口"
 /**
  * @brief 检查 QQCT_Layers_Fields_Info_Json_t 的正确性
  */
bool checkQQCT_Layers_Fields_Info_Json(
  const QQCT_Layers_Fields_Info_Json_t& data,
  std::shared_ptr<spdlog::logger> logger)
{
  bool isOk = true;
  //  日志开始
  logger->info("******************************************************************检查 QQCT_Layers_Fields_Info.json 配置文件开始******************************************************************");


  // 1. Note 是否为空
  if (data.Note.empty())
  {
    logger->warn("[checkQQCT_Layers_Fields_Info_Json] Note 为空");
  }

  // 2. 遍历每一个图层，检查字段数量是否与数据实际一致
  for (const auto& layer : data.vQQCT_Layers_Fields)
  {
    // spdlog::debug(...) 可以输出更详细信息，或根据需要使用 spdlog::info/ spdlog::warn 等
    logger->debug("[checkQQCT_Layers_Fields_Info_Json] 检查的图层为: {}", layer.QQCT_Layer_Name);

    // Check Size_Of_Fields
    if (layer.Size_Of_Fields <= 0)
    {
      logger->warn("图层 '{}' 的Size_Of_Fields不合法--->{}", layer.QQCT_Layer_Name, layer.Size_Of_Fields);
      isOk = false;
    }

  }

  //  日志结束
  logger->info("******************************************************************检查 QQCT_Layers_Fields_Info.json 配置文件结束******************************************************************");

  return isOk;
}

/**
 * @brief 检查 QQCT_Layers_Mapping_NJBDX_Layers_Json_t 的正确性
 */
bool checkQQCT_Layers_Mapping_NJBDX_Layers_Json(
  const QQCT_Layers_Mapping_NJBDX_Layers_Json_t& data,
  std::shared_ptr<spdlog::logger> logger)
{
  bool isOk = true;
  //  日志开始
  logger->info("******************************************************************检查 QQCT_Layers_Mapping_NJBDX_Layers.json 配置文件开始******************************************************************");

  if (data.Note.empty())
  {
    logger->warn("[checkQQCT_Layers_Mapping_NJBDX_Layers_Json] Note 为空");
  }

  for (const auto& mapping : data.vQQCT_layers_mapping_NJBDX_layers)
  {
    // 根据需要检查字段
    if (mapping.QQCT_Layer.QQCT_layer_english_name.empty())
    {
      logger->warn("QQCT_Layer 有一个空的english_name.");
      isOk = false;
    }

    // 也可根据实际需求检查 vNJBDX_Mapping_Layers
    if (mapping.vNJBDX_Mapping_Layers.empty())
    {
      logger->warn("QQCT_Layer '{}' 没有NJBDX_Mapping_Layers映射", mapping.QQCT_Layer.QQCT_layer_english_name);
      isOk = false;
    }
  }

  //  日志结束
  logger->info("******************************************************************检查 QQCT_Layers_Mapping_NJBDX_Layers.json 配置文件结束******************************************************************");

  return isOk;
}

/**
 * @brief 检查 QQCT_Classification_Code_Conditional_Mapping_Lists_Json_t 的正确性
 */
bool checkQQCT_Classification_Code_Conditional_Mapping_Lists_Json(
  const QQCT_Classification_Code_Conditional_Mapping_Lists_Json_t& data,
  std::shared_ptr<spdlog::logger> logger)
{
  bool isOk = true;
  //  日志开始
  logger->info("******************************************************************检查 QQCT_Classification_Code_Conditional_Mapping_Lists.json 配置文件开始******************************************************************");

  if (data.Note.empty())
  {
    logger->warn("[QQCT_Classification_Code_Conditional_Mapping_Lists_Json] Note 为空。");
  }

  //  日志结束
  logger->info("******************************************************************检查 QQCT_Classification_Code_Conditional_Mapping_Lists.json 配置文件结束******************************************************************");

  return isOk;
}

/**
 * @brief 检查 QQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t 的正确性
 */
bool checkQQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json(
  const QQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t& data,
  std::shared_ptr<spdlog::logger> logger)
{
  bool isOk = true;
  //  日志开始
  logger->info("******************************************************************检查 QQCT_Other_Fields_Mapping_NJBDX_Other_Fields.json 配置文件开始******************************************************************");

  if (data.Note.empty())
  {
    logger->warn("[checkQQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json] Note为空。");
  }

  for (const auto& layer : data.vQQCT_Layers_Other_Fields_Mapping)
  {
    if (layer.NJBDX_Layer_English_Name.empty())
    {
      logger->warn("NJBDX_Layer_English_Name不能为空。");
      isOk = false;
    }
  }

  //  日志结束
  logger->info("******************************************************************检查 QQCT_Other_Fields_Mapping_NJBDX_Other_Fields.json 配置文件结束******************************************************************");
  return isOk;
}

/**
 * @brief 检查 NJBDX_Layers_Fields_Info_Json_t 的正确性
 */
bool checkNJBDX_Layers_Fields_Info_Json(
  const QQCT_NJBDX_Layers_Fields_Info_Json_t& data,
  std::shared_ptr<spdlog::logger> logger)
{
  bool isOk = true;
  //  日志开始
  logger->info("******************************************************************检查 NJBDX_Layers_Fields_Info.json 配置文件开始******************************************************************");

  // 1. Note 是否为空
  if (data.Note.empty())
  {
    logger->warn("[checkNJBDX_Layers_Fields_Info_Json] Note 为空");
  }

  // 2. 遍历每一个图层，检查字段数量是否与数据实际一致
  for (const auto& layer : data.vNJBDX_Layers_Fields)
  {
    // spdlog::debug(...) 可以输出更详细信息，或根据需要使用 spdlog::info/ spdlog::warn 等
    logger->debug("[checkNJBDX_Layers_Fields_Info_Json] 检查的图层为: {}", layer.NJBDX_Layer_English_Name);

    // Check Size_Of_Fields
    if (layer.Size_Of_Fields <= 0)
    {
      logger->warn("图层 '{}' 的Size_Of_Fields不合法--->{}", layer.NJBDX_Layer_English_Name, layer.Size_Of_Fields);
      isOk = false;
    }

  }

  //  日志结束
  logger->info("******************************************************************检查 NJBDX_Layers_Fields_Info.json 配置文件结束******************************************************************");
  return isOk;
}
#pragma endregion

#pragma region "3 辅助函数"
// 辅助函数：将字符串映射到 OGRwkbGeometryType
OGRwkbGeometryType stringToOGRGeometryType(const std::string& typeStr)
{
  static const std::unordered_map<std::string, OGRwkbGeometryType> typeMap = {
      {"Point",      wkbPoint},
      {"MultiPoint", wkbMultiPoint},
      {"LineString", wkbLineString},
      {"MultiLineString", wkbMultiLineString},
      {"Polygon",    wkbPolygon},
      {"MultiPolygon", wkbMultiPolygon},
      {"GeometryCollection", wkbGeometryCollection}
      // 根据需要添加更多类型
  };

  auto it = typeMap.find(typeStr);
  if (it != typeMap.end())
  {
    return it->second;
  }
  else
  {
    throw std::invalid_argument("未知的 GDAL 几何类型: " + typeStr);
  }
}
#pragma endregion
