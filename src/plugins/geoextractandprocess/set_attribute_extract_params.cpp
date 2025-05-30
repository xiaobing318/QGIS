#include "set_attribute_extract_params.h"

#include "qgshelp.h"
#include "qgsmaplayer.h"
#include "qgsproject.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayer.h"
#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsgui.h"

#include <QFileDialog>
#include <QMessageBox>

#include <QFile>
#include <QtXml\QtXml>
#include <QtXml\QDomDocument>
#include "cpl_config.h"

#include "gdal_priv.h"

#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>

#define _atoi64(val)   strtoll(val,NULL,10)

#endif //

using namespace std;

QgsSetAttributeExtractParamsDlg::QgsSetAttributeExtractParamsDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);
	ui.tabWidget_LayerType->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_A->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_B->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_C->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_D->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_E->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_F->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_G->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_H->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_I->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_J->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_K->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_L->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_M->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_N->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_O->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_P->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_Q->setCurrentIndex(0);
	ui.tabWidget_FeatureClass_R->setCurrentIndex(0);

	// 默认比例尺不可选
	ui.comboBox_Scale->setEnabled(false);
	ui.radioButton_InputDB->setChecked(true);

	connect(ui.pushButton_LoadExtractParams, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_LoadExtractParams_clicked);
	connect(ui.pushButton_SaveExtractParams, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SaveExtractParams_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::Button_Cancel_rejected);
	
	connect(ui.pushButton_Open, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_Open_clicked);
	connect(ui.pushButton_Save, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_Save_clicked);
	connect(ui.pushButton_SelectAllLayers, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllLayers_clicked);

	//---------------各图层的要素类和字段全选按钮关联--------------------------------//
	connect(ui.pushButton_SelectAllFeatureClasses_A, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_A_clicked);
	connect(ui.pushButton_SelectAllFields_A, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_A_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_B, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_B_clicked);
	connect(ui.pushButton_SelectAllFields_B, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_B_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_C, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_C_clicked);
	connect(ui.pushButton_SelectAllFields_C, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_C_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_D, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_D_clicked);
	connect(ui.pushButton_SelectAllFields_D, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_D_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_E, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_E_clicked);
	connect(ui.pushButton_SelectAllFields_E, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_E_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_F, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_F_clicked);
	connect(ui.pushButton_SelectAllFields_F, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_F_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_G, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_G_clicked);
	connect(ui.pushButton_SelectAllFields_G, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_G_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_H, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_H_clicked);
	connect(ui.pushButton_SelectAllFields_H, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_H_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_I, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_I_clicked);
	connect(ui.pushButton_SelectAllFields_I, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_I_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_J, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_J_clicked);
	connect(ui.pushButton_SelectAllFields_J, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_J_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_K, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_K_clicked);
	connect(ui.pushButton_SelectAllFields_K, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_K_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_L, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_L_clicked);
	connect(ui.pushButton_SelectAllFields_L, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_L_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_M, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_M_clicked);
	connect(ui.pushButton_SelectAllFields_M, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_M_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_N, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_N_clicked);
	connect(ui.pushButton_SelectAllFields_N, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_N_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_O, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_O_clicked);
	connect(ui.pushButton_SelectAllFields_O, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_O_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_P, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_P_clicked);
	connect(ui.pushButton_SelectAllFields_P, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_P_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_Q, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_Q_clicked);
	connect(ui.pushButton_SelectAllFields_Q, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_Q_clicked);

	connect(ui.pushButton_SelectAllFeatureClasses_R, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_R_clicked);
	connect(ui.pushButton_SelectAllFields_R, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_R_clicked);

	//-------------------------------------------------------------------------------//
	connect(ui.pushButton_CancelSelectAllLayers, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllLayers_clicked);

	//-------------------------------------------------------------------------------//

	//---------------各图层的要素类和字段全不☑按钮关联--------------------------------//
	connect(ui.pushButton_CancelSelectAllFeatureClasses_A, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_A_clicked);
	connect(ui.pushButton_CancelSelectAllFields_A, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_A_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_B, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_B_clicked);
	connect(ui.pushButton_CancelSelectAllFields_B, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_B_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_C, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_C_clicked);
	connect(ui.pushButton_CancelSelectAllFields_C, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_C_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_D, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_D_clicked);
	connect(ui.pushButton_CancelSelectAllFields_D, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_D_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_E, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_E_clicked);
	connect(ui.pushButton_CancelSelectAllFields_E, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_E_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_F, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_F_clicked);
	connect(ui.pushButton_CancelSelectAllFields_F, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_F_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_G, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_G_clicked);
	connect(ui.pushButton_CancelSelectAllFields_G, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_G_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_H, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_H_clicked);
	connect(ui.pushButton_CancelSelectAllFields_H, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_H_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_I, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_I_clicked);
	connect(ui.pushButton_CancelSelectAllFields_I, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_I_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_J, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_J_clicked);
	connect(ui.pushButton_CancelSelectAllFields_J, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_J_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_K, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_K_clicked);
	connect(ui.pushButton_CancelSelectAllFields_K, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_K_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_L, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_L_clicked);
	connect(ui.pushButton_CancelSelectAllFields_L, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_L_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_M, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_M_clicked);
	connect(ui.pushButton_CancelSelectAllFields_M, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_M_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_N, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_N_clicked);
	connect(ui.pushButton_CancelSelectAllFields_N, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_N_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_O, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_O_clicked);
	connect(ui.pushButton_CancelSelectAllFields_O, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_O_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_P, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_P_clicked);
	connect(ui.pushButton_CancelSelectAllFields_P, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_P_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_Q, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_Q_clicked);
	connect(ui.pushButton_CancelSelectAllFields_Q, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_Q_clicked);

	connect(ui.pushButton_CancelSelectAllFeatureClasses_R, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_R_clicked);
	connect(ui.pushButton_CancelSelectAllFields_R, &QPushButton::clicked, this, &QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_R_clicked);


	connect(ui.checkBox_OutputSheet, &QCheckBox::stateChanged, this, &QgsSetAttributeExtractParamsDlg::on_checkBox_OutputSheet_stateChanged);

	m_mCode_Names.clear();
	m_mName_Codes.clear();

	// 初始化要素类编码名称映射表
	InitFeatureClassNameCodeTable();

	// 图层名列表:A到R
	m_vStrLayerName.push_back("A");
	m_vStrLayerName.push_back("B");
	m_vStrLayerName.push_back("C");
	m_vStrLayerName.push_back("D");
	m_vStrLayerName.push_back("E");

	m_vStrLayerName.push_back("F");
	m_vStrLayerName.push_back("G");
	m_vStrLayerName.push_back("H");
	m_vStrLayerName.push_back("I");
	m_vStrLayerName.push_back("J");

	m_vStrLayerName.push_back("K");
	m_vStrLayerName.push_back("L");
	m_vStrLayerName.push_back("M");
	m_vStrLayerName.push_back("N");
	m_vStrLayerName.push_back("O");

	m_vStrLayerName.push_back("P");
	m_vStrLayerName.push_back("Q");
	m_vStrLayerName.push_back("R");

	// 初始化所有要素类列表、字段列表为不选中状态
	for (int i = 0; i < m_vStrLayerName.size(); i++)
	{
		SetListWidgetAllUnSelectedStatus(m_vStrLayerName[i]);
		SetCheckBoxAllUnSelectedStatus(m_vStrLayerName[i]);
	}

	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputDataPath->setText(m_qstrOutputDataPath);
}

QgsSetAttributeExtractParamsDlg::~QgsSetAttributeExtractParamsDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("SetAttributeExtractParams/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("SetAttributeExtractParams/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
}

void QgsSetAttributeExtractParamsDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("SetAttributeExtractParams/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("SetAttributeExtractParams/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}

// 获取输入数据类型
int QgsSetAttributeExtractParamsDlg::GetInputDataType()
{
	// 如果输入数据源为数据库
	if (ui.radioButton_InputDB->isChecked())
	{
		return 1;
	}
	else
	{
		return 2;
	}
}

// 获取输入数据路径
QString QgsSetAttributeExtractParamsDlg::GetInputDataPath()
{
	return m_qstrInputDataPath;
}

// 获取输出数据类型
int QgsSetAttributeExtractParamsDlg::GetOutputDataType()
{
	// 输出本地文件
	return 2;
	// 如果输出数据源为数据库
	/*if (ui.radioButton_OutputDB->isChecked())
	{
		return 1;
	}
	else
	{
		return 2;
	}*/
}

// 获取输出数据路径
QString QgsSetAttributeExtractParamsDlg::GetOutputDataPath()
{
	return m_qstrOutputDataPath;
}

void QgsSetAttributeExtractParamsDlg::GetExtractDataParams(vector<ExtractDataParam>& vExtractDataParam)
{
	vExtractDataParam.clear();

	// 要素提取参数，从A图层到R图层
	vector<string> vLayerType;
	vLayerType.clear();
	vLayerType.push_back("A");
	vLayerType.push_back("B");
	vLayerType.push_back("C");
	vLayerType.push_back("D");
	vLayerType.push_back("E");
	vLayerType.push_back("F");

	vLayerType.push_back("G");
	vLayerType.push_back("H");
	vLayerType.push_back("I");
	vLayerType.push_back("J");
	vLayerType.push_back("K");

	vLayerType.push_back("L");
	vLayerType.push_back("M");
	vLayerType.push_back("N");
	vLayerType.push_back("O");
	vLayerType.push_back("P");
	vLayerType.push_back("Q");
	vLayerType.push_back("R");

	// 循环获取每个图层的字段选择状态
	for (int i = 0; i < vLayerType.size(); i++)
	{
		ExtractDataParam param;
		param.strLayerName = vLayerType[i];

		// 获取图层字段选择状态
		GetLayerFieldsChecked(vLayerType[i], param.vFields);

		// 获取图层要素类选择状态
		GetLayerFeatureClassChecked(vLayerType[i], param.vFeatureClassCode);

		vExtractDataParam.push_back(param);
	}
}

bool QgsSetAttributeExtractParamsDlg::GetCheckState()
{
	// 如果分幅输出按钮选中
	if (ui.checkBox_OutputSheet->isChecked())
	{
		return true;
	}
	else
	{
		return false;
	}
}

int QgsSetAttributeExtractParamsDlg::GetSacleType()
{
	// 1— 1:100万；2— 1 : 50万； 3— 1 : 25万；4— 1 : 10万；
	// 5— 1:5万；	6— 1:2.5万；  7— 1:1万；   8— 1:5千

	// 1:100万
	if (ui.comboBox_Scale->currentIndex() == 0)
	{
		return 1;
	}

	// 1:50万
	else if (ui.comboBox_Scale->currentIndex() == 1)
	{
		return 2;
	}

	// 1:25万
	else if (ui.comboBox_Scale->currentIndex() == 2)
	{
		return 3;
	}

	// 1:10万
	else if (ui.comboBox_Scale->currentIndex() == 3)
	{
		return 4;
	}

	// 1:5万
	else if (ui.comboBox_Scale->currentIndex() == 4)
	{
		return 5;
	}

	// 1:2.5万
	else if (ui.comboBox_Scale->currentIndex() == 5)
	{
		return 6;
	}

	// 1:1万
	else if (ui.comboBox_Scale->currentIndex() == 6)
	{
		return 7;
	}

	// 1:5千
	else if (ui.comboBox_Scale->currentIndex() == 7)
	{
		return 8;
	}

	return 0;
}

