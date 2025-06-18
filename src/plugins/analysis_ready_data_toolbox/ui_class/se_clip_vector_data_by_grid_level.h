#ifndef SE_CLIP_VECTOR_DATA_BY_GRID_LEVEL
#define SE_CLIP_VECTOR_DATA_BY_GRID_LEVEL

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_clip_vector_data_by_grid_level.h"
//#include "../ui_task/se_pack_tif_ARD_task.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>

using namespace std;


class SE_ClipVectorDataByGridLevelDlg : public QDialog
{
	Q_OBJECT

public:
	SE_ClipVectorDataByGridLevelDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_ClipVectorDataByGridLevelDlg() override;

	void restoreState();


private:

	Ui_ClipVectorDataByGridLevelDlg ui;

private slots:

	// 打开分析就绪数据目录
	void pushButton_Open_clicked();

	// 保存数据
	void pushButton_Save_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();

	// 保存日志数据
	void pushButton_SaveLog_clicked();

	// 任务完成
	void onTaskFinished(bool result);



private:

	// 输入文件路径
	QString m_qstrInputDataPath;

	// 保存文件路径
	QString m_qstrOutputDataPath;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

private:

	// 计算进度
	void CalculateTotalProgress();

	// 获取当前路径下所有shp文件路径
	void GetShpFilesFromFilePath(
		const string& strFilePath,
		vector<string>& vShpFilePath);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

};

#endif // SE_CLIP_VECTOR_DATA_BY_GRID_LEVEL
