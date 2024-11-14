#ifndef _SEMANTIC_FUSION_H
#define _SEMANTIC_FUSION_H
/*-------------STL-------------*/
#include <vector>
#include <string>

/*-------------QT-------------*/
#include <QDialog>
#include <QString>

/*-------------ui-------------*/
#include "ui_semantic_fusion.h"

/*-------------基础算法库-------------*/
#include "CSE_GeoExtractAndProcess.h"


class qgs_semantic_fusion : public QDialog
{
	Q_OBJECT

public:
	qgs_semantic_fusion(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~qgs_semantic_fusion() override;

private:
/**************************************************类成员对象set\get函数******************************************************/
	//  获取输入数据目录成员函数
	QString get_m_input_data_path();

	//  获取输出数据目录成员函数
	QString get_m_output_data_path();

  //  设置输入数据目录成员函数
  void set_m_input_data_path(const std::string& input_data_path);

  //  设置输出数据目录成员函数
  void set_m_output_data_path(const std::string& output_data_path);

	//  获取数据源类型
	int get_input_data_src_type();

  // 恢复保存参数
  void restore_states();
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
  /*2024-03-11:一体化属性表结构*/
  void Load_YTH_fields4all_layers_config_file(YTH_fields4all_layers_t& YTH_fields4all_layers);

  /*2023-03-11：YTH同JBDX图层映射表*/
  void Load_YTH_JBDX_layer_mapping_table(YTH_JBDX_layer_mapping_table_t& YTH_JBDX_layer_mapping_table);

  /*2023-03-11：YTH同JBDX编码映射表*/
  void Load_YTH_JBDX_code_mapping_table(YTH_JBDX_code_mapping_table_t& YTH_JBDX_code_mapping_table);

  /*2023-03-11：YTH同OSM图层映射表*/
  void Load_YTH_OSM_layer_mapping_table(YTH_OSM_layer_mapping_table_t& YTH_OSM_layer_mapping_table);

  /*2023-03-11：YTH同OSM编码映射表*/
  void Load_YTH_OSM_code_mapping_table(YTH_OSM_code_mapping_table_t& YTH_OSM_code_mapping_table);

  /*2023-03-11：YTH同S57图层映射表*/
  void Load_YTH_S57_layer_mapping_table(YTH_S57_layer_mapping_table_t& YTH_S57_layer_mapping_table);

  /*2023-03-11：YTH同S57编码映射表*/
  void Load_YTH_S57_code_mapping_table(YTH_S57_code_mapping_table_t& YTH_S57_code_mapping_table);

  /*2023-03-11：YTH同SYDH图层映射表*/
  void Load_YTH_SYDH_layer_mapping_table(YTH_SYDH_layer_mapping_table_t& YTH_SYDH_layer_mapping_table);

  /*2023-03-11：YTH同SYDH编码映射表*/
  void Load_YTH_SYDH_code_mapping_table(YTH_SYDH_code_mapping_table_t& YTH_SYDH_code_mapping_table);

  /*2023-03-29：YTH同QQCT编码映射表*/
  void Load_YTH_QQCT_layer_mapping_table(YTH_QQCT_layer_mapping_table_t& YTH_QQCT_layer_mapping_table);

  /*2023-03-29：YTH同QQCT编码映射表*/
  void Load_YTH_QQCT_code_mapping_table(YTH_QQCT_code_mapping_table_t& YTH_QQCT_code_mapping_table);



/**************************************************读取配置文件相关的函数******************************************************/

private slots:
  void Button_input_data_view();
  void Button_output_data_view();
  void Button_OK();
  void Button_Cancel();


private:

	Ui_qgs_semantic_fusion ui;

	//  输入数据路径
	QString m_input_data_path;

	//  输出数据路径
	QString m_output_data_path;

};

#endif // _SEMANTIC_FUSION_H
