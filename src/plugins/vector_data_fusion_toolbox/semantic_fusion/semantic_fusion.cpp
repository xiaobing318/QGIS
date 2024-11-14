#pragma region "头文件"
/*-------------STL-------------*/
#include <string>

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

/*-------------QGIS-------------*/
#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsgui.h"

/*-------------基础算法库-------------*/
#include "CSE_GeoExtractAndProcess.h"

/*-------------对应头文件-------------*/
#include "semantic_fusion.h"

#pragma endregion

#pragma region "构造函数、析构函数"
qgs_semantic_fusion::qgs_semantic_fusion(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);

	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.Button_input_data_view, &QPushButton::clicked, this, &qgs_semantic_fusion::Button_input_data_view);
	connect(ui.Button_output_data_view, &QPushButton::clicked, this, &qgs_semantic_fusion::Button_output_data_view);
	connect(ui.Button_OK, &QPushButton::clicked, this, &qgs_semantic_fusion::Button_OK);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &qgs_semantic_fusion::Button_Cancel);

  restore_states();
}

qgs_semantic_fusion::~qgs_semantic_fusion()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("qgs_semantic_fusion/m_input_data_path"), m_input_data_path, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("qgs_semantic_fusion/m_output_data_path"), m_output_data_path, QgsSettings::Section::Plugins);

}
#pragma endregion

#pragma region "类成员对象set、get函数"
//  获取输入数据目录成员函数
QString qgs_semantic_fusion::get_m_input_data_path()
{
  return m_input_data_path;
}

//  获取输出数据目录成员函数
QString qgs_semantic_fusion::get_m_output_data_path()
{
  return m_output_data_path;
}

//  设置输入数据目录成员函数
void qgs_semantic_fusion::set_m_input_data_path(const std::string& input_data_path)
{
  m_input_data_path = QString::fromStdString(input_data_path);
}

//  设置输出数据目录成员函数
void qgs_semantic_fusion::set_m_output_data_path(const std::string& output_data_path)
{
  m_output_data_path = QString::fromStdString(output_data_path);
}

//  获取数据源类型
int qgs_semantic_fusion::get_input_data_src_type()
{
  if (ui.radioButton_JBDX->isChecked())
  {
    return 0;
  }
  else if (ui.radioButton_OSM->isChecked())
  {
    return 1;
  }
  else if (ui.radioButton_S57->isChecked())
  {
    return 2;
  }
  else if (ui.radioButton_SYDH->isChecked())
  {
    return 3;
  }
  else if (ui.radioButton_QQCT->isChecked())
  {
    return 4;
  }
  //	如果全都没有被选择的话将会返回5
  return 5;
}

