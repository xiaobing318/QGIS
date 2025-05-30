#ifndef SE_PBF2GPKG_H
#define SE_PBF2GPKG_H
/*--------------QT---------------*/
#include <QDialog>
#include <QString>

/*--------------QGIS---------------*/
#include "qgisinterface.h"

/*--------------标准库---------------*/
#include <vector>
#include <string>
#include <tchar.h>

/*--------------ui类---------------*/
#include "ui_se_pbf2gpkg.h"
//#include "../ui_thread/se_pbf2gpkg_thread.h"


class CSE_pbf2gpkg_dialog : public QDialog
{
	Q_OBJECT

public:
	CSE_pbf2gpkg_dialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_pbf2gpkg_dialog() override;

	//	获取pbf数据目录路径
	std::string get_pbf_input_data_path();
	
	//	获取gpkg输出数据路径
	std::string get_gpkg_output_data_path();

	//	获取想要处理pbf瓦片的级数
	std::string get_pbf_data_input_level();

	//	获取输入数据源中的一个特定图层或要素类
	std::string get_specific_layer_name();

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);
private:
	//	判断当前路径下是否存在想要处理的瓦片级数
	int is_exist_pbf_data_input_level(
		const std::string& pbf_input_data_path,
		const std::string& pbf_data_input_level,
		std::string& full_path_of_existing_subfolder);

	int get_all_pbf_file_paths(
		const std::string& input_data_path,
		std::vector<std::vector<std::string>>& all_pbf_file_paths);

	std::string TChar2String(TCHAR* STR);

	void change_path_symbol(std::string& path);

	int deleteFile(const std::string& filePath);

	std::string ConvertEncoding(const std::string& input, const std::string& fromEncoding, const std::string& toEncoding);

public slots:

private:

	Ui_se_pbf2gpkg_dialog ui;

	//	恢复保存参数
	void restoreState();

	//	保存界面输入的参数
	void setState();

	//	输入图层路径
	QString m_qstr_pbf_data_input_path;

	//	图层保存路径
	QString m_qstr_gpkg_data_output_path;

	//	pbf矢量地理空间瓦片级数
	QString m_qstr_pbf_data_input_level;

	//	输入数据源中的一个特定图层或要素类
	QString m_qstr_specific_layer_name;

	//	QGIS插件接口
	QgisInterface* m_qgisInterface;


	//	设置一个创建空的gpkg文件的flag
	int create_empty_gpkg_flag;




private slots:
	//	打开pbf地理空间数据
	void Button_pbf_data_path_open();

	//	打开gpkg地理空间数据库保存位置
	void Button_gpkg_data_path_open();

	//	pbf地理空间数据文件向gpkg地理空间数据库进行转化
	void Button_OK_accepted();

	//	取消pbf地理空间数据文件向gpkg地理空间数据库进行转化
	void Button_Cancel_rejected();

	//	输入的pbf地理空间数据路径更新
	void on_pbf_InputDataPath_TextChanged(const QString& qstr);

	//	输出的gpkg地理空间数据路径更新
	void on_gpkg_OutputDataPath_TextChanged(const QString& qstr);
	
	int convert_each_pbf2gpkg(
		const std::string& pbf_data_input_path,
		const std::string& gpkg_data_output_path);
};

#endif // SE_PBF2GPKG_H
