#include <vector>
#include <string>
#include <windows.h>

#include <QString>
#include <QStringList>
#include <QDir>
#include <QDomDocument>

#include "CSE_GeoExtractAndProcess.h"
#include "log_message.h"

class convert_multi_datasrc2gjb
{
public:
  //  构造函数
  convert_multi_datasrc2gjb(
  const int& data_source_type,
  const std::string& input_data_path,
  const std::string& output_data_path);

  ~convert_multi_datasrc2gjb();

public:
  void perform_convert_multi_datasrc2gjb();

private:
  QStringList GetDirPathOfSplDir(const QString& dirPath);

  /*国军标图层配置文件*/
  void LoadDLHJConfig_GJBLayerField(GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List);

  /*国标图层配置文件*/
  void LoadDLHJConfig_GBLayer(GBlayerLayer_List_t& GBlayerLayer_List);

  /*军标图层配置文件*/
  void LoadDLHJConfig_JBLayer(JBlayerLayer_List_t& JBlayerLayer_List);

  /*导航图层配置文件*/
  void LoadDLHJConfig_DHLayer(DHlayerLayer_List_t& DHlayerLayer_List);

  /*OSM图层配置文件*/
  void LoadDLHJConfig_OSMLayer(OSMlayerLayer_List_t& OSMlayerLayer_List);

  /*TDT图层配置文件*/
  void LoadDLHJConfig_TDTLayer(TDTlayerLayer_List_t& TDTlayerLayer_List);



  // GB到GJB编码配置文件
  void LoadDLHJConfig_GBCode2GJBCode(GB2GJBLayer_List_t& GB2GJBLayer_List);

  // JB到GJB编码配置文件
  void LoadDLHJConfig_JBCode2GJBCode(JB2GJBLayer_List_t& JB2GJBLayer_List);

  // DH到GJB编码配置文件
  void LoadDLHJConfig_DHCode2GJBCode(DH2GJBLayer_List_t& DH2GJBLayer_List);

  // OSM到GJB编码配置文件
  void LoadDLHJConfig_OSMCode2GJBCode(OSM2GJBLayer_List_t& OSM2GJBLayer_List);

  // TDT到GJB编码配置文件
  void LoadDLHJConfig_TDTCode2GJBCode(TDT2GJBLayer_List_t& TDT2GJBLayer_List);

  //	从读取到的配置文件信息提取国标图层名称列表
  void ExtractGBlayerNameList(GBlayerLayer_List_t& GBlayerLayer_List, vector<string>& GBlayerLayer_List_name);

  //	从读取到的配置文件信息提取军标图层名称列表
  void ExtractJBlayerNameList(JBlayerLayer_List_t& JBlayerLayer_List, vector<string>& JBlayerLayer_List_name);

  //	从读取到的配置文件信息提取导航图层名称列表
  void ExtractDHlayerNameList(DHlayerLayer_List_t& DHlayerLayer_List, vector<string>& DHlayerLayer_List_name);

  //	从读取到的配置文件信息提取OSM图层名称列表
  void ExtractOSMlayerNameList(OSMlayerLayer_List_t& OSMlayerLayer_List, vector<string>& OSMlayerLayer_List_name);

  //	从读取到的配置文件信息提取TDT图层名称列表
  void ExtractTDTlayerNameList(TDTlayerLayer_List_t& TDTlayerLayer_List, vector<string>& TDTlayerLayer_List_name);


  //	从读取到的配置文件信息提取国标图层名称列表
  void ExtractGBlayerNameList(
    const GBlayerLayer_List_t& GBlayerLayer_List,
    std::vector<std::string>& GBlayerLayer_List_name);

  void IsTheLayerNamingStandardized(
    const QStringList& qstrPathList,
    const QString qstrInputDataPath,
    const std::vector<std::string>& vNameList);

  QStringList GetShpFileNames(const QString& path);


  // 将UTF-8字符串转换为宽字符串
  std::wstring UTF8ToWideChar(const std::string& str);

  // 将宽字符串转换为GBK编码的字符串
  std::string WideCharToGBK(const std::wstring& wstr);

  bool bIsExistedInLayerNameList(string strLayerName, const vector<string>& vNameList);

private:
  int m_data_source_type;
  std::string m_input_data_path;
  std::string m_output_data_path;
  
};