// 获取图层字段选择状态
void QgsSetAttributeExtractParamsDlg::GetLayerFieldsChecked(string strLayerType, vector<string>& vFields)
{
	vFields.clear();
	/*根据图层类型，获取字段选中状态，将选中的字段加入到列表中*/

	// 测量控制点
	if (strLayerType == "A")
	{
		if (ui.checkBox_YaoSuBianHao_A->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_A->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_A->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_A->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_DengJi_A->isChecked())
		{
			vFields.push_back("等级");
		}
		if (ui.checkBox_GaoCheng_A->isChecked())
		{
			vFields.push_back("高程");
		}
		if (ui.checkBox_BiGao_A->isChecked())
		{
			vFields.push_back("比高");
		}
		if (ui.checkBox_LiLunHengZuoBiao_A->isChecked())
		{
			vFields.push_back("理论横坐标");
		}
		if (ui.checkBox_LiLunZongZuoBiao_A->isChecked())
		{
			vFields.push_back("理论纵坐标");
		}
		if (ui.checkBox_TuXingTeZheng_A->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_A->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_A->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 工农业社会文化设施
	if (strLayerType == "B")
	{
		if (ui.checkBox_YaoSuBianHao_B->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_B->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_B->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_B->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_LeiBie_B->isChecked())
		{
			vFields.push_back("类别");
		}
		if (ui.checkBox_GaoCheng_B->isChecked())
		{
			vFields.push_back("高程");
		}
		if (ui.checkBox_BiGao_B->isChecked())
		{
			vFields.push_back("比高");
		}
		if (ui.checkBox_XingZhengQuHua_B->isChecked())
		{
			vFields.push_back("行政区划");
		}
		if (ui.checkBox_TuXingTeZheng_B->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_B->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_B->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 居民地及附属设施
	if (strLayerType == "C")
	{
		if (ui.checkBox_YaoSuBianHao_C->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_C->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_C->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_C->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_JuZhuYueFen_C->isChecked())
		{
			vFields.push_back("居住月份");
		}
		if (ui.checkBox_RenKou_C->isChecked())
		{
			vFields.push_back("人口");
		}
		if (ui.checkBox_BiGao_C->isChecked())
		{
			vFields.push_back("比高");
		}
		if (ui.checkBox_XingZhengQuHua_C->isChecked())
		{
			vFields.push_back("行政区划");
		}
		if (ui.checkBox_TuXingTeZheng_C->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_C->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_C->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 陆地交通
	if (strLayerType == "D")
	{
		if (ui.checkBox_YaoSuBianHao_D->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_D->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_D->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_D->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_BianHao_D->isChecked())
		{
			vFields.push_back("编号");
		}
		if (ui.checkBox_DengJi_D->isChecked())
		{
			vFields.push_back("等级");
		}
		if (ui.checkBox_KuanDu_D->isChecked())
		{
			vFields.push_back("宽度");
		}
		if (ui.checkBox_PuKuan_D->isChecked())
		{
			vFields.push_back("铺宽");
		}
		if (ui.checkBox_QiaoChang_D->isChecked())
		{
			vFields.push_back("桥长");
		}
		if (ui.checkBox_JingKongGao_D->isChecked())
		{
			vFields.push_back("净空高");
		}
		if (ui.checkBox_ZaiZhongDunShu_D->isChecked())
		{
			vFields.push_back("载重吨数");
		}
		if (ui.checkBox_LiCheng_D->isChecked())
		{
			vFields.push_back("里程");
		}
		if (ui.checkBox_BiGao_D->isChecked())
		{
			vFields.push_back("比高");
		}
		if (ui.checkBox_TongXingYueFen_D->isChecked())
		{
			vFields.push_back("通行月份");
		}
		if (ui.checkBox_ShuiShen_D->isChecked())
		{
			vFields.push_back("水深");
		}
		if (ui.checkBox_DiZhi_D->isChecked())
		{
			vFields.push_back("底质");
		}
		if (ui.checkBox_QuLvBanJing_D->isChecked())
		{
			vFields.push_back("曲率半径");
		}
		if (ui.checkBox_ZuiDaZongPo_D->isChecked())
		{
			vFields.push_back("最大纵坡");
		}
		if (ui.checkBox_TuXingTeZheng_D->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_D->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_D->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 管线
	if (strLayerType == "E")
	{
		if (ui.checkBox_YaoSuBianHao_E->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_E->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_E->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_E->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_JingKongGao_E->isChecked())
		{
			vFields.push_back("净空高");
		}
		if (ui.checkBox_MaiCangShenDu_E->isChecked())
		{
			vFields.push_back("埋藏深度");
		}
		if (ui.checkBox_CunZaiZhuangTai_E->isChecked())
		{
			vFields.push_back("存在状态");
		}
		if (ui.checkBox_ZuoYongFangShi_E->isChecked())
		{
			vFields.push_back("作用方式");
		}
		if (ui.checkBox_XianZhiZhongLei_E->isChecked())
		{
			vFields.push_back("限制种类");
		}
		if (ui.checkBox_TuXingTeZheng_E->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_E->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_E->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 水域/陆地
	if (strLayerType == "F")
	{
		if (ui.checkBox_YaoSuBianHao_F->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_F->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_F->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_F->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_KuanDu_F->isChecked())
		{
			vFields.push_back("宽度");
		}
		if (ui.checkBox_ShuiShen_F->isChecked())
		{
			vFields.push_back("水深");
		}
		if (ui.checkBox_NiShen_F->isChecked())
		{
			vFields.push_back("泥深");
		}
		if (ui.checkBox_ShiLingYueFen_F->isChecked())
		{
			vFields.push_back("时令月份");
		}
		if (ui.checkBox_ChangDu_F->isChecked())
		{
			vFields.push_back("长度");
		}
		if (ui.checkBox_GaoCheng_F->isChecked())
		{
			vFields.push_back("高程");
		}
		if (ui.checkBox_BiGao_F->isChecked())
		{
			vFields.push_back("比高");
		}
		if (ui.checkBox_KuRongLiang_F->isChecked())
		{
			vFields.push_back("库容量");
		}
		if (ui.checkBox_DunWei_F->isChecked())
		{
			vFields.push_back("吨位");
		}
		if (ui.checkBox_HeLiuDaiMa_F->isChecked())
		{
			vFields.push_back("河流代码");
		}
		if (ui.checkBox_TongHangXingZhi_F->isChecked())
		{
			vFields.push_back("通航性质");
		}
		if (ui.checkBox_ZhiWuLeiXing_F->isChecked())
		{
			vFields.push_back("植物类型");
		}
		if (ui.checkBox_BiaoMianWuZhi_F->isChecked())
		{
			vFields.push_back("表面物质");
		}
		if (ui.checkBox_CunZaiZhuangTai_F->isChecked())
		{
			vFields.push_back("存在状态");
		}
		if (ui.checkBox_WeiZhiZhiLiang_F->isChecked())
		{
			vFields.push_back("位置质量");
		}
		if (ui.checkBox_YanSe_F->isChecked())
		{
			vFields.push_back("颜色");
		}
		if (ui.checkBox_YuShuiMianGuanXi_F->isChecked())
		{
			vFields.push_back("与水面关系");
		}
		if (ui.checkBox_TuXingTeZheng_F->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_F->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_F->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 海底地貌及底质
	if (strLayerType == "G")
	{
		if (ui.checkBox_YaoSuBianHao_G->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_G->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_G->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_G->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_ShuiShenZhi_G->isChecked())
		{
			vFields.push_back("水深值");
		}
		if (ui.checkBox_ShuiShenZhi1_G->isChecked())
		{
			vFields.push_back("水深值1");
		}
		if (ui.checkBox_ShuiShenZhi2_G->isChecked())
		{
			vFields.push_back("水深值2");
		}
		if (ui.checkBox_CeShenJiShu_G->isChecked())
		{
			vFields.push_back("测深技术");
		}
		if (ui.checkBox_CeShenZhiLiang_G->isChecked())
		{
			vFields.push_back("测深质量");
		}
		if (ui.checkBox_WeiZhiZhiLiang_G->isChecked())
		{
			vFields.push_back("位置质量");
		}
		if (ui.checkBox_ZuoYongFangShi_G->isChecked())
		{
			vFields.push_back("作用方式");
		}
		if (ui.checkBox_BiaoMianWuZhi_G->isChecked())
		{
			vFields.push_back("表面物质");
		}
		if (ui.checkBox_WuZhiXingTai_G->isChecked())
		{
			vFields.push_back("物质形态");
		}
		if (ui.checkBox_WeiXianJi_G->isChecked())
		{
			vFields.push_back("危险级");
		}
		if (ui.checkBox_TuXingTeZheng_G->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_G->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_G->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 礁石、沉船、障碍物
	if (strLayerType == "H")
	{
		if (ui.checkBox_YaoSuBianHao_H->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_H->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_H->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_H->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_ShuiShenZhi_H->isChecked())
		{
			vFields.push_back("水深值");
		}
		if (ui.checkBox_GaoDu_H->isChecked())
		{
			vFields.push_back("高度");
		}
		if (ui.checkBox_ChuiGao_H->isChecked())
		{
			vFields.push_back("垂高");
		}
		if (ui.checkBox_CunZaiZhuangTai_H->isChecked())
		{
			vFields.push_back("存在状态");
		}
		if (ui.checkBox_CeShenJiShu_H->isChecked())
		{
			vFields.push_back("测深技术");
		}
		if (ui.checkBox_CeShenZhiLiang_H->isChecked())
		{
			vFields.push_back("测深质量");
		}
		if (ui.checkBox_WeiZhiZhiLiang_H->isChecked())
		{
			vFields.push_back("位置质量");
		}
		if (ui.checkBox_XianZhiZhongLei_H->isChecked())
		{
			vFields.push_back("限制种类");
		}
		if (ui.checkBox_ZuoYongFangShi_H->isChecked())
		{
			vFields.push_back("作用方式");
		}
		if (ui.checkBox_BiaoMianWuZhi_H->isChecked())
		{
			vFields.push_back("表面物质");
		}
		if (ui.checkBox_YuShuiMianGuanXi_H->isChecked())
		{
			vFields.push_back("与水面关系");
		}
		if (ui.checkBox_WeiXianJi_H->isChecked())
		{
			vFields.push_back("危险级");
		}
		if (ui.checkBox_TuXingTeZheng_H->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_H->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_H->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 水文
	if (strLayerType == "I")
	{
		if (ui.checkBox_YaoSuBianHao_I->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_I->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_I->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_KuanDu_I->isChecked())
		{
			vFields.push_back("宽度");
		}
		if (ui.checkBox_ShuiShen_I->isChecked())
		{
			vFields.push_back("水深");
		}
		if (ui.checkBox_DiZhi_I->isChecked())
		{
			vFields.push_back("底质");
		}
		if (ui.checkBox_LiuSu_I->isChecked())
		{
			vFields.push_back("流速");
		}
		if (ui.checkBox_FangWei_I->isChecked())
		{
			vFields.push_back("方位");
		}
		if (ui.checkBox_GaoCheng_I->isChecked())
		{
			vFields.push_back("高程");
		}
		if (ui.checkBox_YueFen_I->isChecked())
		{
			vFields.push_back("月份");
		}
		if (ui.checkBox_MiaoShu_I->isChecked())
		{
			vFields.push_back("描述");
		}
		if (ui.checkBox_TuXingTeZheng_I->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_I->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_I->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 陆地地貌及土质
	if (strLayerType == "J")
	{
		if (ui.checkBox_YaoSuBianHao_J->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_J->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_J->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_J->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_LeiBie_J->isChecked())
		{
			vFields.push_back("类别");
		}
		if (ui.checkBox_GaoCheng_J->isChecked())
		{
			vFields.push_back("高程");
		}
		if (ui.checkBox_BiGao_J->isChecked())
		{
			vFields.push_back("比高");
		}
		if (ui.checkBox_GouKuan_J->isChecked())
		{
			vFields.push_back("沟宽");
		}
		if (ui.checkBox_FangXiang_J->isChecked())
		{
			vFields.push_back("方向");
		}
		if (ui.checkBox_TuXingTeZheng_J->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_J->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_J->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 境界与政区
	if (strLayerType == "K")
	{
		if (ui.checkBox_YaoSuBianHao_K->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_K->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_K->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_K->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_DaiMa_K->isChecked())
		{
			vFields.push_back("代码");
		}
		if (ui.checkBox_BianHao_K->isChecked())
		{
			vFields.push_back("编号");
		}
		if (ui.checkBox_XuHao_K->isChecked())
		{
			vFields.push_back("序号");
		}
		if (ui.checkBox_GaoCheng_K->isChecked())
		{
			vFields.push_back("高程");
		}
		if (ui.checkBox_BiGao_K->isChecked())
		{
			vFields.push_back("比高");
		}
		if (ui.checkBox_TuXingTeZheng_K->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_K->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_K->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 植被
	if (strLayerType == "L")
	{
		if (ui.checkBox_YaoSuBianHao_L->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_L->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_L->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_L->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_GaoCheng_L->isChecked())
		{
			vFields.push_back("高程");
		}
		if (ui.checkBox_GaoDu_L->isChecked())
		{
			vFields.push_back("高度");
		}
		if (ui.checkBox_XiongJing_L->isChecked())
		{
			vFields.push_back("胸径");
		}
		if (ui.checkBox_KuanDu_L->isChecked())
		{
			vFields.push_back("宽度");
		}
		if (ui.checkBox_TuXingTeZheng_L->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_L->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_L->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 地磁要素
	if (strLayerType == "M")
	{
		if (ui.checkBox_YaoSuBianHao_M->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_M->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_M->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_CiChaZhi_M->isChecked())
		{
			vFields.push_back("磁差值");
		}
		if (ui.checkBox_CanKaoNian_M->isChecked())
		{
			vFields.push_back("参考年");
		}
		if (ui.checkBox_NianBianZhi_M->isChecked())
		{
			vFields.push_back("年变值");
		}
		if (ui.checkBox_TuXingTeZheng_M->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_M->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_M->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 助航设备及航道
	if (strLayerType == "N")
	{
		if (ui.checkBox_YaoSuBianHao_N->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_N->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_N->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_N->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_XingZhi_N->isChecked())
		{
			vFields.push_back("性质");
		}
		if (ui.checkBox_BianHao_N->isChecked())
		{
			vFields.push_back("编号");
		}
		if (ui.checkBox_YanSe_N->isChecked())
		{
			vFields.push_back("颜色");
		}
		if (ui.checkBox_SeCaiTuAn_N->isChecked())
		{
			vFields.push_back("色彩图案");
		}
		if (ui.checkBox_DingBiaoYanSe_N->isChecked())
		{
			vFields.push_back("顶标颜色");
		}
		if (ui.checkBox_FaGuangZhuangTai_N->isChecked())
		{
			vFields.push_back("发光状态");
		}
		if (ui.checkBox_GaoDu_N->isChecked())
		{
			vFields.push_back("高度");
		}
		if (ui.checkBox_DengGuangTeXing_N->isChecked())
		{
			vFields.push_back("灯光特性");
		}
		if (ui.checkBox_XinHaoZu_N->isChecked())
		{
			vFields.push_back("信号组");
		}
		if (ui.checkBox_XinHaoZhouQi_N->isChecked())
		{
			vFields.push_back("信号周期");
		}
		if (ui.checkBox_ZuoYongJuLi_N->isChecked())
		{
			vFields.push_back("作用距离");
		}
		if (ui.checkBox_DengGuangKeShi_N->isChecked())
		{
			vFields.push_back("灯光可视");
		}
		if (ui.checkBox_FangWei_N->isChecked())
		{
			vFields.push_back("方位");
		}
		if (ui.checkBox_HangXingZhiXiang_N->isChecked())
		{
			vFields.push_back("航行指向");
		}
		if (ui.checkBox_GuangHuJiaoDu1_N->isChecked())
		{
			vFields.push_back("光弧角度1");
		}
		if (ui.checkBox_GuangHuJiaoDu2_N->isChecked())
		{
			vFields.push_back("光弧角度2");
		}
		if (ui.checkBox_LeiDaKeShi_N->isChecked())
		{
			vFields.push_back("雷达可视");
		}
		if (ui.checkBox_ShuiShenZhi1_N->isChecked())
		{
			vFields.push_back("水深值1");
		}
		if (ui.checkBox_ZuoYongFangShi_N->isChecked())
		{
			vFields.push_back("作用方式");
		}
		if (ui.checkBox_FuBiaoXiTong_N->isChecked())
		{
			vFields.push_back("浮标系统");
		}
		if (ui.checkBox_TuXingTeZheng_N->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_N->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_N->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 海上区域界线
	if (strLayerType == "O")
	{
		if (ui.checkBox_YaoSuBianHao_O->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_O->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_J->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_O->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_GuoJiaDaiMa_O->isChecked())
		{
			vFields.push_back("国家代码");
		}
		if (ui.checkBox_BianHao_O->isChecked())
		{
			vFields.push_back("编号");
		}
		if (ui.checkBox_XianZhiZhongLei_O->isChecked())
		{
			vFields.push_back("限制种类");
		}
		if (ui.checkBox_CunZaiZhuangTai_O->isChecked())
		{
			vFields.push_back("存在状态");
		}
		if (ui.checkBox_LeiDaKeShi_O->isChecked())
		{
			vFields.push_back("雷达可视");
		}
		if (ui.checkBox_BanJing_O->isChecked())
		{
			vFields.push_back("半径");
		}
		if (ui.checkBox_GaoDu_O->isChecked())
		{
			vFields.push_back("高度");
		}
		if (ui.checkBox_TuXingTeZheng_O->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_O->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_O->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 航空要素
	if (strLayerType == "P")
	{
		if (ui.checkBox_YaoSuBianHao_P->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_P->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_P->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_P->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_BianHao_P->isChecked())
		{
			vFields.push_back("编号");
		}
		if (ui.checkBox_PaoDaoCiFangXiang_P->isChecked())
		{
			vFields.push_back("跑道磁方向");
		}
		if (ui.checkBox_GaoCheng_P->isChecked())
		{
			vFields.push_back("高程");
		}
		if (ui.checkBox_BiGao_P->isChecked())
		{
			vFields.push_back("比高");
		}
		if (ui.checkBox_KuanDu_P->isChecked())
		{
			vFields.push_back("宽度");
		}
		if (ui.checkBox_TuXingTeZheng_P->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_P->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_P->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 军事区域
	if (strLayerType == "Q")
	{
		if (ui.checkBox_YaoSuBianHao_Q->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_Q->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_Q->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_LeiXing_Q->isChecked())
		{
			vFields.push_back("类型");
		}
		if (ui.checkBox_XianZhiZhongLei_Q->isChecked())
		{
			vFields.push_back("限制种类");
		}
		if (ui.checkBox_TuXingTeZheng_Q->isChecked())
		{
			vFields.push_back("图形特征");
		}
		if (ui.checkBox_ZhuJiZhiZhen_Q->isChecked())
		{
			vFields.push_back("注记指针");
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_Q->isChecked())
		{
			vFields.push_back("外挂表指针");
		}
	}

	// 注记
	if (strLayerType == "R")
	{
		if (ui.checkBox_YaoSuBianHao_R->isChecked())
		{
			vFields.push_back("要素编号");
		}
		if (ui.checkBox_BianMa_R->isChecked())
		{
			vFields.push_back("编码");
		}
		if (ui.checkBox_MingCheng_R->isChecked())
		{
			vFields.push_back("名称");
		}
		if (ui.checkBox_ZiTi_R->isChecked())
		{
			vFields.push_back("字体");
		}
		if (ui.checkBox_ZiXing_R->isChecked())
		{
			vFields.push_back("字形");
		}
		if (ui.checkBox_ZiJi_R->isChecked())
		{
			vFields.push_back("字级");
		}
		if (ui.checkBox_ZiXiang_R->isChecked())
		{
			vFields.push_back("字向");
		}
		if (ui.checkBox_YanSe_R->isChecked())
		{
			vFields.push_back("颜色");
		}
	}

	vector<string> vFields_GBK;
	vFields_GBK.clear();
	// 将UTF-8编码转换为GBK编码
	for (int i = 0; i < vFields.size(); i++)
	{
		string strGBK = UTF8_To_GBK(vFields[i]);
		vFields_GBK.push_back(strGBK);
	}

	vFields.clear();
	vFields = vFields_GBK;
}

// 获取图层字段选择状态
void QgsSetAttributeExtractParamsDlg::GetLayerFeatureClassChecked(string strLayerType, vector<string>& vFeatureClassCode)
{
	vFeatureClassCode.clear();
	// 存储list控件
	vector<QListWidget*> vQListWidget;
	vQListWidget.clear();

	/*根据图层类型，获取要素类名称选中状态，将选中的要素类加入到列表中*/

	// 测量控制点
	if (strLayerType == "A")
	{
		// A的list控件包括：listWidget_CeLiangKongZhiDian
		vQListWidget.push_back(ui.listWidget_CeLiangKongZhiDian);
	}

	// 工农业社会文化设施
	if (strLayerType == "B")
	{
		// B的list控件包括：
		/* 1、listWidget_GongYe
		   2、listWidget_NongYe
		   3、listWidget_KeXueWenWei
		   4、listWidget_ZhengFuJiGuanZhuDi
		   5、listWidget_GongGongFuWuSheShi
		   6、listWidget_GangKouGuanLi
		   7、listWidget_HangHaiXinHaoTaiZhan
		   8、listWidget_HuanShan
		   9、listWidget_QiTa_B
		*/

		vQListWidget.push_back(ui.listWidget_GongYe);
		vQListWidget.push_back(ui.listWidget_NongYe);
		vQListWidget.push_back(ui.listWidget_KeXueWenWei);
		vQListWidget.push_back(ui.listWidget_ZhengFuJiGuanZhuDi);
		vQListWidget.push_back(ui.listWidget_GongGongFuWuSheShi);
		vQListWidget.push_back(ui.listWidget_GangKouGuanLi);
		vQListWidget.push_back(ui.listWidget_HangHaiXinHaoTaiZhan);
		vQListWidget.push_back(ui.listWidget_HuanShan);
		vQListWidget.push_back(ui.listWidget_QiTa_B);
	}

	// 居民地及附属设施
	if (strLayerType == "C")
	{
		// C的list控件包括：
		/* 1、listWidget_DuLiJianZhuWu
		   2、listWidget_JuZhuQu
		   3、listWidget_QiTaJianZhuWu
		*/

		vQListWidget.push_back(ui.listWidget_DuLiJianZhuWu);
		vQListWidget.push_back(ui.listWidget_JuZhuQu);
		vQListWidget.push_back(ui.listWidget_QiTaJianZhuWu);
	}

	// 陆地交通
	if (strLayerType == "D")
	{
		// D的list控件包括：
		/* 1、listWidget_TieLu
		   2、listWidget_TieLuCheZhanJiFuShuSheShi
		   3、listWidget_GongLu
		   4、listWidget_QiTaDaoLu
		   5、listWidget_FuShuJianZhuWu
		*/

		vQListWidget.push_back(ui.listWidget_TieLu);
		vQListWidget.push_back(ui.listWidget_TieLuCheZhanJiFuShuSheShi);
		vQListWidget.push_back(ui.listWidget_GongLu);
		vQListWidget.push_back(ui.listWidget_QiTaDaoLu);
		vQListWidget.push_back(ui.listWidget_FuShuJianZhuWu);
	}

	// 管线
	if (strLayerType == "E")
	{
		// E的list控件包括：
		/* 1、listWidget_DianLiXian
		   2、listWidget_TongXinXian
		   3、listWidget_GuanDao
		*/

		vQListWidget.push_back(ui.listWidget_DianLiXian);
		vQListWidget.push_back(ui.listWidget_TongXinXian);
		vQListWidget.push_back(ui.listWidget_GuanDao);
	}

	// 水域/陆地
	if (strLayerType == "F")
	{
		// F的list控件包括：
		/* 1、listWidget_AnXianAn
		   2、listWidget_HeLiu
		   3、listWidget_YunHeGouQu
		   4、listWidget_HuPoShuiKuChiTang
		   5、listWidget_ShuiLiSheShi
		   6、listWidget_QiTaShuiXiYaoSu
		   7、listWidget_YiBanDi
		   8、listWidget_FangBoDi
		   9、listWidget_GangKouMaTou
		   10、listWidget_BoWei
		   11、listWidget_LuDiDaoYuHaiYang
		   12、listWidget_Tan
		*/

		vQListWidget.push_back(ui.listWidget_AnXianAn);
		vQListWidget.push_back(ui.listWidget_HeLiu);
		vQListWidget.push_back(ui.listWidget_YunHeGouQu);

		vQListWidget.push_back(ui.listWidget_HuPoShuiKuChiTang);
		vQListWidget.push_back(ui.listWidget_ShuiLiSheShi);
		vQListWidget.push_back(ui.listWidget_QiTaShuiXiYaoSu);

		vQListWidget.push_back(ui.listWidget_YiBanDi);
		vQListWidget.push_back(ui.listWidget_FangBoDi);
		vQListWidget.push_back(ui.listWidget_GangKouMaTou);

		vQListWidget.push_back(ui.listWidget_BoWei);
		vQListWidget.push_back(ui.listWidget_LuDiDaoYuHaiYang);
		vQListWidget.push_back(ui.listWidget_Tan);
	}

	// 海底地貌及底质
	if (strLayerType == "G")
	{
		// G的list控件包括：
		/* 1、listWidget_ShenDu
		   2、listWidget_HaiDiDiZhi
		   3、listWidget_QiTa_G
		*/

		vQListWidget.push_back(ui.listWidget_ShenDu);
		vQListWidget.push_back(ui.listWidget_HaiDiDiZhi);
		vQListWidget.push_back(ui.listWidget_QiTa_G);
	}

	// 礁石、沉船、障碍物
	if (strLayerType == "H")
	{
		// H的list控件包括：
		/* 1、listWidget_JiaoShi
		   2、listWidget_ChenChuan
		   3、listWidget_ZhangAiWu
		   4、listWidget_BuYuSheShiDeng
		*/

		vQListWidget.push_back(ui.listWidget_JiaoShi);
		vQListWidget.push_back(ui.listWidget_ChenChuan);
		vQListWidget.push_back(ui.listWidget_ZhangAiWu);
		vQListWidget.push_back(ui.listWidget_BuYuSheShiDeng);
	}

	// 水文
	if (strLayerType == "I")
	{
		// I的list控件包括：
		/* 1、listWidget_NeiHeShuiWen
		   2、listWidget_HaiLiuChaoLiu
		   3、listWidget_QiangLieShuiWenXianXiang
		   4、listWidget_ChaoXiChaoXin
		*/

		vQListWidget.push_back(ui.listWidget_NeiHeShuiWen);
		vQListWidget.push_back(ui.listWidget_HaiLiuChaoLiu);
		vQListWidget.push_back(ui.listWidget_QiangLieShuiWenXianXiang);
		vQListWidget.push_back(ui.listWidget_ChaoXiChaoXin);
	}

	// 陆地地貌及土质
	if (strLayerType == "J")
	{
		// J的list控件包括：
		/* 1、listWidget_DengGaoXian
		   2、listWidget_DiMaoGaoCheng
		   3、listWidget_XueShanDiMao
		   4、listWidget_HuangTuDiMao
		   5、listWidget_YanRongDiMao
		   6、listWidget_FengChengDiMao
		   7、listWidget_HuoShanDiMao
		   8、listWidget_QiTaDiMao
		*/

		vQListWidget.push_back(ui.listWidget_DengGaoXian);
		vQListWidget.push_back(ui.listWidget_DiMaoGaoCheng);
		vQListWidget.push_back(ui.listWidget_XueShanDiMao);
		vQListWidget.push_back(ui.listWidget_HuangTuDiMao);

		vQListWidget.push_back(ui.listWidget_YanRongDiMao);
		vQListWidget.push_back(ui.listWidget_FengChengDiMao);
		vQListWidget.push_back(ui.listWidget_HuoShanDiMao);
		vQListWidget.push_back(ui.listWidget_QiTaDiMao);
	}

	// 境界与政区
	if (strLayerType == "K")
	{
		// K的list控件包括：
		/* 1、listWidget_GuoJieJiLingHai
		   2、listWidget_GuoNeiJingJie
		   3、listWidget_XingZhengQu
		   4、listWidget_QiTaJieXian
		*/

		vQListWidget.push_back(ui.listWidget_GuoJieJiLingHai);
		vQListWidget.push_back(ui.listWidget_GuoNeiJingJie);
		vQListWidget.push_back(ui.listWidget_XingZhengQu);
		vQListWidget.push_back(ui.listWidget_QiTaJieXian);
	}

	// 植被
	if (strLayerType == "L")
	{
		// L的list控件包括：
		/* 1、listWidget_LinDi
		   2、listWidget_TianDi
		   3、listWidget_DiLeiJieXian
		*/

		vQListWidget.push_back(ui.listWidget_LinDi);
		vQListWidget.push_back(ui.listWidget_TianDi);
		vQListWidget.push_back(ui.listWidget_DiLeiJieXian);
	}

	// 地磁要素
	if (strLayerType == "M")
	{
		// L的list控件包括：
		/* 1、listWidget_DiCiYaoSu
		*/

		vQListWidget.push_back(ui.listWidget_DiCiYaoSu);
	}

	// 助航设备及航道
	if (strLayerType == "N")
	{
		// N的list控件包括：
		/* 1、listWidget_DengBiao
		   2、listWidget_DengTaDengZhuang
		   3、listWidget_HuoJieShiDengZhuang
		   4、listWidget_ShuiZhongDengZhuang
		   5、listWidget_TaXingLiBiao

		   6、listWidget_GeShiLiBiao
		   7、listWidget_LiBiao
		   8、listWidget_ShuiZhongLiBiao
		   9、listWidget_ChuanXingDengFuBiao
		   10、listWidget_DengChuan

		   11、listWidget_DaXingFuBiao
		   12、listWidget_ZhuXingFuBiao
		   13、listWidget_GanXingFuBiao
		   14、listWidget_ZhuiXingFuBiao
		   15、listWidget_QiuXingFuBiao

		   16、listWidget_GuanXingFuBiao
		   17、listWidget_TongXingFuBiao
		   18、listWidget_TeShuBiaoZhi
		   19、listWidget_HangHaiLeiDa
		   20、listWidget_WuXianDian

		   21、listWidget_WuHao
		   22、listWidget_HangDaoJiXiangGuanBiaoZhi
		*/

		vQListWidget.push_back(ui.listWidget_DengBiao);
		vQListWidget.push_back(ui.listWidget_DengTaDengZhuang);
		vQListWidget.push_back(ui.listWidget_HuoJieShiDengZhuang);
		vQListWidget.push_back(ui.listWidget_ShuiZhongDengZhuang);
		vQListWidget.push_back(ui.listWidget_TaXingLiBiao);

		vQListWidget.push_back(ui.listWidget_GeShiLiBiao);
		vQListWidget.push_back(ui.listWidget_LiBiao);
		vQListWidget.push_back(ui.listWidget_ShuiZhongLiBiao);
		vQListWidget.push_back(ui.listWidget_ChuanXingDengFuBiao);
		vQListWidget.push_back(ui.listWidget_DengChuan);

		vQListWidget.push_back(ui.listWidget_DaXingFuBiao);
		vQListWidget.push_back(ui.listWidget_ZhuXingFuBiao);
		vQListWidget.push_back(ui.listWidget_GanXingFuBiao);
		vQListWidget.push_back(ui.listWidget_ZhuiXingFuBiao);
		vQListWidget.push_back(ui.listWidget_QiuXingFuBiao);

		vQListWidget.push_back(ui.listWidget_GuanXingFuBiao);
		vQListWidget.push_back(ui.listWidget_TongXingFuBiao);
		vQListWidget.push_back(ui.listWidget_TeShuBiaoZhi);
		vQListWidget.push_back(ui.listWidget_HangHaiLeiDa);
		vQListWidget.push_back(ui.listWidget_WuXianDian);

		vQListWidget.push_back(ui.listWidget_WuHao);
		vQListWidget.push_back(ui.listWidget_HangDaoJiXiangGuanBiaoZhi);
	}

	// 海上区域界线
	if (strLayerType == "O")
	{
		// O的list控件包括：
		/* 1、listWidget_HaiShangGuanLiQu
		   2、listWidget_QingDaoQu
		   3、listWidget_MaoDi
		   4、listWidget_HaiShangXianZhiQu
		   5、listWidget_JinHaiSheShi
		*/

		vQListWidget.push_back(ui.listWidget_HaiShangGuanLiQu);
		vQListWidget.push_back(ui.listWidget_QingDaoQu);
		vQListWidget.push_back(ui.listWidget_MaoDi);
		vQListWidget.push_back(ui.listWidget_HaiShangXianZhiQu);
		vQListWidget.push_back(ui.listWidget_JinHaiSheShi);
	}

	// 航空要素
	if (strLayerType == "P")
	{
		// P的list控件包括：
		/* 1、listWidget_FeiJiJiChang
		   2、listWidget_HangKongZhangAiWu
		   3、listWidget_HangKongDaoHang
		   4、listWidget_KongZhongQuYuJiJieXian
		   5、listWidget_HangLuHangXian
		   6、listWidget_JiChangChangDao
		*/

		vQListWidget.push_back(ui.listWidget_FeiJiJiChang);
		vQListWidget.push_back(ui.listWidget_HangKongZhangAiWu);
		vQListWidget.push_back(ui.listWidget_HangKongDaoHang);
		vQListWidget.push_back(ui.listWidget_KongZhongQuYuJiJieXian);
		vQListWidget.push_back(ui.listWidget_HangLuHangXian);
		vQListWidget.push_back(ui.listWidget_JiChangChangDao);
	}

	// 军事区域
	if (strLayerType == "Q")
	{
		// Q的list控件包括：
		/* 1、listWidget_JunShiXunLianQu
		   2、listWidget_JunShiGuanLiQu
		*/

		vQListWidget.push_back(ui.listWidget_JunShiXunLianQu);
		vQListWidget.push_back(ui.listWidget_JunShiGuanLiQu);
	}

	// 注记
	if (strLayerType == "R")
	{
		// R的list控件包括：
		/* 1、listWidget_JuMinDiMingCheng
		   2、listWidget_XingZhengQuMingCheng
		   3、listWidget_ZhuanYouMingCheng
		   4、listWidget_HaiYangHuPoHeChuan
		   5、listWidget_AoDiPenDi
		   6、listWidget_DaoBanDao
		   7、listWidget_ShanMaiShanLing
		   8、listWidget_DuLiGaoDi
		   9、listWidget_QiTaZhuJi
		*/

		vQListWidget.push_back(ui.listWidget_JuMinDiMingCheng);
		vQListWidget.push_back(ui.listWidget_XingZhengQuMingCheng);
		vQListWidget.push_back(ui.listWidget_ZhuanYouMingCheng);

		vQListWidget.push_back(ui.listWidget_HaiYangHuPoHeChuan);
		vQListWidget.push_back(ui.listWidget_AoDiPenDi);
		vQListWidget.push_back(ui.listWidget_DaoBanDao);

		vQListWidget.push_back(ui.listWidget_ShanMaiShanLing);
		vQListWidget.push_back(ui.listWidget_DuLiGaoDi);
		vQListWidget.push_back(ui.listWidget_QiTaZhuJi);
	}

	for (int i = 0; i < vQListWidget.size(); i++)
	{
		// 每个list的元素个数
		int iCount = vQListWidget[i]->count();
		for (int iRow = 0; iRow < iCount; iRow++)
		{
			QListWidgetItem* pItem = vQListWidget[i]->item(iRow);
			// 如果当前list元素为☑状态
			if (pItem->checkState() == Qt::Checked)
			{
				QString qstrText = pItem->text();
				QString qstrName = qstrText;
				QString qstrCode;
				// 通过映射表获取编码
				MAP_FeatureClassName_Code::iterator iter = m_mName_Codes.find(qstrName);
				if (iter != m_mName_Codes.end())
				{
					qstrCode = iter->second;
					string strCode = qstrCode.toStdString();
					vFeatureClassCode.push_back(strCode);
				}
			}
		}
	}
}

void QgsSetAttributeExtractParamsDlg::pushButton_Open_clicked()
{
	int iInputDataType = GetInputDataType();

	// 如果选择数据库类型，则输入路径为gpkg或gdb文件所在全路径
	if (iInputDataType == 1)
	{
		QString curPath = m_qstrInputDataPath;
		QString dlgTitle = "选择数据库文件";
		QString filter = "gpkg 文件(*.gpkg);;gdb 文件(*.gdb)";
		QString strFileName = QFileDialog::getOpenFileName(this,
			dlgTitle, curPath, filter);
		if (!strFileName.isEmpty())
		{
			m_qstrInputDataPath = strFileName;
		}
	}
	// 如果选择文件系统，则输入路径为文件夹目录
	else if (iInputDataType == 2)
	{
		// 选择文件夹
		QString curPath = m_qstrInputDataPath;
		QString dlgTile = "请选择输入数据路径";
		QString selectedDir = QFileDialog::getExistingDirectory(
			this,
			dlgTile,
			curPath,
			QFileDialog::ShowDirsOnly);

		if (!selectedDir.isEmpty())
		{
			m_qstrInputDataPath = selectedDir;
		}
	}
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
}

void QgsSetAttributeExtractParamsDlg::pushButton_Save_clicked()
{
	int iOutputDataType = GetOutputDataType();

	// 如果选择数据库类型，则输出路径为gpkg或gdb文件所在全路径
	if (iOutputDataType == 1)
	{
		QString curPath = m_qstrOutputDataPath; 
		QString dlgTitle = "请选择输出数据路径";
		QString filter = "gpkg 文件(*.gpkg)";
		QString strFileName = QFileDialog::getSaveFileName(this,
			dlgTitle, curPath, filter);
		if (!strFileName.isEmpty())
		{
			QFile file(strFileName);
			if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
				return;
			file.close();

			m_qstrOutputDataPath = strFileName;
		}
	}
	// 如果选择文件系统，则输入路径为文件夹目录
	else if (iOutputDataType == 2)
	{
		// 选择文件夹
		QString curPath = m_qstrOutputDataPath; 
		QString dlgTile = "请选择输出数据路径";
		QString selectedDir = QFileDialog::getExistingDirectory(
			this,
			dlgTile,
			curPath,
			QFileDialog::ShowDirsOnly);

		if (!selectedDir.isEmpty())
		{
			m_qstrOutputDataPath = selectedDir;
		}
	}
	ui.lineEdit_OutputDataPath->setText(m_qstrOutputDataPath);
}

void QgsSetAttributeExtractParamsDlg::pushButton_LoadExtractParams_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = "选择配置文件";
	QString filter = "xml 文件(*.xml)";
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();
	vector<ExtractDataParam> vExtractDataParam;
	vExtractDataParam.clear();
	while (!node.isNull())
	{
		// 循环遍历每个子节点
		ExtractDataParam param;

		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					param.strLayerName = elementTemp.text().toStdString();
				}

				else if (elementTemp.tagName() == "Fields")
				{
					// 获取字段列表
					QDomNodeList fieldList = nodeTemp.childNodes();
					for (int j = 0; j < fieldList.size(); j++)
					{
						QDomNode fieldNode = fieldList.at(j);
						QDomElement fieldElement = fieldNode.toElement();
						QString qstrField = fieldElement.text();
						string strField = qstrField.toLocal8Bit();
						param.vFields.push_back(strField);
					}
				}

				else if (elementTemp.tagName() == "FeatureClassCodes")
				{
					// 获取字段列表
					QDomNodeList FeatureClassList = nodeTemp.childNodes();
					for (int j = 0; j < FeatureClassList.size(); j++)
					{
						QDomNode FeatureClassNode = FeatureClassList.at(j);
						QDomElement fieldElement = FeatureClassNode.toElement();
						QString qstrFeatureClass = fieldElement.text();
						param.vFeatureClassCode.push_back(qstrFeatureClass.toStdString());
					}
				}
			}
		}

		vExtractDataParam.push_back(param);

		node = node.nextSibling();//读取下一个兄弟节点
	}

	// 根据配置文件设置界面
	for (int i = 0; i < vExtractDataParam.size(); i++)
	{
		ExtractDataParam param = vExtractDataParam[i];
		SetCheckBoxStatus(param);
		SetListWidgetStatus(param);
	}
}

// 设置字段复选框的状态
void QgsSetAttributeExtractParamsDlg::SetCheckBoxStatus(ExtractDataParam& param)
{
	// 按图层依次进行设置：A到R
	// 测量控制点
	if (param.strLayerName == "A")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_A->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_A->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_A->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_A->setChecked(true);
			}

			if (qstrField == "等级")
			{
				ui.checkBox_DengJi_A->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_A->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_A->setChecked(true);
			}

			if (qstrField == "理论横坐标")
			{
				ui.checkBox_LiLunHengZuoBiao_A->setChecked(true);
			}

			if (qstrField == "理论纵坐标")
			{
				ui.checkBox_LiLunZongZuoBiao_A->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_A->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_A->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_A->setChecked(true);
			}
		}
	}

	// 工农业社会文化设施
	if (param.strLayerName == "B")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_B->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_B->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_B->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_B->setChecked(true);
			}

			if (qstrField == "类别")
			{
				ui.checkBox_LeiBie_B->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_B->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_B->setChecked(true);
			}

			if (qstrField == "行政区划")
			{
				ui.checkBox_XingZhengQuHua_B->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_B->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_B->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_B->setChecked(true);
			}
		}
	}

	// 居民地及附属设施
	if (param.strLayerName == "C")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_C->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_C->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_C->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_C->setChecked(true);
			}

			if (qstrField == "居住月份")
			{
				ui.checkBox_JuZhuYueFen_C->setChecked(true);
			}

			if (qstrField == "人口")
			{
				ui.checkBox_RenKou_C->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_C->setChecked(true);
			}

			if (qstrField == "行政区划")
			{
				ui.checkBox_XingZhengQuHua_C->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_C->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_C->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_C->setChecked(true);
			}
		}
	}

	// 陆地交通
	if (param.strLayerName == "D")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_D->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_D->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_D->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_D->setChecked(true);
			}

			if (qstrField == "编号")
			{
				ui.checkBox_BianHao_D->setChecked(true);
			}

			if (qstrField == "等级")
			{
				ui.checkBox_DengJi_D->setChecked(true);
			}

			if (qstrField == "宽度")
			{
				ui.checkBox_KuanDu_D->setChecked(true);
			}

			if (qstrField == "铺宽")
			{
				ui.checkBox_PuKuan_D->setChecked(true);
			}

			if (qstrField == "桥长")
			{
				ui.checkBox_QiaoChang_D->setChecked(true);
			}

			if (qstrField == "净空高")
			{
				ui.checkBox_JingKongGao_D->setChecked(true);
			}

			if (qstrField == "载重吨数")
			{
				ui.checkBox_ZaiZhongDunShu_D->setChecked(true);
			}

			if (qstrField == "里程")
			{
				ui.checkBox_LiCheng_D->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_D->setChecked(true);
			}

			if (qstrField == "通行月份")
			{
				ui.checkBox_TongXingYueFen_D->setChecked(true);
			}

			if (qstrField == "水深")
			{
				ui.checkBox_ShuiShen_D->setChecked(true);
			}

			if (qstrField == "底质")
			{
				ui.checkBox_DiZhi_D->setChecked(true);
			}

			if (qstrField == "曲率半径")
			{
				ui.checkBox_QuLvBanJing_D->setChecked(true);
			}

			if (qstrField == "最大纵坡")
			{
				ui.checkBox_ZuiDaZongPo_D->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_D->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_D->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_D->setChecked(true);
			}
		}
	}

	// 管线
	if (param.strLayerName == "E")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_E->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_E->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_E->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_E->setChecked(true);
			}

			if (qstrField == "净空高")
			{
				ui.checkBox_JingKongGao_E->setChecked(true);
			}

			if (qstrField == "埋藏深度")
			{
				ui.checkBox_MaiCangShenDu_E->setChecked(true);
			}

			if (qstrField == "存在状态")
			{
				ui.checkBox_CunZaiZhuangTai_E->setChecked(true);
			}

			if (qstrField == "作用方式")
			{
				ui.checkBox_ZuoYongFangShi_E->setChecked(true);
			}

			if (qstrField == "限制种类")
			{
				ui.checkBox_XianZhiZhongLei_E->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_E->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_E->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_E->setChecked(true);
			}
		}
	}

	// 水域/陆地
	if (param.strLayerName == "F")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_F->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_F->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_F->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_F->setChecked(true);
			}

			if (qstrField == "宽度")
			{
				ui.checkBox_KuanDu_F->setChecked(true);
			}

			if (qstrField == "水深")
			{
				ui.checkBox_ShuiShen_F->setChecked(true);
			}

			if (qstrField == "泥深")
			{
				ui.checkBox_NiShen_F->setChecked(true);
			}

			if (qstrField == "时令月份")
			{
				ui.checkBox_ShiLingYueFen_F->setChecked(true);
			}

			if (qstrField == "长度")
			{
				ui.checkBox_ChangDu_F->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_F->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_F->setChecked(true);
			}

			if (qstrField == "库容量")
			{
				ui.checkBox_KuRongLiang_F->setChecked(true);
			}

			if (qstrField == "吨位")
			{
				ui.checkBox_DunWei_F->setChecked(true);
			}

			if (qstrField == "河流代码")
			{
				ui.checkBox_HeLiuDaiMa_F->setChecked(true);
			}

			if (qstrField == "通航性质")
			{
				ui.checkBox_TongHangXingZhi_F->setChecked(true);
			}

			if (qstrField == "植物类型")
			{
				ui.checkBox_ZhiWuLeiXing_F->setChecked(true);
			}

			if (qstrField == "表面物质")
			{
				ui.checkBox_BiaoMianWuZhi_F->setChecked(true);
			}

			if (qstrField == "存在状态")
			{
				ui.checkBox_CunZaiZhuangTai_F->setChecked(true);
			}

			if (qstrField == "位置质量")
			{
				ui.checkBox_WeiZhiZhiLiang_F->setChecked(true);
			}

			if (qstrField == "颜色")
			{
				ui.checkBox_YanSe_F->setChecked(true);
			}

			if (qstrField == "与水面关系")
			{
				ui.checkBox_YuShuiMianGuanXi_F->setChecked(true);
			}
			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_F->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_F->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_F->setChecked(true);
			}
		}
	}

	// 海底地貌及底质
	if (param.strLayerName == "G")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_G->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_G->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_G->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_G->setChecked(true);
			}

			if (qstrField == "水深值")
			{
				ui.checkBox_ShuiShenZhi_G->setChecked(true);
			}

			if (qstrField == "水深值1")
			{
				ui.checkBox_ShuiShenZhi1_G->setChecked(true);
			}

			if (qstrField == "水深值2")
			{
				ui.checkBox_ShuiShenZhi2_G->setChecked(true);
			}

			if (qstrField == "测深技术")
			{
				ui.checkBox_CeShenJiShu_G->setChecked(true);
			}

			if (qstrField == "测深质量")
			{
				ui.checkBox_CeShenZhiLiang_G->setChecked(true);
			}

			if (qstrField == "位置质量")
			{
				ui.checkBox_WeiZhiZhiLiang_G->setChecked(true);
			}

			if (qstrField == "作用方式")
			{
				ui.checkBox_ZuoYongFangShi_G->setChecked(true);
			}

			if (qstrField == "表面物质")
			{
				ui.checkBox_BiaoMianWuZhi_G->setChecked(true);
			}

			if (qstrField == "物质形态")
			{
				ui.checkBox_WuZhiXingTai_G->setChecked(true);
			}

			if (qstrField == "危险级")
			{
				ui.checkBox_WeiXianJi_G->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_G->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_G->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_G->setChecked(true);
			}
		}
	}

	// 礁石、沉船、障碍物
	if (param.strLayerName == "H")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_H->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_H->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_H->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_H->setChecked(true);
			}

			if (qstrField == "水深值")
			{
				ui.checkBox_ShuiShenZhi_H->setChecked(true);
			}

			if (qstrField == "高度")
			{
				ui.checkBox_GaoDu_H->setChecked(true);
			}

			if (qstrField == "垂高")
			{
				ui.checkBox_ChuiGao_H->setChecked(true);
			}

			if (qstrField == "存在状态")
			{
				ui.checkBox_CunZaiZhuangTai_H->setChecked(true);
			}

			if (qstrField == "测深技术")
			{
				ui.checkBox_CeShenJiShu_H->setChecked(true);
			}

			if (qstrField == "测深质量")
			{
				ui.checkBox_CeShenZhiLiang_H->setChecked(true);
			}

			if (qstrField == "位置质量")
			{
				ui.checkBox_WeiZhiZhiLiang_H->setChecked(true);
			}

			if (qstrField == "限制种类")
			{
				ui.checkBox_XianZhiZhongLei_H->setChecked(true);
			}

			if (qstrField == "作用方式")
			{
				ui.checkBox_ZuoYongFangShi_H->setChecked(true);
			}

			if (qstrField == "表面物质")
			{
				ui.checkBox_BiaoMianWuZhi_H->setChecked(true);
			}

			if (qstrField == "与水面关系")
			{
				ui.checkBox_YuShuiMianGuanXi_H->setChecked(true);
			}

			if (qstrField == "危险级")
			{
				ui.checkBox_WeiXianJi_H->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_H->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_H->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_H->setChecked(true);
			}
		}
	}

	// 工水文
	if (param.strLayerName == "I")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_I->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_I->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_I->setChecked(true);
			}

			if (qstrField == "宽度")
			{
				ui.checkBox_KuanDu_I->setChecked(true);
			}

			if (qstrField == "水深")
			{
				ui.checkBox_ShuiShen_I->setChecked(true);
			}

			if (qstrField == "底质")
			{
				ui.checkBox_DiZhi_I->setChecked(true);
			}

			if (qstrField == "流速")
			{
				ui.checkBox_LiuSu_I->setChecked(true);
			}

			if (qstrField == "方位")
			{
				ui.checkBox_FangWei_I->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_I->setChecked(true);
			}

			if (qstrField == "月份")
			{
				ui.checkBox_YueFen_I->setChecked(true);
			}

			if (qstrField == "描述")
			{
				ui.checkBox_MiaoShu_I->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_I->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_I->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_I->setChecked(true);
			}
		}
	}

	// 陆地地貌及土质
	if (param.strLayerName == "J")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_J->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_J->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_J->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_J->setChecked(true);
			}

			if (qstrField == "类别")
			{
				ui.checkBox_LeiBie_J->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_J->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_J->setChecked(true);
			}

			if (qstrField == "沟宽")
			{
				ui.checkBox_GouKuan_J->setChecked(true);
			}

			if (qstrField == "方向")
			{
				ui.checkBox_FangXiang_J->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_J->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_J->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_J->setChecked(true);
			}
		}
	}

	// 境界与政区
	if (param.strLayerName == "K")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_K->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_K->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_K->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_K->setChecked(true);
			}

			if (qstrField == "代码")
			{
				ui.checkBox_DaiMa_K->setChecked(true);
			}

			if (qstrField == "编号")
			{
				ui.checkBox_BianHao_K->setChecked(true);
			}

			if (qstrField == "序号")
			{
				ui.checkBox_XuHao_K->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_K->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_K->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_K->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_K->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_K->setChecked(true);
			}
		}
	}

	// 植被
	if (param.strLayerName == "L")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_L->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_L->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_L->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_L->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_L->setChecked(true);
			}

			if (qstrField == "高度")
			{
				ui.checkBox_GaoDu_L->setChecked(true);
			}

			if (qstrField == "胸径")
			{
				ui.checkBox_XiongJing_L->setChecked(true);
			}

			if (qstrField == "宽度")
			{
				ui.checkBox_KuanDu_L->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_L->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_L->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_L->setChecked(true);
			}
		}
	}

	// 地磁要素
	if (param.strLayerName == "M")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_M->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_M->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_M->setChecked(true);
			}

			if (qstrField == "磁差值")
			{
				ui.checkBox_CiChaZhi_M->setChecked(true);
			}

			if (qstrField == "参考年")
			{
				ui.checkBox_CanKaoNian_M->setChecked(true);
			}

			if (qstrField == "年变值")
			{
				ui.checkBox_NianBianZhi_M->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_M->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_M->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_M->setChecked(true);
			}
		}
	}

	// 助航设备及航道
	if (param.strLayerName == "N")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_N->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_N->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_N->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_N->setChecked(true);
			}

			if (qstrField == "性质")
			{
				ui.checkBox_XingZhi_N->setChecked(true);
			}

			if (qstrField == "编号")
			{
				ui.checkBox_BianHao_N->setChecked(true);
			}

			if (qstrField == "颜色")
			{
				ui.checkBox_YanSe_N->setChecked(true);
			}

			if (qstrField == "色彩图案")
			{
				ui.checkBox_SeCaiTuAn_N->setChecked(true);
			}

			if (qstrField == "顶标颜色")
			{
				ui.checkBox_DingBiaoYanSe_N->setChecked(true);
			}

			if (qstrField == "发光状态")
			{
				ui.checkBox_FaGuangZhuangTai_N->setChecked(true);
			}

			if (qstrField == "高度")
			{
				ui.checkBox_GaoDu_N->setChecked(true);
			}

			if (qstrField == "灯光特性")
			{
				ui.checkBox_DengGuangTeXing_N->setChecked(true);
			}

			if (qstrField == "信号组")
			{
				ui.checkBox_XinHaoZu_N->setChecked(true);
			}

			if (qstrField == "信号周期")
			{
				ui.checkBox_XinHaoZhouQi_N->setChecked(true);
			}

			if (qstrField == "作用距离")
			{
				ui.checkBox_ZuoYongJuLi_N->setChecked(true);
			}

			if (qstrField == "灯光可视")
			{
				ui.checkBox_DengGuangKeShi_N->setChecked(true);
			}

			if (qstrField == "方位")
			{
				ui.checkBox_FangWei_N->setChecked(true);
			}

			if (qstrField == "航行指向")
			{
				ui.checkBox_HangXingZhiXiang_N->setChecked(true);
			}

			if (qstrField == "光弧角度1")
			{
				ui.checkBox_GuangHuJiaoDu1_N->setChecked(true);
			}

			if (qstrField == "光弧角度2")
			{
				ui.checkBox_GuangHuJiaoDu2_N->setChecked(true);
			}

			if (qstrField == "雷达可视")
			{
				ui.checkBox_LeiDaKeShi_N->setChecked(true);
			}

			if (qstrField == "水深值1")
			{
				ui.checkBox_ShuiShenZhi1_N->setChecked(true);
			}

			if (qstrField == "作用方式")
			{
				ui.checkBox_ZuoYongFangShi_N->setChecked(true);
			}

			if (qstrField == "浮标系统")
			{
				ui.checkBox_FuBiaoXiTong_N->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_N->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_N->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_N->setChecked(true);
			}
		}
	}

	// 海上区域界线
	if (param.strLayerName == "O")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_O->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_O->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_O->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_O->setChecked(true);
			}

			if (qstrField == "国家代码")
			{
				ui.checkBox_GuoJiaDaiMa_O->setChecked(true);
			}

			if (qstrField == "编号")
			{
				ui.checkBox_BianHao_O->setChecked(true);
			}

			if (qstrField == "限制种类")
			{
				ui.checkBox_XianZhiZhongLei_O->setChecked(true);
			}

			if (qstrField == "存在状态")
			{
				ui.checkBox_CunZaiZhuangTai_O->setChecked(true);
			}

			if (qstrField == "雷达可视")
			{
				ui.checkBox_LeiDaKeShi_O->setChecked(true);
			}

			if (qstrField == "半径")
			{
				ui.checkBox_BanJing_O->setChecked(true);
			}

			if (qstrField == "高度")
			{
				ui.checkBox_GaoDu_O->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_O->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_O->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_O->setChecked(true);
			}
		}
	}

	// 航空要素
	if (param.strLayerName == "P")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_P->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_P->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_P->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_P->setChecked(true);
			}

			if (qstrField == "编号")
			{
				ui.checkBox_BianHao_P->setChecked(true);
			}

			if (qstrField == "跑道磁方向")
			{
				ui.checkBox_PaoDaoCiFangXiang_P->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_P->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_P->setChecked(true);
			}

			if (qstrField == "宽度")
			{
				ui.checkBox_KuanDu_P->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_P->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_P->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_P->setChecked(true);
			}
		}
	}

	// 军事区域
	if (param.strLayerName == "Q")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_Q->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_Q->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_Q->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_Q->setChecked(true);
			}

			if (qstrField == "限制种类")
			{
				ui.checkBox_XianZhiZhongLei_Q->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_Q->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_Q->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_Q->setChecked(true);
			}
		}
	}

	// 注记
	if (param.strLayerName == "R")
	{
		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_R->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_R->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_R->setChecked(true);
			}

			if (qstrField == "字体")
			{
				ui.checkBox_ZiTi_R->setChecked(true);
			}

			if (qstrField == "字形")
			{
				ui.checkBox_ZiXing_R->setChecked(true);
			}

			if (qstrField == "字级")
			{
				ui.checkBox_ZiJi_R->setChecked(true);
			}

			if (qstrField == "字向")
			{
				ui.checkBox_ZiXiang_R->setChecked(true);
			}

			if (qstrField == "颜色")
			{
				ui.checkBox_YanSe_R->setChecked(true);
			}
		}
	}
}

