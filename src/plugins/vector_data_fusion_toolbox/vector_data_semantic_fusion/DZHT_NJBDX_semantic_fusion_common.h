#ifndef DZHT_NJBDX_SEMANTIC_FUSION_COMMON_H
#define DZHT_NJBDX_SEMANTIC_FUSION_COMMON_H

#pragma region "0 包含头文件（减少重复）"
#include <string>
#include <vector>
//  确保包含 GDAL/OGR 相关头文件，用于 OGRFieldType
#include "ogrsf_frmts.h"

#pragma endregion

#pragma region "1 DZHT配置文件自定义结构体"

#pragma region "1.1 DZHT_Layers_Fields_Info.json配置文件"
/*
Note: 杨小兵-2025-01-14

1、当前 DZHT_Layers_Fields_Info_Json 结构体的作用是为了映射 DZHT_Layers_Fields_Info.json 文件中的内容
2、这里设置的结构体结构应该与配置文件中的结构相同
3、抽象的层次从上到下是越来越高
*/

// DZHT数据源中单个图层的单个字段信息
typedef struct DZHT_Field
{
  std::string chinese_field_name;   // 中文字段名
  std::string english_field_name;   // 英文字段名
  OGRFieldType field_type;          // 字段类型
  int field_width;                 // 字段长度
  int field_precision;              // 字段精度
  std::string field_note;           // 字段备注

  // 默认构造函数
  DZHT_Field()
    : chinese_field_name(""),
    english_field_name(""),
    field_type(OFTString),          // 默认类型可以根据实际需求调整
    field_width(0),
    field_precision(0),
    field_note("")
  {
  }

} DZHT_Field_t;

// DZHT数据源中单个图层相关信息
typedef struct DZHT_Layers_Fields
{
  std::string DZHT_Layer_Name;             // 图层名称
  int Size_Of_Fields;                       // 字段数量
  std::vector<DZHT_Field_t> vFields;       // 字段集合

  // 默认构造函数
  DZHT_Layers_Fields()
    : DZHT_Layer_Name(""),
    Size_Of_Fields(0),
    vFields()
  {
  }

} DZHT_Layers_Fields_t;

// DZHT数据源中所有图层字段信息（有哪些图层？每个图层有哪些字段？）
typedef struct DZHT_Layers_Fields_Info_Json
{
  std::string Note;                                        // 配置文件备注
  std::vector<DZHT_Layers_Fields_t> vDZHT_Layers_Fields; // 图层字段信息集合

  // 默认构造函数
  DZHT_Layers_Fields_Info_Json()
    : Note(""),
    vDZHT_Layers_Fields()
  {
  }

} DZHT_Layers_Fields_Info_Json_t;

//  1.1 DZHT_Layers_Fields_Info.json配置文件
#pragma endregion



#pragma region "1.2 DZHT_Layers_Mapping_NJBDX_Layers.json配置文件"
/*
Note: 杨小兵-2025-01-14

1、当前 DZHT_Layers_Mapping_NJBDX_Layers_Json 结构体的作用是为了映射 DZHT_Layers_Mapping_NJBDX_Layers.json 文件中的内容
2、这里设置的结构体结构应该与配置文件中的结构相同
3、抽象的层次从上到下是越来越高
*/

// DZHT数据源映射到NJBDX数据源图层未来可能拓展的部分
typedef struct DZHT_layer_extention
{
  // TODO:添加额外扩展的部分

  // 默认构造函数
  DZHT_layer_extention()
  {
  }
} DZHT_layer_extention_t;

// DZHT数据源映射到NJBDX数据源图层未来可能拓展的部分
typedef struct DZHT_NJBDX_layer_extention
{
  // TODO:添加额外扩展的部分

  // 默认构造函数
  DZHT_NJBDX_layer_extention()
  {
  }
} DZHT_NJBDX_layer_extention_t;

