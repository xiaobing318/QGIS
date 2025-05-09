#ifndef SE_FEATURE_ATTRIBUTE_THREAD_H
#define SE_FEATURE_ATTRIBUTE_THREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
/*----------------GDAL--------------*/
#include <gdal.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
/*-----------------------------------*/
#include <string>
#include <map>
#include "vector/cse_vector_datacheck.h"

/*读取xml头文件*/
#include <libxml/parser.h>
using namespace std;
typedef map<string, string> MAP_STRING_2_STRING;

class SE_FeatureAttributeThread : public QThread
{
    Q_OBJECT

public:
    SE_FeatureAttributeThread(QObject *parent = 0);
    ~SE_FeatureAttributeThread();

    // 设置线程运行输入参数
    // qstrInputShpDataPath：    输入shp数据路径
    // qstrInputXmlPath：        xml数据路径
    // qstrOutputLogPath：       输出日志文件路径
    void SetThreadParams(QString qstrInputShpDataPath,
                         QString qstrInputXmlPath,
                         QString qstrOutputLogPath);

signals:
    
    // 返回百分比进度
    void resultProcess(const double& dPercent, const QString& s);
  

protected:
    void run() override;

    bool FilePathIsExisted(QString qstrPath);

    bool FileIsExisted(QString qstrFilePath);

    void GetFolderNameFromPath_string(string strPath, string& strFolderName);

    SE_Error FeatureAttributeCheck(const char* szInputShpPath, const char* szAttrCheckConfigXmlFile, vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo);

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

    QStringList GetSubDirPathOfCurrentDir(QString dirPath);
private:


    QMutex mutex;
    QWaitCondition condition;
    
    // 输出日志文件路径
    QString m_qstrOutputLogPath;

    // 输入shp数据路径
    QString m_qstrInputShpDataPath;

    // xml数据路径
    QString m_qstrInputXmlPath;

    bool restart;
    bool abort;

};


#endif // SE_FEATURE_ATTRIBUTE_THREAD_H
