#ifndef SE_PACK_SHP_ARD
#define SE_PACK_SHP_ARD

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_pack_vector_ARD.h"
#include "../ui_task/se_pack_shp_ARD_task.h"
#include "commontype/se_analysis_ready_data_tool_def.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>

using namespace std;


class SE_PackShpARDDlg : public QDialog
{
	Q_OBJECT

public:
	SE_PackShpARDDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_PackShpARDDlg() override;

	void restoreState();


private:

	Ui_PackShpARDDlg ui;

private slots:

	// 打开分析就绪数据目录
	void pushButton_Open_clicked();

	// 打开元数据文件
	void pushButton_OpenMetaData_clicked();

	// 保存分析就绪数据包
	void pushButton_Save_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();


	// 任务完成
	void onTaskFinished(bool result);

	// 保存日志数据
	void pushButton_SaveLog_clicked();

private:

	// 输入文件路径
	QString m_qstrInputDataPath;

	// 输入元数据文件路径
	QString m_qstrInputMetaDataPath;

	// 保存文件路径
	QString m_qstrOutputDataPath;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

private:

	// 计算进度
	void CalculateTotalProgress();

	// 读取矢量分析就绪数据元数据
	bool ReadMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo);

	// 元数据存储到矢量分析就绪数据包中
	void WriteMetaDataToJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo);

	// 获取当前目录下所有子目录
	void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);
};

#endif // SE_PACK_SHP_ARD
