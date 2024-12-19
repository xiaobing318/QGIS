#pragma region "头文件"
/*-------------STL-------------*/
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>

/*---------------GDAL--------------------*/
#include <gdal.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>
#include <gdal_version.h>
#include <cpl_error.h>
#include <cpl_string.h>
#include <ogrsf_frmts.h>
#include <ogr_feature.h>
#include <gdal_priv.h>
#include <cpl_conv.h>

/*-------------QT-------------*/
#include <QFile>
#include <QDomDocument>
#include <QDebug>
#include <QDomNode>
#include <QDomElement>
#include <QCoreApplication>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QtXml/qdom.h>
#include <QPushButton>

/*-------------QGIS-------------*/
#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsgui.h"

/*-------------基础算法库-------------*/
#include "character_set_encoding/character_set_encoding_conversion.h"

/*-------------对应头文件-------------*/
#include "source_data_inspection_tool.h"

using namespace std;

#pragma endregion

#pragma region "构造函数、析构函数"
qgs_source_data_inspection_tool::qgs_source_data_inspection_tool(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);

	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.Button_input_data_view, &QPushButton::clicked, this, &qgs_source_data_inspection_tool::Button_input_data_view);
	connect(ui.Button_output_data_view, &QPushButton::clicked, this, &qgs_source_data_inspection_tool::Button_log_file_output_data_view);
	connect(ui.Button_OK, &QPushButton::clicked, this, &qgs_source_data_inspection_tool::Button_OK);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &qgs_source_data_inspection_tool::Button_Cancel);
  connect(ui.comboBox_character_set, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &qgs_source_data_inspection_tool::QComboBox_get_character_set_encoding);

  restore_states();
}

qgs_source_data_inspection_tool::~qgs_source_data_inspection_tool()
{
  //  保存用户当前设置的一些参数
  store_states();

}
#pragma endregion

#pragma region "类成员对象set、get函数"
//  获取输入数据目录成员函数
QString qgs_source_data_inspection_tool::get_m_input_data_path()
{
  return m_input_data_path;
}

//  获取输出数据目录成员函数
QString qgs_source_data_inspection_tool::get_m_log_file_output_path()
{
  return m_log_file_output_path;
}

//  获取数据源类型
int qgs_source_data_inspection_tool::get_input_data_src_type()
{
  if (ui.radioButton_JBDX->isChecked())
  {
    return 1;
  }
  else if (ui.radioButton_S57->isChecked())
  {
    return 2;
  }
  else if (ui.radioButton_QQCT->isChecked())
  {
    return 3;
  }
  //	如果全都没有被选择的话将会返回5
  return -1;
}

//  获取字符集编码
std::string qgs_source_data_inspection_tool::get_character_set_type()
{
  // 获取QComboBox当前选中的文本内容
  QString currentText = ui.comboBox_character_set->currentText();

  // 将QString转换为std::string
  std::string currentTextStd = currentText.toUtf8();

  return currentTextStd;
}

//  设置输入数据目录成员函数
void qgs_source_data_inspection_tool::set_m_input_data_path(const std::string& input_data_path)
{
  m_input_data_path = QString::fromStdString(input_data_path);
}

//  设置输出数据目录成员函数
void qgs_source_data_inspection_tool::set_m_log_file_output_path(const std::string& output_data_path)
{
  m_log_file_output_path = QString::fromStdString(output_data_path);
}

//  设置字符集编码
void qgs_source_data_inspection_tool::set_character_set_type(const QString& character_set_type)
{
  m_character_set_type = character_set_type;
}


