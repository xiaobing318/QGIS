#include "convert_multi_datasrc2gjb.h"

convert_multi_datasrc2gjb::convert_multi_datasrc2gjb(
  const int& data_source_type,
  const std::string& input_data_path,
  const std::string& output_data_path)
{
  this->m_data_source_type = data_source_type;
  this->m_input_data_path = input_data_path;
  this->m_output_data_path = output_data_path;

}

convert_multi_datasrc2gjb::~convert_multi_datasrc2gjb()
{

}

void convert_multi_datasrc2gjb::perform_convert_multi_datasrc2gjb()
{
  /*
  算法流程：
    2. 加载包含不同标准之间映射规则的各种配置文件。
    3. 检查数据中的图层名称与提取的名称的一致性。
    4. 使用提取和验证的数据执行最终的转换过程。
  */

#pragma region "（第一步）获取数据源类型、获取输出数据路径、获取保存处理后数据的路径"
  // 获取数据来源
  int iDataType = this->m_data_source_type;

  // 获取输入数据路径
  string strInputDataPath = this->m_input_data_path;

  // 获取数据保存目录
  string strSaveDataPath = this->m_output_data_path;

  // 获取图幅列表
  QStringList qstrPathList = GetDirPathOfSplDir(QString::fromStdString(strInputDataPath));

#pragma endregion

#pragma region "（第二步）DLHJ：将配置文件读取到自定义结构体中（从用户的角度来说，并不会关心配置文件，因此关于配置文件的检查将会在另外的部分进行检查）"

  //	加载一体化数据格式配置文件（不同图层分类中的属性字段配置文件）
  GJBLayerFieldLayer_List_t GJBLayerFieldLayer_List;
  /*国军标图层配置文件*/
  LoadDLHJConfig_GJBLayerField(GJBLayerFieldLayer_List);

  //	GB（国标数据）
  GBlayerLayer_List_t GBlayerLayer_List;
  /*国标编码到国军标编码配置文件*/
  GB2GJBLayer_List_t GB2GJBLayer_List;

  //	JB（军标数据）
  JBlayerLayer_List_t JBlayerLayer_List;
  /*军标编码到国军标编码配置文件*/
  JB2GJBLayer_List_t JB2GJBLayer_List;

  //	DH（导航数据）
  DHlayerLayer_List_t DHlayerLayer_List;
  /*导航编码到国军标编码配置文件*/
  DH2GJBLayer_List_t DH2GJBLayer_List;

  //	OSM（OSM数据）
  OSMlayerLayer_List_t OSMlayerLayer_List;
  /*OSM编码到国军标编码配置文件*/
  OSM2GJBLayer_List_t OSM2GJBLayer_List;

  //	TDT（TDT数据）
  TDTlayerLayer_List_t TDTlayerLayer_List;
  /*TDT编码到国军标编码配置文件*/
  TDT2GJBLayer_List_t TDT2GJBLayer_List;

  //	根据不同的数据来源加载图层配置文件、数据源编码到一体化编码配置文件
  if (iDataType == 0)
  {
    /*国标图层配置文件*/
    LoadDLHJConfig_GBLayer(GBlayerLayer_List);
    LoadDLHJConfig_GBCode2GJBCode(GB2GJBLayer_List);

    // 从上述读取到的数据中提取国标图层名称列表
    vector<string> GBlayerLayer_List_name;
    GBlayerLayer_List_name.clear();
    ExtractGBlayerNameList(GBlayerLayer_List, GBlayerLayer_List_name);

    //	检查分幅数据中的shp图层名是否和第三步中得到的名称列表一致（为了检查文件名的有效性或者规范性）
    IsTheLayerNamingStandardized(qstrPathList, QString::fromStdString(strInputDataPath), GBlayerLayer_List_name);
  }
  else if (iDataType == 1)
  {
    /*军标图层配置文件*/
    LoadDLHJConfig_JBLayer(JBlayerLayer_List);
    LoadDLHJConfig_JBCode2GJBCode(JB2GJBLayer_List);

    // 从上述读取到的数据中提取军标图层名称列表
    vector<string> JBlayerLayer_List_name;
    JBlayerLayer_List_name.clear();
    ExtractJBlayerNameList(JBlayerLayer_List, JBlayerLayer_List_name);

    //	检查分幅数据中的shp图层名是否和第三步中得到的名称列表一致（为了检查文件名的有效性或者规范性）
    IsTheLayerNamingStandardized(qstrPathList, QString::fromStdString(strInputDataPath), JBlayerLayer_List_name);
  }
  else if (iDataType == 2)
  {
    /*导航图层配置文件*/
    LoadDLHJConfig_DHLayer(DHlayerLayer_List);
    LoadDLHJConfig_DHCode2GJBCode(DH2GJBLayer_List);

    // 从上述读取到的数据中提取国标图层名称列表
    vector<string> DHlayerLayer_List_name;
    DHlayerLayer_List_name.clear();
    ExtractDHlayerNameList(DHlayerLayer_List, DHlayerLayer_List_name);

    //	检查分幅数据中的shp图层名是否和第三步中得到的名称列表一致（为了检查文件名的有效性或者规范性）
    IsTheLayerNamingStandardized(qstrPathList, QString::fromStdString(strInputDataPath), DHlayerLayer_List_name);
  }
  else if (iDataType == 3)
  {
    /*OSM图层配置文件*/
    LoadDLHJConfig_OSMLayer(OSMlayerLayer_List);
    LoadDLHJConfig_OSMCode2GJBCode(OSM2GJBLayer_List);

    // 从上述读取到的数据中提取国标图层名称列表
    vector<string> OSMlayerLayer_List_name;
    OSMlayerLayer_List_name.clear();
    ExtractOSMlayerNameList(OSMlayerLayer_List, OSMlayerLayer_List_name);

    //	检查分幅数据中的shp图层名是否和第三步中得到的名称列表一致（为了检查文件名的有效性或者规范性）
    IsTheLayerNamingStandardized(qstrPathList, QString::fromStdString(strInputDataPath), OSMlayerLayer_List_name);
  }
  else if (iDataType == 4)
  {
    /*TDT图层配置文件*/
    LoadDLHJConfig_TDTLayer(TDTlayerLayer_List);
    LoadDLHJConfig_TDTCode2GJBCode(TDT2GJBLayer_List);

    // 从上述读取到的数据中提取国标图层名称列表
    vector<string> TDTlayerLayer_List_name;
    TDTlayerLayer_List_name.clear();
    ExtractTDTlayerNameList(TDTlayerLayer_List, TDTlayerLayer_List_name);

    //	检查分幅数据中的shp图层名是否和第三步中得到的名称列表一致（为了检查文件名的有效性或者规范性）
    IsTheLayerNamingStandardized(qstrPathList, QString::fromStdString(strInputDataPath), TDTlayerLayer_List_name);
  }

#pragma endregion

#pragma region "（第三步）DLHJ：从集合中选择出部分正确的子集（有效的shp文件），然后利用GJB一体化生成接口进行处理"

#pragma region "1、将多个有效的分幅数据存放在向量中，用作GJB一体化生成接口的参数"
  //	存储多个分幅数据的绝对路径
  std::vector<std::string> vInputPath;
  vInputPath.clear();

  /*
    qstrPathList.size() == 0说明指定目录下没有子目录（qstrPathList：多个图幅目录组成一个图幅列表）
  那么strInputDataPath这个路径下只会存在分幅路径（其中有多个shp文件），这样设计可以使得选中一个包含多个分幅数据
  的路径可以正常工作，也可以使得单个分幅数据正常工作（杨小兵-2023-9-9）
  */
  // 如果是单图幅，将这个单个图幅的路径保存到vInputPath
  if (qstrPathList.size() == 0)
  {
    //	strInputDataPath为输入数据路径（m_qstrInputDataPath经过转化得到的）
    vInputPath.push_back(strInputDataPath);
  }
  else
  {
    //	将所有子目录的绝对路径添加到 `vInputPath` 中
    for (int i = 0; i < qstrPathList.size(); i++)
    {
      string strPathTemp = qstrPathList[i].toStdString();
      vInputPath.push_back(strPathTemp);
    }
  }

#pragma endregion

#pragma region "2、利用GJB一体化生成接口进行处理"

  // 调用国军标一体化数据生成算法
  /*杨小兵-2024-03-03
  参数说明：
    vInputPath：strInputDataPath路径下的多个分幅数据的绝对路径
    strSaveDataPath:调用GJB一体化生成接口后产生数据的保存目录
    iDataType：数据来源（GB、JB或者是其他的数据来源）
    GBlayerLayer_List：国标图层配置文件（从XML文件提取到自定义数据结构中）
    JBlayerLayer_List：军标图层配置文件（从XML文件提取到自定义数据结构中）
    GJBLayerFieldLayer_List：国军标一体化配置文件（从XML文件提取到自定义数据结构中）
    GB2GJBLayer_List：GB到GJB编码映射配置文件（从XML文件提取到自定义数据结构中）
    JB2GJBLayer_List：JB到GJB编码映射配置文件（从XML文件提取到自定义数据结构中）
  */
  int iResult = 0;
  if (iDataType == 0)
  {
    iResult = CSE_GeoExtractAndProcess::DLHJConvertGBDataToGJB(
      vInputPath,
      strSaveDataPath,
      iDataType,
      GBlayerLayer_List,
      GJBLayerFieldLayer_List,
      GB2GJBLayer_List,
      NULL);
  }
  else if (iDataType == 1)
  {
    iResult = CSE_GeoExtractAndProcess::DLHJConvertJBDataToGJB(
      vInputPath,
      strSaveDataPath,
      iDataType,
      JBlayerLayer_List,
      GJBLayerFieldLayer_List,
      JB2GJBLayer_List,
      NULL);
  }
  else if (iDataType == 2)
  {
    iResult = CSE_GeoExtractAndProcess::DLHJConvertDHDataToGJB(
      vInputPath,
      strSaveDataPath,
      iDataType,
      DHlayerLayer_List,
      GJBLayerFieldLayer_List,
      DH2GJBLayer_List,
      NULL);
  }
  else if (iDataType == 3)
  {
    iResult = CSE_GeoExtractAndProcess::DLHJConvertOSMDataToGJB(
      vInputPath,
      strSaveDataPath,
      iDataType,
      OSMlayerLayer_List,
      GJBLayerFieldLayer_List,
      OSM2GJBLayer_List,
      NULL);
  }
  else if (iDataType == 4)
  {
    iResult = CSE_GeoExtractAndProcess::DLHJConvertTDTDataToGJB(
      vInputPath,
      strSaveDataPath,
      iDataType,
      TDTlayerLayer_List,
      GJBLayerFieldLayer_List,
      TDT2GJBLayer_List,
      NULL);
  }

  //	GJB一体化数据生成不成功
  if (iResult != 0)
  {
    log_message::show_log_message(5);
  }
  //	GJB一体化数据生成成功！
#pragma endregion

#pragma endregion

}


/*获取在指定目录下的目录的路径*/
QStringList convert_multi_datasrc2gjb::GetDirPathOfSplDir(const QString& dirPath)
{
  QStringList dirPaths;
  QDir splDir(dirPath);
  QFileInfoList fileInfoListInSplDir = splDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  QFileInfo tempFileInfo;
  foreach(tempFileInfo, fileInfoListInSplDir) {
    dirPaths << tempFileInfo.absoluteFilePath();
  }
  return dirPaths;
}


