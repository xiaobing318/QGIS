#ifndef SE_GEOCOORDSYS_TRANS_H
#define SE_GEOCOORDSYS_TRANS_H

#include <QDialog>

#include "ui_se_geocoordsys_trans.h"
#include "vector/cse_coordinate_transformation.h"
#include <QString>
#include "qgisinterface.h"
#include <vector>
#include <string>
#include "se_geocoordsys_trans_thread.h"


/*----------------spdlog第三方日志库--------------*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"	//	创建基本的日志文件，这个日志文件用来接收一般的信息
/*----------------spdlog第三方日志库--------------*/

using namespace std;

class CSE_GeoCoordSysTransDialog : public QDialog
{
	Q_OBJECT

public:
	CSE_GeoCoordSysTransDialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_GeoCoordSysTransDialog() override;

public:

	// 获取数据路径
	QString GetInputDataPath();

	// 获取输出数据路径
	QString GetOutputDataPath();

	// 获取输入坐标系类型
	// 1—CGCS2000；2—BJS54；3—XAS80;4—WGS84；5—境外坐标系
	int GetFromGeo();

	// 获取输出坐标系类型
	// 1—CGCS2000；2—BJS54；3—XAS80;4—WGS84；
	int GetToGeo();

	// 获取坐标转换参数
	void GetTransParams(CoordTransParams &params);

	// 获取椭球参数
	void GetEllipseParams(double& dSemiMajorAxis, double& dSemiMinorAxis);
	
	// 初始化文本框
	void InitLineEdit();


	// 获取当前目录下所有子目录
	QStringList GetSubDirPathOfCurrentDir(QString dirPath);


	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

public slots:

private:

	Ui_SE_GeoCoordSysTransDialog ui;
	// 恢复保存参数
	void restoreState();

	// 图层保存路径
	QString m_qstrSaveDataPath;

	// 输入图层路径
	QString m_qstrInputDataPath;

	QgisInterface* m_qgisInterface;

	// 图幅个数
	int m_SheetCount;

	// 地理坐标系转换线程
	CSE_GeoCoordSysTransThread* m_pThread;

  //	（杨小兵-2024-04-02）spdlog日志器等级设置（默认为trace级别：最为详细的级别；全部的级别：trace、debug、info、warn、err、critical、off）
  spdlog::level::level_enum m_log_level;

  //	（杨小兵-2024-04-02）获取汇总日志保存路径
  QString m_summary_log_saving_path;
private slots:

	// 保存结果数据
	void Button_Save_clicked();

	// 打开odata数据目录
	void Button_Open_clicked();

	// 转换
	void Button_OK_accepted();

	// 关闭
	void Button_Cancel_rejected();

  //	杨小兵-2024-04-02：汇总日志保存路径
  void Button_summary_log_saving_path_clicked();

  //  杨小兵-2024-04-02：获取汇总日志保存路径状态更新
  void on_summary_log_saving_path_TextChanged(const QString& qstr);

  spdlog::level::level_enum get_log_level();

	// 输入路径状态更新
	void on_lineEdit_InputDataPath_TextChanged(const QString& qstr);

	// 数据保存目录更新
	void on_lineEdit_OutputDataPath_TextChanged(const QString& qstr);
	
	// 输入坐标系更新
	void on_radioButton_From_CGCS2000_clicked(bool checked);
	void on_radioButton_From_WGS84_clicked(bool checked);
	void on_radioButton_From_BJS54_clicked(bool checked);
	void on_radioButton_From_XAS80_clicked(bool checked);
	void on_radioButton_From_Foreign_clicked(bool checked);

	// 输出坐标系更新
	void on_radioButton_To_CGCS2000_clicked(bool checked);
	void on_radioButton_To_WGS84_clicked(bool checked);
	void on_radioButton_To_BJS54_clicked(bool checked);
	void on_radioButton_To_XAS80_clicked(bool checked);

	// 长半轴更新
	void on_lineEdit_SemiMajorAxis_TextChanged(const QString& qstr);

	// 短半轴更新
	void on_lineEdit_SemiMinorAxis_TextChanged(const QString& qstr);

	// 转换参数更新
	void on_lineEdit_dX_TextChanged(const QString& qstr);
	void on_lineEdit_dY_TextChanged(const QString& qstr);
	void on_lineEdit_dZ_TextChanged(const QString& qstr);
	void on_lineEdit_rX_TextChanged(const QString& qstr);
	void on_lineEdit_rY_TextChanged(const QString& qstr);
	void on_lineEdit_rZ_TextChanged(const QString& qstr);
	void on_lineEdit_K_TextChanged(const QString& qstr);

	// 
	void handleResults(const int& processed_frame_data_flag, const int& iSuccessProcessed, const QString& s);
};

#endif // SE_GEOCOORDSYS_TRANS_H
