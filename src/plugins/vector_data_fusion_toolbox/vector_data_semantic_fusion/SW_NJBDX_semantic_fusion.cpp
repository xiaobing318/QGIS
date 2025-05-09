#pragma region "1 包含头文件（减少重复）"
/************ C++/STL ************/
#include <string>
#include <filesystem>
#include <set>
#include <iostream>
#include <regex>
#include <fstream>

/************ GDAL/OGR ************/
#include "ogrsf_frmts.h"
#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "ogr_feature.h"

/************ spdlog ************/
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

/************ Qt ************/
#include <QFile>
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QString>

/************ QGIS 基础 ************/
#include "qgsapplication.h"
#include "qgsgui.h"
#include "qgssettings.h"
#include <QgsMessageLog.h>

/************ QGIS - 表达式、要素、图层操作 ************/
#include "qgsexpression.h"
#include "qgsexpressioncontext.h"
#include "qgsexpressioncontextutils.h"
#include "qgsfeature.h"
#include "qgsfields.h"
#include "qgsfield.h"

/************ 项目头文件 ************/
#include "SW_NJBDX_semantic_fusion.h"




#include <iostream>
#include <memory>
#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>
#include <ogr_spatialref.h>
#include <ogr_geometry.h>
#include <ogr_feature.h>
#include <ogr_api.h>

#pragma endregion

#pragma region "2 构造函数、析构函数"
qgs_SW_NJBDX_semantic_fusion::qgs_SW_NJBDX_semantic_fusion(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl),
  m_input_data_path(""),
  m_output_data_path(""),
  m_counter_total(0),
  m_counter_succeeded(0)
{
	ui.setupUi(this);

	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.Button_input_data_view, &QPushButton::clicked, this, &qgs_SW_NJBDX_semantic_fusion::Button_input_data_view);
	connect(ui.Button_output_data_view, &QPushButton::clicked, this, &qgs_SW_NJBDX_semantic_fusion::Button_output_data_view);
	connect(ui.Button_OK, &QPushButton::clicked, this, &qgs_SW_NJBDX_semantic_fusion::Button_OK);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &qgs_SW_NJBDX_semantic_fusion::Button_Cancel);

  restore_states();
}

qgs_SW_NJBDX_semantic_fusion::~qgs_SW_NJBDX_semantic_fusion()
{
  save_states();
}
#pragma endregion

#pragma region "3 类成员对象set、get函数"
//  获取输入数据目录成员函数
QString qgs_SW_NJBDX_semantic_fusion::get_m_input_data_path()
{
  //  从输入框中获取当前内容
  QString inputPath = ui.lineEdit_input_data_path->text();
  if (!inputPath.isEmpty())
  {
    //  对输入框中的路径进行规范化，并更新内部变量
    m_input_data_path = normalizePath(inputPath);
  }
  else
  {
    //  对内部变量不进行更新，直接返回空字符串
    return "";
  }
  // 返回内部变量中存储的路径
  return m_input_data_path;
}


//  获取输出数据目录成员函数
QString qgs_SW_NJBDX_semantic_fusion::get_m_output_data_path()
{
  //  从输入框中获取当前内容
  QString outputPath = ui.lineEdit_output_data_path->text();
  if (!outputPath.isEmpty())
  {
    //  对输入框中的路径进行规范化，并更新内部变量
    m_output_data_path = normalizePath(outputPath);
  }
  else
  {
    //  对内部变量不进行更新，直接返回空字符串
    return "";
  }
  return m_output_data_path;
}

//  设置输入数据目录成员函数（目前还没有用到）
void qgs_SW_NJBDX_semantic_fusion::set_m_input_data_path(const std::string& input_data_path)
{
  m_input_data_path = QString::fromStdString(input_data_path);
}

//  设置输出数据目录成员函数（目前还没有用到）
void qgs_SW_NJBDX_semantic_fusion::set_m_output_data_path(const std::string& output_data_path)
{
  m_output_data_path = QString::fromStdString(output_data_path);
}

//  恢复保存参数
void qgs_SW_NJBDX_semantic_fusion::restore_states()
{
  //  从上一次保存的路径数据中得到输入输出数据
  QgsSettings settings;
  //  指定从Plugins节读取设置
  settings.beginGroup("vector_data_SW_NJBDX_semantic_fusion");
  this->m_input_data_path = settings.value(QStringLiteral("vector_data_SW_NJBDX_semantic_fusion/m_input_data_path"), QDir::homePath()).toString();
  this->m_output_data_path = settings.value(QStringLiteral("vector_data_SW_NJBDX_semantic_fusion/m_output_data_path"), QDir::homePath()).toString();
  //  结束分组，非常重要，确保设置正确保存在指定的节中
  settings.endGroup();

  //  填充输入输出数据路径，显示在交互界面上
  ui.lineEdit_input_data_path->setText(m_input_data_path);
  ui.lineEdit_output_data_path->setText(m_output_data_path);
}

//  保存参数
void qgs_SW_NJBDX_semantic_fusion::save_states()
{
  QgsSettings settings;
  // 指定设置保存到Plugins节
  settings.beginGroup("vector_data_SW_NJBDX_semantic_fusion");
  settings.setValue(QStringLiteral("vector_data_SW_NJBDX_semantic_fusion/m_input_data_path"), m_input_data_path);
  settings.setValue(QStringLiteral("vector_data_SW_NJBDX_semantic_fusion/m_output_data_path"), m_output_data_path);
  // 结束分组，非常重要，确保设置正确保存在指定的节中
  settings.endGroup();
}
#pragma endregion

#pragma region "4 私有槽函数"

void qgs_SW_NJBDX_semantic_fusion::Button_input_data_view()
{
  //  选择文件夹
  QString curPath = m_input_data_path;
  QString dlgTile = QObject::tr("SW--->NJBDX：请选择语义融合工具输入数据源所在的目录位置");
  QString selectedDir = QFileDialog::getExistingDirectory(
    this,
    dlgTile,
    curPath,
    QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    ui.lineEdit_input_data_path->setText(selectedDir);
    //  需要将读取进来的路径字符串进行规范化
    m_input_data_path = normalizePath(selectedDir);
  }
}

void qgs_SW_NJBDX_semantic_fusion::Button_output_data_view()
{
  //  选择文件夹
  QString curPath = m_output_data_path;
  QString dlgTile = QObject::tr("SW--->NJBDX：请选择语义融合处理后数据存放的目录位置");
  QString selectedDir = QFileDialog::getExistingDirectory(
    this,
    dlgTile,
    curPath,
    QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    ui.lineEdit_output_data_path->setText(selectedDir);
    //  需要将读取进来的路径字符串进行规范化
    m_output_data_path = normalizePath(selectedDir);
  }
}

void qgs_SW_NJBDX_semantic_fusion::Button_OK()
{
  /*
  功能说明：
    1、当UI界面中的“确定”按钮被点击的时候，对SW--->NJBDX处理开始
    2、UI界面中的输入和输出路径作为该工具的输入、输出路径参数

  算法流程：
    1. 验证用户输入的数据源路径和保存转换后数据的路径。
    2. 加载SW--->NJBDX涉及到的四个配置文件。
    3. 使用提取和验证的数据执行最终的转换过程。
  */

  /*
    在点击确定的时候说明SW--->NJBDX转化开始了，在后面的语义融合过程中可能会出现异常导致程序不能顺利进行到最后,
  因此首先在这里将界面参数保存下来。
  */
  save_states();

#pragma region "（第一步）判断输入数据路径、输出数据路径在文件系统中是否存在"

  //  检查目录是否存在
  if (!is_exist_directory(get_m_input_data_path()))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("输入数据目录不存在，请重新输入！"));
    return;
  }
  if (!is_exist_directory(get_m_output_data_path()))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("输出数据目录不存在，请重新输入！"));
    return;
  }
#pragma endregion 

#pragma region "（第二步）获取输入数据路径、输出数据的路径"

  //  获取输入数据路径
  QString qstr_input_data_path = get_m_input_data_path();
  std::string str_input_data_path = qstr_input_data_path.toUtf8().toStdString();

  //  获取数据保存目录
  QString qstr_output_data_path = get_m_output_data_path();
  std::string str_output_data_path = qstr_output_data_path.toUtf8().toStdString();

  //  获取输入路径下中的分幅数据列表
  QStringList qstr_input_data_path_list = get_sub_directories(qstr_input_data_path);

#pragma endregion

#pragma region "（第三步）读取SW--->NJBDX配置文件，并且检查五个配置文件是否有效"
  //  获取程序所在目录，并且对路径字符串统一化
  QString appDirPath = normalizePath(QCoreApplication::applicationDirPath());
  //  创建指向config文件夹的QDir对象
  QDir configDir(appDirPath);
  //  切换到config文件夹
  if (!configDir.cd("DLHJ_Vector_Data_Semantic_Fusion_Config"))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("配置目录 'DLHJ_Vector_Data_Semantic_Fusion_Config' 不存在！"));
    return;
  }

  //  拼接各个配置文件的完整路径
  QString configFilePath1 = configDir.filePath("SW/SW_Layers_Fields_Info.json");
  QString configFilePath2 = configDir.filePath("SW/SW_Layers_Mapping_NJBDX_Layers.json");
  QString configFilePath3 = configDir.filePath("SW/SW_Classification_Code_Conditional_Mapping_Lists.json");
  QString configFilePath4 = configDir.filePath("SW/SW_Other_Fields_Mapping_NJBDX_Other_Fields.json");
  QString configFilePath5 = configDir.filePath("NJBDX/NJBDX_Layers_Fields_Info.json");
  //  检查配置文件路径是否存在
  if (!is_exist_file(configFilePath1))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("配置文件SW_Layers_Fields_Info.json不存在！"));
    return;
  }
  if (!is_exist_file(configFilePath2))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("配置文件SW_Layers_Mapping_NJBDX_Layers.json不存在！"));
    return;
  }
  if (!is_exist_file(configFilePath3))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("配置文件SW_Classification_Code_Conditional_Mapping_Lists.json不存在！"));
    return;
  }
  if (!is_exist_file(configFilePath4))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("配置文件SW_Other_Fields_Mapping_NJBDX_Other_Fields.json不存在！"));
    return;
  }
  if (!is_exist_file(configFilePath5))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("配置文件NJBDX_Layers_fields_Info.json不存在！"));
    return;
  }

  //  创建或获取一个日志器
  std::shared_ptr<spdlog::logger> read_config_logger = spdlog::get("read_config_logger");
  if (!read_config_logger)
  {
    //  使用 std::unique_ptr
    auto formatter = std::make_unique<spdlog::pattern_formatter>("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

#pragma region "处理日志文件路径的字符集编码问题，在windows平台上文件系统使用的是UTF-16字符集编码"
    //  拼接配置文件检查日志输出目录
    QString config_check_log_dir = appDirPath + "/DLHJ_Vector_Data_Semantic_Fusion_Config";
    //  创建目录操作对象
    QDir outputDir(config_check_log_dir);
    //  这个目录需要存在，因为相关的配置文件保存在这个目录中
    if (!outputDir.exists())
    {
        showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("配置目录DLHJ_Vector_Data_Semantic_Fusion_Config不存在！"));
        return;
    }
    //  拼接日志文件路径，返回“输出目录/config_check_log.txt”
    QString config_check_log_path = outputDir.absoluteFilePath("config_check_log.txt");

#pragma endregion

    //  创建文件日志器
    read_config_logger = spdlog::basic_logger_mt(
      "read_config_logger",
      normalizePath(config_check_log_path).toLocal8Bit().toStdString(),
      true);
    //  使用 std::move(...) 传递 formatter
    read_config_logger->set_formatter(std::move(formatter));
    //  设置日志级别为最低级别，以便记录所有级别的日志
    read_config_logger->set_level(spdlog::level::trace);
    //  设置在任何级别时都刷新
    read_config_logger->flush_on(spdlog::level::trace);
    //  记录初始化信息
    read_config_logger->info("读取配置文件日志开始！");
  }
  else
  {
    read_config_logger->info("读取配置文件日志开始！");
  }

  //  读取配置文件内容
  SW_Layers_Fields_Info_Json_t SW_Layers_Fields_Info_Json_entity = readSW_Layers_Fields_Info_Json(
    normalizePath(configFilePath1).toUtf8(),
    read_config_logger);
  //  检查配置文件是否有效
  if (!checkSW_Layers_Fields_Info_Json(SW_Layers_Fields_Info_Json_entity, read_config_logger))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("SW_Layers_Fields_Info.json配置文件存在问题，请检查！"));
    return;
  }

  SW_Layers_Mapping_NJBDX_Layers_Json_t SW_Layers_Mapping_NJBDX_Layers_Json_entity = readSW_Layers_Mapping_NJBDX_Layers_Json(
    normalizePath(configFilePath2).toUtf8(),
    read_config_logger);
  if (!checkSW_Layers_Mapping_NJBDX_Layers_Json(SW_Layers_Mapping_NJBDX_Layers_Json_entity, read_config_logger))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("SW_Layers_Mapping_NJBDX_Layers.json配置文件存在问题，请检查！"));
    return;
  }


  SW_Classification_Code_Conditional_Mapping_Lists_Json_t SW_Classification_Code_Conditional_Mapping_Lists_Json_entity = readSW_Classification_Code_Conditional_Mapping_Lists_Json(
    normalizePath(configFilePath3).toUtf8(),
    read_config_logger);
  if (!checkSW_Classification_Code_Conditional_Mapping_Lists_Json(SW_Classification_Code_Conditional_Mapping_Lists_Json_entity, read_config_logger))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("SW_Classification_Code_Conditional_Mapping_Lists.json配置文件存在问题，请检查！"));
    return;
  }

  SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity = readSW_Other_Fields_Mapping_NJBDX_Other_Fields_Json(
    normalizePath(configFilePath4).toUtf8(),
    read_config_logger);
  if (!checkSW_Other_Fields_Mapping_NJBDX_Other_Fields_Json(SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity, read_config_logger))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("SW_Other_Fields_Mapping_NJBDX_Other_Fields.json配置文件存在问题，请检查！"));
    return;
  }

  SW_NJBDX_Layers_Fields_Info_Json_t NJBDX_Layers_Fields_Info_Json_entity = SW_readNJBDX_Layers_Fields_Info_Json(
    normalizePath(configFilePath5).toUtf8(),
    read_config_logger);
  if (!checkNJBDX_Layers_Fields_Info_Json(NJBDX_Layers_Fields_Info_Json_entity, read_config_logger))
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("NJBDX_layers_fields_info.json配置文件存在问题，请检查！"));
    return;
  }

#pragma endregion

#pragma region "（第四步）循环从分幅数据中处理SW--->NJBDX语义融合"

#pragma region "1、获取分幅数据的绝对路径"
  //	创建临时变量用来存储输入数据路径中多个分幅数据的绝对路径
  std::vector<std::string> vstr_input_frame_data_path;
  vstr_input_frame_data_path.clear();

  //  如果输入路径中没有分幅数据，则将输入路径保存到这个临时变量中，相当于单个分幅数据
  if (qstr_input_data_path_list.size() == 0)
  {
    //	将输入路径保存到临时变量中以供后续使用
    vstr_input_frame_data_path.push_back(str_input_data_path);
  }
  else
  {
    //	将所有子目录的绝对路径添加到临时变量中以供后续使用
    for (int i = 0; i < qstr_input_data_path_list.size(); i++)
    {
      std::string strPathTemp = normalizePath(qstr_input_data_path_list[i]).toUtf8().toStdString();
      vstr_input_frame_data_path.push_back(strPathTemp);
    }
  }

#pragma endregion

#pragma region "2、SW--->NJBDX语义融合处理"

  //  SW--->NJBDX
  try
  {
      SW2NJBDX(
      vstr_input_frame_data_path,
      str_output_data_path,
      SW_Layers_Fields_Info_Json_entity,
      SW_Layers_Mapping_NJBDX_Layers_Json_entity,
      SW_Classification_Code_Conditional_Mapping_Lists_Json_entity,
      SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity,
      NJBDX_Layers_Fields_Info_Json_entity);
  }
  catch (const std::exception& e)
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), QString::fromStdString(e.what()));
    return;
  }
  catch (...)
  {
    showMessageBox(tr("矢量数据语义融合（SW--->NJBDX）"), tr("发生未知异常！"));
    return;
  }

  //  显示统计信息
  {
    //  记录日志
    QMessageBox msgBox1;
    msgBox1.setWindowTitle(tr("矢量语义融合处理（SW--->NJBDX）"));
    //  拼接显示信息
    QString qstrText = tr("一共") + QString::number(m_counter_total)
      + tr("幅分幅数据；其中成功处理了") + QString::number(m_counter_succeeded)
      + tr("幅分幅数据；失败了") + QString::number(m_counter_total - m_counter_succeeded)
      + tr("幅分幅数据。详细信息请查看日志文件！");
    msgBox1.setText(qstrText);
    msgBox1.setStandardButtons(QMessageBox::Ok);
    msgBox1.setDefaultButton(QMessageBox::Ok);
    msgBox1.exec();
  
    //  重置总图幅计数器、成功处理图幅计数器（目前使用成员变量的方式，如果修改成临时变量的方式则不需要在这里进行重置）
    m_counter_total = 0;
    m_counter_succeeded = 0;

    return;
  }
