#ifndef SE_FEATURE_ATTRIBUTE_CHECK_YTH_H
#define SE_FEATURE_ATTRIBUTE_CHECK_YTH_H

#include <QDialog>


#include "ui_se_feature_attribute_check_yth.h"
#include "vector/cse_vector_datacheck.h"
#include "../ui_thread/se_feature_attribute_check_thread_yth.h"
#include <QString>
/*----------------GDAL--------------*/
#include <gdal.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
/*-----------------------------------*/
#include <vector>
#include <string>
#include <map>

#include "qgisinterface.h"

/*读取xml头文件*/
#include <libxml/parser.h>
using namespace std;

class CSE_FeatureAttributeCheckYTHDialog : public QDialog
{
	Q_OBJECT

public:
	CSE_FeatureAttributeCheckYTHDialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_FeatureAttributeCheckYTHDialog() override;



public:

	// 获取xml数据路径
	QString GetInputXmlPath();

	// 获取shp数据路径
	QString GetInputShpDataPath();

	// 获取输出日志文件路径
	QString GetOutputLogPath();

	// 获取指定目录的最后一级目录
	void GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName);

	// 获取指定目录的最后一级目录
	void GetFolderNameFromPath_string(string strPath, string& strFolderName);

	int GBK_2_UTF8(char* utfstr, const char* srcstr, int maxutfstrlen);

	// 属性检查
	SE_Error FeatureAttributeCheck(const char* szInputShpPath, const char* szAttrCheckConfigXmlFile, vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo);

	// 加载属性检查配置文件
	bool LoadFeatureAttrCheckXmlConfigFile(const char* szAttrCheckConfigXmlFile, vector<VectorLayerFieldInfo>& vLayerConfigFieldInfo);

	string UTF8_To_GBK(const string& str);

	void GetLayerInfo(string strLayerName, string& strSheetNumber, string& strLayerType, string& strGeometryType);

	void Parse_layer_fields(xmlDocPtr doc, xmlNodePtr cur, VectorLayerFieldInfo& info);

	void Parse_fields(xmlDocPtr doc, xmlNodePtr cur, FieldInfo& info);

	void Parse_field_enum_values_list_simple(xmlDocPtr doc, xmlNodePtr cur, vector<string>& vSimpleEnumFieldValue);

	void Parse_field_enum_values_list_complex(xmlDocPtr doc, xmlNodePtr cur, vector<FieldEnumValue>& vComplexEnumFieldValue);

	void Parse_field_enum_values(xmlDocPtr doc, xmlNodePtr cur, FieldEnumValue& enumValue);

	void GetAttributeCheckInfoByLayerType(string strLayerType, vector<VectorLayerFieldInfo>& vLayersFieldInfo, VectorLayerFieldInfo& sLayerFieldInfo);

	void LayerFieldInfoCheck(vector<LayerFieldInfo>& vLayerFieldInfo, VectorLayerFieldInfo& sLayerFieldCheckInfo, vector<VectorFieldAttrCheckError>& vAttributeErrorList);

	SE_Error GetFeatureAttr(OGRFeature* poFeature, map<string, string>& mFieldValue, vector<LayerFieldInfo>& vLayerFieldInfo);

	void LayerFieldAttrCheck(map<string, string>& mapFieldValue, VectorLayerFieldInfo& sLayerFieldCheckInfo, vector<VectorFieldAttrCheckError>& vAttributeErrorList);

	bool FieldValueIsExistedInSimpleEnumList(vector<string>& vSimpleEnumValue, string strValue);

	// 获取当前目录下所有子目录
	QStringList GetSubDirPathOfCurrentDir(QString dirPath);

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);


public slots:

private:

	Ui_SeFeatureAttributeCheckYTHDialog ui;

	// 恢复保存参数
	void restoreState();

	// 日志保存路径
	QString m_qstrSaveLogPath;

	// 输入xml路径
	QString m_qstrInputXmlPath;

	// 输入shp路径
	QString m_qstrInputShpDataPath;

	QgisInterface* m_qgisInterface;

	SE_FeatureAttributeThreadYTH m_Thread;

private slots:

	// 保存日志数据
	void Button_Save_clicked();

	// 打开xml目录
	void Button_OpenXml_clicked();

	// 打开shp数据目录
	void Button_OpenShp_clicked();

	// 要素属性检查
	void Button_OK_accepted();

	// 关闭
	void Button_Cancel_rejected();

	void on_lineEdit_InputShpDataPath_TextChanged(const QString& qstr);

	void on_lineEdit_InputXmlPath_TextChanged(const QString& qstr);

	void on_lineEdit_OutputLogPath_TextChanged(const QString& qstr);

	void handleResults(const double& dPercent, const QString& s);
};

#endif // SE_FEATURE_ATTRIBUTE_CHECK_YTH_H
