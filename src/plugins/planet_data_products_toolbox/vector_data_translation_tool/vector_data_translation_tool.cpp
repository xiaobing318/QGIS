#pragma region "包含头文件（减少重复）"
/*-------------矢量数据翻译工具相关头文件-------------*/
#include "vector_data_translation_tool.h"
/*-------------矢量数据翻译工具相关头文件-------------*/

/*------------------QGIS相关的头文件-------------------*/
#include "qgshelp.h"
#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsgui.h"
/*------------------QGIS相关的头文件-------------------*/

/*-----------------------QT-----------------------*/
#include <QComboBox>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>

/*-----------------------QT-----------------------*/

/*-----------------------GDAL-----------------------*/
#include "gdal_priv.h"
#include <ogrsf_frmts.h>
#include <ogr_feature.h>
#include "ogr_geometry.h"
/*-----------------------GDAL-----------------------*/


/*-----------------------STL-----------------------*/
#include <filesystem>
#include <fstream> // 包含对文件操作的库
/*-----------------------STL-----------------------*/

/*----------------第三方库--------------*/
//	创建基本的日志文件，这个日志文件用来接收一般的信息
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

//  用来实现网络链接
#include <curl/curl.h>
//  解析JSON数据结构的库
#include <nlohmann/json.hpp>
/*----------------第三方库--------------*/

#pragma endregion

#pragma region "工具的公开API接口"

vector_data_translation::vector_data_translation(QWidget *parent, Qt::WindowFlags fl)
  :QDialog( parent, fl )
{
  //  给UI界面分配内存空间
  this->m_SE_VectorDataTranslationGuiBase = new SE_VectorDataTranslationGuiBase();
  //  用于初始化和布置由Qt Designer创建的用户界面组件
  this->m_SE_VectorDataTranslationGuiBase->setupUi(this);
  //  用于自动保存和恢复窗口的几何布局
  QgsGui::enableAutoGeometryRestore(this);
  //  用于恢复上一次使用保存的文件路径等状态
  restoreState();

  //  将UI界面元素同响应函数连接
  connect(
    m_SE_VectorDataTranslationGuiBase->QPushButton_input_data_path_browse, &QPushButton::clicked,
    this, &vector_data_translation::QPushButton_input_data_path_browse);
  //  将UI界面元素同响应函数连接
  connect(
    m_SE_VectorDataTranslationGuiBase->QPushBotton_load_config_file, &QPushButton::clicked,
    this, &vector_data_translation::QPushBotton_load_config_file);
  //  将UI界面元素同响应函数连接
  connect(
    m_SE_VectorDataTranslationGuiBase->QPushButton_log_browse, &QPushButton::clicked,
    this, &vector_data_translation::QPushButton_log_browse);
  //  将UI界面元素同响应函数连接
  connect(
    m_SE_VectorDataTranslationGuiBase->Button_Start, &QPushButton::clicked,
    this, &vector_data_translation::Button_Start);
  connect(
    m_SE_VectorDataTranslationGuiBase->Button_Close, &QPushButton::clicked,
    this, &vector_data_translation::Button_Close);

  m_SE_VectorDataTranslationGuiBase->QLineEdit_input_data_path->setText(m_input_data_path);
  m_SE_VectorDataTranslationGuiBase->QLineEdit_log_saving_path->setText(m_log_saving_path);
  
}

vector_data_translation::~vector_data_translation()
{
  //  TODO:释放内存
  if (this->m_SE_VectorDataTranslationGuiBase)
  {
    delete this->m_SE_VectorDataTranslationGuiBase;
  }

  if (this->poShpDS)
  {
    GDALClose(this->poShpDS);
  }

  saveState();
}

std::string vector_data_translation::erro_t2string(const error_t& erro_type)
{
  switch (erro_type)
  {
  case error_t::NO_ERRO:
  {
    return std::string("没有错误");
  }
  case error_t::FILE_IS_NOT_EXIST:
  {
    return std::string("文件不存在");
  }
  //  TODO:补全其他剩余的错误类型
  default:
  {
    return std::string("未知错误类型");
  }

  } //  switch end
}

#pragma endregion

#pragma region "工具的私有API接口"
//  "工具的私有API接口" start
#pragma region "1、内部逻辑函数"
/********************************************set/get********************************************/
QString vector_data_translation::get_m_input_data_path()
{
  return this->m_input_data_path;
}

QString vector_data_translation::get_m_log_saving_path()
{
  return this->m_log_saving_path;
}

QString vector_data_translation::get_m_config_file_path()
{
  return this->m_config_file_path;
}