// 恢复保存参数
void qgs_semantic_fusion::restore_states()
{
  // 从上一次保存的路径数据中得到输入输出数据
  QgsSettings settings;
  m_input_data_path = settings.value(QStringLiteral("qgs_semantic_fusion/m_input_data_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
  m_output_data_path = settings.value(QStringLiteral("qgs_semantic_fusion/m_output_data_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

  //  填充输入输出数据路径，显示在交互界面上
  ui.lineEdit_input_data_path->setText(m_input_data_path);
  ui.lineEdit_output_data_path->setText(m_output_data_path);

  // 默认选择的是GB数据源
  ui.radioButton_JBDX->setChecked(true);
}
#pragma endregion

#pragma region "私有函数：读取配置文件相关的函数"

/*读取一体化属性表结构*/
void qgs_semantic_fusion::Load_YTH_fields4all_layers_config_file(YTH_fields4all_layers_t& YTH_fields4all_layers)
{
  /*
  函数功能：
    函数从 `YTH_fields_info4each_layer.xml` 配置文件中加载YTH图层字段信息（一体化属性表结构）。
  这些信息被解析并存储在一个自定义数据结构 `YTH_fields4all_layers_t` 中，以便后续操作。

  算法流程：
    1. 构建 XML 配置文件的完整路径。
    2. 打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中。
    3. 遍历 XML 文档的根节点下的每一个子节点（标记为 "YTH_layer"）。
    4. 对于每个 "YTH_layer" 节点，提取其中的 "YTH_first_class_code"、"YTH_First_class_name" 、"YTH_layer_name"和 "Field_List" 等信息，并存储在一个临时的 `YTH_single_layer_t` 结构体中。
    5. 如果存在 "YTH_fields"，则进一步提取其下的 "field" 节点信息，包括字段名称、别名、类型、长度等，并存储在 `field_t` 类型的结构体中。
    6.
    7. 将所有提取的信息汇总到 `YTH_layer` 结构体，并添加到 `YTH_fields4all_layers_t` 列表中。
    8. 循环步骤 3-7，直到处理完所有 "YTH_layer" 节点。
  */
#pragma region "1、构建XML配置文件的完整路径"
  //	构建到YTH的XML配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_fields_info4each_layer.xml";
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
    YTH_single_layer_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "YTH_layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_nodes = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_nodes.toElement();
        if (Layer_child_element.tagName() == "YTH_first_class_code")
        {
          temp_layer.YTH_first_class_code = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_First_class_name")
        {
          temp_layer.YTH_First_class_name = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_layer_name")
        {
          temp_layer.YTH_layer_name = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_layer_name_alias")
        {
          temp_layer.YTH_layer_name_alias = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_fields")
        {
          // 获取Field_List的所有子节点
          QDomNodeList Field_List_all_child_nodes = Layer_child_nodes.childNodes();
          for (size_t j = 0; j < Field_List_all_child_nodes.size(); j++)
          {
            //	创建一个临时的field_t数据结构，用来存放读取到的数据
            field_t single_field;

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
                  single_field.field_name = Field_child_element.text().toStdString();
                }
                else if (Field_child_element.tagName() == "field_name_alias")
                {
                  single_field.field_name_alias = Field_child_element.text().toStdString();
                }
                else if (Field_child_element.tagName() == "is_key_field")
                {
                  single_field.is_key_field = Field_child_element.text().toStdString();
                }
                else if (Field_child_element.tagName() == "default_value")
                {
                  single_field.default_value = Field_child_element.text().toStdString();
                }
                else if (Field_child_element.tagName() == "field_type")
                {
                  single_field.field_type = Field_child_element.text().toStdString();
                }
                else if (Field_child_element.tagName() == "field_length")
                {
                  single_field.field_length = Field_child_element.text().toStdString();
                }
                else if (Field_child_element.tagName() == "field_precision")
                {
                  single_field.field_precision = Field_child_element.text().toStdString();
                }
                else if (Field_child_element.tagName() == "field_range")
                {
                  single_field.field_range = Field_child_element.text().toStdString();
                }
                else if (Field_child_element.tagName() == "field_data_source")
                {
                  single_field.field_data_source = Field_child_element.text().toStdString();
                }
              }
            }

            //	将读取到的Layer数据存放到上一层数据结构中
            temp_layer.vfield.push_back(single_field);
          }
        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_fields4all_layers.vYTH_single_layer.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

/*读取YTH同JBDX图层映射表*/
void qgs_semantic_fusion::Load_YTH_JBDX_layer_mapping_table(YTH_JBDX_layer_mapping_table_t& YTH_JBDX_layer_mapping_table)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建YTH同JBDX图层映射表配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_JBDX_layer_mapping_table.xml";
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
    YTH_JBDX_single_layer_mapping_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_nodes = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_nodes.toElement();
        if (Layer_child_element.tagName() == "JBDX_layer_name")
        {
          temp_layer.JBDX_layer_name = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "JBDX_layer_name_alias")
        {
          temp_layer.JBDX_layer_name_alias = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "JBDX_layer_geo_type")
        {
          temp_layer.JBDX_layer_geo_type = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_layer_list")
        {
          // 获取YTH_layer_list的所有子节点
          QDomNodeList YTH_layer_list_all_child_nodes = Layer_child_nodes.childNodes();
          for (size_t j = 0; j < YTH_layer_list_all_child_nodes.size(); j++)
          {
            //	获取YTH_layer_list的所有子节点的第j个子节点
            QDomNode current_YTH_layer = YTH_layer_list_all_child_nodes.at(j);

            //	判断当前节点的标签是否为Field
            if (current_YTH_layer.toElement().tagName() == "YTH_layer")
            {
              temp_layer.vYTH_layer_list.push_back(current_YTH_layer.toElement().text().toStdString());
            }

          }
        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_JBDX_layer_mapping_table.vYTH_JBDX_layer_mapping.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

/*读取YTH同JBDX编码映射表*/
void qgs_semantic_fusion::Load_YTH_JBDX_code_mapping_table(YTH_JBDX_code_mapping_table_t& YTH_JBDX_code_mapping_table)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建YTH同JBDX图层映射表配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_JBDX_code_mapping_table.xml";
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
    YTH_JBDX_code_mapping_single_layer_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_node = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_node.toElement();
        if (Layer_child_element.tagName() == "YTHLayerID")
        {
          temp_layer.YTHLayerID = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_features")
        {

          // 获取YTH_features的所有子节点
          QDomNodeList YTH_features_all_child_nodes = Layer_child_node.childNodes();
          for (size_t j = 0; j < YTH_features_all_child_nodes.size(); j++)
          {
            //	创建一个临时的自定义结构用来存储YTH_features
            YTH_JBDX_single_feature_t temp_YTH_single_feature;

            //	获取YTH_features的所有子节点的第j个子节点
            QDomNode current_YTH_single_feature = YTH_features_all_child_nodes.at(j);

            //	判断当前节点的标签是否为YTH_JBDX_single_feature
            if (current_YTH_single_feature.toElement().tagName() == "YTH_single_feature")
            {
              // 获取YTH_JBDX_single_feature的所有子节点
              QDomNodeList YTH_single_feature_all_child_nodes = current_YTH_single_feature.childNodes();
              for (size_t k = 0; k < YTH_single_feature_all_child_nodes.size(); k++)
              {
                //	获取YTH_features的所有子节点的第k个子节点
                QDomNode current_YTH_single_feature_child_node = YTH_single_feature_all_child_nodes.at(k);
                QDomElement current_YTH_single_feature_child_element = current_YTH_single_feature_child_node.toElement();
                if (current_YTH_single_feature_child_element.tagName() == "YTH_feature_classification_name")
                {
                  temp_YTH_single_feature.YTH_feature_classification_name = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "YTH_feature_code_field")
                {
                  temp_YTH_single_feature.YTH_feature_code_field = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "JBDX_origin_layer_name")
                {
                  temp_YTH_single_feature.JBDX_origin_layer_name = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "SQL")
                {
                  temp_YTH_single_feature.SQL = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "JBDX_feaure_name")
                {
                  temp_YTH_single_feature.JBDX_feaure_name = current_YTH_single_feature_child_element.text().toStdString();
                }
              }
            }

            temp_layer.vYTH_single_feature.push_back(temp_YTH_single_feature);
          }


        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_JBDX_code_mapping_table.vYTH_JBDX_code_mapping_single_layer.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

/*读取YTH同OSM图层映射表*/
void qgs_semantic_fusion::Load_YTH_OSM_layer_mapping_table(YTH_OSM_layer_mapping_table_t& YTH_OSM_layer_mapping_table)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建YTH同OSM图层映射表配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_OSM_layer_mapping_table.xml";
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
    YTH_OSM_single_layer_mapping_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_nodes = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_nodes.toElement();
        if (Layer_child_element.tagName() == "OSM_layer_name")
        {
          temp_layer.OSM_layer_name = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "OSM_layer_name_alias")
        {
          temp_layer.OSM_layer_name_alias = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "OSM_layer_geo_type")
        {
          temp_layer.OSM_layer_geo_type = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_layer_list")
        {
          // 获取YTH_layer_list的所有子节点
          QDomNodeList YTH_layer_list_all_child_nodes = Layer_child_nodes.childNodes();
          for (size_t j = 0; j < YTH_layer_list_all_child_nodes.size(); j++)
          {
            //	获取YTH_layer_list的所有子节点的第j个子节点
            QDomNode current_YTH_layer = YTH_layer_list_all_child_nodes.at(j);

            //	判断当前节点的标签是否为Field
            if (current_YTH_layer.toElement().tagName() == "YTH_layer")
            {
              temp_layer.vYTH_layer_list.push_back(current_YTH_layer.toElement().text().toStdString());
            }

          }
        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_OSM_layer_mapping_table.vYTH_OSM_layer_mapping.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

/*读取YTH同OSM编码映射表*/
void qgs_semantic_fusion::Load_YTH_OSM_code_mapping_table(YTH_OSM_code_mapping_table_t& YTH_OSM_code_mapping_table)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建YTH同OSM图层映射表配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_OSM_code_mapping_table.xml";
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
    YTH_OSM_code_mapping_single_layer_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_node = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_node.toElement();
        if (Layer_child_element.tagName() == "YTHLayerID")
        {
          temp_layer.YTHLayerID = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_features")
        {

          // 获取YTH_features的所有子节点
          QDomNodeList YTH_features_all_child_nodes = Layer_child_node.childNodes();
          for (size_t j = 0; j < YTH_features_all_child_nodes.size(); j++)
          {
            //	创建一个临时的自定义结构用来存储YTH_features
            YTH_OSM_single_feature_t temp_YTH_single_feature;

            //	获取YTH_features的所有子节点的第j个子节点
            QDomNode current_YTH_single_feature = YTH_features_all_child_nodes.at(j);

            //	判断当前节点的标签是否为YTH_OSM_single_feature
            if (current_YTH_single_feature.toElement().tagName() == "YTH_single_feature")
            {
              // 获取YTH_OSM_single_feature的所有子节点
              QDomNodeList YTH_single_feature_all_child_nodes = current_YTH_single_feature.childNodes();
              for (size_t k = 0; k < YTH_single_feature_all_child_nodes.size(); k++)
              {
                //	获取YTH_features的所有子节点的第k个子节点
                QDomNode current_YTH_single_feature_child_node = YTH_single_feature_all_child_nodes.at(k);
                QDomElement current_YTH_single_feature_child_element = current_YTH_single_feature_child_node.toElement();
                if (current_YTH_single_feature_child_element.tagName() == "YTH_feature_classification_name")
                {
                  temp_YTH_single_feature.YTH_feature_classification_name = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "YTH_feature_code_field")
                {
                  temp_YTH_single_feature.YTH_feature_code_field = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "OSM_origin_layer_name")
                {
                  temp_YTH_single_feature.OSM_origin_layer_name = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "SQL")
                {
                  temp_YTH_single_feature.SQL = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "OSM_feaure_name")
                {
                  temp_YTH_single_feature.OSM_feaure_name = current_YTH_single_feature_child_element.text().toStdString();
                }
              }
            }

            temp_layer.vYTH_single_feature.push_back(temp_YTH_single_feature);
          }


        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_OSM_code_mapping_table.vYTH_OSM_code_mapping_single_layer.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

/*读取YTH同S57图层映射表*/
void qgs_semantic_fusion::Load_YTH_S57_layer_mapping_table(YTH_S57_layer_mapping_table_t& YTH_S57_layer_mapping_table)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建YTH同S57图层映射表配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_S57_layer_mapping_table.xml";
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
    YTH_S57_single_layer_mapping_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "Layer")
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
        else if (Layer_child_element.tagName() == "S57_layer_name_alias")
        {
          temp_layer.S57_layer_name_alias = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "S57_layer_geo_type")
        {
          temp_layer.S57_layer_geo_type = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_layer_list")
        {
          // 获取YTH_layer_list的所有子节点
          QDomNodeList YTH_layer_list_all_child_nodes = Layer_child_nodes.childNodes();
          for (size_t j = 0; j < YTH_layer_list_all_child_nodes.size(); j++)
          {
            //	获取YTH_layer_list的所有子节点的第j个子节点
            QDomNode current_YTH_layer = YTH_layer_list_all_child_nodes.at(j);

            //	判断当前节点的标签是否为Field
            if (current_YTH_layer.toElement().tagName() == "YTH_layer")
            {
              temp_layer.vYTH_layer_list.push_back(current_YTH_layer.toElement().text().toStdString());
            }

          }
        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_S57_layer_mapping_table.vYTH_S57_layer_mapping.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

/*读取YTH同S57编码映射表*/
void qgs_semantic_fusion::Load_YTH_S57_code_mapping_table(YTH_S57_code_mapping_table_t& YTH_S57_code_mapping_table)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建YTH同S57图层映射表配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_S57_code_mapping_table.xml";
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
    YTH_S57_code_mapping_single_layer_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_node = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_node.toElement();
        if (Layer_child_element.tagName() == "YTHLayerID")
        {
          temp_layer.YTHLayerID = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_features")
        {

          // 获取YTH_features的所有子节点
          QDomNodeList YTH_features_all_child_nodes = Layer_child_node.childNodes();
          for (size_t j = 0; j < YTH_features_all_child_nodes.size(); j++)
          {
            //	创建一个临时的自定义结构用来存储YTH_features
            YTH_S57_single_feature_t temp_YTH_single_feature;

            //	获取YTH_features的所有子节点的第j个子节点
            QDomNode current_YTH_single_feature = YTH_features_all_child_nodes.at(j);

            //	判断当前节点的标签是否为YTH_S57_single_feature
            if (current_YTH_single_feature.toElement().tagName() == "YTH_single_feature")
            {
              // 获取YTH_S57_single_feature的所有子节点
              QDomNodeList YTH_single_feature_all_child_nodes = current_YTH_single_feature.childNodes();
              for (size_t k = 0; k < YTH_single_feature_all_child_nodes.size(); k++)
              {
                //	获取YTH_features的所有子节点的第k个子节点
                QDomNode current_YTH_single_feature_child_node = YTH_single_feature_all_child_nodes.at(k);
                QDomElement current_YTH_single_feature_child_element = current_YTH_single_feature_child_node.toElement();
                if (current_YTH_single_feature_child_element.tagName() == "YTH_feature_classification_name")
                {
                  temp_YTH_single_feature.YTH_feature_classification_name = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "YTH_feature_code_field")
                {
                  temp_YTH_single_feature.YTH_feature_code_field = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "S57_origin_layer_name")
                {
                  temp_YTH_single_feature.S57_origin_layer_name = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "SQL")
                {
                  temp_YTH_single_feature.SQL = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "S57_feaure_name")
                {
                  temp_YTH_single_feature.S57_feaure_name = current_YTH_single_feature_child_element.text().toStdString();
                }
              }
            }

            temp_layer.vYTH_single_feature.push_back(temp_YTH_single_feature);
          }


        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_S57_code_mapping_table.vYTH_S57_code_mapping_single_layer.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

/*读取YTH同SYDH图层映射表*/
void qgs_semantic_fusion::Load_YTH_SYDH_layer_mapping_table(YTH_SYDH_layer_mapping_table_t& YTH_SYDH_layer_mapping_table)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建YTH同SYDH图层映射表配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_SYDH_layer_mapping_table.xml";
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
    YTH_SYDH_single_layer_mapping_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_nodes = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_nodes.toElement();
        if (Layer_child_element.tagName() == "SYDH_layer_name")
        {
          temp_layer.SYDH_layer_name = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "SYDH_layer_name_alias")
        {
          temp_layer.SYDH_layer_name_alias = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "SYDH_layer_geo_type")
        {
          temp_layer.SYDH_layer_geo_type = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_layer_list")
        {
          // 获取YTH_layer_list的所有子节点
          QDomNodeList YTH_layer_list_all_child_nodes = Layer_child_nodes.childNodes();
          for (size_t j = 0; j < YTH_layer_list_all_child_nodes.size(); j++)
          {
            //	获取YTH_layer_list的所有子节点的第j个子节点
            QDomNode current_YTH_layer = YTH_layer_list_all_child_nodes.at(j);

            //	判断当前节点的标签是否为Field
            if (current_YTH_layer.toElement().tagName() == "YTH_layer")
            {
              temp_layer.vYTH_layer_list.push_back(current_YTH_layer.toElement().text().toStdString());
            }

          }
        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_SYDH_layer_mapping_table.vYTH_SYDH_layer_mapping.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

/*读取YTH同SYDH编码映射表*/
void qgs_semantic_fusion::Load_YTH_SYDH_code_mapping_table(YTH_SYDH_code_mapping_table_t& YTH_SYDH_code_mapping_table)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建YTH同SYDH图层映射表配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_SYDH_code_mapping_table.xml";
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
    YTH_SYDH_code_mapping_single_layer_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_node = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_node.toElement();
        if (Layer_child_element.tagName() == "YTHLayerID")
        {
          temp_layer.YTHLayerID = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_features")
        {

          // 获取YTH_features的所有子节点
          QDomNodeList YTH_features_all_child_nodes = Layer_child_node.childNodes();
          for (size_t j = 0; j < YTH_features_all_child_nodes.size(); j++)
          {
            //	创建一个临时的自定义结构用来存储YTH_features
            YTH_SYDH_single_feature_t temp_YTH_single_feature;

            //	获取YTH_features的所有子节点的第j个子节点
            QDomNode current_YTH_single_feature = YTH_features_all_child_nodes.at(j);

            //	判断当前节点的标签是否为YTH_SYDH_single_feature
            if (current_YTH_single_feature.toElement().tagName() == "YTH_single_feature")
            {
              // 获取YTH_SYDH_single_feature的所有子节点
              QDomNodeList YTH_single_feature_all_child_nodes = current_YTH_single_feature.childNodes();
              for (size_t k = 0; k < YTH_single_feature_all_child_nodes.size(); k++)
              {
                //	获取YTH_features的所有子节点的第k个子节点
                QDomNode current_YTH_single_feature_child_node = YTH_single_feature_all_child_nodes.at(k);
                QDomElement current_YTH_single_feature_child_element = current_YTH_single_feature_child_node.toElement();
                if (current_YTH_single_feature_child_element.tagName() == "YTH_feature_classification_name")
                {
                  temp_YTH_single_feature.YTH_feature_classification_name = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "YTH_feature_code_field")
                {
                  temp_YTH_single_feature.YTH_feature_code_field = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "SYDH_origin_layer_name")
                {
                  temp_YTH_single_feature.SYDH_origin_layer_name = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "SQL")
                {
                  temp_YTH_single_feature.SQL = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "SYDH_feaure_name")
                {
                  temp_YTH_single_feature.SYDH_feaure_name = current_YTH_single_feature_child_element.text().toStdString();
                }
              }
            }

            temp_layer.vYTH_single_feature.push_back(temp_YTH_single_feature);
          }


        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_SYDH_code_mapping_table.vYTH_SYDH_code_mapping_single_layer.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

/*读取YTH同QQCT图层映射表*/
void qgs_semantic_fusion::Load_YTH_QQCT_layer_mapping_table(YTH_QQCT_layer_mapping_table_t& YTH_QQCT_layer_mapping_table)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建YTH同QQCT图层映射表配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_QQCT_layer_mapping_table.xml";
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
    YTH_QQCT_single_layer_mapping_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_nodes = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_nodes.toElement();
        if (Layer_child_element.tagName() == "QQCT_layer_name")
        {
          temp_layer.QQCT_layer_name = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "QQCT_layer_name_alias")
        {
          temp_layer.QQCT_layer_name_alias = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "QQCT_layer_geo_type")
        {
          temp_layer.QQCT_layer_geo_type = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_layer_list")
        {
          // 获取YTH_layer_list的所有子节点
          QDomNodeList YTH_layer_list_all_child_nodes = Layer_child_nodes.childNodes();
          for (size_t j = 0; j < YTH_layer_list_all_child_nodes.size(); j++)
          {
            //	获取YTH_layer_list的所有子节点的第j个子节点
            QDomNode current_YTH_layer = YTH_layer_list_all_child_nodes.at(j);

            //	判断当前节点的标签是否为Field
            if (current_YTH_layer.toElement().tagName() == "YTH_layer")
            {
              temp_layer.vYTH_layer_list.push_back(current_YTH_layer.toElement().text().toStdString());
            }

          }
        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_QQCT_layer_mapping_table.vYTH_QQCT_layer_mapping.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

/*读取YTH同QQCT编码映射表*/
void qgs_semantic_fusion::Load_YTH_QQCT_code_mapping_table(YTH_QQCT_code_mapping_table_t& YTH_QQCT_code_mapping_table)
{
#pragma region "1、构建XML配置文件的完整路径"
  //	构建YTH同QQCT图层映射表配置文件路径
  QString curPath = QCoreApplication::applicationDirPath();
  QString strFileName = curPath + "/semantic_fusion_tool_xmls/YTH_QQCT_code_mapping_table.xml";
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
    YTH_QQCT_code_mapping_single_layer_t temp_layer;

    // 当前节点的标签如果是Layer的话（假设XML文件就是预想的结构，已经经过了前面的检查）
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取当前节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理所有的子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取当前节点的所有子节点的第i个子节点
        QDomNode Layer_child_node = Layer_all_child_nodes.at(i);
        QDomElement Layer_child_element = Layer_child_node.toElement();
        if (Layer_child_element.tagName() == "YTHLayerID")
        {
          temp_layer.YTHLayerID = Layer_child_element.text().toStdString();
        }
        else if (Layer_child_element.tagName() == "YTH_features")
        {

          // 获取YTH_features的所有子节点
          QDomNodeList YTH_features_all_child_nodes = Layer_child_node.childNodes();
          for (size_t j = 0; j < YTH_features_all_child_nodes.size(); j++)
          {
            //	创建一个临时的自定义结构用来存储YTH_features
            YTH_QQCT_single_feature_t temp_YTH_single_feature;

            //	获取YTH_features的所有子节点的第j个子节点
            QDomNode current_YTH_single_feature = YTH_features_all_child_nodes.at(j);

            //	判断当前节点的标签是否为YTH_QQCT_single_feature
            if (current_YTH_single_feature.toElement().tagName() == "YTH_single_feature")
            {
              // 获取YTH_QQCT_single_feature的所有子节点
              QDomNodeList YTH_single_feature_all_child_nodes = current_YTH_single_feature.childNodes();
              for (size_t k = 0; k < YTH_single_feature_all_child_nodes.size(); k++)
              {
                //	获取YTH_features的所有子节点的第k个子节点
                QDomNode current_YTH_single_feature_child_node = YTH_single_feature_all_child_nodes.at(k);
                QDomElement current_YTH_single_feature_child_element = current_YTH_single_feature_child_node.toElement();
                if (current_YTH_single_feature_child_element.tagName() == "YTH_feature_classification_name")
                {
                  temp_YTH_single_feature.YTH_feature_classification_name = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "YTH_feature_code_field")
                {
                  temp_YTH_single_feature.YTH_feature_code_field = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "QQCT_origin_layer_name")
                {
                  temp_YTH_single_feature.QQCT_origin_layer_name = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "SQL")
                {
                  temp_YTH_single_feature.SQL = current_YTH_single_feature_child_element.text().toStdString();
                }
                else if (current_YTH_single_feature_child_element.tagName() == "QQCT_feaure_name")
                {
                  temp_YTH_single_feature.QQCT_feaure_name = current_YTH_single_feature_child_element.text().toStdString();
                }
              }
            }

            temp_layer.vYTH_single_feature.push_back(temp_YTH_single_feature);
          }


        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    YTH_QQCT_code_mapping_table.vYTH_QQCT_code_mapping_single_layer.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }
#pragma endregion
}

#pragma endregion

#pragma region "私有槽函数"

void qgs_semantic_fusion::Button_input_data_view()
{
  // 选择文件夹
  QString curPath = m_input_data_path;
  QString dlgTile = QObject::tr("请选择语义融合输入数据源所在的目录位置");
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

void qgs_semantic_fusion::Button_output_data_view()
{
  // 选择文件夹
  QString curPath = m_output_data_path;
  QString dlgTile = QObject::tr("请选择语义融合处理后数据存放的目录位置");
  QString selectedDir = QFileDialog::getExistingDirectory(
    this,
    dlgTile,
    curPath,
    QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    ui.lineEdit_output_data_path->setText(selectedDir);
    m_output_data_path = selectedDir;
  }
}

void qgs_semantic_fusion::Button_OK()
{
  /*
  功能说明：
    1、当UI界面中的“确定”按钮被点击的时候，对不同来源的数据进行一体化转化便正式开始
    2、UI界面中的一些其他内容是作为这部分的处理的参数
    3、这个函数就是“一体化”算法实际执行的函数

  算法流程：
    1. 验证用户输入的数据源路径和保存转换后数据的路径。
    2. 加载包含不同标准之间映射规则的各种配置文件。
    3. 使用提取和验证的数据执行最终的转换过程。

  */
#pragma region "（第一步）DLHJ：输入数据路径、输出数据路径检查"
  if (!is_exist_file_directory(ui.lineEdit_input_data_path->text()))
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据语义融合"));
    msgBox.setText(tr("输入数据目录不存在，请重新输入！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }

  if (!is_exist_file_directory(ui.lineEdit_output_data_path->text()))
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("矢量数据语义融合"));
    msgBox.setText(tr("输出数据目录不存在，请重新输入！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    return;
  }
#pragma endregion 

#pragma region "（第二步）获取数据源类型、获取输出数据路径、获取保存处理后数据的路径"
  // 获取数据来源
  int input_data_src_type = get_input_data_src_type();

  // 获取输入数据路径
  QString qstr_input_data_path = get_m_input_data_path();
  string str_input_data_path = qstr_input_data_path.toLocal8Bit();

  // 获取数据保存目录
  QString qstr_output_data_path = get_m_output_data_path();
  string str_output_data_path = qstr_output_data_path.toLocal8Bit();

  // 获取图幅列表
  QStringList qstr_input_data_path_list = get_sub_directories(qstr_input_data_path);

#pragma endregion

#pragma region "（第三步）DLHJ：将配置文件读取到自定义结构体中（从用户的角度来说，并不会关心配置文件，因此关于配置文件的检查将会在另外的部分进行检查）"

  /*读取一体化属性表结构*/
  YTH_fields4all_layers_t YTH_fields4all_layers;
  Load_YTH_fields4all_layers_config_file(YTH_fields4all_layers);


  /*读取YTH同JBDX图层映射表*/
  YTH_JBDX_layer_mapping_table_t YTH_JBDX_layer_mapping_table;
  /*读取YTH同JBDX图层映射表*/
  YTH_JBDX_code_mapping_table_t YTH_JBDX_code_mapping_table;


  /*读取YTH同OSM图层映射表*/
  YTH_OSM_layer_mapping_table_t YTH_OSM_layer_mapping_table;
  /*读取YTH同OSM图层映射表*/
  YTH_OSM_code_mapping_table_t YTH_OSM_code_mapping_table;


  /*读取YTH同S57图层映射表*/
  YTH_S57_layer_mapping_table_t YTH_S57_layer_mapping_table;
  /*读取YTH同S57图层映射表*/
  YTH_S57_code_mapping_table_t YTH_S57_code_mapping_table;


  /*读取YTH同SYDH图层映射表*/
  YTH_SYDH_layer_mapping_table_t YTH_SYDH_layer_mapping_table;
  /*读取YTH同SYDH图层映射表*/
  YTH_SYDH_code_mapping_table_t YTH_SYDH_code_mapping_table;

  /*读取YTH同QQCT图层映射表*/
  YTH_QQCT_layer_mapping_table_t YTH_QQCT_layer_mapping_table;
  /*读取YTH同QQCT图层映射表*/
  YTH_QQCT_code_mapping_table_t YTH_QQCT_code_mapping_table;



  //	根据不同的数据来源加载图层配置文件、数据源编码到一体化编码配置文件
  if (input_data_src_type == 0)
  {
    /*读取YTH同JBDX图层映射表*/
    Load_YTH_JBDX_layer_mapping_table(YTH_JBDX_layer_mapping_table);
    /*读取YTH同JBDX编码映射表*/
    Load_YTH_JBDX_code_mapping_table(YTH_JBDX_code_mapping_table);
  }
  else if (input_data_src_type == 1)
  {
    /*读取YTH同OSM图层映射表*/
    Load_YTH_OSM_layer_mapping_table(YTH_OSM_layer_mapping_table);
    /*读取YTH同OSM编码映射表*/
    Load_YTH_OSM_code_mapping_table(YTH_OSM_code_mapping_table);
  }
  else if (input_data_src_type == 2)
  {
    /*读取YTH同S57图层映射表*/
    Load_YTH_S57_layer_mapping_table(YTH_S57_layer_mapping_table);
    /*读取YTH同S57编码映射表*/
    Load_YTH_S57_code_mapping_table(YTH_S57_code_mapping_table);

  }
  else if (input_data_src_type == 3)
  {
    /*读取YTH同SYDH图层映射表*/
    Load_YTH_SYDH_layer_mapping_table(YTH_SYDH_layer_mapping_table);
    /*读取YTH同SYDH编码映射表*/
    Load_YTH_SYDH_code_mapping_table(YTH_SYDH_code_mapping_table);
  }
  else if (input_data_src_type == 4)
  {
    /*读取YTH同QQCT图层映射表*/
    Load_YTH_QQCT_layer_mapping_table(YTH_QQCT_layer_mapping_table);
    /*读取YTH同QQCT编码映射表*/
    Load_YTH_QQCT_code_mapping_table(YTH_QQCT_code_mapping_table);
  }
  else if (input_data_src_type = 5)
  {
    //  其他类型的数据源
  }

#pragma endregion

#pragma region "（第四步）DLHJ：从集合中选择出部分正确的子集（有效的shp文件），然后利用YTH生成接口进行处理"

#pragma region "1、将多个有效的分幅数据存放在向量中，用作一体化生成接口的参数"
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

#pragma endregion

#pragma region "2、利用YTH一体化生成接口进行处理"

  int iResult = 0;
  if (input_data_src_type == 0)
  {
    //  JBDX
    iResult = CSE_GeoExtractAndProcess::DLHJ_convert_JBDXDATA2YTHData(
      v_input_frame_data_path,
      str_output_data_path,
      input_data_src_type,
      YTH_fields4all_layers,
      YTH_JBDX_layer_mapping_table,
      YTH_JBDX_code_mapping_table,
      NULL);
  }
  else if (input_data_src_type == 1)
  {
    //  OSM
    iResult = CSE_GeoExtractAndProcess::DLHJ_convert_OSMDATA2YTHData(
      v_input_frame_data_path,
      str_output_data_path,
      input_data_src_type,
      YTH_fields4all_layers,
      YTH_OSM_layer_mapping_table,
      YTH_OSM_code_mapping_table,
      NULL);
  }
  else if (input_data_src_type == 2)
  {
    //  S57
    iResult = CSE_GeoExtractAndProcess::DLHJ_convert_S57DATA2YTHData(
      v_input_frame_data_path,
      str_output_data_path,
      input_data_src_type,
      YTH_fields4all_layers,
      YTH_S57_layer_mapping_table,
      YTH_S57_code_mapping_table,
      NULL);
  }
  else if (input_data_src_type == 3)
  {
    //  SYDH
    iResult = CSE_GeoExtractAndProcess::DLHJ_convert_SYDHDATA2YTHData(
      v_input_frame_data_path,
      str_output_data_path,
      input_data_src_type,
      YTH_fields4all_layers,
      YTH_SYDH_layer_mapping_table,
      YTH_SYDH_code_mapping_table,
      NULL);
  }
  else if (input_data_src_type == 4)
  {
    //  QQCT
    iResult = CSE_GeoExtractAndProcess::DLHJ_convert_QQCTDATA2YTHData(
      v_input_frame_data_path,
      str_output_data_path,
      input_data_src_type,
      YTH_fields4all_layers,
      YTH_QQCT_layer_mapping_table,
      YTH_QQCT_code_mapping_table,
      NULL);
  }
  else if (input_data_src_type = 5)
  {
    //  其他类型的数据源
  }

  //	语义融合处理失败
  if (iResult != 0)
  {
    // 记录日志
    QMessageBox msgBox1;
    msgBox1.setWindowTitle(tr("（DLHJ）语义融合处理!"));
    msgBox1.setText(tr("语义融合处理失败！"));
    msgBox1.setStandardButtons(QMessageBox::Yes);
    msgBox1.setDefaultButton(QMessageBox::Yes);
    msgBox1.exec();
    return;
  }
  //	GJB一体化数据生成成功！
  else
  {
    // 记录日志
    QMessageBox msgBox1;
    msgBox1.setWindowTitle(tr("（DLHJ）语义融合处理!"));
    msgBox1.setText(tr("语义融合处理完成！"));
    msgBox1.setStandardButtons(QMessageBox::Yes);
    msgBox1.setDefaultButton(QMessageBox::Yes);
    msgBox1.exec();
    return;
  }
#pragma endregion

#pragma endregion


}

void qgs_semantic_fusion::Button_Cancel()
{
  reject();
}

#pragma endregion

#pragma region "类辅助成员函数"
//  判断目录是否存在
bool qgs_semantic_fusion::is_exist_file_directory(const QString& qstr_directory_path)
{
  QDir dir(qstr_directory_path);
  return dir.exists();
}

//  判断文件是否存在
bool qgs_semantic_fusion::is_exist_file(const QString& qstr_file_path)
{
  return QFile::exists(qstr_file_path);
}

//	检查XML文件
void qgs_semantic_fusion::checkAndDebugXML(const QString& strFileName) {
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
QStringList qgs_semantic_fusion::get_sub_directories(const QString& file_directory_path)
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