// 设置各图层要素类列表框状态
void QgsSetAttributeExtractParamsDlg::SetListWidgetStatus(ExtractDataParam& param)
{
	// 存储list控件
	vector<QListWidget*> vQListWidget;
	vQListWidget.clear();

	// 按图层依次进行设置：A到R
	// 测量控制点
	if (param.strLayerName == "A")
	{
		vQListWidget.push_back(ui.listWidget_CeLiangKongZhiDian);
	}

	// 工农业社会文化设施
	if (param.strLayerName == "B")
	{
		vQListWidget.push_back(ui.listWidget_GongYe);
		vQListWidget.push_back(ui.listWidget_NongYe);
		vQListWidget.push_back(ui.listWidget_KeXueWenWei);
		vQListWidget.push_back(ui.listWidget_ZhengFuJiGuanZhuDi);
		vQListWidget.push_back(ui.listWidget_GongGongFuWuSheShi);
		vQListWidget.push_back(ui.listWidget_GangKouGuanLi);
		vQListWidget.push_back(ui.listWidget_HangHaiXinHaoTaiZhan);
		vQListWidget.push_back(ui.listWidget_HuanShan);
		vQListWidget.push_back(ui.listWidget_QiTa_B);
	}

	// 居民地及附属设施
	if (param.strLayerName == "C")
	{
		vQListWidget.push_back(ui.listWidget_DuLiJianZhuWu);
		vQListWidget.push_back(ui.listWidget_JuZhuQu);
		vQListWidget.push_back(ui.listWidget_QiTaJianZhuWu);
	}

	// 陆地交通
	if (param.strLayerName == "D")
	{
		vQListWidget.push_back(ui.listWidget_TieLu);
		vQListWidget.push_back(ui.listWidget_TieLuCheZhanJiFuShuSheShi);
		vQListWidget.push_back(ui.listWidget_GongLu);
		vQListWidget.push_back(ui.listWidget_QiTaDaoLu);
		vQListWidget.push_back(ui.listWidget_FuShuJianZhuWu);
	}

	// 管线
	if (param.strLayerName == "E")
	{
		vQListWidget.push_back(ui.listWidget_DianLiXian);
		vQListWidget.push_back(ui.listWidget_TongXinXian);
		vQListWidget.push_back(ui.listWidget_GuanDao);
	}

	// 水域/陆地
	if (param.strLayerName == "F")
	{
		vQListWidget.push_back(ui.listWidget_AnXianAn);
		vQListWidget.push_back(ui.listWidget_HeLiu);
		vQListWidget.push_back(ui.listWidget_YunHeGouQu);

		vQListWidget.push_back(ui.listWidget_HuPoShuiKuChiTang);
		vQListWidget.push_back(ui.listWidget_ShuiLiSheShi);
		vQListWidget.push_back(ui.listWidget_QiTaShuiXiYaoSu);

		vQListWidget.push_back(ui.listWidget_YiBanDi);
		vQListWidget.push_back(ui.listWidget_FangBoDi);
		vQListWidget.push_back(ui.listWidget_GangKouMaTou);

		vQListWidget.push_back(ui.listWidget_BoWei);
		vQListWidget.push_back(ui.listWidget_LuDiDaoYuHaiYang);
		vQListWidget.push_back(ui.listWidget_Tan);
	}

	// 海底地貌及底质
	if (param.strLayerName == "G")
	{
		vQListWidget.push_back(ui.listWidget_ShenDu);
		vQListWidget.push_back(ui.listWidget_HaiDiDiZhi);
		vQListWidget.push_back(ui.listWidget_QiTa_G);
	}

	// 礁石、沉船、障碍物
	if (param.strLayerName == "H")
	{
		vQListWidget.push_back(ui.listWidget_JiaoShi);
		vQListWidget.push_back(ui.listWidget_ChenChuan);
		vQListWidget.push_back(ui.listWidget_ZhangAiWu);
		vQListWidget.push_back(ui.listWidget_BuYuSheShiDeng);
	}

	// 工水文
	if (param.strLayerName == "I")
	{
		vQListWidget.push_back(ui.listWidget_NeiHeShuiWen);
		vQListWidget.push_back(ui.listWidget_HaiLiuChaoLiu);
		vQListWidget.push_back(ui.listWidget_QiangLieShuiWenXianXiang);
		vQListWidget.push_back(ui.listWidget_ChaoXiChaoXin);
	}

	// 陆地地貌及土质
	if (param.strLayerName == "J")
	{
		vQListWidget.push_back(ui.listWidget_DengGaoXian);
		vQListWidget.push_back(ui.listWidget_DiMaoGaoCheng);
		vQListWidget.push_back(ui.listWidget_XueShanDiMao);
		vQListWidget.push_back(ui.listWidget_HuangTuDiMao);

		vQListWidget.push_back(ui.listWidget_YanRongDiMao);
		vQListWidget.push_back(ui.listWidget_FengChengDiMao);
		vQListWidget.push_back(ui.listWidget_HuoShanDiMao);
		vQListWidget.push_back(ui.listWidget_QiTaDiMao);
	}

	// 境界与政区
	if (param.strLayerName == "K")
	{
		vQListWidget.push_back(ui.listWidget_GuoJieJiLingHai);
		vQListWidget.push_back(ui.listWidget_GuoNeiJingJie);
		vQListWidget.push_back(ui.listWidget_XingZhengQu);
		vQListWidget.push_back(ui.listWidget_QiTaJieXian);
	}

	// 植被
	if (param.strLayerName == "L")
	{
		vQListWidget.push_back(ui.listWidget_LinDi);
		vQListWidget.push_back(ui.listWidget_TianDi);
		vQListWidget.push_back(ui.listWidget_DiLeiJieXian);
	}

	// 地磁要素
	if (param.strLayerName == "M")
	{
		vQListWidget.push_back(ui.listWidget_DiCiYaoSu);
	}

	// 助航设备及航道
	if (param.strLayerName == "N")
	{
		vQListWidget.push_back(ui.listWidget_DengBiao);
		vQListWidget.push_back(ui.listWidget_DengTaDengZhuang);
		vQListWidget.push_back(ui.listWidget_HuoJieShiDengZhuang);
		vQListWidget.push_back(ui.listWidget_ShuiZhongDengZhuang);
		vQListWidget.push_back(ui.listWidget_TaXingLiBiao);

		vQListWidget.push_back(ui.listWidget_GeShiLiBiao);
		vQListWidget.push_back(ui.listWidget_LiBiao);
		vQListWidget.push_back(ui.listWidget_ShuiZhongLiBiao);
		vQListWidget.push_back(ui.listWidget_ChuanXingDengFuBiao);
		vQListWidget.push_back(ui.listWidget_DengChuan);

		vQListWidget.push_back(ui.listWidget_DaXingFuBiao);
		vQListWidget.push_back(ui.listWidget_ZhuXingFuBiao);
		vQListWidget.push_back(ui.listWidget_GanXingFuBiao);
		vQListWidget.push_back(ui.listWidget_ZhuiXingFuBiao);
		vQListWidget.push_back(ui.listWidget_QiuXingFuBiao);

		vQListWidget.push_back(ui.listWidget_GuanXingFuBiao);
		vQListWidget.push_back(ui.listWidget_TongXingFuBiao);
		vQListWidget.push_back(ui.listWidget_TeShuBiaoZhi);
		vQListWidget.push_back(ui.listWidget_HangHaiLeiDa);
		vQListWidget.push_back(ui.listWidget_WuXianDian);

		vQListWidget.push_back(ui.listWidget_WuHao);
		vQListWidget.push_back(ui.listWidget_HangDaoJiXiangGuanBiaoZhi);
	}

	// 海上区域界线
	if (param.strLayerName == "O")
	{
		vQListWidget.push_back(ui.listWidget_HaiShangGuanLiQu);
		vQListWidget.push_back(ui.listWidget_QingDaoQu);
		vQListWidget.push_back(ui.listWidget_MaoDi);
		vQListWidget.push_back(ui.listWidget_HaiShangXianZhiQu);
		vQListWidget.push_back(ui.listWidget_JinHaiSheShi);
	}

	// 航空要素
	if (param.strLayerName == "P")
	{
		vQListWidget.push_back(ui.listWidget_FeiJiJiChang);
		vQListWidget.push_back(ui.listWidget_HangKongZhangAiWu);
		vQListWidget.push_back(ui.listWidget_HangKongDaoHang);
		vQListWidget.push_back(ui.listWidget_KongZhongQuYuJiJieXian);
		vQListWidget.push_back(ui.listWidget_HangLuHangXian);
		vQListWidget.push_back(ui.listWidget_JiChangChangDao);
	}

	// 军事区域
	if (param.strLayerName == "Q")
	{
		vQListWidget.push_back(ui.listWidget_JunShiXunLianQu);
		vQListWidget.push_back(ui.listWidget_JunShiGuanLiQu);
	}

	// 注记
	if (param.strLayerName == "R")
	{
		vQListWidget.push_back(ui.listWidget_JuMinDiMingCheng);
		vQListWidget.push_back(ui.listWidget_XingZhengQuMingCheng);
		vQListWidget.push_back(ui.listWidget_ZhuanYouMingCheng);

		vQListWidget.push_back(ui.listWidget_HaiYangHuPoHeChuan);
		vQListWidget.push_back(ui.listWidget_AoDiPenDi);
		vQListWidget.push_back(ui.listWidget_DaoBanDao);

		vQListWidget.push_back(ui.listWidget_ShanMaiShanLing);
		vQListWidget.push_back(ui.listWidget_DuLiGaoDi);
		vQListWidget.push_back(ui.listWidget_QiTaZhuJi);
	}

	// 遍历list控件，判断每个item名称是否在要素类名称列表中，如果在，则设置选中状态
	for (int i = 0; i < vQListWidget.size(); i++)
	{
		// 每个list的元素个数
		int iCount = vQListWidget[i]->count();
		for (int iRow = 0; iRow < iCount; iRow++)
		{
			// 获取每个元素
			QString qstrText = vQListWidget[i]->item(iRow)->text();

			// 判断每个元素是否在要素类列表中，如果在，则设置状态为选中
			if (bIsInFeatureClasses(qstrText, param.vFeatureClassCode))
			{
				vQListWidget[i]->item(iRow)->setCheckState(Qt::Checked);
			}
		}
	}
}