/*国军标图层配置文件*/
void convert_multi_datasrc2gjb::LoadDLHJConfig_GJBLayerField(GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List)
{
  /*
  函数功能：
    函数主要用于从 `GJB_LayerField.xml` 配置文件中加载国军标（GJB）图层字段的配置信息。
  这些信息被解析并存储在一个自定义数据结构 `GJBLayerFieldLayer_List_t` 中，以便后续操作。

  算法流程：
    1. 构建 XML 配置文件的完整路径。
    2. 打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中。
    3. 遍历 XML 文档的根节点下的每一个子节点（标记为 "Layer"）。
    4. 对于每个 "Layer" 节点，提取其中的 "LayerName"、"LayerNameAlias" 和 "Field_List" 等信息，并存储在一个临时的 `GJBLayerFieldLayer_t` 结构体中。
    5. 如果存在 "Field_List"，则进一步提取其下的 "Field" 节点信息，包括字段名称、别名、类型、长度等，并存储在 `GJBLayerFieldField_List_t` 结构体中。
    6. 如果存在 "DataSource_List"，则进一步提取其下的 "DataSource" 节点信息，并存储在 `GJBLayerFieldDataSource_List_t` 结构体中。
    7. 将所有提取的信息汇总到 `GJBLayerFieldLayer_t` 结构体，并添加到 `GJBLayerFieldLayer_List.vLayer` 列表中。
    8. 循环步骤 3-7，直到处理完所有 "Layer" 节点。
  */
  //	构建到国标XML配置文件的路径
  // 定义一个字符数组来存储路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);

  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/GJB_LayerField.xml");

  //	首先对XML配置文件进行检查
  //checkAndDebugXML(strFileName);


  //	首先以读取模式打开XML文件
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

  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点   QDomNode 节点
  QDomNode current_layer_node = root.firstChild();
  while (!current_layer_node.isNull())
  {
    //	创建一个临时的Layer，用来读取到的数据
    GJBLayerFieldLayer_t temp_layer;

    // 当前节点的标签如果是Layer的话
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
        if (Layer_child_element.tagName() == "GJBLayerName")
        {
          temp_layer.LayerName = Layer_child_element.text().toLocal8Bit();
        }
        else if (Layer_child_element.tagName() == "GJBLayerNameAlias")
        {
          temp_layer.LayerNameAlias = Layer_child_element.text().toLocal8Bit();
        }
        else if (Layer_child_element.tagName() == "Field_List")
        {
          //	创建一个临时的Field_List数据结构，用来存放读取到的数据
          GJBLayerFieldField_List_t temp_Field_List;

          // 获取Field_List的所有子节点
          QDomNodeList Field_List_all_child_nodes = Layer_child_nodes.childNodes();
          for (size_t j = 0; j < Field_List_all_child_nodes.size(); j++)
          {

            //	获取Field_List的所有子节点的第j个子节点
            QDomNode Field = Field_List_all_child_nodes.at(j);

            //	判断当前节点的标签是否为Field
            if (Field.toElement().tagName() == "Field")
            {
              // 创建一个临时的Field，用来存放读取到的数据
              GJBLayerFieldField_t temp_Field;
              //	获取当前Field节点的所有子节点
              QDomNodeList Field_all_child_nodes = Field.childNodes();
              //	循环处理Field节点的所有子节点
              for (size_t k = 0; k < Field_all_child_nodes.size(); k++)
              {

                //	获取Field节点的所有子节点的第k个子节点
                QDomNode Field_child_node = Field_all_child_nodes.at(k);
                QDomElement Field_child_element = Field_child_node.toElement();

                // 依次判断字段节点下所有节点的名称
                if (Field_child_element.tagName() == "Name")
                {
                  temp_Field.Name = Field_child_element.text().toLocal8Bit();
                }
                else if (Field_child_element.tagName() == "Alias")
                {
                  temp_Field.Alias = Field_child_element.text().toLocal8Bit();
                }
                else if (Field_child_element.tagName() == "Type")
                {
                  temp_Field.Type = Field_child_element.text().toLocal8Bit();
                }
                else if (Field_child_element.tagName() == "Len")
                {
                  temp_Field.Len = Field_child_element.text().toLocal8Bit().toInt();
                }
                else if (Field_child_element.tagName() == "DataSource_List")
                {
                  //	创建一个临时的DataSource_List用来存放读取到的数据
                  GJBLayerFieldDataSource_List_t temp_DataSource_List;

                  // 获取DataSource_List节点的所有子节点
                  QDomNodeList DataSource_List_all_child_nodes = Field_child_node.childNodes();
                  //	循环处理DataSource_List的所有子节点
                  for (size_t m = 0; m < DataSource_List_all_child_nodes.size(); m++)
                  {
                    //	获取当前DataSource_List节点的所有子节点中的第m个节点
                    QDomNode DataSource = DataSource_List_all_child_nodes.at(m);
                    if (DataSource.toElement().tagName() == "DataSource")
                    {
                      //	创建一个临时的DataSource数据结构用来存放读取到的数据
                      GJBLayerFieldDataSource_t temp_DataSource;
                      //	获取当前DataSource所有的子节点
                      QDomNodeList DataSource_all_child_nodes = DataSource.childNodes();
                      //	循环处理DataSource所有的子节点
                      for (size_t n = 0; n < DataSource_all_child_nodes.size(); n++)
                      {
                        //	获取DataSource节点下所有节点中的第n个子节点
                        QDomNode DataSource_child_node = DataSource_all_child_nodes.at(n);
                        QDomElement DataSource_child_element = DataSource_child_node.toElement();

                        if (DataSource_child_element.tagName() == "DataSourceName")
                        {
                          temp_DataSource.DataSourceName = DataSource_child_element.text().toLocal8Bit();
                        }
                        else if (DataSource_child_element.tagName() == "DataSourceLayerName")
                        {
                          temp_DataSource.DataSourceLayerName = DataSource_child_element.text().toLocal8Bit();
                        }
                        else if (DataSource_child_element.tagName() == "DataSourceField")
                        {
                          temp_DataSource.DataSourceField = DataSource_child_element.text().toLocal8Bit();
                        }

                      }

                      //	将读取到的Layer数据存放到上一层数据结构中
                      temp_DataSource_List.vDataSource.push_back(temp_DataSource);
                    }

                  }

                  //	将读取到的Layer数据存放到上一层数据结构中
                  temp_Field.DataSource_List = temp_DataSource_List;
                }

              }

              //	将读取到的Layer数据存放到上一层数据结构中
              temp_Field_List.vField.push_back(temp_Field);
            }

          }

          //	将读取到的Layer数据存放到上一层数据结构中
          temp_layer.Field_List = temp_Field_List;
        }
      }

    }

    //	将读取到的Layer数据存放到上一层数据结构中
    GJBLayerFieldLayer_List.vLayer.push_back(temp_layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }

}

/*国标图层配置文件*/
void convert_multi_datasrc2gjb::LoadDLHJConfig_GBLayer(GBlayerLayer_List_t& GBlayerLayer_List)
{
  /*
  函数功能说明：
    1. 该函数主要用于从特定的XML配置文件中读取内容，并将其存储到自定义的数据结构（结构体）中。
    2. 在进行XML内容解析前，调用`checkAndDebugXML()`函数进行文件检查

  算法流程：
    1. 构建指向国标XML配置文件的路径。
    2. 调用`checkAndDebugXML()`函数进行XML配置文件的合法性检查。
    3. 以只读模式打开XML文件。
    4. 使用QDomDocument对象解析XML文件。
    5. 关闭文件。
    6. 读取并遍历XML文件的根节点和子节点。
    7. 将读取的内容存入预定义的数据结构。

  */
  //	构建到国标XML配置文件的路径
  // 定义一个字符数组来存储路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);

  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/GB_Layer.xml");

  //	首先对XML配置文件进行检查
  //checkAndDebugXML(strFileName);

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

  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点(QDomNode节点)
  QDomNode current_layer_node = root.firstChild();
  // 循环遍历每个根节点的每一个子节点（Layer为子节点）
  while (!current_layer_node.isNull())
  {
    //	创建一个临时的Layer，用来存放读取到的Layer数据
    GBlayerLayer_t temp_Layer;
    //	判断当前Layer节点的标签是否为Layer
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取“Layer”节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理“Layer”节点的所有子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取“Layer”节点的所有子节点的第i个子节点
        QDomNode Layer_child_node = Layer_all_child_nodes.at(i);
        QDomElement elementTemp = Layer_child_node.toElement();
        if (elementTemp.tagName() == "GBLayerName")
        {
          temp_Layer.LayerName = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "GBLayerNameAlias")
        {
          temp_Layer.LayerNameAlias = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "GBLayerGeoType")
        {
          temp_Layer.LayerGeoType = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "GJBLayerList")
        {
          //	创建一个临时的GJBLayerList数据结构，用来存放读取到的数据
          GBlayerGJBLayerList_t temp_GJBLayerList;

          //	获取GJBLayerList的所有子节点
          QDomNodeList GJBLayerList_all_child_nodes = Layer_child_node.childNodes();
          //	循环处理GJBLayerList所有的子节点
          for (size_t j = 0; j < GJBLayerList_all_child_nodes.size(); j++)
          {
            //	获取GJBLayerList所有的子节点的第j个子节点
            QDomNode GJBLayer_node = GJBLayerList_all_child_nodes.at(j);
            //	转化成元素数据类型
            QDomElement GJBLayer_element = GJBLayer_node.toElement();
            QByteArray qByte = GJBLayer_element.text().toLocal8Bit();
            string GJBLayer = string(qByte);
            temp_GJBLayerList.vGJBLayer.push_back(GJBLayer);

          }

          //	将读取到的temp_GJBLayerList存放到上一层数据结构中
          temp_Layer.GJBLayerList = temp_GJBLayerList;
        }
      }
    }

    //	将读取到的Layer数据存放到上一层数据结构中
    GBlayerLayer_List.vLayer.push_back(temp_Layer);

    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }

}

