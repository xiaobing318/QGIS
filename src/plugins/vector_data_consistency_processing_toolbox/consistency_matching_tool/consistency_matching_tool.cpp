#pragma region "包含头文件（减少重复）"
/*-------------一致性处理工具相关头文件-------------*/
#include "consistency_matching_tool.h"
#include "consistency_processing_base.h"
#include <vector/cse_vector_feature_matching.h>
/*-------------一致性处理工具相关头文件-------------*/

/*------------------QGIS相关的头文件-------------------*/
#include "qgshelp.h"
#include "qgslayertree.h"
#include "qgslayertreemodel.h"
#include "qgsmaplayer.h"
#include "qgsproject.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayer.h"
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

/*----------------spdlog第三方日志库--------------*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"	//	创建基本的日志文件，这个日志文件用来接收一般的信息
/*----------------spdlog第三方日志库--------------*/

#pragma endregion


#pragma region "工具的公开API接口"

qgs_consistency_matching::qgs_consistency_matching(QWidget *parent, Qt::WindowFlags fl)
  :QDialog( parent, fl )
{
  //  分配内存空间
  this->m_SE_ConsistencyMatchingGuiBase = new SE_ConsistencyMatchingGuiBase();
  //  用于初始化和布置由Qt Designer创建的用户界面组件
  this->m_SE_ConsistencyMatchingGuiBase->setupUi(this);
  //  用于自动保存和恢复窗口的几何布局
  QgsGui::enableAutoGeometryRestore(this);
  //  用于恢复上一次使用保存的文件路径等状态
  restoreState();

  connect(
    m_SE_ConsistencyMatchingGuiBase->QPushButton_background_browse, &QPushButton::clicked,
    this, &qgs_consistency_matching::QPushButton_background_browse);
  connect(
    m_SE_ConsistencyMatchingGuiBase->QPushButton_matching_browse, &QPushButton::clicked,
    this, &qgs_consistency_matching::QPushButton_matching_browse);
  connect(
    m_SE_ConsistencyMatchingGuiBase->QPushButton_log_browse, &QPushButton::clicked,
    this, &qgs_consistency_matching::QPushButton_log_browse);
  connect(
    m_SE_ConsistencyMatchingGuiBase->QPushButton_output_data_path_browse, &QPushButton::clicked,
    this, &qgs_consistency_matching::QPushButton_output_data_path_browse);
  //  TODO:现在的需求没有自动生成配置文件的需求
  //connect(
  //  m_SE_ConsistencyMatchingGuiBase->QPushBotton_generate_config_file, &QPushButton::clicked,
  //  this, &qgs_consistency_matching::QPushBotton_generate_config_file);
  connect(
    m_SE_ConsistencyMatchingGuiBase->QPushBotton_load_config_file, &QPushButton::clicked,
    this, &qgs_consistency_matching::QPushBotton_load_config_file);

  connect(
    m_SE_ConsistencyMatchingGuiBase->Button_Start, &QPushButton::clicked,
    this, &qgs_consistency_matching::Button_Start);
  connect(
    m_SE_ConsistencyMatchingGuiBase->Button_Close, &QPushButton::clicked,
    this, &qgs_consistency_matching::Button_Close);

  //  默认设置为geopackage
  m_SE_ConsistencyMatchingGuiBase->QRadioButton_geopackage->setChecked(true);

  m_SE_ConsistencyMatchingGuiBase->QLineEdit_background_input_data_path->setText(m_background_data_path);
  m_SE_ConsistencyMatchingGuiBase->QLineEdit_matching_input_data_path->setText(m_matching_data_path);
  m_SE_ConsistencyMatchingGuiBase->QLineEdit_log_saving_path->setText(m_log_saving_path);
  m_SE_ConsistencyMatchingGuiBase->QLineEdit_output_data_path->setText(m_output_data_path);
  
}

qgs_consistency_matching::~qgs_consistency_matching()
{
  //  TODO:释放内存
  if (this->m_SE_ConsistencyMatchingGuiBase)
  {
    delete this->m_SE_ConsistencyMatchingGuiBase;
  }

  if (this->poShpBackgroundDS)
  {
    GDALClose(this->poShpBackgroundDS);
  }
  if (this->poShpMatchDS)
  {
    GDALClose(this->poShpMatchDS);
  }
  if (this->poGeoPackageBackgroundDS)
  {
    GDALClose(this->poGeoPackageBackgroundDS);
  }
  if (this->poGeoPackageMatchDS)
  {
    GDALClose(this->poGeoPackageMatchDS);
  }

  saveState();
}

