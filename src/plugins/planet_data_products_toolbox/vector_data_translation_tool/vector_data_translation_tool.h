#ifndef VECTOR_DATA_TRANSLATION_TOOL_H
#define VECTOR_DATA_TRANSLATION_TOOL_H

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
#include "ui_vector_data_translation_tool.h"
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

typedef enum error
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
  WRITE_FEATURE_FAILED = 30,
  OGRFEATUREDEFN_IS_NULLPTR = 31,
  FIELD_NAME_IS_NULL = 32,
  AWAITING_FIELD_NOT_FOUND = 33,
  CURL_EASY_PERFORM_FAILED = 34
}error_t;

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
/*
* 配置文件结构
*******************************************************************
*字段*                     *翻译后语种*              *存储字段*
*******************************************************************
*AA *                       *CH|EN*            *AA_cname| AA_ename*
*******************************************************************
*BB *                        *EN*                   *BB_ename*
*******************************************************************
*CC *                        *NUL*                    *NUL*
*******************************************************************
*/
typedef struct single_field_record
{
  std::string field_name;
  std::vector<std::string> vlanguage_codes;
  std::vector<std::string> vstorage_fields;
  single_field_record()
  {
    field_name = "";
    vlanguage_codes.clear();
    vstorage_fields.clear();
  }
}single_field_record_t;

typedef struct config_file_info
{
  std::vector<single_field_record_t> vfield_records;
  config_file_info()
  {
    vfield_records.clear();
  }
}config_file_info_t;
#pragma endregion

#pragma endregion

#pragma region "vector_data_translation工具类"
class vector_data_translation : public QDialog, private Ui::SE_VectorDataTranslationGuiBase
{
    Q_OBJECT

#pragma region "工具的公开API接口"
  public:
    vector_data_translation( QWidget *parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags() );
    ~vector_data_translation() override;
    std::string erro_t2string(const error_t& erro_type);
  public slots:
#pragma endregion

#pragma region "工具的私有API接口"
//  "工具的私有API接口" start
  private:
    
#pragma region "1、内部逻辑函数"
/********************************************set/get********************************************/
    QString get_m_input_data_path();
    QString get_m_log_saving_path();
    QString get_m_config_file_path();
    character_set_encoding_type_t get_m_character_set_encoding_type();
    log_level_t get_m_log_level();
    
    void set_m_input_data_path(const QString&input_data_path);
    void set_m_log_saving_path(const QString& log_saving_path);
    void set_m_config_file_path(const QString& config_file_path);
    void set_m_character_set_encoding_type(const character_set_encoding_type_t& character_set_encoding_type);
    void set_m_log_level(const log_level_t& log_level);
/********************************************set/get********************************************/

/********************************************Utility Functions********************************************/
    //  解析配置文件
    error_t parse_config_file(const QString& config_file_path);
    //  检查读取得到的配置文件信息是否有效
    error_t is_config_file_info_t_valid();
    //  检查字段名称是否在配置信息中存在
    int is_config_info_exist(const std::string& strFieldValue, bool& flag);
    //  解析语种字符串
    error_t parsing_language_string(const std::string& strFieldValue, single_field_record_t& single_field_record);
    //  解析存储字段名称
    error_t parsing_store_field_string(const std::string& strFieldValue, single_field_record_t& single_field_record);
    //  检查路径path是否有效
    error_t is_path_valid(const QString& path);
    //  检查给定文件是否存在
    error_t is_file_exist(const QString& file_path);
    //  检查给定目录是否存在
    error_t is_dir_exist(const QString& dir_path);
    //  打开一个指定的Shapefile数据集
    GDALDataset* open_shapefile_dataset(const QString& dataset_path);
    //  初始化GDAL
    void initialize_gdal();
    //  检查GDAL数据集中是否存在指定名称的矢量图层
    bool hasLayer(GDALDataset* poDS, const char* layerName, int& index);
    //  检查一个图层中是否存在名为fieldName的字段
    bool checkFieldExists(OGRLayer* layer, const std::string& fieldName);
    //  对单个图层进行翻译操作
    error_t translate_single_layer(OGRLayer* layer, const single_field_record_t& single_field_record);
    //  对特定字段特定语种进行翻译
    error_t translate(OGRLayer* layer, const single_field_record_t& single_field_record, const size_t& index);
    //  执行字符串翻译的具体逻辑
    std::string translate_to_language(const std::string& text, const std::string& dst_lang_code);
    // 用于接收来自 libcurl 的响应数据
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s);
    //  向python翻译服务发送请求
    error_t sendTranslationRequest(
      const std::string& src_lang_code,
      const std::string& dst_lang_code,
      const std::string& text,
      std::string& translated_text);
    //  向python翻译服务发送检测语种请求
    error_t sendDetecteLanguageRequest(const std::string& text, std::string& language_code);
    //  向python翻译服务发送关闭服务请求
    error_t sendCloseServer();
/********************************************Utility Functions********************************************/
#pragma endregion

#pragma region "2、界面元素响应函数"
    //  浏览输入数据在文件系统中的位置并且选择
    void QPushButton_input_data_path_browse();
    //  浏览日志文件在文件系统中的位置并且选择
    void QPushButton_log_browse();
    //  浏览输出数据在文件系统中的位置并且选择（杨小兵-2024-06-24：这个接口已经被去掉了）
    void QPushButton_output_data_path_browse();
    //  加载配置文件
    void QPushBotton_load_config_file();
    //  运行工具
    void Button_Start();
    //  关闭工具
    void Button_Close();

    //  保存UI界面设置
    void saveState();
    //  从数据库中恢复上一次UI界面设置
    void restoreState();
#pragma endregion

//  "工具的私有API接口" end
#pragma endregion

#pragma region "工具的私有数据成员"
//  "工具的私有数据成员" start
  private:
    //  本底数据所在的文件目录（1、字符集编码：UTF-8；2、统一采用'/'而不是'\\'）
    QString m_input_data_path = "";
    //  日志文件所在的文件目录（1、字符集编码：UTF-8；2、统一采用'/'而不是'\\'）
    QString m_log_saving_path = "";
    //  输出数据所在的文件目录（1、字符集编码：UTF-8；2、统一采用'/'而不是'\\'）
    QString m_output_data_path = "";
    //  配置文件所在的文件目录（1、字符集编码：UTF-8；2、统一采用'/'而不是'\\'）（注：目前配置文件路径是硬编码）
    QString m_config_file_path = "";
    //  输入数据属性表中的字符集编码类型：UTF-8、GBK等
    character_set_encoding_type_t m_character_set_encoding_type = character_set_encoding_type::UTF8;
    //  内部配置文件结构体，将配置文件中的内容读取存储到自定义结构体中
    config_file_info_t m_config_file_info = config_file_info();
    //  日志等级（TODO：将会使用spdlog第三方库实现）
    log_level_t m_log_level = log_level::MESSAGE;
    //  数据集(shapefile)
    GDALDataset* poShpDS = nullptr;
    //  ui界面类
    SE_VectorDataTranslationGuiBase* m_SE_VectorDataTranslationGuiBase = nullptr;

//  "工具的私有数据成员" end
#pragma endregion

};
#pragma endregion

#endif // VECTOR_DATA_TRANSLATION_TOOL_H
