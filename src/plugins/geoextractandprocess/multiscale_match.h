#ifndef QGS_SET_MULTISCALEDATA_MATCH_PARAMS_H
#define QGS_SET_MULTISCALEDATA_MATCH_PARAMS_H

#include <QDialog>

#include "ui_multiscale_match.h"
#include "cse_geoextractandprocess.h"

class QgsSetMultiScaleDataMatchParams : public QDialog
{
	Q_OBJECT

public:
	QgsSetMultiScaleDataMatchParams(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsSetMultiScaleDataMatchParams() override;

public slots:

private:

	Ui_QgsSetMultiscaleMatchParamsDlg ui;

	// 大比例尺数据存储路径
	QString m_qstrBigScaleDataPath;

	// 小比例尺数据存储路径
	QString m_qstrSmallScaleDataPath;

	// 多尺度匹配数据库存储路径
	QString m_qstrMultiScaleDBPath;

private slots:

	// 加载配置文件
	void pushButton_LoadMatchParams_clicked();

	// 另存配置文件
	void pushButton_SaveMatchParams_clicked();

	// 确定按钮
	void Button_OK_accepted();

	// 取消按钮
	void Button_Cancel_rejected();

	// 打开小比例尺数据按钮
	void Button_OpenSmallScale_clicked();

	// 打开大比例尺数据按钮
	void Button_OpenBigScale_clicked();

	// 输出匹配数据库路径路径
	void Button_MatchDBSave_clicked();

public:

	/*获取点、线面匹配阈值*/
	void GetMatchDistance(double& dPointDistance, double& dLineAndPolygonDistance);

	/*获取大比例尺数据存储路径*/
	QString GetBigScaleDataPath();

	/*获取小比例尺数据存储路径*/
	QString GetSmallScaleDataPath();

	/*获取多尺度匹配数据库保存路径*/
	QString GetMultiScaleDBSavePath();

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	/*获取图层匹配参数*/
	void GetLayerMatchParams(vector<LayerMatchParam>& vLayerMatchParam);

private:

	/*获取各图层字段选择列表*/
	// strLayerType:图层名称
	// vFields：选中的匹配字段列表，如果列表为空，则说明当前图层不进行匹配
	void GetLayerFieldsChecked(string strLayerType,
		vector<string>& vFields);

	/*获取各图层几何要素选择列表*/
	void GetLayerGeoChecked(string strLayerType,
		bool& bPointChecked,
		bool& bLineChecked,
		bool& bPolygonChecked);

	/*根据配置文件设置ui显示状态*/
	void SetCheckBoxStatus(LayerMatchParam& param);

	/*获取在指定目录下的目录的路径*/
	QStringList GetDirPathOfSplDir(QString dirPath);

	/*获取指定目录的最后一级目录*/
	void GetFolderNameFromPath(string strPath, string& strFolderName);

	// 恢复保存参数
	void restoreState();

	// 判断矩形是否与另一矩形有交集
	bool bRectInOtherRect(SE_DRect dInputRect, SE_DRect dRefRect);


	// 初始化对话框中文本框
	void InitLineEdit();
};

#endif // QGS_SET_MULTISCALEDATA_MATCH_PARAMS_H