//  恢复用户上一次设置的一些参数
void qgs_source_data_inspection_tool::restore_states()
{
  //  从上一次保存的路径数据中得到输入输出数据
  QgsSettings settings;
  m_input_data_path = settings.value(QStringLiteral("qgs_source_data_inspection_tool/m_input_data_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
  m_log_file_output_path = settings.value(QStringLiteral("qgs_source_data_inspection_tool/m_log_file_output_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

  //  填充输入输出数据路径，显示在交互界面上
  ui.lineEdit_input_data_path->setText(m_input_data_path);
  ui.lineEdit_log_file_output_path->setText(m_log_file_output_path);

  //  从上一次保存的下拉选择得到字符集编码
  QString character_set = settings.value(QStringLiteral("qgs_source_data_inspection_tool/character_set"), "从cpg文件中读取", QgsSettings::Section::Plugins).toString();
  
  //  默认选择的是JBDX数据源
  ui.radioButton_JBDX->setChecked(true);
  //  默认选择的是“从cpg文件中读取”
  ui.comboBox_character_set->setCurrentText(character_set);
}

//  保存用户当前设置的一些参数
void qgs_source_data_inspection_tool::store_states()
{
  //  设置当前保存路径
  QgsSettings settings;
  settings.setValue(QStringLiteral("qgs_source_data_inspection_tool/m_input_data_path"), m_input_data_path, QgsSettings::Section::Plugins);
  settings.setValue(QStringLiteral("qgs_source_data_inspection_tool/m_log_file_output_path"), m_log_file_output_path, QgsSettings::Section::Plugins);
  //  设置字符集编码下拉框
  settings.setValue(QStringLiteral("qgs_source_data_inspection_tool/character_set"), m_character_set_type, QgsSettings::Section::Plugins);
}
#pragma endregion

#pragma region "私有函数：读取配置文件相关的函数"

/*2024-04-03: S57属性表结构(附加了“标识号范围”内容)*/
void qgs_source_data_inspection_tool::Load_S57_fields_info4each_layer_config_file(S57_fields4all_layers_t& S57_fields4all_layers)
{
  /*
  函数功能：
    函数从 `S57_fields_info4each_layer.xml` 配置文件中加载S57图层字段信息。
  这些信息被解析并存储在一个自定义数据结构 `S57_fields4all_layers_t` 中，以便后续操作。

  算法流程：
    1. 构建 XML 配置文件的完整路径。
    2. 打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中。
    3. 遍历 XML 文档的根节点下的每一个子节点（标记为 "S57_layer"）。
    4. 对于每个 "S57_layer" 节点，提取其中的 "S57_layer_name"、"S57_layer_alias_name" 等信息，并存储在一个临时的 `S57_single_layer_t` 结构体中。
    5. 如果存在 "S57_fields"，则进一步提取其下的 "field" 节点信息，包括字段名称、别名、标识号范围等，并存储在 `field_t` 类型的结构体中。
    6.
    7. 将所有提取的信息汇总到 `S57_layer` 结构体，并添加到 `S57_fields4all_layers_t` 列表中。
    8. 循环步骤 3-7，直到处理完所有 "S57_layer" 节点。
  */
#pragma region "1、构建XML配置文件的完整路径"
  //	构建到S57_fields_info4each_layer.xml的XML配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/DLHJ_XMLS/source_data_inspection_tool_xmls/S57_fields_info4each_layer_4_1.xml";
#pragma endregion

#pragma region "2、打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中"
  //	对XML配置文件进行检查
  checkAndDebugXML(strFileName);

  //	以读取模式打开XML文件
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    return;
  }

  //	解析XML内容并填充`QDomDocument`
  QDomDocument doc;
  if (!doc.setContent(&file))
  {
    file.close();
    return;
  }
  file.close();
#pragma endregion

#pragma region "3、遍历 XML 文档的根节点下的每一个子节点"
  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点QDomNode 节点
  QDomNode current_layer_node = root.firstChild();
  while (!current_layer_node.isNull())
  {
    //	创建一个临时的Layer，用来读取到的数据
    S57_single_layer_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "S57_layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_nodes = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_nodes.toElement();
        if (Layer_child_element.tagName() == "S57_layer_name")
        {
          temp_layer.S57_layer_name = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "S57_layer_alias_name")
        {
          temp_layer.S57_layer_alias_name = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "S57_fields")
        {
          // 获取Field_List的所有子节点
          QDomNodeList Field_List_all_child_nodes = Layer_child_nodes.childNodes();
          for (size_t j = 0; j < Field_List_all_child_nodes.size(); j++)
          {
            //	创建一个临时的field_t数据结构，用来存放读取到的数据
            S57_single_field_t S57_single_field;

            //	获取Field_List的所有子节点的第j个子节点
            QDomNode Field = Field_List_all_child_nodes.at(j);

            //	判断当前节点的标签是否为Field
            if (Field.toElement().tagName() == "field")
            {
              //	获取当前Field节点的所有子节点
              QDomNodeList Field_all_child_nodes = Field.childNodes();
              //	循环处理Field节点的所有子节点
              for (size_t k = 0; k < Field_all_child_nodes.size(); k++)
              {
                //	获取Field节点的所有子节点的第k个子节点
                QDomNode Field_child_node = Field_all_child_nodes.at(k);
                QDomElement Field_child_element = Field_child_node.toElement();

                // 依次判断字段节点下所有节点的名称
                if (Field_child_element.tagName() == "field_name")
                {
                  S57_single_field.field_name = Field_child_element.text().toStdString();
                }
                else if (Field_child_element.tagName() == "field_name_alias")
                {
                  S57_single_field.field_name_alias = Field_child_element.text().toStdString();
                }
                else if (Field_child_element.tagName() == "identification_range")
                {
                  S57_single_field.identification_range = Field_child_element.text().toStdString();
                }
              }
            }

            //	将读取到的Layer数据存放到上一层数据结构中
            temp_layer.vS57_single_field.push_back(S57_single_field);
          }
        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    S57_fields4all_layers.vS57_single_layer.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion

}

/*2024-04-03: S57国家代码结构*/
void qgs_source_data_inspection_tool::Load_S57_country_code_config_file(S57_country_list_t& S57_country_list)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建到S57_country_code.xml的XML配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/DLHJ_XMLS/source_data_inspection_tool_xmls/S57_country_code.xml";
#pragma endregion

#pragma region "2、打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中"
  //	对XML配置文件进行检查
  checkAndDebugXML(strFileName);

  //	以读取模式打开XML文件
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    return;
  }

  //	解析XML内容并填充`QDomDocument`
  QDomDocument doc;
  if (!doc.setContent(&file))
  {
    file.close();
    return;
  }
  file.close();
#pragma endregion

#pragma region "3、遍历 XML 文档的根节点下的每一个子节点"
  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点QDomNode 节点
  QDomNode current_country_node = root.firstChild();
  while (!current_country_node.isNull())
  {
    //	创建一个临时的country，用来读取到的数据
    S57_single_country_t temp_single_country;

    // 当前节点的标签如果是country的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_country_node.toElement().tagName() == "country")
    {
      //	获取当前节点的所有子节点
      QDomNodeList current_country_node_all_child_nodes = current_country_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < current_country_node_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode current_country_node_current_child_nodes = current_country_node_all_child_nodes.at(i);
        QDomElement current_country_node_current_child_element = current_country_node_current_child_nodes.toElement();
        if (current_country_node_current_child_element.tagName() == "country_name")
        {
          temp_single_country.country_name = current_country_node_current_child_element.text().toStdString();
        }
        else if (current_country_node_current_child_element.tagName() == "country_code")
        {
          temp_single_country.country_code = current_country_node_current_child_element.text().toStdString();
        }      
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    S57_country_list.vS57_single_country.push_back(temp_single_country);
    //读取下一个兄弟节点
    current_country_node = current_country_node.nextSibling();
  }
#pragma endregion

}

/*2024-04-03: S57生产者编号结构*/
void qgs_source_data_inspection_tool::Load_S57_producer_number_config_file(S57_Producer_number_List_t& S57_Producer_number_List)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建到S57_country_code.xml的XML配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/DLHJ_XMLS/source_data_inspection_tool_xmls/S57_producer_number.xml";
#pragma endregion

#pragma region "2、打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中"
  //	对XML配置文件进行检查
  checkAndDebugXML(strFileName);

  //	以读取模式打开XML文件
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    return;
  }

  //	解析XML内容并填充`QDomDocument`
  QDomDocument doc;
  if (!doc.setContent(&file))
  {
    file.close();
    return;
  }
  file.close();
#pragma endregion

#pragma region "3、遍历 XML 文档的根节点下的每一个子节点"
  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点QDomNode 节点
  QDomNode current_producer_number_node = root.firstChild();
  while (!current_producer_number_node.isNull())
  {
    //	创建一个临时的producer_number，用来读取到的数据
    S57_single_producer_number_t temp_S57_single_producer_number;

    // 当前节点的标签如果是country的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_producer_number_node.toElement().tagName() == "producer_number")
    {
      //	获取当前节点的所有子节点
      QDomNodeList current_producer_number_node_all_child_nodes = current_producer_number_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < current_producer_number_node_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode current_producer_number_node_current_child_nodes = current_producer_number_node_all_child_nodes.at(i);
        QDomElement current_producer_number_node_current_child_element = current_producer_number_node_current_child_nodes.toElement();
        if (current_producer_number_node_current_child_element.tagName() == "value")
        {
          temp_S57_single_producer_number.value = current_producer_number_node_current_child_element.text().toStdString();
        }
        else if (current_producer_number_node_current_child_element.tagName() == "meaning")
        {
          temp_S57_single_producer_number.meaning = current_producer_number_node_current_child_element.text().toStdString();
        }
        else if (current_producer_number_node_current_child_element.tagName() == "scale")
        {
          temp_S57_single_producer_number.scale = current_producer_number_node_current_child_element.text().toStdString();
        }
        else if (current_producer_number_node_current_child_element.tagName() == "note")
        {
          temp_S57_single_producer_number.note = current_producer_number_node_current_child_element.text().toStdString();
        }      
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    S57_Producer_number_List.vS57_single_producer_number.push_back(temp_S57_single_producer_number);
    //读取下一个兄弟节点
    current_producer_number_node = current_producer_number_node.nextSibling();
  }
#pragma endregion


}

/*2024-04-03: S57表1-3海图制图数据要素结构*/
void qgs_source_data_inspection_tool::Load_S57_chart_cartographic_data_elements_1_3(S57_chart_cartographic_data_elements_layer_list_t& S57_chart_cartographic_data_elements_layer_list)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建到S57_country_code.xml的XML配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/DLHJ_XMLS/source_data_inspection_tool_xmls/S57_chart_cartographic_data_elements_1_3.xml";
#pragma endregion

#pragma region "2、打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中"
  //	对XML配置文件进行检查
  checkAndDebugXML(strFileName);

  //	以读取模式打开XML文件
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    return;
  }

  //	解析XML内容并填充`QDomDocument`
  QDomDocument doc;
  if (!doc.setContent(&file))
  {
    file.close();
    return;
  }
  file.close();
#pragma endregion

#pragma region "3、遍历 XML 文档的根节点下的每一个子节点"
  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点QDomNode 节点
  QDomNode current_layer_node = root.firstChild();
  while (!current_layer_node.isNull())
  {
    //	创建一个临时的S57_chart_cartographic_data_elements_layer，用来读取到的数据
    S57_chart_cartographic_data_elements_layer_t temp_S57_chart_cartographic_data_elements_layer;

    // 当前节点的标签如果是country的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "S57_chart_cartographic_data_elements_layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode current_layer_child_node = layer_all_child_nodes.at(i);
        QDomElement current_layer_child_node_element = current_layer_child_node.toElement();
        if (current_layer_child_node_element.tagName() == "S57_chart_cartographic_data_elements_layer_ID")
        {
          temp_S57_chart_cartographic_data_elements_layer.S57_chart_cartographic_data_elements_layer_ID = current_layer_child_node_element.text().toStdString();
        }
        else if (current_layer_child_node_element.tagName() == "S57_Object_label_name")
        {
          temp_S57_chart_cartographic_data_elements_layer.S57_Object_label_name = current_layer_child_node_element.text().toStdString();
        }
        else if (current_layer_child_node_element.tagName() == "S57_Object_label_meaning")
        {
          temp_S57_chart_cartographic_data_elements_layer.S57_Object_label_meaning = current_layer_child_node_element.text().toStdString();
        }
        else if (current_layer_child_node_element.tagName() == "S57_OBJL_filed_attribute_value")
        {
          temp_S57_chart_cartographic_data_elements_layer.S57_OBJL_filed_attribute_value = current_layer_child_node_element.text().toStdString();
        }
        else if (current_layer_child_node_element.tagName() == "geometry_type_list")
        {
          //	获取当前节点的所有子节点
          QDomNodeList geometry_type_list_all_child_nodes = current_layer_child_node.childNodes();
          for (size_t j = 0; j < geometry_type_list_all_child_nodes.size(); j++)
          {
            //	获取当前节点的所有子节点的第i个子节点
            QDomNode current_geometry_type_list_child_node = geometry_type_list_all_child_nodes.at(i);
            QDomElement current_geometry_type_list_child_node_element = current_geometry_type_list_child_node.toElement();
            if (current_geometry_type_list_child_node_element.tagName() == "point")
            {
              temp_S57_chart_cartographic_data_elements_layer.geometry_type_list.point = current_geometry_type_list_child_node_element.text().toStdString();
            }
            else if (current_geometry_type_list_child_node_element.tagName() == "line")
            {
              temp_S57_chart_cartographic_data_elements_layer.geometry_type_list.line = current_geometry_type_list_child_node_element.text().toStdString();
            }
            else if (current_geometry_type_list_child_node_element.tagName() == "polygon")
            {
              temp_S57_chart_cartographic_data_elements_layer.geometry_type_list.polygon = current_geometry_type_list_child_node_element.text().toStdString();
            }
          }

        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    S57_chart_cartographic_data_elements_layer_list.vS57_chart_cartographic_data_elements_layer.push_back(temp_S57_chart_cartographic_data_elements_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

#pragma endregion

#pragma region "私有槽函数"

void qgs_source_data_inspection_tool::Button_input_data_view()
{
  // 选择文件夹
  QString curPath = m_input_data_path;
  QString dlgTile = QObject::tr("请选择源数据检查工具输入数据源所在的目录位置");
  QString selectedDir = QFileDialog::getExistingDirectory(
    this,
    dlgTile,
    curPath,
    QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    ui.lineEdit_input_data_path->setText(selectedDir);
    m_input_data_path = selectedDir;
  }
}

void qgs_source_data_inspection_tool::Button_log_file_output_data_view()
{
  // 设置初始目录为上次用户选择的目录或用户的主目录
  QString curPath = m_log_file_output_path.isEmpty() ? QDir::homePath() : m_log_file_output_path;
  QString dlgTitle = QObject::tr("请选择源数据检查工具处理后日志文件存放的目录位置");

  // 调用文件保存对话框，用户可以选择目录并输入文件名
  QString selectedFilePath = QFileDialog::getSaveFileName(
    this,
    dlgTitle,
    curPath,
    tr("Log Files (*.log);;All Files (*.*)"), // 过滤器设置为只显示.log文件或所有文件
    nullptr,
    QFileDialog::Option::DontConfirmOverwrite); // 不自动确认覆盖，以便我们可以自定义提示

  // 检查用户是否选择了文件
  if (!selectedFilePath.isEmpty())
  {
    // 检查文件是否存在
    QFile file(selectedFilePath);
    if (file.exists())
    {
      // 如果文件已存在，询问用户是否覆盖
      QMessageBox::StandardButton reply;
      reply = QMessageBox::question(this, tr("确认覆盖"), tr("文件已存在。是否覆盖？"),
        QMessageBox::Yes | QMessageBox::No);
      if (reply == QMessageBox::No)
      {
        // 如果用户选择不覆盖，则返回，不执行任何操作
        return;
      }
    }

    // 尝试打开文件，以便写入，QIODevice::WriteOnly | QIODevice::Truncate 确保文件存在则覆盖
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      // 如果文件无法打开，显示错误消息
      QMessageBox::warning(this, tr("错误"), tr("无法创建或打开文件：%1").arg(selectedFilePath));
      return;
    }

    // 文件已创建或覆盖，这里可以写入文件的初始内容
    file.write("***********************************日志开始***********************************\n");

    // 更新UI和内部变量以反映新的文件路径
    ui.lineEdit_log_file_output_path->setText(selectedFilePath);
    m_log_file_output_path = selectedFilePath;

    // 关闭文件
    file.close();
  }
}

void qgs_source_data_inspection_tool::Button_OK()
{
  /*
  功能说明：
    1、当UI界面中的“确定”按钮被点击的时候，对不同来源的数据的命名进行检查
    2、UI界面中的一些其他内容是作为这部分的处理的参数

  算法流程：
    1. 验证用户输入的数据源路径和保存转换后数据的路径。
    2. 加载包含各种配置文件。
  */
#pragma region "（第一步）DLHJ：输入数据路径、输出数据路径检查"
  if (!is_exist_file_directory(get_m_input_data_path()))
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量源数据检查工具"));
    msgBox.setText(tr("输入数据目录不存在，请重新输入！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

  if (!is_exist_file(get_m_log_file_output_path()))
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量源数据检查工具"));
    msgBox.setText(tr("日志文件输出目录不存在，请重新输入！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion 

#pragma region "（第二步）获取数据源类型、获取输出数据路径、获取保存处理后数据的路径"
  // 获取数据来源
  int input_data_src_type = get_input_data_src_type();

  //  设置字符集编码下拉框选项
  set_character_set_type(ui.comboBox_character_set->currentText());

  // 获取输入数据路径
  QString qstr_input_data_path = get_m_input_data_path();
  string str_input_data_path = qstr_input_data_path.toLocal8Bit();

  // 获取日志文件输出保存目录
  QString qstr_log_file_output_path = get_m_log_file_output_path();
  string str_log_file_output_path = qstr_log_file_output_path.toLocal8Bit();

  // 获取图幅列表
  QStringList qstr_input_data_path_list = get_sub_directories(qstr_input_data_path);

#pragma endregion

#pragma region "（第三步）DLHJ：将配置文件读取到自定义结构体中（从用户的角度来说，并不会关心配置文件，因此关于配置文件的检查将会在另外的部分进行检查）"
  //  JBDX数据源相关的配置文件结构体

  //  S57数据源相关的配置文件结构体
  S57_fields4all_layers_t S57_fields4all_layers;
  S57_country_list_t S57_country_list;
  S57_Producer_number_List_t S57_Producer_number_List;
  S57_chart_cartographic_data_elements_layer_list_t S57_chart_cartographic_data_elements_layer_list;

  //  QQCT数据源相关的配置文件结构体

  //	根据不同的数据来源加载图层配置文件
  if (input_data_src_type == 1)
  {
    //  JBDX数据源相关的配置文件

  }
  else if (input_data_src_type == 2)
  {
    //  S57数据源相关的配置文件
    Load_S57_fields_info4each_layer_config_file(S57_fields4all_layers);
    if (S57_fields4all_layers.vS57_single_layer.size() == 0)
    {
      //  说明读取S57所有图层字段信息出现了问题，需要进行排查
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("矢量源数据检查工具"));
      msgBox.setText(tr("读取S57_fields_info4each_layer_4_1.xml文件出现了问题，请查看S57_fields_info4each_layer_4_1.xml是否存在且正确！"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
    }
    Load_S57_country_code_config_file(S57_country_list);
    if (S57_country_list.vS57_single_country.size() == 0)
    {
      //  说明读取S57国家代码信息出现了问题，需要进行排查
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("矢量源数据检查工具"));
      msgBox.setText(tr("读取S57_country_code.xml文件出现了问题，请查看S57_country_code.xml是否存在且正确！"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
    }
    Load_S57_producer_number_config_file(S57_Producer_number_List);
    if (S57_Producer_number_List.vS57_single_producer_number.size() == 0)
    {
      //  说明读取S57生产者编码信息出现了问题，需要进行排查
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("矢量源数据检查工具"));
      msgBox.setText(tr("读取S57_producer_number.xml文件出现了问题，请查看S57_producer_number.xml是否存在且正确！"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
    }
    Load_S57_chart_cartographic_data_elements_1_3(S57_chart_cartographic_data_elements_layer_list);
    if (S57_chart_cartographic_data_elements_layer_list.vS57_chart_cartographic_data_elements_layer.size() == 0)
    {
      //  说明读取"S57电子海图数据文件命名正确性检查--表1-3海图制图数据要素"出现了问题，需要进行排查
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("矢量源数据检查工具"));
      msgBox.setText(tr("读取S57_chart_cartographic_data_elements_1_3.xml文件出现了问题，请查看S57_chart_cartographic_data_elements_1_3.xml是否存在且正确！"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
    }

  }
  else if (input_data_src_type == 3)
  {
    //  QQCT数据源相关的配置文件
  }
  else if (input_data_src_type = 4)
  {
    //  其他类型的数据源
  }
  else if (input_data_src_type = -1)
  {
    //  未知数据源
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量源数据检查工具"));
    msgBox.setText(tr("未知数据源，请检查源数据选择是否正确！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

#pragma endregion

#pragma region "（第四步）DLHJ：从集合中选择出部分正确的子集（有效的shp文件），然后利用YTH生成接口进行处理"

#pragma region "1、将多个有效的分幅数据存放在向量中，用作矢量源数据检查工具接口的参数"
  //	存储多个分幅数据的绝对路径
  std::vector<std::string> v_input_frame_data_path;
  v_input_frame_data_path.clear();

  /*
    qstr_input_data_path_list.size() == 0说明指定目录下没有子目录（qstr_input_data_path_list：多个图幅目录组成一个图幅列表）
  那么str_input_data_path这个路径下只会存在分幅路径（其中有多个shp文件），这样设计可以使得选中一个包含多个分幅数据
  的路径可以正常工作，也可以使得单个分幅数据正常工作（杨小兵-2023-9-9）
  */
  // 如果是单图幅，将这个单个图幅的路径保存到v_input_frame_data_path
  if (qstr_input_data_path_list.size() == 0)
  {
    //	str_input_data_path为输入数据路径
    v_input_frame_data_path.push_back(str_input_data_path);
  }
  else
  {
    //	将所有子目录的绝对路径添加到v_input_frame_data_path 中
    for (int i = 0; i < qstr_input_data_path_list.size(); i++)
    {
      std::string strPathTemp = qstr_input_data_path_list[i].toLocal8Bit();
      v_input_frame_data_path.push_back(strPathTemp);
    }
  }

  if (v_input_frame_data_path.size() == 0)
  {
    //  说明给定路径中的数据为空
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量源数据检查工具"));
    msgBox.setText(tr("给定输入源数据文件路径中不存在数据，请检查给定路径中是否存在数据！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

#pragma endregion

#pragma region "2、利用矢量源数据检查接口进行处理"

  int iResult = 0;
  if (input_data_src_type == 1)
  {
    //  JBDX

  }
  else if (input_data_src_type == 2)
  {
    //  S57
    iResult = S57_naming_correctness_check(
      S57_fields4all_layers,
      S57_country_list,
      S57_Producer_number_List,
      S57_chart_cartographic_data_elements_layer_list,
      v_input_frame_data_path,
      str_log_file_output_path
    );
  }
  else if (input_data_src_type == 3)
  {
    //  QQCT

  }
  else if (input_data_src_type == 4)
  {
    //  其他数据源

  }
  else if (input_data_src_type == -1)
  {
    //  未知数据源
  }

  //	语义融合处理失败
  if (iResult != 0)
  {
    // 记录日志
    QMessageBox msgBox1;
    msgBox1.setWindowTitle(tr("（DLHJ）矢量数据预处理工具箱!"));
    msgBox1.setText(tr("矢量源数据检查工具处理失败！"));
    msgBox1.setStandardButtons(QMessageBox::Yes);
    msgBox1.setDefaultButton(QMessageBox::Yes);
    msgBox1.exec();
    return;
  }
  //	矢量源数据检查工具处理完成
  else
  {
    // 记录日志
    QMessageBox msgBox1;
    msgBox1.setWindowTitle(tr("（DLHJ）矢量数据预处理工具箱!"));
    msgBox1.setText(tr("矢量源数据检查工具处理完成！"));
    msgBox1.setStandardButtons(QMessageBox::Yes);
    msgBox1.setDefaultButton(QMessageBox::Yes);
    msgBox1.exec();
    return;
  }
#pragma endregion

#pragma endregion


}

void qgs_source_data_inspection_tool::Button_Cancel()
{
  reject();
}

void qgs_source_data_inspection_tool::QComboBox_get_character_set_encoding()
{ 
    // 获取当前选中的字符集编码
  QString currentCharacterSet = ui.comboBox_character_set->currentText();
  m_character_set_type = currentCharacterSet;
  // 保存到QSettings
  QgsSettings settings;
  settings.setValue(QStringLiteral("qgs_source_data_inspection_tool/character_set"), currentCharacterSet, QgsSettings::Section::Plugins);

}

#pragma endregion

#pragma region "类辅助成员函数"
//  判断目录是否存在
bool qgs_source_data_inspection_tool::is_exist_file_directory(const QString& qstr_directory_path)
{
  QDir dir(qstr_directory_path);
  return dir.exists();
}

//  判断文件是否存在
bool qgs_source_data_inspection_tool::is_exist_file(const QString& qstr_file_path)
{
  return QFile::exists(qstr_file_path);
}

//	检查XML文件
void qgs_source_data_inspection_tool::checkAndDebugXML(const QString& strFileName) {
  // 检查文件是否存在
  if (!QFile::exists(strFileName)) {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("DLHJ（地理环境）"));
    QString message = tr("配置文件:") + strFileName + tr("不存在，请检查！");
    msgBox.setText(message);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

  // 检查文件是否可读
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("DLHJ（地理环境）"));
    QString message = tr("配置文件:") + strFileName + tr("不可读，请检查文件权限！");
    msgBox.setText(message);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

  // 利用<QDebug>类对XML数据文件可能存在的错误进行排查
  QDomDocument doc;
  QString errorMsg;  // 用于存储错误信息的字符串
  int errorLine;     // 用于存储出错的行号
  int errorColumn;   // 用于存储出错的列号
  if (!doc.setContent(&file, &errorMsg, &errorLine, &errorColumn)) {
    qDebug() << "Error parsing XML: " << errorMsg;
    qDebug() << "Line: " << errorLine << " Column: " << errorColumn;
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("DLHJ（地理环境）"));
    QString message = tr("XML文件(") + strFileName + tr(")解析出错！\n错误信息：");
    msgBox.setText(message + errorMsg +
      "\n行号：" + QString::number(errorLine) +
      "\n列号：" + QString::number(errorColumn));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    file.close();
    return;
  }
  file.close();
}

//  获取指定目录中的多个子目录
QStringList qgs_source_data_inspection_tool::get_sub_directories(const QString& file_directory_path)
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
#pragma endregion

#pragma region "源数据检查工具实际的逻辑处理函数"

#pragma region "JBDX数据源处理函数"

#pragma endregion

#pragma region "S57数据源处理函数"
/*
一、参数信息
1、各类配置文件
2、输入数据源文件路径
3、输出日志文件路径

二、返回值
  返回值为整数类型用来指示可能发生的错误

三、注
  这个函数假设的前提就是所有的参数存在并且是正确的，也就是说这些参数的检查将会在调用者调用这个函数之前检查这些参数的有效性
*/
int qgs_source_data_inspection_tool::S57_naming_correctness_check(
  const S57_fields4all_layers_t& S57_fields4all_layers,
  const S57_country_list_t& S57_country_list,
  const S57_Producer_number_List_t& S57_Producer_number_List,
  const S57_chart_cartographic_data_elements_layer_list_t& S57_chart_cartographic_data_elements_layer_list,
  const std::vector<std::string> v_input_frame_data_path,
  const std::string& str_log_file_output_path)
{

#pragma region "如果在输出目录中存在上一次剩余的相同名称的文件，那么需要删除"
  //  使用std::filesystem::path构造路径对象
  std::filesystem::path log_file_path{ str_log_file_output_path };
  /*创建一个文件流，用来向这个文件流中输出流数据（使用std::ofstream尝试打开文件。
  * 这个文件在选择文件的时候就已经被创建了
  */
  std::ofstream file_stream(log_file_path, std::ofstream::out);
  if (!file_stream.is_open())
  {
    // 如果文件无法打开（或创建），返回错误
    return 1;
  }
#pragma endregion

  size_t vector_size = v_input_frame_data_path.size();
#pragma region "向当前file_stream文件中输入统计信息"
  file_stream << "分幅数据一共有：" << vector_size << "个\n";

#pragma endregion

  for (size_t i = 0; i < vector_size; i++)
  {
    std::string temp_input_path = v_input_frame_data_path[i];
    std::string temp_output_path = str_log_file_output_path;
    //  将输入输出路径处理
    std::replace(temp_input_path.begin(), temp_input_path.end(), '\\', '/');
    std::replace(temp_output_path.begin(), temp_output_path.end(), '\\', '/');
    //  转化为UTF-8编码的
    character_set_encoding_conversion character_set_encoding_conversion_instance("UTF-8", "GBK");
    std::string str_input_data_path_utf8 = character_set_encoding_conversion_instance.execute_character_set_encoding_conversion(temp_input_path);
    std::string str_log_file_output_path_utf8 = character_set_encoding_conversion_instance.execute_character_set_encoding_conversion(temp_output_path);

    //  对分幅数据进行检查
    S57_single_framed_data_naming_correctness_check(
      S57_fields4all_layers,
      S57_country_list,
      S57_Producer_number_List,
      S57_chart_cartographic_data_elements_layer_list,
      str_input_data_path_utf8,
      file_stream);
  }


#pragma region "关闭资源"
  file_stream.close();
#pragma endregion
}

/*
一、解释
  这个函数是对S57单个图幅数据文件进行命名正确性检查

二、错误管理
1 ESRI Shapefile获取失败
2 分幅数据打开失败
3 分幅数据名称不是8位
4 前两位（生产者编码即国家代码）不是有效的
5 第三位为航海用途，不在《航海用途说明》的取值范围内，请检查配置文件
*/
int qgs_source_data_inspection_tool::S57_single_framed_data_naming_correctness_check(
  const S57_fields4all_layers_t& S57_fields4all_layers,
  const S57_country_list_t& S57_country_list,
  const S57_Producer_number_List_t& S57_Producer_number_List,
  const S57_chart_cartographic_data_elements_layer_list_t& S57_chart_cartographic_data_elements_layer_list,
  const std::string& str_input_data_path_utf8,
  std::ofstream& file_stream)
{
#pragma region "1、首先向file_stream这个文件中写入能够表示当前分幅数据的开始标识内容"
  {
    std::string message = "-----------------分幅数据（" + str_input_data_path_utf8 + "）-----------------\n";
    file_stream << message;
  }
#pragma endregion

#pragma region "2、将单个shp分幅数据目录路径下的所有图层读取到内存当中（打开资源）"
  //	对GDAL库的一些全局的设置（在整个项目中只需要设置一次就可以了）
  //  让GDAL库支持UTF-8编码的文件名路径
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  /*
  当`SHAPE_ENCODING`配置选项被设置为空字符串时，GDAL的行为是尝试自动识别Shapefile属性表中的编码。
  如果自动识别失败，或者数据没有使用特定的编码标记，GDAL通常会回退到使用系统默认的编码方式来处理文本。
  */
  std::string character_set_encoding;
  if (get_character_set_type() == "从cpg文件中读取")
  {
    character_set_encoding = "";
  }
  else
  {
    character_set_encoding = get_character_set_type();
  }
  CPLSetConfigOption("SHAPE_ENCODING", character_set_encoding.c_str());
  //	注册所有可用的驱动器
  GDALAllRegister();
  //	利用所有可用驱动器中的矢量驱动器
  const char* pszShpDriverName = "ESRI Shapefile";
  GDALDriver* poShpDriver;
  poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
  if (poShpDriver == NULL)
  {
    std::string message = "ESRI Shapefile驱动获取失败！\n";
    file_stream << message;
    return 1;
  }

  //	利用矢量驱动器打开单个shp分幅数据目录路径下的数据,以只读数据的方式打开数据
  GDALDataset* poSingleFrameS57DataSet = (GDALDataset*)GDALOpenEx(str_input_data_path_utf8.c_str(), GDAL_OF_READONLY, NULL, NULL, NULL);
  // 文件不存在或打开失败
  if (poSingleFrameS57DataSet == NULL)
  {
    std::string message = "分幅数据：" + str_input_data_path_utf8 + "文件不存在或打开失败！\n";
    file_stream << message;
    return 2;
  }
#pragma endregion

#pragma region "3、对分幅数据的文件夹名称正确性进行检查"
/*
图幅命名正确性检查：
1、文件名为八位；
2、前两位为生产者编码，检查是否为表1-1电子海图生产者编号说明的国家代码范围；
3、第三位为航海用途，检查是否为表1-2航海用途说明的取值范围字段；
4、后五位为单一单元编码，无固定规则，不检查。（例：C13A7300、GR3CFV6S）

*/

  //  1、获取得到分幅数据的名称
  size_t frame_data_name_starting_position = str_input_data_path_utf8.find_last_of("/");
  std::string frame_data_name = str_input_data_path_utf8.substr(frame_data_name_starting_position + 1);

  {
    std::string message = "---------对" + frame_data_name + "图幅命名正确性进行检查---------\n";
    file_stream << message;
  }

  if (frame_data_name.size() != 8)
  {
    std::string message = frame_data_name + "分幅数据名称不是8位\n";
    file_stream << message;
    return 3;
  }

  bool is_find_flag = is_find(frame_data_name.substr(0, 2), S57_country_list);
  if (!is_find_flag)
  {
    std::string message = frame_data_name + "前两位（生产者编码即国家代码）不是有效的\n";
    file_stream << message;
    return 4;
  }
  //  第三位为航海用途，检查是否为表1-2航海用途说明的取值范围字段；
  bool is_valid_flag = is_valid(frame_data_name.substr(2,1), S57_Producer_number_List);
  if (!is_valid_flag)
  {
    std::string message = frame_data_name + "第三位为航海用途，不在《航海用途说明》的取值范围内，请检查配置文件\n";
    file_stream << message;
    return 5;
  }

  //  当前分幅数据文件命名正确性检查成功
  {
    std::string message = frame_data_name + "图幅命名符合规范！\n";
    file_stream << message;
  }

#pragma endregion

#pragma region "4、对分幅数据中的各个图层名称正确性进行检查"
  //  如果前面对分幅数据文件命名正确性的检查没有出现问题，那么现在需要对分幅数据中的各个图层的命名进行检查
  {
    std::string message = "\n---------对" + frame_data_name + "图幅中的各个图层命名正确性进行检查开始---------\n";
    file_stream << message;
  }

  //  获取当前数据集中的shapefile图层数量
  int size_layer = poSingleFrameS57DataSet->GetLayerCount();
  int number_of_layers_with_correct_name = 0;

  //  循环对每一个图层的命名正确性进行检查
  for (int i = 0; i < size_layer; i++)
  {
    //  获取当前图层的名称
    OGRLayer* current_layer = poSingleFrameS57DataSet->GetLayer(i);
    const char* layer_name_cstr = current_layer->GetName();
    std::string str_layer_name = std::string(layer_name_cstr);

    GIntBig size_feature = current_layer->GetFeatureCount();
    if (size_feature != 0)
    {
      //  选择第一个要素并且获取其OBJL字段的属性值
      OGRFeature* feature = current_layer->GetFeature(0);
      if (feature == nullptr)
      {
        std::string message = str_layer_name + "获取要素失败！\n";
        file_stream << message;
        continue;
      }
      //  获取字段OBJL中的属性值
      const char* attribute_value = feature->GetFieldAsString("OBJL");
      std::string str_attribute_value = std::string(attribute_value);

      //  在S57_chart_cartographic_data_elements_layer_list中查找是否存在OBJL字段属性值？有的话将对应的物标名称返回
      std::string object_label_name = get_object_label_name(S57_chart_cartographic_data_elements_layer_list, str_attribute_value);
      if (object_label_name != "")
      {
        //  如果找到之后还要判断物标名称和图层名称中的是否一致！
        if (str_layer_name.find(object_label_name.c_str()) == std::string::npos)
        {
          //  说明在字符串中没有找到子字符串
          std::string message = str_layer_name + "图层名中的物标名称在标准规范（表1-3：海图制图数据要素）中没有找到对应的物表名称，图层命名错误！\n";
          file_stream << message;
          continue;
        }
        else
        {
          //  如果当前图层的命名是符合规范的，那么可以选择将结果输出到文件中，这里只需要将出现问题的罗列出来，如果需要输出图层名称正确的信息，可以在这里完成
          number_of_layers_with_correct_name++;
        }
      }
      else
      {
        //  说明没有找到
        std::string message = str_layer_name + "图层名中的物标名称在标准规范（表1-3：海图制图数据要素）中没有找到对应的物表名称\n";
        file_stream << message;
        continue;
      }
    }
    else
    {
      std::string message = frame_data_name + "图层为空！\n";
      file_stream << message;
      continue;
    }
    

  }

#pragma endregion

#pragma region "5、向file_stream这个文件中写入能够表示当前分幅数据的结束标识内容"
  {
    std::string message = "*************************" + frame_data_name + "统计信息" + "*************************\n";
    message = "图层命名检查一共:" + std::to_string(size_layer) + "个文件。\n";
    file_stream << message;
    message = "图层命名检查完成:" + std::to_string(number_of_layers_with_correct_name) + "个文件。\n";
    file_stream << message;
    message = "图层命名检查错误:" + std::to_string(size_layer - number_of_layers_with_correct_name) + "个文件。\n";
    file_stream << message;
    message = "---------对" + frame_data_name + "图幅中的各个图层命名正确性进行检查结束---------\n";
    file_stream << message;
    message = "\n\n\n\n";
    file_stream << message;
  }
#pragma endregion


  return 0;
}

bool qgs_source_data_inspection_tool::is_find(const std::string str, const S57_country_list_t& S57_country_list)
{
  bool is_find_flag = false;
  for (size_t i = 0; i < S57_country_list.vS57_single_country.size(); i++)
  {
    std::string temp = S57_country_list.vS57_single_country[i].country_code;
    if (temp == str)
    {
      is_find_flag = true;
      break;
    }
  }
  return is_find_flag;
}

bool qgs_source_data_inspection_tool::is_valid(const std::string str, const S57_Producer_number_List_t& S57_Producer_number_List)
{
  bool is_valid_flag = false;
  for (size_t i = 0; i < S57_Producer_number_List.vS57_single_producer_number.size(); i++)
  {
    if (S57_Producer_number_List.vS57_single_producer_number[i].value == str)
    {
      is_valid_flag = true;
      break;
    }
  }
  return is_valid_flag;
}

std::string qgs_source_data_inspection_tool::get_object_label_name(
  const S57_chart_cartographic_data_elements_layer_list_t& S57_chart_cartographic_data_elements_layer_list,
  const std::string& str_attribute_value)
{
  bool is_find_flag = false;
  S57_chart_cartographic_data_elements_layer_t temp;
  for (size_t i = 0; i < S57_chart_cartographic_data_elements_layer_list.vS57_chart_cartographic_data_elements_layer.size(); i++)
  {
    temp = S57_chart_cartographic_data_elements_layer_list.vS57_chart_cartographic_data_elements_layer[i];
    if (temp.S57_OBJL_filed_attribute_value == str_attribute_value)
    {
      is_find_flag = true;
      break;
    }
  }
  return is_find_flag ? (temp.S57_Object_label_name) : "no_found";
}

#pragma endregion

#pragma region "QQCT数据源处理函数"

#pragma endregion



#pragma endregion