/*军标图层配置文件*/
void convert_multi_datasrc2gjb::LoadDLHJConfig_JBLayer(JBlayerLayer_List_t& JBlayerLayer_List)
{
  /*
  函数功能：
    1、从一个 XML 文件中加载军标图层的配置数据，这些数据将被存储在一个自定义的数据结构中，以便后续使用。
  算法流程
    1. 构建 XML 配置文件的完整路径。
    2. 打开 XML 文件，并将其内容加载到 QDomDocument 对象中。
    3. 在文档中迭代，特别是查找标记为 "Layer" 的节点。
    4. 对于每个 "Layer" 节点，使用其数据填充一个临时的 `JBLayerLayer_t` 对象。
    5. 如果找到一个 "GJBLayerList" 节点，使用其数据填充一个 `JBLayerGJBLayerList_t` 对象。
    6. 将填充好的 `JBLayerLayer_t` 对象添加到 `JBlayerLayer_List.vLayer` 列表中。
    7. 继续，直到处理完所有的 "Layer" 节点。

  */
  //	构建到国标XML配置文件的路径
  // 定义一个字符数组来存储路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);
  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/JB_Layer.xml");


  //	对XML配置文件进行检查
  //checkAndDebugXML(strFileName);


  //	首先以读取模式打开XML文件
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

  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点(QDomNode节点)
  QDomNode current_layer_node = root.firstChild();
  while (!current_layer_node.isNull())
  {
    //	创建一个临时的Layer，用来存放读取到的Layer数据
    JBLayerLayer_t temp_Layer;
    // 判断当前节点的标签是否为Layer
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取当前Layer节点的所有子节点
      QDomNodeList layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理当前Layer节点的所有子节点
      for (size_t i = 0; i < layer_all_child_nodes.size(); i++)
      {
        //	获取当前Layer节点的所有子节点的第i个节点
        QDomNode layer_child_node = layer_all_child_nodes.at(i);
        QDomElement elementTemp = layer_child_node.toElement();
        if (elementTemp.tagName() == "JBLayerName")
        {
          temp_Layer.LayerName = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "JBLayerNameAlias")
        {
          temp_Layer.LayerNameAlias = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "GJBLayerList")
        {
          //	创建一个临时的GJBLayerList，用来存放读取到的GJBLayerList数据
          JBlayerGJBLayerList_t temp_GJBLayerList;
          // 获取GJBLayerList所有子节点
          QDomNodeList GJBLayerList_all_child_nodes = layer_child_node.childNodes();
          for (size_t j = 0; j < GJBLayerList_all_child_nodes.size(); j++)
          {

            //	获取GJBLayerList所有子节点的第j个子节点
            QDomNode GJBLayer_node = GJBLayerList_all_child_nodes.at(j);
            QDomElement GJBLayer_element = GJBLayer_node.toElement();
            QString qstrGJB = GJBLayer_element.text();
            string GJBLayer = qstrGJB.toLocal8Bit();

            //	将读取到的Layer数据存放到上一层数据结构中
            temp_GJBLayerList.vGJBLayer.push_back(GJBLayer);

          }

          //	将读取到的Layer数据存放到上一层数据结构中
          temp_Layer.GJBLayerList = temp_GJBLayerList;
        }
      }
    }

    //	将读取到的Layer数据存放到上一层数据结构中
    JBlayerLayer_List.vLayer.push_back(temp_Layer);
    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }

}

/*导航图层配置文件*/
void convert_multi_datasrc2gjb::LoadDLHJConfig_DHLayer(DHlayerLayer_List_t& DHlayerLayer_List)
{
  /*
  函数功能说明：
    1. 该函数主要用于从特定的XML配置文件中读取内容，并将其存储到自定义的数据结构（结构体）中。
    2. 在进行XML内容解析前，调用`checkAndDebugXML()`函数进行文件检查

  算法流程：
    1. 构建指向国标XML配置文件的路径。
    2. 调用`checkAndDebugXML()`函数进行XML配置文件的合法性检查。
    3. 以只读模式打开XML文件。
    4. 使用QDomDocument对象解析XML文件。
    5. 关闭文件。
    6. 读取并遍历XML文件的根节点和子节点。
    7. 将读取的内容存入预定义的数据结构。

  */
  //	构建到国标XML配置文件的路径
  // 定义一个字符数组来存储路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);

  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/DH_Layer.xml");


  //	首先对XML配置文件进行检查
  //checkAndDebugXML(strFileName);

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

  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点(QDomNode节点)
  QDomNode current_layer_node = root.firstChild();
  // 循环遍历每个根节点的每一个子节点（Layer为子节点）
  while (!current_layer_node.isNull())
  {
    //	创建一个临时的Layer，用来存放读取到的Layer数据
    DHlayerLayer_t temp_Layer;
    //	判断当前Layer节点的标签是否为Layer
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取“Layer”节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理“Layer”节点的所有子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取“Layer”节点的所有子节点的第i个子节点
        QDomNode Layer_child_node = Layer_all_child_nodes.at(i);
        QDomElement elementTemp = Layer_child_node.toElement();
        if (elementTemp.tagName() == "DHLayerName")
        {
          temp_Layer.LayerName = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "DHLayerNameAlias")
        {
          temp_Layer.LayerNameAlias = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "DHLayerGeoType")
        {
          temp_Layer.LayerGeoType = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "GJBLayerList")
        {
          //	创建一个临时的GJBLayerList数据结构，用来存放读取到的数据
          DHlayerGJBLayerList_t temp_GJBLayerList;

          //	获取GJBLayerList的所有子节点
          QDomNodeList GJBLayerList_all_child_nodes = Layer_child_node.childNodes();
          //	循环处理GJBLayerList所有的子节点
          for (size_t j = 0; j < GJBLayerList_all_child_nodes.size(); j++)
          {
            //	获取GJBLayerList所有的子节点的第j个子节点
            QDomNode GJBLayer_node = GJBLayerList_all_child_nodes.at(j);
            //	转化成元素数据类型
            QDomElement GJBLayer_element = GJBLayer_node.toElement();
            QByteArray qByte = GJBLayer_element.text().toLocal8Bit();
            string GJBLayer = string(qByte);
            temp_GJBLayerList.vGJBLayer.push_back(GJBLayer);

          }

          //	将读取到的temp_GJBLayerList存放到上一层数据结构中
          temp_Layer.GJBLayerList = temp_GJBLayerList;
        }
      }
    }

    //	将读取到的Layer数据存放到上一层数据结构中
    DHlayerLayer_List.vLayer.push_back(temp_Layer);

    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }

}

/*OSM图层配置文件*/
void convert_multi_datasrc2gjb::LoadDLHJConfig_OSMLayer(OSMlayerLayer_List_t& OSMlayerLayer_List)
{
  /*
  函数功能说明：
    1. 该函数主要用于从特定的XML配置文件中读取内容，并将其存储到自定义的数据结构（结构体）中。
    2. 在进行XML内容解析前，调用`checkAndDebugXML()`函数进行文件检查

  算法流程：
    1. 构建指向国标XML配置文件的路径。
    2. 调用`checkAndDebugXML()`函数进行XML配置文件的合法性检查。
    3. 以只读模式打开XML文件。
    4. 使用QDomDocument对象解析XML文件。
    5. 关闭文件。
    6. 读取并遍历XML文件的根节点和子节点。
    7. 将读取的内容存入预定义的数据结构。

  */
  //	构建到国标XML配置文件的路径
  // 定义一个字符数组来存储路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);

  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/OSM_Layer.xml");


  //	首先对XML配置文件进行检查
  //checkAndDebugXML(strFileName);

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

  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点(QDomNode节点)
  QDomNode current_layer_node = root.firstChild();
  // 循环遍历每个根节点的每一个子节点（Layer为子节点）
  while (!current_layer_node.isNull())
  {
    //	创建一个临时的Layer，用来存放读取到的Layer数据
    OSMlayerLayer_t temp_Layer;
    //	判断当前Layer节点的标签是否为Layer
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取“Layer”节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理“Layer”节点的所有子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取“Layer”节点的所有子节点的第i个子节点
        QDomNode Layer_child_node = Layer_all_child_nodes.at(i);
        QDomElement elementTemp = Layer_child_node.toElement();
        if (elementTemp.tagName() == "OSMLayerName")
        {
          temp_Layer.LayerName = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "OSMLayerNameAlias")
        {
          temp_Layer.LayerNameAlias = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "OSMLayerGeoType")
        {
          temp_Layer.LayerGeoType = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "GJBLayerList")
        {
          //	创建一个临时的GJBLayerList数据结构，用来存放读取到的数据
          OSMlayerGJBLayerList_t temp_GJBLayerList;

          //	获取GJBLayerList的所有子节点
          QDomNodeList GJBLayerList_all_child_nodes = Layer_child_node.childNodes();
          //	循环处理GJBLayerList所有的子节点
          for (size_t j = 0; j < GJBLayerList_all_child_nodes.size(); j++)
          {
            //	获取GJBLayerList所有的子节点的第j个子节点
            QDomNode GJBLayer_node = GJBLayerList_all_child_nodes.at(j);
            //	转化成元素数据类型
            QDomElement GJBLayer_element = GJBLayer_node.toElement();
            QByteArray qByte = GJBLayer_element.text().toLocal8Bit();
            string GJBLayer = string(qByte);
            temp_GJBLayerList.vGJBLayer.push_back(GJBLayer);

          }

          //	将读取到的temp_GJBLayerList存放到上一层数据结构中
          temp_Layer.GJBLayerList = temp_GJBLayerList;
        }
      }
    }

    //	将读取到的Layer数据存放到上一层数据结构中
    OSMlayerLayer_List.vLayer.push_back(temp_Layer);

    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }

}

/*TDT图层配置文件*/
void convert_multi_datasrc2gjb::LoadDLHJConfig_TDTLayer(TDTlayerLayer_List_t& TDTlayerLayer_List)
{
  /*
  函数功能说明：
    1. 该函数主要用于从特定的XML配置文件中读取内容，并将其存储到自定义的数据结构（结构体）中。
    2. 在进行XML内容解析前，调用`checkAndDebugXML()`函数进行文件检查

  算法流程：
    1. 构建指向国标XML配置文件的路径。
    2. 调用`checkAndDebugXML()`函数进行XML配置文件的合法性检查。
    3. 以只读模式打开XML文件。
    4. 使用QDomDocument对象解析XML文件。
    5. 关闭文件。
    6. 读取并遍历XML文件的根节点和子节点。
    7. 将读取的内容存入预定义的数据结构。

  */
  //	构建到国标XML配置文件的路径
  // 定义一个字符数组来存储路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);

  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/TDT_Layer.xml");


  //	首先对XML配置文件进行检查
  //checkAndDebugXML(strFileName);

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

  // 读取根节点
  QDomElement root = doc.documentElement();
  // 读取第一个子节点(QDomNode节点)
  QDomNode current_layer_node = root.firstChild();
  // 循环遍历每个根节点的每一个子节点（Layer为子节点）
  while (!current_layer_node.isNull())
  {
    //	创建一个临时的Layer，用来存放读取到的Layer数据
    TDTlayerLayer_t temp_Layer;
    //	判断当前Layer节点的标签是否为Layer
    if (current_layer_node.toElement().tagName() == "Layer")
    {
      //	获取“Layer”节点的所有子节点
      QDomNodeList Layer_all_child_nodes = current_layer_node.childNodes();
      //	循环处理“Layer”节点的所有子节点
      for (size_t i = 0; i < Layer_all_child_nodes.size(); i++)
      {
        //	获取“Layer”节点的所有子节点的第i个子节点
        QDomNode Layer_child_node = Layer_all_child_nodes.at(i);
        QDomElement elementTemp = Layer_child_node.toElement();
        if (elementTemp.tagName() == "TDTLayerName")
        {
          temp_Layer.LayerName = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "TDTLayerNameAlias")
        {
          temp_Layer.LayerNameAlias = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "TDTLayerGeoType")
        {
          temp_Layer.LayerGeoType = elementTemp.text().toLocal8Bit();
        }
        else if (elementTemp.tagName() == "GJBLayerList")
        {
          //	创建一个临时的GJBLayerList数据结构，用来存放读取到的数据
          TDTlayerGJBLayerList_t temp_GJBLayerList;

          //	获取GJBLayerList的所有子节点
          QDomNodeList GJBLayerList_all_child_nodes = Layer_child_node.childNodes();
          //	循环处理GJBLayerList所有的子节点
          for (size_t j = 0; j < GJBLayerList_all_child_nodes.size(); j++)
          {
            //	获取GJBLayerList所有的子节点的第j个子节点
            QDomNode GJBLayer_node = GJBLayerList_all_child_nodes.at(j);
            //	转化成元素数据类型
            QDomElement GJBLayer_element = GJBLayer_node.toElement();
            QByteArray qByte = GJBLayer_element.text().toLocal8Bit();
            string GJBLayer = string(qByte);
            temp_GJBLayerList.vGJBLayer.push_back(GJBLayer);

          }

          //	将读取到的temp_GJBLayerList存放到上一层数据结构中
          temp_Layer.GJBLayerList = temp_GJBLayerList;
        }
      }
    }

    //	将读取到的Layer数据存放到上一层数据结构中
    TDTlayerLayer_List.vLayer.push_back(temp_Layer);

    //读取下一个兄弟节点
    current_layer_node = current_layer_node.nextSibling();
  }

}



