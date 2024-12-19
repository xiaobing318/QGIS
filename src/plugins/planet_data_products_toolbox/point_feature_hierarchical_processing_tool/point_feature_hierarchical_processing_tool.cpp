#pragma region "包含头文件（减少重复）"
/*-------------点要素分级处理工具相关头文件-------------*/
#include "point_feature_hierarchical_processing_tool.h"
/*-------------点要素分级处理工具相关头文件-------------*/

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

/*------------------------SE------------------------*/
#include "project/cse_geotransformation.h"
#include "naviinfo/cse_poi_process.h"
#include "commontype/se_commontypedef.h"
#include "cse_dataprocessimp.h"
/*------------------------SE------------------------*/


/*-----------------------STL-----------------------*/
#include <filesystem>
#include <fstream> // 包含对文件操作的库
/*-----------------------STL-----------------------*/

/*----------------第三方库--------------*/
//	创建基本的日志文件，这个日志文件用来接收一般的信息
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
/*----------------第三方库--------------*/

#pragma endregion

#pragma region "工具的公开API接口"

point_feature_hierarchical_processing::point_feature_hierarchical_processing(QWidget *parent, Qt::WindowFlags fl)
  :QDialog( parent, fl )
{
  //  给UI界面分配内存空间
  this->m_SE_PointFeatureHierarchicalGuiBase = new SE_PointFeatureHierarchicalGuiBase();
  //  用于初始化和布置由Qt Designer创建的用户界面组件
  this->m_SE_PointFeatureHierarchicalGuiBase->setupUi(this);
  //  用于自动保存和恢复窗口的几何布局
  QgsGui::enableAutoGeometryRestore(this);
  //  用于恢复上一次使用保存的文件路径等状态
  restoreState();
  //  将恢复的UI设置显示出来
  m_SE_PointFeatureHierarchicalGuiBase->QLineEdit_input_data_path->setText(m_input_data_path);
  m_SE_PointFeatureHierarchicalGuiBase->QLineEdit_log_saving_path->setText(m_log_saving_path);
  m_SE_PointFeatureHierarchicalGuiBase->QLineEdit_output_data_path->setText(m_output_data_path);
  m_SE_PointFeatureHierarchicalGuiBase->QLineEdit_config_file_path->setText(m_config_file_path);


  //  将UI界面元素同响应函数连接(打开输入数据浏览)
  connect(
    m_SE_PointFeatureHierarchicalGuiBase->QPushButton_input_data_path_browse, &QPushButton::clicked,
    this, &point_feature_hierarchical_processing::QPushButton_input_data_path_browse);
  //  将UI界面元素同响应函数连接(打开配置文件浏览)
  connect(m_SE_PointFeatureHierarchicalGuiBase->QPushButton_config_file_path_browse, &QPushButton::clicked,
  this, &point_feature_hierarchical_processing::QPushButton_config_file_path_browse);
  //  将UI界面元素同响应函数连接(打开日志文件浏览)
  connect(
    m_SE_PointFeatureHierarchicalGuiBase->QPushButton_log_browse, &QPushButton::clicked,
    this, &point_feature_hierarchical_processing::QPushButton_log_browse);
  //  将UI界面元素同响应函数连接(打开输出数据浏览)
  connect(
    m_SE_PointFeatureHierarchicalGuiBase->QPushButton_output_data_path_browse, &QPushButton::clicked,
    this, &point_feature_hierarchical_processing::QPushButton_output_data_path_browse);
  //  将UI界面元素同响应函数连接(开始运行工具)
  connect(
    m_SE_PointFeatureHierarchicalGuiBase->Button_Start, &QPushButton::clicked,
    this, &point_feature_hierarchical_processing::Button_Start);
  //  将UI界面元素同响应函数连接(关闭工具)
  connect(
    m_SE_PointFeatureHierarchicalGuiBase->Button_Close, &QPushButton::clicked,
    this, &point_feature_hierarchical_processing::Button_Close);  
}

