#ifndef QGS_GEOEXTRACTANDPROCESS_PLUGIN_SET_MERGE_PARAMS_H
#define QGS_GEOEXTRACTANDPROCESS_PLUGIN_SET_MERGE_PARAMS_H

#include <QDialog>

#include "ui_geoextractandprocess_plugin_set_merge_params.h"
#include "cse_geoextractandprocess.h"

class QgsSetMergeParams : public QDialog
{
	Q_OBJECT

public:
	QgsSetMergeParams(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsSetMergeParams() override;

public slots:

private:

	Ui_QgsGeoExtractAndProcessPluginSetMergeParams ui;

private slots:

	// 加载配置文件
	void pushButton_LoadMergeParams_clicked();

	// 另存配置文件
	void pushButton_SaveMergeParams_clicked();

	void Button_OK_accepted();
	void Button_Cancel_rejected();
	void Button_Help_showHelp();

	// 设置输入限制
	void InitLineEdit();

public:

	/*获取接边几何类型选择状态：线接边或面接边*/
	void GetGeoTypeChecked(bool& bLineChecked, bool& bPolygonChecked);

	/*获取接边比例尺*/
	int GetScaleType();

	/*获取缓冲区距离*/
	double GetBufferDistance();

	/*获取图层匹配参数*/
	void GetLayerMatchParams(vector<LayerMatchParam>& vLayerMatchParam);

private:
	// 缓冲区距离
	QString m_strBufferDistance;

	/*获取各图层字段选择列表*/
	// strLayerType:图层名称
	// vFields：选中的匹配字段列表，如果列表为空，则说明当前图层不进行匹配
	void GetLayerFieldsChecked(string strLayerType,
		vector<string>& vFields);
	/*根据配置文件设置ui显示状态*/
	void SetCheckBoxStatus(LayerMatchParam& param);
};

#endif // QGS_GEOEXTRACTANDPROCESS_PLUGIN_SET_MERGE_PARAMS_H
