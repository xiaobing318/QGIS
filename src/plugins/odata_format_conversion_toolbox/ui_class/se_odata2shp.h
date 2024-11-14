#ifndef SE_ODATA2SHP_H
#define SE_ODATA2SHP_H

#include <QDialog>

#include "ui_se_odata2shp.h"
#include "vector/cse_vector_format_conversion.h"
#include <QString>
#include "qgisinterface.h"
#include <vector>
#include <string>
#include "../ui_thread/se_odata2shp_thread.h"

/*----------------spdlog第三方日志库--------------*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"	//	创建基本的日志文件，这个日志文件用来接收一般的信息
/*----------------spdlog第三方日志库--------------*/

using namespace std;

class CSE_VectorFormatConversionDialog : public QDialog
{
	Q_OBJECT

public:
	CSE_VectorFormatConversionDialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_VectorFormatConversionDialog() override;

public:

	// 获取odata数据路径
	QString GetInputDataPath();

	// 获取输出数据路径
	QString GetOutputDataPath();

	// 获取输出数据空间参考系：1——与输入数据保持一致；2——输出地理坐标系；3——输出投影坐标系
	int GetOutputSRS();

	// 获取比例尺分母
	QString GetScale();

	// 获取X偏移量
	double GetOffsetX();

	// 获取Y偏移量
	double GetOffsetY();

	//	获得是否选择手动输入放大系数（杨小兵-2023-12-07）
	int get_setting_zoomscale_method();

	//	获得手动输入的放大系数数值（杨小兵-2023-12-07）
	double get_dzoomscale();

	//  获取图层信息方式：1——从*.SMS文件中获取图层信息；2——从实际odata数据目录中获取图层信息
	int get_the_method_of_obtaining_layer_information();

	spdlog::level::level_enum get_log_level();

	//	获取汇总日志保存路径
	std::string get_summary_log_saving_path_str();
	//	获取汇总日志保存路径	
	QString get_summary_log_saving_path_qstr();

	//  获取当前路径的最后一级目录名
	void GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName);

	//  获取比例尺分母
	int GetScale(string strCode);

	//  初始化文本框
	void InitLineEdit();

	//  获取当前目录下所有shp文件
	QStringList GetShpFileNames(const QString& path);

	//  获取当前目录下所有子目录
	QStringList GetSubDirPathOfCurrentDir(QString dirPath);


	//  判断目录是否存在
	bool FilePathIsExisted(const QString& qstrPath);

	//  判断文件是否存在
	bool FileIsExisted(const QString& qstrFilePath);

public slots:

private:

	Ui_SeVectorFormatConversionDialog ui;
	// 恢复保存参数
	void restoreState();
	//	保存输入输出路径参数
	void saveState();

	// 图层保存路径
	QString m_qstrSaveDataPath;

	// 输入图层路径
	QString m_qstrInputDataPath;

	QgisInterface* m_qgisInterface;

	// 图幅个数
	int m_SheetCount;

	//	（杨小兵-2023-12-11：添加多线程并行，增加速度；代码由杨志龙杨老师提供，杨小兵进行集成。变量作用：多线程对象，根据计算机CPU核数分配线程数）
	SE_Odata2shpThread* m_pThread;

	//	（杨小兵-2024-01-24）spdlog日志器等级设置（默认为trace级别：最为详细的级别；全部的级别：trace、debug、info、warn、err、critical、off）
	spdlog::level::level_enum m_log_level;

	//	（杨小兵-2024-01-29）获取汇总日志保存路径
	QString m_summary_log_saving_path;

  //  杨小兵-2024-11-07：统计信息
private:
  int total_processed_frame_data_count;
  int total_successfully_processed_frame_data_count;
  bool m_isHandlingResults;


private slots:

	// 保存结果数据
	void Button_Save_clicked();

	//  打开odata数据目录
	void Button_Open_clicked();

	//	汇总日志保存路径
	void Button_summary_log_saving_path_clicked();

	//  转换
	void Button_OK_accepted();

	//  关闭
	void Button_Cancel_rejected();

	//  odata路径状态更新
	void on_InputDataPath_TextChanged(const QString& qstr);

	//	（杨小兵-2024-01-29）获取汇总日志保存路径状态更新
	void on_summary_log_saving_path_TextChanged(const QString& qstr);

	//  数据保存目录更新
	void on_lineEdit_OutputDataPath_TextChanged(const QString& qstr);
	
	//  输出坐标系更新
	void on_radioButton_OriginSRS_clicked(bool checked = false);

	void on_radioButton_GeoSRS_clicked(bool checked);

	void on_radioButton_ProjSRS_clicked(bool checked);

	// X偏移量更新
	void on_lineEdit_OffsetX_TextChanged(const QString& qstr);

	// Y偏移量更新
	void on_lineEdit_OffsetY_TextChanged(const QString& qstr);

  //  处理进度
	void handleResults(
    const int& processed_frame_data_flag,
    const int& iProcessed,
    const QString& s);

  //  合并日志
  void mergeLogFiles();

  //  新的槽函数处理合并和进度显示
  void onAllProcessingCompleted();
};

#endif // SE_ODATA2SHP_H