#pragma endregion

#pragma endregion

}

void qgs_SW_NJBDX_semantic_fusion::Button_Cancel()
{
  reject();
}

#pragma endregion

#pragma region "6 类辅助成员函数"
//  判断目录是否存在
bool qgs_SW_NJBDX_semantic_fusion::is_exist_directory(const QString& qstr_directory_path)
{
  if (qstr_directory_path.isEmpty())
  {
    return false;
  }
  QDir dir(qstr_directory_path);
  return dir.exists();
}

//  判断文件是否存在
bool qgs_SW_NJBDX_semantic_fusion::is_exist_file(const QString& qstr_file_path)
{
  return QFile::exists(qstr_file_path);
}

//  获取指定目录中的多个子目录
QStringList qgs_SW_NJBDX_semantic_fusion::get_sub_directories(const QString& file_directory_path)
{
  QStringList dirPaths;
  QDir QDir_file_directory_path(file_directory_path);
  QFileInfoList file_info_list_in_QDir_file_directory_path = QDir_file_directory_path.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  QFileInfo tempFileInfo;
  foreach(tempFileInfo, file_info_list_in_QDir_file_directory_path)
  {
    dirPaths << tempFileInfo.absoluteFilePath();
  }
  return dirPaths;
}

//  自定义信息提示框
void qgs_SW_NJBDX_semantic_fusion::showMessageBox(const QString& title, const QString& text)
{
  QMessageBox msgBox;
  msgBox.setWindowTitle(title);
  msgBox.setText(text);
  msgBox.setStandardButtons(QMessageBox::Yes);
  msgBox.setDefaultButton(QMessageBox::Yes);
  msgBox.exec();
}

//  辅助函数：转换路径为统一格式
QString qgs_SW_NJBDX_semantic_fusion::normalizePath(const QString& path)
{
  // 将反斜杠替换为正斜杠
  QString normalizedPath = path;
  normalizedPath.replace("\\", "/");
  return normalizedPath;
}
#pragma endregion

#pragma region "7 SW--->NJBDX内部函数"

void qgs_SW_NJBDX_semantic_fusion::SW2NJBDX(
  const std::vector<std::string>& vstr_input_frame_data_path,
  const std::string& str_output_data_path,
  const SW_Layers_Fields_Info_Json_t& SW_Layers_Fields_Info_Json_entity,
  const SW_Layers_Mapping_NJBDX_Layers_Json_t& SW_Layers_Mapping_NJBDX_Layers_Json_entity,
  const SW_Classification_Code_Conditional_Mapping_Lists_Json_t& SW_Classification_Code_Conditional_Mapping_Lists_Json_entity,
  const SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t& SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity,
  const SW_NJBDX_Layers_Fields_Info_Json_t& NJBDX_Layers_Fields_Info_Json_entity)
{
  /*
  Note:杨小兵-2025-01-15

  1、函数功能
    1.1 字符串数组中存储的是多个分幅数据的绝对路径，每一个分幅数据中是由多个图层构成的
    1.2 该函数将会循环将每个分幅数据中的图层进行语义融合处理
  2、算法流程
    2.1 获取当前分幅数据绝对路径
    2.2 对当前分幅数据进行语义融合处理
    2.3 对处理结果进行判断
  3、前提假设
    3.1 上述传入的七个参数都是有效的（由调用者保证）
  */

  //  获取分幅数据的数量
  size_t size = vstr_input_frame_data_path.size();

  //  更新总图幅计数器
  m_counter_total = size;

  //  循环处理
  for (size_t i = 0; i < size; i++)
  {
    try {
       //  记录当前语义融合处理的结果标志
       int iresult_single_frame_data;
       //  对当前分幅数据进行语义融合处理
       iresult_single_frame_data = SW2NJBDX_Single_Frame_Data(
         vstr_input_frame_data_path[i],
         str_output_data_path,
         SW_Layers_Fields_Info_Json_entity,
         SW_Layers_Mapping_NJBDX_Layers_Json_entity,
         SW_Classification_Code_Conditional_Mapping_Lists_Json_entity,
         SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity,
         NJBDX_Layers_Fields_Info_Json_entity);
       //	如果当前分幅数据语义融合处理失败则需要将日志信息输出到QGIS日志板上
       if (iresult_single_frame_data != 0)
       {
         QString qstrMsg = QString("分幅数据：%1 矢量数据语义融合失败！")
           .arg(QString::fromStdString(vstr_input_frame_data_path[i]));
         QString qstrTag = "SW--->NJBDX";
         QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);

         //  继续处理其他分幅数据
         continue;
       }
       else
       {
         //  更新成功计数器
         m_counter_succeeded++;
       }
    }
    catch (const std::exception& e)
    {
      // 使用 QString 进行字符串拼接
      QString qstrMsg = QString("分幅数据：%1 处理时发生异常：%2")
        .arg(QString::fromStdString(vstr_input_frame_data_path[i]))
        .arg(QString::fromStdString(e.what()));
      QString qstrTag = "SW--->NJBDX";
      QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
      // 继续处理下一个分幅数据
      continue;
    }
    catch (...)
    {
      QString qstrMsg = QString("分幅数据：%1 处理时发生未知异常！")
        .arg(QString::fromStdString(vstr_input_frame_data_path[i]));
      QString qstrTag = "SW--->NJBDX";
      QgsMessageLog::logMessage(qstrMsg, qstrTag, Qgis::Critical);
      // 继续处理下一个分幅数据
      continue;
    }
  }

  //  成功处理返回
  return;
}

