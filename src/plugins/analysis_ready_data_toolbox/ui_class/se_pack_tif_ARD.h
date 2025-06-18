#ifndef SE_PACK_TIF_ARD
#define SE_PACK_TIF_ARD

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_pack_raster_ARD.h"
#include "../ui_task/se_pack_tif_ARD_task.h"
#include "commontype/se_analysis_ready_data_tool_def.h"
#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>
#include <sqlite3.h>

using namespace std;


class SE_PackTifARDDlg : public QDialog
{
	Q_OBJECT

public:
	SE_PackTifARDDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_PackTifARDDlg() override;

	void restoreState();


private:

	Ui_PackTifARDDlg ui;

private slots:

	// 打开分析就绪数据目录
	void pushButton_Open_clicked();

	// 打开地形因子元数据文件
	void pushButton_OpenTerrainMetaData_clicked();

	// 打开栅格化元数据文件
	void pushButton_OpenRasterizeMetaData_clicked();

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

	// 输入地形因子元数据路径
	QString m_qstrInputTerrainPath;

	// 输入栅格化元数据路径
	QString m_qstrInputRasterizePath;

	// 保存文件路径
	QString m_qstrOutputDataPath;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

private:

	// 计算进度
	void CalculateTotalProgress();

	// 读取json元数据文件
	bool ReadMetaDataJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo);

	// 将元数据写入json文件
	void WriteMetaDataToJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo);

	// 获取子目录
	void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

};

#endif // SE_PACK_TIF_ARD