character_set_encoding_type_t vector_data_translation::get_m_character_set_encoding_type()
{
  QComboBox* comboBox = m_SE_VectorDataTranslationGuiBase->QComboxBox_character_set;
  QString text = comboBox->currentText();
  if (text == "UTF-8")
  {
    set_m_character_set_encoding_type(character_set_encoding_type_t::UTF8);
    return character_set_encoding_type_t::UTF8;
  }
  else if (text == "GB2312")
  {
    set_m_character_set_encoding_type(character_set_encoding_type_t::GB2312);
    return character_set_encoding_type_t::GB2312;
  }
  else if (text == "GBK")
  {
    set_m_character_set_encoding_type(character_set_encoding_type_t::GBK);
    return character_set_encoding_type_t::GBK;
  }
  else
  {
    set_m_character_set_encoding_type(character_set_encoding_type_t::UNKNOW);
    return character_set_encoding_type_t::UNKNOW;
  }
}

log_level_t vector_data_translation::get_m_log_level()
{
  return this->m_log_level;
}



void vector_data_translation::set_m_input_data_path(const QString& input_data_path)
{
  this->m_input_data_path = input_data_path;
}

void vector_data_translation::set_m_log_saving_path(const QString& log_saving_path)
{
  this->m_log_saving_path = log_saving_path;
}

void vector_data_translation::set_m_config_file_path(const QString& config_file_path)
{
  this->m_config_file_path = config_file_path;
}

void vector_data_translation::set_m_character_set_encoding_type(
  const character_set_encoding_type_t& character_set_encoding_type)
{
  if (character_set_encoding_type == character_set_encoding_type_t::UNKNOW)
  {
    return;
  }
  else
  {
    this->m_character_set_encoding_type = character_set_encoding_type;
  }
}

void vector_data_translation::set_m_log_level(const log_level_t& log_level)
{
  this->m_log_level = log_level;
}
/********************************************set/get********************************************/



