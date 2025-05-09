#ifndef QGS_CONVERT_DATA_TO_GJB_H
#define QGS_CONVERT_DATA_TO_GJB_H

#include <QDialog>

#include "ui_convert_data_to_gjb.h"
#include "CSE_GeoExtractAndProcess.h"
#include <vector>

using namespace std;

class QgsConvertDataToGJB : public QDialog
{
	Q_OBJECT

public:
	QgsConvertDataToGJB(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsConvertDataToGJB() override;

public:

	// 获取划分图层存储路径
	QString GetLayerSavePath();

	// 获取文件路径
	QString GetInputFilePath();

	// 获取输入类型:0为民口数据；1为军口数据
	int GetInputType();

	/*国标图层配置文件*/
	void LoadBetterThan5wConfig_GBLayer(vector<GBLayerInfo>& vGBLayerInfo);

	/*军标图层配置文件*/
	void LoadBetterThan5wConfig_JBLayer(vector<JBLayerInfo>& vJBLayerInfo);

	/*军标图层配置文件*/
	void LoadBetterThan5wConfig_GJBLayerField(vector<GJBLayerInfo>& vGJBLayerInfo);

	// GB到GJB编码配置文件
	void LoadBetterThan5wConfig_GBCode2GJBCode(vector<GB2GJB_FeatureClassMap>& vGB2GJB);

	// JB到GJB编码配置文件
	void LoadBetterThan5wConfig_JBCode2GJBCode(vector<JB2GJB_FeatureClassMap>& vJB2GJB);

	// 获取当前目录下所有shp文件
	QStringList GetShpFileNames(const QString& path);

	//int GeoExtractAndProcessProgress(double dTotalWork, double dCompletedWork, double dPercent, const char* pszMessage);

	QStringList GetDirPathOfSplDir(QString dirPath);

	// 判断字符串是否包含列表中的任意一字符串
	bool bIsExistedInLayerNameList(string strLayerName, const vector<string>& vNameList);

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);


#pragma region "DLHJ-杨小兵-2023-9-12"

public:
	void DLHJButton_OK_accepted();

	/*国标图层配置文件*/
	void LoadDLHJConfig_GBLayer(GBlayerLayer_List_t& GBlayerLayer_List);

	/*军标图层配置文件*/
	void LoadDLHJConfig_JBLayer(JBlayerLayer_List_t& JBlayerLayer_List);
	
	/*导航图层配置文件*/
	void LoadDLHJConfig_DHLayer(DHlayerLayer_List_t& DHlayerLayer_List);

	/*OSM图层配置文件*/
	void LoadDLHJConfig_OSMLayer(OSMlayerLayer_List_t& OSMlayerLayer_List);

	/*TDT图层配置文件*/
	void LoadDLHJConfig_TDTLayer(TDTlayerLayer_List_t& TDTlayerLayer_List);

	/*国军标图层配置文件*/
	void LoadDLHJConfig_GJBLayerField(GJBLayerFieldLayer_List_t& GJBLayerFieldLayer_List);

	// GB到GJB编码配置文件
	void LoadDLHJConfig_GBCode2GJBCode(GB2GJBLayer_List_t& GB2GJBLayer_List);

	// JB到GJB编码配置文件
	void LoadDLHJConfig_JBCode2GJBCode(JB2GJBLayer_List_t& JB2GJBLayer_List);

	// DH到GJB编码配置文件
	void LoadDLHJConfig_DHCode2GJBCode(DH2GJBLayer_List_t& DH2GJBLayer_List);

	// OSM到GJB编码配置文件
	void LoadDLHJConfig_OSMCode2GJBCode(OSM2GJBLayer_List_t& OSM2GJBLayer_List);

	// TDT到GJB编码配置文件
	void LoadDLHJConfig_TDTCode2GJBCode(TDT2GJBLayer_List_t& TDT2GJBLayer_List);

	//	从读取到的配置文件信息提取国标图层名称列表
	void ExtractGBlayerNameList(GBlayerLayer_List_t& GBlayerLayer_List, vector<string>& GBlayerLayer_List_name);

	//	从读取到的配置文件信息提取军标图层名称列表
	void ExtractJBlayerNameList(JBlayerLayer_List_t& JBlayerLayer_List, vector<string>& JBlayerLayer_List_name);

	//	从读取到的配置文件信息提取导航图层名称列表
	void ExtractDHlayerNameList(DHlayerLayer_List_t& DHlayerLayer_List, vector<string>& DHlayerLayer_List_name);

	//	从读取到的配置文件信息提取OSM图层名称列表
	void ExtractOSMlayerNameList(OSMlayerLayer_List_t& OSMlayerLayer_List, vector<string>& OSMlayerLayer_List_name);

	//	从读取到的配置文件信息提取TDT图层名称列表
	void ExtractTDTlayerNameList(TDTlayerLayer_List_t& TDTlayerLayer_List, vector<string>& TDTlayerLayer_List_name);

  /*2023-03-11：YTH同OSM图层映射表*/
  void Load_YTH_OSM_layer_mapping_table(YTH_OSM_layer_mapping_table_t& YTH_OSM_layer_mapping_table);

  /*2023-03-11：YTH同OSM编码映射表*/
  void Load_YTH_OSM_code_mapping_table(YTH_OSM_code_mapping_table_t& YTH_OSM_code_mapping_table);

  /*2024-03-11:一体化属性表结构*/
  void Load_YTH_fields4all_layers_config_file(YTH_fields4all_layers_t& YTH_fields4all_layers);


	//	检查XML文件
	void checkAndDebugXML(const QString& strFileName);

	int MyProgressCallback(double dTotalWork, double dCompletedWork, double dPercent, const char* pszMessage);

	void QgsConvertDataToGJB::IsTheLayerNamingStandardized(
		const QStringList& qstrPathList,
		const QString qstrInputDataPath,
		const vector<string>& vNameList);
#pragma endregion

public slots:

private:

	Ui_QgsConvertDataToGJB ui;
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

#endif // QGS_CONVERT_DATA_TO_GJB_H
