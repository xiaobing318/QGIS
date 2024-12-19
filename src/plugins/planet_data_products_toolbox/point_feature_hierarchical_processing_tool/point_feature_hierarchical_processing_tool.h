#ifndef POINT_FEATURE_HIERARCHICAL_PROCESSING_TOOL_H
#define POINT_FEATURE_HIERARCHICAL_PROCESSING_TOOL_H

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
#include "ui_point_feature_hierarchical_processing_tool.h"
/*------------------UI_header-------------------*/

#pragma endregion

#pragma region "工具将会使用到的自定义结构体"
//  PFHPT = point_feature_hierarchical_processing_tool
namespace PFHPT
{
typedef long long GIntBig;

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
  CURL_EASY_PERFORM_FAILED = 34,
  CONFIG_FILE_IS_EMPTY = 35,
  CONFIG_FILE_GRID_LEVEL_IS_NOT_VALID = 36,
  CONFIG_FILE_GRID_SIZE_IS_NOT_VALID = 37,
  WRAPPER4PROJECT2WEB_MERCATOR_IS_FAILED = 38,
  WRAPPER4PROJECT2WEB_MERCATOR_IS_SUCCESS = 39,
  WRAPPER4CREATE_GRID_DATA_IS_FAILED = 40,
  WRAPPER4CREATE_GRID_DATA_IS_SUCCESS = 41,
  WRAPPER4ASSIGN_LEVEL_VALUE_BY_GRID_IS_FAILED = 42,
  WRAPPER4ASSIGN_LEVEL_VALUE_BY_GRID_IS_SUCCESSS = 43
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

typedef struct config_file_info
{
/*示例
GateLevel					18
Level级别个数				5
9	10	11	12	13
48000	24000	12000	6000	3000
*/

  //  级别数量
  int size;
  //  不同级别列表
  std::vector<int> vLevelList;
  //  不同级别对应的格网尺寸列表
  std::vector<double> vGridWidth;
  config_file_info()
  {
    size = 0;
    vLevelList.clear();
    vGridWidth.clear();
  }
}config_file_info_t;
#pragma endregion

}
#pragma endregion

#pragma region "point_feature_hierarchical_processing工具类"
class point_feature_hierarchical_processing : public QDialog, private Ui::SE_PointFeatureHierarchicalGuiBase
{
    Q_OBJECT

#pragma region "工具的公开API接口"
  public:
    point_feature_hierarchical_processing( QWidget *parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags() );
    ~point_feature_hierarchical_processing() override;
    std::string erro_t2string(const PFHPT::error_t& erro_type);
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
    QString get_m_output_data_path();
    PFHPT::character_set_encoding_type_t get_m_character_set_encoding_type();
    PFHPT::log_level_t get_m_log_level();
    
    void set_m_input_data_path(const QString&input_data_path);
    void set_m_log_saving_path(const QString& log_saving_path);
    void set_m_config_file_path(const QString& config_file_path);
    void set_m_output_data_path(const QString& output_data_path);
    void set_m_character_set_encoding_type(const PFHPT::character_set_encoding_type_t& character_set_encoding_type);
    void set_m_log_level(const PFHPT::log_level_t& log_level);
/********************************************set/get********************************************/

/********************************************Utility Functions********************************************/
    //  解析配置文件
    PFHPT::error_t parse_config_file(const QString& config_file_path);
    //  检查读取得到的配置文件信息是否有效
    PFHPT::error_t is_config_file_info_t_valid();
    //  检查路径path是否有效
    PFHPT::error_t is_path_valid(const QString& path);
    //  检查给定文件是否存在
    PFHPT::error_t is_file_exist(const QString& file_path);
    //  检查给定目录是否存在
    PFHPT::error_t is_dir_exist(const QString& dir_path);
    //  打开一个指定的Shapefile数据集
    GDALDataset* open_shapefile_dataset(const QString& dataset_path);
    //  初始化GDAL并设置相关选项
    void initialize_gdal();
    //  检查一个图层中是否存在名为fieldName的字段
    bool checkFieldExists(OGRLayer* layer, const std::string& fieldName);
/********************************************Utility Functions********************************************/


/********************************************实现具体逻辑的函数********************************************/
    //  第一步：对输入数据进行Web墨卡托投影
    PFHPT::error_t Wrapper4ProjectToWebMercator(
      std::string strInputDataPath, 
      std::string strOutputDataPath);

    //  第二步：根据输入数据生成格网
    PFHPT::error_t Wrapper4CreateGridData(
      std::string strInputDataPath,
      std::vector<int> vLevelList,
      std::vector<double> vGridWidth, 
      std::string strOutputDataPath);

    //  第三步：对输入的矢量数据按照格网赋值
    PFHPT::error_t Wrapper4AssignLevelValueByGrid(
      std::string strInputDataPath, 
      std::string strOutputDataPath, 
      std::vector<int> vLevelList, 
      std::vector<double> vGridWidth);

/********************************************实现具体逻辑的函数********************************************/
#pragma endregion

#pragma region "2、界面元素响应函数"
    //  浏览输入数据在文件系统中的位置并且选择
    void QPushButton_input_data_path_browse();
    //  浏览日志文件在文件系统中的位置并且选择
    void QPushButton_log_browse();
    //  浏览配置文件在文件系统中的位置并且选择
    void QPushButton_config_file_path_browse();
    //  浏览输出数据在文件系统中的位置并且选择
    void QPushButton_output_data_path_browse();
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
    //  配置文件所在的文件目录（1、字符集编码：UTF-8；2、统一采用'/'而不是'\\'）
    QString m_config_file_path = "";

    //  输入数据属性表中的字符集编码类型：UTF-8、GBK等
    PFHPT::character_set_encoding_type_t m_character_set_encoding_type = PFHPT::character_set_encoding_type::UTF8;
    //  内部配置文件结构体，将配置文件中的内容读取存储到自定义结构体中
    PFHPT::config_file_info_t m_config_file_info = PFHPT::config_file_info();
    //  日志等级（TODO：将会使用spdlog第三方库实现）
    PFHPT::log_level_t m_log_level = PFHPT::log_level::MESSAGE;
    //  数据集(shapefile)
    GDALDataset* poShpDS = nullptr;
    //  ui界面类
    SE_PointFeatureHierarchicalGuiBase* m_SE_PointFeatureHierarchicalGuiBase = nullptr;

//  "工具的私有数据成员" end
#pragma endregion

};
#pragma endregion

#endif // POINT_FEATURE_HIERARCHICAL_PROCESSING_TOOL_H