/********************************************Utility Functions********************************************/
//  解析配置文件
error_t vector_data_translation::parse_config_file(const QString& config_file_path)
{
  //  将config_file_path中的csv文件信息读取到m_config_file_info中保存起来

  // 利用所有可用驱动器中的矢量驱动器
  const char* pszShpDriverName = "CSV";
  GDALDriver* poShpDriver;
  poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
  if (poShpDriver == NULL)
  {
    return error_t::GET_CSV_DRIVER_FAILED;
  }

  // 打开CSV文件
  std::string csv_path = config_file_path.toUtf8();
  GDALDataset* poDS = (GDALDataset*)GDALOpenEx(csv_path.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
  if (poDS == NULL)
  {
    return error_t::OPEN_GDALDATASET_FAILED;
  }

  // 获取图层
  OGRLayer* poLayer = poDS->GetLayer(0);
  if (poLayer == NULL)
  {
    GDALClose(poDS);
    return error_t::OPEN_FIRST_LAYER_FAILED;
  }

  //  获取图层字段信息
  OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
  // 遍历图层中的所有要素
  OGRFeature* poFeature;
  //  字段定义
  OGRFieldDefn* poFieldDefn;
  //  字段名
  std::string strFieldName;
  //  字段值
  std::string strFieldValue;
  
  //  创建单个字段记录结构体用来存储读取到的单条信息
  single_field_record_t single_field_record;
  //  要素名称在配置信息（自定义结构体）中的索引值
  int index4field_record = -1;


  poLayer->ResetReading();
  while ((poFeature = poLayer->GetNextFeature()) != NULL)
  {
    //  用来读取一个行（单个要素）信息
    for (int i = 0; i < poFeature->GetFieldCount(); i++)
    {
      poFieldDefn = poFDefn->GetFieldDefn(i);
      strFieldName = poFieldDefn->GetNameRef();
      strFieldValue = poFeature->GetFieldAsString(i);
      switch (i)
      {
      case 0: // 翻译字段名称
      {
        bool flag = false;
        index4field_record = is_config_info_exist(strFieldValue, flag);
        if (!flag)
        {
          single_field_record.field_name = strFieldValue;
        }
        //  如果存在的话则不进行处理
        break;
      }
      case 1: // 翻译后语种
      {
        //  说明当前的要素名称并没有出现重复，哪些需要将翻译后语种内容解析出来
        if(index4field_record == -1)
        {
          error_t parsing_language_string_flag = parsing_language_string(strFieldValue, single_field_record);
          //  进行错误处理
          if(parsing_language_string_flag != error_t::NO_ERRO)
          {
            //  TODO:写入到日志文件中
          }
        }
        //  如果存在的话则不进行处理
        break;
      }
      case 2: // 存储字段名称
      {
        if(index4field_record == -1)
        {
          error_t parsing_store_field_string_flag = parsing_store_field_string(strFieldValue, single_field_record);
          //  进行错误处理
          if(parsing_store_field_string_flag != error_t::NO_ERRO)
          {
            //  TODO:写入到日志文件中
          }
        }
        //  如果存在的话则不进行处理
        break;
      }
      } //  switch end
    }

    //  将读取到的单个要素信息保存到配置信息（自定义结构体）中
    (this->m_config_file_info).vfield_records.push_back(single_field_record);

    //  销毁创建的要素
    OGRFeature::DestroyFeature(poFeature);
  }

  // 关闭数据集
  GDALClose(poDS);

  return error_t::NO_ERRO;
}

//  检查读取得到的配置文件信息是否有效
error_t vector_data_translation::is_config_file_info_t_valid()
{
  //  目前先不用对配置文件中内容的有效性进行检查
  //  TODO：实现对配置文件有效性的检查
  return error_t::CONFIG_FILE_INFO_T_IS_VALID;
}

//  检查字段名称是否在配置信息中存在
int vector_data_translation::is_config_info_exist(const std::string& strFieldValue, bool& flag)
{
  int index = -1;
  size_t field_count = this->m_config_file_info.vfield_records.size();
  for(size_t i = 0; i < field_count; i++)
  {
    if((this->m_config_file_info.vfield_records[i]).field_name == strFieldValue)
    {
      index = i;
      flag = true;
      break;
    }
  }
  return index;
}

//  解析语种字符串
error_t vector_data_translation::parsing_language_string(
  const std::string& strFieldValue,
  single_field_record_t& single_field_record)
{
  // 将 string 转换为 stringstream，便于分割
  std::istringstream iss(strFieldValue);  
  std::string item;
  // 使用 getline 循环读取每个通过 '|' 分割的部分
  while (std::getline(iss, item, '|'))
  {
    single_field_record.vlanguage_codes.push_back(item);
  }
  return error_t::NO_ERRO;
}

//  解析存储字段名称
error_t vector_data_translation::parsing_store_field_string(
  const std::string& strFieldValue,
  single_field_record_t& single_field_record)
{
  // 将 string 转换为 stringstream，便于分割
  std::istringstream iss(strFieldValue);  
  std::string item;
  // 使用 getline 循环读取每个通过 '|' 分割的部分
  while (std::getline(iss, item, '|'))
  {
    single_field_record.vstorage_fields.push_back(item);
  }
  return error_t::NO_ERRO;
}

//  检查路径path是否有效
error_t vector_data_translation::is_path_valid(const QString& path)
{
  QFileInfo checkFile(path);
  // 检查路径是否存在并且是一个文件或目录
  if (checkFile.exists() && (checkFile.isFile() || checkFile.isDir()))
  {
    return error_t::PATH_IS_VALID; // 路径有效
  }
  else
  {
    return error_t::PATH_IS_NOT_VALID; // 路径无效
  }
}

//  检查给定文件是否存在
error_t vector_data_translation::is_file_exist(const QString& file_path)
{
  QFileInfo fileInfo(file_path);
  // 检查文件是否存在并且确实是一个文件
  if (fileInfo.exists() && fileInfo.isFile())
  {
    return error_t::FILE_IS_EXIST; // 文件存在
  }
  else
  {
    return error_t::FILE_IS_NOT_EXIST; // 文件不存在
  }
}

//  检查给定目录是否存在
error_t vector_data_translation::is_dir_exist(const QString& dir_path)
{
  QFileInfo dirInfo(dir_path);
  // 检查路径是否存在并且是一个目录
  if (dirInfo.exists() && dirInfo.isDir())
  {
    return error_t::DIR_IS_EXIST; // 目录存在
  }
  else
  {
    return error_t::DIR_IS_NOT_EXIST; // 目录不存在
  }
}

//  打开一个指定的Shapefile数据集
GDALDataset* vector_data_translation::open_shapefile_dataset(const QString& dataset_path)
{
  // 确保在打开数据集前初始化GDAL
  initialize_gdal();  

  std::string str_dataset_path = dataset_path.toStdString();
  const char* pszDrivers[] = { nullptr, nullptr };
  GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(str_dataset_path.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, pszDrivers, NULL));
  if (poDS)
  {
    return poDS;
  }
  else
  {
    return nullptr;
  }
}

//  初始化GDAL并设置相关选项
void vector_data_translation::initialize_gdal()
{
  // 注册所有的驱动
  GDALAllRegister();

  // 设置文件名为UTF-8编码，以支持非ASCII字符的文件名
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");

  // 设置Shapefile的编码为UTF-8，以正确处理多语言文本字段
  CPLSetConfigOption("SHAPE_ENCODING", "UTF-8");
}

/**
 * 检查GDAL数据集中是否存在指定名称的矢量图层。
 * @param poDS 指向GDAL数据集的指针。
 * @param layerName 要检查的图层名称。
 * @return 存在返回true，否则返回false。
 */
bool vector_data_translation::hasLayer(GDALDataset* poDS, const char* layerName, int& index)
{
  index = -1;

  if (poDS == nullptr)
  {
    return false; // 如果数据集指针为空，则直接返回false
  }

  // 遍历数据集中的所有图层
  for (int i = 0; i < poDS->GetLayerCount(); ++i)
  {
    OGRLayer* poLayer = poDS->GetLayer(i);
    if (poLayer != nullptr)
    {
      // 获取图层名称并比较
      if (strcmp(poLayer->GetName(), layerName) == 0)
      {
        index = i;
        return true; // 如果找到一个名称匹配的图层，则返回true
      }
    }
  }
  return false; // 遍历所有图层后，没有找到匹配的名称，返回false
}


