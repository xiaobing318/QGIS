#ifndef SE_GRIDDIVISION_RULE_DLG
#define SE_GRIDDIVISION_RULE_DLG

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_grid_division_rule.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>

using namespace std;

struct GRID_DIVISION_RULE
{
	int			iBaseLevel;
	int			iDataLevel;
	string		strResolution;
	string		strGridSize;

	GRID_DIVISION_RULE()
	{
		iBaseLevel = 0;
		iDataLevel = 0;
		strResolution = "";
		strGridSize = "";
	}
};


class CSE_GridDivisionRuleDlg : public QDialog
{
	Q_OBJECT

public:
	CSE_GridDivisionRuleDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~CSE_GridDivisionRuleDlg() override;

	void restoreState();

	

private:

	Ui_GridDivisionRuleDlg ui;

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

	// 格网划分规则
	vector<GRID_DIVISION_RULE> m_vGridDivisionRule;
};

#endif // SE_GRIDDIVISION_RULE_DLG