// DZHT数据源中单个DZHT图层的信息
typedef struct DZHT_Layer
{
  std::string DZHT_layer_english_name;
  std::string DZHT_layer_chinese_name;
  std::string DZHT_layer_notes;
  OGRwkbGeometryType DZHT_layer_type;
  int DZHT_layer_field_number;
  DZHT_layer_extention_t DZHT_layer_extention;

  // 默认构造函数
  DZHT_Layer()
    : DZHT_layer_english_name(""),
    DZHT_layer_chinese_name(""),
    DZHT_layer_notes(""),
    DZHT_layer_type(wkbUnknown),
    DZHT_layer_field_number(0),
    DZHT_layer_extention()
  {
  }
} DZHT_Layer_t;

// DZHT数据源中单个NJBDX图层的信息
typedef struct DZHT_NJBDX_Mapping_Layer
{
  std::string NJBDX_layer_english_name;
  std::string NJBDX_layer_chinese_name;
  std::string NJBDX_layer_notes;
  OGRwkbGeometryType NJBDX_layer_type;
  int NJBDX_layer_field_number;
  DZHT_NJBDX_layer_extention_t NJBDX_layer_extention;

  // 默认构造函数
  DZHT_NJBDX_Mapping_Layer()
    : NJBDX_layer_english_name(""),
    NJBDX_layer_chinese_name(""),
    NJBDX_layer_notes(""),
    NJBDX_layer_type(wkbUnknown),
    NJBDX_layer_field_number(0),
    NJBDX_layer_extention()
  {
  }
} DZHT_NJBDX_Mapping_Layer_t;

// DZHT数据源中单个图层映射到多个NJBDX图层的信息
typedef struct DZHT_layers_mapping_NJBDX_layers
{
  DZHT_Layer_t DZHT_Layer;
  std::vector<DZHT_NJBDX_Mapping_Layer_t> vNJBDX_Mapping_Layers;

  // 默认构造函数
  DZHT_layers_mapping_NJBDX_layers()
    : DZHT_Layer(),
    vNJBDX_Mapping_Layers()
  {
  }
} DZHT_layers_mapping_NJBDX_layers_t;

// DZHT数据源中图层信息（哪些图层映射到哪些图层中？一对多映射，这其中包含多对一）
typedef struct DZHT_Layers_Mapping_NJBDX_Layers_Json
{
  std::string Note;
  std::vector<DZHT_layers_mapping_NJBDX_layers_t> vDZHT_layers_mapping_NJBDX_layers;

  // 默认构造函数
  DZHT_Layers_Mapping_NJBDX_Layers_Json()
    : Note(""),
    vDZHT_layers_mapping_NJBDX_layers()
  {
  }
} DZHT_Layers_Mapping_NJBDX_Layers_Json_t;

//  1.2 DZHT_Layers_Mapping_NJBDX_Layers.json配置文件
#pragma endregion



#pragma region "1.3 DZHT_Classification_Code_Conditional_Mapping_Lists.json配置文件"
/*
Note: 杨小兵-2025-01-14

1、当前 DZHT_Classification_Code_Conditional_Mapping_Lists_Json 结构体的作用是为了映射 DZHT_Classification_Code_Conditional_Mapping_Lists.json 文件中的内容
2、这里设置的结构体结构应该与配置文件中的结构相同
3、抽象的层次从上到下是越来越高
*/

// DZHT数据源中单个分类编码映射表项的信息
typedef struct DZHT_Classification_Code_Conditional_Mapping_Lists
{
  std::string Note;
  std::string DZHT_Classification_Code_Mapping_Condition;
  std::string DZHT_Classification_Code_Mapping_Condition_Flag;
  std::string NJBDX_Classification_Code_Mapping_Condition;

  // 默认构造函数
  DZHT_Classification_Code_Conditional_Mapping_Lists()
    : Note(""),
    DZHT_Classification_Code_Mapping_Condition(""),
    DZHT_Classification_Code_Mapping_Condition_Flag(""),
    NJBDX_Classification_Code_Mapping_Condition("")
  {
  }
} DZHT_Classification_Code_Conditional_Mapping_Lists_t;