////  将匹配信息写入到shapefile中
//error_t vector_data_translation::write_matching_form2shapefile(
//  OGRLayer* background_data_layer,
//  OGRLayer* match_data_layer,
//  const matching_form_t& matching_form)
//{
//#pragma region "1、进行一些必要的检查"
//  if ((!background_data_layer) || (!match_data_layer))
//  {
//    return error_t::OGRLAYER_IS_NULLPTR;
//  }
//#pragma endregion
//
//#pragma region "2、在指定路径下创建shapefile并且写入数据"
//  std::string output_path = (this->m_output_data_path).toStdString();
//  //  获取background_data_layer图层名称
//  std::string background_data_layer_name = background_data_layer->GetName();
//  //  获取match_data_layer图层名称
//  std::string match_data_layer_name = match_data_layer->GetName();
//  //  拼接文件全路径
//  std::string shp_match_form_path = output_path + "/" + "match_form4_" + background_data_layer_name + "_" + match_data_layer_name + ".shp";
//
//  // 利用所有可用驱动器中的矢量驱动器
//  const char* pszShpDriverName = "ESRI Shapefile";
//  GDALDriver* poShpDriver;
//  poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
//  if (poShpDriver == NULL)
//  {
//    return error_t::GET_SHP_DRIVER_FAILED;
//  }
//
//  // 创建Shapefile
//  GDALDataset* poDS;
//  poDS = poShpDriver->Create(shp_match_form_path.c_str(), 0, 0, 0, GDT_Unknown, NULL);
//  if (poDS == NULL)
//  {
//    return error_t::CREATE_SHP_FILE_FAILED;
//  }
//
//  //  判断图层要素几何类型
//  OGRwkbGeometryType geomType = wkbFlatten(background_data_layer->GetGeomType());
//  // 创建图层
//  OGRLayer* poLayer;
//  poLayer = poDS->CreateLayer(match_data_layer_name.c_str(), NULL, geomType, NULL);
//  if (poLayer == NULL)
//  {
//    GDALClose(poDS);
//    return error_t::CREATE_OGRLAYER_FAILED;
//  }
//
//#pragma region "给Shapefile图层添加字段"
//  // 添加"空间匹配度"字段
//  OGRFieldDefn oFieldSpaceMatch("SMD", OFTString);
//  oFieldSpaceMatch.SetWidth(256);
//  if (poLayer->CreateField(&oFieldSpaceMatch) != OGRERR_NONE)
//  {
//    GDALClose(poDS);
//    return error_t::CREATE_FIELD_FAILED;
//  }
//
//  // 添加"属性相似度"字段
//  OGRFieldDefn oFieldAttrSim("Attr_Sim", OFTString);
//  oFieldAttrSim.SetWidth(256);
//  if (poLayer->CreateField(&oFieldAttrSim) != OGRERR_NONE)
//  {
//    GDALClose(poDS);
//    return error_t::CREATE_FIELD_FAILED;
//  }
//
//  // 添加"同名要素确认度"字段
//  OGRFieldDefn oFieldSameEleConf("SameEleConf", OFTString);
//  oFieldSameEleConf.SetWidth(256);
//  if (poLayer->CreateField(&oFieldSameEleConf) != OGRERR_NONE)
//  {
//    GDALClose(poDS);
//    return error_t::CREATE_FIELD_FAILED;
//  }
//
//  // 添加"本底图层要素ID"字段
//  OGRFieldDefn oFieldData1ID("Data1_ID", OFTString);
//  oFieldData1ID.SetWidth(256);
//  if (poLayer->CreateField(&oFieldData1ID) != OGRERR_NONE)
//  {
//    GDALClose(poDS);
//    return error_t::CREATE_FIELD_FAILED;
//  }
//
//  // 添加"匹配图层要素ID"字段
//  OGRFieldDefn oFieldRelFacID("RelFacID", OFTString);
//  oFieldRelFacID.SetWidth(256);
//  if (poLayer->CreateField(&oFieldRelFacID) != OGRERR_NONE)
//  {
//    GDALClose(poDS);
//    return error_t::CREATE_FIELD_FAILED;
//  }
//
//  // 添加"现势性比对"字段
//  OGRFieldDefn oFieldTCStatus("TCStatus", OFTString);
//  oFieldTCStatus.SetWidth(256);
//  if (poLayer->CreateField(&oFieldTCStatus) != OGRERR_NONE)
//  {
//    GDALClose(poDS);
//    return error_t::CREATE_FIELD_FAILED;
//  }
//#pragma endregion
//
//#pragma region "写入数据到Shapefile"
//  OGRFeature* poFeature = nullptr;
//  OGRFeature* bgFeature = nullptr;
//  for (const matching_record_t matching_record : matching_form.vmatching_record)
//  {
//    //  本底数据中要素的几何信息写入到其中
//    bgFeature = background_data_layer->GetFeature(std::stoll(matching_record.Data1_ID));
//    if (bgFeature == nullptr)
//    {
//      //  TODO:将错误信息写入到日志中
//
//      //  继续执行下一个匹配记录
//      continue;
//    }
//    OGRGeometry* bg_feature_geometry = bgFeature->GetGeometryRef();
//    if(bg_feature_geometry == nullptr)
//    {
//      //  TODO:将错误信息写入到日志中
//
//      //  继续执行下一个匹配记录
//      continue;
//    }
//
//
//    //  创建新的矢量要素
//    poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
//    if (poFeature == nullptr)
//    {
//      //  TODO:将错误信息写入到日志中
//
//      //  继续执行下一个匹配记录
//      continue;
//    }
//    poFeature->SetField("SMD", matching_record.SMD.c_str());
//    poFeature->SetField("Attr_Sim", matching_record.Attr_Sim.c_str());
//    poFeature->SetField("SameEleConf", matching_record.SameEleConf.c_str());
//    poFeature->SetField("Data1_ID", matching_record.Data1_ID.c_str());
//    poFeature->SetField("RelFacID", matching_record.RelFacID.c_str());
//    poFeature->SetField("TCStatus", matching_record.TCStatus.c_str());
//    poFeature->SetGeometry(bg_feature_geometry);
//
//
//    if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
//    {
//      OGRFeature::DestroyFeature(poFeature);
//      OGRFeature::DestroyFeature(bgFeature);
//      //  TODO:将错误信息写入到日志中
//
//      //  继续执行下一个匹配记录
//      continue; 
//    }
//
//    OGRFeature::DestroyFeature(poFeature);
//    OGRFeature::DestroyFeature(bgFeature);
//  }
//
//  GDALClose(poDS);
//#pragma endregion
//
//#pragma endregion
//
//  return error_t::NO_ERRO;
//}