// GB到GJB编码配置文件
void convert_multi_datasrc2gjb::LoadDLHJConfig_GBCode2GJBCode(GB2GJBLayer_List_t& GB2GJBLayer_List)
{
  /*
  函数功能：
    函数主要用于从 `GBCode2GJBCode.xml` 文件中加载国家标准（GB）到国军标（GJB）的编码映射信息。
  这些信息被解析并存储在一个自定义的数据结构 `GB2GJBLayer_List_t` 中，以便于后续的数据转换操作。

  算法流程：
    1. 构建 XML 文件的完整路径。
    2. 打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中。
    3. 遍历 XML 文档的根节点下的所有子节点（标记为 "Layer"）。
    4. 对于每个 "Layer" 节点，提取其中的 "GBLayerName"、"GBLayerNameAlias" 和 "GJBLayerList" 等信息，并存储在一个临时的 `GB2GJBLayer_t` 结构体中。
    5. 如果存在 "GJBLayerList"，则进一步提取其下的 "GJBLayer" 节点信息，并存储在 `GB2GJBGJBLayerList_t` 结构体中。
    6. 如果存在 "Codes"，则进一步提取其中的 "GBCode"、"GJBCode" 和 "ConvertFlag" 等信息，并存储在 `GB2GJBCodes_t` 结构体中。
    7. 将所有提取的信息汇总到 `GB2GJBLayer_t` 结构体，并添加到 `GB2GJBLayer_List.vLayer` 列表中。
    8. 继续执行步骤 3-7，直到处理完所有 "Layer" 节点。
  */
  //	构建GB2GJB配置文件的绝对路径
  // 定义一个字符数组来存储路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);

  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/GBCode2GJBCode.xml");


  //	首先对XML配置文件进行检查
  //checkAndDebugXML(strFileName);


  //	利用QT的文件系统相关操作的类，将GB2GJB的配置文件打开
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    return;
  }

  //	利用QT的QDomDocument类将GB2GJB的XML配置文件读取到doc数据结构中，在后面将会利用doc对XML配置文件进行解析
  QDomDocument doc;
  if (!doc.setContent(&file))
  {
    file.close();
    return;
  }
  file.close();

  // 当XML配置文件成功读取到QDomDocument数据结构中，然后根据doc这个实例读取其根节点
  QDomElement root = doc.documentElement();

  // 读取第一个子节点（QDomNode节点）
  QDomNode node = root.firstChild();
  //	在这个实例中每一个node就是一个Layer
  while (!node.isNull())
  {
    // 获取当前节点元素名称
    QString tagName = node.toElement().tagName();
    // 对当前节点元素名称进行检查（如果当前节点元素名称为Layer的时候对其进行处理）
    if (tagName == "Layer")
    {
      //	创建单个临时的Layer实例用来存储对应XML中的数据
      GB2GJBLayer_t temp_Layer;

      //	获取当前Layer节点的所有子节点
      QDomNodeList nodeList = node.childNodes();
      //	循环对当前Layer节点所有子节点进行处理
      for (size_t i = 0; i < nodeList.size(); i++)
      {
        //	获取当前Layer节点所有子节点中的第i个子节点
        QDomNode nodeTemp = nodeList.at(i);
        //	将其转化成元素类型（node是更为通用的数据类型，但是为了得到更加具体的信息例如标签，文本内容则需要类型的转化）
        QDomElement elementTemp = nodeTemp.toElement();

        //	在GB2GJB配置文件中Layer元素涉及到三个子节点（GBLayerName、GBLayerNameAlias、GJBLayerList）
        if (elementTemp.tagName() == "GBLayerName")
        {
          //	如果标签是GBLayerName的子节点，则将其文本内容存放到gb2gjb_single_temp_layer数据结构中
          QByteArray qByte = elementTemp.text().toLocal8Bit();
          temp_Layer.GBLayerName = string(qByte);
        }
        else if (elementTemp.tagName() == "GBLayerNameAlias")
        {
          //	如果标签是GBLayerNameAlias的子节点，则将其文本内容存放到temp_Layer数据结构中
          QByteArray qByte = elementTemp.text().toLocal8Bit();
          temp_Layer.GBLayerNameAlias = string(qByte);
        }
        else if (elementTemp.tagName() == "GJBLayerList")
        {
          //	创建一个临时的GB2GJBGJBLayerList_t类型的数据结构
          GB2GJBGJBLayerList_t temp_GB2GJBGJBLayerList;

          //	如果标签是GJBLayerList的子节点，则将其子节点的内容循环放入gb2gjb_single_temp_layer数据结构中
          QDomNodeList GJBLayerList = nodeTemp.childNodes();
          for (size_t j = 0; j < GJBLayerList.size(); j++)
          {
            //	创建一个临时的GB2GJBGJBLayer_t类型的数据结构，用来存放读取到的GJBLayer数据
            GB2GJBGJBLayer_t temp_GJBLayer;

            //	获取GJBLayerList节点下的第j个子节点
            QDomNode GJBLayer = GJBLayerList.at(j);
            //	获取第j个子节点GJBLayer的所有子节点
            QDomNodeList GJBLayer_all_child_nodes = GJBLayer.childNodes();
            //	循环处理第j个子节点GJBLayer的所有子节点
            for (size_t k = 0; k < GJBLayer_all_child_nodes.size(); k++)
            {
              //	获取第j个子节点GJBLayer的所有子节点中的第k个子节点
              QDomNode GJBLayer_child_node = GJBLayer_all_child_nodes.at(k);
              //	将当前第j个子节点GJBLayer的所有子节点中的第k个子节点转化成元素类型
              QDomElement GJBLayer_child_element = GJBLayer_child_node.toElement();
              if (GJBLayer_child_element.tagName() == "GJBLayerName")
              {
                //	如果标签是GJBLayerName的子节点，则将其文本内容存放到gb2gjb_single_temp_layer数据结构中
                QByteArray qByte = GJBLayer_child_element.text().toLocal8Bit();
                temp_GJBLayer.GJBLayerName = string(qByte);
              }
              else if (GJBLayer_child_element.tagName() == "GJBLayerNameAlias")
              {
                //	如果标签是GJBLayerNameAlias的子节点，则将其文本内容存放到gb2gjb_single_temp_layer数据结构中
                QByteArray qByte = GJBLayer_child_element.text().toLocal8Bit();
                temp_GJBLayer.GJBLayerNameAlias = string(qByte);
              }
              else if (GJBLayer_child_element.tagName() == "Codes")
              {
                //	创建一个临时的GB2GJBCodes_t类型数据类型用来存放Codes内容
                GB2GJBCodes_t temp_GB2GJBCodes;
                //	如果标签是Codes的子节点，则将其内容循环读取存放到gb2gjb_single_temp_layer数据结构中
                //	首先获取Codes节点下的所有子节点
                QDomNodeList Codes_all_child_nodes = GJBLayer_child_node.childNodes();
                //	循环对Codes节点下的所有子节点进行处理
                for (size_t l = 0; l < Codes_all_child_nodes.size(); l++)
                {
                  //	创建一个GB2GJBCode_t结构体
                  GB2GJBCode_t tempGB2GJBCode;
                  //	获取Codes节点下的所有子节点的第l个子节点（Code节点涉及到三个子节点：GBCode、GJBCode、ConvertFlag）
                  QDomNode Codes_child_node = Codes_all_child_nodes.at(l);
                  //	将当前Codes子节点转化成元素类型
                  QDomElement Codes_child_element = Codes_child_node.toElement();
                  //	判断当前Codes子节点的标签是否为Code
                  if (Codes_child_element.tagName() == "Code")
                  {
                    //	获取当前节点Code的所有子节点
                    QDomNodeList Code_all_child_nodes = Codes_child_node.childNodes();
                    //	循环处理Code的每一个子节点
                    for (size_t m = 0; m < Code_all_child_nodes.size(); m++)
                    {
                      //	获取Code所有节点的第m个节点
                      QDomNode Code_child_node = Code_all_child_nodes.at(m);
                      //	将其转化成元素类型
                      QDomElement Code_child_element = Code_child_node.toElement();
                      if (Code_child_element.tagName() == "SQL")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempGB2GJBCode.SQL = string(qByte);
                        string name = string(qByte);
                        std::cout << name << std::endl;
                      }
                      else if (Code_child_element.tagName() == "GJBCode")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempGB2GJBCode.GJBCode = string(qByte);

                      }
                      else if (Code_child_element.tagName() == "ConvertFlag")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempGB2GJBCode.ConvertFlag = string(qByte);
                      }
                    }
                    //	将获取到的数据存放到上一层的数据结构中
                    temp_GB2GJBCodes.vCode.push_back(tempGB2GJBCode);

                  }
                }
                //	将Codes数据结构中的内容存放到上一层中的数据结构中
                temp_GJBLayer.Codes = temp_GB2GJBCodes;
              }
            }

            //	将读取到的GJBLayer存放到GJBLayerList数据结构中
            temp_GB2GJBGJBLayerList.vGJBLayer.push_back(temp_GJBLayer);
          }

          //	将读取到的temp_GB2GJBGJBLayerList数据存放到temp_Layer数据结构中
          temp_Layer.GJBLayerList = temp_GB2GJBGJBLayerList;
        }
      }

      GB2GJBLayer_List.vLayer.push_back(temp_Layer);
    }

    //	切换到下一个子节点
    node = node.nextSibling();//读取下一个兄弟节点
  }

}