// DZHT数据源中分类编码映射的所有信息（给定的分类编码应该映射到NJBDX数据源中的哪一个分类编码中）
typedef struct DZHT_Classification_Code_Conditional_Mapping_Lists_Json
{
  std::string Note;
  std::vector<DZHT_Classification_Code_Conditional_Mapping_Lists_t> vDZHT_Classification_Code_Conditional_Mapping_Lists;

  // 默认构造函数
  DZHT_Classification_Code_Conditional_Mapping_Lists_Json()
    : Note(""),
    vDZHT_Classification_Code_Conditional_Mapping_Lists()
  {
  }
} DZHT_Classification_Code_Conditional_Mapping_Lists_Json_t;

//  1.3 DZHT_Classification_Code_Conditional_Mapping_Lists.json配置文件
#pragma endregion 



#pragma region "1.4 DZHT_Other_Fields_Mapping_NJBDX_Other_Fields.json配置文件"
/*
Note: 杨小兵-2025-01-14

1、当前 DZHT_Other_Fields_Mapping_NJBDX_Other_Fields_Json 结构体的作用是为了映射 DZHT_Other_Fields_Mapping_NJBDX_Other_Fields.json 文件中的内容
2、这里设置的结构体结构应该与配置文件中的结构相同
3、抽象的层次从上到下是越来越高
*/

//  DZHT数据源中单个其他字段映射的信息
typedef struct DZHT_Other_Fields_Mapping
{
  std::string NJBDX_field_name;
  std::string DZHT_field_name;
  std::string field_mapping_codition_flag;
  std::string field_mapping_codition;

  //  默认构造函数
  DZHT_Other_Fields_Mapping()
    :NJBDX_field_name(""),
    DZHT_field_name(""),
    field_mapping_codition_flag(""),
    field_mapping_codition("")
  {
  }
}DZHT_Other_Fields_Mapping_t;

// DZHT数据源中单图层其他字段映射的信息
typedef struct DZHT_Layers_Other_Fields_Mapping
{
  std::string NJBDX_Layer_English_Name;
  std::string NJBDX_Layer_Chinese_Name;
  std::string NJBDX_Layer_Numerical_Identification;
  std::vector<DZHT_Other_Fields_Mapping_t> vOther_Fields_Mapping;

  //  默认构造函数
  DZHT_Layers_Other_Fields_Mapping()
    :NJBDX_Layer_English_Name(""),
    NJBDX_Layer_Chinese_Name(""),
    NJBDX_Layer_Numerical_Identification(""),
    vOther_Fields_Mapping()
  {
  }
}DZHT_Layers_Other_Fields_Mapping_t;

// DZHT数据源中其他字段映射的所有信息（一个DZHT数据源中要素的其他字段如何映射到NJBDX要素的其他字段中？）
typedef struct DZHT_Other_Fields_Mapping_NJBDX_Other_Fields_Json
{
  std::string Note;
  std::vector<DZHT_Layers_Other_Fields_Mapping_t> vDZHT_Layers_Other_Fields_Mapping;
  //  默认构造函数
  DZHT_Other_Fields_Mapping_NJBDX_Other_Fields_Json()
    :Note(""),
    vDZHT_Layers_Other_Fields_Mapping()
  {
  }
}DZHT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t;

//  1.4 DZHT_Other_Fields_Mapping_NJBDX_Other_Fields.json配置文件
#pragma endregion



