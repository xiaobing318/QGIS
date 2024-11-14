#ifndef _SEMANTIC_FUSION_H
#define _SEMANTIC_FUSION_H
/*-------------STL-------------*/
#include <vector>
#include <string>

/*-------------QT-------------*/
#include <QDialog>
#include <QString>

/*-------------ui-------------*/
#include "ui_source_data_inspection_tool.h"

/*-------------基础算法库-------------*/

/*-------------数据结构头文件-------------*/
#include "source_data_inspection_tool_data_structure.h"


class qgs_source_data_inspection_tool : public QDialog
{
	Q_OBJECT

public:
	qgs_source_data_inspection_tool(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~qgs_source_data_inspection_tool() override;

#pragma region "源数据检查工具实际的逻辑处理函数"

  int S57_naming_correctness_check(
    const S57_fields4all_layers_t& S57_fields4all_layers,
    const S57_country_list_t& S57_country_list,
    const S57_Producer_number_List_t& S57_Producer_number_List,
    const S57_chart_cartographic_data_elements_layer_list_t& S57_chart_cartographic_data_elements_layer_list,
    const std::vector<std::string> v_input_frame_data_path,
    const std::string& str_log_file_output_path);

  int S57_single_framed_data_naming_correctness_check(
    const S57_fields4all_layers_t& S57_fields4all_layers,
    const S57_country_list_t& S57_country_list,
    const S57_Producer_number_List_t& S57_Producer_number_List,
    const S57_chart_cartographic_data_elements_layer_list_t& S57_chart_cartographic_data_elements_layer_list,
    const std::string& str_input_data_path_utf8,
    std::ofstream& file_stream);

  bool is_find(const std::string str, const S57_country_list_t& S57_country_list);

  bool is_valid(const std::string str, const S57_Producer_number_List_t& S57_Producer_number_List);

  std::string get_object_label_name(
    const S57_chart_cartographic_data_elements_layer_list_t& S57_chart_cartographic_data_elements_layer_list,
    const std::string& str_attribute_value);
#pragma endregion

private:
/**************************************************类成员对象set\get函数******************************************************/
	//  获取输入数据目录成员函数
	QString get_m_input_data_path();

	//  获取输出数据目录成员函数
	QString get_m_log_file_output_path();

  //  获取数据源类型
  int get_input_data_src_type();

  //  获取字符集编码
  std::string get_character_set_type();

  //  设置输入数据目录成员函数
  void set_m_input_data_path(const std::string& input_data_path);

  //  设置输出数据目录成员函数
  void set_m_log_file_output_path(const std::string& lineEdit_log_file_output_path);

  //  设置字符集编码
  void set_character_set_type(const QString& character_set_type);



  //  恢复用户上一次设置的一些参数
  void restore_states();

  //  保存用户设置的一些参数
  void store_states();
/**************************************************类成员对象set\get函数******************************************************/

/**************************************************类辅助成员函数******************************************************/
  //  获取指定目录中的多个子目录
  QStringList get_sub_directories(const QString& file_directory_path);

	// 判断文件目录是否存在
	bool is_exist_file_directory(const QString& qstr_file_path);

	// 判断文件是否存在
	bool is_exist_file(const QString& qstr_file_path);

  //	检查XML文件
  void checkAndDebugXML(const QString& strFileName);
  /**************************************************类辅助成员函数******************************************************/

/**************************************************读取配置文件相关的函数******************************************************/
  /*2024-04-03: S57属性表结构(附加了“标识号范围”内容)*/
  void Load_S57_fields_info4each_layer_config_file(S57_fields4all_layers_t& S57_fields4all_layers);

  /*2024-04-03: S57国家代码结构*/
  void Load_S57_country_code_config_file(S57_country_list_t& S57_country_list);

  /*2024-04-03: S57生产者编号结构*/
  void Load_S57_producer_number_config_file(S57_Producer_number_List_t& S57_Producer_number_List);

  /*2024-04-03: S57表1-3海图制图数据要素结构*/
  void Load_S57_chart_cartographic_data_elements_1_3(S57_chart_cartographic_data_elements_layer_list_t& S57_chart_cartographic_data_elements_layer_list);


/**************************************************读取配置文件相关的函数******************************************************/

private slots:
  void Button_input_data_view();
  void Button_log_file_output_data_view();
  void Button_OK();
  void Button_Cancel();
  void QComboBox_get_character_set_encoding();


private:

  Ui_qgs_source_data_inspection_tool ui;

	//  输入数据路径
	QString m_input_data_path;

	//  输出数据路径
	QString m_log_file_output_path;

  //  字符及编码
  QString m_character_set_type;

};

#endif // _SEMANTIC_FUSION_H