bool vector_data_translation::checkFieldExists(OGRLayer* layer, const std::string& fieldName)
{
  if (!layer)
  {
    return error_t::OGRLAYER_IS_NULLPTR;
  }
  // 获取图层的特征定义
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  if (!defn)
  {
    return error_t::OGRFEATUREDEFN_IS_NULLPTR;
  }
  // 检查字段名称是否存在
  int fieldIndex = defn->GetFieldIndex(fieldName.c_str());
  // 如果字段索引不为 -1，则字段存在
  return fieldIndex != -1;  
}

error_t vector_data_translation::translate_single_layer(OGRLayer* layer, const single_field_record_t& single_field_record)
{
#pragma region "1、进行必要检查"
  if (!layer)
  {
    return error_t::OGRLAYER_IS_NULLPTR;
  }
  if(single_field_record.field_name == "")
  {
    return error_t::FIELD_NAME_IS_NULL;
  }
  //  single_field_record其中的vlanguage_codes和vstorage_fields如果不是相等的话，则不能进行正确翻译（这在之前的逻辑中检查，这里不进行检查）
#pragma endregion

#pragma region "2、进行翻译处理"

  for(size_t index_vlanguage_codes = 0; index_vlanguage_codes < single_field_record.vlanguage_codes.size(); index_vlanguage_codes++)
  {
    error_t translate_flag = translate(layer, single_field_record, index_vlanguage_codes);
  }

#pragma endregion

}

error_t vector_data_translation::translate(
  OGRLayer* layer, 
  const single_field_record_t& single_field_record,
  const size_t& index)
{
#pragma region "1、进行必要检查"
  if (!layer)
  {
    return error_t::OGRLAYER_IS_NULLPTR;
  }
#pragma endregion

#pragma region "2、执行翻译"
  //  获取得到翻译后的语言代码、翻译后的字段名称
  const std::string language_code = single_field_record.vlanguage_codes[index];
  const std::string storage_field_name = single_field_record.vstorage_fields[index];
  const std::string awaiting_field_name = single_field_record.field_name;
  //  1、首先识别出来field_name字符串的语种（目前还没有实现）


  //  2、在layer图层中创建一个新的字段，名称为storage_field_name，类型是字符串，长度为256
  //  首先检查是否已经存在了想要创建的字段
  int is_storage_field_exist = layer->GetLayerDefn()->GetFieldIndex(storage_field_name.c_str());
  if (is_storage_field_exist != -1)
  {
    //  说明找到了，那么不需要创建新的字段，只需要进行赋值就可以了，这里什么都不需要操作

  }
  else
  {
    //  说明没有找到，那么需要创建新的字段，并且进行赋值
    OGRFieldDefn oField(storage_field_name.c_str(), OFTString);
    oField.SetWidth(256);
    if (layer->CreateField(&oField) != OGRERR_NONE)
    {
      return error_t::CREATE_FIELD_FAILED;
    }
  }

  // 3、在layer图层中找到名为awaiting_field_name的字段，获取其字段值
  int fieldIndex = layer->GetLayerDefn()->GetFieldIndex(awaiting_field_name.c_str());
  if (fieldIndex == -1)
  {
    return error_t::AWAITING_FIELD_NOT_FOUND;
  }

  // 4、遍历所有特征进行翻译和存储
  OGRFeature* feature;
  layer->ResetReading();
  while ((feature = layer->GetNextFeature()) != nullptr)
  {
    const char* srcValue = feature->GetFieldAsString(fieldIndex);
    // 假设有一个translate函数转换字符串到目标语言
    std::string translatedValue = translate_to_language(srcValue, language_code);
    feature->SetField(storage_field_name.c_str(), translatedValue.c_str());
    layer->SetFeature(feature);
    OGRFeature::DestroyFeature(feature);
  }
#pragma endregion

  return error_t::NO_ERRO;
}