#pragma region "1.5 DZHT：单个分幅数据中所有矢量图层信息"
//  DZHT：单个矢量数据层信息
typedef struct DZHT_single_frame_data_single_layer_info
{
  std::string layer_name;                    // 图层名称
  OGRwkbGeometryType layer_geo_type;         // 图层几何类型（使用GDAL中的枚举）
  OGRLayer* polayer;                         // 图层指针
  OGRSpatialReference* poSRS;                // 图层空间参考

  // 默认构造函数
  DZHT_single_frame_data_single_layer_info()
    : layer_name(""),
    layer_geo_type(wkbUnknown),
    polayer(nullptr),
    poSRS(nullptr)
  {
  }

} DZHT_single_frame_data_single_layer_info_t;

//  DZHT：单个分幅数据中所有矢量数据层信息
typedef struct DZHT_single_frame_data_all_layers_info
{
  std::string DZHT_single_frame_data_path;                                                             // 分幅数据目录路径
  GDALDataset* DZHT_single_frame_data_poDS;                                                            // 分幅数据集指针
  std::vector<DZHT_single_frame_data_single_layer_info_t> vDZHT_single_frame_data_single_layer_info;  // 所有图层的信息

  // 默认构造函数
  DZHT_single_frame_data_all_layers_info()
    : DZHT_single_frame_data_path(""),
    DZHT_single_frame_data_poDS(nullptr),
    vDZHT_single_frame_data_single_layer_info()
  {
  }

  // 析构函数（TODO:是否在这里释放？还是说在某个地方手动释放）
  //~DZHT_single_frame_data_all_layers_info()
  //{
  //  // 关闭 GDAL 数据集
  //  if (DZHT_single_frame_data_poDS)
  //  {
  //    GDALClose(DZHT_single_frame_data_poDS);
  //    DZHT_single_frame_data_poDS = nullptr;
  //  }
  //  // 释放每个图层的空间参考
  //  for (auto& layer_info : vDZHT_single_frame_data_single_layer_info)
  //  {
  //    if (layer_info.poSRS)
  //    {
  //      layer_info.poSRS->Release();
  //      layer_info.poSRS = nullptr;
  //    }
  //    if (layer_info.polayer)
  //    {
  //      layer_info.polayer->Release();
  //      layer_info.polayer = nullptr;
  //    }
  //  }
  //}
} DZHT_single_frame_data_all_layers_info_t;

//  1.5 DZHT：单个分幅数据中所有矢量图层信息
#pragma endregion



#pragma region "1.6 NJBDX：单个分幅数据中所有矢量图层信息"
//  NJBDX：单个矢量数据层信息
typedef struct DZHT_NJBDX_single_frame_data_single_layer_info
{
  std::string layer_name;                    // 图层名称
  OGRwkbGeometryType layer_geo_type;         // 图层几何类型（使用GDAL中的枚举）
  OGRLayer* polayer;                         // 图层指针
  OGRSpatialReference* poSRS;                // 图层空间参考

  // 默认构造函数
  DZHT_NJBDX_single_frame_data_single_layer_info()
    : layer_name(""),
    layer_geo_type(wkbUnknown),
    polayer(nullptr),
    poSRS(nullptr)
  {
  }

} DZHT_NJBDX_single_frame_data_single_layer_info_t;