// JB到GJB编码配置文件
void convert_multi_datasrc2gjb::LoadDLHJConfig_JBCode2GJBCode(JB2GJBLayer_List_t& JB2GJBLayer_List)
{
  /*
  函数功能:
    函数主要用于从 `JBCode2GJBCode.xml` 文件中加载军标（JB）到国军标（GJB）的编码映射信息。
  这些信息被解析并存储在一个自定义的数据结构 `JB2GJBLayer_List_t` 中，以便后续的数据转换操作。

  算法流程:
    1. 构建 XML 文件的完整路径。
    2. 打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中。
    3. 遍历 XML 文档的根节点下的所有子节点（标记为 "Layer"）。
    4. 对于每个 "Layer" 节点，提取其中的 "JBLayerName"、"JBLayerNameAlias" 和 "GJBLayerList" 等信息，并存储在一个临时的 `JB2GJBLayer_t` 结构体中。
    5. 如果存在 "GJBLayerList"，则进一步提取其下的 "GJBLayer" 节点信息，并存储在 `JB2GJBGJBLayerList_t` 结构体中。
    6. 如果存在 "Codes"，则进一步提取其中的 "SQL"、"GJBCode" 和 "ConvertFlag" 等信息，并存储在 `JB2GJBCodes_t` 结构体中。
    7. 将所有提取的信息汇总到 `JB2GJBLayer_t` 结构体，并添加到 `JB2GJBLayer_List.vLayer` 列表中。
    8. 继续执行步骤 3-7，直到处理完所有 "Layer" 节点。
  */
  //	构建JB2GJB配置文件的绝对路径
  // 定义一个字符数组来存储路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);

  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/JBCode2GJBCode.xml");


  //	首先对XML配置文件进行检查
  //checkAndDebugXML(strFileName);

  //	利用QT的文件系统相关操作的类，将JB2GJB的配置文件打开
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    return;
  }

  //	利用QT的QDomDocument类将JB2GJB的XML配置文件读取到doc数据结构中，在后面将会利用doc对XML配置文件进行解析
  QDomDocument doc;
  if (!doc.setContent(&file))
  {
    file.close();
    return;
  }
  file.close();

  // 当XML配置文件成功读取到QDomDocument数据结构中，然后根据doc这个实例读取其根节点
  QDomElement root = doc.documentElement();

  // 读取第一个子节点（QDomNode节点）
  QDomNode node = root.firstChild();
  //	在这个实例中每一个node就是一个Layer
  while (!node.isNull())
  {
    // 获取当前节点元素名称
    QString tagName = node.toElement().tagName();
    // 对当前节点元素名称进行检查（如果当前节点元素名称为Layer的时候对其进行处理）
    if (tagName == "Layer")
    {
      //	创建单个临时的Layer实例用来存储对应XML中的数据
      JB2GJBLayer_t temp_Layer;

      //	获取当前Layer节点的所有子节点
      QDomNodeList nodeList = node.childNodes();
      //	循环对当前Layer节点所有子节点进行处理
      for (size_t i = 0; i < nodeList.size(); i++)
      {
        //	获取当前Layer节点所有子节点中的第i个子节点
        QDomNode nodeTemp = nodeList.at(i);
        //	将其转化成元素类型（node是更为通用的数据类型，但是为了得到更加具体的信息例如标签，文本内容则需要类型的转化）
        QDomElement elementTemp = nodeTemp.toElement();

        //	在GB2GJB配置文件中Layer元素涉及到三个子节点（JBLayerName、JBLayerNameAlias、GJBLayerList）
        if (elementTemp.tagName() == "JBLayerName")
        {
          //	如果标签是GBLayerName的子节点，则将其文本内容存放到gb2gjb_single_temp_layer数据结构中
          QByteArray qByte = elementTemp.text().toLocal8Bit();
          temp_Layer.JBLayerName = string(qByte);
        }
        else if (elementTemp.tagName() == "JBLayerNameAlias")
        {
          //	如果标签是GBLayerNameAlias的子节点，则将其文本内容存放到gb2gjb_single_temp_layer数据结构中
          QByteArray qByte = elementTemp.text().toLocal8Bit();
          temp_Layer.JBLayerNameAlias = string(qByte);
        }
        else if (elementTemp.tagName() == "GJBLayerList")
        {
          //	创建一个临时的JB2GJBGJBLayerList_t类型的数据结构
          JB2GJBGJBLayerList_t temp_JB2GJBGJBLayerList;

          //	如果标签是GJBLayerList的子节点，则将其子节点的内容循环放入temp_Layer数据结构中
          QDomNodeList GJBLayerList = nodeTemp.childNodes();
          for (size_t j = 0; j < GJBLayerList.size(); j++)
          {
            //	创建一个临时的GB2GJBGJBLayer_t类型的数据结构，用来存放读取到的GJBLayer数据
            JB2GJBGJBLayer_t temp_GJBLayer;

            //	获取GJBLayerList节点下的第j个子节点
            QDomNode GJBLayer = GJBLayerList.at(j);
            //	获取第j个子节点GJBLayer的所有子节点
            QDomNodeList GJBLayer_all_child_nodes = GJBLayer.childNodes();
            //	循环处理第j个子节点GJBLayer的所有子节点
            for (size_t k = 0; k < GJBLayer_all_child_nodes.size(); k++)
            {
              //	获取第j个子节点GJBLayer的所有子节点中的第k个子节点
              QDomNode GJBLayer_child_node = GJBLayer_all_child_nodes.at(k);
              //	将当前第j个子节点GJBLayer的所有子节点中的第k个子节点转化成元素类型
              QDomElement GJBLayer_child_element = GJBLayer_child_node.toElement();
              if (GJBLayer_child_element.tagName() == "GJBLayerName")
              {
                //	如果标签是GJBLayerName的子节点，则将其文本内容存放到gb2gjb_single_temp_layer数据结构中
                QByteArray qByte = GJBLayer_child_element.text().toLocal8Bit();
                temp_GJBLayer.GJBLayerName = string(qByte);
              }
              else if (GJBLayer_child_element.tagName() == "GJBLayerNameAlias")
              {
                //	如果标签是GJBLayerNameAlias的子节点，则将其文本内容存放到gb2gjb_single_temp_layer数据结构中
                QByteArray qByte = GJBLayer_child_element.text().toLocal8Bit();
                temp_GJBLayer.GJBLayerNameAlias = string(qByte);
              }
              else if (GJBLayer_child_element.tagName() == "Codes")
              {
                //	创建一个临时的GB2GJBCodes_t类型数据类型用来存放Codes内容
                JB2GJBCodes_t temp_JB2GJBCodes;
                //	如果标签是Codes的子节点，
                //	首先获取Codes节点下的所有子节点
                QDomNodeList Codes_all_child_nodes = GJBLayer_child_node.childNodes();
                //	循环对Codes节点下的所有子节点进行处理
                for (size_t l = 0; l < Codes_all_child_nodes.size(); l++)
                {
                  //	创建一个JB2GJBCode_t结构体
                  JB2GJBCode_t tempJB2GJBCode;
                  //	获取Codes节点下的所有子节点的第l个子节点（Code节点涉及到三个子节点：GBCode、GJBCode、ConvertFlag）
                  QDomNode Codes_child_node = Codes_all_child_nodes.at(l);
                  //	将当前Codes子节点转化成元素类型
                  QDomElement Codes_child_element = Codes_child_node.toElement();
                  //	判断当前Codes子节点的标签是否为Code
                  if (Codes_child_element.tagName() == "Code")
                  {
                    //	获取当前节点Code的所有子节点
                    QDomNodeList Code_all_child_nodes = Codes_child_node.childNodes();
                    //	循环处理Code的每一个子节点
                    for (size_t m = 0; m < Code_all_child_nodes.size(); m++)
                    {
                      //	获取Code所有节点的第m个节点
                      QDomNode Code_child_node = Code_all_child_nodes.at(m);
                      //	将其转化成元素类型
                      QDomElement Code_child_element = Code_child_node.toElement();
                      if (Code_child_element.tagName() == "SQL")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempJB2GJBCode.SQL = string(qByte);
                        string name = string(qByte);
                        std::cout << name << std::endl;
                      }
                      else if (Code_child_element.tagName() == "GJBCode")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempJB2GJBCode.GJBCode = string(qByte);

                      }
                      else if (Code_child_element.tagName() == "ConvertFlag")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempJB2GJBCode.ConvertFlag = string(qByte);
                      }
                    }
                    //	将获取到的数据存放到上一层的数据结构中
                    temp_JB2GJBCodes.vCode.push_back(tempJB2GJBCode);

                  }
                }
                //	将Codes数据结构中的内容存放到上一层中的数据结构中
                temp_GJBLayer.Codes = temp_JB2GJBCodes;
              }
            }

            //	将读取到的GJBLayer存放到GJBLayerList数据结构中
            temp_JB2GJBGJBLayerList.vGJBLayer.push_back(temp_GJBLayer);
          }

          //	将读取到的temp_GB2GJBGJBLayerList数据存放到temp_Layer数据结构中
          temp_Layer.GJBLayerList = temp_JB2GJBGJBLayerList;
        }
      }

      JB2GJBLayer_List.vLayer.push_back(temp_Layer);
    }

    //	切换到下一个子节点
    node = node.nextSibling();//读取下一个兄弟节点
  }

}

