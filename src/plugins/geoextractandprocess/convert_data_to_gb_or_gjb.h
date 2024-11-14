#ifndef QGS_CONVERT_DATA_TO_GB_OR_GJB_H
#define QGS_CONVERT_DATA_TO_GB_OR_GJB_H

#include <QDialog>

#include "ui_convert_data_to_gb_or_gjb.h"
#include "cse_geoextractandprocess.h"
#include <vector>

using namespace std;

class QgsConvertDataToGB_OR_GJB : public QDialog
{
	Q_OBJECT

public:
	QgsConvertDataToGB_OR_GJB(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsConvertDataToGB_OR_GJB() override;

public:

	// 获取数据存储路径
	QString GetLayerSavePath();

	// 获取文件路径
	QString GetInputFilePath();

	// 获取导出数据类型: 0：国标数据；1：军标数据
	int GetOutputType();

	/*国标图层配置文件*/
	void LoadBetterThan5wConfig_GBLayer(vector<GBLayerInfo>& vGBLayerInfo);

	/*军标图层配置文件*/
	void LoadBetterThan5wConfig_JBLayer(vector<JBLayerInfo>& vJBLayerInfo);

	/*军标图层配置文件*/
	void LoadBetterThan5wConfig_GJBLayerField(vector<GJBLayerInfo>& vGJBLayerInfo);

	/*国标图层字段配置文件*/
	void LoadBetterThan5wConfig_GBLayerFieldList(vector<GB_LayerFieldList>& vGBLayerFieldList);

	/*军标图层字段配置文件*/
	void LoadBetterThan5wConfig_JBLayerFieldList(vector<JB_LayerFieldList>& vJBLayerFieldList);

	// 获取shp文件列表
	QStringList GetShpFileNames(const QString& path);

	// 判断字符串是否包含列表中的任意一字符串
	bool bIsExistedInLayerNameList(string strLayerName, vector<string>& vNameList);

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 加载GB到GJB编码映射配置文件
	void LoadBetterThan5wConfig_GBCode2GJBCode(vector<GB2GJB_FeatureClassMap>& vGB2GJB);

	// 加载JB到GJB编码映射配置文件
	void LoadBetterThan5wConfig_JBCode2GJBCode(vector<JB2GJB_FeatureClassMap>& vJB2GJB);

	QStringList GetDirPathOfSplDir(QString dirPath);

public slots:

private:

	Ui_QgsConvertDataToGBorGJB ui;
	// 恢复保存参数
	void restoreState();

	// 图层保存路径
	QString m_qstrSaveDataPath;

	// 输入图层路径
	QString m_qstrInputDataPath;

private slots:

	// 保存按钮
	void Button_Save_clicked();

	// 打开按钮
	void Button_Open_clicked();

	void Button_OK_accepted();

	void Button_Cancel_rejected();
};

#endif // QGS_CONVERT_DATA_TO_GB_OR_GJB_H