int qgs_SW_NJBDX_semantic_fusion::SW2NJBDX_Single_Frame_Data(
  const std::string& single_frame_data_path,
  const std::string& str_output_data_path,
  const SW_Layers_Fields_Info_Json_t& SW_Layers_Fields_Info_Json_entity,
  const SW_Layers_Mapping_NJBDX_Layers_Json_t& SW_Layers_Mapping_NJBDX_Layers_Json_entity,
  const SW_Classification_Code_Conditional_Mapping_Lists_Json_t& SW_Classification_Code_Conditional_Mapping_Lists_Json_entity,
  const SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t& SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity,
  const SW_NJBDX_Layers_Fields_Info_Json_t& NJBDX_Layers_Fields_Info_Json_entity)
{
  /*
  Note:杨小兵-2025-01-15

  1、函数功能：实现SW单个分幅数据的语义融合处理
  2、算法流程
    2.1 将单个分幅数据目录路径下的所有图层读取到内存当中
    2.2 将所有图层的相关信息读取到自定义数据结构中（图层名称、图层类型、图层指针等等）
    2.3 循环对每一个图层进行转化操作
  3、前提假设
    3.1 上述传入的七个参数都是有效的（由调用者保证）
  */

#pragma region "第一步：创建输出目录、日志器、日志文件"
  //  创建临时变量用来保存最终的输出目录
  std::string strSingleSuccessfulSemanticFusionOutputDataPath = "";
  std::string strSingleFailedSemanticFusionOutputDataPath = "";
  //  创建日志器用来写入日志信息
  std::shared_ptr<spdlog::logger> single_frame_data_successful_logger = nullptr;
  std::shared_ptr<spdlog::logger> single_frame_data_failed_logger = nullptr;

  try {

    //  将 std::string 转为 QString（输入为 UTF-8 编码）
    QString qSingleFrameDataPath = QString::fromStdString(single_frame_data_path);
    QString qOutputDataPath = QString::fromStdString(str_output_data_path);

    //  1. 检查输入路径是否存在且为目录
    QDir inputDir(qSingleFrameDataPath);
    if (!inputDir.exists() || !QFileInfo(qSingleFrameDataPath).isDir())
    {
      QString qstrMsg = QString("在函数 %1 中发生异常：输入的分幅数据路径不存在或不是一个目录：%2")
        .arg(__FUNCTION__)
        .arg(qSingleFrameDataPath);
      QgsMessageLog::logMessage(qstrMsg, "SW--->NJBDX", Qgis::Critical);
      return -1;
    }

    //  2. 获取目录名，比如 DN09490622，QDir::dirName() 返回当前 QDir 对应路径的最后一级目录名
    QString frame_dir_name = inputDir.dirName();
    //  3. 拼接输出目录名称（语义融合成功、语义融合失败）
    QString SuccessfulSemanticFusionOutputDirName = "SuccessfulSemanticFusion_SW2NJBDX_SRCDATA_" + frame_dir_name;
    QString FailedSemanticFusionOutputDirName = "FailedSemanticFusion_SW2NJBDX_SRCDATA_" + frame_dir_name;

    //  拼接最终完整输出目录
    QString qSingleSuccessfulSemanticFusionOutputDataPath = qOutputDataPath + "/" + SuccessfulSemanticFusionOutputDirName;
    QString qSingleFailedSemanticFusionOutputDataPath = qOutputDataPath + "/" + FailedSemanticFusionOutputDirName;
    //  同步回到 std::string，后面可能 GDAL/其他逻辑还要用
    strSingleSuccessfulSemanticFusionOutputDataPath = qSingleSuccessfulSemanticFusionOutputDataPath.toStdString();
    strSingleFailedSemanticFusionOutputDataPath = qSingleFailedSemanticFusionOutputDataPath.toStdString();
    //  4.1 创建最终的输出目录（语义融合成功）
    QDir qSingleSuccessfulSemanticFusionOutputDir(qSingleSuccessfulSemanticFusionOutputDataPath);
    if (!qSingleSuccessfulSemanticFusionOutputDir.exists())
    {
      //  mkpath()：若多级目录不存在会一并创建
      if (!qSingleSuccessfulSemanticFusionOutputDir.mkpath(qSingleSuccessfulSemanticFusionOutputDataPath))
      {
        QString qstrMsg = QString("在函数 %1 中发生异常：无法创建输出目录：%2")
          .arg(__FUNCTION__)
          .arg(qSingleSuccessfulSemanticFusionOutputDataPath);
        QgsMessageLog::logMessage(qstrMsg, "SW--->NJBDX", Qgis::Critical);
        return -2;
      }
    }
    //  4.2 创建最终的输出目录（语义融合失败）
    QDir qSingleFailedSemanticFusionOutputDir(qSingleFailedSemanticFusionOutputDataPath);
    if (!qSingleFailedSemanticFusionOutputDir.exists())
    {
      //  mkpath()：若多级目录不存在会一并创建
      if (!qSingleFailedSemanticFusionOutputDir.mkpath(qSingleFailedSemanticFusionOutputDataPath))
      {
        QString qstrMsg = QString("在函数 %1 中发生异常：无法创建输出目录：%2")
          .arg(__FUNCTION__)
          .arg(qSingleFailedSemanticFusionOutputDataPath);
        QgsMessageLog::logMessage(qstrMsg, "SW--->NJBDX", Qgis::Critical);
        return -2;
      }
    }


    //  5. 拼接日志文件路径，qSingleSuccessfulSemanticFusionOutputDir.absoluteFilePath("log.txt") 返回“输出目录/log.txt”
    QString qSingleSuccessfulSemanticFusionLogFilePath = qSingleSuccessfulSemanticFusionOutputDir.absoluteFilePath("log.txt");
    QString qSingleFailedSemanticFusionLogFilePath = qSingleFailedSemanticFusionOutputDir.absoluteFilePath("log.txt");


    try
    {
      //  创建第一个格式化器
      auto successful_formatter = std::make_unique<spdlog::pattern_formatter>("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
      //  复制一个新的格式化器给第二个日志器
      auto failed_formatter = successful_formatter->clone();

      /*
        创建文件日志器：需要 std::string 路径(最后一个参数 truncate = false 表示追加)
      不再用 toUtf8()，改用 toLocal8Bit() 得到本地编码。
      */
      std::string SingleSuccessfulSemanticFusionLogFilePathSTD = qSingleSuccessfulSemanticFusionLogFilePath.toLocal8Bit().toStdString();
      std::string SingleFailedSemanticFusionLogFilePathSTD = qSingleFailedSemanticFusionLogFilePath.toLocal8Bit().toStdString();

      //  创建当前目录的语义融合处理成功的日志器
      single_frame_data_successful_logger = spdlog::basic_logger_mt(
        "single_frame_successful_logger_" + frame_dir_name.toStdString(),
        SingleSuccessfulSemanticFusionLogFilePathSTD,
        true
      );

      //  创建当前目录的语义融合处理失败的日志器
      single_frame_data_failed_logger = spdlog::basic_logger_mt(
        "single_frame_failed_logger_" + frame_dir_name.toStdString(),
        SingleFailedSemanticFusionLogFilePathSTD,
        true
      );

      //  使用 std::move(...) 传递 successful_formatter、failed_formatter
      single_frame_data_successful_logger->set_formatter(std::move(successful_formatter));
      single_frame_data_failed_logger->set_formatter(std::move(failed_formatter));

      //  设置日志级别为最低级别，以便记录所有级别的日志
      single_frame_data_successful_logger->set_level(spdlog::level::trace);
      single_frame_data_failed_logger->set_level(spdlog::level::trace);

      //  设置在任何级别时都刷新
      single_frame_data_successful_logger->flush_on(spdlog::level::trace);
      single_frame_data_failed_logger->flush_on(spdlog::level::trace);

      //  记录初始化信息
      single_frame_data_successful_logger->info("*****************************************开始处理分幅数据：{}*****************************************", single_frame_data_path);
      single_frame_data_failed_logger->info("*****************************************开始处理分幅数据：{}*****************************************", single_frame_data_path);
      
      //  保证输出的字符串的字符集编码都是UTF-8
      single_frame_data_successful_logger->info("日志文件路径：{}", qSingleSuccessfulSemanticFusionLogFilePath.toUtf8().toStdString());
      single_frame_data_failed_logger->info("日志文件路径：{}", qSingleFailedSemanticFusionLogFilePath.toUtf8().toStdString());
    }
    catch (const spdlog::spdlog_ex& ex)
    {
      QString qstrMsg = QString("在函数 %1 中发生异常：无法初始化日志器：%2")
        .arg(__FUNCTION__)
        .arg(QString::fromStdString(ex.what()));
      QgsMessageLog::logMessage(qstrMsg, "SW--->NJBDX", Qgis::Critical);
      return -3;
    }

  }
  catch (const std::exception& e)
  {
    QString qstrMsg = QString("在函数 %1 处理中发生异常：%2")
      .arg(__FUNCTION__)
      .arg(QString::fromStdString(e.what()));
    QgsMessageLog::logMessage(qstrMsg, "SW--->NJBDX", Qgis::Critical);
    return -4;
  }
  catch (...)
  {
    QString qstrMsg = QString("在函数 %1 处理中发生未知异常！")
      .arg(__FUNCTION__);
    QgsMessageLog::logMessage(qstrMsg, "SW--->NJBDX", Qgis::Critical);
    return -5;
  }

#pragma endregion

#pragma region "第二步：设置GDAL并且获取驱动器"
  //  这告诉 GDAL 文件名不使用系统的本地编码（在 Windows 上为 UTF-16），而是UTF-8编码的路径字符串
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  //  将 SHAPE_ENCODING 设置为空字符串 "" 表示 GDAL 将尝试自动检测 DBF 文件的编码，或者使用默认编码。
  CPLSetConfigOption("SHAPE_ENCODING", "");
  //  注册所有 GDAL 驱动
  GDALAllRegister();
  const char* pszShpDriverName = "ESRI Shapefile";
  GDALDriver* poShpDriver;
  poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
  if (poShpDriver == NULL)
  {
    single_frame_data_successful_logger->error("函数[{}]：获取ESRI Shapefile矢量驱动器失败。", __FUNCTION__);
    single_frame_data_successful_logger->info("*****************************************结束处理分幅数据：{}*****************************************", single_frame_data_path);
    return -6;
  }
#pragma endregion

#pragma region "第三步：将SW数据所有图层的相关信息读取到自定义数据结构中"

  //  利用矢量驱动器打开当前分幅数据目录路径下的矢量数据,以只读数据的方式打开数据
  GDALDataset* poSingleFrameSWDataSet = (GDALDataset*)GDALOpenEx(single_frame_data_path.c_str(), GDAL_OF_READONLY, NULL, NULL, NULL);
  //  文件不存在或打开失败
  if (poSingleFrameSWDataSet == NULL)
  {
    single_frame_data_successful_logger->error("函数[{}]：使用GDALOpenEx打开数据集{}失败。", __FUNCTION__, single_frame_data_path);
    single_frame_data_successful_logger->info("*****************************************结束处理分幅数据：{}*****************************************", single_frame_data_path);
    return -7;
  }

  //	创建变量用来存储SW分幅数据相关信息
  SW_single_frame_data_all_layers_info_t SW_single_frame_data_all_layers_info_entity;
  //  读取当前分幅数据矢量图层的信息：分幅数据路径、数据集指针；对每个图层而言记录信息有：图层名字、图层类型、图层指针、图层空间参考
  SW_Create_single_frame_data_all_layers_info(
    poSingleFrameSWDataSet,
    SW_single_frame_data_all_layers_info_entity,
    single_frame_data_successful_logger);
  //  检查数据集中的图层是否为空
  if (!SW_Check_single_frame_data_all_layers_info(SW_single_frame_data_all_layers_info_entity, single_frame_data_successful_logger))
  {
    single_frame_data_successful_logger->error("函数[{}]：分幅数据{}中的有效矢量图层为空。", __FUNCTION__, single_frame_data_path);
    //  需要将已经打开的数据集关闭以释放内存资源
    SW_Close_single_frame_data_all_layers_info(SW_single_frame_data_all_layers_info_entity);
    single_frame_data_successful_logger->info("*****************************************结束处理分幅数据：{}*****************************************", single_frame_data_path);
    return -8;
  }
#pragma endregion

#pragma region "第四步：将SW数据所有图层的相关信息读取到自定义数据结构中，创建空图层信息"
  //  创建变量用来存储SW分幅数据空数据相关信息
  SW_single_frame_data_all_layers_info_t SW_single_frame_data_all_layers_info_empty_entity;
  //  根据SW单个分幅数据中所有图层的信息创建一个空的输出数据集
  CreateEmptyDatasetFromSWInfo(
      SW_single_frame_data_all_layers_info_entity,
      SW_single_frame_data_all_layers_info_empty_entity,
      strSingleFailedSemanticFusionOutputDataPath,
      poShpDriver,
      single_frame_data_failed_logger);
  //  检查数据集中的图层是否为空
  if (!SW_Check_single_frame_data_all_layers_info(SW_single_frame_data_all_layers_info_empty_entity, single_frame_data_failed_logger))
  {
    single_frame_data_failed_logger->error("函数[{}]：分幅数据{}中的有效矢量图层为空。", __FUNCTION__, single_frame_data_path);
    //  需要将已经打开的数据集关闭以释放内存资源
    SW_Close_single_frame_data_all_layers_info(SW_single_frame_data_all_layers_info_entity);
    single_frame_data_failed_logger->info("*****************************************结束处理分幅数据：{}*****************************************", single_frame_data_path);
    return -8;
  }
#pragma endregion

#pragma region "第五步：根据SW实际数据和图层映射关系创建NJBDX数据集信息到自定义数据结构中"
  //  创建变量用来存储NJBDX分幅数据相关信息
  SW_NJBDX_single_frame_data_all_layers_info_t NJBDX_single_frame_data_all_layers_info_entity;
  //  使用SW数据、图层映射关系、NJBDX字段信息创建NJBDX数据集，即创建图层
  SW_Create_NJBDX_all_layers_info(
    SW_single_frame_data_all_layers_info_entity,
    SW_Layers_Mapping_NJBDX_Layers_Json_entity,
    SW_Layers_Fields_Info_Json_entity,
    NJBDX_Layers_Fields_Info_Json_entity,
    strSingleSuccessfulSemanticFusionOutputDataPath,
    NJBDX_single_frame_data_all_layers_info_entity,
    single_frame_data_successful_logger);
  //  检查数据集中的图层是否为空
  if (!SW_Check_NJBDX_all_layers_info(NJBDX_single_frame_data_all_layers_info_entity))
  {
    single_frame_data_successful_logger->error("函数[{}]：创建的NJBDX分幅数据{}中的有效矢量图层为空。", __FUNCTION__, strSingleSuccessfulSemanticFusionOutputDataPath);
    //  需要将两个数据集关闭以释放内存资源
    SW_Close_single_frame_data_all_layers_info(SW_single_frame_data_all_layers_info_entity);
    SW_Close_NJBDX_all_layers_info(NJBDX_single_frame_data_all_layers_info_entity);
    single_frame_data_successful_logger->info("*****************************************结束处理分幅数据：{}*****************************************", single_frame_data_path);
    return -9;
  }

#pragma endregion

#pragma region "第六步：根据SW数据集信息和NJBDX数据集信息、分类编码配置信息进行字段属性映射"
  SW_Mapping_NJBDX_Fields(
    SW_single_frame_data_all_layers_info_entity,
    SW_single_frame_data_all_layers_info_empty_entity,
    NJBDX_single_frame_data_all_layers_info_entity,
    SW_Layers_Fields_Info_Json_entity,
    NJBDX_Layers_Fields_Info_Json_entity,
    SW_Layers_Mapping_NJBDX_Layers_Json_entity,
    SW_Classification_Code_Conditional_Mapping_Lists_Json_entity,
    SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity,
    single_frame_data_successful_logger,
    single_frame_data_failed_logger);
#pragma endregion

#pragma region "第七步：释放SW分幅数据、NJBDX分幅数据中创建的内存资源"
  //  需要将三个数据集关闭以释放内存资源
  SW_Close_single_frame_data_all_layers_info(SW_single_frame_data_all_layers_info_entity);
  //  删除空图层以及关闭非空图层
  SW_Delete_And_Close_single_frame_data_all_layers_info(SW_single_frame_data_all_layers_info_empty_entity);
  //  删除空图层以及关闭非空图层
  SW_Close_NJBDX_all_layers_info(NJBDX_single_frame_data_all_layers_info_entity);
  single_frame_data_successful_logger->info("*****************************************结束处理分幅数据：{}*****************************************", single_frame_data_path);
  single_frame_data_failed_logger->info("*****************************************结束处理分幅数据：{}*****************************************", single_frame_data_path);
  single_frame_data_successful_logger->info("\n");
  single_frame_data_failed_logger->info("\n");

  //  使用 spdlog::shutdown() 来关闭所有日志器
  spdlog::shutdown();
#pragma endregion

  return 0;
}





#pragma region "SW2NJBDX_Single_Frame_Data中第六步对应函数"

// -----------------------------------------------------------------------------
//  主要实现函数：将 SW 数据源中的要素按分类编码映射到 NJBDX 数据源中
// -----------------------------------------------------------------------------
void qgs_SW_NJBDX_semantic_fusion::SW_Mapping_NJBDX_Fields(
  const SW_single_frame_data_all_layers_info_t& SW_single_frame_data_all_layers_info_entity,
  const SW_single_frame_data_all_layers_info_t& SW_single_frame_data_all_layers_info_empty_entity,
  const SW_NJBDX_single_frame_data_all_layers_info_t& NJBDX_single_frame_data_all_layers_info_entity,
  const SW_Layers_Fields_Info_Json_t& SW_Layers_Fields_Info_Json_entity,
  const SW_NJBDX_Layers_Fields_Info_Json_t& NJBDX_Layers_Fields_Info_Json_entity,
  const SW_Layers_Mapping_NJBDX_Layers_Json_t& SW_Layers_Mapping_NJBDX_Layers_Json_entity,
  const SW_Classification_Code_Conditional_Mapping_Lists_Json_t& SW_Classification_Code_Conditional_Mapping_Lists_Json_entity,
  const SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t& SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity,
  std::shared_ptr<spdlog::logger> single_frame_data_successful_logger,
  std::shared_ptr<spdlog::logger> single_frame_data_failed_logger)
{
  single_frame_data_successful_logger->info("<------------------函数[{}]日志开始--->进行 SW -> NJBDX 字段属性值映射------------------>",
    __FUNCTION__);

  //  循环处理 SW 图层
  int SW_layer_counter = 1;
  for (const auto& SW_single_layer : SW_single_frame_data_all_layers_info_entity.vSW_single_frame_data_single_layer_info)
  {
    //  日志中提示当前正在处理的SW图层
    single_frame_data_successful_logger->info("<----------函数[{}]：开始处理 SW 中第 {} 个图层：{} (该图层GDAL几何类型：{} )---------->",
      __FUNCTION__,
      SW_layer_counter,
      SW_single_layer.layer_name,
      OGRGeometryTypeToName(SW_single_layer.layer_geo_type));

#pragma region "1 在 SW_Layers_Mapping_NJBDX_Layers_Json_t 中找出当前SW图层对应的映射(一对多)"

    //  在 SW_Layers_Mapping_NJBDX_Layers_Json_t 中找出当前SW图层对应的映射(一对多)
    const auto& allMappings = SW_Layers_Mapping_NJBDX_Layers_Json_entity.vSW_layers_mapping_NJBDX_layers;
    //  使用 std::find_if 查找匹配的映射
    auto itFound = std::find_if(allMappings.begin(), allMappings.end(),
      [&SW_single_layer, SW_Layers_Fields_Info_Json_entity, this, single_frame_data_successful_logger](const SW_layers_mapping_NJBDX_layers_t& item) -> bool
      {
        //  解析当前图层的名称（例如D）
        std::string parsedName = this->SW_Extract_LayerName(
          SW_single_layer.layer_name,
          SW_Layers_Fields_Info_Json_entity,
          single_frame_data_successful_logger);

        //  如果解析失败，跳过此元素
        if (parsedName.empty())
        {
          single_frame_data_successful_logger->info("函数[{}]：当前 SW 图层{}名称类型解析失败。", __FUNCTION__, SW_single_layer.layer_name);
          return false;
        }

        //  比较解析后的名称与映射中的名称
        bool nameMatches = (item.SW_Layer.SW_layer_english_name == parsedName);

        //  比较几何类型
        bool geometryMatches = (item.SW_Layer.SW_layer_type == SW_single_layer.layer_geo_type);

        //  返回两个条件都满足的结果
        return nameMatches && geometryMatches;
      }
    );
    //  如果在图层映射表中没有找到映射关系则需要将情况写入日志中
    if (itFound == allMappings.end())
    {
      single_frame_data_successful_logger->warn("函数[{}]：无法在SW_Layers_Mapping_NJBDX_Layers.json中找到与图层 [{}] 对应的映射关系，跳过当前图层的处理。",
        __FUNCTION__,
        SW_single_layer.layer_name);
      continue;
    }

    //  在上一步中找到了当前SW图层对应映射的NJBDX图层，获取SW当前 OGRLayer* 图层指针
    OGRLayer* poSWLayer = SW_single_layer.polayer;
    //  判断SW图层指针是否有效
    if (!poSWLayer)
    {
      single_frame_data_successful_logger->warn("函数[{}]：SW 图层 [{}] 的 OGRLayer 指针为空，跳过当前图层的处理。",
        __FUNCTION__,
        SW_single_layer.layer_name);
      continue;
    }
#pragma endregion

#pragma region "2 遍历分类编码条件映射表"

    //  首先需要查询到SW空数据图层名称同SW非空数据图层名称相同的图层指针（对于遍历分类编条件映射表只需要查找一次）
    OGRLayer* pDestLayer = NULL;
    for (const auto& SW_current_single_empty_layer : SW_single_frame_data_all_layers_info_empty_entity.vSW_single_frame_data_single_layer_info)
    {
      if (SW_current_single_empty_layer.layer_name == SW_single_layer.layer_name)
      {
        pDestLayer = SW_current_single_empty_layer.polayer;
        break;
      }
    }
    if (!pDestLayer)
    {
      single_frame_data_successful_logger->warn("函数[{}]：当前处理的SW图层名称{}在语义融合失败图层名称集合中没有找到相同的图层名称，跳过语义融合失败图层要素写入操作。",
        __FUNCTION__,
        SW_single_layer.layer_name);
      continue;
    }

    //  在遍历当前SW图层前增加一个记录处理要素ID的集合
    std::set<GIntBig> processedFIDs;

    //  在遍历当前SW图层前创建内存图层,为了可以动态操作输入数据图层中的要素而不会影响原始图层，例如删除要素。
    OGRLayer* poMemoryLayer = nullptr;
    {
      GDALDriver* poMemDriver = GetGDALDriverManager()->GetDriverByName("Memory");
      if (poMemDriver == nullptr)
      {
        single_frame_data_successful_logger->error("函数[{}]：无法获取内存驱动，跳过当前图层处理。", __FUNCTION__);
        continue;
      }
      GDALDataset* poMemDS = poMemDriver->Create("temp", 0, 0, 0, GDT_Unknown, nullptr);
      if (poMemDS == nullptr)
      {
        single_frame_data_successful_logger->error("函数[{}]：无法创建内存数据集，跳过当前图层处理。", __FUNCTION__);
        continue;
      }
      poMemoryLayer = poMemDS->CopyLayer(poSWLayer, "temp_layer");
      if (poMemoryLayer == nullptr)
      {
        single_frame_data_successful_logger->error("函数[{}]：无法复制图层到内存，跳过当前图层处理。", __FUNCTION__);
        GDALClose(poMemDS);
        continue;
      }
    }

    //  设置一个flag来表示当前图层是否在分类编码条件映射表中是否有映射条件（初始为false意味着没有找到）
    bool is_exist_classification_code_mapping_condition_flag = false;

    //  遍历分类编码条件映射表
    for (const auto& codeMapping : SW_Classification_Code_Conditional_Mapping_Lists_Json_entity.vSW_Classification_Code_Conditional_Mapping_Lists)
    {
#pragma region "2.1 分类编码条件映射表项必要检查"
      //  判断当前‘分类编码条件映射表项’是否需要映射
      if (codeMapping.SW_Classification_Code_Mapping_Condition_Flag != "yes")
      {
        single_frame_data_successful_logger->info("函数[{}]：当前'分类编码条件映射表项'中的'映射条件标志'不是'yes'。具体的'分类编码条件映射表项'如下所示：", __FUNCTION__);
        single_frame_data_successful_logger->info("\"Note\":\"{}\"", codeMapping.Note);
        single_frame_data_successful_logger->info("\"SW_Classification_Code_Mapping_Condition\":\"{}\"", codeMapping.SW_Classification_Code_Mapping_Condition);
        single_frame_data_successful_logger->info("\"SW_Classification_Code_Mapping_Condition_Flag\":\"{}\"", codeMapping.SW_Classification_Code_Mapping_Condition_Flag);
        single_frame_data_successful_logger->info("\"NJBDX_Classification_Code_Mapping_Condition\":\"{}\"", codeMapping.NJBDX_Classification_Code_Mapping_Condition);
        //  继续处理下一个'分类编码条件映射表项'
        continue;
      }
      //  TODO:检查表达式合法性
      //if (!is_valid_qgis_expression(codeMapping.SW_Classification_Code_Mapping_Condition))
      //{
      //  single_frame_data_successful_logger->info("函数[{}]：当前'分类编码条件映射表项'中的'SW_Classification_Code_Mapping_Condition'无效。具体的'分类编码条件映射表项'如下所示：", __FUNCTION__);
      //  single_frame_data_successful_logger->info("\"Note\":\"{}\"", codeMapping.Note);
      //  single_frame_data_successful_logger->info("\"SW_Classification_Code_Mapping_Condition\":\"{}\"", codeMapping.SW_Classification_Code_Mapping_Condition);
      //  single_frame_data_successful_logger->info("\"SW_Classification_Code_Mapping_Condition_Flag\":\"{}\"", codeMapping.SW_Classification_Code_Mapping_Condition_Flag);
      //  single_frame_data_successful_logger->info("\"NJBDX_Classification_Code_Mapping_Condition\":\"{}\"", codeMapping.NJBDX_Classification_Code_Mapping_Condition);
      //  //  继续处理下一个'分类编码条件映射表项'
      //  continue;

      //}
      //if (!is_valid_qgis_expression(codeMapping.NJBDX_Classification_Code_Mapping_Condition))
      //{
      //  single_frame_data_successful_logger->info("函数[{}]：当前'分类编码条件映射表项'中的'NJBDX_Classification_Code_Mapping_Condition'无效。具体的'分类编码条件映射表项'如下所示：", __FUNCTION__);
      //  single_frame_data_successful_logger->info("\"Note\":\"{}\"", codeMapping.Note);
      //  single_frame_data_successful_logger->info("\"SW_Classification_Code_Mapping_Condition\":\"{}\"", codeMapping.SW_Classification_Code_Mapping_Condition);
      //  single_frame_data_successful_logger->info("\"SW_Classification_Code_Mapping_Condition_Flag\":\"{}\"", codeMapping.SW_Classification_Code_Mapping_Condition_Flag);
      //  single_frame_data_successful_logger->info("\"NJBDX_Classification_Code_Mapping_Condition\":\"{}\"", codeMapping.NJBDX_Classification_Code_Mapping_Condition);
      //  //  继续处理下一个'分类编码条件映射表项'
      //  continue;
      //}
#pragma endregion

#pragma region "2.2 根据 SW_Classification_Code_Mapping_Condition 对 SW 要素进行筛选"

      /*
      Notes:杨小兵-2025-03-05

      1、在SW_Classification_Code_Mapping_Condition其中添加了SW图层名称信息，这里需要将SW图层名称和筛选条件两者进行区分（抽取出来）
      2、获取得到SW的图层名，判断当前处理的图层名称是否同当前矢量图层名称是否相同，如果相同则继续进行处理，反之则跳过
      3、SW_Classification_Code_Mapping_Condition字符串示例：ACHARE|\"CODE\"=140403 AND \"TYPE\"!='栈道'(需要单独提取ACHARE和\"CODE\"=140403 AND \"TYPE\"!='栈道')
      */

      //  判断当前图层名称是否同当前分类编码映射条件中指定的图层名称相同，需要针对不同的数据源进行不同的图层名称的抽取规则，并且设置成通用和非通用两种方式

      //  首先获取到分类编码中指定图层的名称
      std::string Current_SW_single_layer_name = extractBeforeDelimiter(codeMapping.SW_Classification_Code_Mapping_Condition);
      //  需要对当前图层名称进行过滤抽取，例如从DN101202_B_point中抽取出B
      std::string Current_SW_single_extract_layer_name = SW_Extract_LayerName(SW_single_layer.layer_name, SW_Layers_Fields_Info_Json_entity, single_frame_data_successful_logger);

      //  判断当前图层名称是否为空
      if (Current_SW_single_layer_name == "")
      {
        single_frame_data_successful_logger->error("函数[{}]：分类编码映射配置文件过滤条件 [{}] 中的图层名称为空，在分类编码映射配置文件过滤条件 [{}] 中应该设置为 CommonLayerName（表示图层名称通用）或者设置为具体的图层名称，请重新检查并且设置分类编码映射配置文件。",
              __FUNCTION__, codeMapping.SW_Classification_Code_Mapping_Condition, codeMapping.SW_Classification_Code_Mapping_Condition);
          //  跳过当前分类编码映射表项的处理，继续处理下一个分类编码表项。
          continue;
      }
      //  判断当前图层是否为通用形式
      else if (Current_SW_single_layer_name == "CommonLayerName")
      {
          //  如果在分类编码配置文件中指定的图层名称是通用形式，则不需要对图层名称进行进一步的判断，只需要设置一下标签默认当前图层在分类编码映射表中存在映射表项，但这样是否合理？（合理）
          is_exist_classification_code_mapping_condition_flag = true;
      }
      else if (Current_SW_single_layer_name != Current_SW_single_extract_layer_name)
      {
          //  跳过当前分类编码映射表项的处理，继续处理下一个分类编码表项。
          continue;
      }
      else
      {
          //  说明过滤条件中设置的图层名称和当前图层名称一致，则需要设置该标志意味着当前图层在分类编码映射表中存在映射关系
          is_exist_classification_code_mapping_condition_flag = true;
      }


      //  根据 SW_Classification_Code_Mapping_Condition 对 SW 要素进行筛选，例如可能为："\"CODE\"=140403 AND \"TYPE\"!='栈道'"
      const std::string ogrAttributeFilter = extractAfterDelimiter(codeMapping.SW_Classification_Code_Mapping_Condition);

      //  动态抽取过滤条件中所有字段名称
      std::vector<std::string> fieldNames = extractFieldNames(ogrAttributeFilter);

      //  创建要素指针用来在后续对符合过滤条件的要素进行处理
      OGRFeature* poFeature = nullptr;

      //  检查图层定义中是否包含所有过滤条件中引用的字段
      bool allFieldsExist = true;
      OGRFeatureDefn* poSWDefn = poMemoryLayer->GetLayerDefn();
      for (const auto& fieldName : fieldNames)
      {
        if (poSWDefn->GetFieldIndex(fieldName.c_str()) == -1)
        {
          single_frame_data_successful_logger->warn("函数[{}]：图层 [{}] 中不存在字段 [{}]，过滤条件 [{}] 将不会生效。",
            __FUNCTION__, SW_single_layer.layer_name, fieldName, ogrAttributeFilter);
          allFieldsExist = false;
          break;
        }
      }
      //  如果所有字段都存在，则设置属性过滤器；
      if (allFieldsExist)
      {
        if (poMemoryLayer->SetAttributeFilter(ogrAttributeFilter.c_str()) != OGRERR_NONE)
        {
          single_frame_data_successful_logger->error("函数[{}]：设置属性过滤器 [{}] 失败，属性过滤条件存在错误，请仔细检查是否存在拼写错误或者逻辑错误。",
            __FUNCTION__,
            ogrAttributeFilter);
          continue;
        }
      }
      else
      {
        //  所有字段中的部分字段不存在，不设置属性过滤器，并且对要素不进行处理，直接跳过。
        poMemoryLayer->SetAttributeFilter(nullptr);
        continue;
      }
      poMemoryLayer->ResetReading();
      //  对符合过滤条件的要素进行处理，这里需要将语义融合不成功的记录在pDestLayer图层中
      while ((poFeature = poMemoryLayer->GetNextFeature()) != nullptr)
      {
        //  先计算 NJBDX_Classification_Code_Mapping_Condition 的结果，例如 "11010202"，然后取其前两位 "11" 来决定映射到哪个 NJBDX 图层
        std::string classificationCodeResult = SW_extractClassificationCode(codeMapping.NJBDX_Classification_Code_Mapping_Condition);

        //  取前两位(示例：若得到 "11010202" -> "11")，根据需求可截取其他子串或使用正则等
        std::string targetLayerNumericalIdentification = "";
        std::string targetLayerEnglishName = "";
        if (classificationCodeResult.size() >= 2)
        {
          targetLayerNumericalIdentification = classificationCodeResult.substr(0, 2);  // 提取前两位
        }
        else
        {
          //  若长度不够则跳过
          single_frame_data_successful_logger->warn("函数[{}]：无法从 [{}] 提取有效的图层标识，跳过此要素。", __FUNCTION__, classificationCodeResult);
          
          //  将要素拷贝到目的图层中
          CopyOneFeatureToEmptyLayer(
              poFeature,
              pDestLayer,
              single_frame_data_failed_logger);

          //  处理失败的要素写入“语义融合失败图层”之后将其FID记录到集合中
          processedFIDs.insert(poFeature->GetFID());

          OGRFeature::DestroyFeature(poFeature);
          continue;
        }

        //  这里需要将图层标识号（例如11、12、13等等）同具体的英文名称映射从而获取目标图层的英文名称
        for (auto& NJBDX_current_layer : NJBDX_Layers_Fields_Info_Json_entity.vNJBDX_Layers_Fields)
        {
          if (NJBDX_current_layer.NJBDX_Layer_Numerical_Identification == targetLayerNumericalIdentification)
          {
            targetLayerEnglishName = NJBDX_current_layer.NJBDX_Layer_English_Name;
            break;
          }
        }
        if (targetLayerEnglishName == "")
        {
          //  说明没有找到对应的英文名称
          single_frame_data_successful_logger->warn("函数[{}]：无法从 [{}] 提取有效的图层英文名称，跳过此要素。", __FUNCTION__, classificationCodeResult);
          
          //  将要素拷贝到目的图层中
          CopyOneFeatureToEmptyLayer(
              poFeature,
              pDestLayer,
              single_frame_data_failed_logger);

          //  处理失败的要素写入“语义融合失败图层”之后将其FID记录到集合中
          processedFIDs.insert(poFeature->GetFID());

          OGRFeature::DestroyFeature(poFeature);
          continue;
        }

        //  找到目标 NJBDX 图层后，获取其 OGRLayer*
        OGRLayer* poNJBDXLayer = nullptr;
        for (auto& nLayerInfo : NJBDX_single_frame_data_all_layers_info_entity.vNJBDX_single_frame_data_single_layer_info)
        {
          
          
          //  这里需要根据几何类型来拼接图层名称
          const std::string NJBDXLayerName = targetLayerEnglishName + "_" + SW_OGRGeometryType2String(poSWLayer->GetGeomType());
          //  比较解析后的名称与映射中的名称
          bool nameMatches = (nLayerInfo.layer_name == NJBDXLayerName);

          //  比较几何类型（NJBDX图层的几何类型应该同SW图层的几何类型是相同的，不应该存在类型不同的情况）
          bool geometryMatches = (nLayerInfo.layer_geo_type == poSWLayer->GetGeomType());

          //  如果图层名称、图层几何类型两个条件都满足，那么当前的NJBDX图层即为需要的图层 
          if (nameMatches && geometryMatches)
          {
            poNJBDXLayer = nLayerInfo.polayer;
            break;
          }
        }
        if (!poNJBDXLayer)
        {
          single_frame_data_successful_logger->info("函数[{}]：在 NJBDX 数据源中未找到图层 [{}]，可能不需要为此图层写入要素。", __FUNCTION__, targetLayerEnglishName);
          
          //  将要素拷贝到目的图层中
          CopyOneFeatureToEmptyLayer(
              poFeature,
              pDestLayer,
              single_frame_data_failed_logger);

          //  处理失败的要素写入“语义融合失败图层”之后将其FID记录到集合中
          processedFIDs.insert(poFeature->GetFID());

          OGRFeature::DestroyFeature(poFeature);
          continue;
        }

        //  获取 NJBDX 图层的字段定义
        OGRFeatureDefn* poNJBDXDefn = poNJBDXLayer->GetLayerDefn();
        if (!poNJBDXDefn)
        {
          single_frame_data_successful_logger->error("函数[{}]：无法获取 NJBDX 图层 [{}] 的字段定义，跳过写入。", __FUNCTION__, targetLayerEnglishName);
          
          //  将要素拷贝到目的图层中
          CopyOneFeatureToEmptyLayer(
              poFeature,
              pDestLayer,
              single_frame_data_failed_logger);

          //  处理失败的要素写入“语义融合失败图层”之后将其FID记录到集合中
          processedFIDs.insert(poFeature->GetFID());

          OGRFeature::DestroyFeature(poFeature);
          //  继续处理下一个要素
          continue;
        }

        //  创建新要素并复制几何
        OGRFeature* poNewNjbdxFeature = OGRFeature::CreateFeature(poNJBDXDefn);
        if (!poNewNjbdxFeature)
        {
          single_frame_data_successful_logger->error("函数[{}]：图层 [{}]中创建 NJBDX 新要素失败，跳过写入。",
            __FUNCTION__,
            targetLayerEnglishName);
          
          //  将要素拷贝到目的图层中
          CopyOneFeatureToEmptyLayer(
              poFeature,
              pDestLayer,
              single_frame_data_failed_logger);

          //  处理失败的要素写入“语义融合失败图层”之后将其FID记录到集合中
          processedFIDs.insert(poFeature->GetFID());

          OGRFeature::DestroyFeature(poFeature);
          //  继续处理下一个要素
          continue;
        }
        //  将SW当前图层要素的几何定义赋值到新创建的NJBDX要素中
        SW_copy_geometry_from_source_to_target(poFeature, poNewNjbdxFeature, single_frame_data_successful_logger);



        //  日志说明现在开始处理SW数据源中当前图层的‘分类编码’字段属性值映射
        single_frame_data_successful_logger->info("<-----------------------------------第一步----------------------------------->");
        single_frame_data_successful_logger->info("函数[{}]：第一步--->开始处理 SW 图层：{} 中要素ID为 {} 的'分类编码'字段属性值映射。",
          __FUNCTION__,
          SW_single_layer.layer_name,
          poFeature->GetFID() + 1);

        //  写入分类编码字段（例如 NJBDX 图层中专门有 "CODE" 字段，即分类编码字段）
        SW_mapping_classification_code_field(poNewNjbdxFeature, "CODE", classificationCodeResult);

        //  日志说明现在结束处理SW数据源中当前图层的‘分类编码’字段属性值映射
        single_frame_data_successful_logger->info("函数[{}]：第一步--->结束处理 SW 图层：{} 中要素ID为 {} 的'分类编码'字段属性值映射。", __FUNCTION__, SW_single_layer.layer_name, poFeature->GetFID() + 1);
        single_frame_data_successful_logger->info("<-----------------------------------第一步----------------------------------->\n");





        //  日志说明现在开始处理SW数据源中当前图层的‘其他’字段属性值映射
        single_frame_data_successful_logger->info("<-----------------------------------第二步----------------------------------->");
        single_frame_data_successful_logger->info("函数[{}]：第二步--->开始处理 SW 图层：{} 中要素ID为 {} 的'其他'字段属性值映射。", __FUNCTION__, SW_single_layer.layer_name, poFeature->GetFID() + 1);

        //  获取SW_Classification_Code_Conditional_Mapping_Lists_Json_entity对应NJBDX的特定图层
        SW_Layers_Other_Fields_Mapping_t SW_Layers_Other_Fields_Mapping_entity;
        for (int i = 0; i < SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity.vSW_Layers_Other_Fields_Mapping.size(); i++)
        {
          //  这里的图层标识符可以是11、12、13等等
          if (SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity.vSW_Layers_Other_Fields_Mapping[i].NJBDX_Layer_English_Name == targetLayerEnglishName)
          {
            SW_Layers_Other_Fields_Mapping_entity = SW_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity.vSW_Layers_Other_Fields_Mapping[i];
            break;
          }
        }
        //  检查是否找到其他字段映射的配置信息
        if (!(SW_Layers_Other_Fields_Mapping_entity.vOther_Fields_Mapping.size()))
        {
          single_frame_data_successful_logger->error("函数[{}]：在SW_Other_Fields_Mapping_NJBDX_Other_Fields.json配置文件中没有找到NJBDX图层[{}]配置信息，跳过该要素的'其他'字段属性值映射。",
            __FUNCTION__,
            targetLayerEnglishName);


          //  将要素拷贝到目的图层中
          CopyOneFeatureToEmptyLayer(
            poFeature,
            pDestLayer,
            single_frame_data_failed_logger);

          //  处理失败的要素写入“语义融合失败图层”之后将其FID记录到集合中
          processedFIDs.insert(poFeature->GetFID());


          //  日志说明现在结束处理SW数据源中当前图层的‘其他’字段属性值映射
          single_frame_data_successful_logger->info("函数[{}]：第二步--->结束处理 SW 图层：{} 中要素ID为 {} 的'其他'字段属性值映射。",
            __FUNCTION__,
            SW_single_layer.layer_name,
            poFeature->GetFID() + 1);
          single_frame_data_successful_logger->info("-----------------------------------第二步----------------------------------->\n");
          //  处理下一个要素
          continue;
        }
        //  映射SW图层的其他字段属性
        SW_mapping_other_fields(
          poFeature,
          poNewNjbdxFeature,
          SW_Layers_Other_Fields_Mapping_entity,
          single_frame_data_successful_logger);

        //  日志说明现在结束处理SW数据源中当前图层的‘其他’字段属性值映射
        single_frame_data_successful_logger->info("函数[{}]：第二步--->结束处理 SW 图层：{} 中要素ID为 {} 的'其他'字段属性值映射。", __FUNCTION__, SW_single_layer.layer_name, poFeature->GetFID() + 1);
        single_frame_data_successful_logger->info("<-----------------------------------第二步----------------------------------->\n");



        //  写入要素到目标图层
        if (poNJBDXLayer->CreateFeature(poNewNjbdxFeature) != OGRERR_NONE)
        {
          //  将要素拷贝到目的图层中
          CopyOneFeatureToEmptyLayer(
            poFeature,
            pDestLayer,
            single_frame_data_failed_logger);

          //  处理失败的要素写入“语义融合失败图层”之后将其FID记录到集合中
          processedFIDs.insert(poFeature->GetFID());

          single_frame_data_successful_logger->error("函数[{}]：向 NJBDX 图层 [{}] 写入要素失败。", __FUNCTION__, targetLayerEnglishName);
        }

        // 写入 NJBDX 图层成功后将其FID记录到集合中
        processedFIDs.insert(poFeature->GetFID());
        //  释放要素资源
        OGRFeature::DestroyFeature(poNewNjbdxFeature);

        // 从内存图层中删除已处理的要素
        if (poMemoryLayer->DeleteFeature(poFeature->GetFID()) != OGRERR_NONE)
        {
          single_frame_data_successful_logger->error("函数[{}]：从内存图层删除要素 [{}] 失败。", __FUNCTION__, poFeature->GetFID() + 1);
        }

        OGRFeature::DestroyFeature(poFeature);
      } // end while

      //  重置过滤器
      poMemoryLayer->SetAttributeFilter(nullptr);
#pragma endregion

    } // end for codeMapping

    //  检查当前图层在分类编码条件映射表中是否存在映射表项
    if (!is_exist_classification_code_mapping_condition_flag)
    {
        //  如果当前图层在分类编码条件映射表中不存在映射表项，则输出日志信息
      single_frame_data_successful_logger->warn("函数[{}]：SW_Classification_Code_Conditional_Mapping_Lists.json配置文件中没有 SW 图层：{} 的映射信息。",
            __FUNCTION__, 
            SW_single_layer.layer_name);
    }



    //  处理未被任何映射条件处理的要素
    poMemoryLayer->SetAttributeFilter(nullptr);
    poMemoryLayer->ResetReading();
    OGRFeature* poRemainingFeature = nullptr;
    while ((poRemainingFeature = poMemoryLayer->GetNextFeature()) != nullptr)
    {
      //  如果该要素未在 processedFIDs 中，则拷贝到 pDestLayer
      if (processedFIDs.find(poRemainingFeature->GetFID()) == processedFIDs.end())
      {
        single_frame_data_successful_logger->info("函数[{}]：输入图层[{}]中要素ID为 {} 语义融合失败，将该要素写入到语义融合失败图层。", __FUNCTION__, poRemainingFeature->GetFID() + 1);
        CopyOneFeatureToEmptyLayer(poRemainingFeature, pDestLayer, single_frame_data_failed_logger);
      }
      OGRFeature::DestroyFeature(poRemainingFeature);
    }

    //  清理内存数据集
    GDALClose(poMemoryLayer->GetDataset());

#pragma endregion

    //  日志中提示当前正在处理的SW图层
    single_frame_data_successful_logger->info("<----------函数[{}]：结束处理 SW 中第 {} 个图层：{} (该图层GDAL几何类型：{} )---------->\n",
      __FUNCTION__,
      SW_layer_counter,
      SW_single_layer.layer_name,
      OGRGeometryTypeToName(SW_single_layer.layer_geo_type));
    SW_layer_counter++;
  } // end for SW_single_layer

  single_frame_data_successful_logger->info("<------------------函数[{}]日志开始--->进行 SW -> NJBDX 字段属性值映射------------------>",
    __FUNCTION__);
}

/**
 * 从输入字符串中提取第一个连续的8位数字字符串。
 *
 * @param input 要处理的输入字符串。
 * @return 提取到的8位数字字符串，如果未找到则返回空字符串。
 */
std::string qgs_SW_NJBDX_semantic_fusion::SW_extractClassificationCode(const std::string& input)
{
  // 定义匹配8位数字的正则表达式
  std::regex pattern("\\b\\d{8}\\b");
  std::smatch match;

  // 在输入字符串中搜索匹配
  if (std::regex_search(input, match, pattern)) {
    return match.str();
  }

  // 如果没有找到匹配，返回空字符串
  return "";
}

/**
 * @brief 执行 QGIS 表达式，将右侧表达式的计算结果写入到 poTargetFeature 的左侧字段中。
 *
 * 主要修改：在构造表达式上下文后，转换 poSourceFeature 为 QgsFeature 并调用
 * context.setFeature(qgsFeature); 解决表达式中对字段 "NAME" 的引用问题。
 */
std::string qgs_SW_NJBDX_semantic_fusion::SW_execute_single_qgis_expression(
  const std::string& expression,
  OGRFeature* poSourceFeature,
  OGRFeature* poTargetFeature,
  std::shared_ptr<spdlog::logger> logger)
{
  //  1. 参数有效性判断
  if (!poSourceFeature || !poTargetFeature || expression.empty())
  {
    logger->error("函数[{}]：无效参数(空Feature或空表达式)。", __FUNCTION__);
    return std::string();
  }

  //  2. 解析表达式：左侧为目标字段，右侧为 QGIS 表达式
  const size_t eqPos = expression.find('=');
  if (eqPos == std::string::npos)
  {
    logger->error("函数[{}]：表达式中缺少 '='，无法执行字段更新。表达式：{}", __FUNCTION__, expression);
    return std::string();
  }

  std::string leftField = expression.substr(0, eqPos);
  std::string rightExpr = expression.substr(eqPos + 1);

  //  去除左右两边空格
  auto trim = [](std::string& s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
  };
  trim(leftField);
  trim(rightExpr);

  if (leftField.empty() || rightExpr.empty())
  {
    logger->error("函数[{}]：目标字段或右侧表达式为空。原始表达式：{}", __FUNCTION__, expression);
    return std::string();
  }

  //  3. 构造 QGIS 表达式 (右侧)
  QgsExpression exp(QString::fromStdString(rightExpr));
  if (exp.hasParserError())
  {
    logger->error("函数[{}]：解析表达式失败：{}，错误信息：{}.",
      __FUNCTION__,
      rightExpr,
      exp.parserErrorString().toStdString());
    return std::string();
  }

  //  4. 构造表达式上下文
  QgsExpressionContext context;
  SW_fillExpressionContextFromOGRFeature(poSourceFeature, context);
  QgsFeature qgsFeature = convertOGRFeatureToQgsFeature(poSourceFeature);
  context.setFeature(qgsFeature);

  //  5. 进行表达式评估
  QVariant result = exp.evaluate(&context);
  if (exp.hasEvalError())
  {
    logger->error("函数[{}]：表达式计算出错：{}，错误信息：{} 。",
      __FUNCTION__,
      rightExpr,
      exp.evalErrorString().toStdString());
    return std::string();
  }

  //  6. 获取目标字段索引
  OGRFeatureDefn* pTargetDefn = poTargetFeature->GetDefnRef();
  if (!pTargetDefn)
  {
    logger->error("函数[{}]：目标要素的字段定义为空，无法写入字段。", __FUNCTION__);
    return std::string();
  }

  int targetFieldIndex = pTargetDefn->GetFieldIndex(leftField.c_str());
  if (targetFieldIndex < 0)
  {
    logger->error("函数[{}]：目标要素无此字段：{}.", __FUNCTION__, leftField);
    return std::string();
  }

  //  7. 处理结果：如果为 NULL，则跳过设置
  if (result.isNull())
  {
    logger->debug("函数[{}]：表达式结果为 NULL，跳过字段更新（为了满足 NJBDX 中的字段属性值来自 SW 多个不同字段的属性值，而不是唯一指定的字段，该调试信息可以直接忽略。）：{}，表达式：{}",
      __FUNCTION__,
      leftField,
      rightExpr);
    return std::string();
  }

  //  8. 如果结果非 NULL，写入目标字段
  switch (result.type())
  {
  case QMetaType::Int:
  case QMetaType::LongLong:
  case QMetaType::UInt:
  case QMetaType::ULongLong:
    poTargetFeature->SetField(targetFieldIndex, static_cast<GIntBig>(result.toLongLong()));
    break;
  case QMetaType::Double:
    poTargetFeature->SetField(targetFieldIndex, result.toDouble());
    break;
  case QMetaType::Bool:
    poTargetFeature->SetField(targetFieldIndex, result.toBool() ? 1 : 0);
    break;
  default:
  {
    std::string valStr = result.toString().toStdString();
    poTargetFeature->SetField(targetFieldIndex, valStr.c_str());
  }
  break;
  }

  //  9. 返回计算结果（以字符串形式返回）
  return result.toString().toStdString();
}

std::vector<std::string> qgs_SW_NJBDX_semantic_fusion::SW_execute_multiple_qgis_expressions(
  const std::vector<std::string>& expressions,
  OGRFeature* poSourceFeature,
  OGRFeature* poTargetFeature,
  std::shared_ptr<spdlog::logger> logger)
{
  std::vector<std::string> results;
  if (!poSourceFeature || !poTargetFeature || expressions.empty())
  {
    logger->error("函数[{}]：无效参数(空Feature或空表达式)。", __FUNCTION__);
    return results;
  }

  // 构造表达式上下文
  QgsExpressionContext context;
  SW_fillExpressionContextFromOGRFeature(poSourceFeature, context);
  QgsFeature qgsFeature = convertOGRFeatureToQgsFeature(poSourceFeature);
  context.setFeature(qgsFeature);

  OGRFeatureDefn* pTargetDefn = poTargetFeature->GetDefnRef();
  if (!pTargetDefn)
  {
    logger->error("函数[{}]：目标要素的字段定义为空。", __FUNCTION__);
    return results;
  }

  // 遍历所有表达式
  for (const std::string& expression : expressions)
  {
    // 找到表达式中的 '='，拆分左右部分
    const size_t eqPos = expression.find('=');
    if (eqPos == std::string::npos)
    {
      logger->error("函数[{}]：表达式中缺少 '='，无法执行字段更新。表达式：{}", __FUNCTION__, expression);
      results.push_back("");
      continue;
    }

    std::string leftField = expression.substr(0, eqPos);
    std::string rightExpr = expression.substr(eqPos + 1);

    // 去除左右两侧空格
    auto trim = [](std::string& s) {
      s.erase(0, s.find_first_not_of(" \t\r\n"));
      s.erase(s.find_last_not_of(" \t\r\n") + 1);
    };
    trim(leftField);
    trim(rightExpr);

    if (leftField.empty() || rightExpr.empty())
    {
      logger->error("函数[{}]：目标字段或右侧表达式为空。原始表达式：{}", __FUNCTION__, expression);
      results.push_back("");
      continue;
    }

    // 构造并计算表达式
    QgsExpression exp(QString::fromStdString(rightExpr));
    if (exp.hasParserError())
    {
      logger->error("函数[{}]：解析表达式失败：{}，错误信息：{}.",
        __FUNCTION__, rightExpr, exp.parserErrorString().toStdString());
      results.push_back("");
      continue;
    }

    QVariant result = exp.evaluate(&context);
    if (exp.hasEvalError())
    {
      logger->error("函数[{}]：表达式计算出错：{}，错误信息：{}.",
        __FUNCTION__, rightExpr, exp.evalErrorString().toStdString());
      results.push_back("");
      continue;
    }

    // 找到目标字段在 poTargetFeature 中的索引
    int targetFieldIndex = pTargetDefn->GetFieldIndex(leftField.c_str());
    if (targetFieldIndex < 0)
    {
      logger->error("函数[{}]：目标要素无此字段：{}.", __FUNCTION__, leftField);
      results.push_back("");
      continue;
    }

    // 根据计算结果的类型选择合适的 SetField 重载
    switch (result.type())
    {
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
      poTargetFeature->SetField(targetFieldIndex, static_cast<GIntBig>(result.toLongLong()));
      break;
    case QMetaType::Double:
      poTargetFeature->SetField(targetFieldIndex, result.toDouble());
      break;
    case QMetaType::Bool:
      poTargetFeature->SetField(targetFieldIndex, result.toBool() ? 1 : 0);
      break;
    default:
      poTargetFeature->SetField(targetFieldIndex, result.toString().toStdString().c_str());
      break;
    }

    // 保存此次表达式计算结果（以字符串形式）
    results.push_back(result.toString().toStdString());
  }
  return results;
}


/**
 * @brief 将 OGRFeature 转换为 QgsFeature，以便在表达式上下文中设置 feature，
 *        使得表达式中对字段名称（如 "NAME"）的引用能正确解析。
 */
QgsFeature qgs_SW_NJBDX_semantic_fusion::convertOGRFeatureToQgsFeature(OGRFeature* poSourceFeature)
{
  QgsFeature qgsFeature;
  if (!poSourceFeature)
    return qgsFeature;

  OGRFeatureDefn* poDefn = poSourceFeature->GetDefnRef();
  if (!poDefn)
    return qgsFeature;

  QgsFields fields;
  // 遍历 OGRFeature 的字段定义，构造 QgsFields
  for (int i = 0; i < poDefn->GetFieldCount(); i++)
  {
    OGRFieldDefn* fieldDefn = poDefn->GetFieldDefn(i);
    if (!fieldDefn)
      continue;
    // 根据 OGR 字段类型映射到 QVariant 类型
    QVariant::Type qType = QVariant::String;
    switch (fieldDefn->GetType())
    {
    case OFTInteger:
    case OFTInteger64:
      qType = QVariant::LongLong;
      break;
    case OFTReal:
      qType = QVariant::Double;
      break;
    default:
      qType = QVariant::String;
      break;
    }
    fields.append(QgsField(QString::fromUtf8(fieldDefn->GetNameRef()), qType));
  }
  /*
  Notes:杨小兵-2025-02-12

  1、QgsFeature 字段与属性的分离
    1.1 注意 QGIS 的 API 中，setFields() 默认不会为 feature 初始化属性值（即不会为内部属性数组分配空间），除非显式要求分配默认值。
  2、内部属性数组未初始化
    2.1 由于内部属性数组为空，当 QGIS 表达式引擎（或者其它内部逻辑）调用 feature 的 attribute(index) 方法时，就会发现没有任何属性（大小为 0），从而触发“Attribute index X out of bounds [0;0]”警告。
    2.2 尽管后续你通过 setAttribute() 填充了部分值，但如果内部数组本身没有被预先分配成与字段数一致的大小，每次对属性的访问都会检测到索引越界。
  3、多次访问导致重复警告
    3.1 QGIS 的表达式解析或其它内部代码在多次循环或多次尝试访问 feature 的每个字段时，都可能触发这种警告，因而日志中出现大量重复的“Attribute index ... out of bounds”消息。
  4、为了解决这些警告，可以确保在设置字段后，对 feature 的属性数组进行初始化。常用的做法有：
    4.1 在 setFields 时分配默认值，使用 QGIS API 提供的第二个参数，将默认值分配给属性：qgsFeature.setFields(fields, true);这样，内部属性数组会被初始化成与字段数相同的大小，每个位置赋以默认（空或 null）值。
  */
  qgsFeature.setFields(fields, true);

  // 设置 QgsFeature 的属性值，根据不同字段类型进行精细转换
  for (int i = 0; i < fields.count(); i++)
  {
    if (poSourceFeature->IsFieldSetAndNotNull(i))
    {
      OGRFieldDefn* fieldDefn = poDefn->GetFieldDefn(i);
      if (!fieldDefn)
        continue;

      switch (fieldDefn->GetType())
      {
      case OFTInteger:
      case OFTInteger64:
      {
        // 整数类型转换
        GIntBig intValue = poSourceFeature->GetFieldAsInteger64(i);
        qgsFeature.setAttribute(i, QVariant::fromValue(intValue));
        break;
      }
      case OFTReal:
      {
        // 浮点数类型转换
        double dValue = poSourceFeature->GetFieldAsDouble(i);
        qgsFeature.setAttribute(i, QVariant::fromValue(dValue));
        break;
      }
      case OFTString:
      {
        // 字符串类型转换
        QString strValue = QString::fromUtf8(poSourceFeature->GetFieldAsString(i));
        qgsFeature.setAttribute(i, strValue);
        break;
      }
      case OFTDate:
      case OFTTime:
      case OFTDateTime:
      {
        // 日期/时间类型转换：调用 GetFieldAsDateTime 获取年月日时分秒信息
        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, tzFlag = 0;
        if (poSourceFeature->GetFieldAsDateTime(i, &year, &month, &day, &hour, &minute, &second, &tzFlag))
        {
          QDate date(year, month, day);
          QTime time(hour, minute, second);
          QDateTime dateTime(date, time);
          qgsFeature.setAttribute(i, dateTime);
        }
        else
        {
          // 若解析失败，则回退为字符串转换
          QString strValue = QString::fromUtf8(poSourceFeature->GetFieldAsString(i));
          qgsFeature.setAttribute(i, strValue);
        }
        break;
      }
      default:
      {
        // 其它类型，采用字符串转换
        QString strValue = QString::fromUtf8(poSourceFeature->GetFieldAsString(i));
        qgsFeature.setAttribute(i, strValue);
        break;
      }
      }
    }
  }
  return qgsFeature;
}

/**
 * @brief 将源要素(来自SW)的几何复制到新要素(将写入NJBDX)
 *
 * @param poSourceFeature 源 OGRFeature 指针
 * @param poTargetFeature 目标 OGRFeature 指针
 */
void qgs_SW_NJBDX_semantic_fusion::SW_copy_geometry_from_source_to_target(
  OGRFeature* poSourceFeature,
  OGRFeature* poTargetFeature,
  std::shared_ptr<spdlog::logger> single_frame_data_logger)
{
  //  检查输入参数的有效性
  if (!poSourceFeature || !poTargetFeature)
  {
    return;
  }
  //  获取源要素的几何引用
  OGRGeometry* poSrcGeometry = poSourceFeature->GetGeometryRef();
  //  如果获取的源要素几何引用是有效的
  if (poSrcGeometry)
  {
    //  克隆几何并赋给目标要素
    OGRGeometry* poClonedGeometry = poSrcGeometry->clone();
    poTargetFeature->SetGeometry(poClonedGeometry);
    // poClonedGeometry 由 poTargetFeature 接管，无需手动 OGRGeometryFactory::destroyGeometry
  }
  else
  {
    //  记录错误信息
    single_frame_data_logger->info("函数[{}]：将SW要素的几何信息复制给NJBDX要素出现了错误。", __FUNCTION__);
  }
}

/**
 * @brief 将“分类编码”字段写入到目标要素中
 *
 * @param poTargetFeature 目标要素（将写入 NJBDX）
 * @param targetFieldName 目标字段名（如 "ClassificationCode"）
 * @param classificationCodeExpr 根据表达式计算得到的分类编码值
 */
void qgs_SW_NJBDX_semantic_fusion::SW_mapping_classification_code_field(
  OGRFeature* poTargetFeature,
  const std::string& targetFieldName,
  const std::string& classificationCodeExpr)
{
  //  检查输入参数的有效性
  if (!poTargetFeature)
  {
    return;
  }

  //  直接将字符串写入目标要素的指定字段，如果有更复杂的类型（如整数类型），应先进行转换，TODO:当前函数实现不完整
  poTargetFeature->SetField(targetFieldName.c_str(), classificationCodeExpr.c_str());
}

/**
 * @brief 将其他字段（非分类编码字段）从SW映射到NJBDX
 *
 * @param poSourceFeature 源 OGRFeature（SW）
 * @param poTargetFeature 目标 OGRFeature（NJBDX）
 * @param vOtherFieldsMapping 当前图层的“其他字段映射”规则
 * @param single_frame_data_logger 日志记录器
 */
void qgs_SW_NJBDX_semantic_fusion::SW_mapping_other_fields(
  OGRFeature* poSourceFeature,
  OGRFeature* poTargetFeature,
  const SW_Layers_Other_Fields_Mapping_t& SW_Layers_Other_Fields_Mapping_entity,
  std::shared_ptr<spdlog::logger> single_frame_data_logger)
{
  //  检查输入参数的有效性，函数调用者可以保证要素指针是有效的
  if (!poSourceFeature || !poTargetFeature)
  {
    return;
  }

  //  遍历当前图层在 “SW_Other_Fields_Mapping_NJBDX_Other_Fields.json” 中配置的映射关系
  for (const auto& mappingItem : SW_Layers_Other_Fields_Mapping_entity.vOther_Fields_Mapping)
  {
    //  如果 field_mapping_codition_flag != "yes"，说明该字段不需要进行映射
    if (mappingItem.field_mapping_codition_flag != "yes")
    {
      //  继续处理下一个字段映射
      continue;
    }

    ////  TODO:mappingItem.field_mapping_codition 也是 QGIS 表达式，需要先校验并执行
    //if (!is_valid_qgis_expression(mappingItem.field_mapping_codition))
    //{
    //  //  TODO:这里需要记录详细信息，说明整个字段映射条件
    //  single_frame_data_logger->warn(
    //    "函数[{}]：其他字段映射中的表达式无效，跳过映射。字段：{}，表达式：{}",
    //    __FUNCTION__,
    //    mappingItem.SW_field_name,
    //    mappingItem.field_mapping_codition);

    //  //  继续处理下一个字段映射
    //  continue;
    //}

    /*
    Notes:杨小兵-2025-02-12

    1、示例
    （1）假设我们有一个矢量要素，其中包含以下字段：
      population（整数，表示人口数量）
      city_name（字符串，表示城市名称）
    一个常见的 QGIS 表达式示例：
      "population" > 10000 AND "city_name" = 'Beijing'
    （2）解释：
      population > 10000 表示人口数量大于 1 万
      city_name = 'Beijing' 表示城市名称为 "Beijing"
      AND 表示逻辑与，只有当这两个条件都满足时，表达式才为 true

    2、示例2
    （1）如果换一个示例用于更新，比如：
      "population" = "population" + 500
    （2）解释：
      将要素的 population 字段更新为 原 population 值加上 500
      这种表达式在 QGIS 里经常用于字段计算器，也可以在 C++ 里通过 QgsExpression 动态求值并写回字段值。
    */
    std::string evalResult = SW_execute_single_qgis_expression(
      mappingItem.field_mapping_codition,
      poSourceFeature,
      poTargetFeature,
      single_frame_data_logger);


    // TODO:编写日志
  }
}

/**
 * @brief 从 OGRFeature 中取出字段名与字段值，并加入到 QGIS Expression 上下文 (QgsExpressionContext) 中。
 *
 * 因为 QGIS 3.28 版本中没有 QgsExpressionContext::setVariable()，
 * 所以使用 QgsExpressionContextScope 将每个字段以变量的形式加入上下文，
 * 但注意：表达式中引用变量时需要使用 '@' 前缀。
 *
 * @param poSourceFeature 传入的源要素(OGR)
 * @param context         用于表达式计算的 QGIS 表达式上下文
 */
void qgs_SW_NJBDX_semantic_fusion::SW_fillExpressionContextFromOGRFeature(
  OGRFeature* poSourceFeature,
  QgsExpressionContext& context)
{
  if (!poSourceFeature)
    return;

  OGRFeatureDefn* poDefn = poSourceFeature->GetDefnRef();
  if (!poDefn)
    return;

  QgsExpressionContextScope* scope = new QgsExpressionContextScope();
  int fieldCount = poDefn->GetFieldCount();
  for (int i = 0; i < fieldCount; ++i)
  {
    OGRFieldDefn* fieldDefn = poDefn->GetFieldDefn(i);
    if (!fieldDefn)
      continue;

    const char* fieldName = fieldDefn->GetNameRef();
    if (!fieldName)
      continue;

    if (!poSourceFeature->IsFieldSetAndNotNull(i))
      continue;

    OGRFieldType fieldType = fieldDefn->GetType();
    QVariant qValue;
    switch (fieldType)
    {
    case OFTInteger:
    case OFTInteger64:
      qValue = QVariant(poSourceFeature->GetFieldAsInteger64(i));
      break;
    case OFTReal:
      qValue = QVariant(poSourceFeature->GetFieldAsDouble(i));
      break;
    case OFTString:
      qValue = QVariant(QString::fromUtf8(poSourceFeature->GetFieldAsString(i)));
      break;
    case OFTDate:
    case OFTTime:
    case OFTDateTime:
      qValue = QVariant(QString::fromUtf8(poSourceFeature->GetFieldAsString(i)));
      break;
    default:
      qValue = QVariant(QString::fromUtf8(poSourceFeature->GetFieldAsString(i)));
      break;
    }
    scope->setVariable(QString(fieldName), qValue);
  }
  context.appendScope(scope);
}

//  将原图层中的所有要素拷贝到目标图层中（目前没有用到，该函数存在对源要素多次释放的问题，不要在该函数内容释放源要素）
void qgs_SW_NJBDX_semantic_fusion::CopyFeaturesToEmptyLayer(
    OGRLayer* pSrcLayer, 
    OGRLayer* pDestLayer, 
    std::shared_ptr<spdlog::logger> logger)
{
    //  检查图层指针有效性
    if (pSrcLayer == nullptr || pDestLayer == nullptr)
    {
        logger->error("函数[{}]：无效的源图层或目标图层指针。",  __FUNCTION__);
        return;
    }

    //  重置图层要素读取顺序
    pSrcLayer->ResetReading();
    //  对符合过滤条件的要素进行处理
    OGRFeature* poSrcFeature = NULL;
    while ((poSrcFeature = pSrcLayer->GetNextFeature()) != nullptr)
    {
        logger->info("函数[{}]：开始处理 SW 图层：{} 中要素ID为 {} 的要素。", 
        __FUNCTION__, 
        pSrcLayer->GetName(), 
        poSrcFeature->GetFID() + 1);

        //  根据目标图层的要素定义创建一个新的要素
        OGRFeature* poDestFeature = OGRFeature::CreateFeature(pDestLayer->GetLayerDefn());
        if (poDestFeature == nullptr)
        {
            logger->error("函数[{}]：在目的图层{}中创建目标要素失败。", __FUNCTION__, pDestLayer->GetName());
            OGRFeature::DestroyFeature(poSrcFeature);

            logger->info("函数[{}]：结束处理 SW 图层：{} 中要素ID为 {} 的要素。", 
            __FUNCTION__, 
            pSrcLayer->GetName(), 
            poSrcFeature->GetFID() + 1);

            return;
        }
    
        //  复制几何信息：获取源要素的几何并克隆到新要素中
        OGRGeometry* poGeometry = poSrcFeature->GetGeometryRef();
        if (poGeometry != nullptr)
        {
            OGRGeometry* poClonedGeom = poGeometry->clone();
            if (poClonedGeom != nullptr)
            {
                poDestFeature->SetGeometry(poClonedGeom);
                //  注意：克隆的几何对象已交由poDestFeature管理，不需要额外释放
            }
            else
            {
                logger->warn("函数[{}]：从源图层{}中将要素的几何信息克隆到目的图层{}要素中失败，几何信息未复制。", 
                    __FUNCTION__, 
                    pSrcLayer->GetName(),
                    pDestLayer->GetName());
            }
        }
    
        //  复制字段属性：遍历所有字段并复制
        int fieldCount = poSrcFeature->GetFieldCount();
        for (int i = 0; i < fieldCount; ++i)
        {
            poDestFeature->SetField(i, poSrcFeature->GetRawFieldRef(i));
        }
    
        //  将新要素写入到目标图层中
        if (pDestLayer->CreateFeature(poDestFeature) != OGRERR_NONE)
        {
            logger->error("函数[{}]：向目的图层{}写入要素失败。", __FUNCTION__, pDestLayer->GetName());
        }
        else
        {
            logger->info("函数[{}]：成功写入一个要素到目标图层{}。", __FUNCTION__, pDestLayer->GetName());
        }
    
        //  清理内存：销毁创建的要素
        OGRFeature::DestroyFeature(poSrcFeature);
        OGRFeature::DestroyFeature(poDestFeature);

        logger->info("函数[{}]：结束处理 SW 图层：{} 中要素ID为 {} 的要素。", 
        __FUNCTION__, 
        pSrcLayer->GetName(), 
        poSrcFeature->GetFID() + 1);
    }

}

//  将原图层中的指定要素要素拷贝到目标图层中
void qgs_SW_NJBDX_semantic_fusion::CopyOneFeatureToEmptyLayer(
  OGRFeature* poSrcFeature,
  OGRLayer* pDestLayer,
  std::shared_ptr<spdlog::logger> logger)
{
  if (!pDestLayer)
  {
    logger->error("函数[{}]：目标图层指针为空，不是有效目标图层。", __FUNCTION__);
    return;
  }

  if (!poSrcFeature)
  {
    logger->error("函数[{}]：从源图层[{}]中获取到的要素不是有效的（空要素指针）。",
      __FUNCTION__,
      pDestLayer->GetName());
    return;
  }

  // 根据目标图层的要素定义创建一个新的要素
  OGRFeature* poDestFeature = OGRFeature::CreateFeature(pDestLayer->GetLayerDefn());
  if (!poDestFeature)
  {
    logger->error("函数[{}]：在目标图层[{}]中创建一个新的目标要素失败。",
      __FUNCTION__,
      pDestLayer->GetName());
    return;
  }

  // 克隆几何
  OGRGeometry* poGeometry = poSrcFeature->GetGeometryRef();
  if (poGeometry)
  {
    OGRGeometry* poClonedGeom = poGeometry->clone();
    if (!poClonedGeom)
    {
      //  这里源图层和目标图层都是相同名称
      logger->warn("函数[{}]：从输入图层[{}]中克隆要素ID为 {} 矢量要素的几何信息失败，该要素几何信息未复制。",
        __FUNCTION__,
        pDestLayer->GetName(),
        poSrcFeature->GetFID() + 1);
    }
    else
    {
      poDestFeature->SetGeometry(poClonedGeom);
    }
  }

  // 复制字段属性
  int fieldCount = poSrcFeature->GetFieldCount();
  for (int i = 0; i < fieldCount; ++i)
  {
    poDestFeature->SetField(i, poSrcFeature->GetRawFieldRef(i));
  }

  // 写入到目标图层
  if (pDestLayer->CreateFeature(poDestFeature) != OGRERR_NONE)
  {
    //  这里源图层和目标图层都是相同名称
    logger->error("函数[{}]：向目标图层[{}]写入一个输入图层[{}]中要素ID为 {} 的矢量要素失败。",
      __FUNCTION__,
      pDestLayer->GetName(),
      pDestLayer->GetName(),
      poSrcFeature->GetFID() + 1);
  }
  else
  {
    //  这里源图层和目标图层都是相同名称
    logger->info("函数[{}]：向目标图层[{}]写入一个输入图层[{}]中要素ID为 {} 的矢量要素成功。",
      __FUNCTION__,
      pDestLayer->GetName(),
      pDestLayer->GetName(),
      poSrcFeature->GetFID() + 1);
  }

  //  销毁目标要素（写入后即可销毁）
  OGRFeature::DestroyFeature(poDestFeature);

  //  这里不要销毁 poSrcFeature，让外部循环或外部函数自己调用 DestroyFeature(poSrcFeature)
}

#pragma endregion






//  获取SW单个分幅数据集的信息
void qgs_SW_NJBDX_semantic_fusion::SW_Create_single_frame_data_all_layers_info(
  GDALDataset* poSingleFrameSWDataSet,
  SW_single_frame_data_all_layers_info_t& SW_single_frame_data_all_layers_info_entity,
  std::shared_ptr<spdlog::logger> single_frame_data_logger)
{

  if (!poSingleFrameSWDataSet)
  {
    //  数据集指针为空则设置成默认值
    SW_single_frame_data_all_layers_info_entity.SW_single_frame_data_poDS = NULL;
    single_frame_data_logger->error("函数[{}]：数据集指针为空。", __FUNCTION__);
    return;
  }
  else
  {
    //  设置数据集指针
    SW_single_frame_data_all_layers_info_entity.SW_single_frame_data_poDS = poSingleFrameSWDataSet;
  }

  //  获取并设置数据集路径
  const char* dataset_path = poSingleFrameSWDataSet->GetDescription();
  if (!dataset_path)
  {
    //  获取不到则设置成默认值
    SW_single_frame_data_all_layers_info_entity.SW_single_frame_data_path = "";
    single_frame_data_logger->error("函数[{}]：获取数据集路径失败。", __FUNCTION__);
    return;
  }
  else
  {
    //  获取得到则赋值
    SW_single_frame_data_all_layers_info_entity.SW_single_frame_data_path = dataset_path;
  }

  //  获取图层数量
  int layer_count = poSingleFrameSWDataSet->GetLayerCount();

  //  遍历每个图层
  for (int i = 0; i < layer_count; ++i)
  {
    //  获取图层指针
    OGRLayer* layer = poSingleFrameSWDataSet->GetLayer(i);
    if (!layer)
    {
      single_frame_data_logger->error("函数[{}]：获取第{}个图层失败，已经跳过该图层的处理。", __FUNCTION__, i);
      //  跳过无效图层
      continue; 
    }
      
    //  创建单个图层信息结构体变量
    SW_single_frame_data_single_layer_info_t layer_info;
    //  获取并设置图层名称
    const char* layer_name = layer->GetName();
    if (!layer_name)
    {
      single_frame_data_logger->error("函数[{}]：获取第{}个图层的名称失败，已经跳过该图层的处理。", __FUNCTION__, i);
      layer_info.layer_name = "";
      continue;
    }
    else
    {
      layer_info.layer_name = layer_name;
    }
    

    //  获取并设置几何类型
    OGRwkbGeometryType geom_type = layer->GetGeomType();
    layer_info.layer_geo_type = geom_type;

    //  赋值图层指针到自定义结构体中
    layer_info.polayer = layer;

    //  获取空间参考
    OGRSpatialReference* poSRS = layer->GetSpatialRef();
    if (poSRS)
    {
      layer_info.poSRS = poSRS;
    }
    else
    {
      single_frame_data_logger->error("函数[{}]：获取第{}个图层的空间参考失败，已经跳过该图层的处理。", __FUNCTION__, i);
      layer_info.poSRS = nullptr;
      continue;
    }

    //  将图层信息添加到向量中
    SW_single_frame_data_all_layers_info_entity.vSW_single_frame_data_single_layer_info.push_back(layer_info);
  }
}

/**
 * @brief 根据SW单个分幅数据中所有图层的信息创建一个空的输出数据集，
 *        新数据集中每个图层拥有相同的图层名称、几何类型、空间参考和字段定义，但不包含任何要素。
 *
 * @param srcData 已经读取到的SW单个分幅数据的所有图层信息
 * @param newDatasetPath 新数据集的输出路径（例如ESRI Shapefile路径，注意对于Shapefile每个图层对应一个文件）
 * @param pDriver 用于创建数据集的GDALDriver指针（例如通过GetGDALDriverManager()->GetDriverByName("ESRI Shapefile")获得）
 * @param logger 日志器指针，用于记录过程中的信息和错误
 * @return GDALDataset* 指向新创建空数据集的指针，如果创建失败则返回nullptr
 */
void qgs_SW_NJBDX_semantic_fusion::CreateEmptyDatasetFromSWInfo(
  const SW_single_frame_data_all_layers_info_t& SW_single_frame_data_all_layers_info_entity,
  SW_single_frame_data_all_layers_info_t& SW_single_frame_data_all_layers_info_empty_entity,
  const std::string& newDatasetPath,
  GDALDriver* pDriver,
  std::shared_ptr<spdlog::logger> logger)
{
  //  判断驱动是否有效
  if (!pDriver)
  {
    logger->error("函数[{}]：无效的GDAL驱动指针。", __FUNCTION__);
    return;
  }

  //  使用GDAL驱动创建一个新的数据集。注意：对于矢量数据，宽度、高度以及波段数一般无意义，传0即可，GDT_Unknown表示数据类型无要求，后面的参数可以为NULL。
  GDALDataset* pNewDataset = pDriver->Create(newDatasetPath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
  if (!pNewDataset)
  {
    logger->error("函数[{}]：无法在路径 {} 创建新数据集。", __FUNCTION__, newDatasetPath);
    return;
  }
  //  数据集创建成功之后写入到自定义结构体中
  SW_single_frame_data_all_layers_info_empty_entity.SW_single_frame_data_path = newDatasetPath;
  SW_single_frame_data_all_layers_info_empty_entity.SW_single_frame_data_poDS = pNewDataset;

  SW_single_frame_data_single_layer_info_t SW_current_single_frame_data_single_layer_info_entity;
  //  遍历源数据中的每个图层，按原有结构创建对应的空图层
  for (const auto& layerInfo : SW_single_frame_data_all_layers_info_entity.vSW_single_frame_data_single_layer_info)
  {
    //  判断原始图层指针是否有效
    if (!layerInfo.polayer)
    {
      logger->error("函数[{}]：图层 {} 的指针为空，跳过该图层创建。", __FUNCTION__, layerInfo.layer_name);
      continue;
    }

    //  获取原始图层的空间参考和几何类型
    OGRSpatialReference* pSRS = layerInfo.poSRS;
    OGRwkbGeometryType geomType = layerInfo.layer_geo_type;

    //  将图层信息写入到自定义结构体中
    SW_current_single_frame_data_single_layer_info_entity.layer_name = layerInfo.layer_name;
    SW_current_single_frame_data_single_layer_info_entity.layer_geo_type = layerInfo.layer_geo_type;
    SW_current_single_frame_data_single_layer_info_entity.poSRS = layerInfo.poSRS;

    // 检查新数据集中是否存在同名图层，若存在则先删除所有同名图层
    int nLayers = pNewDataset->GetLayerCount();
    for (int i = nLayers - 1; i >= 0; i--)
    {
      OGRLayer* existLayer = pNewDataset->GetLayer(i);
      if (existLayer && (layerInfo.layer_name == existLayer->GetName()))
      {
        if (pNewDataset->DeleteLayer(i) == OGRERR_NONE)
        {
          logger->info("函数[{}]：已删除已存在的图层 {}。", __FUNCTION__, layerInfo.layer_name);
        }
        else
        {
          logger->error("函数[{}]：删除已存在的图层 {} 失败。", __FUNCTION__, layerInfo.layer_name);
        }
      }
    }

    //  在新数据集中创建一个图层，名称、空间参考和几何类型均与原图层相同
    OGRLayer* pNewLayer = pNewDataset->CreateLayer(layerInfo.layer_name.c_str(), pSRS, geomType, nullptr);
    if (!pNewLayer)
    {
      logger->error("函数[{}]：无法在新数据集中创建图层 {}。", __FUNCTION__, layerInfo.layer_name);
      continue;
    }

    //  从原始图层获取字段定义（字段信息）
    OGRFeatureDefn* pSrcFDefn = layerInfo.polayer->GetLayerDefn();
    if (pSrcFDefn)
    {
      int fieldCount = pSrcFDefn->GetFieldCount();
      for (int i = 0; i < fieldCount; ++i)
      {
        OGRFieldDefn* pFieldDefn = pSrcFDefn->GetFieldDefn(i);
        if (pFieldDefn)
        {
          //  将字段定义添加到新图层中
          if (pNewLayer->CreateField(pFieldDefn) != OGRERR_NONE)
          {
            logger->error("函数[{}]：在图层 {} 中创建字段 {} 失败。",
              __FUNCTION__,
              layerInfo.layer_name,
              pFieldDefn->GetNameRef());
          }
        }
      }
      SW_current_single_frame_data_single_layer_info_entity.polayer = pNewLayer;
    }
    else
    {
      logger->warn("函数[{}]：图层 {} 中没有获取到字段定义。",
        __FUNCTION__,
        layerInfo.layer_name);
      SW_current_single_frame_data_single_layer_info_entity.polayer = NULL;
    }

    //  手动创建 .cpg 文件，确保写入编码信息（处理中文路径问题）,将 newDatasetPath 转换为 QString
    QString qOutputPath = QString::fromStdString(newDatasetPath);
    //  将 SW图幅名称转换为 QString，并拼接 .cpg 后缀
    QString qCpgFilePath = qOutputPath + "/" + QString::fromStdString(layerInfo.layer_name) + ".cpg";
    //  转换为本地8位编码，解决 Windows 下中文路径问题
    QByteArray baCpgFilePath = qCpgFilePath.toLocal8Bit();

    std::ofstream cpgFile(baCpgFilePath.constData());
    if (!cpgFile.is_open())
    {
      logger->error("函数[{}]：无法创建 .cpg 文件：{}", __FUNCTION__, qCpgFilePath.toStdString());
    }
    else
    {
      cpgFile << "UTF-8";  // 写入编码信息
      cpgFile.close();
      logger->info("函数[{}]：成功创建 .cpg 文件：{}", __FUNCTION__, qCpgFilePath.toStdString());
    }

    SW_single_frame_data_all_layers_info_empty_entity.vSW_single_frame_data_single_layer_info.push_back(SW_current_single_frame_data_single_layer_info_entity);
  }

  //  Flush 缓存并关闭数据集，释放文件锁
  pNewDataset->FlushCache();
  return;
}

//  检查SW数据集中的有效图层是否为空
bool qgs_SW_NJBDX_semantic_fusion::SW_Check_single_frame_data_all_layers_info(
  const SW_single_frame_data_all_layers_info_t& SW_single_frame_data_all_layers_info_entity,
  std::shared_ptr<spdlog::logger> single_frame_data_logger)
{
  //  TODO：添加更为详细的检查
  if (SW_single_frame_data_all_layers_info_entity.vSW_single_frame_data_single_layer_info.size() == 0)
  {
    return false;
  }
  return true;
}

//  释放SW单个分幅数据集资源
void qgs_SW_NJBDX_semantic_fusion::SW_Close_single_frame_data_all_layers_info(
  SW_single_frame_data_all_layers_info_t& SW_single_frame_data_all_layers_info_entity)
{
  if (SW_single_frame_data_all_layers_info_entity.SW_single_frame_data_poDS)
  {
    GDALClose(SW_single_frame_data_all_layers_info_entity.SW_single_frame_data_poDS);
  }
  return;
}

void qgs_SW_NJBDX_semantic_fusion::SW_Delete_And_Close_single_frame_data_all_layers_info(
    SW_single_frame_data_all_layers_info_t& SW_single_frame_data_all_layers_info_entity)
{
    GDALDataset* poDataset = SW_single_frame_data_all_layers_info_entity.SW_single_frame_data_poDS;
    if (poDataset)
    {
        // 倒序遍历图层，避免在删除图层后导致后面图层索引错乱
        for (int i = poDataset->GetLayerCount() - 1; i >= 0; --i)
        {
            OGRLayer* poLayer = poDataset->GetLayer(i);
            if (poLayer)
            {
                // 获取要素数量
                GIntBig nFeatureCount = poLayer->GetFeatureCount();
                // 如果图层要素数为 0，则删除该图层
                if (nFeatureCount == 0)
                {
                    // 删除图层。注意需要确认当前驱动是否支持 DeleteLayer
                    poDataset->DeleteLayer(i);
                }
            }
        }

        // 最后关闭数据集并将指针置空
        GDALClose(poDataset);
        SW_single_frame_data_all_layers_info_entity.SW_single_frame_data_poDS = nullptr;
    }
}





//  创建NJBDX单个分幅数据集的信息
bool qgs_SW_NJBDX_semantic_fusion::SW_Create_NJBDX_all_layers_info(
  const SW_single_frame_data_all_layers_info_t& SW_single_frame_data_all_layers_info_entity,
  const SW_Layers_Mapping_NJBDX_Layers_Json_t& SW_Layers_Mapping_NJBDX_Layers_Json_entity,
  const SW_Layers_Fields_Info_Json_t& SW_Layers_Fields_Info_Json_entity,
  const SW_NJBDX_Layers_Fields_Info_Json_t& NJBDX_Layers_Fields_Info_Json_entity,
  const std::string& NJBDX_str_output_data_path,
  SW_NJBDX_single_frame_data_all_layers_info_t& NJBDX_single_frame_data_all_layers_info_entity,
  std::shared_ptr<spdlog::logger> single_frame_data_logger)
{
  //  日志：开始
  single_frame_data_logger->info("********函数[{}]：日志开始--->在目录{}中创建 NJBDX Shapefile 图层********", __FUNCTION__, NJBDX_str_output_data_path);

  NJBDX_single_frame_data_all_layers_info_entity.NJBDX_single_frame_data_path = NJBDX_str_output_data_path;

#pragma region "1 设置GDAL并且获取驱动器"
  //  这告诉 GDAL 文件名使用 UTF-8 编码
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  //  自动检测 DBF 编码
  CPLSetConfigOption("SHAPE_ENCODING", "");
  //  注册所有 GDAL 驱动
  GDALAllRegister();
  const char* pszShpDriverName = "ESRI Shapefile";
  GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
  if (poDriver == NULL)
  {
    single_frame_data_logger->error("函数[{}]：获取ESRI Shapefile矢量驱动器失败。", __FUNCTION__);
    return false;
  }
#pragma endregion

#pragma region "2 根据SW图层信息遍历图层信息"
  //  用于存储已创建的 “NJBDX 图层英文名 -> GDALDataset*” 以避免重复创建
  std::map<std::string, GDALDataset*> createdDatasets;

  //  遍历所有 SW 图层信息
  for (const auto& SWLayerInfo : SW_single_frame_data_all_layers_info_entity.vSW_single_frame_data_single_layer_info)
  {
    //  获取得到当前SW单个图层的名称例如（DN09490622_D_line）
    const std::string& SWLayerName = SWLayerInfo.layer_name;
    //  获取得到当前SW单个图层的几何类型
    OGRwkbGeometryType SW_layer_geo_type = SWLayerInfo.layer_geo_type;

#pragma region "2.1 在SW_Layers_Mapping_NJBDX_Layers.json中找到对应的映射项"
    //  2.1 在映射表中查找对应的 NJBDX 图层集合
    SW_layers_mapping_NJBDX_layers_t MappingItem;
    //  提取SW图层名称(DN09490622_D_line--->D，根据不同的数据源将会有不同的抽取规则)
    std::string SW_extracted_layername = SW_Extract_LayerName(SWLayerName, SW_Layers_Fields_Info_Json_entity, single_frame_data_logger);

    //  循环遍历所有 SW_mapping_NJBDX 图层信息项
    for (auto& mapItem : SW_Layers_Mapping_NJBDX_Layers_Json_entity.vSW_layers_mapping_NJBDX_layers)
    {
      //  每次迭代都重新初始化匹配标志
      bool layer_name_match_flag = false;
      bool layer_geo_type_match_flag = false;

      //  使用图层的英文名称判断是否匹配
      if (mapItem.SW_Layer.SW_layer_english_name == SW_extracted_layername)
      {
        layer_name_match_flag = true;
      }
      //  判断图层的几何类型是否匹配
      if (mapItem.SW_Layer.SW_layer_type == SW_layer_geo_type)
      {
        layer_geo_type_match_flag = true;
      }

      //  如果图层的名称和几何类型都匹配，则获取到正确的映射信息
      if (layer_name_match_flag && layer_geo_type_match_flag)
      {
        MappingItem = mapItem;
        break;
      }
    }
    //  如果没有获取到对应的图层映射信息，说明配置文件填写不完整
    if (MappingItem.vNJBDX_Mapping_Layers.size() == 0)
    {
      //  未在映射表中找到对应的 NJBDX 图层
      single_frame_data_logger->info("函数[{}]：SW 图层[{}]在映射表中找到了空的对应 NJBDX 图层，已跳过该图层的处理", __FUNCTION__, SWLayerName);
      continue;
    }
#pragma endregion

#pragma region "2.2 根据SW图层的映射关系创建新的NJBDX图层"
    //  循环遍历该 SW 图层映射到的多个 NJBDX 图层
    for (auto& singleNJBDXLayerMapping : MappingItem.vNJBDX_Mapping_Layers)
    {
      //  获取当前NJBDX图层信息中的英文名称（TODO:这里需要进行修改，从11、12等修改为具体的英文名称）
      const std::string& NJBDXLayerEnglishName = singleNJBDXLayerMapping.NJBDX_layer_english_name;
      //  将英文名称和图层几何类型结合起来（TODO:这里需要进行修改，从11_Point等修改为英文名称_Point）
      const std::string& NJBDXLayerEnglishName_GeoType = NJBDXLayerEnglishName + "_" + SW_OGRGeometryType2String(singleNJBDXLayerMapping.NJBDX_layer_type);

      //  判断是否已经创建过同名的 NJBDX 图层（Shapefile）
      GDALDataset* poExistingDataset = nullptr;
      auto itFound = createdDatasets.find(NJBDXLayerEnglishName_GeoType);
      if (itFound != createdDatasets.end())
      {
        poExistingDataset = itFound->second;
      }
      //  如果还没创建，执行创建
      if (!poExistingDataset)
      {
        //  拼接输出 Shapefile 路径
        std::string shpFilePath = NJBDX_str_output_data_path + "/" + NJBDXLayerEnglishName_GeoType + ".shp";

        //  如果已存在同名文件，先删除（可按需决定是否覆盖）
        if (CPLCheckForFile((char*)shpFilePath.c_str(), nullptr))
        {
          if (poDriver->Delete(shpFilePath.c_str()) != CE_None)
          {
            single_frame_data_logger->info("函数[{}]：删除已存在Shapefile文件失败：{}", __FUNCTION__, shpFilePath);
            //  继续处理下一个映射图层
            continue;
          }
          single_frame_data_logger->info("函数[{}]：覆盖删除旧Shapefile文件：{}", __FUNCTION__, shpFilePath);
        }

        // 设置创建选项，指定 CPG 选项为 UTF-8
        char** papszOptions = nullptr;
        papszOptions = CSLSetNameValue(papszOptions, "CPG", "UTF-8");

        //  创建新的 Shapefile 数据集
        GDALDataset* poNewDS = poDriver->Create(shpFilePath.c_str(), 0, 0, 0, GDT_Unknown, papszOptions);
        if (!poNewDS)
        {
          single_frame_data_logger->error("函数[{}]：创建Shapefile文件失败：{}", __FUNCTION__, shpFilePath);
          //  继续处理下一个映射图层
          continue;
        }

        //  设置空间参考（沿用SW图层的 poSRS）
        OGRSpatialReference* poRef = nullptr;
        if (SWLayerInfo.poSRS)
        {
          poRef = SWLayerInfo.poSRS->Clone();
        }

        //  在该 Shapefile 数据集中创建图层
        OGRLayer* poNewLayer = poNewDS->CreateLayer(NJBDXLayerEnglishName_GeoType.c_str(), poRef, SW_layer_geo_type, nullptr);
        if (!poNewLayer)
        {
          single_frame_data_logger->error("函数[{}]：在 {} 创建图层 [{}] 失败！", __FUNCTION__, shpFilePath, NJBDXLayerEnglishName_GeoType);
          //  之后释放创建出来的空间引用
          if (poRef) poRef->Release();
          GDALClose(poNewDS);
          //  继续处理下一个映射图层
          continue;
        }
        //  poRef 在 CreateLayer 之后释放
        if (poRef)
        {
          poRef->Release();
          poRef = nullptr;
        }

        //  NJBDX_Layers_Fields_Info_Json_entity 中，根据 NJBDXLayerEnglishName_GeoType 匹配字段配置，并创建字段
        bool bFindFieldCfg = false;
        //  创建NJBDX中当前图层的字段信息
        for (auto& layersFields : NJBDX_Layers_Fields_Info_Json_entity.vNJBDX_Layers_Fields)
        {
          //  获取得到想要的NJBDX的图层信息
          if (layersFields.NJBDX_Layer_English_Name == NJBDXLayerEnglishName)
          {
            bFindFieldCfg = true;
            //  遍历字段配置，逐一创建字段
            for (auto& fieldDef : layersFields.vFields)
            {
              OGRFieldDefn oFieldDefn(fieldDef.english_field_name.c_str(), fieldDef.field_type);
              oFieldDefn.SetWidth(fieldDef.field_width);
              oFieldDefn.SetPrecision(fieldDef.field_precision);

              if (poNewLayer->CreateField(&oFieldDefn) != OGRERR_NONE)
              {
                single_frame_data_logger->error("函数{}：图层 [{}] 创建字段 [{}] 失败！",
                  __FUNCTION__, NJBDXLayerEnglishName, fieldDef.english_field_name);
              }
              else
              {
                single_frame_data_logger->info("函数{}：图层 [{}] 成功创建字段 [{}]",
                  __FUNCTION__, NJBDXLayerEnglishName, fieldDef.english_field_name);
              }
            }
            //  找到匹配就不再往下搜
            break;
          }
        }
        //  如果在配置文件中没有找到对应图层的字段信息，则给出提示
        if (!bFindFieldCfg)
        {
          single_frame_data_logger->warn("函数[{}]：NJBDX图层 [{}] 在字段配置表中无匹配项，跳过字段创建。", __FUNCTION__, NJBDXLayerEnglishName);
        }


        //  手动创建 .cpg 文件，确保写入编码信息（处理中文路径问题）,将 NJBDX_str_output_data_path 转换为 QString
        QString qOutputPath = QString::fromStdString(NJBDX_str_output_data_path);
        //  将 NJBDXLayerEnglishName_GeoType 转换为 QString，并拼接 .cpg 后缀
        QString qCpgFilePath = qOutputPath + "/" + QString::fromStdString(NJBDXLayerEnglishName_GeoType) + ".cpg";
        //  转换为本地8位编码，解决 Windows 下中文路径问题
        QByteArray baCpgFilePath = qCpgFilePath.toLocal8Bit();

        std::ofstream cpgFile(baCpgFilePath.constData());
        if (!cpgFile.is_open())
        {
          single_frame_data_logger->error("函数[{}]：无法创建 .cpg 文件：{}", __FUNCTION__, qCpgFilePath.toStdString());
        }
        else
        {
          cpgFile << "UTF-8";  // 写入编码信息
          cpgFile.close();
          single_frame_data_logger->info("函数[{}]：成功创建 .cpg 文件：{}", __FUNCTION__, qCpgFilePath.toStdString());
        }


        //  记录到 map，避免重复创建
        createdDatasets[NJBDXLayerEnglishName_GeoType] = poNewDS;

        //  在输出结构 NJBDX_single_frame_data_all_layers_info_entity 中记录该图层信息
        {
          SW_NJBDX_single_frame_data_single_layer_info_t newLayerInfo;
          newLayerInfo.layer_name = NJBDXLayerEnglishName_GeoType;
          newLayerInfo.layer_geo_type = SW_layer_geo_type;
          newLayerInfo.polayer = poNewLayer;  // 此时的图层指针来自独立的数据集
          newLayerInfo.poSRS = poNewLayer->GetSpatialRef();

          NJBDX_single_frame_data_all_layers_info_entity.vNJBDX_single_frame_data_single_layer_info.push_back(newLayerInfo);

          single_frame_data_logger->info("函数[{}]：创建NJBDX图层 [{}] 成功 (来自 SW图层 [{}])",
            __FUNCTION__, NJBDXLayerEnglishName_GeoType, SWLayerName);
        }
      }
      else
      {
        //  已经创建过同名 NJBDX 图层，则打印日志不再创建
        single_frame_data_logger->info("函数[{}]：NJBDX图层 [{}] 已创建过，跳过重复创建。", __FUNCTION__, NJBDXLayerEnglishName_GeoType);
      }
    } // end for (auto& singleNJBDXLayerMapping ...)
#pragma endregion

  } // end for (auto& SWLayerInfo ...)
#pragma endregion

#pragma region "3 关闭所有的数据集然后再以目录的方式打开"
  //  对 createdDatasets 中的数据集执行 GDALClose()
  for (const auto& SW_createdDataset : createdDatasets)
  {
    GDALClose(SW_createdDataset.second);
  }

  //  以更新方式打开整个数据目录（目录下存在多个Shapefile），后续将对图层进行添加要素
  GDALDataset* poSingleFrameNJBDXDataSet = (GDALDataset*)GDALOpenEx(NJBDX_str_output_data_path.c_str(), GDAL_OF_UPDATE, NULL, NULL, NULL);
  //  文件不存在或打开失败
  if (poSingleFrameNJBDXDataSet == NULL)
  {
    single_frame_data_logger->error("函数[{}]：使用GDALOpenEx打开数据集{}失败，可能指定目录中不存在有效的shapefile数据。", __FUNCTION__, NJBDX_str_output_data_path);
    //  日志：输出统计信息
    single_frame_data_logger->info("函数[{}]：已完成 NJBDX Shapefile 图层的创建，共创建(或复用) 0 个图层。", __FUNCTION__);
    //  日志：结束
    single_frame_data_logger->info("********函数[{}]日志结束--->在目录{}中创建 NJBDX Shapefile 图层********", __FUNCTION__, NJBDX_str_output_data_path);
    return false;
  }
  else
  {
    //  日志：输出统计信息
    single_frame_data_logger->info("函数[{}]：已完成 NJBDX Shapefile 图层的创建，共创建(或复用) {} 个图层。", __FUNCTION__, createdDatasets.size());
    single_frame_data_logger->info("********函数[{}]日志结束--->在目录{}中创建 NJBDX Shapefile 图层********", __FUNCTION__, NJBDX_str_output_data_path);
  }

  //  更新之前保存的图层指针,因为之前存入的图层指针来自已关闭的数据集，现在通过新打开的数据集重新获取各个图层的指针
  for (auto& layerInfo : NJBDX_single_frame_data_all_layers_info_entity.vNJBDX_single_frame_data_single_layer_info)
  {
    OGRLayer* updatedLayer = poSingleFrameNJBDXDataSet->GetLayerByName(layerInfo.layer_name.c_str());
    if (updatedLayer)
    {
      layerInfo.polayer = updatedLayer;
      layerInfo.poSRS = updatedLayer->GetSpatialRef();
    }
    else
    {
      single_frame_data_logger->warn("函数[{}]：重新打开数据集后，图层[{}]为空。", __FUNCTION__, layerInfo.layer_name);
    }
  }

  //  Flush 缓存并关闭数据集，释放文件锁
  poSingleFrameNJBDXDataSet->FlushCache();
  //  保存以目录方式打开的数据集指针
  NJBDX_single_frame_data_all_layers_info_entity.NJBDX_single_frame_data_poDS = poSingleFrameNJBDXDataSet;
#pragma endregion

  //  日志：结束
  single_frame_data_logger->info("********函数[{}]：日志结束--->在目录{}中创建 NJBDX Shapefile 图层********", __FUNCTION__, NJBDX_str_output_data_path);
  return true;
}

//  检查NJBDX数据集中的有效图层是否为空
bool qgs_SW_NJBDX_semantic_fusion::SW_Check_NJBDX_all_layers_info(
  const SW_NJBDX_single_frame_data_all_layers_info_t& NJBDX_single_frame_data_all_layers_info_entity)
{
  if (NJBDX_single_frame_data_all_layers_info_entity.vNJBDX_single_frame_data_single_layer_info.size() == 0)
  {
    return false;
  }
  return true;
}

void qgs_SW_NJBDX_semantic_fusion::SW_Close_NJBDX_all_layers_info(
  SW_NJBDX_single_frame_data_all_layers_info_t& NJBDX_single_frame_data_all_layers_info_entity)
{
  GDALDataset* poDataset = NJBDX_single_frame_data_all_layers_info_entity.NJBDX_single_frame_data_poDS;
  if (poDataset)
  {
    // 倒序遍历图层，避免在删除图层后导致后面图层索引错乱
    for (int i = poDataset->GetLayerCount() - 1; i >= 0; --i)
    {
      OGRLayer* poLayer = poDataset->GetLayer(i);
      if (poLayer)
      {
        // 获取要素数量
        GIntBig nFeatureCount = poLayer->GetFeatureCount();
        // 如果图层要素数为 0，则删除该图层
        if (nFeatureCount == 0)
        {
          // 删除图层。注意需要确认当前驱动是否支持 DeleteLayer
          poDataset->DeleteLayer(i);
        }
      }
    }

    // 最后关闭数据集并将指针置空
    GDALClose(poDataset);
    NJBDX_single_frame_data_all_layers_info_entity.NJBDX_single_frame_data_poDS = nullptr;
  }
}









//  提取图层名称
std::string qgs_SW_NJBDX_semantic_fusion::SW_Extract_LayerName(
    const std::string& layerName,
    const SW_Layers_Fields_Info_Json_t& SW_Layers_Fields_Info_Json_entity,
    std::shared_ptr<spdlog::logger> logger)
{
    // 如果传入的 layerName 为空，直接返回
    if (layerName.empty())
    {
        logger->warn("函数[{}]：传入的 layerName 为空字符串，即矢量图层名称为空。", __FUNCTION__);
        return "";
    }

    //  遍历配置文件中所有图层的配置信息
    for (const auto& layerFields : SW_Layers_Fields_Info_Json_entity.vSW_Layers_Fields)
    {
        //  如果配置中的 SW_Layer_Name 为空，直接跳过
        if (layerFields.SW_Layer_Name.empty())
        {
            logger->warn("函数[{}]：SW_Layers_Fields_Info.json配置文件中某个 SW_Layer_Name 为空字符串，跳过矢量图层名称提取处理。", __FUNCTION__);
            continue;
        }

        /*
        Notes:杨小兵-2025-02-16
          1、构造正则表达式，std::regex::icase 表示大小写不敏感，如果想要让匹配变成大小写敏感，只需要去掉正则表达式构造时传入的 std::regex::icase 标志即可
          //std::regex re(layerFields.SW_Layer_Name, std::regex::icase);
        */
        std::regex re(layerFields.SW_Layer_Name);

        //  使用 std::regex_search 来检查 layerName 是否包含该模式
        if (std::regex_search(layerName, re))
        {
            // 如果匹配成功，可以根据业务需求做更多处理
            logger->info("函数[{}]：图层名称[{}] 与SW_Layers_Fields_Info.json配置文件中的[{}] 匹配成功。",
                __FUNCTION__, layerName, layerFields.SW_Layer_Name);

            //  例如可以直接返回匹配到的 SW_Layer_Name
            return layerFields.SW_Layer_Name;
        }
    }

    //  如果遍历完也没有找到匹配的配置信息
    logger->info("函数[{}]：图层名称[{}] 未在SW_Layers_Fields_Info.json配置文件中找到匹配项。", __FUNCTION__, layerName);
    return "";
}

//  辅助函数：将OGRwkbGeometryType映射到字符串
std::string qgs_SW_NJBDX_semantic_fusion::SW_OGRGeometryType2String(const OGRwkbGeometryType& GeoType)
{
  //  使用 wkbFlatten 提取基本几何类型，忽略附加的标志（例如 3D 或测量）
  OGRwkbGeometryType flatType = wkbFlatten(GeoType);

  //  定义映射表，针对常见的矢量几何类型进行映射
  static const std::unordered_map<OGRwkbGeometryType, std::string> typeMap = {
      {wkbPoint,              "Point"},
      {wkbMultiPoint,         "MultiPoint"},
      {wkbLineString,         "Line"},  // 这里将 LineString 简化为 Line
      {wkbMultiLineString,    "MultiLineString"},
      {wkbPolygon,            "Polygon"},
      {wkbMultiPolygon,       "MultiPolygon"},
      {wkbGeometryCollection, "GeometryCollection"}
      //  根据需要可以添加更多类型
  };

  //  尝试在映射表中查找基本类型
  auto it = typeMap.find(flatType);
  if (it != typeMap.end())
  {
    return it->second;
  }
  else
  {
    //  如果映射表中没有找到对应的类型，则调用 GDAL 自带函数获取类型名称，OGRGeometryTypeToName 可以处理任意几何类型，并返回一个描述字符串
    const char* pszTypeName = OGRGeometryTypeToName(GeoType);
    if (pszTypeName)
    {
      return std::string(pszTypeName);
    }
    else
    {
      //  如果连 GDAL 函数也无法识别，则抛出异常，同时使用 std::to_string 转换枚举数值
      throw std::invalid_argument("未知的 GDAL 几何类型: " + std::to_string(GeoType));
    }
  }
}

//  辅助函数：从过滤条件中提取所有字段名称,该函数使用正则表达式，匹配所有形如 "字段名" 后面紧跟比较运算符的模式
std::vector<std::string> qgs_SW_NJBDX_semantic_fusion::extractFieldNames(const std::string& filter)
{
  std::vector<std::string> fieldNames;
  // 正则模式：匹配双引号中的内容，后面跟空白和比较运算符（=, !=, <, >, <=, >=）
  std::regex re("\"([^\"]+)\"\\s*(=|!=|<|>|<=|>=)");
  std::smatch match;
  std::string::const_iterator searchStart(filter.cbegin());
  while (std::regex_search(searchStart, filter.cend(), match, re))
  {
    // match[1] 为双引号内的字段名称
    fieldNames.push_back(match[1].str());
    searchStart = match.suffix().first;
  }
  return fieldNames;
}

//  辅助函数：1. 提取|符号前的子字符串并自动除去前后空格
std::string qgs_SW_NJBDX_semantic_fusion::extractBeforeDelimiter(const std::string& str)
{
    // lambda表达式：去除左右两边空格
    auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);
    };

    std::string result;
    size_t pos = str.find('|');
    if (pos != std::string::npos)
    {
        result = str.substr(0, pos);
    }

    // 去除左右两边空格
    trim(result);
    return result;
}

//  辅助函数：2. 提取|符号后的子字符串并自动除去前后空格
std::string qgs_SW_NJBDX_semantic_fusion::extractAfterDelimiter(const std::string& str)
{
    // lambda表达式：去除左右两边空格
    auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);
    };

    std::string result;
    size_t pos = str.find('|');
    if (pos != std::string::npos)
    {
        // 使用 pos + 1 来跳过 | 符号
        result = str.substr(pos + 1);
    }

    // 去除左右两边空格
    trim(result);
    return result;
}

#pragma endregion
