#include <vector>
#include <string>

#include <QString>
#include <QDomDocument>

#include "CSE_GeoExtractAndProcess.h"
#include "log_message.h"

class get_shp_data_from_gpkg
{
public:
  //  构造函数
  get_shp_data_from_gpkg(
  const int& InputDataType,
  const int& iScaleType,
  const bool& bOutputSheet,
  const std::string& xml_input_data_path,
  const std::string& input_data_path,
  const int& iOutputDataType,
  const std::string& output_data_path);

  ~get_shp_data_from_gpkg();

public:
  void perform_get_shp_data_from_gpkg();

private:
  void load_xml_configure_file();
  int perform_process_on_a_single_work_area();

  

private:
  int m_InputDataType;
  int m_iScaleType;
  bool m_bOutputSheet;
  std::string m_xml_input_data_path;
  std::string m_input_data_path;
  int m_iOutputDataType;
  std::string m_output_data_path;
  std::vector<ExtractDataParam> m_vExtractDataParam;
};