std::string vector_data_translation::translate_to_language(const std::string& text, const std::string& dst_lang_code)
{
  if (text == "")
  {
    return std::string("");
  }
  // 这里是实际的翻译逻辑或API调用
  std::string src_lang_code = "";
  //  判断text字符串的语种
  error_t sendDetecteLanguageRequest_flag = sendDetecteLanguageRequest(text, src_lang_code);
  if (sendDetecteLanguageRequest_flag != error_t::NO_ERRO)
  {
    //  TODO:将检测失败的信息写入到日志文件中，目前只是简单的返回特定字符串
    return std::string("语种检测出现了问题");
  }
  if (src_lang_code == "")
  {
    return std::string("未识别的语种");
  }

  std::string translated_text = "";
  error_t sendTranslationRequest_flag = sendTranslationRequest(src_lang_code, dst_lang_code, text, translated_text);
  if (sendTranslationRequest_flag != error_t::NO_ERRO)
  {
    //  TODO:字符串翻译出现问题
    return std::string("翻译出现了问题");
  }
  return translated_text;
}

// 用于接收来自 libcurl 的响应数据
size_t vector_data_translation::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
  size_t newLength = size * nmemb;
  try
  {
    s->append((char*)contents, newLength);
  }
  catch (std::bad_alloc& e)
  {
    // 处理内存不足错误
    return 0;
  }
  return newLength;
}

//  向python翻译服务发送翻译请求
error_t vector_data_translation::sendTranslationRequest(
  const std::string& src_lang_code,
  const std::string& dst_lang_code,
  const std::string& text,
  std::string& translated_text)
{
  CURL* curl = curl_easy_init();
  if (curl)
  {
    //                  {"text": "مرحبا", "src": "ar", "dest": "en"}
    //std::string data = "{\"text\": \"" + text + "\", \"src\": \"ar\", \"dest\": \"en\"}";
    std::string data = "{\"text\": \"" + text + "\", \"src\": \"" + src_lang_code + "\", \"dest\": \"" + dst_lang_code + "\"}";
    std::string response_string;

    // 初始化结构体指针
    struct curl_slist* headers = NULL;
    // 设置 Content-Type
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // 添加 HTTP 头
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:5000/translate");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
      //  TODO:如果出现错误，需要写入到日志中
      //std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
      return error_t::CURL_EASY_PERFORM_FAILED;
    }
    else
    {
      // 解析响应 JSON 并输出翻译结果
      nlohmann::json result = nlohmann::json::parse(response_string);
      //std::cout << "Translated text: " << result["translated_text"].get<std::string>() << std::endl;
      //  提取出其中的翻译后的字符串
      translated_text = result["translated_text"].get<std::string>();
    }
    // 清理 header list
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }
  return error_t::NO_ERRO;
}

//  向python翻译服务发送检测语种请求
error_t vector_data_translation::sendDetecteLanguageRequest(const std::string& text, std::string& language_code)
{
  CURL* curl = curl_easy_init();
  if (curl)
  {
    std::string data = "{\"text\": \"" + text + "\"}";
    std::string response_string;

    // 初始化结构体指针
    struct curl_slist* headers = NULL;
    // 设置 Content-Type
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // 添加 HTTP 头
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:5000/detecte_language");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
      //  TODO:如果出现错误，需要写入到日志中
      return error_t::CURL_EASY_PERFORM_FAILED;
    }
    else
    {
      // 解析响应 JSON 并输出翻译结果
      nlohmann::json result = nlohmann::json::parse(response_string);
      //  提取出返回的语种代码
      language_code = result["detected_language"].get<std::string>();
    }
    // 清理 header list
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }
  return error_t::NO_ERRO;
}

//  向python翻译服务发送关闭服务请求
error_t vector_data_translation::sendCloseServer()
{
  CURL* curl = curl_easy_init();
  if (curl)
  {
    std::string response_string;

    // 初始化结构体指针
    struct curl_slist* headers = NULL;
    // 设置 Content-Type
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // 添加 HTTP 头
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:5000/shutdown");
    //curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
      //  TODO:如果出现错误，需要写入到日志中
      return error_t::CURL_EASY_PERFORM_FAILED;
    }
    // 清理 header list
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }
  return error_t::NO_ERRO;
}
/********************************************Utility Functions********************************************/
#pragma endregion

