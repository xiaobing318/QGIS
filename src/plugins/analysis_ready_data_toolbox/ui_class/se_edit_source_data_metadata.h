#ifndef SE_EDIT_SOURCE_DATA_METADATA_H
#define SE_EDIT_SOURCE_DATA_METADATA_H

#include <QDialog>

#include "commontype/se_config.h"
#include "commontype/se_commontypedef.h"
#include "ui_edit_source_data_metadata.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>

using namespace std;





class CSE_EditSrcDataMetadataDlg : public QDialog
{
	Q_OBJECT

public:
	CSE_EditSrcDataMetadataDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_EditSrcDataMetadataDlg() override;

	void restoreState();

	
private:

	Ui_EditSrcDataMetadataDlg ui;

private slots:

	// 加载模版文件
	void pushButton_LoadFile_clicked();

	// 保存元数据文件
	void pushButton_Save_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();

	// 文本框更新
	void on_InputDataPath_TextChanged();




private:

	// 获取输入文件路径
	QString GetInputDataPath();

	// 更新表格控件
	void UpdateTableWidget(QTableWidget* pWidget);

	// 从表格控件获取元数据模板信息
	void GetMetaDataInfosFromTableWidget(QTableWidget* pWidget, vector<MetaDataTemplateInfo>& vInfos);

	// 字符串编码转换
	string convertEncoding(const string& input, const string& fromEncoding, const string& toEncoding);

	// CSV文件编码转换
	void convertCsv(const string& inputFile, const string& outputFile);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);
	
    // 写元数据文件
	void WriteMetaDataToJsonFileFromTables(const string& strFile, 
		vector<MetaDataTemplateInfo>& vMetaInfo,
		int iDataTypeTabIndex);
private:

	// 输入文件路径
	QString m_qstrInputDataPath;
	
	// 输出文件路径
	QString m_qstrOutputDataPath;

	// 元数据模板
	vector<MetaDataTemplateInfo> m_vMetaDatas;
};

#endif // SE_EDIT_SOURCE_DATA_METADATA_H
