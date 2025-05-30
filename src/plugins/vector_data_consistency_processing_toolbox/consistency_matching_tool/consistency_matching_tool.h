#ifndef CONSISTENCY_MATCHING_TOOL_H
#define CONSISTENCY_MATCHING_TOOL_H

#pragma region "包含头文件（减少重复）"
/*------------------STL-------------------*/
#include <vector>
#include <string>
/*------------------STL-------------------*/

/**********************GDAL***************************/
//  这个头文件提供了对各种矢量数据格式（包括Shapefile）的基本操作接口，如访问数据源、读写图层等API接口
#include <ogrsf_frmts.h>
//  这个头文件提供了对单个矢量要素的操作，包括获取和设置几何形状和属性字段
#include <ogr_feature.h>
//  这个头文件定义了`OGRGeometry`类及其相关的子类，如`OGRPolygon`等，这些类提供了广泛的几何操作方法，包括但不限于相交、并集、差集、缓冲区等几何计算
#include "ogr_geometry.h"
/**********************GDAL***************************/


/*------------------QT-------------------*/
#include <QDialog>
#include <QString>
#include <QStringList>
/*------------------QT-------------------*/



/*------------------UI_header-------------------*/
#include "./ui_consistency_matching_tool_guibase.h"
/*------------------UI_header-------------------*/

#pragma endregion

#pragma region "工具将会使用到的自定义结构体"
typedef long long GIntBig;

typedef enum vector_data_type
{
  Unknow = 0,
  GeoPackage = 1,
  Shapefile = 2
}vector_data_type_t;

typedef enum character_set_encoding_type
{
  UNKNOW = 0,
  UTF8 = 1,
  GBK = 2,
  GB2312 = 3
}character_set_encoding_type_t;

typedef struct thresholds
{
  QString threshold_level1;
  QString threshold_level2;
  QString threshold_level3;
  QString threshold_level4;
  thresholds()
  {
    threshold_level1 = "";
    threshold_level2 = "";
    threshold_level3 = "";
    threshold_level4 = "";
  }
}thresholds_t;

typedef enum erro
{
  NO_ERRO = 0,
  FILE_IS_NOT_EXIST = 1,
  FILE_IS_EXIST = 2,
  DIR_IS_NOT_EXIST = 3,
  DIR_IS_EXIST = 4,
  PATH_IS_VALID = 5,
  PATH_IS_NOT_VALID = 6,
  CONFIG_FILE_IS_EXIST = 7,
  CONFIG_FILE_IS_NOT_EXIST = 8,
  CONFIG_FILE_INFO_T_IS_VALID = 9,
  OPEN_GDALDATASET_FAILED = 10,
  OPEN_FIRST_LAYER_FAILED = 11,
  GET_SINGLE_LAYER_FIELDS_SUCCESSFUL = 12,
  GET_CSV_DRIVER_FAILED = 13,
  CREATE_GDALDATASET_FAILED = 14,
  CREATE_OGRLAYER_FAILED = 15,
  CREATE_FIELD_FAILED = 16,
  CREATE_FEATURE_FAILED = 17,
  GET_SHP_DRIVER_FAILED = 18,
  THRESHOLDS_ARE_NOT_VALID = 19,
  THRESHOLDS_ARE_VALID = 20,
  OGRLAYER_IS_NULLPTR = 21,
  NO_FEATURE_FOUND = 22,
  NO_GEOMETRY_FOUND = 23,
  FID_FIELD_NO_FOUND = 24,
  MATCH_PAIR_IS_EMPTY = 25,
  MATCH_PAIR_IS_NOT_VALID = 26,
  FEATURE_NOT_FOUND = 27,
  SRCDATE_FIELD_NOT_FOUND = 28,
  CREATE_SHP_FILE_FAILED = 29,
  WRITE_FEATURE_FAILED = 30
}erro_t;

typedef enum log_level
{
  //  消息
  MESSAGE = 0,
  //  警告
  WARN = 1,
  //  关闭
  CLOSE = 2,
  //  调试
  DEBUG = 3,
  //  详细
  DETAILED = 4,
  //  一般错误（General errors）
  GENERAL_ERRO = 5,
  //  致命错误
  FATAL_ERRO = 6,
}log_level_t;

#pragma region "配置文件信息结构体"
typedef struct matching_field_pair
{
  std::string background_field_name;
  std::string matching_field_name;
  std::string weight;
  matching_field_pair()
  {
    background_field_name = "";
    matching_field_name = "";
    weight = "";
  }
}matching_field_pair_t;