#pragma region "2、界面元素响应函数"

void vector_data_translation::QPushButton_input_data_path_browse()
{
  // 定位到上一次保存的本底数据所在的文件
  QString curPath = this->m_input_data_path;
  QString dlgTile = QObject::tr("选择待翻译的矢量数据所在的文件夹");
  // 使用 QFileDialog 弹出窗口让用户选择目录
  QString selectedDir = QFileDialog::getExistingDirectory(this, dlgTile, curPath, QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    //  将选择输入数据所在的文件目录显示出来
    this->m_SE_VectorDataTranslationGuiBase->QLineEdit_input_data_path  ->setText(selectedDir);
    //  应该将本底数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_input_data_path = QString::fromUtf8(utf8Path.data());
  }

}

void vector_data_translation::QPushButton_log_browse()
{
  // 定位到上一次保存的匹配数据所在的文件
  QString curPath = this->m_log_saving_path;
  // 设置对话框标题
  QString dlgTitle = QObject::tr("保存日志文件");
  // 弹出保存文件的对话框
  QString filter = QObject::tr("Log files (*.log);;All files (*)");
  QString fileName = QFileDialog::getSaveFileName(this, dlgTitle, curPath, filter);

  // 检查用户是否选中了文件
  if (!fileName.isEmpty())
  {
    QFile file(fileName);

    // 打开文件进行写操作（此操作会清空已有内容）
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      file.close();
    }

    // 显示选中的日志文件路径
    this->m_SE_VectorDataTranslationGuiBase->QLineEdit_log_saving_path->setText(fileName);
    // 处理路径，替换路径分隔符，转换编码
    QString processedPath = fileName.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    this->m_log_saving_path = QString::fromUtf8(utf8Path.data());
  }
}

//  杨小兵-2024-06-24：这个接口已经被去掉了
//void vector_data_translation::QPushButton_output_data_path_browse()
//{
//  // 定位到上一次保存的输出数据所在的文件
//  QString curPath = this->m_output_data_path;
//  QString dlgTile = QObject::tr("选择翻译后的输出数据所在的文件夹");
//  // 使用 QFileDialog 弹出窗口让用户选择目录
//  QString selectedDir = QFileDialog::getExistingDirectory(this, dlgTile, curPath, QFileDialog::ShowDirsOnly);
//
//  if (!selectedDir.isEmpty())
//  {
//    //  将选择的输出数据所在的文件目录显示出来
//    this->m_SE_VectorDataTranslationGuiBase->QLineEdit_output_data_path->setText(selectedDir);
//    //  应该将输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
//    QString processedPath = selectedDir.replace("\\", "/");
//    QByteArray utf8Path = processedPath.toUtf8();
//    // 将处理后的路径保存到内部数据结构中
//    this->m_output_data_path = QString::fromUtf8(utf8Path.data());
//  }
//}

