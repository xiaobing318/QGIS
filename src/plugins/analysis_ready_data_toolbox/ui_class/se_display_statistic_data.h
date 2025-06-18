#ifndef SE_DISPLAY_STATISTIC_DATA_H
#define SE_DISPLAY_STATISTIC_DATA_H

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_display_statistic_data.h"
#include "commontype/se_analysis_ready_data_tool_def.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"
/*读取xml头文件*/
#include <libxml/parser.h>
#include <qstringlist.h>
#include <filesystem>

using namespace std;

namespace fs = std::filesystem;

// 定义一个结构体来存储文件数量和总大小
struct FileStats {
	int fileCount = 0;
	long long totalSize = 0;
};

class SE_DisplayStatisticDataDlg : public QDialog
{
	Q_OBJECT

public:
	SE_DisplayStatisticDataDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_DisplayStatisticDataDlg() override;

	void restoreState();


private:

	Ui_DisplayStatisticDataDialog ui;

private slots:

	// 添加数据包所在路径
	void pushButton_AddDataPath();

	// 清空目录列表
	void pushButton_ClearDataPath();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();


private:


private:

	// 判断是否存在当前元素
	bool IsExistedInVector(vector<string>& vValues, string strValue);

	// 获取子目录
	void GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir);

	// 递归统计文件夹下文件信息的函数
	FileStats countFilesAndSize(const fs::path& directory);

};

#endif // SE_DISPLAY_STATISTIC_DATA_H