typedef struct layer_mathcing_layer_pair
{
  std::string background_data_single_layer_name;
  std::string matching_data_single_layer_name;
  std::vector<matching_field_pair_t> vmatching_field_pair;
  layer_mathcing_layer_pair()
  {
    background_data_single_layer_name = "";
    matching_data_single_layer_name = "";
    vmatching_field_pair.clear();
  }
}layer_mathcing_layer_pair_t;

typedef struct config_file_info
{
  std::vector<layer_mathcing_layer_pair_t> vlayer_mathcing_layer_pair;
  config_file_info()
  {
    vlayer_mathcing_layer_pair.clear();
  }
}config_file_info_t;
#pragma endregion

#pragma region "矢量要素匹配对结构体"
typedef struct ogrfeature_index_pair
{
  GIntBig background_fid;
  GIntBig match_fid;
  std::string threshold_level;
  ogrfeature_index_pair()
  {
    background_fid = -1;
    match_fid = -1;
    threshold_level = "0";
  }
}ogrfeature_index_pair_t;

typedef struct ogrfeature_match_pairs
{
  std::vector<ogrfeature_index_pair_t> vogrfeature_index_pairs;
  ogrfeature_match_pairs()
  {
    vogrfeature_index_pairs.clear();
  }
}ogrfeature_match_pairs_t;
#pragma endregion

#pragma region "匹配表单结构体"
typedef struct matching_record
{
  //  空间匹配度
  std::string SMD;
  //  属性相似度
  std::string Attr_Sim;
  //  同名要素确认度
  std::string SameEleConf;
  //  本底图层要素ID
  std::string Data1_ID;
  //  匹配图层要素ID
  std::string RelFacID;
  //  现势性比对
  std::string TCStatus;
  matching_record()
  {
    SMD = "";
    Attr_Sim = "";
    SameEleConf = "";
    Data1_ID = "";
    RelFacID = "";
    TCStatus = "";
  }
}matching_record_t;

typedef struct matching_form
{
  std::vector<matching_record_t> vmatching_record;
  matching_form()
  {
    vmatching_record.clear();
  }
}matching_form_t;

#pragma endregion

#pragma endregion

#pragma region "qgs_consistency_matching工具类"
class qgs_consistency_matching : public QDialog, private Ui::SE_ConsistencyMatchingGuiBase
{
    Q_OBJECT

#pragma region "工具的公开API接口"
  public:
    qgs_consistency_matching( QWidget *parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags() );
    ~qgs_consistency_matching() override;
    std::string erro_t2string(const erro_t& erro_type);
  public slots:
#pragma endregion

#pragma region "工具的私有API接口"
  private:
    void Button_Start();
    void Button_Close();

    QString get_m_background_data_path();
    QString get_m_matching_data_path();
    vector_data_type_t get_m_input_data_type();
    character_set_encoding_type_t get_m_character_set_encoding_type();
    thresholds_t get_m_thresholds();
    QString get_m_config_file_path();
    log_level_t get_m_log_level();
    QString get_m_log_saving_path();
    QString get_m_output_data_path();

    void set_m_background_data_path(const QString &background_data_path);
    void set_m_matching_data_path(const QString &matching_data_path);
    void set_m_input_data_type(const vector_data_type_t& vector_data_type);
    void set_m_character_set_encoding_type(const character_set_encoding_type_t& character_set_encoding_type);
    void set_m_thresholds(const thresholds_t& thresholds);
    void set_m_config_file_path(const QString & config_file_path);
    void set_m_log_level(const log_level_t& log_level);
    void set_m_log_saving_path(const QString& log_saving_path);
    void set_m_output_data_path(const QString& output_data_path);

    //  解释config_file_path配置文件中的内容，将读取到的信息存储到m_config_file_info中
    erro_t parse_config_file(const QString& config_file_path);
    int is_background_layer_exist(const std::string& strFieldName, bool& flag);
    int is_matching_layer_exist(const std::string& strFieldName, bool& flag);
    erro_t is_config_file_info_t_valid();
    //  路径path是否有效
    erro_t is_path_valid(const QString& path);
    //  给定文件路径，检查文件是否存在
    erro_t is_file_exist(const QString& file_path);
    //  给定目录路径，检查目录是否存在
    erro_t is_dir_exist(const QString& dir_path);
    //  验证获取到的阈值是否有效
    erro_t is_thresholds_valid(const thresholds_t& thresholds);
    // 打开一个指定的Shapefile数据集
    GDALDataset* open_shapefile_dataset(const QString& dataset_path);
    // 打开一个指定的GeoPackage数据集
    GDALDataset* open_geopackage_dataset(const QString& dataset_path);
    //  初始化GDAL
    void initialize_gdal();
    //  检查GDAL数据集中是否存在指定名称的矢量图层
    bool hasLayer(GDALDataset* poDS, const char* layerName, int& index);
    //  矢量图层<---->矢量图层空间一致性匹配处理
    erro_t layer_spatial_match(
      OGRLayer* background_data_layer,
      OGRLayer* match_data_layer,
      ogrfeature_match_pairs_t& match_pairs);
    //  矢量图层<---->矢量图层属性一致性匹配处理
    erro_t layer_attri_match(
      OGRLayer* background_data_layer,
      OGRLayer* match_data_layer,
      const ogrfeature_match_pairs_t& match_pairs,
      matching_form_t& matching_form);
    //  计算属性相似度（Attr_Sim）
    std::string get_Attr_Sim();

