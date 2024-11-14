#ifndef QGS_SET_BETTERTHAN5W_MATCH_PARAMS_H
#define QGS_SET_BETTERTHAN5W_MATCH_PARAMS_H

#include <QDialog>

#include "ui_better_than_5w_match.h"
#include "CSE_GeoExtractAndProcess.h"

class QgsSetBetterThan5WMatchParams : public QDialog
{
	Q_OBJECT

public:
	QgsSetBetterThan5WMatchParams(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsSetBetterThan5WMatchParams() override;

public slots:

private:

	Ui_QgsSetBetterThan5wMatchParamsDlg ui;

	// 军标数据存储路径
	QString m_qstrJunBiaoDataPath;

	// 国标数据存储路径
	QString m_qstrGuoBiaoDataPath;

	// 融合匹配数据库存储路径
	QString m_qstrMatchDBPath;

private slots:

	// 加载配置文件
	void pushButton_LoadMatchParams_clicked();

	// 另存配置文件
	void pushButton_SaveMatchParams_clicked();

	// 确定按钮
	void Button_OK_accepted();

	// 取消按钮
	void Button_Cancel_rejected();

	// 打开军标数据目录按钮
	void Button_OpenJunBiaoData();

	// 打开国标数据目录按钮
	void Button_OpenGuoBiaoData();

	// 输出匹配数据库路径路径
	void Button_MatchDBSave_clicked();

public:

	/*获取点、线面匹配阈值*/
	void GetMatchDistance(double& dPointDistance, double& dLineAndPolygonDistance);

	/*获取军标数据存储路径*/
	QString GetJunBiaoDataPath();

	/*获取国标数据存储路径*/
	QString GetGuoBiaoDataPath();

	/*获取融合匹配数据库保存路径*/
	QString GetMatchDBSavePath();

	void restoreState();

	// 输入文本框限制
	void InitLineEdit();

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

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
};

#endif // QGS_SET_BETTERTHAN5W_MATCH_PARAMS_H
