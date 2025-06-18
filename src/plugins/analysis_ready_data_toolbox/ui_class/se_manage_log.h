#ifndef SE_MANAGE_LOG_H
#define SE_MANAGE_LOG_H

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_manage_log.h"
#include "commontype/se_analysis_ready_data_tool_def.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"
/*读取xml头文件*/
#include <libxml/parser.h>
#include <qstringlist.h>

using namespace std;
#include <filesystem>
namespace fs = std::filesystem;

class SE_ManageLogDlg : public QDialog
{
	Q_OBJECT

public:
	SE_ManageLogDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_ManageLogDlg() override;

	void restoreState();


private:

	Ui_ManageLogDialog ui;

private slots:

	// 添加路径
	void pushButton_AddDataPath();

	// 清空列表
	void pushButton_ClearDataPath();


	// 保存就绪数据包
	void pushButton_Save_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();


private:

	// 输入分析就绪型数据路径
	QString m_qstrInputDataPath;

	// 输入配置文件路径
	QString m_qstrConfigFilePath;

	// 保存文件路径
	QString m_qstrOutputDataPath;

	// 输入地形因子元数据路径
	QString m_qstrInputTerrainPath;

	// 输入栅格化元数据路径
	QString m_qstrInputRasterizePath;

	// 任务信息
	vector<THEME_TaskInfo> m_vTaskInfos;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

private:

	
	// 判断是否存在当前元素
	bool IsExistedInVector(vector<string>& vValues, string strValue);

	// 获取子目录
	void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

    // 复制文件夹
	bool copyDirectory(const fs::path& sourceDir, const fs::path& destinationDir);

    // 复制文件
	bool copyFile(const fs::path& sourceFilePath, const fs::path& destinationFilePath);

    // 获取txt文件
	void GetTxtFilesFromFilePath(const string& strFilePath, vector<string>& vFilePath);


};

#endif // SE_MANAGE_LOG_H
