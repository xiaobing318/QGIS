#ifndef QGS_SET_ATTRIBUTE_EXTRACT_PARAMS_H
#define QGS_SET_ATTRIBUTE_EXTRACT_PARAMS_H

#include <QDialog>

#include "ui_set_attribute_extract_params.h"
#include "cse_geoextractandprocess.h"
#include <vector>

using namespace std;

/*要素类名称-编码映射*/
typedef map<QString, QString> MAP_FeatureClassName_Code;

/*要素类编码-名称映射*/
typedef map<QString, QString> MAP_FeatureClassCode_Name;

class QgsSetAttributeExtractParamsDlg : public QDialog
{
	Q_OBJECT

public:
	QgsSetAttributeExtractParamsDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsSetAttributeExtractParamsDlg() override;

	void restoreState();

private:

	// 设置要素提取参数对话框UI
	Ui_QgsSetAttributeExtractParams ui;

private slots:

	// 选择输入数据路径
	void pushButton_Open_clicked();

	// 选择输出数据路径
	void pushButton_Save_clicked();

	// 加载配置文件
	void pushButton_LoadExtractParams_clicked();

	// 另存配置文件
	void pushButton_SaveExtractParams_clicked();

	// 图层全选，要素和字段全选
	void pushButton_SelectAllLayers_clicked();

	// 确定按钮
	void Button_OK_accepted();

	// 取消按钮
	void Button_Cancel_rejected();

	// 帮助按钮
	void Button_Help_showHelp();

	//-------------A---------------//
	// 图层A的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_A_clicked();

	// 图层A的字段全选按钮
	void pushButton_SelectAllFields_A_clicked();

	// 图层A的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_A_clicked();

	// 图层A的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_A_clicked();

	//-------------B---------------//
	// 图层B的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_B_clicked();

	// 图层B的字段全选按钮
	void pushButton_SelectAllFields_B_clicked();

	// 图层B的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_B_clicked();

	// 图层B的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_B_clicked();

	//-------------C---------------//
	// 图层C的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_C_clicked();

	// 图层C的字段全选按钮
	void pushButton_SelectAllFields_C_clicked();

	// 图层C的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_C_clicked();

	// 图层C的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_C_clicked();

	//-------------D---------------//
	// 图层D的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_D_clicked();

	// 图层D的字段全选按钮
	void pushButton_SelectAllFields_D_clicked();

	// 图层D的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_D_clicked();

	// 图层D的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_D_clicked();

	//-------------E---------------//
	// 图层E的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_E_clicked();

	// 图层E的字段全选按钮
	void pushButton_SelectAllFields_E_clicked();

	// 图层E的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_E_clicked();

	// 图层E的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_E_clicked();

	//-------------F---------------//
	// 图层F的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_F_clicked();

	// 图层F的字段全选按钮
	void pushButton_SelectAllFields_F_clicked();

	// 图层F的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_F_clicked();

	// 图层F的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_F_clicked();

	//-------------G---------------//
	// 图层G的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_G_clicked();

	// 图层G的字段全选按钮
	void pushButton_SelectAllFields_G_clicked();

	// 图层G的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_G_clicked();

	// 图层G的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_G_clicked();

	//-------------H---------------//
	// 图层H的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_H_clicked();

	// 图层H的字段全选按钮
	void pushButton_SelectAllFields_H_clicked();

	// 图层H的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_H_clicked();

	// 图层H的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_H_clicked();

	//-------------I---------------//
	// 图层I的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_I_clicked();

	// 图层I的字段全选按钮
	void pushButton_SelectAllFields_I_clicked();

	// 图层I的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_I_clicked();

	// 图层I的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_I_clicked();

	//-------------J---------------//
	// 图层J的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_J_clicked();

	// 图层J的字段全选按钮
	void pushButton_SelectAllFields_J_clicked();

	// 图层J的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_J_clicked();

	// 图层J的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_J_clicked();

	//-------------K---------------//
	// 图层K的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_K_clicked();

	// 图层K的字段全选按钮
	void pushButton_SelectAllFields_K_clicked();

	// 图层K的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_K_clicked();

	// 图层K的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_K_clicked();

	//-------------L---------------//
	// 图层L的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_L_clicked();

	// 图层L的字段全选按钮
	void pushButton_SelectAllFields_L_clicked();

	// 图层L的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_L_clicked();

	// 图层L的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_L_clicked();

	//-------------M---------------//
	// 图层M的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_M_clicked();

	// 图层M的字段全选按钮
	void pushButton_SelectAllFields_M_clicked();

