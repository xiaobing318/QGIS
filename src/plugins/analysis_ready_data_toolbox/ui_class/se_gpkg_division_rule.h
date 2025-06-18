#ifndef SE_GPKG_DIVISION_RULE
#define SE_GPKG_DIVISION_RULE

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_gpkg_division_rule.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>

using namespace std;

/*数据包分包规则*/
struct GPKG_DIVISION_RULE
{
	int			iMinLevel;		// 包内最小级别
	int			iMaxLevel;		// 包内最大级别
	int			iBaseLevel;		// 基础分包级别

	GPKG_DIVISION_RULE()
	{
		iMinLevel = 0;
		iMaxLevel = 0;
		iBaseLevel = 0;
	}
};

class SE_GpkgDivisionRuleDlg : public QDialog
{
	Q_OBJECT

public:
	SE_GpkgDivisionRuleDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_GpkgDivisionRuleDlg() override;

	void restoreState();

	

private:

	Ui_GpkgDivisionRuleDlg ui;

private slots:

	// 加载配置文件
	void pushButton_LoadFile_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();

	// 文本框更新
	void on_InputDataPath_TextChanged();


private:

	// 获取输入文件路径
	QString GetInputDataPath();

	// 输入文件路径
	QString m_qstrInputDataPath;

	// 更新TableWidget
	void UpdateTableWidget();

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

private:

	// 输入格网划分规则文件路径
	QString m_qstrInputFilePath;

	// 分包配置规则
	vector<GPKG_DIVISION_RULE> m_vGpkgDivisionRule;
};

#endif // SE_GPKG_DIVISION_RULE
