#ifndef SE_ATTRIBUTE_EXTRACT_H
#define SE_ATTRIBUTE_EXTRACT_H

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_attribute_extract.h"
#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>

#include "attribute_extract_thread.h"

using namespace std;

/*数据提取参数结构体*/
struct AttributeExtractParam
{
	string strLayerFlag;				// 图层标识
	string strFields;					// 当前图层内字段匹配列表
	string strAttributeFilter;			// 属性过滤条件

	AttributeExtractParam()
	{
		strLayerFlag = "";
		strAttributeFilter = "";
		strFields = "";
	}
};

class SE_AttributeExtractDlg : public QDialog
{
	Q_OBJECT

public:
	SE_AttributeExtractDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_AttributeExtractDlg() override;

	void restoreState();

private:

	// 设置要素提取参数对话框UI
	Ui_SE_AttributeExtractDialog ui;

private slots:

	// 选择输入数据路径
	void pushButton_Open_clicked();

	// 选择输出数据路径
	void pushButton_Save_clicked();

	// 确定按钮
	void Button_OK_accepted();

	// 取消按钮
	void Button_Cancel_rejected();

	// 取消图层全选按钮
	void pushButton_CancelSelectAllLayers_clicked();

	// 图层全选
	void pushButton_SelectAllLayers_clicked();

	// 分幅输出复选框
	//void on_checkBox_OutputSheet_stateChanged(int arg1);

	// 加载配置文件
	void pushButton_LoadExtractParams_clicked();

	// 导出配置文件
	void pushButton_SaveExtractParams_clicked();

	// 导出配置文件模板
	void pushButton_SaveExtractParamsTemplate_clicked();

	// 输入路径改变时槽函数
	void on_InputDataPath_TextChanged(const QString& qstr);

	// 进度信息
	void handleResults(const int& iTotalProcessed, const QString& s);


private:

	// 判断是否有重复记录
	bool IsExistedInVector(AttributeExtractParam param, vector<AttributeExtractParam>& vAttributeExtractParam);


	// 根据J标图层名获取图层类型
	bool GetLayerInfo(string strLayerName, string& strSheetNumber, string& strLayerType, string& strGeometryType);

	// 根据图层名称获取提取参数
	void GetExtractParamByLayerName(string strLayerName, vector<AttributeExtractParam>& vAttributeExtractParam, AttributeExtractParam& param);

	// 获取输入数据类型
	int GetInputDataType();

	// 获取输入数据路径
	QString GetInputDataPath();

	// 获取输出数据路径
	QString GetOutputDataPath();

	// 获取待提取图层列表
	void GetLayers(vector<LayerFileInfo>& vLayers);

	// 获取图层字段设置列表
	void GetLayerFields(vector<string>& vLayerFields);

	// 获取图层属性过滤条件
	void GetLayerAttributeFilters(vector<string>& vLayerAttributeFilters);
	
	// 获取图层选择状态
	void GetLayerSelected(vector<bool>& vLayerSelected);

	// 获取当前目录的所有子目录
	QStringList GetDirPathOfSplDir(QString dirPath);

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	// 获取指定文件夹下所有的shp文件
	QStringList GetShpFileNames(const QString& path);

	// 获取GPKG中图层集合
	bool GetOGRLayerNamesFromGPKG(const QString& path, vector<string>& vLayerNames);

	// 更新TableWidget
	void UpdateTableWidget();

	// 获取数据提取参数
	void GetAttributeExtractParam(vector<AttributeExtractParam>& vAttributeExtractParam);

	// 更新数据提取参数
	void UpdateTableWidgetExtractParams();
private:

	// 输入数据路径（图幅文件夹形式）
	QString m_qstrInputDataPath;

	// 输入gpkg数据路径（数据库形式）
	QStringList m_qstrInputDataPathList;

	// 输出数据路径
	QString m_qstrOutputDataPath;

	// 待提取的图层路径列表
	QStringList m_qstrFilePathList;

	// 图层对象列表
	vector<OGRLayer*> m_vOGRLayers;

	// 数据提取配置参数，包括图层名称及对应的字段及属性筛选条件
	vector<AttributeExtractParam> m_vAttributeExtractParam;

	// 数据提取工具线程
	SE_AttributeExtractThread m_AttributeExtractThread;

	// 待处理的图层个数
	int m_iTotalLayerCount;
};

#endif // SE_ATTRIBUTE_EXTRACT_H
