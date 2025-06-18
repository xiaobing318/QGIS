#ifndef SE_MODELS_DLG
#define SE_MODELS_DLG

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_models.h"

#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>

using namespace std;


class SE_ModelsDlg : public QDialog
{
	Q_OBJECT

public:
	SE_ModelsDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_ModelsDlg() override;

	void restoreState();

	

private:

	Ui_ModelsDlg ui;

private slots:

	// 加载配置文件
	void pushButton_LoadFile_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();

	// 文本框更新
	void on_InputDataPath_TextChanged();



	// 进度信息
	//void handleResults(const int& iTotalProcessed, const QString& s);


private:

	// 获取输入文件路径
	QString GetInputDataPath();

	// 输入文件路径
	QString m_qstrInputDataPath;

	// 更新TableWidget
	void UpdateTableWidget();

	// 调整表格样式
	void AdjustTableWidgetStyle();

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

private:

	// 输入模型配置文件路径
	QString m_qstrInputFilePath;

	// 模型标识
	vector<QString> m_vModelIdentifys;
	
	// 模型名称
	vector<QString> m_vModelNames;
public:


	// 获取选择的模型列表
	void GetModelInfosFromModelDlg(vector<QString>& vModelIdentifys, vector<QString>& vModelNames);
};

#endif // SE_MODELS_DLG