	// 图层M的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_M_clicked();

	// 图层M的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_M_clicked();

	//-------------N---------------//
	// 图层N的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_N_clicked();

	// 图层N的字段全选按钮
	void pushButton_SelectAllFields_N_clicked();

	// 图层N的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_N_clicked();

	// 图层N的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_N_clicked();

	//-------------O---------------//
	// 图层O的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_O_clicked();

	// 图层O的字段全选按钮
	void pushButton_SelectAllFields_O_clicked();

	// 图层O的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_O_clicked();

	// 图层O的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_O_clicked();

	//-------------P---------------//
	// 图层P的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_P_clicked();

	// 图层P的字段全选按钮
	void pushButton_SelectAllFields_P_clicked();

	// 图层P的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_P_clicked();

	// 图层P的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_P_clicked();

	//-------------Q---------------//
	// 图层Q的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_Q_clicked();

	// 图层Q的字段全选按钮
	void pushButton_SelectAllFields_Q_clicked();

	// 图层Q的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_Q_clicked();

	// 图层Q的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_Q_clicked();

	//-------------R---------------//
	// 图层R的要素类全选按钮
	void pushButton_SelectAllFeatureClasses_R_clicked();

	// 图层R的字段全选按钮
	void pushButton_SelectAllFields_R_clicked();

	// 图层R的要素类全部不☑按钮
	void pushButton_CancelSelectAllFeatureClasses_R_clicked();

	// 图层R的字段全部不☑按钮
	void pushButton_CancelSelectAllFields_R_clicked();

	//-----------------------------//
	// 取消图层全选按钮
	void pushButton_CancelSelectAllLayers_clicked();

	// 分幅输出复选框
	void on_checkBox_OutputSheet_stateChanged(int arg1);

public:

	// 获取输入数据类型
	int GetInputDataType();

	// 获取输入数据路径
	QString GetInputDataPath();

	// 获取输出数据类型
	int GetOutputDataType();

	// 获取输出数据路径
	QString GetOutputDataPath();

	// 获取图层匹配参数
	void GetExtractDataParams(vector<ExtractDataParam>& vExtractDataParam);

	// 获取分幅提取单选按钮状态
	bool GetCheckState();

	// 获取分幅比例尺：1— 1:100万；2— 1:50万； 3— 1:25万；4— 1:10万；
	//                 5— 1:5万；	6— 1:2.5万；7— 1:1万； 8— 1:5千
	int GetSacleType();

private:

	// 获取各图层字段选择列表
	// strLayerType:图层名称
	// vFields：选中的提取字段列表，如果列表为空，则说明当前图层不进行匹配
	void GetLayerFieldsChecked(string strLayerType, vector<string>& vFields);

	// 获取各图层要素类选择列表
	// strLayerType:图层名称
	// vFields：选中的要素类编码列表，如果列表为空，则说明当前图层不进行提取
	void GetLayerFeatureClassChecked(string strLayerType, vector<string>& vFeatureClassCode);

	// 设置各图层字段复选框状态
	void SetCheckBoxStatus(ExtractDataParam& param);

	// 设置各图层要素类列表框状态
	void SetListWidgetStatus(ExtractDataParam& param);

	// 初始化要素类名称和编码的映射表
	void InitFeatureClassNameCodeTable();

	// 判断列表中元素是否在要素类列表中
	bool bIsInFeatureClasses(QString qstrText, vector<string>& vFeatureClassCode);

	// UTF-8编码转GBK
	string UTF8_To_GBK(const string& str);

	// 根据图层名称设置要素类列表框全选状态
	void SetListWidgetAllSelectedStatus(string strLayerName);

	// 根据图层名称设置字段复选框状态
	void SetCheckBoxAllSelectedStatus(string strLayerName);

	// 根据图层名称设置要素类列表框全部不选中状态（初始化）
	void SetListWidgetAllUnSelectedStatus(string strLayerName);

	// 根据图层名称设置字段复选框全部不☑状态
	void SetCheckBoxAllUnSelectedStatus(string strLayerName);

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);



private:

	// 编码—名称映射表
	MAP_FeatureClassCode_Name m_mCode_Names;

	// 名称—编码映射表
	MAP_FeatureClassName_Code m_mName_Codes;

	// 输入数据路径
	QString m_qstrInputDataPath;

	// 输出数据路径
	QString m_qstrOutputDataPath;

	// 图层A到R
	vector<string> m_vStrLayerName;
};

#endif // QGS_SET_ATTRIBUTE_EXTRACT_PARAMS_H