void vector_data_translation::QPushBotton_load_config_file()
{
#pragma region "1、构建配置文件所在的路径、检查配置文件是否存在"
  //	构建配置文件所在的路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString StrFileName = curPath + "/planet_data_products_toolbox_config_files/vector_data_translation_tool_config_file.csv";

  //  windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8
  QString processedPath = StrFileName.replace("\\", "/");
  QByteArray utf8Path = processedPath.toUtf8();
  QString Utf8StrFileName = QString::fromUtf8(utf8Path.data());

  //  检查配置文件是否存在
  error_t is_file_exist_flag = is_file_exist(Utf8StrFileName);
  if (is_file_exist_flag != error_t::FILE_IS_EXIST)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据翻译工具"));
    msgBox.setText(tr("配置文件读取失败（原因：配置文件不存在，请在)") + curPath +
      tr("/planet_data_products_toolbox_config_files") +
      tr("路径下创建vector_data_translation_tool_config_file.csv配置文件）！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion

#pragma region "2、如果配置文件存在，则读取配置文件中的内容"

  error_t parse_config_file_flag = parse_config_file(Utf8StrFileName);

  if (parse_config_file_flag != error_t::NO_ERRO)
  {
    //  读取配置文件失败！
    //  TODO：将错误码和字符串之间建立联系
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据翻译工具"));
    msgBox.setText(tr("读取配置文件失败！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  else
  {
    //  读取配置文件成功！
    //  TODO：将错误码和字符串之间建立联系
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据翻译工具"));
    msgBox.setText(tr("读取配置文件成功！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    //  需要提示读取配置文件成功
  }
#pragma endregion

#pragma region "3、检查读取到配置文件中的内容是否有效"
  error_t is_config_file_info_t_valid_flag = is_config_file_info_t_valid();
  if (is_config_file_info_t_valid_flag != error_t::CONFIG_FILE_INFO_T_IS_VALID)
  {
    //  “匹配字段选择和字段权重设置”配置文件内容无效！
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据翻译工具"));
    msgBox.setText(tr("配置文件内容无效！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  else
  {
    //  配置文件内容有效的时候不需要进行提示
  }
#pragma endregion

}

void vector_data_translation::Button_Start()
{

#pragma region "1、配置文件是否加载提示信息"
  if (this->m_config_file_info.vfield_records.size() == 0)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据翻译工具"));
    msgBox.setText(tr("是否已经加载了配置文件？如果没有加载配置文件，请先加载配置文件再进行操作"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    //  只是作为一个提示信息
    msgBox.exec();
  }
#pragma endregion

#pragma region "2、读取参数信息并且检查其有效性"
  //  读取输入数据字符编码
  character_set_encoding_type_t character_set_encoding_type = get_m_character_set_encoding_type();
  if (character_set_encoding_type == character_set_encoding_type_t::UNKNOW)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据翻译工具"));
    msgBox.setText(tr("未选择输入数据字符集编码！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

  //  读取到的配置文件信息是否为空
  if ((this->m_config_file_info).vfield_records.size() == 0)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据翻译工具"));
    msgBox.setText(tr("配置文件为空（可能的原因：1、配置文件为空；2、没有加载配置文件）"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

  //  检查输入数据集（Shapefile）是否已经被正常打开了，如果没有则需要打开并且将数据集指针保存下来
  if(!(this->poShpDS))
  {
    this->poShpDS = open_shapefile_dataset(get_m_input_data_path());
    if (!(this->poShpDS))
    {
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("矢量数据翻译工具"));
      msgBox.setText(tr("待翻译的输入数据打开失败（可能的原因：1、待翻译的输入数据为空；2、待翻译的输入数据损坏；3、待翻译的输入数据文件不全）"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
    }

  }
#pragma endregion

#pragma region "3、对图层对进行翻译处理"
  //  用来统计哪些图层被处理了
  int has_processed_layers = 0;
  //  获取输入数据集中一共有多少图层
  int layer_count = this->poShpDS->GetLayerCount();
  //  获取m_config_file_info中有多个条记录
  size_t field_records_count = this->m_config_file_info.vfield_records.size();
  for (size_t index_layer = 0; index_layer < layer_count; index_layer++)
  {
#pragma region "对当前图层特定的字段逐个进行翻译"
    for(size_t index_field = 0; index_field < field_records_count; index_field++)
    {
      const single_field_record_t& current_field_record = this->m_config_file_info.vfield_records[index_field];
      //  获取当前配置文件中字段记录（字段名称）是否在当前图层中存在
      bool checkFieldExistsFlag = checkFieldExists(this->poShpDS->GetLayer(index_layer), current_field_record.field_name);
      if(!checkFieldExistsFlag)
      {
        //  如果在当前图层中不存在特定字段，那么处理下一个字段
        continue;
      }
      //  如果在当前图层中存在特定字段,则进行翻译处理
      error_t translate_single_layer_flag = translate_single_layer(this->poShpDS->GetLayer(index_layer), current_field_record);
      if(translate_single_layer_flag != error_t::NO_ERRO)
      {
        //  TODO:将错误信息写入到日志当中
      }
      has_processed_layers++;
    }
    //  获取到第一个字段名称

#pragma endregion
  }


  if (has_processed_layers != layer_count)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据翻译工具"));
    msgBox.setText(tr("正确处理了") + QString::number(has_processed_layers) + tr("图层，") +
      tr("还有") + QString::number(layer_count - has_processed_layers) + tr("图层对没有处理。") +
      tr("（可能的原因：1、图层数据无效；2、翻译工具出现错误；）"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  else
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据翻译工具"));
    msgBox.setText(tr("矢量数据翻译处理完成！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion

}

void vector_data_translation::Button_Close()
{
  saveState();
  reject();
}

void vector_data_translation::saveState()
{
  QgsSettings settings;
  // 指定设置保存到Plugins节
  settings.beginGroup("vector_data_translation_tool");
  settings.setValue(QStringLiteral("vector_data_translation_tool/m_input_data_path"), m_input_data_path);
  settings.setValue(QStringLiteral("vector_data_translation_tool/m_log_saving_path"), m_log_saving_path);
  // 结束分组，非常重要，确保设置正确保存在指定的节中
  settings.endGroup();
}

void vector_data_translation::restoreState()
{
  QgsSettings settings;
  // 指定从Plugins节读取设置
  settings.beginGroup("vector_data_translation_tool");
  m_input_data_path = settings.value(QStringLiteral("vector_data_translation_tool/m_input_data_path"), QDir::homePath()).toString();
  m_log_saving_path = settings.value(QStringLiteral("vector_data_translation_tool/m_log_saving_path"), QDir::homePath()).toString();
  // 结束分组
  settings.endGroup();
}


#pragma endregion
//  "工具的私有API接口" end
#pragma endregion
