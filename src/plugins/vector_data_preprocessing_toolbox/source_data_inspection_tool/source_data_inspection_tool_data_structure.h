#ifndef SOURCE_DATA_INSPECTION_TOOL_DATA_STRUCTURE_H
#define SOURCE_DATA_INSPECTION_TOOL_DATA_STRUCTURE_H

#include <vector>
#include <string>

#pragma region "S57属性表结构"

typedef struct S57_single_field
{
    std::string field_name;
    std::string field_name_alias;
    std::string identification_range;
    S57_single_field()
    {
        field_name = "";
        field_name_alias = "";
        identification_range = "";
    }
}S57_single_field_t;


typedef struct S57_single_layer
{
    std::string S57_layer_name;
    std::string S57_layer_alias_name;
    std::vector<S57_single_field_t> vS57_single_field;

    S57_single_layer()
    {
        S57_layer_name = "";
        S57_layer_alias_name = "";
        vS57_single_field.clear();
    }
}S57_single_layer_t;


typedef struct S57_fields4all_layers
{
    std::vector<S57_single_layer_t> vS57_single_layer;
    S57_fields4all_layers()
    {
        vS57_single_layer.clear();
    }
}S57_fields4all_layers_t;

#pragma endregion

#pragma region "S57国家代号"
typedef struct S57_single_country
{
    std::string country_name;
    std::string country_code;
    S57_single_country()
    {
        country_name = "";
        country_code = "";
    }
    
}S57_single_country_t;

typedef struct S57_country_list
{
    std::vector<S57_single_country_t> vS57_single_country;
    S57_country_list()
    {
        vS57_single_country.clear();
    }
}S57_country_list_t;

#pragma endregion

#pragma region "S57生产者编号"

typedef struct S57_single_producer_number
{
    std::string value;
    std::string meaning;
    std::string scale;
    std::string note;
    S57_single_producer_number()
    {
        value = "";
        meaning = "";
        scale = "";
        note = "";
    }
}S57_single_producer_number_t;

typedef struct S57_Producer_number_List
{
    std::vector<S57_single_producer_number_t> vS57_single_producer_number;
    S57_Producer_number_List()
    {
        vS57_single_producer_number.clear();
    }
}S57_Producer_number_List_t;

#pragma endregion

#pragma region "S57文件命名正确性检查--表1-3海图制图数据要素"
typedef struct geometry_type_list
{
  std::string point;
  std::string line;
  std::string polygon;
  geometry_type_list()
  {
    point = "";
    line = "";
    polygon = "";
  }
}geometry_type_list_t;

typedef struct S57_chart_cartographic_data_elements_layer
{
  std::string S57_chart_cartographic_data_elements_layer_ID;
  std::string S57_Object_label_name;
  std::string S57_Object_label_meaning;
  std::string S57_OBJL_filed_attribute_value;
  geometry_type_list_t geometry_type_list;
  S57_chart_cartographic_data_elements_layer()
  {
    S57_chart_cartographic_data_elements_layer_ID = "";
    S57_Object_label_name = "";
    S57_Object_label_meaning = "";
    S57_OBJL_filed_attribute_value = "";
  }

}S57_chart_cartographic_data_elements_layer_t;

typedef struct S57_chart_cartographic_data_elements_layer_list
{
  std::vector<S57_chart_cartographic_data_elements_layer_t> vS57_chart_cartographic_data_elements_layer;
  S57_chart_cartographic_data_elements_layer_list()
  {
    vS57_chart_cartographic_data_elements_layer.clear();
  }
}S57_chart_cartographic_data_elements_layer_list_t;
#pragma endregion

#endif //   SOURCE_DATA_INSPECTION_TOOL_DATA_STRUCTURE_H