/*生成界面配置文件*/
void QgsSetAttributeExtractParamsDlg::pushButton_SaveExtractParams_clicked()
{
	// QDomDocument类
	QDomDocument doc;
	QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
	doc.appendChild(instruction);
	// 创建根节点  QDomElemet元素
	QDomElement root = doc.createElement("LayerMergeParams");
	// 添加根节点
	doc.appendChild(root);

	// 获取界面图层字段匹配列表
	vector<ExtractDataParam> vExtractDataParam;
	vExtractDataParam.clear();
	GetExtractDataParams(vExtractDataParam);

	for (int i = 0; i < vExtractDataParam.size(); i++)
	{
		ExtractDataParam param = vExtractDataParam[i];
		// 如果有选择的匹配字段，则进行接边匹配字段xml的生成
		if (param.vFields.size() > 0)
		{
			// 生成Layer节点
			QDomElement layerNode = doc.createElement("Layer");
			root.appendChild(layerNode);

			// 生成LayerName节点
			QDomElement layerName = doc.createElement("LayerName");
			layerNode.appendChild(layerName);
			QDomText strLayerNameText = doc.createTextNode(param.strLayerName.c_str());
			layerName.appendChild(strLayerNameText);

			// 生成Fields节点
			QDomElement fields = doc.createElement("Fields");
			layerNode.appendChild(fields);
			for (int j = 0; j < param.vFields.size(); j++)
			{
				// 生成Field节点
				QDomElement field = doc.createElement("Field");
				fields.appendChild(field);
				QString qstrField = QString::fromLocal8Bit(param.vFields[j].c_str());
				QDomText strFieldText = doc.createTextNode(qstrField);
				field.appendChild(strFieldText);
			}

			// 生成FeatureClassCodes节点
			QDomElement featureClassCodes = doc.createElement("FeatureClassCodes");
			layerNode.appendChild(featureClassCodes);
			for (int j = 0; j < param.vFeatureClassCode.size(); j++)
			{
				// 生成FeatureClassCode节点
				QDomElement featureClassCode = doc.createElement("FeatureClassCode");
				featureClassCodes.appendChild(featureClassCode);
				QDomText strFeatureClassCodeText = doc.createTextNode(param.vFeatureClassCode[j].c_str());
				featureClassCode.appendChild(strFeatureClassCodeText);
			}
		}
	}

	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = "保存配置文件";
	QString filter = "xml 文件(*.xml)";
	QString strFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!strFileName.isEmpty())
	{
		QFile file(strFileName);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
			return;
		QTextStream out(&file);
		out.setCodec("UTF-8");
		doc.save(out, 4, QDomNode::EncodingFromTextStream);
		file.close();
	}
}