std::string qgs_consistency_matching::erro_t2string(const erro_t& erro_type)
{
  switch (erro_type)
  {
  case erro_t::NO_ERRO:
  {
    return std::string("没有错误");
  }
  case erro_t::FILE_IS_NOT_EXIST:
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

void qgs_consistency_matching::Button_Start()
{
  m_SE_ConsistencyMatchingGuiBase->QRadioButton_geopackage->setChecked(false);

#pragma region "1、配置文件是否加载提示信息"
  if (this->m_config_file_info.vlayer_mathcing_layer_pair.size() == 0)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("已经加载了“匹配字段选择和字段权重设置”配置文件？如果没有加载配置文件，请先加载配置文件再进行操作"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    //  只是作为一个提示信息
    msgBox.exec();
  }
#pragma endregion

#pragma region "2、读取参数信息并且检查其有效性"
  //  读取输入数据格式
  const vector_data_type_t input_data_type = get_m_input_data_type();
  if (input_data_type == vector_data_type_t::Unknow)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("未选择输入数据格式！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

  //  读取输入数据字符编码
  character_set_encoding_type_t character_set_encoding_type = get_m_character_set_encoding_type();
  if (character_set_encoding_type == character_set_encoding_type_t::UNKNOW)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("未选择输入数据字符集编码！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

  //  读取空间阈值
  thresholds_t thresholds = get_m_thresholds();
  erro_t is_thresholds_valid_flag = is_thresholds_valid(thresholds);
  if (is_thresholds_valid_flag != erro_t::THRESHOLDS_ARE_VALID)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("阈值等级设置错误！阈值等级设置应该从小到大进行设置，例如1、2、3、4.5"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

  //  读取到的配置文件信息是否为空
  if ((this->m_config_file_info).vlayer_mathcing_layer_pair.size() == 0)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("配置文件为空（可能的原因：1、配置文件为空；2、没有加载配置文件）"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

  //  1、检查本底数据集；2、检查匹配数据集是否可以正常打开
  if (m_SE_ConsistencyMatchingGuiBase->QRadioButton_geopackage->isChecked())
  {
    //  本底数据类型和匹配数据都是GeoPackage
    
    //  打开本底数据和检查是否可以正常打开本底数据
    this->poGeoPackageBackgroundDS = open_geopackage_dataset(get_m_background_data_path());
    if (!(this->poGeoPackageBackgroundDS))
    {
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("一致性匹配工具"));
      msgBox.setText(tr("GeoPackage类型的本底数据打开失败（可能的原因：1、本底数据为空；2、本底数据损坏；3、本底数据文件不全）"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
    }

    //  打开匹配数据和检查是否可以正常打开匹配数据
    this->poGeoPackageMatchDS = open_geopackage_dataset(get_m_matching_data_path());
    if (!(this->poGeoPackageMatchDS))
    {
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("一致性匹配工具"));
      msgBox.setText(tr("GeoPackage类型的匹配数据打开失败（可能的原因：1、匹配数据为空；2、匹配数据损坏；3、匹配数据文件不全）"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
    }

  }
  else if (m_SE_ConsistencyMatchingGuiBase->QRadioButton_shapefile->isChecked())
  {
    //  本底数据类型和匹配数据都是Shapefile
    
    //  打开本底数据和检查是否可以正常打开本底数据
    this->poShpBackgroundDS = open_shapefile_dataset(get_m_background_data_path());
    if (!(this->poShpBackgroundDS))
    {
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("一致性匹配工具"));
      msgBox.setText(tr("本底数据打开失败（可能的原因：1、本底数据为空；2、本底数据损坏；3、本底数据文件不全）"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
    }

    //  打开匹配数据和检查是否可以正常打开匹配数据
    this->poShpMatchDS = open_shapefile_dataset(get_m_matching_data_path());
    if (!(this->poShpMatchDS))
    {
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("一致性匹配工具"));
      msgBox.setText(tr("匹配数据打开失败（可能的原因：1、匹配数据为空；2、匹配数据损坏；3、匹配数据文件不全）"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
    }

  }

#pragma endregion

#pragma region "3、对图层对进行一致性匹配处理"
  size_t layer_pairs_count = this->m_config_file_info.vlayer_mathcing_layer_pair.size();
  std::string background_data_layer_name = "";
  std::string matching_data_layer_name = "";

  bool poShpBackgroundDS_hasLayer_flag = false;
  bool poShpMatchDS_hasLayer_flag = false;

  bool poGeoPackageBackgroundDS_hasLayer_flag = false;
  bool poGeoPackageMatchDS_hasLayer_flag = false;

  int index4background_data_layer = -1;
  int index4match_data_layer = -1;
  //  用来统计哪些图层被处理了
  size_t has_processed_layers = 0;

  for (size_t i = 0; i < layer_pairs_count; i++)
  {

#pragma region "1、获取配置文件中需要进行一致性匹配的图层对，并且检查图层对在图层中是否存在"
    background_data_layer_name = m_config_file_info.vlayer_mathcing_layer_pair[i].background_data_single_layer_name;
    matching_data_layer_name = m_config_file_info.vlayer_mathcing_layer_pair[i].matching_data_single_layer_name;

    if (m_SE_ConsistencyMatchingGuiBase->QRadioButton_geopackage->isChecked())
    {
      poGeoPackageBackgroundDS_hasLayer_flag = hasLayer(poGeoPackageBackgroundDS, background_data_layer_name.c_str(), index4background_data_layer);
      poGeoPackageMatchDS_hasLayer_flag = hasLayer(poGeoPackageMatchDS, matching_data_layer_name.c_str(), index4match_data_layer);
    }
    else
    {
      poShpBackgroundDS_hasLayer_flag = hasLayer(poShpBackgroundDS, background_data_layer_name.c_str(), index4background_data_layer);
      poShpMatchDS_hasLayer_flag = hasLayer(poShpMatchDS, matching_data_layer_name.c_str(), index4match_data_layer);

    }

    //  根据不同的数据类型分别进行判断
    if (this->m_SE_ConsistencyMatchingGuiBase->QRadioButton_geopackage->isChecked())
    {
      //  判断图层是否存在
      if ((!poGeoPackageBackgroundDS_hasLayer_flag) && (!poGeoPackageMatchDS_hasLayer_flag))
      {
        //  TODO:如果在数据集中没有找到指定图层的话应该写入到日志中(两个图层都没有找到)
        //  跳过后续的处理，处理下一个图层对
        continue;
      }
      else if ((poGeoPackageBackgroundDS_hasLayer_flag) && (!poGeoPackageMatchDS_hasLayer_flag))
      {
        //  TODO:如果在数据集中没有找到指定图层的话应该写入到日志中(本底数据找到、匹配数据没有找到)
        //  跳过后续的处理，处理下一个图层对
        continue;
      }
      else if ((!poShpBackgroundDS_hasLayer_flag) && (poGeoPackageMatchDS_hasLayer_flag))
      {
        //  TODO:如果在数据集中没有找到指定图层的话应该写入到日志中(本底数据没有找到、匹配数据找到)
        //  跳过后续的处理，处理下一个图层对
        continue;
      }
      else
      {
        has_processed_layers++;
        //  说明两个图层名都找到了，进行后续的匹配处理
      }

    }
    else
    {
      //  判断图层是否存在
      if ((!poShpBackgroundDS_hasLayer_flag) && (!poShpMatchDS_hasLayer_flag))
      {
        //  TODO:如果在数据集中没有找到指定图层的话应该写入到日志中(两个图层都没有找到)
        //  跳过后续的处理，处理下一个图层对
        continue;
      }
      else if ((poShpBackgroundDS_hasLayer_flag) && (!poShpMatchDS_hasLayer_flag))
      {
        //  TODO:如果在数据集中没有找到指定图层的话应该写入到日志中(本底数据找到、匹配数据没有找到)
        //  跳过后续的处理，处理下一个图层对
        continue;
      }
      else if ((!poShpBackgroundDS_hasLayer_flag) && (poShpMatchDS_hasLayer_flag))
      {
        //  TODO:如果在数据集中没有找到指定图层的话应该写入到日志中(本底数据没有找到、匹配数据找到)
        //  跳过后续的处理，处理下一个图层对
        continue;
      }
      else
      {
        has_processed_layers++;
        //  说明两个图层名都找到了，进行后续的匹配处理
      }

    }
#pragma endregion

#pragma region "2、按照图层对来进行矢量数据一致性匹配"
    //  分别获取在各自数据集中存在的图层
    OGRLayer* current_background_data_layer = nullptr;
    OGRLayer* current_matching_data_layer = nullptr;
    if (m_SE_ConsistencyMatchingGuiBase->QRadioButton_geopackage->isChecked())
    {
      current_background_data_layer = poGeoPackageBackgroundDS->GetLayer(index4background_data_layer);
      current_matching_data_layer = poGeoPackageMatchDS->GetLayer(index4match_data_layer);
    }
    else
    {
      current_background_data_layer = poShpBackgroundDS->GetLayer(index4background_data_layer);
      current_matching_data_layer = poShpMatchDS->GetLayer(index4match_data_layer);
    }
    
    ogrfeature_match_pairs_t ogrfeature_match_pairs;

    if ((!current_background_data_layer) || (!current_matching_data_layer))
    {
      //  TODO:如果从数据集指针中获取图层失败的话应该写入到日志中
      //  跳过后续的处理，处理下一个图层对
      continue;
    }

#pragma region "1、空间一致性匹配"
    erro_t layer_spatial_match_flag = layer_spatial_match(current_background_data_layer, current_matching_data_layer, ogrfeature_match_pairs);
    if (layer_spatial_match_flag != erro_t::NO_ERRO)
    {
      //  TODO: 如果出现错误应该将对应的错误写入到日志中
      //  跳过后续的处理，处理下一个图层对
      continue;
    }
#pragma endregion

#pragma region "2、属性一致性匹配"
    //  创建一个新的匹配表单用来存储匹配记录
    matching_form_t matching_form;

    //  根据要素匹配对信息，即ogrfeature_match_pairs来产生最终的要素匹配对
    erro_t layer_attri_match_flag = layer_attri_match(
      current_background_data_layer,
      current_matching_data_layer,
      ogrfeature_match_pairs,
      matching_form);
    if (layer_attri_match_flag != erro_t::NO_ERRO)
    {
      //  TODO: 如果出现错误应该将对应的错误写入到日志中
      //  跳过后续的处理，处理下一个图层对
      continue;
    }

#pragma endregion

#pragma region "3、将匹配表单中的内容写入到输出shapefile文件中"
    //  如果(2、属性一致性匹配)layer_attri_match出现错误将不会执行到这一步
    erro_t write_matching_form2shapefile_flag = write_matching_form2shapefile(
      current_background_data_layer,
      current_matching_data_layer,
      matching_form);
    if (write_matching_form2shapefile_flag != erro_t::NO_ERRO)
    {
      //  TODO: 如果出现错误应该将对应的错误写入到日志中
      //  跳过后续的处理，处理下一个图层对
      continue;
    }
#pragma endregion

#pragma endregion

  }


  if (has_processed_layers != layer_pairs_count)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("正确处理了") + QString::number(has_processed_layers) + tr("图层对，") +
      tr("还有") + QString::number(layer_pairs_count - has_processed_layers) + tr("图层对没有处理。") +
      tr("（可能的原因：1、匹配文件中设置的图层名在数据集中找不到；2、本底数据损坏；3、匹配数据损坏）"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  else
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("一致性匹配处理完成！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion

}

void qgs_consistency_matching::Button_Close()
{
  //  有可能在界面上设置了路径，但是不小心关掉了，这里所做的处理就是将设置了的内容保存起来以便下一次使用的时候恢复
  saveState();
  reject();
}

QString qgs_consistency_matching::get_m_background_data_path()
{
  return m_background_data_path;
}

QString qgs_consistency_matching::get_m_matching_data_path()
{
  return m_matching_data_path;
}

vector_data_type_t qgs_consistency_matching::get_m_input_data_type()
{
  if (m_SE_ConsistencyMatchingGuiBase->QRadioButton_geopackage->isChecked())
  {
    set_m_input_data_type(vector_data_type_t::GeoPackage);
    return vector_data_type_t::GeoPackage;
  }
  else if (m_SE_ConsistencyMatchingGuiBase->QRadioButton_shapefile->isChecked())
  {
    set_m_input_data_type(vector_data_type_t::Shapefile);
    return vector_data_type_t::Shapefile;
  }
  return vector_data_type_t::Unknow;
}

character_set_encoding_type_t qgs_consistency_matching::get_m_character_set_encoding_type()
{
  QComboBox* comboBox = m_SE_ConsistencyMatchingGuiBase->QComboxBox_character_set;
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

thresholds_t qgs_consistency_matching::get_m_thresholds()
{
  QString threshold_setting_1 = m_SE_ConsistencyMatchingGuiBase->QLineEdit_threshold_setting_1->text().toUtf8();
  QString threshold_setting_2 = m_SE_ConsistencyMatchingGuiBase->QLineEdit_threshold_setting_2->text().toUtf8();
  QString threshold_setting_3 = m_SE_ConsistencyMatchingGuiBase->QLineEdit_threshold_setting_3->text().toUtf8();
  QString threshold_setting_4 = m_SE_ConsistencyMatchingGuiBase->QLineEdit_threshold_setting_4->text().toUtf8();
  this->m_thresholds.threshold_level1 = threshold_setting_1;
  this->m_thresholds.threshold_level2 = threshold_setting_2;
  this->m_thresholds.threshold_level3 = threshold_setting_3;
  this->m_thresholds.threshold_level4 = threshold_setting_4;
  return this->m_thresholds;
}

QString qgs_consistency_matching::get_m_config_file_path()
{
  return this->m_config_file_path;
}

log_level_t qgs_consistency_matching::get_m_log_level()
{
  return this->m_log_level;
}

QString qgs_consistency_matching::get_m_log_saving_path()
{
  return this->m_log_saving_path;
}

QString qgs_consistency_matching::get_m_output_data_path()
{
  return this->m_output_data_path;
}

void qgs_consistency_matching::set_m_background_data_path(const QString& background_data_path)
{
  this->m_background_data_path = background_data_path;
}

void qgs_consistency_matching::set_m_matching_data_path(const QString& matching_data_path)
{
  this->m_matching_data_path = matching_data_path;
}

void qgs_consistency_matching::set_m_input_data_type(const vector_data_type_t& vector_data_type)
{
  this->m_input_data_type = vector_data_type;
}

void qgs_consistency_matching::set_m_character_set_encoding_type(
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

void qgs_consistency_matching::set_m_thresholds(const thresholds_t& thresholds)
{
  //  这里还没有对四个阈值的合法性进行检查
  this->m_thresholds.threshold_level1 = thresholds.threshold_level1;
  this->m_thresholds.threshold_level2 = thresholds.threshold_level2;
  this->m_thresholds.threshold_level3 = thresholds.threshold_level3;
  this->m_thresholds.threshold_level4 = thresholds.threshold_level4;
}

void qgs_consistency_matching::set_m_config_file_path(const QString& config_file_path)
{
  this->m_config_file_path = config_file_path;
}

void qgs_consistency_matching::set_m_log_level(const log_level_t& log_level)
{
  this->m_log_level = log_level;
}

void qgs_consistency_matching::set_m_log_saving_path(const QString& log_saving_path)
{
  this->m_log_saving_path = log_saving_path;
}

void qgs_consistency_matching::set_m_output_data_path(const QString& output_data_path)
{
  this->m_output_data_path = output_data_path;
}

//  解释config_file_path配置文件中的内容，将读取到的信息存储到m_config_file_info中
erro_t qgs_consistency_matching::parse_config_file(const QString& config_file_path)
{
  //  将config_file_path中的csv文件信息读取到m_config_file_info中保存起来

  // 利用所有可用驱动器中的矢量驱动器
  const char* pszShpDriverName = "CSV";
  GDALDriver* poShpDriver;
  poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
  if (poShpDriver == NULL)
  {
    return erro_t::GET_CSV_DRIVER_FAILED;
  }

  // 打开CSV文件
  std::string csv_path = config_file_path.toUtf8();
  GDALDataset* poDS = (GDALDataset*)GDALOpenEx(csv_path.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
  if (poDS == NULL)
  {
    return erro_t::OPEN_GDALDATASET_FAILED;
  }

  // 获取图层
  OGRLayer* poLayer = poDS->GetLayer(0);
  if (poLayer == NULL)
  {
    GDALClose(poDS);
    return erro_t::OPEN_FIRST_LAYER_FAILED;
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


  poLayer->ResetReading();
  while ((poFeature = poLayer->GetNextFeature()) != NULL)
  {
    //  创建图层匹配对用来存储读取到的内容
    layer_mathcing_layer_pair_t layer_mathcing_layer_pair;
    //  创建一个字段匹配对结构用来存储读取到的配置文件内容
    matching_field_pair_t temp_matching_field_pair;

    int index_background_layer = -1;
    int index_matching_layer = -1;

    //  用来读取一个行（单个要素）信息
    for (int i = 0; i < poFeature->GetFieldCount(); i++)
    {
      poFieldDefn = poFDefn->GetFieldDefn(i);
      strFieldName = poFieldDefn->GetNameRef();
      strFieldValue = poFeature->GetFieldAsString(i);
      switch (i)
      {
      case 0: // 本底数据图层
      {
        bool flag = false;
        index_background_layer = is_background_layer_exist(strFieldValue, flag);
        if (!flag)
        {
          layer_mathcing_layer_pair.background_data_single_layer_name = strFieldValue;
        }
        //  如果存在的话则不进行处理
        break;
      }
      case 1: // 本底图层匹配字段
      {
        temp_matching_field_pair.background_field_name = strFieldValue;
        break;
      }
      case 2: // 匹配数据图层
      {
        bool flag = false;
        index_matching_layer = is_matching_layer_exist(strFieldValue, flag);
        if (!flag)
        {
          layer_mathcing_layer_pair.matching_data_single_layer_name = strFieldValue;
        }
        break;
      }
      case 3: // 匹配图层匹配字段
      {
        temp_matching_field_pair.matching_field_name = strFieldValue;
        break;
      }
      case 4: // 匹配字段对权重
      {
        temp_matching_field_pair.weight = strFieldValue;
        break;
      }
      } //  switch end
    }

    if (index_background_layer != -1)
    {
      this->m_config_file_info.vlayer_mathcing_layer_pair[index_background_layer].vmatching_field_pair.push_back(temp_matching_field_pair);
    }
    else
    {
      //  保存读取到的数据
      layer_mathcing_layer_pair.vmatching_field_pair.push_back(temp_matching_field_pair);
      this->m_config_file_info.vlayer_mathcing_layer_pair.push_back(layer_mathcing_layer_pair);
    }


    //  销毁创建的要素
    OGRFeature::DestroyFeature(poFeature);
  }

  // 关闭数据集
  GDALClose(poDS);

  return erro_t::NO_ERRO;
}

int qgs_consistency_matching::is_background_layer_exist(const std::string& strFieldName, bool& flag)
{
  int index = -1;
  flag = false;
  size_t numLayers = this->m_config_file_info.vlayer_mathcing_layer_pair.size();
  for (size_t i = 0; i < numLayers; i++)
  {
    if (strFieldName == this->m_config_file_info.vlayer_mathcing_layer_pair[i].background_data_single_layer_name)
    {
      flag = true;
      index = static_cast<int>(i);
      break;
    }
  }
  return index;
}


int qgs_consistency_matching::is_matching_layer_exist(const std::string& strFieldName, bool& flag)
{
  int index = -1;
  flag = false;
  size_t numLayers = this->m_config_file_info.vlayer_mathcing_layer_pair.size();
  for (size_t i = 0; i < numLayers; i++)
  {
    if (strFieldName == this->m_config_file_info.vlayer_mathcing_layer_pair[i].matching_data_single_layer_name)
    {
      flag = true;
      index = static_cast<int>(i);
      break;
    }
  }
  return index;
}

//  检查config_file_info配置文件中的信息是否有效
erro_t qgs_consistency_matching::is_config_file_info_t_valid()
{
  //  目前先不用对配置文件中内容的有效性进行检查
  //  TODO：实现对配置文件有效性的检查
  return erro_t::CONFIG_FILE_INFO_T_IS_VALID;
}

//  路径path是否有效
erro_t qgs_consistency_matching::is_path_valid(const QString& path)
{
  QFileInfo checkFile(path);
  // 检查路径是否存在并且是一个文件或目录
  if (checkFile.exists() && (checkFile.isFile() || checkFile.isDir()))
  {
    return erro_t::PATH_IS_VALID; // 路径有效
  }
  else
  {
    return erro_t::PATH_IS_NOT_VALID; // 路径无效
  }
}

//  给定文件路径，检查文件是否存在
erro_t qgs_consistency_matching::is_file_exist(const QString& file_path)
{
  QFileInfo fileInfo(file_path);
  // 检查文件是否存在并且确实是一个文件
  if (fileInfo.exists() && fileInfo.isFile())
  {
    return erro_t::FILE_IS_EXIST; // 文件存在
  }
  else
  {
    return erro_t::FILE_IS_NOT_EXIST; // 文件不存在
  }
}

//  给定目录路径，检查目录是否存在
erro_t qgs_consistency_matching::is_dir_exist(const QString& dir_path)
{
  QFileInfo dirInfo(dir_path);
  // 检查路径是否存在并且是一个目录
  if (dirInfo.exists() && dirInfo.isDir())
  {
    return erro_t::DIR_IS_EXIST; // 目录存在
  }
  else
  {
    return erro_t::DIR_IS_NOT_EXIST; // 目录不存在
  }
}

//  验证获取到的阈值是否有效
erro_t qgs_consistency_matching::is_thresholds_valid(const thresholds_t& thresholds)
{
  double dthreshold_level1;
  double dthreshold_level2;
  double dthreshold_level3;
  double dthreshold_level4;
  bool status1 = false;
  bool status2 = false;
  bool status3 = false;
  bool status4 = false;
  dthreshold_level1 = thresholds.threshold_level1.toDouble(&status1);
  if (!status1)
  {
    return erro_t::THRESHOLDS_ARE_NOT_VALID;
  }

  dthreshold_level2 = thresholds.threshold_level2.toDouble(&status2);
  if (!status2)
  {
    return erro_t::THRESHOLDS_ARE_NOT_VALID;
  }

  dthreshold_level3 = thresholds.threshold_level3.toDouble(&status3);
  if (!status3)
  {
    return erro_t::THRESHOLDS_ARE_NOT_VALID;
  }

  dthreshold_level4 = thresholds.threshold_level4.toDouble(&status4);
  if (!status4)
  {
    return erro_t::THRESHOLDS_ARE_NOT_VALID;
  }

  if (!((dthreshold_level1 <= dthreshold_level2) &&
    (dthreshold_level2 <= dthreshold_level3) &&
    (dthreshold_level3 <= dthreshold_level4)))
  {
    return erro_t::THRESHOLDS_ARE_NOT_VALID;
  }
  else
  {
    return erro_t::THRESHOLDS_ARE_VALID;
  }
}

//  打开一个指定的Shapefile数据集
GDALDataset* qgs_consistency_matching::open_shapefile_dataset(const QString& dataset_path)
{
  // 确保在打开数据集前初始化GDAL
  initialize_gdal();  

  std::string str_dataset_path = dataset_path.toStdString();
  const char* pszDrivers[] = { nullptr, nullptr };
  GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(str_dataset_path.c_str(), GDAL_OF_VECTOR, NULL, pszDrivers, NULL));
  if (poDS)
  {
    return poDS;
  }
  else
  {
    return nullptr;
  }
}

//  打开一个指定的GeoPackage数据集
GDALDataset* qgs_consistency_matching::open_geopackage_dataset(const QString& dataset_path)
{
  initialize_gdal();  // 确保在打开数据集前初始化GDAL

  std::string str_dataset_path = dataset_path.toStdString();
  // 使用GDALOpenEx打开GeoPackage文件
  const char* pszDrivers[] = { "GPKG", nullptr };
  GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(str_dataset_path.c_str(), GDAL_OF_VECTOR, NULL, pszDrivers, NULL));
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
void qgs_consistency_matching::initialize_gdal()
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
bool qgs_consistency_matching::hasLayer(GDALDataset* poDS, const char* layerName, int& index)
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

//  矢量图层<---->矢量图层一致性匹配处理
erro_t qgs_consistency_matching::layer_spatial_match(
  OGRLayer* background_data_layer,
  OGRLayer* match_data_layer,
  ogrfeature_match_pairs_t& match_pairs)
{
#pragma region "1、进行一些必要的检查"
  if ((!background_data_layer) || (!match_data_layer))
  {
    return erro_t::OGRLAYER_IS_NULLPTR;
  }

#pragma endregion

#pragma region "2、获取图层要素的几何类型"
  /*
  * Assumptions：单个shapefile图层中的要素几何类型都是一致的，因此这里只是在图层中选择了一个要素来获取要素的几何类型，用单个
  * 要素的几何类型来代表整个图层的几何类型
  *
  * TODO：可能需要对整个图层的要素几何类型做一个判断，查看是否存在混合情况
  */

  // 获取本底数据图层的几何类型
  OGRFeature* bgFeature = background_data_layer->GetNextFeature();
  if (!bgFeature)
  {
    // 若无特征，则返回错误
    return erro_t::NO_FEATURE_FOUND;
  }
  OGRGeometry* bgGeometry = bgFeature->GetGeometryRef();
  if (!bgGeometry)
  {
    // 若无几何体，则返回错误
    return erro_t::NO_GEOMETRY_FOUND;
  }
  /************************本底数据图层的几何类型************************/
  OGRwkbGeometryType bgType = bgGeometry->getGeometryType();
  //  释放掉分配的内存
  OGRFeature::DestroyFeature(bgFeature);

  // 获取匹配数据图层的几何类型
  OGRFeature* matchFeature = match_data_layer->GetNextFeature();
  if (!matchFeature)
  {
    // 若无特征，则返回错误
    return erro_t::NO_FEATURE_FOUND;
  }
  OGRGeometry* matchGeometry = matchFeature->GetGeometryRef();
  if (!matchGeometry)
  {
    // 若无几何体，则返回错误
    return erro_t::NO_GEOMETRY_FOUND;
  }
  /************************匹配数据图层的几何类型************************/
  OGRwkbGeometryType matchType = matchGeometry->getGeometryType();
  //  释放掉分配的内存
  OGRFeature::DestroyFeature(matchFeature);

#pragma endregion

#pragma region "3、TODO：建立外接最小矩形来过滤掉一部分的计算量"

#pragma endregion

#pragma region "4、空间一致性匹配"
  //  用来保存符合空间一致性匹配的要素对索引
  ogrfeature_match_pairs_t ogrfeature_match_pairs;

  // 重置图层的读取指针
  background_data_layer->ResetReading();
  match_data_layer->ResetReading();
  // 逐个获取每一个要素
  OGRFeature* background_data_feature = NULL;
  OGRFeature* match_data_feature = NULL;

#pragma region "创建矢量要素匹配参数"
  vector_feature_matching_args_t vector_feature_matching_args1(
    (this->m_thresholds.threshold_level1).toStdString(),
    (this->m_thresholds.threshold_level1).toStdString(),
    match_alg_type_t::DOUBLE_BUFFER_MATCHING_ALG);

  vector_feature_matching_args_t vector_feature_matching_args2(
    (this->m_thresholds.threshold_level2).toStdString(),
    (this->m_thresholds.threshold_level2).toStdString(),
    match_alg_type_t::DOUBLE_BUFFER_MATCHING_ALG);

  vector_feature_matching_args_t vector_feature_matching_args3(
    (this->m_thresholds.threshold_level3).toStdString(),
    (this->m_thresholds.threshold_level3).toStdString(),
    match_alg_type_t::DOUBLE_BUFFER_MATCHING_ALG);

  vector_feature_matching_args_t vector_feature_matching_args4(
    (this->m_thresholds.threshold_level4).toStdString(),
    (this->m_thresholds.threshold_level4).toStdString(),
    match_alg_type_t::DOUBLE_BUFFER_MATCHING_ALG);
#pragma endregion

  while ((background_data_feature = background_data_layer->GetNextFeature()) != NULL)
  {
    while ((match_data_feature = match_data_layer->GetNextFeature()) != NULL)
    {
      //  创建一个用于空间匹配的算法实例（TODO：可以在循环外层创建空间匹配算法实例，在循环内部修改要素指针，这样可以优化性能，需要修改接口）

      vector_feature_matching feature_spatial_match_alg1(background_data_feature, match_data_feature, vector_feature_matching_args1);
      vector_feature_matching feature_spatial_match_alg2(background_data_feature, match_data_feature, vector_feature_matching_args2);
      vector_feature_matching feature_spatial_match_alg3(background_data_feature, match_data_feature, vector_feature_matching_args3);
      vector_feature_matching feature_spatial_match_alg4(background_data_feature, match_data_feature, vector_feature_matching_args4);

#pragma region "分别使用四个阈值来计算空间匹配度（得到了要素匹配对）"
      //  分别使用四个阈值来计算空间匹配度
      if ((wkbFlatten(bgType) == OGRwkbGeometryType::wkbPoint) && (wkbFlatten(matchType) == OGRwkbGeometryType::wkbPoint))
      {
        bool bFlag = false;
        wkbPointMatch_args_t wkbPointMatch_args;
        error_t wkbPointMatch_flag;

#pragma region "在阈值等级1的情况下判断是否匹配"
        wkbPointMatch_args.m_threshold = vector_feature_matching_args1.m_OGRFeature1_buffer;
        wkbPointMatch_args.m_match_alg_type = vector_feature_matching_args1.m_match_alg_type;
        wkbPointMatch_flag = feature_spatial_match_alg1.wkbPointMatch(wkbPointMatch_args, bFlag);
        if (wkbPointMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)
            
            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }
         
          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "1";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
#pragma endregion

#pragma region "在阈值等级2的情况下判断是否匹配"
        //  如果控制流没有进入到上述的分支中，说明第一个阈值等级下没有匹配上,如果进入了上述的控制流将不会执行到这里
        wkbPointMatch_args.m_threshold = vector_feature_matching_args2.m_OGRFeature1_buffer;
        wkbPointMatch_args.m_match_alg_type = vector_feature_matching_args2.m_match_alg_type;
        wkbPointMatch_flag = feature_spatial_match_alg2.wkbPointMatch(wkbPointMatch_args, bFlag);
        if (wkbPointMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "2";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
#pragma endregion

#pragma region "在阈值等级3的情况下判断是否匹配"
        //  如果控制流没有进入到上述的分支中，说明第一个阈值等级下没有匹配上,如果进入了上述的控制流将不会执行到这里
        wkbPointMatch_args.m_threshold = vector_feature_matching_args3.m_OGRFeature1_buffer;
        wkbPointMatch_args.m_match_alg_type = vector_feature_matching_args3.m_match_alg_type;
        wkbPointMatch_flag = feature_spatial_match_alg3.wkbPointMatch(wkbPointMatch_args, bFlag);
        if (wkbPointMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "3";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
#pragma endregion

#pragma region "在阈值等级4的情况下判断是否匹配"
        //  如果控制流没有进入到上述的分支中，说明第一个阈值等级下没有匹配上,如果进入了上述的控制流将不会执行到这里
        wkbPointMatch_args.m_threshold = vector_feature_matching_args4.m_OGRFeature1_buffer;
        wkbPointMatch_args.m_match_alg_type = vector_feature_matching_args4.m_match_alg_type;
        wkbPointMatch_flag = feature_spatial_match_alg4.wkbPointMatch(wkbPointMatch_args, bFlag);
        if (wkbPointMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "4";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
#pragma endregion

      }
      else if ((wkbFlatten(bgType) == OGRwkbGeometryType::wkbLineString) && (wkbFlatten(matchType) == OGRwkbGeometryType::wkbLineString))
      {
        bool bFlag = false;
        wkbLineStringMatch_args_t wkbLineStringMatch_args;
        error_t wkbLineStringMatch_flag;

#pragma region "在阈值等级1的情况下判断是否匹配"
        wkbLineStringMatch_args.m_threshold = vector_feature_matching_args1.m_OGRFeature1_buffer;
        wkbLineStringMatch_args.m_match_alg_type = vector_feature_matching_args1.m_match_alg_type;
        wkbLineStringMatch_flag = feature_spatial_match_alg1.wkbLineStringMatch(wkbLineStringMatch_args, bFlag);
        if (wkbLineStringMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)


            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)


            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "1";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
#pragma endregion

#pragma region "在阈值等级2的情况下判断是否匹配"
        wkbLineStringMatch_args.m_threshold = vector_feature_matching_args2.m_OGRFeature1_buffer;
        wkbLineStringMatch_args.m_match_alg_type = vector_feature_matching_args2.m_match_alg_type;
        wkbLineStringMatch_flag = feature_spatial_match_alg2.wkbLineStringMatch(wkbLineStringMatch_args, bFlag);
        if (wkbLineStringMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "2";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
#pragma endregion

#pragma region "在阈值等级3的情况下判断是否匹配"
        wkbLineStringMatch_args.m_threshold = vector_feature_matching_args3.m_OGRFeature1_buffer;
        wkbLineStringMatch_args.m_match_alg_type = vector_feature_matching_args3.m_match_alg_type;
        wkbLineStringMatch_flag = feature_spatial_match_alg3.wkbLineStringMatch(wkbLineStringMatch_args, bFlag);
        if (wkbLineStringMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "3";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
  #pragma endregion

#pragma region "在阈值等级4的情况下判断是否匹配"
        wkbLineStringMatch_args.m_threshold = vector_feature_matching_args4.m_OGRFeature1_buffer;
        wkbLineStringMatch_args.m_match_alg_type = vector_feature_matching_args4.m_match_alg_type;
        wkbLineStringMatch_flag = feature_spatial_match_alg4.wkbLineStringMatch(wkbLineStringMatch_args, bFlag);
        if (wkbLineStringMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)


            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "4";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
  #pragma endregion

      }
      else if ((wkbFlatten(bgType) == OGRwkbGeometryType::wkbPolygon) && (wkbFlatten(matchType) == OGRwkbGeometryType::wkbPolygon))
      {
        bool bFlag = false;
        wkbPolygonMatch_args_t wkbPolygonMatch_args;
        error_t wkbPolygonMatch_flag;

#pragma region "在阈值等级1的情况下判断是否匹配"
        wkbPolygonMatch_args.m_threshold = vector_feature_matching_args1.m_OGRFeature1_buffer;
        wkbPolygonMatch_args.m_match_alg_type = vector_feature_matching_args1.m_match_alg_type;
        wkbPolygonMatch_flag = feature_spatial_match_alg1.wkbPolygonMatch(wkbPolygonMatch_args, bFlag);
        if (wkbPolygonMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "1";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
#pragma endregion

#pragma region "在阈值等级2的情况下判断是否匹配"
        wkbPolygonMatch_args.m_threshold = vector_feature_matching_args2.m_OGRFeature1_buffer;
        wkbPolygonMatch_args.m_match_alg_type = vector_feature_matching_args2.m_match_alg_type;
        wkbPolygonMatch_flag = feature_spatial_match_alg1.wkbPolygonMatch(wkbPolygonMatch_args, bFlag);
        if (wkbPolygonMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "2";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
#pragma endregion

#pragma region "在阈值等级3的情况下判断是否匹配"
        wkbPolygonMatch_args.m_threshold = vector_feature_matching_args3.m_OGRFeature1_buffer;
        wkbPolygonMatch_args.m_match_alg_type = vector_feature_matching_args3.m_match_alg_type;
        wkbPolygonMatch_flag = feature_spatial_match_alg1.wkbPolygonMatch(wkbPolygonMatch_args, bFlag);
        if (wkbPolygonMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "3";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
  #pragma endregion

#pragma region "在阈值等级4的情况下判断是否匹配"
        wkbPolygonMatch_args.m_threshold = vector_feature_matching_args4.m_OGRFeature1_buffer;
        wkbPolygonMatch_args.m_match_alg_type = vector_feature_matching_args4.m_match_alg_type;
        wkbPolygonMatch_flag = feature_spatial_match_alg1.wkbPolygonMatch(wkbPolygonMatch_args, bFlag);
        if (wkbPolygonMatch_flag != error_t::NO_ERRORS)
        {
          //  TODO:将异常情况写入到日志文件中
        }
        if (bFlag)
        {
          //  获取两个要素的名为“FID”字段的属性值并且记录下来（杨小兵-2024-07-04现在修改为检查“SRCID”）
          ogrfeature_index_pair_t ogrfeature_index_pair;
          GIntBig bg_feature_FID;
          GIntBig mt_feature_FID;

          //int bg_fid_index = background_data_feature->GetFieldIndex("FID");
          int bg_fid_index = background_data_feature->GetFieldIndex("SRCID");
          if (bg_fid_index != -1 && background_data_feature->IsFieldSetAndNotNull(bg_fid_index))
          {
            bg_feature_FID = background_data_feature->GetFieldAsInteger(bg_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          //int mt_fid_index = match_data_feature->GetFieldIndex("FID");
          int mt_fid_index = match_data_feature->GetFieldIndex("SRCID");
          if (mt_fid_index != -1 && match_data_feature->IsFieldSetAndNotNull(mt_fid_index))
          {
            mt_feature_FID = match_data_feature->GetFieldAsInteger(mt_fid_index);
          }
          else
          {
            //  TODO:处理字段不存在或为NULL的情况(记录日志内容还未实现)

            //  释放内存
            OGRFeature::DestroyFeature(background_data_feature);
            OGRFeature::DestroyFeature(match_data_feature);
            return erro_t::FID_FIELD_NO_FOUND;
          }

          ogrfeature_index_pair.background_fid = bg_feature_FID;
          ogrfeature_index_pair.match_fid = mt_feature_FID;
          ogrfeature_index_pair.threshold_level = "4";
          ogrfeature_match_pairs.vogrfeature_index_pairs.push_back(ogrfeature_index_pair);

          //  说明已经找到了对应的匹配要素，现在应该跳出第二个循环
          break;
        }
  #pragma endregion

      }
      else if ((wkbFlatten(bgType) == OGRwkbGeometryType::wkbMultiPoint) && (wkbFlatten(matchType) == OGRwkbGeometryType::wkbMultiPoint))
      {
        //  TODO:还没有具体的空间匹配算法
      }
      else if ((wkbFlatten(bgType) == OGRwkbGeometryType::wkbMultiLineString) && (wkbFlatten(matchType) == OGRwkbGeometryType::wkbMultiLineString))
      {
        //  TODO:还没有具体的空间匹配算法
      }
      else if ((wkbFlatten(bgType) == OGRwkbGeometryType::wkbMultiPolygon) && (wkbFlatten(matchType) == OGRwkbGeometryType::wkbMultiPolygon))
      {
        //  TODO:还没有具体的空间匹配算法
      }
      else if ((wkbFlatten(bgType) == OGRwkbGeometryType::wkbGeometryCollection) && (wkbFlatten(matchType) == OGRwkbGeometryType::wkbGeometryCollection))
      {
        //  TODO:还没有具体的空间匹配算法
      }
      else if ((wkbFlatten(bgType) == OGRwkbGeometryType::wkbLinearRing) && (wkbFlatten(matchType) == OGRwkbGeometryType::wkbLinearRing))
      {
        //  TODO:还没有具体的空间匹配算法
      }
      else
      {
        //  TODO:待实现
      }
#pragma endregion

      //  处理完毕后，释放要素的内存
      OGRFeature::DestroyFeature(background_data_feature);
      OGRFeature::DestroyFeature(match_data_feature);
    }
  }
#pragma endregion

  match_pairs = ogrfeature_match_pairs;
  return erro_t::NO_ERRO;
}

//  矢量图层<---->矢量图层属性一致性匹配处理
erro_t qgs_consistency_matching::layer_attri_match(
  OGRLayer* background_data_layer,
  OGRLayer* match_data_layer,
  const ogrfeature_match_pairs_t& match_pairs,
  matching_form_t& matching_form)
{
#pragma region "1、进行一些必要的检查"
  if ((!background_data_layer) || (!match_data_layer))
  {
    return erro_t::OGRLAYER_IS_NULLPTR;
  }
  if (match_pairs.vogrfeature_index_pairs.size() == 0)
  {
    return erro_t::MATCH_PAIR_IS_EMPTY;
  }
  for (size_t i = 0; i < match_pairs.vogrfeature_index_pairs.size(); i++)
  {
    if((match_pairs.vogrfeature_index_pairs[i].background_fid == -1) || (match_pairs.vogrfeature_index_pairs[i].match_fid == -1))
    {
      return erro_t::MATCH_PAIR_IS_NOT_VALID;
    }
  }
#pragma endregion

#pragma region "2、循环计算匹配表单中需要计算的值并且记录到匹配表单matching_form中"
  ogrfeature_index_pair_t current_ogrfeature_index_pair;
  matching_record_t temp_matching_record;
  for (size_t i = 0; i < match_pairs.vogrfeature_index_pairs.size(); i++)
  {
    current_ogrfeature_index_pair = match_pairs.vogrfeature_index_pairs[i];

#pragma region "计算SMD（空间匹配度）"
    if (current_ogrfeature_index_pair.threshold_level == "1")
    {
      temp_matching_record.SMD = "1";
    }
    else if (current_ogrfeature_index_pair.threshold_level == "2")
    {
      temp_matching_record.SMD = "0.8";
    }
    else if (current_ogrfeature_index_pair.threshold_level == "3")
    {
      temp_matching_record.SMD = "0.6";
    }
    else if (current_ogrfeature_index_pair.threshold_level == "4")
    {
      temp_matching_record.SMD = "0.5";
    }
    else
    {
      temp_matching_record.SMD = "0.0";
    }
#pragma endregion

#pragma region "计算Attr_Sim（属性相似度）"
    //  这部分内容将会使用到从配置文件得到的信息
    temp_matching_record.Attr_Sim = get_Attr_Sim();
#pragma endregion

#pragma region "计算SameEleConf（同名要素确认度）"
    double SMD_value = stod(temp_matching_record.SMD);
    double Attr_Sim_value = stod(temp_matching_record.Attr_Sim);
    temp_matching_record.SameEleConf = std::to_string(SMD_value * Attr_Sim_value);
#pragma endregion

#pragma region "计算Data1_ID（本底图层要素ID）"
    temp_matching_record.Data1_ID = std::to_string(current_ogrfeature_index_pair.background_fid);
#pragma endregion

#pragma region "计算RelFacID（匹配图层要素ID）"
    temp_matching_record.RelFacID = std::to_string(current_ogrfeature_index_pair.match_fid);
#pragma endregion

#pragma region "计算TCStatus（现势性比对）"
    //  TODO:需要获取两个要素中名为"SRCDATE"字段的值，然后比较现势性
    std::string temp_TCStatus = "";
    erro_t get_TCStatus_flag = get_TCStatus(
      background_data_layer,
      match_data_layer,
      current_ogrfeature_index_pair.background_fid,
      current_ogrfeature_index_pair.match_fid,
      temp_TCStatus);

    if (get_TCStatus_flag != erro_t::NO_ERRO)
    {
      //  TODO:将错误记录在日志中
      temp_matching_record.TCStatus = "";
      //  继续执行下一个属性匹配
      continue;
    }
    //  如果获取TCStatus出现错误将不会执行到这一步
    temp_matching_record.TCStatus = temp_TCStatus;
#pragma endregion

    //  将计算得到的单条记录保存起来
    matching_form.vmatching_record.push_back(temp_matching_record);
  }

  return erro_t::NO_ERRO;
#pragma endregion

}

//  计算属性相似度（Attr_Sim）
std::string qgs_consistency_matching::get_Attr_Sim()
{
  //  TODO:待实现
  return std::string("0.5");
}

//  计算现势性（TCStatus）
erro_t qgs_consistency_matching::get_TCStatus(
  OGRLayer* background_data_layer,
  OGRLayer* match_data_layer,
  const GIntBig& bg_feature_fid,
  const GIntBig& mt_feature_fid,
  std::string& TCStatus_value)
{
#pragma region "1、进行一些必要的检查"
  if ((!background_data_layer) || (!match_data_layer))
  {
    return erro_t::OGRLAYER_IS_NULLPTR;
  }

  OGRFeature* bg_feature = background_data_layer->GetFeature(bg_feature_fid);
  if (!bg_feature)
  {
    return erro_t::FEATURE_NOT_FOUND;
  }

  OGRFeature* mt_feature = match_data_layer->GetFeature(mt_feature_fid);
  if (!mt_feature)
  {
    // 释放已获取的资源
    OGRFeature::DestroyFeature(bg_feature);
    return erro_t::FEATURE_NOT_FOUND;
  }

  const char* bg_srcdate = bg_feature->GetFieldAsString("SRCDATE");
  if (!bg_srcdate)
  {
    // 释放已获取的资源
    OGRFeature::DestroyFeature(bg_feature);
    OGRFeature::DestroyFeature(mt_feature);
    return erro_t::SRCDATE_FIELD_NOT_FOUND;
  }

  const char* mt_srcdate = mt_feature->GetFieldAsString("SRCDATE");
  if (!mt_srcdate)
  {
    // 释放已获取的资源
    OGRFeature::DestroyFeature(bg_feature);
    OGRFeature::DestroyFeature(mt_feature);
    return erro_t::SRCDATE_FIELD_NOT_FOUND;
  }
#pragma endregion

#pragma region "2、现势性判断"
  int ibg_srcdate = atoi(bg_srcdate);
  int imt_srcdate = atoi(mt_srcdate);
  if (imt_srcdate < ibg_srcdate)
  {
    // 释放已获取的资源
    OGRFeature::DestroyFeature(bg_feature);
    OGRFeature::DestroyFeature(mt_feature);
    TCStatus_value = "1";
    return erro_t::NO_ERRO;
  }
  else
  {
    // 释放已获取的资源
    OGRFeature::DestroyFeature(bg_feature);
    OGRFeature::DestroyFeature(mt_feature);
    TCStatus_value = "-1";
    return erro_t::NO_ERRO;
  }
#pragma endregion
}

//  将匹配信息写入到shapefile中
erro_t qgs_consistency_matching::write_matching_form2shapefile(
  OGRLayer* background_data_layer,
  OGRLayer* match_data_layer,
  const matching_form_t& matching_form)
{
#pragma region "1、进行一些必要的检查"
  if ((!background_data_layer) || (!match_data_layer))
  {
    return erro_t::OGRLAYER_IS_NULLPTR;
  }
#pragma endregion

#pragma region "2、在指定路径下创建shapefile并且写入数据"
  std::string output_path = (this->m_output_data_path).toStdString();
  //  获取background_data_layer图层名称
  std::string background_data_layer_name = background_data_layer->GetName();
  //  获取match_data_layer图层名称
  std::string match_data_layer_name = match_data_layer->GetName();
  //  拼接文件全路径
  std::string shp_match_form_path = output_path + "/" + "match_form4_" + background_data_layer_name + "_" + match_data_layer_name + ".shp";

  // 利用所有可用驱动器中的矢量驱动器
  const char* pszShpDriverName = "ESRI Shapefile";
  GDALDriver* poShpDriver;
  poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
  if (poShpDriver == NULL)
  {
    return erro_t::GET_SHP_DRIVER_FAILED;
  }

  // 创建Shapefile
  GDALDataset* poDS;
  poDS = poShpDriver->Create(shp_match_form_path.c_str(), 0, 0, 0, GDT_Unknown, NULL);
  if (poDS == NULL)
  {
    return erro_t::CREATE_SHP_FILE_FAILED;
  }

  //  判断图层要素几何类型
  OGRwkbGeometryType geomType = wkbFlatten(background_data_layer->GetGeomType());
  // 创建图层
  OGRLayer* poLayer;
  poLayer = poDS->CreateLayer(match_data_layer_name.c_str(), NULL, geomType, NULL);
  if (poLayer == NULL)
  {
    GDALClose(poDS);
    return erro_t::CREATE_OGRLAYER_FAILED;
  }

#pragma region "给Shapefile图层添加字段"
  // 添加"空间匹配度"字段
  OGRFieldDefn oFieldSpaceMatch("SMD", OFTString);
  oFieldSpaceMatch.SetWidth(256);
  if (poLayer->CreateField(&oFieldSpaceMatch) != OGRERR_NONE)
  {
    GDALClose(poDS);
    return erro_t::CREATE_FIELD_FAILED;
  }

  // 添加"属性相似度"字段
  OGRFieldDefn oFieldAttrSim("Attr_Sim", OFTString);
  oFieldAttrSim.SetWidth(256);
  if (poLayer->CreateField(&oFieldAttrSim) != OGRERR_NONE)
  {
    GDALClose(poDS);
    return erro_t::CREATE_FIELD_FAILED;
  }

  // 添加"同名要素确认度"字段
  OGRFieldDefn oFieldSameEleConf("SameEleConf", OFTString);
  oFieldSameEleConf.SetWidth(256);
  if (poLayer->CreateField(&oFieldSameEleConf) != OGRERR_NONE)
  {
    GDALClose(poDS);
    return erro_t::CREATE_FIELD_FAILED;
  }

  // 添加"本底图层要素ID"字段
  OGRFieldDefn oFieldData1ID("Data1_ID", OFTString);
  oFieldData1ID.SetWidth(256);
  if (poLayer->CreateField(&oFieldData1ID) != OGRERR_NONE)
  {
    GDALClose(poDS);
    return erro_t::CREATE_FIELD_FAILED;
  }

  // 添加"匹配图层要素ID"字段
  OGRFieldDefn oFieldRelFacID("RelFacID", OFTString);
  oFieldRelFacID.SetWidth(256);
  if (poLayer->CreateField(&oFieldRelFacID) != OGRERR_NONE)
  {
    GDALClose(poDS);
    return erro_t::CREATE_FIELD_FAILED;
  }

  // 添加"现势性比对"字段
  OGRFieldDefn oFieldTCStatus("TCStatus", OFTString);
  oFieldTCStatus.SetWidth(256);
  if (poLayer->CreateField(&oFieldTCStatus) != OGRERR_NONE)
  {
    GDALClose(poDS);
    return erro_t::CREATE_FIELD_FAILED;
  }
#pragma endregion

#pragma region "写入数据到Shapefile"
  OGRFeature* poFeature = nullptr;
  OGRFeature* bgFeature = nullptr;
  for (const matching_record_t matching_record : matching_form.vmatching_record)
  {
    //  本底数据中要素的几何信息写入到其中
    bgFeature = background_data_layer->GetFeature(std::stoll(matching_record.Data1_ID));
    if (bgFeature == nullptr)
    {
      //  TODO:将错误信息写入到日志中

      //  继续执行下一个匹配记录
      continue;
    }
    OGRGeometry* bg_feature_geometry = bgFeature->GetGeometryRef();
    if(bg_feature_geometry == nullptr)
    {
      //  TODO:将错误信息写入到日志中

      //  继续执行下一个匹配记录
      continue;
    }


    //  创建新的矢量要素
    poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
    if (poFeature == nullptr)
    {
      //  TODO:将错误信息写入到日志中

      //  继续执行下一个匹配记录
      continue;
    }
    poFeature->SetField("SMD", matching_record.SMD.c_str());
    poFeature->SetField("Attr_Sim", matching_record.Attr_Sim.c_str());
    poFeature->SetField("SameEleConf", matching_record.SameEleConf.c_str());
    poFeature->SetField("Data1_ID", matching_record.Data1_ID.c_str());
    poFeature->SetField("RelFacID", matching_record.RelFacID.c_str());
    poFeature->SetField("TCStatus", matching_record.TCStatus.c_str());
    poFeature->SetGeometry(bg_feature_geometry);


    if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
    {
      OGRFeature::DestroyFeature(poFeature);
      OGRFeature::DestroyFeature(bgFeature);
      //  TODO:将错误信息写入到日志中

      //  继续执行下一个匹配记录
      continue; 
    }

    OGRFeature::DestroyFeature(poFeature);
    OGRFeature::DestroyFeature(bgFeature);
  }

  GDALClose(poDS);
#pragma endregion

#pragma endregion

  return erro_t::NO_ERRO;
}


void qgs_consistency_matching::saveState()
{
  QgsSettings settings;
  // 指定设置保存到Plugins节
  settings.beginGroup("consistency_matching_tool");
  settings.setValue(QStringLiteral("consistency_matching_tool/m_background_data_path"), m_background_data_path);
  settings.setValue(QStringLiteral("consistency_matching_tool/m_matching_data_path"), m_matching_data_path);
  settings.setValue(QStringLiteral("consistency_matching_tool/m_config_file_path"), m_config_file_path);
  settings.setValue(QStringLiteral("consistency_matching_tool/m_log_saving_path"), m_log_saving_path);
  settings.setValue(QStringLiteral("consistency_matching_tool/m_output_data_path"), m_output_data_path);
  // 结束分组，非常重要，确保设置正确保存在指定的节中
  settings.endGroup();
}

void qgs_consistency_matching::restoreState()
{
  QgsSettings settings;
  // 指定从Plugins节读取设置
  settings.beginGroup("consistency_matching_tool");
  m_background_data_path = settings.value(QStringLiteral("consistency_matching_tool/m_background_data_path"), QDir::homePath()).toString();
  m_matching_data_path = settings.value(QStringLiteral("consistency_matching_tool/m_matching_data_path"), QDir::homePath()).toString();
  m_config_file_path = settings.value(QStringLiteral("consistency_matching_tool/m_config_file_path"), QDir::homePath()).toString();
  m_log_saving_path = settings.value(QStringLiteral("consistency_matching_tool/m_log_saving_path"), QDir::homePath()).toString();
  m_output_data_path = settings.value(QStringLiteral("consistency_matching_tool/m_output_data_path"), QDir::homePath()).toString();
}

void qgs_consistency_matching::QPushButton_background_browse()
{
  // 定位到上一次保存的本底数据所在的文件
  QString curPath = this->m_background_data_path;
  QString dlgTile = QObject::tr("选择本底数据所在的文件夹");
  // 使用 QFileDialog 弹出窗口让用户选择目录
  QString selectedDir = QFileDialog::getExistingDirectory(this, dlgTile, curPath, QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    //  将选择的本底数据所在的文件目录显示出来
    this->m_SE_ConsistencyMatchingGuiBase->QLineEdit_background_input_data_path->setText(selectedDir);
    //  应该将本底数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_background_data_path = QString::fromUtf8(utf8Path.data());
  }

}

void qgs_consistency_matching::QPushButton_matching_browse()
{
  // 定位到上一次保存的匹配数据所在的文件
  QString curPath = this->m_matching_data_path;
  QString dlgTile = QObject::tr("选择匹配数据所在的文件夹");
  // 使用 QFileDialog 弹出窗口让用户选择目录
  QString selectedDir = QFileDialog::getExistingDirectory(this, dlgTile, curPath, QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    //  将选择的匹配数据所在的文件目录显示出来
    this->m_SE_ConsistencyMatchingGuiBase->QLineEdit_matching_input_data_path->setText(selectedDir);
    //  应该将匹配数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_matching_data_path = QString::fromUtf8(utf8Path.data());
  }
}

void qgs_consistency_matching::QPushButton_log_browse()
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
    this->m_SE_ConsistencyMatchingGuiBase->QLineEdit_log_saving_path->setText(fileName);
    // 处理路径，替换路径分隔符，转换编码
    QString processedPath = fileName.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    this->m_log_saving_path = QString::fromUtf8(utf8Path.data());
  }
}



void qgs_consistency_matching::QPushButton_output_data_path_browse()
{
  // 定位到上一次保存的输出数据所在的文件
  QString curPath = this->m_output_data_path;
  QString dlgTile = QObject::tr("选择输出数据所在的文件夹");
  // 使用 QFileDialog 弹出窗口让用户选择目录
  QString selectedDir = QFileDialog::getExistingDirectory(this, dlgTile, curPath, QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    //  将选择的输出数据所在的文件目录显示出来
    this->m_SE_ConsistencyMatchingGuiBase->QLineEdit_output_data_path->setText(selectedDir);
    //  应该将输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_output_data_path = QString::fromUtf8(utf8Path.data());
  }
}

//  目前的方案不需要自动生成配置文件，因此这部分内容暂时用不到
void qgs_consistency_matching::QPushBotton_generate_config_file()
{
#pragma region "1、检查本底数据路径是否有效"
  //  根据输入的本底数据和匹配数据生成配置文件
  //  1、检查本底数据路径是否有效
  if (is_dir_exist(get_m_background_data_path()) != erro::DIR_IS_EXIST)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("生成配置文件失败（原因：本底数据目录不存在）！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion

#pragma region "2、检查匹配数据路径是否有效"
  //  2、检查匹配数据路径是否有效
  if (is_dir_exist(get_m_matching_data_path()) != erro::DIR_IS_EXIST)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("生成配置文件失败（原因：匹配数据目录不存在）！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion

#pragma region "3、检查日志数据路径是否有效"
  //  3、检查日志数据路径是否有效
  if (is_file_exist(get_m_log_saving_path()) != erro::FILE_IS_EXIST)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("生成配置文件失败（原因：日志文件不存在）！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion

#pragma region "4、阈值等级这里不进行检查"
#pragma endregion

#pragma region "5、生成“匹配字段选择和字段权重设置”配置文件"
  // 1）获取给定本底数据路径中的图层
  std::vector<std::string> background_layers = getLayers(get_m_background_data_path().toStdString());
  // 2）获取给定匹配补充数据路径中的图层
  std::vector<std::string> matching_layers = getLayers(get_m_matching_data_path().toStdString());
  // 3）创建映射 
  std::vector<std::string> mapping = createMapping(background_layers, matching_layers);
  // 4）将映射表单写入到特定路径即m_config_file_path中的csv文件中(将映射表单写入到特定路径的csv文件中)
  erro_t result = writeMapping2csv(mapping, get_m_log_saving_path().toStdString());
  if (result != erro_t::NO_ERRO)
  {
    //  TODO：将错误码和字符串之间建立联系
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("生成”匹配字段选择和字段权重设置“配置文件失败！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  else
  {
    //  TODO：将错误码和字符串之间建立联系
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("生成”匹配字段选择和字段权重设置“配置文件成功！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion
}

void qgs_consistency_matching::QPushBotton_load_config_file()
{
#pragma region "1、构建配置文件所在的路径、检查配置文件是否存在"
  //	构建配置文件所在的路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString StrFileName = curPath + "/consistency_matching_toolbox_configuraion_files/consistency_matching_tool_config_file.csv";

  //  windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8
  QString processedPath = StrFileName.replace("\\", "/");
  QByteArray utf8Path = processedPath.toUtf8();
  QString Utf8StrFileName = QString::fromUtf8(utf8Path.data());

  //  检查配置文件是否存在
  erro_t is_file_exist_flag = is_file_exist(Utf8StrFileName);
  if (is_file_exist_flag != erro_t::FILE_IS_EXIST)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("配置文件读取失败（原因：配置文件不存在，请创建consistency_matching_tool_config_file.csv配置文件）！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion

#pragma region "2、如果配置文件存在，则读取配置文件中的内容"

  erro_t parse_config_file_flag = parse_config_file(Utf8StrFileName);

  if (parse_config_file_flag != erro_t::NO_ERRO)
  {
    //  读取配置文件失败！
    //  TODO：将错误码和字符串之间建立联系
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("读取”匹配字段选择和字段权重设置“配置文件失败！"));
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
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("读取”匹配字段选择和字段权重设置“配置文件成功！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    //  需要提示读取配置文件成功
  }
#pragma endregion

#pragma region "3、检查读取到配置文件中的内容是否有效"
  erro_t is_config_file_info_t_valid_flag = is_config_file_info_t_valid();
  if (is_config_file_info_t_valid_flag != erro_t::CONFIG_FILE_INFO_T_IS_VALID)
  {
    //  “匹配字段选择和字段权重设置”配置文件内容无效！
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("一致性匹配工具"));
    msgBox.setText(tr("“匹配字段选择和字段权重设置”配置文件内容无效！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
  else
  {
    //  “匹配字段选择和字段权重设置”配置文件内容有效！
    //  “匹配字段选择和字段权重设置”配置文件内容有效的时候不需要进行提示
  }
#pragma endregion

}

std::vector<std::string> qgs_consistency_matching::getLayers(const std::string& base_data_path)
{
  std::vector<std::string> layer_paths;  // 用于存储.shp文件的路径

  try
  {
    // 检查路径是否存在并为目录
    if (std::filesystem::exists(base_data_path) && std::filesystem::is_directory(base_data_path))
    {
      // 遍历给定目录下的所有文件和目录
      for (const auto& entry : std::filesystem::directory_iterator(base_data_path))
      {
        // 确保是文件
        if (entry.is_regular_file())
        {  
            // 获取文件的路径
          auto path = entry.path();
          // 检查文件扩展名是否为.shp
          if (path.extension() == ".shp")
          {
            // 将文件的绝对路径添加到vector中
            layer_paths.push_back(path.string());
          }
        }
      }
    }
  }
  catch (const std::filesystem::filesystem_error& e)
  {  // 捕获可能的文件系统错误
    std::cerr << "文件系统错误: " << e.what() << '\n';
  }
  catch (const std::exception& e) {  // 捕获所有其他类型的异常
    std::cerr << "标准异常: " << e.what() << '\n';
  }

  return layer_paths;
}

std::vector<std::string> qgs_consistency_matching::createMapping(
  const std::vector<std::string>& background_layers,
  const std::vector<std::string>& matching_layers)
{
  // 用于存储所有配对的vector
  std::vector<std::string> mappings;  

  // 遍历background_layers中的每一个图层
  for (const std::string& bg_layer : background_layers)
  {
    // 遍历matching_layers中的每一个图层
    for (const std::string& match_layer : matching_layers)
    {
      // 将两个图层的路径进行拼接，这里用逗号分隔
      std::string mapping = bg_layer + "<--->" + match_layer;
      // 将拼接后的字符串添加到结果vector中
      mappings.push_back(mapping);
    }
  }
  // 返回包含所有配对的vector
  return mappings;  
}

erro_t qgs_consistency_matching::writeMapping2csv(
  const std::vector<std::string>& mappings,
  const std::string& config_file_path)
{
  //  1、首先检查config_file_path是否存在
  erro_t flag = is_file_exist(QString::fromStdString(config_file_path));
  if (flag == erro_t::FILE_IS_NOT_EXIST)
  {
    return erro_t::CONFIG_FILE_IS_NOT_EXIST;
  }
  else
  {
    /*
      说明配置文件是存在的, 将mappings中的东西写入到xml中,
    */  
    for (size_t i = 0; i < mappings.size(); i++)
    {
#pragma region "得到本底数据和匹配数据全路径"
      //  首先得到mappings中的当前元素
      std::string current_pair = mappings[i];
      //  解析得到current_pair中的两个图层
      size_t background_data_path_start = current_pair.find_last_of("<");
      size_t matching_data_path_start = current_pair.find_last_of(">");
      std::string background_data_path = current_pair.substr(0, background_data_path_start);
      std::string matching_data_path = current_pair.substr(matching_data_path_start + 1);
#pragma endregion

#pragma region "得到本底数据的各个字段"
      std::vector<std::string> background_single_layer_fields;
      get_single_layer_fields(background_data_path, background_single_layer_fields);
#pragma endregion

#pragma region "得到匹配数据的各个字段"
      std::vector<std::string> mathcing_single_layer_fields;
      get_single_layer_fields(matching_data_path, mathcing_single_layer_fields);
#pragma endregion

#pragma region "将配对字段写入到csv中"
      for (size_t i = 0; i < background_single_layer_fields.size(); i++)
      {
        for (size_t j = 0; j < mathcing_single_layer_fields.size(); j++)
        {
          write_info2csv(
            config_file_path,
            background_data_path,
            background_single_layer_fields[i],
            matching_data_path,
            mathcing_single_layer_fields[j],
            "0");
        }
      }
#pragma endregion

    }

    return erro_t::NO_ERRO;
  }
}

erro_t qgs_consistency_matching::get_single_layer_fields(
  const std::string& single_layer_path,
  std::vector<std::string>& fields)
{
  // 初始化GDAL库
  GDALAllRegister();
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  CPLSetConfigOption("SHAPE_ENCODING", "UTF-8");

  // 打开数据集(定义GDAL数据集指针)
  GDALDataset* poDS = (GDALDataset*)GDALOpenEx(single_layer_path.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);

  if (poDS == NULL)
  {
    return erro_t::OPEN_GDALDATASET_FAILED;  // 如果数据集为空，返回失败
  }

  // 获取第一个图层
  OGRLayer* poLayer;
  poLayer = poDS->GetLayer(0);
  if (poLayer == NULL)
  {
    GDALClose(poDS);  // 关闭数据集
    return erro_t::OPEN_FIRST_LAYER_FAILED;  // 如果图层为空，返回失败
  }

  // 获取字段定义并添加到fields向量中
  OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
  int nFields = poFDefn->GetFieldCount();
  for (int i = 0; i < nFields; i++)
  {
    OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(i);
    fields.push_back(poFieldDefn->GetNameRef());  // 将字段名称添加到向量中
  }

  // 关闭数据集
  GDALClose(poDS);

  return erro_t::GET_SINGLE_LAYER_FIELDS_SUCCESSFUL;  // 返回成功
}

erro_t qgs_consistency_matching::write_info2csv(
  const std::string& config_file_path,
  const std::string& background_data_path,
  const std::string& background_single_layer_fields,
  const std::string& matching_data_path,
  const std::string& mathcing_single_layer_fields,
  const std::string weight)
{
  QString qstrFileName = QString::fromStdString(config_file_path);
  if (is_file_exist(qstrFileName) == erro_t::FILE_IS_EXIST)
  {
    // 初始化GDAL
    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
    CPLSetConfigOption("SHAPE_ENCODING", "UTF8");

    // 获取CSV驱动
    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("CSV");
    if (poDriver == NULL)
    {
      return erro_t::GET_CSV_DRIVER_FAILED;
    }

    std::string strOutputCSV = qstrFileName.toUtf8();

    // 创建CSV文件
    GDALDataset* poDS;
    poDS = poDriver->Create(strOutputCSV.c_str(), 0, 0, 0, GDT_Unknown, NULL);
    if (poDS == NULL)
    {
      return erro_t::CREATE_GDALDATASET_FAILED;
    }

    std::string strCSVName = CPLGetBasename(strOutputCSV.c_str());

    // 获取图层，如果没有则创建
    OGRLayer* poLayer = poDS->CreateLayer(strCSVName.c_str(), NULL, wkbNone, NULL);
    if (poLayer == NULL)
    {
      return erro_t::CREATE_OGRLAYER_FAILED;
    }

#pragma region "创建图层字段"
    // 创建“本底图层”字段
    QString qstrBaseLayer = tr("本底数据图层");
    std::string strBaseLayer = qstrBaseLayer.toUtf8().constData();
    OGRFieldDefn oBaseLayerField(strBaseLayer.c_str(), OFTString);
    oBaseLayerField.SetWidth(500);
    if (poLayer->CreateField(&oBaseLayerField) != OGRERR_NONE)
    {
      return erro_t::CREATE_FIELD_FAILED;
    }

    // 创建“本底图层匹配字段”字段
    QString qstrBaseMatchField = tr("本底图层匹配字段");
    std::string strBaseMatchField = qstrBaseMatchField.toUtf8().constData();
    OGRFieldDefn oBaseMatchField(strBaseMatchField.c_str(), OFTString);
    oBaseMatchField.SetWidth(100);
    if (poLayer->CreateField(&oBaseMatchField) != OGRERR_NONE)
    {
      return erro_t::CREATE_FIELD_FAILED;
    }

    // 创建“匹配数据图层”字段
    QString qstrMatchLayer = tr("匹配数据图层");
    std::string strMatchLayer = qstrMatchLayer.toUtf8().constData();
    OGRFieldDefn oMatchLayerField(strMatchLayer.c_str(), OFTString);
    oMatchLayerField.SetWidth(500);
    if (poLayer->CreateField(&oMatchLayerField) != OGRERR_NONE)
    {
      return erro_t::CREATE_FIELD_FAILED;
    }

    // 创建“匹配数据图层匹配字段”字段
    QString qstrMatchMatchField = tr("匹配图层匹配字段");
    std::string strMatchMatchField = qstrMatchMatchField.toUtf8().constData();
    OGRFieldDefn oMatchMatchField(strMatchMatchField.c_str(), OFTString);
    oMatchMatchField.SetWidth(100);
    if (poLayer->CreateField(&oMatchMatchField) != OGRERR_NONE)
    {
      return erro_t::CREATE_FIELD_FAILED;
    }

    // 创建“权重”字段
    QString qstrWeightField = tr("匹配字段对权重");
    std::string strWeightField = qstrWeightField.toUtf8().constData();
    OGRFieldDefn oWeightField(strWeightField.c_str(), OFTString);
    oWeightField.SetWidth(10);
    if (poLayer->CreateField(&oWeightField) != OGRERR_NONE)
    {
      return erro_t::CREATE_FIELD_FAILED;
    }

#pragma endregion

#pragma region "创建要素"
    // 创建要素
    OGRFeature* poFeature;
    poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
    poFeature->SetField(0, background_data_path.c_str());
    poFeature->SetField(1, background_single_layer_fields.c_str());
    poFeature->SetField(2, matching_data_path.c_str());
    poFeature->SetField(3, mathcing_single_layer_fields.c_str());
    poFeature->SetField(4, weight.c_str());

    if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
    {
      return erro_t::CREATE_FEATURE_FAILED;
    }

    // 清理
    OGRFeature::DestroyFeature(poFeature);

#pragma endregion

    GDALClose(poDS);
    return erro_t::NO_ERRO;
  }
  else
  {
    return erro_t::CONFIG_FILE_IS_NOT_EXIST;
  }
}

#pragma endregion
