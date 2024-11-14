#include "get_shp_data_from_gpkg.h"

get_shp_data_from_gpkg::get_shp_data_from_gpkg(
  const int& InputDataType,
  const int& iScaleType,
  const bool& bOutputSheet,
  const std::string& xml_input_data_path,
  const std::string& input_data_path,
  const int& iOutputDataType,
  const std::string& output_data_path)
{
  this->m_InputDataType = InputDataType;
  this->m_iScaleType = iScaleType;
  this->m_bOutputSheet = bOutputSheet;
  this->m_xml_input_data_path = xml_input_data_path;
  this->m_input_data_path = input_data_path;
  this->m_iOutputDataType = iOutputDataType;
  this->m_output_data_path = output_data_path;
  this->m_vExtractDataParam.clear();
}

get_shp_data_from_gpkg::~get_shp_data_from_gpkg()
{

}

void get_shp_data_from_gpkg::perform_get_shp_data_from_gpkg()
{
  this->load_xml_configure_file();
  this->perform_process_on_a_single_work_area();
}

void get_shp_data_from_gpkg::load_xml_configure_file()
{

  QString strFileName = QString::fromStdString(this->m_xml_input_data_path);

  QDomDocument doc;
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    //  需要进行错误处理
    log_message::show_log_message(4);
  }

  if (!doc.setContent(&file))
  {
    file.close();
    return;
  }
  file.close();

  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点   QDomNode 节点
  QDomNode node = root.firstChild();

  while (!node.isNull())
  {
    // 循环遍历每个子节点
    ExtractDataParam param;

    // 节点元素名称
    QString tagName = node.toElement().tagName();
    // 节点标记查找
    if (tagName == "Layer")
    {
      QDomNodeList nodeList = node.childNodes();
      for (int i = 0; i < nodeList.size(); i++)
      {
        QDomNode nodeTemp = nodeList.at(i);
        QDomElement elementTemp = nodeTemp.toElement();
        if (elementTemp.tagName() == "LayerName")
        {
          param.strLayerName = elementTemp.text().toStdString();
        }

        else if (elementTemp.tagName() == "Fields")
        {
          // 获取字段列表
          QDomNodeList fieldList = nodeTemp.childNodes();
          for (int j = 0; j < fieldList.size(); j++)
          {
            QDomNode fieldNode = fieldList.at(j);
            QDomElement fieldElement = fieldNode.toElement();
            QString qstrField = fieldElement.text();
            string strField = qstrField.toLocal8Bit();
            param.vFields.push_back(strField);
          }
        }

        else if (elementTemp.tagName() == "FeatureClassCodes")
        {
          // 获取字段列表
          QDomNodeList FeatureClassList = nodeTemp.childNodes();
          for (int j = 0; j < FeatureClassList.size(); j++)
          {
            QDomNode FeatureClassNode = FeatureClassList.at(j);
            QDomElement fieldElement = FeatureClassNode.toElement();
            QString qstrFeatureClass = fieldElement.text();
            param.vFeatureClassCode.push_back(qstrFeatureClass.toStdString());
          }
        }
      }
    }

    this->m_vExtractDataParam.push_back(param);

    node = node.nextSibling();//读取下一个兄弟节点
  }
}

int get_shp_data_from_gpkg::perform_process_on_a_single_work_area()
{

  //GeoExtractAndProcessProgressFunc pfnProgress;
  // 进度回调函数待补充（生成进度条）
  int iResult = CSE_GeoExtractAndProcess::ExtractDataByAttribute(
    this->m_InputDataType,
    this->m_iScaleType,
    this->m_bOutputSheet,
    this->m_vExtractDataParam,
    this->m_input_data_path,
    this->m_iOutputDataType,
    this->m_output_data_path,
    nullptr);

  if (iResult != 0)
  {
    //  进行错误处理
    log_message::show_log_message(5);
    return 5;
  }
  return 0;
}

