#include "merge_process.h"

automatic_edge_connection::automatic_edge_connection(
  const std::string& xml_configure_file_data_path,
  const std::string& gpkg_data_path,
  const int& iScaleType,
  const double& dDistance)
{
  this->m_xml_configure_file_data_path = xml_configure_file_data_path;
  this->m_gpkg_data_path = gpkg_data_path;
  this->m_iScaleType = iScaleType;
  this->m_dDistance = dDistance;
  this->m_vLayerMatchParam.clear();
}

automatic_edge_connection::~automatic_edge_connection()
{

}

void automatic_edge_connection::perform_edge_joining_operations_on_a_single_work_area()
{
  this->load_merge_params();
  this->auto_merge_on_a_single_work_area();
}

void automatic_edge_connection::load_merge_params()
{
  const std::string& xml_configure_file_data_path = this->m_xml_configure_file_data_path;

  QString strFileName = QString::fromStdString(xml_configure_file_data_path);

  QDomDocument doc;
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    //  需要进行错误处理
    log_message::show_log_message(4);
  }
  //  判断XML文件是否可以正常打开
  if (!doc.setContent(&file))
  {
    file.close();
  }
  file.close();

  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点   QDomNode 节点
  QDomNode node = root.firstChild();

  //  将XML文件中的配置信息读取到m_vLayerMatchParam自定义结构体中，循环遍历每个子节点
  while (!node.isNull())
  {
    //  设置一个局部的变量用来存储读取到的单个图层XML配置信息
    LayerMatchParam param;

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
        else if (elementTemp.tagName() == "LineChecked")
        {
          if (elementTemp.text() == "true")
          {
            param.bLineStringChecked = true;
          }
          else
          {
            param.bLineStringChecked = false;
          }
        }
        else if (elementTemp.tagName() == "PolygonChecked")
        {
          if (elementTemp.text() == "true")
          {
            param.bPolygonChecked = true;
          }
          else
          {
            param.bPolygonChecked = false;
          }
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
      }
    }

    this->m_vLayerMatchParam.push_back(param);

    node = node.nextSibling();//读取下一个兄弟节点
  }

}


int automatic_edge_connection::auto_merge_on_a_single_work_area()
{
  // 如果接边参数没有设置，则进行提示
  if (m_vLayerMatchParam.size() == 0)
  {
    log_message::show_log_message(2);
    return 3;
  }

  //  这是对一个作业区内存在多个gpkg文件的时候循环进行接边处理，暂时先对一个gpkg文件中的多个图幅进行接边处理
  //for (int i = 0; i < m_vGpkgPath.size(); i++)
  //{
  //  // 调用接口进行自动接边,调用接边接口进行计算
  //  std::vector<LayerMergeRecord> vLayerMergeRecord;
  //  vLayerMergeRecord.clear();
  //  int iResult = CSE_GeoExtractAndProcess::OpAutoMerge(
  //    this->m_vLayerMatchParam,
  //    this->m_iScaleType,
  //    this->m_dDistance,
  //    this->m_vGpkgPath[i],
  //    vLayerMergeRecord);
  //  if (iResult != 0)
  //  {
  //    //  进行错误处理
  //    log_message::show_log_message(iResult);
  //    return iResult;
  //  }
  //}


  // 调用接口进行自动接边，调用接边接口进行计算
  vector<LayerMergeRecord> vLayerMergeRecord;
  vLayerMergeRecord.clear();

  int iResult = CSE_GeoExtractAndProcess::OpAutoMerge(
    this->m_vLayerMatchParam,
    this->m_iScaleType,
    this->m_dDistance,
    this->m_gpkg_data_path,
    vLayerMergeRecord);

  if (iResult != 0)
  {
    //  进行错误处理
    log_message::show_log_message(5);
    return 5;
  }
  return 0;
}