//  NJBDX：单个分幅数据中所有矢量数据层信息
typedef struct DZHT_NJBDX_single_frame_data_all_layers_info
{
  std::string NJBDX_single_frame_data_path;                                                             // 分幅数据目录路径
  GDALDataset* NJBDX_single_frame_data_poDS;                                                            // 分幅数据集指针
  std::vector<DZHT_NJBDX_single_frame_data_single_layer_info_t> vNJBDX_single_frame_data_single_layer_info;  // 所有图层的信息

  // 默认构造函数
  DZHT_NJBDX_single_frame_data_all_layers_info()
    : NJBDX_single_frame_data_path(""),
    NJBDX_single_frame_data_poDS(nullptr),
    vNJBDX_single_frame_data_single_layer_info()
  {
  }

  // 析构函数（TODO:是否在这里释放？还是说在某个地方手动释放）
  //~NJBDX_single_frame_data_all_layers_info()
  //{
  //  // 关闭 GDAL 数据集
  //  if (NJBDX_single_frame_data_poDS)
  //  {
  //    GDALClose(NJBDX_single_frame_data_poDS);
  //    NJBDX_single_frame_data_poDS = nullptr;
  //  }
  //  // 释放每个图层的空间参考
  //  for (auto& layer_info : vNJBDX_single_frame_data_single_layer_info)
  //  {
  //    if (layer_info.poSRS)
  //    {
  //      layer_info.poSRS->Release();
  //      layer_info.poSRS = nullptr;
  //    }
  //    if (layer_info.polayer)
  //    {
  //      layer_info.polayer->Release();
  //      layer_info.polayer = nullptr;
  //    }
  //  }
  //}
} DZHT_NJBDX_single_frame_data_all_layers_info_t;

//  1.6 NJBDX：单个分幅数据中所有矢量图层信息
#pragma endregion

#pragma endregion

#pragma region "2 NJBDX配置文件自定义结构体"

#pragma region "2.1 NJBDX_Layers_Fields_Info.json配置文件"
/*
Note: 杨小兵-2025-01-14

1、当前 DZHT_NJBDX_Layers_Fields_Info_Json 结构体的作用是为了映射 NJBDX_Layers_Fields_Info.json 文件中的内容
2、这里设置的结构体结构应该与配置文件中的结构相同
3、抽象的层次从上到下是越来越高
*/

//  NJBDX数据源中单个图层的单个字段信息
typedef struct DZHT_NJBDX_Field
{
  std::string chinese_field_name;   // 中文字段名
  std::string english_field_name;   // 英文字段名
  OGRFieldType field_type;          // 字段类型
  int field_width;                 // 字段长度
  int field_precision;              // 字段精度
  std::string field_note;           // 字段备注

  // 默认构造函数
  DZHT_NJBDX_Field()
    : chinese_field_name(""),
    english_field_name(""),
    field_type(OFTString),          // 默认类型可以根据实际需求调整
    field_width(0),
    field_precision(0),
    field_note("")
  {
  }

} DZHT_NJBDX_Field_t;

//  NJBDX数据源中单个图层相关信息
typedef struct DZHT_NJBDX_Layers_Fields
{
  std::string NJBDX_Layer_English_Name;                     // 图层英文名称
  std::string NJBDX_Layer_Chinese_Name;                     // 图层中文名称
  std::string NJBDX_Layer_Numerical_Identification;         // 图层数值标识
  int Size_Of_Fields;                                       // 字段数量
  std::vector<DZHT_NJBDX_Field_t> vFields;                 // 字段集合

  // 默认构造函数
  DZHT_NJBDX_Layers_Fields()
    : NJBDX_Layer_English_Name(""),
    NJBDX_Layer_Chinese_Name(""),
    NJBDX_Layer_Numerical_Identification(""),
    Size_Of_Fields(0),
    vFields()
  {
  }

} DZHT_NJBDX_Layers_Fields_t;

//  NJBDX数据源中所有图层字段信息（有哪些图层？每个图层有哪些字段？）
typedef struct DZHT_NJBDX_Layers_Fields_Info_Json
{
  std::string Note;                                                 // 配置文件备注
  std::vector<DZHT_NJBDX_Layers_Fields_t> vNJBDX_Layers_Fields;    // 图层字段信息集合

  //  默认构造函数
  DZHT_NJBDX_Layers_Fields_Info_Json()
    : Note(""),
    vNJBDX_Layers_Fields()
  {
  }

} DZHT_NJBDX_Layers_Fields_Info_Json_t;

//  2.1 NJBDX_Layers_Fields_Info.json配置文件
#pragma endregion

#pragma endregion

#endif // DZHT_NJBDX_SEMANTIC_FUSION_COMMON_H