// DH到GJB编码配置文件
void convert_multi_datasrc2gjb::LoadDLHJConfig_DHCode2GJBCode(DH2GJBLayer_List_t& DH2GJBLayer_List)
{
  /*
  函数功能：
    函数主要用于从 `DHCode2GJBCode.xml` 文件中加载国家标准（DH）到国军标（GJB）的编码映射信息。
  这些信息被解析并存储在一个自定义的数据结构 `DH2GJBLayer_List_t` 中，以便于后续的数据转换操作。

  算法流程：
    1. 构建 XML 文件的完整路径。
    2. 打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中。
    3. 遍历 XML 文档的根节点下的所有子节点（标记为 "Layer"）。
    4. 对于每个 "Layer" 节点，提取其中的 "DHLayerName"、"DHLayerNameAlias" 和 "GJBLayerList" 等信息，并存储在一个临时的 `DH2GJBLayer_t` 结构体中。
    5. 如果存在 "GJBLayerList"，则进一步提取其下的 "GJBLayer" 节点信息，并存储在 `DH2GJBGJBLayerList_t` 结构体中。
    6. 如果存在 "Codes"，则进一步提取其中的 "DHCode"、"GJBCode" 和 "ConvertFlag" 等信息，并存储在 `DH2GJBCodes_t` 结构体中。
    7. 将所有提取的信息汇总到 `DH2GJBLayer_t` 结构体，并添加到 `DH2GJBLayer_List.vLayer` 列表中。
    8. 继续执行步骤 3-7，直到处理完所有 "Layer" 节点。
  */
  //	构建DH2GJB配置文件的绝对路径
  // 定义一个字符数组来存储路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);

  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/DHCode2GJBCode.xml");


  //	首先对XML配置文件进行检查
  //checkAndDebugXML(strFileName);


  //	利用QT的文件系统相关操作的类，将DH2GJB的配置文件打开
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    return;
  }

  //	利用QT的QDomDocument类将DH2GJB的XML配置文件读取到doc数据结构中，在后面将会利用doc对XML配置文件进行解析
  QDomDocument doc;
  if (!doc.setContent(&file))
  {
    file.close();
    return;
  }
  file.close();

  // 当XML配置文件成功读取到QDomDocument数据结构中，然后根据doc这个实例读取其根节点
  QDomElement root = doc.documentElement();

  // 读取第一个子节点（QDomNode节点）
  QDomNode node = root.firstChild();
  //	在这个实例中每一个node就是一个Layer
  while (!node.isNull())
  {
    // 获取当前节点元素名称
    QString tagName = node.toElement().tagName();
    // 对当前节点元素名称进行检查（如果当前节点元素名称为Layer的时候对其进行处理）
    if (tagName == "Layer")
    {
      //	创建单个临时的Layer实例用来存储对应XML中的数据
      DH2GJBLayer_t temp_Layer;

      //	获取当前Layer节点的所有子节点
      QDomNodeList nodeList = node.childNodes();
      //	循环对当前Layer节点所有子节点进行处理
      for (size_t i = 0; i < nodeList.size(); i++)
      {
        //	获取当前Layer节点所有子节点中的第i个子节点
        QDomNode nodeTemp = nodeList.at(i);
        //	将其转化成元素类型（node是更为通用的数据类型，但是为了得到更加具体的信息例如标签，文本内容则需要类型的转化）
        QDomElement elementTemp = nodeTemp.toElement();

        //	在DH2GJB配置文件中Layer元素涉及到三个子节点（DHLayerName、DHLayerNameAlias、GJBLayerList）
        if (elementTemp.tagName() == "DHLayerName")
        {
          //	如果标签是DHLayerName的子节点，则将其文本内容存放到DH2gjb_single_temp_layer数据结构中
          QByteArray qByte = elementTemp.text().toLocal8Bit();
          temp_Layer.DHLayerName = string(qByte);
        }
        else if (elementTemp.tagName() == "DHLayerNameAlias")
        {
          //	如果标签是DHLayerNameAlias的子节点，则将其文本内容存放到temp_Layer数据结构中
          QByteArray qByte = elementTemp.text().toLocal8Bit();
          temp_Layer.DHLayerNameAlias = string(qByte);
        }
        else if (elementTemp.tagName() == "GJBLayerList")
        {
          //	创建一个临时的DH2GJBGJBLayerList_t类型的数据结构
          DH2GJBGJBLayerList_t temp_DH2GJBGJBLayerList;

          //	如果标签是GJBLayerList的子节点，则将其子节点的内容循环放入DH2gjb_single_temp_layer数据结构中
          QDomNodeList GJBLayerList = nodeTemp.childNodes();
          for (size_t j = 0; j < GJBLayerList.size(); j++)
          {
            //	创建一个临时的DH2GJBGJBLayer_t类型的数据结构，用来存放读取到的GJBLayer数据
            DH2GJBGJBLayer_t temp_GJBLayer;

            //	获取GJBLayerList节点下的第j个子节点
            QDomNode GJBLayer = GJBLayerList.at(j);
            //	获取第j个子节点GJBLayer的所有子节点
            QDomNodeList GJBLayer_all_child_nodes = GJBLayer.childNodes();
            //	循环处理第j个子节点GJBLayer的所有子节点
            for (size_t k = 0; k < GJBLayer_all_child_nodes.size(); k++)
            {
              //	获取第j个子节点GJBLayer的所有子节点中的第k个子节点
              QDomNode GJBLayer_child_node = GJBLayer_all_child_nodes.at(k);
              //	将当前第j个子节点GJBLayer的所有子节点中的第k个子节点转化成元素类型
              QDomElement GJBLayer_child_element = GJBLayer_child_node.toElement();
              if (GJBLayer_child_element.tagName() == "GJBLayerName")
              {
                //	如果标签是GJBLayerName的子节点，则将其文本内容存放到DH2gjb_single_temp_layer数据结构中
                QByteArray qByte = GJBLayer_child_element.text().toLocal8Bit();
                temp_GJBLayer.GJBLayerName = string(qByte);
              }
              else if (GJBLayer_child_element.tagName() == "GJBLayerNameAlias")
              {
                //	如果标签是GJBLayerNameAlias的子节点，则将其文本内容存放到DH2gjb_single_temp_layer数据结构中
                QByteArray qByte = GJBLayer_child_element.text().toLocal8Bit();
                temp_GJBLayer.GJBLayerNameAlias = string(qByte);
              }
              else if (GJBLayer_child_element.tagName() == "Codes")
              {
                //	创建一个临时的DH2GJBCodes_t类型数据类型用来存放Codes内容
                DH2GJBCodes_t temp_DH2GJBCodes;
                //	如果标签是Codes的子节点，则将其内容循环读取存放到DH2gjb_single_temp_layer数据结构中
                //	首先获取Codes节点下的所有子节点
                QDomNodeList Codes_all_child_nodes = GJBLayer_child_node.childNodes();
                //	循环对Codes节点下的所有子节点进行处理
                for (size_t l = 0; l < Codes_all_child_nodes.size(); l++)
                {
                  //	创建一个DH2GJBCode_t结构体
                  DH2GJBCode_t tempDH2GJBCode;
                  //	获取Codes节点下的所有子节点的第l个子节点（Code节点涉及到三个子节点：DHCode、GJBCode、ConvertFlag）
                  QDomNode Codes_child_node = Codes_all_child_nodes.at(l);
                  //	将当前Codes子节点转化成元素类型
                  QDomElement Codes_child_element = Codes_child_node.toElement();
                  //	判断当前Codes子节点的标签是否为Code
                  if (Codes_child_element.tagName() == "Code")
                  {
                    //	获取当前节点Code的所有子节点
                    QDomNodeList Code_all_child_nodes = Codes_child_node.childNodes();
                    //	循环处理Code的每一个子节点
                    for (size_t m = 0; m < Code_all_child_nodes.size(); m++)
                    {
                      //	获取Code所有节点的第m个节点
                      QDomNode Code_child_node = Code_all_child_nodes.at(m);
                      //	将其转化成元素类型
                      QDomElement Code_child_element = Code_child_node.toElement();
                      if (Code_child_element.tagName() == "SQL")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempDH2GJBCode.SQL = string(qByte);
                        string name = string(qByte);
                        std::cout << name << std::endl;
                      }
                      else if (Code_child_element.tagName() == "GJBCode")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempDH2GJBCode.GJBCode = string(qByte);

                      }
                      else if (Code_child_element.tagName() == "ConvertFlag")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempDH2GJBCode.ConvertFlag = string(qByte);
                      }
                    }
                    //	将获取到的数据存放到上一层的数据结构中
                    temp_DH2GJBCodes.vCode.push_back(tempDH2GJBCode);

                  }
                }
                //	将Codes数据结构中的内容存放到上一层中的数据结构中
                temp_GJBLayer.Codes = temp_DH2GJBCodes;
              }
            }

            //	将读取到的GJBLayer存放到GJBLayerList数据结构中
            temp_DH2GJBGJBLayerList.vGJBLayer.push_back(temp_GJBLayer);
          }

          //	将读取到的temp_DH2GJBGJBLayerList数据存放到temp_Layer数据结构中
          temp_Layer.GJBLayerList = temp_DH2GJBGJBLayerList;
        }
      }

      DH2GJBLayer_List.vLayer.push_back(temp_Layer);
    }

    //	切换到下一个子节点
    node = node.nextSibling();//读取下一个兄弟节点
  }

}

// OSM到GJB编码配置文件
void convert_multi_datasrc2gjb::LoadDLHJConfig_OSMCode2GJBCode(OSM2GJBLayer_List_t& OSM2GJBLayer_List)
{
  /*
  函数功能：
    函数主要用于从 `OSMCode2GJBCode.xml` 文件中加载国家标准（OSM）到国军标（GJB）的编码映射信息。
  这些信息被解析并存储在一个自定义的数据结构 `OSM2GJBLayer_List_t` 中，以便于后续的数据转换操作。

  算法流程：
    1. 构建 XML 文件的完整路径。
    2. 打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中。
    3. 遍历 XML 文档的根节点下的所有子节点（标记为 "Layer"）。
    4. 对于每个 "Layer" 节点，提取其中的 "OSMLayerName"、"OSMLayerNameAlias" 和 "GJBLayerList" 等信息，并存储在一个临时的 `OSM2GJBLayer_t` 结构体中。
    5. 如果存在 "GJBLayerList"，则进一步提取其下的 "GJBLayer" 节点信息，并存储在 `OSM2GJBGJBLayerList_t` 结构体中。
    6. 如果存在 "Codes"，则进一步提取其中的 "OSMCode"、"GJBCode" 和 "ConvertFlag" 等信息，并存储在 `OSM2GJBCodes_t` 结构体中。
    7. 将所有提取的信息汇总到 `OSM2GJBLayer_t` 结构体，并添加到 `OSM2GJBLayer_List.vLayer` 列表中。
    8. 继续执行步骤 3-7，直到处理完所有 "Layer" 节点。
  */
  //	构建OSM2GJB配置文件的绝对路径
  // 定义一个字符数组来存储路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);

  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/OSMCode2GJBCode.xml");


  //	首先对XML配置文件进行检查
  //checkAndDebugXML(strFileName);


  //	利用QT的文件系统相关操作的类，将OSM2GJB的配置文件打开
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    return;
  }

  //	利用QT的QDomDocument类将OSM2GJB的XML配置文件读取到doc数据结构中，在后面将会利用doc对XML配置文件进行解析
  QDomDocument doc;
  if (!doc.setContent(&file))
  {
    file.close();
    return;
  }
  file.close();

  // 当XML配置文件成功读取到QDomDocument数据结构中，然后根据doc这个实例读取其根节点
  QDomElement root = doc.documentElement();

  // 读取第一个子节点（QDomNode节点）
  QDomNode node = root.firstChild();
  //	在这个实例中每一个node就是一个Layer
  while (!node.isNull())
  {
    // 获取当前节点元素名称
    QString tagName = node.toElement().tagName();
    // 对当前节点元素名称进行检查（如果当前节点元素名称为Layer的时候对其进行处理）
    if (tagName == "Layer")
    {
      //	创建单个临时的Layer实例用来存储对应XML中的数据
      OSM2GJBLayer_t temp_Layer;

      //	获取当前Layer节点的所有子节点
      QDomNodeList nodeList = node.childNodes();
      //	循环对当前Layer节点所有子节点进行处理
      for (size_t i = 0; i < nodeList.size(); i++)
      {
        //	获取当前Layer节点所有子节点中的第i个子节点
        QDomNode nodeTemp = nodeList.at(i);
        //	将其转化成元素类型（node是更为通用的数据类型，但是为了得到更加具体的信息例如标签，文本内容则需要类型的转化）
        QDomElement elementTemp = nodeTemp.toElement();

        //	在OSM2GJB配置文件中Layer元素涉及到三个子节点（OSMLayerName、OSMLayerNameAlias、GJBLayerList）
        if (elementTemp.tagName() == "OSMLayerName")
        {
          //	如果标签是OSMLayerName的子节点，则将其文本内容存放到OSM2gjb_single_temp_layer数据结构中
          QByteArray qByte = elementTemp.text().toLocal8Bit();
          temp_Layer.OSMLayerName = string(qByte);
        }
        else if (elementTemp.tagName() == "OSMLayerNameAlias")
        {
          //	如果标签是OSMLayerNameAlias的子节点，则将其文本内容存放到temp_Layer数据结构中
          QByteArray qByte = elementTemp.text().toLocal8Bit();
          temp_Layer.OSMLayerNameAlias = string(qByte);
        }
        else if (elementTemp.tagName() == "GJBLayerList")
        {
          //	创建一个临时的OSM2GJBGJBLayerList_t类型的数据结构
          OSM2GJBGJBLayerList_t temp_OSM2GJBGJBLayerList;

          //	如果标签是GJBLayerList的子节点，则将其子节点的内容循环放入OSM2gjb_single_temp_layer数据结构中
          QDomNodeList GJBLayerList = nodeTemp.childNodes();
          for (size_t j = 0; j < GJBLayerList.size(); j++)
          {
            //	创建一个临时的OSM2GJBGJBLayer_t类型的数据结构，用来存放读取到的GJBLayer数据
            OSM2GJBGJBLayer_t temp_GJBLayer;

            //	获取GJBLayerList节点下的第j个子节点
            QDomNode GJBLayer = GJBLayerList.at(j);
            //	获取第j个子节点GJBLayer的所有子节点
            QDomNodeList GJBLayer_all_child_nodes = GJBLayer.childNodes();
            //	循环处理第j个子节点GJBLayer的所有子节点
            for (size_t k = 0; k < GJBLayer_all_child_nodes.size(); k++)
            {
              //	获取第j个子节点GJBLayer的所有子节点中的第k个子节点
              QDomNode GJBLayer_child_node = GJBLayer_all_child_nodes.at(k);
              //	将当前第j个子节点GJBLayer的所有子节点中的第k个子节点转化成元素类型
              QDomElement GJBLayer_child_element = GJBLayer_child_node.toElement();
              if (GJBLayer_child_element.tagName() == "GJBLayerName")
              {
                //	如果标签是GJBLayerName的子节点，则将其文本内容存放到OSM2gjb_single_temp_layer数据结构中
                QByteArray qByte = GJBLayer_child_element.text().toLocal8Bit();
                temp_GJBLayer.GJBLayerName = string(qByte);
              }
              else if (GJBLayer_child_element.tagName() == "GJBLayerNameAlias")
              {
                //	如果标签是GJBLayerNameAlias的子节点，则将其文本内容存放到OSM2gjb_single_temp_layer数据结构中
                QByteArray qByte = GJBLayer_child_element.text().toLocal8Bit();
                temp_GJBLayer.GJBLayerNameAlias = string(qByte);
              }
              else if (GJBLayer_child_element.tagName() == "Codes")
              {
                //	创建一个临时的OSM2GJBCodes_t类型数据类型用来存放Codes内容
                OSM2GJBCodes_t temp_OSM2GJBCodes;
                //	如果标签是Codes的子节点，则将其内容循环读取存放到OSM2gjb_single_temp_layer数据结构中
                //	首先获取Codes节点下的所有子节点
                QDomNodeList Codes_all_child_nodes = GJBLayer_child_node.childNodes();
                //	循环对Codes节点下的所有子节点进行处理
                for (size_t l = 0; l < Codes_all_child_nodes.size(); l++)
                {
                  //	创建一个OSM2GJBCode_t结构体
                  OSM2GJBCode_t tempOSM2GJBCode;
                  //	获取Codes节点下的所有子节点的第l个子节点（Code节点涉及到三个子节点：OSMCode、GJBCode、ConvertFlag）
                  QDomNode Codes_child_node = Codes_all_child_nodes.at(l);
                  //	将当前Codes子节点转化成元素类型
                  QDomElement Codes_child_element = Codes_child_node.toElement();
                  //	判断当前Codes子节点的标签是否为Code
                  if (Codes_child_element.tagName() == "Code")
                  {
                    //	获取当前节点Code的所有子节点
                    QDomNodeList Code_all_child_nodes = Codes_child_node.childNodes();
                    //	循环处理Code的每一个子节点
                    for (size_t m = 0; m < Code_all_child_nodes.size(); m++)
                    {
                      //	获取Code所有节点的第m个节点
                      QDomNode Code_child_node = Code_all_child_nodes.at(m);
                      //	将其转化成元素类型
                      QDomElement Code_child_element = Code_child_node.toElement();
                      if (Code_child_element.tagName() == "SQL")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempOSM2GJBCode.SQL = string(qByte);
                        string name = string(qByte);
                        std::cout << name << std::endl;
                      }
                      else if (Code_child_element.tagName() == "GJBCode")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempOSM2GJBCode.GJBCode = string(qByte);

                      }
                      else if (Code_child_element.tagName() == "ConvertFlag")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempOSM2GJBCode.ConvertFlag = string(qByte);
                      }
                    }
                    //	将获取到的数据存放到上一层的数据结构中
                    temp_OSM2GJBCodes.vCode.push_back(tempOSM2GJBCode);

                  }
                }
                //	将Codes数据结构中的内容存放到上一层中的数据结构中
                temp_GJBLayer.Codes = temp_OSM2GJBCodes;
              }
            }

            //	将读取到的GJBLayer存放到GJBLayerList数据结构中
            temp_OSM2GJBGJBLayerList.vGJBLayer.push_back(temp_GJBLayer);
          }

          //	将读取到的temp_OSM2GJBGJBLayerList数据存放到temp_Layer数据结构中
          temp_Layer.GJBLayerList = temp_OSM2GJBGJBLayerList;
        }
      }

      OSM2GJBLayer_List.vLayer.push_back(temp_Layer);
    }

    //	切换到下一个子节点
    node = node.nextSibling();//读取下一个兄弟节点
  }

}