    //  计算属性相似度（Attr_Sim）
    erro_t get_TCStatus(
      OGRLayer* background_data_layer,
      OGRLayer* match_data_layer,
      const GIntBig& bg_feature_fid,
      const GIntBig& mt_feature_fid,
      std::string& TCStatus_value);

    //  将匹配信息写入到shapefile中
    erro_t write_matching_form2shapefile(
      OGRLayer* background_data_layer,
      OGRLayer* match_data_layer,
      const matching_form_t& matching_form);

    std::vector<std::string> getLayers(const std::string& base_data_path);
    std::vector<std::string> createMapping(
      const std::vector<std::string>& background_layers,
      const std::vector<std::string>& matching_layers);

    erro_t writeMapping2csv(
      const std::vector<std::string>& mappings,
      const std::string& config_file_path);

    erro_t get_single_layer_fields(
      const std::string& single_layer_path,
      std::vector<std::string>& fields);

    erro_t write_info2csv(
      const std::string& config_file_path,
      const std::string& background_data_path,
      const std::string& background_single_layer_fields,
      const std::string& matching_data_path,
      const std::string& mathcing_single_layer_fields,
      const std::string weight);

    erro_t layer_matching(
      const std::string& background_data_layer_path,
      const std::string& matching_data_layer_path,
      const std::string& output_data_path);

    void saveState();
    void restoreState();
    void QPushButton_background_browse();
    void QPushButton_matching_browse();
    void QPushButton_log_browse();
    void QPushButton_output_data_path_browse();
    void QPushBotton_generate_config_file();
    void QPushBotton_load_config_file();
#pragma endregion

#pragma region "工具的私有数据成员"
  private:
    //  本底数据所在的文件目录（1、字符集编码：UTF-8；2、统一采用'/'而不是'\\'）
    QString m_background_data_path = "";
    //  匹配数据所在的文件目录（1、字符集编码：UTF-8；2、统一采用'/'而不是'\\'）
    QString m_matching_data_path = "";
    //  日志文件所在的文件目录（1、字符集编码：UTF-8；2、统一采用'/'而不是'\\'）
    QString m_log_saving_path = "";
    //  输出数据所在的文件目录（1、字符集编码：UTF-8；2、统一采用'/'而不是'\\'）
    QString m_output_data_path = "";
    //  配置文件所在的文件目录（1、字符集编码：UTF-8；2、统一采用'/'而不是'\\'）（注：目前配置文件路径是硬编码）
    QString m_config_file_path = "";

    //  输入数据类型：Shapefile、GeoPackage
    vector_data_type_t m_input_data_type = vector_data_type::GeoPackage;
    //  输入数据属性表中的字符集编码类型：UTF-8、GBK等
    character_set_encoding_type_t m_character_set_encoding_type = character_set_encoding_type::UTF8;
    //  矢量要素空间匹配将会用到的四个阈值等级
    thresholds_t m_thresholds = thresholds_t();
    //  空间匹配算法种类


    //  内部配置文件结构体，将配置文件中的内容读取存储到自定义结构体中
    config_file_info_t m_config_file_info = config_file_info();
    //  日志等级（TODO：将会使用spdlog第三方库实现）
    log_level_t m_log_level = log_level::MESSAGE;

    //  本底数据集(shapefile)
    GDALDataset* poShpBackgroundDS = nullptr;
    //  匹配数据集(shapefile)
    GDALDataset* poShpMatchDS = nullptr;

    //  本底数据集(geopackage)
    GDALDataset* poGeoPackageBackgroundDS = nullptr;
    //  匹配数据集(geopackage)
    GDALDataset* poGeoPackageMatchDS = nullptr;


    //  ui界面类
    SE_ConsistencyMatchingGuiBase* m_SE_ConsistencyMatchingGuiBase = nullptr;

#pragma endregion

};
#pragma endregion

#endif // CONSISTENCY_MATCHING_TOOL_H