// 设置图层全选状态，即所有图层字段及要素类全部选中
void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllLayers_clicked()
{
	for (int i = 0; i < m_vStrLayerName.size(); i++)
	{
		// 根据图层名称设置要素类列表框全选状态
		SetListWidgetAllSelectedStatus(m_vStrLayerName[i]);

		// 根据图层名称设置字段复选框状态
		SetCheckBoxAllSelectedStatus(m_vStrLayerName[i]);
	}
}

void QgsSetAttributeExtractParamsDlg::Button_OK_accepted()
{
	if (!FileIsExisted(ui.lineEdit_InputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("输入数据库文件不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	if (!FilePathIsExisted(ui.lineEdit_OutputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("输出数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	/*
	add by yangzhilong @2023-08-10
	增加输入数据比例尺与输出数据比例尺是否匹配的判断
	规则如下：如果数据比例尺为25w，选择分幅输出的比例尺需大于数据比例尺
	*/

	/*5万作业区：5W_0101_OperationArea
	 25万作业区：25W_0101_OperationArea
	 100万作业区：100W_OperationArea
	*/

	string strInputDataPath = ui.lineEdit_InputDataPath->text().toLocal8Bit();

	string strDBName = CPLGetBasename(strInputDataPath.c_str());
	
	// 获取比例尺
	int iIndex = strDBName.find_first_of("_");
	string strScale = strDBName.substr(0, iIndex);

	int iScaleIndex = ui.comboBox_Scale->currentIndex();

	if (ui.checkBox_OutputSheet->isChecked())
	{
		// 如果输入数据库存储数据为5W比例尺并且选择分幅输出
		if (strScale == "5W" )
		{
			if (iScaleIndex <= 4)
			{
				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("地理提取与加工"));
				msgBox.setText(tr("输出比例尺应大于输入数据比例尺，请重新选择输出比例尺！"));
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				return;
			}
		}

		// 如果输入数据库存储数据为25W比例尺并且选择分幅输出
		else if (strScale == "25W")
		{
			if (iScaleIndex <= 2)
			{
				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("地理提取与加工"));
				msgBox.setText(tr("输出比例尺应大于输入数据比例尺，请重新选择输出比例尺！"));
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				return;
			}
		}

		// 如果输入数据库存储数据为100W比例尺并且选择分幅输出
		else if (strScale == "100W")
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("地理提取与加工"));
			msgBox.setText(tr("当前比例尺不适合分幅输出！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;		
		}
	}


	accept();
}

void QgsSetAttributeExtractParamsDlg::Button_Cancel_rejected()
{
	reject();
}

void QgsSetAttributeExtractParamsDlg::Button_Help_showHelp()
{
	// TODO:增加帮助文档
	//QgsHelp::openHelp( QStringLiteral( "plugins/core_plugins/plugins_offline_editing.html" ) );
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_A_clicked()
{
	SetListWidgetAllSelectedStatus("A");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_A_clicked()
{
	SetCheckBoxAllSelectedStatus("A");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_A_clicked()
{
	SetListWidgetAllUnSelectedStatus("A");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_A_clicked()
{
	SetCheckBoxAllUnSelectedStatus("A");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_B_clicked()
{
	SetListWidgetAllSelectedStatus("B");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_B_clicked()
{
	SetCheckBoxAllSelectedStatus("B");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_B_clicked()
{
	SetListWidgetAllUnSelectedStatus("B");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_B_clicked()
{
	SetCheckBoxAllUnSelectedStatus("B");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_C_clicked()
{
	SetListWidgetAllSelectedStatus("C");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_C_clicked()
{
	SetCheckBoxAllSelectedStatus("C");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_C_clicked()
{
	SetListWidgetAllUnSelectedStatus("C");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_C_clicked()
{
	SetCheckBoxAllUnSelectedStatus("C");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_D_clicked()
{
	SetListWidgetAllSelectedStatus("D");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_D_clicked()
{
	SetCheckBoxAllSelectedStatus("D");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_D_clicked()
{
	SetListWidgetAllUnSelectedStatus("D");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_D_clicked()
{
	SetCheckBoxAllUnSelectedStatus("D");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_E_clicked()
{
	SetListWidgetAllSelectedStatus("E");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_E_clicked()
{
	SetCheckBoxAllSelectedStatus("E");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_E_clicked()
{
	SetListWidgetAllUnSelectedStatus("E");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_E_clicked()
{
	SetCheckBoxAllUnSelectedStatus("E");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_F_clicked()
{
	SetListWidgetAllSelectedStatus("F");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_F_clicked()
{
	SetCheckBoxAllSelectedStatus("F");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_F_clicked()
{
	SetListWidgetAllUnSelectedStatus("F");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_F_clicked()
{
	SetCheckBoxAllUnSelectedStatus("F");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_G_clicked()
{
	SetListWidgetAllSelectedStatus("G");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_G_clicked()
{
	SetCheckBoxAllSelectedStatus("G");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_G_clicked()
{
	SetListWidgetAllUnSelectedStatus("G");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_G_clicked()
{
	SetCheckBoxAllUnSelectedStatus("G");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_H_clicked()
{
	SetListWidgetAllSelectedStatus("H");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_H_clicked()
{
	SetCheckBoxAllSelectedStatus("H");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_H_clicked()
{
	SetListWidgetAllUnSelectedStatus("H");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_H_clicked()
{
	SetCheckBoxAllUnSelectedStatus("H");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_I_clicked()
{
	SetListWidgetAllSelectedStatus("I");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_I_clicked()
{
	SetCheckBoxAllSelectedStatus("I");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_I_clicked()
{
	SetListWidgetAllUnSelectedStatus("I");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_I_clicked()
{
	SetCheckBoxAllUnSelectedStatus("I");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_J_clicked()
{
	SetListWidgetAllSelectedStatus("J");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_J_clicked()
{
	SetCheckBoxAllSelectedStatus("J");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_J_clicked()
{
	SetListWidgetAllUnSelectedStatus("J");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_J_clicked()
{
	SetCheckBoxAllUnSelectedStatus("J");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_K_clicked()
{
	SetListWidgetAllSelectedStatus("K");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_K_clicked()
{
	SetCheckBoxAllSelectedStatus("K");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_K_clicked()
{
	SetListWidgetAllUnSelectedStatus("K");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_K_clicked()
{
	SetCheckBoxAllUnSelectedStatus("K");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_L_clicked()
{
	SetListWidgetAllSelectedStatus("L");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_L_clicked()
{
	SetCheckBoxAllSelectedStatus("L");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_L_clicked()
{
	SetListWidgetAllUnSelectedStatus("L");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_L_clicked()
{
	SetCheckBoxAllUnSelectedStatus("L");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_M_clicked()
{
	SetListWidgetAllSelectedStatus("M");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_M_clicked()
{
	SetCheckBoxAllSelectedStatus("M");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_M_clicked()
{
	SetListWidgetAllUnSelectedStatus("M");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_M_clicked()
{
	SetCheckBoxAllUnSelectedStatus("M");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_N_clicked()
{
	SetListWidgetAllSelectedStatus("N");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_N_clicked()
{
	SetCheckBoxAllSelectedStatus("N");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_N_clicked()
{
	SetListWidgetAllUnSelectedStatus("N");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_N_clicked()
{
	SetCheckBoxAllUnSelectedStatus("N");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_O_clicked()
{
	SetListWidgetAllSelectedStatus("O");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_O_clicked()
{
	SetCheckBoxAllSelectedStatus("O");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_O_clicked()
{
	SetListWidgetAllUnSelectedStatus("O");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_O_clicked()
{
	SetCheckBoxAllUnSelectedStatus("O");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_P_clicked()
{
	SetListWidgetAllSelectedStatus("P");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_P_clicked()
{
	SetCheckBoxAllSelectedStatus("P");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_P_clicked()
{
	SetListWidgetAllUnSelectedStatus("P");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_P_clicked()
{
	SetCheckBoxAllUnSelectedStatus("P");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_Q_clicked()
{
	SetListWidgetAllSelectedStatus("Q");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_Q_clicked()
{
	SetCheckBoxAllSelectedStatus("Q");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_Q_clicked()
{
	SetListWidgetAllUnSelectedStatus("Q");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_Q_clicked()
{
	SetCheckBoxAllUnSelectedStatus("Q");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFeatureClasses_R_clicked()
{
	SetListWidgetAllSelectedStatus("R");
}

void QgsSetAttributeExtractParamsDlg::pushButton_SelectAllFields_R_clicked()
{
	SetCheckBoxAllSelectedStatus("R");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFeatureClasses_R_clicked()
{
	SetListWidgetAllUnSelectedStatus("R");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllFields_R_clicked()
{
	SetCheckBoxAllUnSelectedStatus("R");
}

void QgsSetAttributeExtractParamsDlg::pushButton_CancelSelectAllLayers_clicked()
{
	// 初始化所有要素类列表、字段列表为不选中状态
	for (int i = 0; i < m_vStrLayerName.size(); i++)
	{
		SetListWidgetAllUnSelectedStatus(m_vStrLayerName[i]);
		SetCheckBoxAllUnSelectedStatus(m_vStrLayerName[i]);
	}
}

// 初始化要素类名称和编码的映射表
void QgsSetAttributeExtractParamsDlg::InitFeatureClassNameCodeTable()
{
	// 获取映射表配置参数
	QString qstrPath = QCoreApplication::applicationDirPath();

#ifdef OS_FAMILY_WINDOWS

	qstrPath += "\\config\\featureclass_name_code.cfg";

#else

	qstrPath += "/config/featureclass_name_code.cfg";

#endif

	FILE* fp = fopen(qstrPath.toStdString().c_str(), "r");
	if (!fp)
	{
		// 记录日志
		return;
	}

	int iCount = 0;		// 要素类对照个数
	char szTemp[300] = { 0 };
	QString qstrCode;
	QString qstrName;
	fscanf(fp, "%d", &iCount);
	for (int i = 0; i < iCount; i++)
	{
		// 要素类编码
		fscanf(fp, "%s", szTemp);
		qstrCode = szTemp;
		memset(szTemp, 0, 300);
		// 要素类名称
		fscanf(fp, "%s", szTemp);
		qstrName = szTemp;
		memset(szTemp, 0, 300);

		m_mName_Codes.insert(MAP_FeatureClassName_Code::value_type(qstrName, qstrCode));
		m_mCode_Names.insert(MAP_FeatureClassCode_Name::value_type(qstrCode, qstrName));
	}

	fclose(fp);

	// 测试输入
	/*FILE* fpout = fopen("d:\\out.txt", "w");
	MAP_FeatureClassName_Code::iterator iter;
	for (iter = m_mName_Codes.begin(); iter != m_mName_Codes.end(); iter++)
	{
		fprintf(fpout, "%s\t%s\n", iter->first.c_str(), iter->second.c_str());
	}

	fclose(fpout);*/
}

// 判断列表中元素是否在要素类列表中
bool QgsSetAttributeExtractParamsDlg::bIsInFeatureClasses(QString qstrText, vector<string>& vFeatureClassCode)
{
	for (int i = 0; i < vFeatureClassCode.size(); i++)
	{
		QString qstrCode = vFeatureClassCode[i].c_str();
		QString qstrName;
		MAP_FeatureClassCode_Name::iterator iter = m_mCode_Names.find(qstrCode);
		if (iter != m_mCode_Names.end())
		{
			qstrName = iter->second;
		}

		if (qstrText == qstrName)
		{
			return true;
		}
	}

	return false;
}

/*UTF-8转GBK*/
string QgsSetAttributeExtractParamsDlg::UTF8_To_GBK(const string& str)
{
	int nwLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);

	wchar_t* pwBuf = new wchar_t[nwLen + 1];
	memset(pwBuf, 0, nwLen * 2 + 2);

	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = WideCharToMultiByte(CP_ACP, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

	char* pBuf = new char[nLen + 1];
	memset(pBuf, 0, nLen + 1);

	WideCharToMultiByte(CP_ACP, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr = pBuf;

	delete[]pBuf;
	delete[]pwBuf;

	pBuf = NULL;
	pwBuf = NULL;

	return retStr;
}

// 根据图层类型设置要素类列表框全选状态
void QgsSetAttributeExtractParamsDlg::SetListWidgetAllSelectedStatus(string strLayerName)
{
	// 存储list控件
	vector<QListWidget*> vQListWidget;
	vQListWidget.clear();

	// 按图层依次进行设置：A到R
	// 测量控制点
	if (strLayerName == "A")
	{
		vQListWidget.push_back(ui.listWidget_CeLiangKongZhiDian);
	}

	// 工农业社会文化设施
	if (strLayerName == "B")
	{
		vQListWidget.push_back(ui.listWidget_GongYe);
		vQListWidget.push_back(ui.listWidget_NongYe);
		vQListWidget.push_back(ui.listWidget_KeXueWenWei);
		vQListWidget.push_back(ui.listWidget_ZhengFuJiGuanZhuDi);
		vQListWidget.push_back(ui.listWidget_GongGongFuWuSheShi);
		vQListWidget.push_back(ui.listWidget_GangKouGuanLi);
		vQListWidget.push_back(ui.listWidget_HangHaiXinHaoTaiZhan);
		vQListWidget.push_back(ui.listWidget_HuanShan);
		vQListWidget.push_back(ui.listWidget_QiTa_B);
	}

	// 居民地及附属设施
	if (strLayerName == "C")
	{
		vQListWidget.push_back(ui.listWidget_DuLiJianZhuWu);
		vQListWidget.push_back(ui.listWidget_JuZhuQu);
		vQListWidget.push_back(ui.listWidget_QiTaJianZhuWu);
	}

	// 陆地交通
	if (strLayerName == "D")
	{
		vQListWidget.push_back(ui.listWidget_TieLu);
		vQListWidget.push_back(ui.listWidget_TieLuCheZhanJiFuShuSheShi);
		vQListWidget.push_back(ui.listWidget_GongLu);
		vQListWidget.push_back(ui.listWidget_QiTaDaoLu);
		vQListWidget.push_back(ui.listWidget_FuShuJianZhuWu);
	}

	// 管线
	if (strLayerName == "E")
	{
		vQListWidget.push_back(ui.listWidget_DianLiXian);
		vQListWidget.push_back(ui.listWidget_TongXinXian);
		vQListWidget.push_back(ui.listWidget_GuanDao);
	}

	// 水域/陆地
	if (strLayerName == "F")
	{
		vQListWidget.push_back(ui.listWidget_AnXianAn);
		vQListWidget.push_back(ui.listWidget_HeLiu);
		vQListWidget.push_back(ui.listWidget_YunHeGouQu);

		vQListWidget.push_back(ui.listWidget_HuPoShuiKuChiTang);
		vQListWidget.push_back(ui.listWidget_ShuiLiSheShi);
		vQListWidget.push_back(ui.listWidget_QiTaShuiXiYaoSu);

		vQListWidget.push_back(ui.listWidget_YiBanDi);
		vQListWidget.push_back(ui.listWidget_FangBoDi);
		vQListWidget.push_back(ui.listWidget_GangKouMaTou);

		vQListWidget.push_back(ui.listWidget_BoWei);
		vQListWidget.push_back(ui.listWidget_LuDiDaoYuHaiYang);
		vQListWidget.push_back(ui.listWidget_Tan);
	}

	// 海底地貌及底质
	if (strLayerName == "G")
	{
		vQListWidget.push_back(ui.listWidget_ShenDu);
		vQListWidget.push_back(ui.listWidget_HaiDiDiZhi);
		vQListWidget.push_back(ui.listWidget_QiTa_G);
	}

	// 礁石、沉船、障碍物
	if (strLayerName == "H")
	{
		vQListWidget.push_back(ui.listWidget_JiaoShi);
		vQListWidget.push_back(ui.listWidget_ChenChuan);
		vQListWidget.push_back(ui.listWidget_ZhangAiWu);
		vQListWidget.push_back(ui.listWidget_BuYuSheShiDeng);
	}

	// 工水文
	if (strLayerName == "I")
	{
		vQListWidget.push_back(ui.listWidget_NeiHeShuiWen);
		vQListWidget.push_back(ui.listWidget_HaiLiuChaoLiu);
		vQListWidget.push_back(ui.listWidget_QiangLieShuiWenXianXiang);
		vQListWidget.push_back(ui.listWidget_ChaoXiChaoXin);
	}

	// 陆地地貌及土质
	if (strLayerName == "J")
	{
		vQListWidget.push_back(ui.listWidget_DengGaoXian);
		vQListWidget.push_back(ui.listWidget_DiMaoGaoCheng);
		vQListWidget.push_back(ui.listWidget_XueShanDiMao);
		vQListWidget.push_back(ui.listWidget_HuangTuDiMao);

		vQListWidget.push_back(ui.listWidget_YanRongDiMao);
		vQListWidget.push_back(ui.listWidget_FengChengDiMao);
		vQListWidget.push_back(ui.listWidget_HuoShanDiMao);
		vQListWidget.push_back(ui.listWidget_QiTaDiMao);
	}

	// 境界与政区
	if (strLayerName == "K")
	{
		vQListWidget.push_back(ui.listWidget_GuoJieJiLingHai);
		vQListWidget.push_back(ui.listWidget_GuoNeiJingJie);
		vQListWidget.push_back(ui.listWidget_XingZhengQu);
		vQListWidget.push_back(ui.listWidget_QiTaJieXian);
	}

	// 植被
	if (strLayerName == "L")
	{
		vQListWidget.push_back(ui.listWidget_LinDi);
		vQListWidget.push_back(ui.listWidget_TianDi);
		vQListWidget.push_back(ui.listWidget_DiLeiJieXian);
	}

	// 地磁要素
	if (strLayerName == "M")
	{
		vQListWidget.push_back(ui.listWidget_DiCiYaoSu);
	}

	// 助航设备及航道
	if (strLayerName == "N")
	{
		vQListWidget.push_back(ui.listWidget_DengBiao);
		vQListWidget.push_back(ui.listWidget_DengTaDengZhuang);
		vQListWidget.push_back(ui.listWidget_HuoJieShiDengZhuang);
		vQListWidget.push_back(ui.listWidget_ShuiZhongDengZhuang);
		vQListWidget.push_back(ui.listWidget_TaXingLiBiao);

		vQListWidget.push_back(ui.listWidget_GeShiLiBiao);
		vQListWidget.push_back(ui.listWidget_LiBiao);
		vQListWidget.push_back(ui.listWidget_ShuiZhongLiBiao);
		vQListWidget.push_back(ui.listWidget_ChuanXingDengFuBiao);
		vQListWidget.push_back(ui.listWidget_DengChuan);

		vQListWidget.push_back(ui.listWidget_DaXingFuBiao);
		vQListWidget.push_back(ui.listWidget_ZhuXingFuBiao);
		vQListWidget.push_back(ui.listWidget_GanXingFuBiao);
		vQListWidget.push_back(ui.listWidget_ZhuiXingFuBiao);
		vQListWidget.push_back(ui.listWidget_QiuXingFuBiao);

		vQListWidget.push_back(ui.listWidget_GuanXingFuBiao);
		vQListWidget.push_back(ui.listWidget_TongXingFuBiao);
		vQListWidget.push_back(ui.listWidget_TeShuBiaoZhi);
		vQListWidget.push_back(ui.listWidget_HangHaiLeiDa);
		vQListWidget.push_back(ui.listWidget_WuXianDian);

		vQListWidget.push_back(ui.listWidget_WuHao);
		vQListWidget.push_back(ui.listWidget_HangDaoJiXiangGuanBiaoZhi);
	}

	// 海上区域界线
	if (strLayerName == "O")
	{
		vQListWidget.push_back(ui.listWidget_HaiShangGuanLiQu);
		vQListWidget.push_back(ui.listWidget_QingDaoQu);
		vQListWidget.push_back(ui.listWidget_MaoDi);
		vQListWidget.push_back(ui.listWidget_HaiShangXianZhiQu);
		vQListWidget.push_back(ui.listWidget_JinHaiSheShi);
	}

	// 航空要素
	if (strLayerName == "P")
	{
		vQListWidget.push_back(ui.listWidget_FeiJiJiChang);
		vQListWidget.push_back(ui.listWidget_HangKongZhangAiWu);
		vQListWidget.push_back(ui.listWidget_HangKongDaoHang);
		vQListWidget.push_back(ui.listWidget_KongZhongQuYuJiJieXian);
		vQListWidget.push_back(ui.listWidget_HangLuHangXian);
		vQListWidget.push_back(ui.listWidget_JiChangChangDao);
	}

	// 军事区域
	if (strLayerName == "Q")
	{
		vQListWidget.push_back(ui.listWidget_JunShiXunLianQu);
		vQListWidget.push_back(ui.listWidget_JunShiGuanLiQu);
	}

	// 注记
	if (strLayerName == "R")
	{
		vQListWidget.push_back(ui.listWidget_JuMinDiMingCheng);
		vQListWidget.push_back(ui.listWidget_XingZhengQuMingCheng);
		vQListWidget.push_back(ui.listWidget_ZhuanYouMingCheng);

		vQListWidget.push_back(ui.listWidget_HaiYangHuPoHeChuan);
		vQListWidget.push_back(ui.listWidget_AoDiPenDi);
		vQListWidget.push_back(ui.listWidget_DaoBanDao);

		vQListWidget.push_back(ui.listWidget_ShanMaiShanLing);
		vQListWidget.push_back(ui.listWidget_DuLiGaoDi);
		vQListWidget.push_back(ui.listWidget_QiTaZhuJi);
	}

	// 遍历list控件,设置选中状态
	for (int i = 0; i < vQListWidget.size(); i++)
	{
		// 每个list的元素个数
		int iCount = vQListWidget[i]->count();
		for (int iRow = 0; iRow < iCount; iRow++)
		{
			// 设置每个元素为选中状态☑
			vQListWidget[i]->item(iRow)->setCheckState(Qt::Checked);
		}
	}
}

void QgsSetAttributeExtractParamsDlg::SetCheckBoxAllSelectedStatus(string strLayerName)
{
	// 按图层依次进行设置：A到R
	// 测量控制点
	if (strLayerName == "A")
	{
		ui.checkBox_YaoSuBianHao_A->setChecked(true);
		ui.checkBox_BianMa_A->setChecked(true);
		ui.checkBox_MingCheng_A->setChecked(true);

		ui.checkBox_LeiXing_A->setChecked(true);
		ui.checkBox_DengJi_A->setChecked(true);
		ui.checkBox_GaoCheng_A->setChecked(true);

		ui.checkBox_BiGao_A->setChecked(true);
		ui.checkBox_LiLunHengZuoBiao_A->setChecked(true);
		ui.checkBox_LiLunZongZuoBiao_A->setChecked(true);

		ui.checkBox_TuXingTeZheng_A->setChecked(true);
		ui.checkBox_ZhuJiZhiZhen_A->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_A->setChecked(true);
	}

	// 工农业社会文化设施
	if (strLayerName == "B")
	{
		ui.checkBox_YaoSuBianHao_B->setChecked(true);
		ui.checkBox_BianMa_B->setChecked(true);
		ui.checkBox_MingCheng_B->setChecked(true);

		ui.checkBox_LeiXing_B->setChecked(true);
		ui.checkBox_LeiBie_B->setChecked(true);
		ui.checkBox_GaoCheng_B->setChecked(true);

		ui.checkBox_BiGao_B->setChecked(true);
		ui.checkBox_XingZhengQuHua_B->setChecked(true);
		ui.checkBox_TuXingTeZheng_B->setChecked(true);

		ui.checkBox_ZhuJiZhiZhen_B->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_B->setChecked(true);
	}

	// 居民地及附属设施
	if (strLayerName == "C")
	{
		ui.checkBox_YaoSuBianHao_C->setChecked(true);
		ui.checkBox_BianMa_C->setChecked(true);
		ui.checkBox_MingCheng_C->setChecked(true);

		ui.checkBox_LeiXing_C->setChecked(true);
		ui.checkBox_JuZhuYueFen_C->setChecked(true);
		ui.checkBox_RenKou_C->setChecked(true);

		ui.checkBox_BiGao_C->setChecked(true);
		ui.checkBox_XingZhengQuHua_C->setChecked(true);
		ui.checkBox_TuXingTeZheng_C->setChecked(true);

		ui.checkBox_ZhuJiZhiZhen_C->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_C->setChecked(true);
	}

	// 陆地交通
	if (strLayerName == "D")
	{
		ui.checkBox_YaoSuBianHao_D->setChecked(true);
		ui.checkBox_BianMa_D->setChecked(true);
		ui.checkBox_MingCheng_D->setChecked(true);

		ui.checkBox_LeiXing_D->setChecked(true);
		ui.checkBox_BianHao_D->setChecked(true);
		ui.checkBox_DengJi_D->setChecked(true);

		ui.checkBox_KuanDu_D->setChecked(true);
		ui.checkBox_PuKuan_D->setChecked(true);
		ui.checkBox_QiaoChang_D->setChecked(true);

		ui.checkBox_JingKongGao_D->setChecked(true);
		ui.checkBox_ZaiZhongDunShu_D->setChecked(true);
		ui.checkBox_LiCheng_D->setChecked(true);

		ui.checkBox_BiGao_D->setChecked(true);
		ui.checkBox_TongXingYueFen_D->setChecked(true);
		ui.checkBox_ShuiShen_D->setChecked(true);

		ui.checkBox_DiZhi_D->setChecked(true);
		ui.checkBox_QuLvBanJing_D->setChecked(true);
		ui.checkBox_ZuiDaZongPo_D->setChecked(true);

		ui.checkBox_TuXingTeZheng_D->setChecked(true);
		ui.checkBox_ZhuJiZhiZhen_D->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_D->setChecked(true);
	}

	// 管线
	if (strLayerName == "E")
	{
		ui.checkBox_YaoSuBianHao_E->setChecked(true);
		ui.checkBox_BianMa_E->setChecked(true);
		ui.checkBox_MingCheng_E->setChecked(true);

		ui.checkBox_LeiXing_E->setChecked(true);
		ui.checkBox_JingKongGao_E->setChecked(true);
		ui.checkBox_MaiCangShenDu_E->setChecked(true);

		ui.checkBox_CunZaiZhuangTai_E->setChecked(true);
		ui.checkBox_ZuoYongFangShi_E->setChecked(true);
		ui.checkBox_XianZhiZhongLei_E->setChecked(true);

		ui.checkBox_TuXingTeZheng_E->setChecked(true);
		ui.checkBox_ZhuJiZhiZhen_E->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_E->setChecked(true);
	}

	// 水域/陆地
	if (strLayerName == "F")
	{
		ui.checkBox_YaoSuBianHao_F->setChecked(true);
		ui.checkBox_BianMa_F->setChecked(true);
		ui.checkBox_MingCheng_F->setChecked(true);

		ui.checkBox_LeiXing_F->setChecked(true);
		ui.checkBox_KuanDu_F->setChecked(true);
		ui.checkBox_ShuiShen_F->setChecked(true);

		ui.checkBox_NiShen_F->setChecked(true);
		ui.checkBox_ShiLingYueFen_F->setChecked(true);
		ui.checkBox_ChangDu_F->setChecked(true);

		ui.checkBox_GaoCheng_F->setChecked(true);
		ui.checkBox_BiGao_F->setChecked(true);
		ui.checkBox_KuRongLiang_F->setChecked(true);

		ui.checkBox_DunWei_F->setChecked(true);
		ui.checkBox_HeLiuDaiMa_F->setChecked(true);
		ui.checkBox_TongHangXingZhi_F->setChecked(true);

		ui.checkBox_ZhiWuLeiXing_F->setChecked(true);
		ui.checkBox_BiaoMianWuZhi_F->setChecked(true);
		ui.checkBox_CunZaiZhuangTai_F->setChecked(true);

		ui.checkBox_WeiZhiZhiLiang_F->setChecked(true);
		ui.checkBox_YanSe_F->setChecked(true);
		ui.checkBox_YuShuiMianGuanXi_F->setChecked(true);

		ui.checkBox_TuXingTeZheng_F->setChecked(true);
		ui.checkBox_ZhuJiZhiZhen_F->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_F->setChecked(true);
	}

	// 海底地貌及底质
	if (strLayerName == "G")
	{
		ui.checkBox_YaoSuBianHao_G->setChecked(true);
		ui.checkBox_BianMa_G->setChecked(true);
		ui.checkBox_MingCheng_G->setChecked(true);

		ui.checkBox_LeiXing_G->setChecked(true);
		ui.checkBox_ShuiShenZhi_G->setChecked(true);
		ui.checkBox_ShuiShenZhi1_G->setChecked(true);

		ui.checkBox_ShuiShenZhi2_G->setChecked(true);
		ui.checkBox_CeShenJiShu_G->setChecked(true);
		ui.checkBox_CeShenZhiLiang_G->setChecked(true);

		ui.checkBox_WeiZhiZhiLiang_G->setChecked(true);
		ui.checkBox_ZuoYongFangShi_G->setChecked(true);
		ui.checkBox_BiaoMianWuZhi_G->setChecked(true);

		ui.checkBox_WuZhiXingTai_G->setChecked(true);
		ui.checkBox_WeiXianJi_G->setChecked(true);
		ui.checkBox_TuXingTeZheng_G->setChecked(true);

		ui.checkBox_ZhuJiZhiZhen_G->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_G->setChecked(true);
	}

	// 礁石、沉船、障碍物
	if (strLayerName == "H")
	{
		ui.checkBox_YaoSuBianHao_H->setChecked(true);
		ui.checkBox_BianMa_H->setChecked(true);
		ui.checkBox_MingCheng_H->setChecked(true);

		ui.checkBox_LeiXing_H->setChecked(true);
		ui.checkBox_ShuiShenZhi_H->setChecked(true);
		ui.checkBox_GaoDu_H->setChecked(true);

		ui.checkBox_ChuiGao_H->setChecked(true);
		ui.checkBox_CunZaiZhuangTai_H->setChecked(true);
		ui.checkBox_CeShenJiShu_H->setChecked(true);

		ui.checkBox_CeShenZhiLiang_H->setChecked(true);
		ui.checkBox_WeiZhiZhiLiang_H->setChecked(true);
		ui.checkBox_XianZhiZhongLei_H->setChecked(true);

		ui.checkBox_ZuoYongFangShi_H->setChecked(true);
		ui.checkBox_BiaoMianWuZhi_H->setChecked(true);
		ui.checkBox_YuShuiMianGuanXi_H->setChecked(true);

		ui.checkBox_WeiXianJi_H->setChecked(true);
		ui.checkBox_TuXingTeZheng_H->setChecked(true);
		ui.checkBox_ZhuJiZhiZhen_H->setChecked(true);

		ui.checkBox_WaiGuaBiaoZhiZhen_H->setChecked(true);
	}

	// 水文
	if (strLayerName == "I")
	{
		ui.checkBox_YaoSuBianHao_I->setChecked(true);
		ui.checkBox_BianMa_I->setChecked(true);
		ui.checkBox_MingCheng_I->setChecked(true);

		ui.checkBox_KuanDu_I->setChecked(true);
		ui.checkBox_ShuiShen_I->setChecked(true);
		ui.checkBox_DiZhi_I->setChecked(true);

		ui.checkBox_LiuSu_I->setChecked(true);
		ui.checkBox_FangWei_I->setChecked(true);
		ui.checkBox_GaoCheng_I->setChecked(true);

		ui.checkBox_YueFen_I->setChecked(true);
		ui.checkBox_MiaoShu_I->setChecked(true);
		ui.checkBox_TuXingTeZheng_I->setChecked(true);

		ui.checkBox_ZhuJiZhiZhen_I->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_I->setChecked(true);
	}

	// 陆地地貌及土质
	if (strLayerName == "J")
	{
		ui.checkBox_YaoSuBianHao_J->setChecked(true);
		ui.checkBox_BianMa_J->setChecked(true);
		ui.checkBox_MingCheng_J->setChecked(true);

		ui.checkBox_LeiXing_J->setChecked(true);
		ui.checkBox_LeiBie_J->setChecked(true);
		ui.checkBox_GaoCheng_J->setChecked(true);

		ui.checkBox_BiGao_J->setChecked(true);
		ui.checkBox_GouKuan_J->setChecked(true);
		ui.checkBox_FangXiang_J->setChecked(true);

		ui.checkBox_TuXingTeZheng_J->setChecked(true);
		ui.checkBox_ZhuJiZhiZhen_J->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_J->setChecked(true);
	}

	// 境界与政区
	if (strLayerName == "K")
	{
		ui.checkBox_YaoSuBianHao_K->setChecked(true);
		ui.checkBox_BianMa_K->setChecked(true);
		ui.checkBox_MingCheng_K->setChecked(true);

		ui.checkBox_LeiXing_K->setChecked(true);
		ui.checkBox_DaiMa_K->setChecked(true);
		ui.checkBox_BianHao_K->setChecked(true);

		ui.checkBox_XuHao_K->setChecked(true);
		ui.checkBox_GaoCheng_K->setChecked(true);
		ui.checkBox_BiGao_K->setChecked(true);

		ui.checkBox_TuXingTeZheng_K->setChecked(true);
		ui.checkBox_ZhuJiZhiZhen_K->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_K->setChecked(true);
	}

	// 植被
	if (strLayerName == "L")
	{
		ui.checkBox_YaoSuBianHao_L->setChecked(true);
		ui.checkBox_BianMa_L->setChecked(true);
		ui.checkBox_MingCheng_L->setChecked(true);

		ui.checkBox_LeiXing_L->setChecked(true);
		ui.checkBox_GaoCheng_L->setChecked(true);
		ui.checkBox_GaoDu_L->setChecked(true);

		ui.checkBox_XiongJing_L->setChecked(true);
		ui.checkBox_KuanDu_L->setChecked(true);
		ui.checkBox_TuXingTeZheng_L->setChecked(true);

		ui.checkBox_ZhuJiZhiZhen_L->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_L->setChecked(true);
	}

	// 地磁要素
	if (strLayerName == "M")
	{
		ui.checkBox_YaoSuBianHao_M->setChecked(true);
		ui.checkBox_BianMa_M->setChecked(true);
		ui.checkBox_MingCheng_M->setChecked(true);

		ui.checkBox_CiChaZhi_M->setChecked(true);
		ui.checkBox_CanKaoNian_M->setChecked(true);
		ui.checkBox_NianBianZhi_M->setChecked(true);

		ui.checkBox_TuXingTeZheng_M->setChecked(true);
		ui.checkBox_ZhuJiZhiZhen_M->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_M->setChecked(true);
	}

	// 助航设备及航道
	if (strLayerName == "N")
	{
		ui.checkBox_YaoSuBianHao_N->setChecked(true);
		ui.checkBox_BianMa_N->setChecked(true);
		ui.checkBox_MingCheng_N->setChecked(true);

		ui.checkBox_LeiXing_N->setChecked(true);
		ui.checkBox_XingZhi_N->setChecked(true);
		ui.checkBox_BianHao_N->setChecked(true);

		ui.checkBox_YanSe_N->setChecked(true);
		ui.checkBox_SeCaiTuAn_N->setChecked(true);
		ui.checkBox_DingBiaoYanSe_N->setChecked(true);

		ui.checkBox_FaGuangZhuangTai_N->setChecked(true);
		ui.checkBox_GaoDu_N->setChecked(true);
		ui.checkBox_DengGuangTeXing_N->setChecked(true);

		ui.checkBox_XinHaoZu_N->setChecked(true);
		ui.checkBox_XinHaoZhouQi_N->setChecked(true);
		ui.checkBox_ZuoYongJuLi_N->setChecked(true);

		ui.checkBox_DengGuangKeShi_N->setChecked(true);
		ui.checkBox_FangWei_N->setChecked(true);
		ui.checkBox_HangXingZhiXiang_N->setChecked(true);

		ui.checkBox_GuangHuJiaoDu1_N->setChecked(true);
		ui.checkBox_GuangHuJiaoDu2_N->setChecked(true);
		ui.checkBox_LeiDaKeShi_N->setChecked(true);

		ui.checkBox_ShuiShenZhi1_N->setChecked(true);
		ui.checkBox_ZuoYongFangShi_N->setChecked(true);
		ui.checkBox_FuBiaoXiTong_N->setChecked(true);

		ui.checkBox_TuXingTeZheng_N->setChecked(true);
		ui.checkBox_ZhuJiZhiZhen_N->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_N->setChecked(true);
	}

	// 海上区域界线
	if (strLayerName == "O")
	{
		ui.checkBox_YaoSuBianHao_O->setChecked(true);
		ui.checkBox_BianMa_O->setChecked(true);
		ui.checkBox_MingCheng_O->setChecked(true);

		ui.checkBox_LeiXing_O->setChecked(true);
		ui.checkBox_GuoJiaDaiMa_O->setChecked(true);
		ui.checkBox_BianHao_O->setChecked(true);

		ui.checkBox_XianZhiZhongLei_O->setChecked(true);
		ui.checkBox_CunZaiZhuangTai_O->setChecked(true);
		ui.checkBox_LeiDaKeShi_O->setChecked(true);

		ui.checkBox_BanJing_O->setChecked(true);
		ui.checkBox_GaoDu_O->setChecked(true);
		ui.checkBox_TuXingTeZheng_O->setChecked(true);

		ui.checkBox_ZhuJiZhiZhen_O->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_O->setChecked(true);
	}

	// 航空要素
	if (strLayerName == "P")
	{
		ui.checkBox_YaoSuBianHao_P->setChecked(true);
		ui.checkBox_BianMa_P->setChecked(true);
		ui.checkBox_MingCheng_P->setChecked(true);

		ui.checkBox_LeiXing_P->setChecked(true);
		ui.checkBox_BianHao_P->setChecked(true);
		ui.checkBox_PaoDaoCiFangXiang_P->setChecked(true);

		ui.checkBox_GaoCheng_P->setChecked(true);
		ui.checkBox_BiGao_P->setChecked(true);
		ui.checkBox_KuanDu_P->setChecked(true);

		ui.checkBox_TuXingTeZheng_P->setChecked(true);
		ui.checkBox_ZhuJiZhiZhen_P->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_P->setChecked(true);
	}

	// 军事区域
	if (strLayerName == "Q")
	{
		ui.checkBox_YaoSuBianHao_Q->setChecked(true);
		ui.checkBox_BianMa_Q->setChecked(true);
		ui.checkBox_MingCheng_Q->setChecked(true);

		ui.checkBox_LeiXing_Q->setChecked(true);
		ui.checkBox_XianZhiZhongLei_Q->setChecked(true);
		ui.checkBox_TuXingTeZheng_Q->setChecked(true);

		ui.checkBox_ZhuJiZhiZhen_Q->setChecked(true);
		ui.checkBox_WaiGuaBiaoZhiZhen_Q->setChecked(true);
	}

	// 注记
	if (strLayerName == "R")
	{
		ui.checkBox_YaoSuBianHao_R->setChecked(true);
		ui.checkBox_BianMa_R->setChecked(true);
		ui.checkBox_MingCheng_R->setChecked(true);

		ui.checkBox_ZiTi_R->setChecked(true);
		ui.checkBox_ZiXing_R->setChecked(true);
		ui.checkBox_ZiJi_R->setChecked(true);

		ui.checkBox_ZiXiang_R->setChecked(true);
		ui.checkBox_YanSe_R->setChecked(true);
	}
}

void QgsSetAttributeExtractParamsDlg::SetListWidgetAllUnSelectedStatus(string strLayerName)
{
	// 存储list控件
	vector<QListWidget*> vQListWidget;
	vQListWidget.clear();

	// 按图层依次进行设置：A到R
	// 测量控制点
	if (strLayerName == "A")
	{
		vQListWidget.push_back(ui.listWidget_CeLiangKongZhiDian);
	}

	// 工农业社会文化设施
	if (strLayerName == "B")
	{
		vQListWidget.push_back(ui.listWidget_GongYe);
		vQListWidget.push_back(ui.listWidget_NongYe);
		vQListWidget.push_back(ui.listWidget_KeXueWenWei);
		vQListWidget.push_back(ui.listWidget_ZhengFuJiGuanZhuDi);
		vQListWidget.push_back(ui.listWidget_GongGongFuWuSheShi);
		vQListWidget.push_back(ui.listWidget_GangKouGuanLi);
		vQListWidget.push_back(ui.listWidget_HangHaiXinHaoTaiZhan);
		vQListWidget.push_back(ui.listWidget_HuanShan);
		vQListWidget.push_back(ui.listWidget_QiTa_B);
	}

	// 居民地及附属设施
	if (strLayerName == "C")
	{
		vQListWidget.push_back(ui.listWidget_DuLiJianZhuWu);
		vQListWidget.push_back(ui.listWidget_JuZhuQu);
		vQListWidget.push_back(ui.listWidget_QiTaJianZhuWu);
	}

	// 陆地交通
	if (strLayerName == "D")
	{
		vQListWidget.push_back(ui.listWidget_TieLu);
		vQListWidget.push_back(ui.listWidget_TieLuCheZhanJiFuShuSheShi);
		vQListWidget.push_back(ui.listWidget_GongLu);
		vQListWidget.push_back(ui.listWidget_QiTaDaoLu);
		vQListWidget.push_back(ui.listWidget_FuShuJianZhuWu);
	}

	// 管线
	if (strLayerName == "E")
	{
		vQListWidget.push_back(ui.listWidget_DianLiXian);
		vQListWidget.push_back(ui.listWidget_TongXinXian);
		vQListWidget.push_back(ui.listWidget_GuanDao);
	}

	// 水域/陆地
	if (strLayerName == "F")
	{
		vQListWidget.push_back(ui.listWidget_AnXianAn);
		vQListWidget.push_back(ui.listWidget_HeLiu);
		vQListWidget.push_back(ui.listWidget_YunHeGouQu);

		vQListWidget.push_back(ui.listWidget_HuPoShuiKuChiTang);
		vQListWidget.push_back(ui.listWidget_ShuiLiSheShi);
		vQListWidget.push_back(ui.listWidget_QiTaShuiXiYaoSu);

		vQListWidget.push_back(ui.listWidget_YiBanDi);
		vQListWidget.push_back(ui.listWidget_FangBoDi);
		vQListWidget.push_back(ui.listWidget_GangKouMaTou);

		vQListWidget.push_back(ui.listWidget_BoWei);
		vQListWidget.push_back(ui.listWidget_LuDiDaoYuHaiYang);
		vQListWidget.push_back(ui.listWidget_Tan);
	}

	// 海底地貌及底质
	if (strLayerName == "G")
	{
		vQListWidget.push_back(ui.listWidget_ShenDu);
		vQListWidget.push_back(ui.listWidget_HaiDiDiZhi);
		vQListWidget.push_back(ui.listWidget_QiTa_G);
	}

	// 礁石、沉船、障碍物
	if (strLayerName == "H")
	{
		vQListWidget.push_back(ui.listWidget_JiaoShi);
		vQListWidget.push_back(ui.listWidget_ChenChuan);
		vQListWidget.push_back(ui.listWidget_ZhangAiWu);
		vQListWidget.push_back(ui.listWidget_BuYuSheShiDeng);
	}

	// 工水文
	if (strLayerName == "I")
	{
		vQListWidget.push_back(ui.listWidget_NeiHeShuiWen);
		vQListWidget.push_back(ui.listWidget_HaiLiuChaoLiu);
		vQListWidget.push_back(ui.listWidget_QiangLieShuiWenXianXiang);
		vQListWidget.push_back(ui.listWidget_ChaoXiChaoXin);
	}

	// 陆地地貌及土质
	if (strLayerName == "J")
	{
		vQListWidget.push_back(ui.listWidget_DengGaoXian);
		vQListWidget.push_back(ui.listWidget_DiMaoGaoCheng);
		vQListWidget.push_back(ui.listWidget_XueShanDiMao);
		vQListWidget.push_back(ui.listWidget_HuangTuDiMao);

		vQListWidget.push_back(ui.listWidget_YanRongDiMao);
		vQListWidget.push_back(ui.listWidget_FengChengDiMao);
		vQListWidget.push_back(ui.listWidget_HuoShanDiMao);
		vQListWidget.push_back(ui.listWidget_QiTaDiMao);
	}

	// 境界与政区
	if (strLayerName == "K")
	{
		vQListWidget.push_back(ui.listWidget_GuoJieJiLingHai);
		vQListWidget.push_back(ui.listWidget_GuoNeiJingJie);
		vQListWidget.push_back(ui.listWidget_XingZhengQu);
		vQListWidget.push_back(ui.listWidget_QiTaJieXian);
	}

	// 植被
	if (strLayerName == "L")
	{
		vQListWidget.push_back(ui.listWidget_LinDi);
		vQListWidget.push_back(ui.listWidget_TianDi);
		vQListWidget.push_back(ui.listWidget_DiLeiJieXian);
	}

	// 地磁要素
	if (strLayerName == "M")
	{
		vQListWidget.push_back(ui.listWidget_DiCiYaoSu);
	}

	// 助航设备及航道
	if (strLayerName == "N")
	{
		vQListWidget.push_back(ui.listWidget_DengBiao);
		vQListWidget.push_back(ui.listWidget_DengTaDengZhuang);
		vQListWidget.push_back(ui.listWidget_HuoJieShiDengZhuang);
		vQListWidget.push_back(ui.listWidget_ShuiZhongDengZhuang);
		vQListWidget.push_back(ui.listWidget_TaXingLiBiao);

		vQListWidget.push_back(ui.listWidget_GeShiLiBiao);
		vQListWidget.push_back(ui.listWidget_LiBiao);
		vQListWidget.push_back(ui.listWidget_ShuiZhongLiBiao);
		vQListWidget.push_back(ui.listWidget_ChuanXingDengFuBiao);
		vQListWidget.push_back(ui.listWidget_DengChuan);

		vQListWidget.push_back(ui.listWidget_DaXingFuBiao);
		vQListWidget.push_back(ui.listWidget_ZhuXingFuBiao);
		vQListWidget.push_back(ui.listWidget_GanXingFuBiao);
		vQListWidget.push_back(ui.listWidget_ZhuiXingFuBiao);
		vQListWidget.push_back(ui.listWidget_QiuXingFuBiao);

		vQListWidget.push_back(ui.listWidget_GuanXingFuBiao);
		vQListWidget.push_back(ui.listWidget_TongXingFuBiao);
		vQListWidget.push_back(ui.listWidget_TeShuBiaoZhi);
		vQListWidget.push_back(ui.listWidget_HangHaiLeiDa);
		vQListWidget.push_back(ui.listWidget_WuXianDian);

		vQListWidget.push_back(ui.listWidget_WuHao);
		vQListWidget.push_back(ui.listWidget_HangDaoJiXiangGuanBiaoZhi);
	}

	// 海上区域界线
	if (strLayerName == "O")
	{
		vQListWidget.push_back(ui.listWidget_HaiShangGuanLiQu);
		vQListWidget.push_back(ui.listWidget_QingDaoQu);
		vQListWidget.push_back(ui.listWidget_MaoDi);
		vQListWidget.push_back(ui.listWidget_HaiShangXianZhiQu);
		vQListWidget.push_back(ui.listWidget_JinHaiSheShi);
	}

	// 航空要素
	if (strLayerName == "P")
	{
		vQListWidget.push_back(ui.listWidget_FeiJiJiChang);
		vQListWidget.push_back(ui.listWidget_HangKongZhangAiWu);
		vQListWidget.push_back(ui.listWidget_HangKongDaoHang);
		vQListWidget.push_back(ui.listWidget_KongZhongQuYuJiJieXian);
		vQListWidget.push_back(ui.listWidget_HangLuHangXian);
		vQListWidget.push_back(ui.listWidget_JiChangChangDao);
	}

	// 军事区域
	if (strLayerName == "Q")
	{
		vQListWidget.push_back(ui.listWidget_JunShiXunLianQu);
		vQListWidget.push_back(ui.listWidget_JunShiGuanLiQu);
	}

	// 注记
	if (strLayerName == "R")
	{
		vQListWidget.push_back(ui.listWidget_JuMinDiMingCheng);
		vQListWidget.push_back(ui.listWidget_XingZhengQuMingCheng);
		vQListWidget.push_back(ui.listWidget_ZhuanYouMingCheng);

		vQListWidget.push_back(ui.listWidget_HaiYangHuPoHeChuan);
		vQListWidget.push_back(ui.listWidget_AoDiPenDi);
		vQListWidget.push_back(ui.listWidget_DaoBanDao);

		vQListWidget.push_back(ui.listWidget_ShanMaiShanLing);
		vQListWidget.push_back(ui.listWidget_DuLiGaoDi);
		vQListWidget.push_back(ui.listWidget_QiTaZhuJi);
	}

	// 遍历list控件,设置选中状态
	for (int i = 0; i < vQListWidget.size(); i++)
	{
		// 每个list的元素个数
		int iCount = vQListWidget[i]->count();
		for (int iRow = 0; iRow < iCount; iRow++)
		{
			// 设置每个元素为选中状态不选中状态
			vQListWidget[i]->item(iRow)->setCheckState(Qt::Unchecked);
		}
	}
}

void QgsSetAttributeExtractParamsDlg::SetCheckBoxAllUnSelectedStatus(string strLayerName)
{
	// 按图层依次进行设置：A到R
	// 测量控制点
	if (strLayerName == "A")
	{
		ui.checkBox_YaoSuBianHao_A->setChecked(false);
		ui.checkBox_BianMa_A->setChecked(false);
		ui.checkBox_MingCheng_A->setChecked(false);

		ui.checkBox_LeiXing_A->setChecked(false);
		ui.checkBox_DengJi_A->setChecked(false);
		ui.checkBox_GaoCheng_A->setChecked(false);

		ui.checkBox_BiGao_A->setChecked(false);
		ui.checkBox_LiLunHengZuoBiao_A->setChecked(false);
		ui.checkBox_LiLunZongZuoBiao_A->setChecked(false);

		ui.checkBox_TuXingTeZheng_A->setChecked(false);
		ui.checkBox_ZhuJiZhiZhen_A->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_A->setChecked(false);
	}

	// 工农业社会文化设施
	if (strLayerName == "B")
	{
		ui.checkBox_YaoSuBianHao_B->setChecked(false);
		ui.checkBox_BianMa_B->setChecked(false);
		ui.checkBox_MingCheng_B->setChecked(false);

		ui.checkBox_LeiXing_B->setChecked(false);
		ui.checkBox_LeiBie_B->setChecked(false);
		ui.checkBox_GaoCheng_B->setChecked(false);

		ui.checkBox_BiGao_B->setChecked(false);
		ui.checkBox_XingZhengQuHua_B->setChecked(false);
		ui.checkBox_TuXingTeZheng_B->setChecked(false);

		ui.checkBox_ZhuJiZhiZhen_B->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_B->setChecked(false);
	}

	// 居民地及附属设施
	if (strLayerName == "C")
	{
		ui.checkBox_YaoSuBianHao_C->setChecked(false);
		ui.checkBox_BianMa_C->setChecked(false);
		ui.checkBox_MingCheng_C->setChecked(false);

		ui.checkBox_LeiXing_C->setChecked(false);
		ui.checkBox_JuZhuYueFen_C->setChecked(false);
		ui.checkBox_RenKou_C->setChecked(false);

		ui.checkBox_BiGao_C->setChecked(false);
		ui.checkBox_XingZhengQuHua_C->setChecked(false);
		ui.checkBox_TuXingTeZheng_C->setChecked(false);

		ui.checkBox_ZhuJiZhiZhen_C->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_C->setChecked(false);
	}

	// 陆地交通
	if (strLayerName == "D")
	{
		ui.checkBox_YaoSuBianHao_D->setChecked(false);
		ui.checkBox_BianMa_D->setChecked(false);
		ui.checkBox_MingCheng_D->setChecked(false);

		ui.checkBox_LeiXing_D->setChecked(false);
		ui.checkBox_BianHao_D->setChecked(false);
		ui.checkBox_DengJi_D->setChecked(false);

		ui.checkBox_KuanDu_D->setChecked(false);
		ui.checkBox_PuKuan_D->setChecked(false);
		ui.checkBox_QiaoChang_D->setChecked(false);

		ui.checkBox_JingKongGao_D->setChecked(false);
		ui.checkBox_ZaiZhongDunShu_D->setChecked(false);
		ui.checkBox_LiCheng_D->setChecked(false);

		ui.checkBox_BiGao_D->setChecked(false);
		ui.checkBox_TongXingYueFen_D->setChecked(false);
		ui.checkBox_ShuiShen_D->setChecked(false);

		ui.checkBox_DiZhi_D->setChecked(false);
		ui.checkBox_QuLvBanJing_D->setChecked(false);
		ui.checkBox_ZuiDaZongPo_D->setChecked(false);

		ui.checkBox_TuXingTeZheng_D->setChecked(false);
		ui.checkBox_ZhuJiZhiZhen_D->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_D->setChecked(false);
	}

	// 管线
	if (strLayerName == "E")
	{
		ui.checkBox_YaoSuBianHao_E->setChecked(false);
		ui.checkBox_BianMa_E->setChecked(false);
		ui.checkBox_MingCheng_E->setChecked(false);

		ui.checkBox_LeiXing_E->setChecked(false);
		ui.checkBox_JingKongGao_E->setChecked(false);
		ui.checkBox_MaiCangShenDu_E->setChecked(false);

		ui.checkBox_CunZaiZhuangTai_E->setChecked(false);
		ui.checkBox_ZuoYongFangShi_E->setChecked(false);
		ui.checkBox_XianZhiZhongLei_E->setChecked(false);

		ui.checkBox_TuXingTeZheng_E->setChecked(false);
		ui.checkBox_ZhuJiZhiZhen_E->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_E->setChecked(false);
	}

	// 水域/陆地
	if (strLayerName == "F")
	{
		ui.checkBox_YaoSuBianHao_F->setChecked(false);
		ui.checkBox_BianMa_F->setChecked(false);
		ui.checkBox_MingCheng_F->setChecked(false);

		ui.checkBox_LeiXing_F->setChecked(false);
		ui.checkBox_KuanDu_F->setChecked(false);
		ui.checkBox_ShuiShen_F->setChecked(false);

		ui.checkBox_NiShen_F->setChecked(false);
		ui.checkBox_ShiLingYueFen_F->setChecked(false);
		ui.checkBox_ChangDu_F->setChecked(false);

		ui.checkBox_GaoCheng_F->setChecked(false);
		ui.checkBox_BiGao_F->setChecked(false);
		ui.checkBox_KuRongLiang_F->setChecked(false);

		ui.checkBox_DunWei_F->setChecked(false);
		ui.checkBox_HeLiuDaiMa_F->setChecked(false);
		ui.checkBox_TongHangXingZhi_F->setChecked(false);

		ui.checkBox_ZhiWuLeiXing_F->setChecked(false);
		ui.checkBox_BiaoMianWuZhi_F->setChecked(false);
		ui.checkBox_CunZaiZhuangTai_F->setChecked(false);

		ui.checkBox_WeiZhiZhiLiang_F->setChecked(false);
		ui.checkBox_YanSe_F->setChecked(false);
		ui.checkBox_YuShuiMianGuanXi_F->setChecked(false);

		ui.checkBox_TuXingTeZheng_F->setChecked(false);
		ui.checkBox_ZhuJiZhiZhen_F->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_F->setChecked(false);
	}

	// 海底地貌及底质
	if (strLayerName == "G")
	{
		ui.checkBox_YaoSuBianHao_G->setChecked(false);
		ui.checkBox_BianMa_G->setChecked(false);
		ui.checkBox_MingCheng_G->setChecked(false);

		ui.checkBox_LeiXing_G->setChecked(false);
		ui.checkBox_ShuiShenZhi_G->setChecked(false);
		ui.checkBox_ShuiShenZhi1_G->setChecked(false);

		ui.checkBox_ShuiShenZhi2_G->setChecked(false);
		ui.checkBox_CeShenJiShu_G->setChecked(false);
		ui.checkBox_CeShenZhiLiang_G->setChecked(false);

		ui.checkBox_WeiZhiZhiLiang_G->setChecked(false);
		ui.checkBox_ZuoYongFangShi_G->setChecked(false);
		ui.checkBox_BiaoMianWuZhi_G->setChecked(false);

		ui.checkBox_WuZhiXingTai_G->setChecked(false);
		ui.checkBox_WeiXianJi_G->setChecked(false);
		ui.checkBox_TuXingTeZheng_G->setChecked(false);

		ui.checkBox_ZhuJiZhiZhen_G->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_G->setChecked(false);
	}

	// 礁石、沉船、障碍物
	if (strLayerName == "H")
	{
		ui.checkBox_YaoSuBianHao_H->setChecked(false);
		ui.checkBox_BianMa_H->setChecked(false);
		ui.checkBox_MingCheng_H->setChecked(false);

		ui.checkBox_LeiXing_H->setChecked(false);
		ui.checkBox_ShuiShenZhi_H->setChecked(false);
		ui.checkBox_GaoDu_H->setChecked(false);

		ui.checkBox_ChuiGao_H->setChecked(false);
		ui.checkBox_CunZaiZhuangTai_H->setChecked(false);
		ui.checkBox_CeShenJiShu_H->setChecked(false);

		ui.checkBox_CeShenZhiLiang_H->setChecked(false);
		ui.checkBox_WeiZhiZhiLiang_H->setChecked(false);
		ui.checkBox_XianZhiZhongLei_H->setChecked(false);

		ui.checkBox_ZuoYongFangShi_H->setChecked(false);
		ui.checkBox_BiaoMianWuZhi_H->setChecked(false);
		ui.checkBox_YuShuiMianGuanXi_H->setChecked(false);

		ui.checkBox_WeiXianJi_H->setChecked(false);
		ui.checkBox_TuXingTeZheng_H->setChecked(false);
		ui.checkBox_ZhuJiZhiZhen_H->setChecked(false);

		ui.checkBox_WaiGuaBiaoZhiZhen_H->setChecked(false);
	}

	// 水文
	if (strLayerName == "I")
	{
		ui.checkBox_YaoSuBianHao_I->setChecked(false);
		ui.checkBox_BianMa_I->setChecked(false);
		ui.checkBox_MingCheng_I->setChecked(false);

		ui.checkBox_KuanDu_I->setChecked(false);
		ui.checkBox_ShuiShen_I->setChecked(false);
		ui.checkBox_DiZhi_I->setChecked(false);

		ui.checkBox_LiuSu_I->setChecked(false);
		ui.checkBox_FangWei_I->setChecked(false);
		ui.checkBox_GaoCheng_I->setChecked(false);

		ui.checkBox_YueFen_I->setChecked(false);
		ui.checkBox_MiaoShu_I->setChecked(false);
		ui.checkBox_TuXingTeZheng_I->setChecked(false);

		ui.checkBox_ZhuJiZhiZhen_I->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_I->setChecked(false);
	}

	// 陆地地貌及土质
	if (strLayerName == "J")
	{
		ui.checkBox_YaoSuBianHao_J->setChecked(false);
		ui.checkBox_BianMa_J->setChecked(false);
		ui.checkBox_MingCheng_J->setChecked(false);

		ui.checkBox_LeiXing_J->setChecked(false);
		ui.checkBox_LeiBie_J->setChecked(false);
		ui.checkBox_GaoCheng_J->setChecked(false);

		ui.checkBox_BiGao_J->setChecked(false);
		ui.checkBox_GouKuan_J->setChecked(false);
		ui.checkBox_FangXiang_J->setChecked(false);

		ui.checkBox_TuXingTeZheng_J->setChecked(false);
		ui.checkBox_ZhuJiZhiZhen_J->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_J->setChecked(false);
	}

	// 境界与政区
	if (strLayerName == "K")
	{
		ui.checkBox_YaoSuBianHao_K->setChecked(false);
		ui.checkBox_BianMa_K->setChecked(false);
		ui.checkBox_MingCheng_K->setChecked(false);

		ui.checkBox_LeiXing_K->setChecked(false);
		ui.checkBox_DaiMa_K->setChecked(false);
		ui.checkBox_BianHao_K->setChecked(false);

		ui.checkBox_XuHao_K->setChecked(false);
		ui.checkBox_GaoCheng_K->setChecked(false);
		ui.checkBox_BiGao_K->setChecked(false);

		ui.checkBox_TuXingTeZheng_K->setChecked(false);
		ui.checkBox_ZhuJiZhiZhen_K->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_K->setChecked(false);
	}

	// 植被
	if (strLayerName == "L")
	{
		ui.checkBox_YaoSuBianHao_L->setChecked(false);
		ui.checkBox_BianMa_L->setChecked(false);
		ui.checkBox_MingCheng_L->setChecked(false);

		ui.checkBox_LeiXing_L->setChecked(false);
		ui.checkBox_GaoCheng_L->setChecked(false);
		ui.checkBox_GaoDu_L->setChecked(false);

		ui.checkBox_XiongJing_L->setChecked(false);
		ui.checkBox_KuanDu_L->setChecked(false);
		ui.checkBox_TuXingTeZheng_L->setChecked(false);

		ui.checkBox_ZhuJiZhiZhen_L->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_L->setChecked(false);
	}

	// 地磁要素
	if (strLayerName == "M")
	{
		ui.checkBox_YaoSuBianHao_M->setChecked(false);
		ui.checkBox_BianMa_M->setChecked(false);
		ui.checkBox_MingCheng_M->setChecked(false);

		ui.checkBox_CiChaZhi_M->setChecked(false);
		ui.checkBox_CanKaoNian_M->setChecked(false);
		ui.checkBox_NianBianZhi_M->setChecked(false);

		ui.checkBox_TuXingTeZheng_M->setChecked(false);
		ui.checkBox_ZhuJiZhiZhen_M->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_M->setChecked(false);
	}

	// 助航设备及航道
	if (strLayerName == "N")
	{
		ui.checkBox_YaoSuBianHao_N->setChecked(false);
		ui.checkBox_BianMa_N->setChecked(false);
		ui.checkBox_MingCheng_N->setChecked(false);

		ui.checkBox_LeiXing_N->setChecked(false);
		ui.checkBox_XingZhi_N->setChecked(false);
		ui.checkBox_BianHao_N->setChecked(false);

		ui.checkBox_YanSe_N->setChecked(false);
		ui.checkBox_SeCaiTuAn_N->setChecked(false);
		ui.checkBox_DingBiaoYanSe_N->setChecked(false);

		ui.checkBox_FaGuangZhuangTai_N->setChecked(false);
		ui.checkBox_GaoDu_N->setChecked(false);
		ui.checkBox_DengGuangTeXing_N->setChecked(false);

		ui.checkBox_XinHaoZu_N->setChecked(false);
		ui.checkBox_XinHaoZhouQi_N->setChecked(false);
		ui.checkBox_ZuoYongJuLi_N->setChecked(false);

		ui.checkBox_DengGuangKeShi_N->setChecked(false);
		ui.checkBox_FangWei_N->setChecked(false);
		ui.checkBox_HangXingZhiXiang_N->setChecked(false);

		ui.checkBox_GuangHuJiaoDu1_N->setChecked(false);
		ui.checkBox_GuangHuJiaoDu2_N->setChecked(false);
		ui.checkBox_LeiDaKeShi_N->setChecked(false);

		ui.checkBox_ShuiShenZhi1_N->setChecked(false);
		ui.checkBox_ZuoYongFangShi_N->setChecked(false);
		ui.checkBox_FuBiaoXiTong_N->setChecked(false);

		ui.checkBox_TuXingTeZheng_N->setChecked(false);
		ui.checkBox_ZhuJiZhiZhen_N->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_N->setChecked(false);
	}

	// 海上区域界线
	if (strLayerName == "O")
	{
		ui.checkBox_YaoSuBianHao_O->setChecked(false);
		ui.checkBox_BianMa_O->setChecked(false);
		ui.checkBox_MingCheng_O->setChecked(false);

		ui.checkBox_LeiXing_O->setChecked(false);
		ui.checkBox_GuoJiaDaiMa_O->setChecked(false);
		ui.checkBox_BianHao_O->setChecked(false);

		ui.checkBox_XianZhiZhongLei_O->setChecked(false);
		ui.checkBox_CunZaiZhuangTai_O->setChecked(false);
		ui.checkBox_LeiDaKeShi_O->setChecked(false);

		ui.checkBox_BanJing_O->setChecked(false);
		ui.checkBox_GaoDu_O->setChecked(false);
		ui.checkBox_TuXingTeZheng_O->setChecked(false);

		ui.checkBox_ZhuJiZhiZhen_O->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_O->setChecked(false);
	}

	// 航空要素
	if (strLayerName == "P")
	{
		ui.checkBox_YaoSuBianHao_P->setChecked(false);
		ui.checkBox_BianMa_P->setChecked(false);
		ui.checkBox_MingCheng_P->setChecked(false);

		ui.checkBox_LeiXing_P->setChecked(false);
		ui.checkBox_BianHao_P->setChecked(false);
		ui.checkBox_PaoDaoCiFangXiang_P->setChecked(false);

		ui.checkBox_GaoCheng_P->setChecked(false);
		ui.checkBox_BiGao_P->setChecked(false);
		ui.checkBox_KuanDu_P->setChecked(false);

		ui.checkBox_TuXingTeZheng_P->setChecked(false);
		ui.checkBox_ZhuJiZhiZhen_P->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_P->setChecked(false);
	}

	// 军事区域
	if (strLayerName == "Q")
	{
		ui.checkBox_YaoSuBianHao_Q->setChecked(false);
		ui.checkBox_BianMa_Q->setChecked(false);
		ui.checkBox_MingCheng_Q->setChecked(false);

		ui.checkBox_LeiXing_Q->setChecked(false);
		ui.checkBox_XianZhiZhongLei_Q->setChecked(false);
		ui.checkBox_TuXingTeZheng_Q->setChecked(false);

		ui.checkBox_ZhuJiZhiZhen_Q->setChecked(false);
		ui.checkBox_WaiGuaBiaoZhiZhen_Q->setChecked(false);
	}

	// 注记
	if (strLayerName == "R")
	{
		ui.checkBox_YaoSuBianHao_R->setChecked(false);
		ui.checkBox_BianMa_R->setChecked(false);
		ui.checkBox_MingCheng_R->setChecked(false);

		ui.checkBox_ZiTi_R->setChecked(false);
		ui.checkBox_ZiXing_R->setChecked(false);
		ui.checkBox_ZiJi_R->setChecked(false);

		ui.checkBox_ZiXiang_R->setChecked(false);
		ui.checkBox_YanSe_R->setChecked(false);
	}
}


// 判断目录是否存在
bool QgsSetAttributeExtractParamsDlg::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool QgsSetAttributeExtractParamsDlg::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


void QgsSetAttributeExtractParamsDlg::on_checkBox_OutputSheet_stateChanged(int arg1)
{
	if (arg1 == Qt::CheckState::Checked)
	{
		ui.comboBox_Scale->setEnabled(true);
	}
	else
	{
		ui.comboBox_Scale->setEnabled(false);
	}
}
