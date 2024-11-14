#include <vector>
#include <string>

#include <QString>
#include <QDomDocument>

#include "CSE_GeoExtractAndProcess.h"
#include "log_message.h"

class automatic_edge_connection
{
public:
  //  构造函数
  automatic_edge_connection(
    const std::string& xml_configure_file_data_path,
    const std::string& gpkg_data_path,
    const int& iScaleType,
    const double& dDistance);

  ~automatic_edge_connection();

public:
  void perform_edge_joining_operations_on_a_single_work_area();

private:
  void load_merge_params();
  int auto_merge_on_a_single_work_area();

private:
  std::string m_xml_configure_file_data_path;
  std::string m_gpkg_data_path;
  // 接边参数— 比例尺
  int m_iScaleType;
  // 接边参数— 接边缓冲区距离
  double m_dDistance;

  //  用来存储从XML文件中读取的配置信息
  std::vector<LayerMatchParam> m_vLayerMatchParam;
  // 作业区gpkg数据库列表
  std::vector<std::string> m_vGpkgPath;
};

