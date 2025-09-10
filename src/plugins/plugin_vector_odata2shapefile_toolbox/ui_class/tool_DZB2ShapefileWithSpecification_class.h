#ifndef TOOL_DZB2SHAPEFILEWITHSPECIFICATION_CLASS_H
#define TOOL_DZB2SHAPEFILEWITHSPECIFICATION_CLASS_H
#pragma region "包含头文件（减少重复）"

/*----------------STL--------------*/
#include <vector>
#include <string>
/*----------------STL--------------*/

/*----------------QT--------------*/
#include <QDialog>
#include <QString>
/*----------------QT--------------*/

/*----------------自定义--------------*/
#include "ui_tool_DZB2ShapefileWithSpecification.h"
#include "base_vector_odata2shapefile.h"
#include "../ui_thread/tool_DZB2ShapefileWithSpecification_thread.h"
/*----------------自定义--------------*/

/*----------------QGIS--------------*/
#include "qgisinterface.h"
/*----------------QGIS--------------*/

/*----------------spdlog第三方日志库--------------*/
#include "spdlog/spdlog.h"
//	创建基本的日志文件，这个日志文件用来接收一般的信息
#include "spdlog/sinks/basic_file_sink.h"
/*----------------spdlog第三方日志库--------------*/

using namespace std;
#pragma endregion

#pragma region "odata格式转化对话类"
class ToolDZB2ShapefileWithSpecificationDialog : public QDialog
{
	Q_OBJECT

public:
	ToolDZB2ShapefileWithSpecificationDialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~ToolDZB2ShapefileWithSpecificationDialog() override;

public:

	//  获取odata数据路径
	QString GetInputDataPath();
	//  获取输出数据路径
	QString GetOutputDataPath();
	//  获取输出数据空间参考系：1——与输入数据保持一致；2——输出地理坐标系；3——输出投影坐标系
	int GetOutputSRS();
	//  获取比例尺分母
	QString GetScale();
	//  获取X偏移量
	double GetOffsetX();
	//  获取Y偏移量
	double GetOffsetY();
	//	获得是否选择手动输入放大系数
	int get_setting_zoomscale_method();
	//	获得手动输入的放大系数数值
	double get_dzoomscale();
	//  获取图层信息方式：1——从*.SMS文件中获取图层信息；2——从实际odata数据目录中获取图层信息
	int get_the_method_of_obtaining_layer_information();
  //  获取日志等级
	spdlog::level::level_enum get_log_level();
	//	获取汇总日志保存路径
	std::string get_summary_log_saving_path_str();
	//	获取汇总日志保存路径
	QString get_summary_log_saving_path_qstr();
	//  获取当前路径的最后一级目录名
	void GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName);
	//  获取比例尺分母
	int GetScale(string strCode);
	//  获取当前目录下所有shp文件
	QStringList GetShpFileNames(const QString& path);
	//  获取当前目录下所有子目录
	QStringList GetSubDirPathOfCurrentDir(QString dirPath);

  //  初始化文本框
  void InitLineEdit();
	//  判断目录是否存在
	bool FilePathIsExisted(const QString& qstrPath);
	//  判断文件是否存在
	bool FileIsExisted(const QString& qstrFilePath);

public slots:

private:
  QgisInterface* m_qgisInterface;

  //  odata格式转化对话框
  Ui_ToolDZB2ShapefileWithSpecificationDialog ui;
	//  恢复用户界面参数
	void restoreState();
	//	保存用户界面参数
	void saveState();

	//  输出图层路径
	QString m_qstrSaveDataPath;
	//  输入图层路径
	QString m_qstrInputDataPath;
	//  图幅个数
	int m_SheetCount;
	//	添加多线程并行，增加速度。变量作用：多线程对象，根据计算机CPU核数分配线程数
  TOOLDZB2ShapefileWithSpecificationThread* m_pThread;
	//	spdlog 日志器等级设置（默认为trace级别：最为详细的级别；全部的级别：trace、debug、info、warn、err、critical、off
	spdlog::level::level_enum m_log_level;
	//	获取汇总日志保存路径
	QString m_summary_log_saving_path;

  //  统计信息
  int total_processed_frame_data_count;
  int total_successfully_processed_frame_data_count;
  bool m_isHandlingResults;


private slots:

	//  浏览输出数据路径
	void Button_Save_clicked();
	//  浏览输入数据路径
	void Button_Open_clicked();
	//	浏览汇总日志保存路径
	void Button_summary_log_saving_path_clicked();
	//  开始odata2shapefile格式转换
	void Button_OK_accepted();
	//  关闭odata2shapefile格式转换
	void Button_Cancel_rejected();
	//  更新输入数据路径状态将其显示在用户界面上
	void on_InputDataPath_TextChanged(const QString& qstr);
	//	更新汇总日志保存路径状态将其显示在用户界面上
	void on_summary_log_saving_path_TextChanged(const QString& qstr);
	//  更新输出数据目录将其显示在用户界面上
	void on_lineEdit_OutputDataPath_TextChanged(const QString& qstr);
  //  X偏移量更新
  void on_lineEdit_OffsetX_TextChanged(const QString& qstr);
  //  Y偏移量更新
  void on_lineEdit_OffsetY_TextChanged(const QString& qstr);

	//  选择不同的坐标系的时候同时更新进度条
	void on_radioButton_OriginSRS_clicked(bool checked = false);
	void on_radioButton_GeoSRS_clicked(bool checked);
	void on_radioButton_ProjSRS_clicked(bool checked);

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
#pragma endregion
#endif // TOOL_DZB2SHAPEFILEWITHSPECIFICATION_CLASS_H