// TDT到GJB编码配置文件
void convert_multi_datasrc2gjb::LoadDLHJConfig_TDTCode2GJBCode(TDT2GJBLayer_List_t& TDT2GJBLayer_List)
{
  /*
  函数功能：
    函数主要用于从 `TDTCode2GJBCode.xml` 文件中加载国家标准（TDT）到国军标（GJB）的编码映射信息。
  这些信息被解析并存储在一个自定义的数据结构 `TDT2GJBLayer_List_t` 中，以便于后续的数据转换操作。

  算法流程：
    1. 构建 XML 文件的完整路径。
    2. 打开 XML 文件，并将其内容加载到 `QDomDocument` 对象中。
    3. 遍历 XML 文档的根节点下的所有子节点（标记为 "Layer"）。
    4. 对于每个 "Layer" 节点，提取其中的 "TDTLayerName"、"TDTLayerNameAlias" 和 "GJBLayerList" 等信息，并存储在一个临时的 `TDT2GJBLayer_t` 结构体中。
    5. 如果存在 "GJBLayerList"，则进一步提取其下的 "GJBLayer" 节点信息，并存储在 `TDT2GJBGJBLayerList_t` 结构体中。
    6. 如果存在 "Codes"，则进一步提取其中的 "TDTCode"、"GJBCode" 和 "ConvertFlag" 等信息，并存储在 `TDT2GJBCodes_t` 结构体中。
    7. 将所有提取的信息汇总到 `TDT2GJBLayer_t` 结构体，并添加到 `TDT2GJBLayer_List.vLayer` 列表中。
    8. 继续执行步骤 3-7，直到处理完所有 "Layer" 节点。
  */
  //	构建TDT2GJB配置文件的绝对路径
  char path[MAX_PATH];
  if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
  {
    log_message::show_log_message(6);
    return;
  }

  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path = std::string(path);
  std::replace(DLHJ_TOOLS_convert_multi_datasrc2gjb_path.begin(), DLHJ_TOOLS_convert_multi_datasrc2gjb_path.end(), '\\', '/');
  size_t last_left_slash_position = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.find_last_of('/');
  std::string DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix = DLHJ_TOOLS_convert_multi_datasrc2gjb_path.substr(0, last_left_slash_position);

  QString strFileName = QString::fromStdString(DLHJ_TOOLS_convert_multi_datasrc2gjb_path_prefix + "/DLHJ_TOOLS_convert_multi_datasrc2gjb_configure_files/TDTCode2GJBCode.xml");

  //	首先对XML配置文件进行检查
  //checkAndDebugXML(strFileName);


  //	利用QT的文件系统相关操作的类，将TDT2GJB的配置文件打开
  QFile file(strFileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    return;
  }

  //	利用QT的QDomDocument类将TDT2GJB的XML配置文件读取到doc数据结构中，在后面将会利用doc对XML配置文件进行解析
  QDomDocument doc;
  if (!doc.setContent(&file))
  {
    file.close();
    return;
  }
  file.close();

  // 当XML配置文件成功读取到QDomDocument数据结构中，然后根据doc这个实例读取其根节点
  QDomElement root = doc.documentElement();

  // 读取第一个子节点（QDomNode节点）
  QDomNode node = root.firstChild();
  //	在这个实例中每一个node就是一个Layer
  while (!node.isNull())
  {
    // 获取当前节点元素名称
    QString tagName = node.toElement().tagName();
    // 对当前节点元素名称进行检查（如果当前节点元素名称为Layer的时候对其进行处理）
    if (tagName == "Layer")
    {
      //	创建单个临时的Layer实例用来存储对应XML中的数据
      TDT2GJBLayer_t temp_Layer;

      //	获取当前Layer节点的所有子节点
      QDomNodeList nodeList = node.childNodes();
      //	循环对当前Layer节点所有子节点进行处理
      for (size_t i = 0; i < nodeList.size(); i++)
      {
        //	获取当前Layer节点所有子节点中的第i个子节点
        QDomNode nodeTemp = nodeList.at(i);
        //	将其转化成元素类型（node是更为通用的数据类型，但是为了得到更加具体的信息例如标签，文本内容则需要类型的转化）
        QDomElement elementTemp = nodeTemp.toElement();

        //	在TDT2GJB配置文件中Layer元素涉及到三个子节点（TDTLayerName、TDTLayerNameAlias、GJBLayerList）
        if (elementTemp.tagName() == "TDTLayerName")
        {
          //	如果标签是TDTLayerName的子节点，则将其文本内容存放到TDT2gjb_single_temp_layer数据结构中
          QByteArray qByte = elementTemp.text().toLocal8Bit();
          temp_Layer.TDTLayerName = string(qByte);
        }
        else if (elementTemp.tagName() == "TDTLayerNameAlias")
        {
          //	如果标签是TDTLayerNameAlias的子节点，则将其文本内容存放到temp_Layer数据结构中
          QByteArray qByte = elementTemp.text().toLocal8Bit();
          temp_Layer.TDTLayerNameAlias = string(qByte);
        }
        else if (elementTemp.tagName() == "GJBLayerList")
        {
          //	创建一个临时的TDT2GJBGJBLayerList_t类型的数据结构
          TDT2GJBGJBLayerList_t temp_TDT2GJBGJBLayerList;

          //	如果标签是GJBLayerList的子节点，则将其子节点的内容循环放入TDT2gjb_single_temp_layer数据结构中
          QDomNodeList GJBLayerList = nodeTemp.childNodes();
          for (size_t j = 0; j < GJBLayerList.size(); j++)
          {
            //	创建一个临时的TDT2GJBGJBLayer_t类型的数据结构，用来存放读取到的GJBLayer数据
            TDT2GJBGJBLayer_t temp_GJBLayer;

            //	获取GJBLayerList节点下的第j个子节点
            QDomNode GJBLayer = GJBLayerList.at(j);
            //	获取第j个子节点GJBLayer的所有子节点
            QDomNodeList GJBLayer_all_child_nodes = GJBLayer.childNodes();
            //	循环处理第j个子节点GJBLayer的所有子节点
            for (size_t k = 0; k < GJBLayer_all_child_nodes.size(); k++)
            {
              //	获取第j个子节点GJBLayer的所有子节点中的第k个子节点
              QDomNode GJBLayer_child_node = GJBLayer_all_child_nodes.at(k);
              //	将当前第j个子节点GJBLayer的所有子节点中的第k个子节点转化成元素类型
              QDomElement GJBLayer_child_element = GJBLayer_child_node.toElement();
              if (GJBLayer_child_element.tagName() == "GJBLayerName")
              {
                //	如果标签是GJBLayerName的子节点，则将其文本内容存放到TDT2gjb_single_temp_layer数据结构中
                QByteArray qByte = GJBLayer_child_element.text().toLocal8Bit();
                temp_GJBLayer.GJBLayerName = string(qByte);
              }
              else if (GJBLayer_child_element.tagName() == "GJBLayerNameAlias")
              {
                //	如果标签是GJBLayerNameAlias的子节点，则将其文本内容存放到TDT2gjb_single_temp_layer数据结构中
                QByteArray qByte = GJBLayer_child_element.text().toLocal8Bit();
                temp_GJBLayer.GJBLayerNameAlias = string(qByte);
              }
              else if (GJBLayer_child_element.tagName() == "Codes")
              {
                //	创建一个临时的TDT2GJBCodes_t类型数据类型用来存放Codes内容
                TDT2GJBCodes_t temp_TDT2GJBCodes;
                //	如果标签是Codes的子节点，则将其内容循环读取存放到TDT2gjb_single_temp_layer数据结构中
                //	首先获取Codes节点下的所有子节点
                QDomNodeList Codes_all_child_nodes = GJBLayer_child_node.childNodes();
                //	循环对Codes节点下的所有子节点进行处理
                for (size_t l = 0; l < Codes_all_child_nodes.size(); l++)
                {
                  //	创建一个TDT2GJBCode_t结构体
                  TDT2GJBCode_t tempTDT2GJBCode;
                  //	获取Codes节点下的所有子节点的第l个子节点（Code节点涉及到三个子节点：TDTCode、GJBCode、ConvertFlag）
                  QDomNode Codes_child_node = Codes_all_child_nodes.at(l);
                  //	将当前Codes子节点转化成元素类型
                  QDomElement Codes_child_element = Codes_child_node.toElement();
                  //	判断当前Codes子节点的标签是否为Code
                  if (Codes_child_element.tagName() == "Code")
                  {
                    //	获取当前节点Code的所有子节点
                    QDomNodeList Code_all_child_nodes = Codes_child_node.childNodes();
                    //	循环处理Code的每一个子节点
                    for (size_t m = 0; m < Code_all_child_nodes.size(); m++)
                    {
                      //	获取Code所有节点的第m个节点
                      QDomNode Code_child_node = Code_all_child_nodes.at(m);
                      //	将其转化成元素类型
                      QDomElement Code_child_element = Code_child_node.toElement();
                      if (Code_child_element.tagName() == "SQL")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempTDT2GJBCode.SQL = string(qByte);
                        string name = string(qByte);
                        std::cout << name << std::endl;
                      }
                      else if (Code_child_element.tagName() == "GJBCode")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempTDT2GJBCode.GJBCode = string(qByte);

                      }
                      else if (Code_child_element.tagName() == "ConvertFlag")
                      {
                        QByteArray qByte = Code_child_element.text().toLocal8Bit();
                        tempTDT2GJBCode.ConvertFlag = string(qByte);
                      }
                    }
                    //	将获取到的数据存放到上一层的数据结构中
                    temp_TDT2GJBCodes.vCode.push_back(tempTDT2GJBCode);

                  }
                }
                //	将Codes数据结构中的内容存放到上一层中的数据结构中
                temp_GJBLayer.Codes = temp_TDT2GJBCodes;
              }
            }

            //	将读取到的GJBLayer存放到GJBLayerList数据结构中
            temp_TDT2GJBGJBLayerList.vGJBLayer.push_back(temp_GJBLayer);
          }

          //	将读取到的temp_TDT2GJBGJBLayerList数据存放到temp_Layer数据结构中
          temp_Layer.GJBLayerList = temp_TDT2GJBGJBLayerList;
        }
      }

      TDT2GJBLayer_List.vLayer.push_back(temp_Layer);
    }

    //	切换到下一个子节点
    node = node.nextSibling();//读取下一个兄弟节点
  }

}