point_feature_hierarchical_processing::~point_feature_hierarchical_processing()
{
  //  释放界面所占有的内存
  if (this->m_SE_PointFeatureHierarchicalGuiBase)
  {
    delete this->m_SE_PointFeatureHierarchicalGuiBase;
  }
  //  释放工具使用数据所占有的内存
  if (this->poShpDS)
  {
    GDALClose(this->poShpDS);
  }

  saveState();
}

std::string point_feature_hierarchical_processing::erro_t2string(const PFHPT::error_t& erro_type)
{
  switch (erro_type)
  {
  case PFHPT::error_t::NO_ERRO:
  {
    return std::string("没有错误");
  }
  case PFHPT::error_t::FILE_IS_NOT_EXIST:
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
QString point_feature_hierarchical_processing::get_m_input_data_path()
{
  return this->m_input_data_path;
}

QString point_feature_hierarchical_processing::get_m_log_saving_path()
{
  return this->m_log_saving_path;
}

QString point_feature_hierarchical_processing::get_m_config_file_path()
{
  return this->m_config_file_path;
}

QString point_feature_hierarchical_processing::get_m_output_data_path()
{
  return this->m_output_data_path;
}

PFHPT::character_set_encoding_type_t point_feature_hierarchical_processing::get_m_character_set_encoding_type()
{
  QComboBox* comboBox = m_SE_PointFeatureHierarchicalGuiBase->QComboxBox_character_set;
  QString text = comboBox->currentText();
  if (text == "UTF-8")
  {
    set_m_character_set_encoding_type(PFHPT::character_set_encoding_type_t::UTF8);
    return PFHPT::character_set_encoding_type_t::UTF8;
  }
  else if (text == "GB2312")
  {
    set_m_character_set_encoding_type(PFHPT::character_set_encoding_type_t::GB2312);
    return PFHPT::character_set_encoding_type_t::GB2312;
  }
  else if (text == "GBK")
  {
    set_m_character_set_encoding_type(PFHPT::character_set_encoding_type_t::GBK);
    return PFHPT::character_set_encoding_type_t::GBK;
  }
  else
  {
    set_m_character_set_encoding_type(PFHPT::character_set_encoding_type_t::UNKNOW);
    return PFHPT::character_set_encoding_type_t::UNKNOW;
  }
}

PFHPT::log_level_t point_feature_hierarchical_processing::get_m_log_level()
{
  return this->m_log_level;
}



void point_feature_hierarchical_processing::set_m_input_data_path(const QString& input_data_path)
{
  this->m_input_data_path = input_data_path;
}

void point_feature_hierarchical_processing::set_m_log_saving_path(const QString& log_saving_path)
{
  this->m_log_saving_path = log_saving_path;
}

void point_feature_hierarchical_processing::set_m_config_file_path(const QString& config_file_path)
{
  this->m_config_file_path = config_file_path;
}

void point_feature_hierarchical_processing::set_m_output_data_path(const QString& output_data_path)
{
  this->m_output_data_path = output_data_path;
}

void point_feature_hierarchical_processing::set_m_character_set_encoding_type(
  const PFHPT::character_set_encoding_type_t& character_set_encoding_type)
{
  if (character_set_encoding_type == PFHPT::character_set_encoding_type_t::UNKNOW)
  {
    return;
  }
  else
  {
    this->m_character_set_encoding_type = character_set_encoding_type;
  }
}

void point_feature_hierarchical_processing::set_m_log_level(const PFHPT::log_level_t& log_level)
{
  this->m_log_level = log_level;
}
/********************************************set/get********************************************/



/********************************************Utility Functions********************************************/
//  解析配置文件
PFHPT::error_t point_feature_hierarchical_processing::parse_config_file(const QString& config_file_path)
{
  //  将config_file_path中的csv文件信息读取到m_config_file_info中保存起来

  // 利用所有可用驱动器中的矢量驱动器
  const char* pszShpDriverName = "CSV";
  GDALDriver* poShpDriver;
  poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
  if (poShpDriver == NULL)
  {
    return PFHPT::error_t::GET_CSV_DRIVER_FAILED;
  }

  // 打开CSV文件
  std::string csv_path = config_file_path.toUtf8();
  GDALDataset* poDS = (GDALDataset*)GDALOpenEx(csv_path.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
  if (poDS == NULL)
  {
    return PFHPT::error_t::OPEN_GDALDATASET_FAILED;
  }

  // 获取图层
  OGRLayer* poLayer = poDS->GetLayer(0);
  if (poLayer == NULL)
  {
    GDALClose(poDS);
    return PFHPT::error_t::OPEN_FIRST_LAYER_FAILED;
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
  
  //  重置要素索引
  poLayer->ResetReading();
  while ((poFeature = poLayer->GetNextFeature()) != NULL)
  {
    //  用来读取一个行（单个要素所有的字段相关的信息）信息
    for (int i = 0; i < poFeature->GetFieldCount(); i++)
    {
      poFieldDefn = poFDefn->GetFieldDefn(i);
      strFieldName = poFieldDefn->GetNameRef();
      strFieldValue = poFeature->GetFieldAsString(i);
      //  不同的字段需要将其属性值保存在对应不同的自定义数据结构中
      switch (i)
      {
      case 0: // 格网等级
      {
        int iFieldValue = std::stoi(strFieldValue);
        (this->m_config_file_info).vLevelList.push_back(iFieldValue);
        break;
      }
      case 1: // 格网尺寸
      {
        double dFieldValue = std::stod(strFieldValue);
        (this->m_config_file_info).vGridWidth.push_back(dFieldValue);
        break;
      }

      } //  switch end
    }

    //  将pair（格网等级<----->格网尺寸）数量更新
    (this->m_config_file_info).size += 1;

    //  销毁创建的要素
    OGRFeature::DestroyFeature(poFeature);
  }

  // 关闭数据集
  GDALClose(poDS);

  return PFHPT::error_t::NO_ERRO;
}

//  检查读取得到的配置文件信息是否有效
PFHPT::error_t point_feature_hierarchical_processing::is_config_file_info_t_valid()
{
#pragma region "1、判断配置文件中的内容是否为空"
  if((this->m_config_file_info).size == 0)
  {
    return PFHPT::error_t::CONFIG_FILE_IS_EMPTY;
  }
#pragma endregion

#pragma region "2、判断配置文件中的格网等级是否在有效范围"
  bool is_grid_level_valid = true;
  for(int i = 0; i < (this->m_config_file_info).size; i++)
  {
    int value = (this->m_config_file_info).vLevelList[i];
    if(!((0 <= value) && (value <= 18)))
    {
      is_grid_level_valid = false;
      break;
    }
  }
  if(!is_grid_level_valid)
  {
    return PFHPT::error_t::CONFIG_FILE_GRID_LEVEL_IS_NOT_VALID;
  }
#pragma endregion

#pragma region "3、判断配置文件中的格网尺寸是否在有效范围"
  bool is_grid_size_valid = true;
  for(int i = 0; i < (this->m_config_file_info).size; i++)
  {
    int value = (this->m_config_file_info).vGridWidth[i];
    if(!(0.0 <= value))
    {
      is_grid_size_valid = false;
      break;
    }
  }
  if(!is_grid_size_valid)
  {
    return PFHPT::error_t::CONFIG_FILE_GRID_SIZE_IS_NOT_VALID;
  }
#pragma endregion

  //  目前先不用对配置文件中内容的有效性进行检查 TODO：实现对配置文件有效性的检查
  return PFHPT::error_t::CONFIG_FILE_INFO_T_IS_VALID;
}

//  检查路径path是否有效
PFHPT::error_t point_feature_hierarchical_processing::is_path_valid(const QString& path)
{
  QFileInfo checkFile(path);
  // 检查路径是否存在并且是一个文件或目录
  if (checkFile.exists() && (checkFile.isFile() || checkFile.isDir()))
  {
    return PFHPT::error_t::PATH_IS_VALID; // 路径有效
  }
  else
  {
    return PFHPT::error_t::PATH_IS_NOT_VALID; // 路径无效
  }
}

//  检查给定文件是否存在
PFHPT::error_t point_feature_hierarchical_processing::is_file_exist(const QString& file_path)
{
  QFileInfo fileInfo(file_path);
  // 检查文件是否存在并且确实是一个文件
  if (fileInfo.exists() && fileInfo.isFile())
  {
    return PFHPT::error_t::FILE_IS_EXIST; // 文件存在
  }
  else
  {
    return PFHPT::error_t::FILE_IS_NOT_EXIST; // 文件不存在
  }
}

//  检查给定目录是否存在
PFHPT::error_t point_feature_hierarchical_processing::is_dir_exist(const QString& dir_path)
{
  QFileInfo dirInfo(dir_path);
  // 检查路径是否存在并且是一个目录
  if (dirInfo.exists() && dirInfo.isDir())
  {
    return PFHPT::error_t::DIR_IS_EXIST; // 目录存在
  }
  else
  {
    return PFHPT::error_t::DIR_IS_NOT_EXIST; // 目录不存在
  }
}

//  打开一个指定的Shapefile数据集
GDALDataset* point_feature_hierarchical_processing::open_shapefile_dataset(const QString& dataset_path)
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
void point_feature_hierarchical_processing::initialize_gdal()
{
  // 注册所有的驱动
  GDALAllRegister();

  // 设置文件名为UTF-8编码，以支持非ASCII字符的文件名
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");

  // 设置Shapefile的编码为UTF-8，以正确处理多语言文本字段
  CPLSetConfigOption("SHAPE_ENCODING", "UTF-8");
}

//  检查一个图层中是否存在名为fieldName的字段
bool point_feature_hierarchical_processing::checkFieldExists(OGRLayer* layer, const std::string& fieldName)
{
  if (!layer)
  {
    return PFHPT::error_t::OGRLAYER_IS_NULLPTR;
  }
  // 获取图层的特征定义
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  if (!defn)
  {
    return PFHPT::error_t::OGRFEATUREDEFN_IS_NULLPTR;
  }
  // 检查字段名称是否存在
  int fieldIndex = defn->GetFieldIndex(fieldName.c_str());
  // 如果字段索引不为 -1，则字段存在
  return fieldIndex != -1;  
}
/********************************************Utility Functions********************************************/


/********************************************实现具体逻辑的函数********************************************/
//  第一步：对输入数据进行Web墨卡托投影
PFHPT::error_t point_feature_hierarchical_processing::Wrapper4ProjectToWebMercator(
  std::string strInputDataPath, 
  std::string strOutputDataPath)
{
  SE_Error seErr = CSE_PoiProcess::ProjectToWebMercator(strInputDataPath.c_str(), strOutputDataPath.c_str());
  if (seErr != SE_ERROR_NONE)
  {
    return PFHPT::error_t::WRAPPER4PROJECT2WEB_MERCATOR_IS_FAILED;
  }
  return PFHPT::error_t::WRAPPER4PROJECT2WEB_MERCATOR_IS_SUCCESS;
}

//  第二步：根据输入数据生成格网
PFHPT::error_t point_feature_hierarchical_processing::Wrapper4CreateGridData(
  std::string strInputDataPath,
  std::vector<int> vLevelList,
  std::vector<double> vGridWidth, 
  std::string strOutputDataPath)
{
  SE_Error seErr = CSE_PoiProcess::CreateGridData(
    strInputDataPath.c_str(),
    this->m_config_file_info.vLevelList,
    this->m_config_file_info.vGridWidth,
    strOutputDataPath.c_str());
  if (seErr != SE_ERROR_NONE)
  {
    return PFHPT::error_t::WRAPPER4CREATE_GRID_DATA_IS_FAILED;
  }
  return PFHPT::error_t::WRAPPER4CREATE_GRID_DATA_IS_SUCCESS;
}

//  第三步：对输入的矢量数据按照格网赋值
PFHPT::error_t point_feature_hierarchical_processing::Wrapper4AssignLevelValueByGrid(
  std::string strInputDataPath, 
  std::string strOutputDataPath, 
  std::vector<int> vLevelList, 
  std::vector<double> vGridWidth)
{
  SE_Error seErr = CSE_PoiProcess::AssignLevelValueByGrid(
      strInputDataPath.c_str(),
      vLevelList,
      vGridWidth,
      strOutputDataPath.c_str());
  if (seErr != SE_ERROR_NONE)
  {
    return PFHPT::error_t::WRAPPER4ASSIGN_LEVEL_VALUE_BY_GRID_IS_FAILED;
  }
  return PFHPT::error_t::WRAPPER4ASSIGN_LEVEL_VALUE_BY_GRID_IS_SUCCESSS;
}


/********************************************实现具体逻辑的函数********************************************/
#pragma endregion

#pragma region "2、界面元素响应函数"
//  浏览输入数据在文件系统中的位置并且选择
void point_feature_hierarchical_processing::QPushButton_input_data_path_browse()
{
  // 定位到上一次保存的输入数据所在的文件
  QString curPath = this->m_input_data_path;
  // 设置对话框标题
  QString dlgTitle = QObject::tr("选择点要素分级处理输入矢量数据");
  // 弹出打开文件的对话框
  QString filter = QObject::tr("Shapefiles (*.shp);;All files (*)");
  QString fileName = QFileDialog::getOpenFileName(this, dlgTitle, curPath, filter);

  // 检查用户是否选中了文件
  if (!fileName.isEmpty())
  {
    // 显示选中的点要素分级处理输入矢量数据路径
    this->m_SE_PointFeatureHierarchicalGuiBase->QLineEdit_input_data_path->setText(fileName);
    // 更新内部路径变量
    this->m_input_data_path = fileName;
  }
}
//  浏览日志文件在文件系统中的位置并且选择
void point_feature_hierarchical_processing::QPushButton_log_browse()
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
    this->m_SE_PointFeatureHierarchicalGuiBase->QLineEdit_log_saving_path->setText(fileName);
    // 处理路径，替换路径分隔符，转换编码
    QString processedPath = fileName.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    this->m_log_saving_path = QString::fromUtf8(utf8Path.data());
  }
}
//  浏览配置文件在文件系统中的位置并且选择
void point_feature_hierarchical_processing::QPushButton_config_file_path_browse()
{
  // 定位到上一次保存的配置文件路径
  QString curPath = this->m_config_file_path;
  // 设置对话框标题
  QString dlgTitle = QObject::tr("选择配置文件(*.csv)文件");
  // 弹出打开文件的对话框
  QString filter = QObject::tr("配置文件 (*.csv);;All files (*)");
  QString fileName = QFileDialog::getOpenFileName(this, dlgTitle, curPath, filter);

  // 检查用户是否选中了文件
  if (!fileName.isEmpty())
  {
    // 显示选中的点要素分级处理输入矢量数据路径
    this->m_SE_PointFeatureHierarchicalGuiBase->QLineEdit_config_file_path->setText(fileName);
    // 更新内部路径变量
    this->m_config_file_path = fileName;
  }
}
//  浏览输出数据在文件系统中的位置并且选择
void point_feature_hierarchical_processing::QPushButton_output_data_path_browse()
{
  // 定位到上一次保存的输出数据所在的文件
  QString curPath = this->m_output_data_path;
  QString dlgTile = QObject::tr("选择点要素分级处理输出矢量数据");
  // 使用 QFileDialog 弹出窗口让用户选择目录
  QString selectedDir = QFileDialog::getExistingDirectory(this, dlgTile, curPath, QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    //  将选择的输出数据所在的文件目录显示出来
    this->m_SE_PointFeatureHierarchicalGuiBase->QLineEdit_output_data_path->setText(selectedDir);
    //  应该将输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_output_data_path = QString::fromUtf8(utf8Path.data());
  }
}

//  这个响应函数不需要，相关的逻辑放在parse_config_file中进行处理
//void point_feature_hierarchical_processing::QPushBotton_load_config_file()
//{
//
//
//}
//  运行工具
void point_feature_hierarchical_processing::Button_Start()
{
#pragma region "（1） 读取参数信息并且检查其有效性"
//  读取参数信息并且检查其有效性start



#pragma region "1、输入矢量数据文件是否存在"
  PFHPT::error_t flag_is_file_exist = is_file_exist(get_m_input_data_path());
  if(flag_is_file_exist != PFHPT::error_t::FILE_IS_EXIST)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("点要素分级处理输入数据文件不存在（可能的原因：1、输入数据路径不存在；2、输入数据为空；3、输入数据文件不全）"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

/*********************************检查输入矢量数据是否存在并且是否可以正常打开********************************/
  //  检查输入数据集（Shapefile）是否已经被正常打开了，如果没有则需要打开并且将数据集指针保存下来
  if(!(this->poShpDS))
  {
    this->poShpDS = open_shapefile_dataset(get_m_input_data_path());

    if (!(this->poShpDS))
    {
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("点要素分级处理工具"));
      msgBox.setText(tr("点要素分级处理输入数据打开失败（可能的原因：1、输入数据为空；2、输入数据损坏；3、输入数据文件不全）"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
    }
    //  如果数据集可以被正常打开，则关闭掉
    GDALClose(this->poShpDS);
    this->poShpDS = nullptr;
  }

#pragma endregion

#pragma region "2、检查配置文件是否存在"
  //	构建配置文件所在的路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString StrFileName = curPath + "/planet_data_products_toolbox_config_files/point_feature_hierarchical_processing_tool_config_file.csv";


  //  检查配置文件是否存在
  PFHPT::error_t is_file_exist_flag = is_file_exist(get_m_config_file_path());
  if (is_file_exist_flag != PFHPT::error_t::FILE_IS_EXIST)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("配置文件读取失败（原因：配置文件不存在，请在)") + curPath +
      tr("/planet_data_products_toolbox_config_files") +
      tr("路径下创建point_feature_hierarchical_processing_tool_config_file.csv配置文件或者在其他位置创建*.csv文件）！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion

#pragma region "3、如果配置文件存在，则读取配置文件中的内容"

  PFHPT::error_t parse_config_file_flag = parse_config_file(get_m_config_file_path());

  if (parse_config_file_flag != PFHPT::error_t::NO_ERRO)
  {
    //  读取配置文件失败！TODO：将错误码和字符串之间建立联系
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("读取配置文件失败！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  //  读取配置文件成功不需要提示
#pragma endregion

#pragma region "4、检查读取到配置文件中的内容是否有效"
  PFHPT::error_t is_config_file_info_t_valid_flag = is_config_file_info_t_valid();
  if (is_config_file_info_t_valid_flag != PFHPT::error_t::CONFIG_FILE_INFO_T_IS_VALID)
  {
    //  “匹配字段选择和字段权重设置”配置文件内容无效！
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("配置文件内容无效（可能的原因：1、配置文件为空；2、配置文件中的格网等级不在有效范围内(0--->18)；3、配置文件中的格网尺寸不在有效范围内(每个格网尺寸数值应该大于0.0))）！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  //  配置文件内容有效的时候不需要进行提示
#pragma endregion

#pragma region "5、读取输入数据字符编码并且检查是否有效"
  //  读取输入数据字符编码
  PFHPT::character_set_encoding_type_t character_set_encoding_type = get_m_character_set_encoding_type();
  if (character_set_encoding_type == PFHPT::character_set_encoding_type_t::UNKNOW)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("未选择输入数据字符集编码！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion

#pragma region "6、输出矢量数据文件目录是否存在"
  PFHPT::error_t flag_is_dir_exist = is_dir_exist(get_m_output_data_path());
  if(flag_is_dir_exist != PFHPT::error_t::DIR_IS_EXIST)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("点要素分级处理输出数据目录不存在（可能的原因：1、输出数据路径不存在；）"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion



//  读取参数信息并且检查其有效性end
#pragma endregion

#pragma region "（2） 对点要素图层进行分级处理"
  //  用来统计哪些图层被处理了
  int has_processed_layers = 0;
  //  获取输入数据集中一共有多少图层
  int layer_count = 1;

/**************************************************************************/
#pragma region "1、构建（第一步：对输入数据进行Web墨卡托投影、第二步：根据输入数据生成格网、第三步：对输入的矢量数据按照格网赋值）三个函数所需的参数"

  //  获取输入图层的名称(两个参数：1、输入单个图层数据路径、输出单个图层数据路径)
  std::string first_step_strInputPath = get_m_input_data_path().toStdString();
  size_t first_step_start_index_name = first_step_strInputPath.find_last_of("/");
  size_t first_step_end_index_name = first_step_strInputPath.find_last_of(".");
  std::string first_step_layer_name = first_step_strInputPath.substr(first_step_start_index_name + 1, first_step_end_index_name - first_step_start_index_name - 1);
  std::string first_step_strOutputPath = get_m_output_data_path().toStdString() + "/" + first_step_layer_name + "_webmercator.shp";

  std::string second_step_strInputPath = first_step_strOutputPath;
  std::vector<int> second_step_vLevelList = (this->m_config_file_info).vLevelList;
  std::vector<double> second_step_vGridWidth = (this->m_config_file_info).vGridWidth;
  size_t second_step_start_index_name = first_step_strOutputPath.find_last_of("/");
  size_t second_step_end_index_name = first_step_strOutputPath.find_last_of(".");
  std::string second_step_layer_name = first_step_strOutputPath.substr(second_step_start_index_name + 1, second_step_end_index_name - second_step_start_index_name - 1);
  std::string second_step_strOutputDataPath = get_m_output_data_path().toStdString() + "/" + second_step_layer_name + "_LevelGrid.shp";

  std::string third_step_strInputFilePath = first_step_strOutputPath;
  size_t third_step_start_index_name = first_step_strOutputPath.find_last_of("/");
  size_t third_step_end_index_name = first_step_strOutputPath.find_last_of(".");
  std::string third_step_layer_name = first_step_strOutputPath.substr(third_step_start_index_name + 1, third_step_end_index_name - third_step_start_index_name - 1);
  std::string third_step_strOutputPath = get_m_output_data_path().toStdString() + "/" + third_step_layer_name + "_grid.shp";
  std::vector<int> third_step_vLevelList = (this->m_config_file_info).vLevelList;
  std::vector<double> third_step_vGridWidth = (this->m_config_file_info).vGridWidth;

#pragma endregion


#pragma region "2、调用函数进行处理"
  //  调用函数进行处理start
 
#pragma region "第一步：对输入数据进行Web墨卡托投影"
  PFHPT::error_t flag4Wrapper4ProjectToWebMercator = Wrapper4ProjectToWebMercator(
    first_step_strInputPath,
    first_step_strOutputPath
  );
  if(flag4Wrapper4ProjectToWebMercator != PFHPT::error_t::WRAPPER4PROJECT2WEB_MERCATOR_IS_SUCCESS)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("对输入数据进行Web墨卡托投影处理失败（可能的原因：1、输入数据不存在；2、输入数据为空；3、输入数据文件不全）"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  //  如果处理成功不需要将结果显示出来
#pragma endregion

#pragma region "第二步：根据输入数据生成格网"
  //  第二步：根据输入数据生成格网
  PFHPT::error_t flag4Wrapper4CreateGridData = Wrapper4CreateGridData(
    second_step_strInputPath,
    second_step_vLevelList,
    second_step_vGridWidth,
    second_step_strOutputDataPath
  );
  if(flag4Wrapper4CreateGridData != PFHPT::error_t::WRAPPER4CREATE_GRID_DATA_IS_SUCCESS)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("对输入数据生成格网处理失败（可能的原因：1、输入数据不存在；2、输入数据为空；3、输入数据文件不全；4、输出目录中已经存在了同名文件）"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  //  如果处理成功不需要将结果显示出来
#pragma endregion

#pragma region "第三步：对输入的矢量数据按照格网赋值"
  //  第三步：对输入的矢量数据按照格网赋值
  PFHPT::error_t flag4Wrapper4AssignLevelValueByGrid = Wrapper4AssignLevelValueByGrid(
    third_step_strInputFilePath,
    third_step_strOutputPath,
    third_step_vLevelList,
    third_step_vGridWidth
  );
  if(flag4Wrapper4AssignLevelValueByGrid != PFHPT::error_t::WRAPPER4ASSIGN_LEVEL_VALUE_BY_GRID_IS_SUCCESSS)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("对输入的矢量数据按照格网赋值处理失败（可能的原因：1、输入数据不存在；2、输入数据为空；3、输入数据文件不全；4、输出目录中已经存在了同名文件）"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  //  如果处理成功不需要将结果显示出来（在后面将会显示出来）
  has_processed_layers++;
#pragma endregion  

//  调用函数进行处理end
#pragma endregion
/**************************************************************************/

  //  对处理结果进行判断
  if (has_processed_layers != layer_count)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("正确处理了") + QString::number(has_processed_layers) + tr("图层，") +
      tr("还有") + QString::number(layer_count - has_processed_layers) + tr("图层对没有处理。") +
      tr("（可能的原因：1、图层数据无效；2、点要素分级处理工具；）"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  else
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("点要素分级处理工具"));
    msgBox.setText(tr("点要素分级处理完成！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion

}
//  关闭工具
void point_feature_hierarchical_processing::Button_Close()
{
  //  有可能在界面上设置了路径，但是不小心关掉了，这里所做的处理就是将设置了的内容保存起来以便下一次使用的时候恢复
  saveState();
  reject();
}
//  保存UI界面设置
void point_feature_hierarchical_processing::saveState()
{
  QgsSettings settings;
  // 指定设置保存到Plugins节
  settings.beginGroup("point_feature_hierarchical_processing_tool");
  settings.setValue(QStringLiteral("point_feature_hierarchical_processing_tool/m_input_data_path"), m_input_data_path);
  settings.setValue(QStringLiteral("point_feature_hierarchical_processing_tool/m_log_saving_path"), m_log_saving_path);
  settings.setValue(QStringLiteral("point_feature_hierarchical_processing_tool/m_output_data_path"), m_output_data_path);
  settings.setValue(QStringLiteral("point_feature_hierarchical_processing_tool/m_config_file_path"), m_config_file_path);
  // 结束分组，非常重要，确保设置正确保存在指定的节中
  settings.endGroup();
}
//  从数据库中恢复上一次UI界面设置
void point_feature_hierarchical_processing::restoreState()
{
  QgsSettings settings;
  // 指定从Plugins节读取设置
  settings.beginGroup("point_feature_hierarchical_processing_tool");
  this->m_input_data_path = settings.value(QStringLiteral("point_feature_hierarchical_processing_tool/m_input_data_path"), QDir::homePath()).toString();
  this->m_log_saving_path = settings.value(QStringLiteral("point_feature_hierarchical_processing_tool/m_log_saving_path"), QDir::homePath()).toString();
  this->m_output_data_path = settings.value(QStringLiteral("point_feature_hierarchical_processing_tool/m_output_data_path"), QDir::homePath()).toString();
  this->m_config_file_path = settings.value(QStringLiteral("point_feature_hierarchical_processing_tool/m_config_file_path"), QDir::homePath()).toString();
  // 结束分组
  settings.endGroup();
}


#pragma endregion
//  "工具的私有API接口" end
#pragma endregion