//	从读取到的配置文件信息提取国标图层名称列表
void convert_multi_datasrc2gjb::ExtractGBlayerNameList(GBlayerLayer_List_t& GBlayerLayer_List, vector<string>& GBlayerLayer_List_name)
{
  for (size_t index = 0; index < GBlayerLayer_List.vLayer.size(); index++)
  {
    GBlayerLayer_List_name.push_back(GBlayerLayer_List.vLayer[index].LayerName);
  }
}

//	从读取到的配置文件信息提取军标图层名称列表
void convert_multi_datasrc2gjb::ExtractJBlayerNameList(JBlayerLayer_List_t& JBlayerLayer_List, vector<string>& JBlayerLayer_List_name)
{
  for (size_t index = 0; index < JBlayerLayer_List.vLayer.size(); index++)
  {
    JBlayerLayer_List_name.push_back(JBlayerLayer_List.vLayer[index].LayerName);
  }
}

//	从读取到的配置文件信息提取导航图层名称列表
void convert_multi_datasrc2gjb::ExtractDHlayerNameList(DHlayerLayer_List_t& DHlayerLayer_List, vector<string>& DHlayerLayer_List_name)
{
  for (size_t index = 0; index < DHlayerLayer_List.vLayer.size(); index++)
  {
    DHlayerLayer_List_name.push_back(DHlayerLayer_List.vLayer[index].LayerName);
  }
}

//	从读取到的配置文件信息提取OSM图层名称列表
void convert_multi_datasrc2gjb::ExtractOSMlayerNameList(OSMlayerLayer_List_t& OSMlayerLayer_List, vector<string>& OSMlayerLayer_List_name)
{
  for (size_t index = 0; index < OSMlayerLayer_List.vLayer.size(); index++)
  {
    OSMlayerLayer_List_name.push_back(OSMlayerLayer_List.vLayer[index].LayerName);
  }
}

//	从读取到的配置文件信息提取TDT图层名称列表
void convert_multi_datasrc2gjb::ExtractTDTlayerNameList(TDTlayerLayer_List_t& TDTlayerLayer_List, vector<string>& TDTlayerLayer_List_name)
{
  for (size_t index = 0; index < TDTlayerLayer_List.vLayer.size(); index++)
  {
    TDTlayerLayer_List_name.push_back(TDTlayerLayer_List.vLayer[index].LayerName);
  }
}




//	从读取到的配置文件信息提取国标图层名称列表
void convert_multi_datasrc2gjb::ExtractGBlayerNameList(
  const GBlayerLayer_List_t& GBlayerLayer_List,
  std::vector<std::string>& GBlayerLayer_List_name)
{
  for (size_t index = 0; index < GBlayerLayer_List.vLayer.size(); index++)
  {
    GBlayerLayer_List_name.push_back(GBlayerLayer_List.vLayer[index].LayerName);
  }
}


void convert_multi_datasrc2gjb::IsTheLayerNamingStandardized(
  const QStringList& qstrPathList,
  const QString qstrInputDataPath,
  const std::vector<std::string>& vNameList)
{
  //	如果是单个图幅
  if (qstrPathList.size() == 0)
  {
    // 获取图幅下所有shp文件列表
    QStringList qShpFileList = GetShpFileNames(qstrInputDataPath);
    // 如果shp文件不存在，提示
    if (qShpFileList.size() == 0)
    {
      //	输入的目录不存在
      // 将UTF-8字符串转换为GBK编码的字符串
      std::string utf8Str;
      std::wstring wStr;
      std::string gbkStr;

      utf8Str = "===============";
      wStr = UTF8ToWideChar(utf8Str);
      gbkStr = WideCharToGBK(wStr);
      std::cout << gbkStr << std::endl;

      utf8Str = "图幅（" + qstrInputDataPath.toStdString() + "）中无shp文件！";
      wStr = UTF8ToWideChar(utf8Str);
      gbkStr = WideCharToGBK(wStr);
      std::cout << gbkStr << std::endl;

      utf8Str = "===============";
      wStr = UTF8ToWideChar(utf8Str);
      gbkStr = WideCharToGBK(wStr);
      std::cout << gbkStr << std::endl;
      return;
    }

    for (int i = 0; i < qShpFileList.size(); i++)
    {
      string strShpFileName = qShpFileList[i].toLocal8Bit();

      // 只要有一个文件不合法，说明输入文件不符合要求，提示并退出操作
      if (!bIsExistedInLayerNameList(strShpFileName, vNameList))
      {
        //	输入的目录不存在
        // 将UTF-8字符串转换为GBK编码的字符串
        std::string utf8Str;
        std::wstring wStr;
        std::string gbkStr;

        utf8Str = "===============";
        wStr = UTF8ToWideChar(utf8Str);
        gbkStr = WideCharToGBK(wStr);
        std::cout << gbkStr << std::endl;

        utf8Str = "文件（" + strShpFileName + ")命名不符合要求！";
        wStr = UTF8ToWideChar(utf8Str);
        gbkStr = WideCharToGBK(wStr);
        std::cout << gbkStr << std::endl;

        utf8Str = "===============";
        wStr = UTF8ToWideChar(utf8Str);
        gbkStr = WideCharToGBK(wStr);
        std::cout << gbkStr << std::endl;
        continue;
      }
    }
  }
  //	如果是多个图幅
  else
  {
    for (int i = 0; i < qstrPathList.size(); i++)
    {
      // 判断每个文件夹下数据是否符合标准
      // 获取图幅下所有shp文件列表
      QStringList qShpFileList = GetShpFileNames(qstrPathList[i]);
      // 如果shp文件不存在，提示
      if (qShpFileList.size() == 0)
      {
        //	输入的目录不存在
        // 将UTF-8字符串转换为GBK编码的字符串
        std::string utf8Str;
        std::wstring wStr;
        std::string gbkStr;

        utf8Str = "===============";
        wStr = UTF8ToWideChar(utf8Str);
        gbkStr = WideCharToGBK(wStr);
        std::cout << gbkStr << std::endl;

        utf8Str = "图幅（" + qstrInputDataPath.toStdString() + "）中无shp文件！";
        wStr = UTF8ToWideChar(utf8Str);
        gbkStr = WideCharToGBK(wStr);
        std::cout << gbkStr << std::endl;

        utf8Str = "===============";
        wStr = UTF8ToWideChar(utf8Str);
        gbkStr = WideCharToGBK(wStr);
        std::cout << gbkStr << std::endl;
        return;
      }

      for (int i = 0; i < qShpFileList.size(); i++)
      {
        string strShpFileName = qShpFileList[i].toLocal8Bit();

        // 只要有一个文件不合法，说明输入文件不符合要求，提示并退出操作
        if (!bIsExistedInLayerNameList(strShpFileName, vNameList))
        {
          //	输入的目录不存在
          // 将UTF-8字符串转换为GBK编码的字符串
          std::string utf8Str;
          std::wstring wStr;
          std::string gbkStr;

          utf8Str = "===============";
          wStr = UTF8ToWideChar(utf8Str);
          gbkStr = WideCharToGBK(wStr);
          std::cout << gbkStr << std::endl;

          utf8Str = "文件（" + strShpFileName + ")命名不符合要求！";
          wStr = UTF8ToWideChar(utf8Str);
          gbkStr = WideCharToGBK(wStr);
          std::cout << gbkStr << std::endl;

          utf8Str = "===============";
          wStr = UTF8ToWideChar(utf8Str);
          gbkStr = WideCharToGBK(wStr);
          std::cout << gbkStr << std::endl;
          continue;
        }
      }
    }
  }

}


QStringList convert_multi_datasrc2gjb::GetShpFileNames(const QString& path)
{
  QDir dir(path);
  QStringList nameFilters;
  nameFilters << "*.shp" << "*.SHP";
  QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
  return files;
}


// 将UTF-8字符串转换为宽字符串
std::wstring convert_multi_datasrc2gjb::UTF8ToWideChar(const std::string& str)
{
  int num = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
  wchar_t* wide = new wchar_t[num];
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wide, num);
  std::wstring wstr(wide);
  delete[] wide;
  return wstr;
}

// 将宽字符串转换为GBK编码的字符串
std::string convert_multi_datasrc2gjb::WideCharToGBK(const std::wstring& wstr)
{
  int num = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
  char* gbk = new char[num];
  WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, gbk, num, NULL, NULL);
  std::string str(gbk);
  delete[] gbk;
  return str;
}


bool convert_multi_datasrc2gjb::bIsExistedInLayerNameList(string strLayerName, const vector<string>& vNameList)
{
  int iCount = 0;
  for (int i = 0; i < vNameList.size(); i++)
  {
    if (strstr(strLayerName.c_str(), vNameList[i].c_str()) != NULL)
    {
      iCount++;
    }
  }

  // 说明未找到
  if (iCount == 0)
  {
    return false;
  }
  else
  {
    return true;
  }

  return true;
}
