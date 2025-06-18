#define _HAS_STD_BYTE 0
// 使用json头文件
#include <nlohmann/json.hpp>
#include "se_manage_metadata.h"

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
#include "cpl_config.h"

#include "gdal_priv.h"
#include <ogrsf_frmts.h>
#include <ogr_feature.h>

#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#endif //
#define _atoi64(val)   strtoll(val,NULL,10)

/*--------------STL---------------*/
#include <filesystem>
#include <fstream>
#include <iostream>

/*--------------STL---------------*/

#include "iconv.h"

using ordered_json = nlohmann::ordered_json;
using namespace std;

CSE_ManageMetaDataDlg::CSE_ManageMetaDataDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.pushButton_OK, &QPushButton::clicked, this, &CSE_ManageMetaDataDlg::pushButton_OK_clicked);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &CSE_ManageMetaDataDlg::pushButton_Cancel_clicked);
	
	connect(ui.Button_OpenMetaData, &QPushButton::clicked, this, &CSE_ManageMetaDataDlg::pushButton_OpenMetaData_clicked);
	connect(ui.Button_SaveMetaData, &QPushButton::clicked, this, &CSE_ManageMetaDataDlg::pushButton_SaveMetaData_clicked);
	connect(ui.pushButton_UpdateMetadataRange, &QPushButton::clicked, this, &CSE_ManageMetaDataDlg::pushButton_UpdateMetadataRange_clicked);
	connect(ui.pushButton_Open, &QPushButton::clicked, this, &CSE_ManageMetaDataDlg::pushButton_OpenARD_clicked);



	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputARDDataPath);
	ui.lineEdit_InputMetaDataPath->setText(m_qstrInputDataPath);
    ui.lineEdit_OutputMetaDataPath->setText(m_qstrOutputDataPath);

	// 默认选第一个tab页
	ui.tabWidget->setCurrentIndex(0);

	// 表头居中
	ui.tableWidget_ARD->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中
	ui.tableWidget_BSXX->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中
	ui.tableWidget_GLXX->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中
	ui.tableWidget_ZLXX->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中
	ui.tableWidget_ZTXX->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_ARD->setAlternatingRowColors(true);
	ui.tableWidget_BSXX->setAlternatingRowColors(true);
	ui.tableWidget_GLXX->setAlternatingRowColors(true);
	ui.tableWidget_ZLXX->setAlternatingRowColors(true);
	ui.tableWidget_ZTXX->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_ARD->horizontalHeader()->setStretchLastSection(true);
	ui.tableWidget_BSXX->horizontalHeader()->setStretchLastSection(true);
	ui.tableWidget_GLXX->horizontalHeader()->setStretchLastSection(true);
	ui.tableWidget_ZLXX->horizontalHeader()->setStretchLastSection(true);
	ui.tableWidget_ZTXX->horizontalHeader()->setStretchLastSection(true);

	m_vMetaDatas.clear();
}

CSE_ManageMetaDataDlg::~CSE_ManageMetaDataDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("MANAGE_VECTOR_ARD_METADATA/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("MANAGE_VECTOR_ARD_METADATA/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("MANAGE_VECTOR_ARD_METADATA/InputARDDataPath"), m_qstrInputARDDataPath, QgsSettings::Section::Plugins);
}

void CSE_ManageMetaDataDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("MANAGE_VECTOR_ARD_METADATA/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("MANAGE_VECTOR_ARD_METADATA/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrInputARDDataPath = settings.value(QStringLiteral("MANAGE_VECTOR_ARD_METADATA/InputARDDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}



// 获取输入数据路径
QString CSE_ManageMetaDataDlg::GetInputDataPath()
{
	return ui.lineEdit_InputMetaDataPath->text();
}



void CSE_ManageMetaDataDlg::UpdateTableWidget(vector<MetaDataTemplateInfo> &vInfo, QTableWidget *pTableWidget)
{
	// 清空表格内容
	pTableWidget->clearContents();

	for (int i = 0; i < vInfo.size(); ++i)
	{
		// 在表格中增加记录
		int iRowCount = 0;
		iRowCount = pTableWidget->rowCount();

		// 增加一行
		pTableWidget->insertRow(iRowCount);

		// 【1】数据项
		pTableWidget->setItem(iRowCount, 0, new QTableWidgetItem(QString::fromUtf8(vInfo[i].strItem.c_str())));

		// 【2】数据类型
		pTableWidget->setItem(iRowCount, 1, new QTableWidgetItem(QString::fromUtf8(vInfo[i].strType.c_str())));

		// 【3】格式
		pTableWidget->setItem(iRowCount, 2, new QTableWidgetItem(QString::fromUtf8(vInfo[i].strFormat.c_str())));

		// 【4】备注
		pTableWidget->setItem(iRowCount, 3, new QTableWidgetItem(QString::fromUtf8(vInfo[i].strDetail.c_str())));

		// 【5】属性值
		pTableWidget->setItem(iRowCount, 4, new QTableWidgetItem(QString::fromUtf8(vInfo[i].strValue.c_str())));

	}


	// 设置表格中某些列内容不可编辑
	for (int i = 0; i < pTableWidget->rowCount(); ++i)
	{
		if (pTableWidget->item(i, 0))
		{
			// 数据项不可编辑
			pTableWidget->item(i, 0)->setFlags(Qt::NoItemFlags);
		}

		if (pTableWidget->item(i, 1))
		{
			// 数据类型不可编辑
			pTableWidget->item(i, 1)->setFlags(Qt::NoItemFlags);
		}

		if (pTableWidget->item(i, 2))
		{
			// 格式不可编辑
			pTableWidget->item(i, 2)->setFlags(Qt::NoItemFlags);
		}

		if (pTableWidget->item(i, 3))
		{
			// 备注不可编辑
			pTableWidget->item(i, 3)->setFlags(Qt::NoItemFlags);
		}
	}


	for (int i = 0; i < pTableWidget->rowCount(); ++i)
	{
		for (int j = 0; j < pTableWidget->columnCount(); ++j)
		{
            pTableWidget->item(i, j)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
			
		}
	}

	// 调用 pTableWidget 对象的 resizeColumnsToContents() 方法
	// 该方法会自动调整表格的列宽，使其能够容纳列中的内容
	pTableWidget->resizeColumnsToContents();

}


void CSE_ManageMetaDataDlg::pushButton_OpenMetaData_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择矢量分析就绪型数据元数据文件");
	QString filter = tr("json 文件(*.json)");
	QString qstrFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (qstrFileName.isEmpty())
	{
		return;
	}

	string strFileName = qstrFileName.toLocal8Bit();
	ui.lineEdit_InputMetaDataPath->setText(qstrFileName);
	
	VectorDataMetadataInfo sVectorMetaInfo;
	// 读取矢量分析就绪数据元数据
	if (!ReadMetaDataJsonFile(strFileName, sVectorMetaInfo))
	{
        QMessageBox::information(this, tr("提示"), tr("读取元数据文件失败！"));
		return;
	}

#pragma region "填充1-管理信息表格"

	/*
	元数据类	数据项	数据元素简称	数据类型	格式	备注
	管理信息	产品名称	GLXX_CPMC	字符型		20C		缺省：NULL
	管理信息	保管单位	GLXX_BGDW	字符型		30C		缺省：NULL
	管理信息	生产单位	GLXX_SCDW	字符型		30C		缺省：NULL
	管理信息	生产日期	GLXX_SCRQ	字符型		8C		YYYYMMDD
	管理信息	更新日期	GLXX_GXRQ	字符型		8C		YYYYMMDD
	管理信息	产品密级	GLXX_CPMJ	字符型		6C		绝密、机密、秘密、内部、公开
	管理信息	产品版本	GLXX_CPBB	字符型		30C		成果的版本号，通常包含生产完成日期信息
	*/

	// 管理信息元数据
	vector<MetaDataTemplateInfo> vGLXXInfos;
	vGLXXInfos.clear();

	MetaDataTemplateInfo sGLXXInfo;
	sGLXXInfo.strItem = "产品名称";
	sGLXXInfo.strType = "字符型";
	sGLXXInfo.strFormat = "20C";
	sGLXXInfo.strDetail = "缺省：NULL";
	sGLXXInfo.strValue = sVectorMetaInfo.strGLXX_CPMC;
	vGLXXInfos.push_back(sGLXXInfo);

	sGLXXInfo.strItem = "保管单位";
	sGLXXInfo.strType = "字符型";
	sGLXXInfo.strFormat = "30C";
	sGLXXInfo.strDetail = "缺省：NULL";
	sGLXXInfo.strValue = sVectorMetaInfo.strGLXX_BGDW;
	vGLXXInfos.push_back(sGLXXInfo);

	sGLXXInfo.strItem = "生产单位";
	sGLXXInfo.strType = "字符型";
	sGLXXInfo.strFormat = "30C";
	sGLXXInfo.strDetail = "缺省：NULL";
	sGLXXInfo.strValue = sVectorMetaInfo.strGLXX_SCDW;
	vGLXXInfos.push_back(sGLXXInfo);

	sGLXXInfo.strItem = "生产日期";
	sGLXXInfo.strType = "字符型";
	sGLXXInfo.strFormat = "8C";
	sGLXXInfo.strDetail = "YYYYMMDD";
    sGLXXInfo.strValue = sVectorMetaInfo.strGLXX_SCRQ;
	vGLXXInfos.push_back(sGLXXInfo);

	sGLXXInfo.strItem = "更新日期";
	sGLXXInfo.strType = "字符型";
	sGLXXInfo.strFormat = "8C";
	sGLXXInfo.strDetail = "YYYYMMDD";
	sGLXXInfo.strValue = sVectorMetaInfo.strGLXX_GXRQ;
	vGLXXInfos.push_back(sGLXXInfo);

	sGLXXInfo.strItem = "产品密级";
	sGLXXInfo.strType = "字符型";
	sGLXXInfo.strFormat = "6C";
	sGLXXInfo.strDetail = "绝密、机密、秘密、内部、公开";
    sGLXXInfo.strValue = sVectorMetaInfo.strGLXX_CPMJ;
	vGLXXInfos.push_back(sGLXXInfo);

	sGLXXInfo.strItem = "产品版本";
	sGLXXInfo.strType = "字符型";
	sGLXXInfo.strFormat = "30C";
	sGLXXInfo.strDetail = "成果的版本号，通常包含生产完成日期信息";
    sGLXXInfo.strValue = sVectorMetaInfo.strGLXX_CPBB;
	vGLXXInfos.push_back(sGLXXInfo);

#pragma endregion

#pragma region "填充2-标识信息表格"

	// 填充2-标识信息表格
	/*
	* 元数据类	数据项		数据元素简称	数据类型	格式	备注
	标识信息	关键字		BSXX_GJZ		字符型		30C		数据关键内容的描述
	标识信息	数据量		BSXX_SJL		字符型		30C		单位“MB”
	标识信息	数据格式	BSXX_SJGS		字符型		30C		参照规范名称编号
	标识信息	数据来源	BSXX_SJLY		字符型		256C	融合数据的来源
	标识信息	分幅命名规范	BSXX_FFMMGF	字符型		30C		参照规范名称编号
	标识信息	图名		BSXX_TM			字符型		30C		如：“丰镇市”
	标识信息	图号		BSXX_TH			字符型		30C		如：LN050

	*/

	// 标识信息元数据
	vector<MetaDataTemplateInfo> vBSXXInfos;
	vBSXXInfos.clear();

	MetaDataTemplateInfo sBSXXInfo;
	sBSXXInfo.strItem = "关键字";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "数据关键内容的描述";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_GJZ;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "数据量";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "单位“MB”";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_SJL;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "数据格式";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "参照规范名称编号";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_SJGS;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "数据来源";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "256C";
	sBSXXInfo.strDetail = "融合数据的来源";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_SJLY;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "分幅命名规范";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "参照规范名称编号";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_FFMMGF;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "图名";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "如：“丰镇市”";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_TM;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "图号";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "如：LN050";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_TH;
	vBSXXInfos.push_back(sBSXXInfo);


	/*	
	标识信息	比例尺分母	BSXX_BLCFM		短整型		12D		“50000”
	标识信息	数据四至范围	BSXX_SJSZFW	字符型		30C		数组，有4个元素，均为浮点型分别是左下和右上纬度、经度坐标
	标识信息	坐标系统	BSXX_ZBXT		字符型		30C		成果使用的大地基准名称
	标识信息	高程基准	BSXX_GCJZ		字符型		30C		成果使用的高程基准名称
	标识信息	深度基准	BSXX_SDJZ		字符型		30C		成果使用的深度基准名称
	标识信息	椭球长半径	BSXX_TQCBZ		浮点型		11.2F	单位：米，缺省：-32767.00
	标识信息	椭球扁率	BSXX_TQBL		浮点型		16.9F	缺省：-32767.000000000
	标识信息	地图投影	BSXX_DTTY		字符型		30C		无投影、高斯-克吕格、等角圆锥、墨卡托、方位等
	标识信息	中央经线	BSXX_ZYJX		短整型		8D		单位：度，-32767
	标识信息	标准纬线	BSXX_BZWX		浮点型		10.5F	单位：度，-32767.0
	标识信息	坐标单位	BSXX_ZBDW		字符型		30C		“度”、“米”*/

	sBSXXInfo.strItem = "比例尺分母";
	sBSXXInfo.strType = "短整型";
	sBSXXInfo.strFormat = "12D";
	sBSXXInfo.strDetail = "“50000”";
	char szTemp[50] = { 0 };
	sprintf(szTemp, "%d", sVectorMetaInfo.iBSXX_BLCFM);
    sBSXXInfo.strValue = szTemp;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "数据四至范围";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "100C";
	sBSXXInfo.strDetail = "数组，有4个元素，均为浮点型分别是左下和右上纬度、经度坐标";
	sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_SJSZFW;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "坐标系统";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "成果使用的大地基准名称";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_ZBXT;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "高程基准";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "成果使用的高程基准名称";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_GCJZ;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "深度基准";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "成果使用的深度基准名称";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_SDJZ;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "椭球长半径";
	sBSXXInfo.strType = "浮点型";
	sBSXXInfo.strFormat = "11.2F";
	sBSXXInfo.strDetail = "单位：米，缺省：-32767.00";
    char szTemp1[50] = { 0 };
    sprintf(szTemp1, "%f", sVectorMetaInfo.dBSXX_TQCBZ);
    sBSXXInfo.strValue = szTemp1;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "椭球扁率";
	sBSXXInfo.strType = "浮点型";
	sBSXXInfo.strFormat = "16.9F";
	sBSXXInfo.strDetail = "缺省：-32767.000000000";
    char szTemp2[50] = { 0 };
    sprintf(szTemp2, "%f", sVectorMetaInfo.dBSXX_TQBL);
    sBSXXInfo.strValue = szTemp2;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "地图投影";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "无投影、高斯-克吕格、等角圆锥、墨卡托、方位等";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_DTTY;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "中央经线";
	sBSXXInfo.strType = "短整型";
	sBSXXInfo.strFormat = "8D";
	sBSXXInfo.strDetail = "单位：度，-32767";
    char szTemp3[50] = { 0 };
    sprintf(szTemp3, "%d", sVectorMetaInfo.iBSXX_ZYJX);
    sBSXXInfo.strValue = szTemp3;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "标准纬线";
	sBSXXInfo.strType = "浮点型";
	sBSXXInfo.strFormat = "10.5F";
	sBSXXInfo.strDetail = "单位：度，-32767.0";
    char szTemp4[50] = { 0 };
    sprintf(szTemp4, "%f", sVectorMetaInfo.dBSXX_BZWX);
    sBSXXInfo.strValue = szTemp4;
	vBSXXInfos.push_back(sBSXXInfo);

	sBSXXInfo.strItem = "坐标单位";
	sBSXXInfo.strType = "字符型";
	sBSXXInfo.strFormat = "30C";
	sBSXXInfo.strDetail = "“度”、“米”";
    sBSXXInfo.strValue = sVectorMetaInfo.strBSXX_ZBDW;
	vBSXXInfos.push_back(sBSXXInfo);

#pragma endregion

#pragma region "填充3-质量信息表格"

	// 填充3-质量信息表格
	/*
	元数据类	数据项	数据元素简称	数据类型	格式	备注
	质量信息	完整性		ZLXX_WZX	字符型		30C		完整、不完整
	质量信息	数据质量	ZLXX_SJZL	字符型		30C		优、良、合格、不合格

	*/

	// 质量信息元数据
	vector<MetaDataTemplateInfo> vZLXXInfos;
	vZLXXInfos.clear();

	MetaDataTemplateInfo sZLXXInfo;
	sZLXXInfo.strItem = "完整性";
	sZLXXInfo.strType = "字符型";
	sZLXXInfo.strFormat = "30C";
	sZLXXInfo.strDetail = "完整、不完整";
    sZLXXInfo.strValue = sVectorMetaInfo.strZLXX_WZX;
	vZLXXInfos.push_back(sZLXXInfo);

	sZLXXInfo.strItem = "数据质量";
	sZLXXInfo.strType = "字符型";
	sZLXXInfo.strFormat = "30C";
	sZLXXInfo.strDetail = "优、良、合格、不合格";
    sZLXXInfo.strValue = sVectorMetaInfo.strZLXX_SJZL;
	vZLXXInfos.push_back(sZLXXInfo);

#pragma endregion

#pragma region "填充4-专题信息表格"

	// 填充4-专题信息表格
	/*
	元数据类	数据项	数据元素简称	数据类型	格式	备注
	专题信息	产品名称	ZTXX_CPMC	字符型		20C		缺省：NULL
	专题信息	保管单位	ZTXX_BGDW	字符型		30C		缺省：NULL
	专题信息	生产单位	ZTXX_SCDW	字符型		30C		缺省：NULL
	专题信息	生产日期	ZTXX_SCRQ	字符型		8C		YYYYMMDD
	专题信息	更新日期	ZTXX_GXRQ	字符型		8C		YYYYMMDD


	*/

	// 专题信息元数据
	vector<MetaDataTemplateInfo> vZTXXInfos;
	vZTXXInfos.clear();

	MetaDataTemplateInfo sZTXXInfo;
	sZTXXInfo.strItem = "产品名称";sZTXXInfo.strType = "字符型";sZTXXInfo.strFormat = "20C";sZTXXInfo.strDetail = "缺省：NULL";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_CPMC;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "保管单位";sZTXXInfo.strType = "字符型";sZTXXInfo.strFormat = "30C";sZTXXInfo.strDetail = "缺省：NULL";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_BGDW;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "生产单位";sZTXXInfo.strType = "字符型";sZTXXInfo.strFormat = "30C";sZTXXInfo.strDetail = "缺省：NULL";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_SCDW;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "生产日期";sZTXXInfo.strType = "字符型";sZTXXInfo.strFormat = "8C";sZTXXInfo.strDetail = "YYYYMMDD";
	sZLXXInfo.strValue = sVectorMetaInfo.strZTXX_SCRQ;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "更新日期";sZTXXInfo.strType = "字符型";sZTXXInfo.strFormat = "8C";sZTXXInfo.strDetail = "YYYYMMDD";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_GXRQ;
	vZTXXInfos.push_back(sZTXXInfo);

	/*------------------------------------------------*/
	/*	专题信息	产品密级	ZTXX_CPMJ	字符型		6C		绝密、机密、秘密、内部、公开
		专题信息	产品版本	ZTXX_CPBB	字符型		30C		成果的版本号，通常包含生产完成日期信息
		专题信息	关键字		ZTXX_GJZ	字符型		30C		数据关键内容的描述
		专题信息	数据量		ZTXX_SJL	字符型		30C		单位“MB”
		专题信息	数据格式	ZTXX_SJGS	字符型		30C		“shp、gdb、gpkg”等*/


	sZTXXInfo.strItem = "产品密级"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "6C"; sZTXXInfo.strDetail = "绝密、机密、秘密、内部、公开";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_CPMJ;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "产品版本"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "30C"; sZTXXInfo.strDetail = "成果的版本号，通常包含生产完成日期信息";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_CPBB;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "关键字"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "30C"; sZTXXInfo.strDetail = "数据关键内容的描述";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_GJZ;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "数据量"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "30C"; sZTXXInfo.strDetail = "单位“MB”";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_SJL;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "数据格式"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "30C"; sZTXXInfo.strDetail = "“shp、gdb、gpkg”等";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_SJGS;
	vZTXXInfos.push_back(sZTXXInfo);


	/*----------------------------------------------------*/
	/*	专题信息	数据来源	ZTXX_SJLY	字符型		256C	
		专题信息	作业区号	ZTXX_ZYQH	字符型		30C	
		专题信息	图名		ZTXX_TM		字符型		30C		成果对应图幅的名称，通常为图幅覆盖范围内最大的居民地名称或者其他地理名称
		专题信息	图号		ZTXX_TH		字符型		30C		LN1050
		专题信息	比例尺分母	ZTXX_BLCFM	短整型		12D		“50000”*/

	sZTXXInfo.strItem = "数据来源"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "256C"; sZTXXInfo.strDetail = "";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_SJLY;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "作业区号"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "30C"; sZTXXInfo.strDetail = "";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_ZYQH;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "图名"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "30C"; sZTXXInfo.strDetail = "成果对应图幅的名称，通常为图幅覆盖范围内最大的居民地名称或者其他地理名称";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_TM;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "图号"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "30C"; sZTXXInfo.strDetail = "LN1050";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_TH;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "比例尺分母"; sZTXXInfo.strType = "短整型"; sZTXXInfo.strFormat = "12D"; sZTXXInfo.strDetail = "“50000”";
	char szValue[20] = { 0 };
    sprintf_s(szValue, "%d", sVectorMetaInfo.iZTXX_BLCFM);
    sZTXXInfo.strValue = szValue;
	vZTXXInfos.push_back(sZTXXInfo);

	/*----------------------------------------------------*/

	/*	专题信息	坐标系统	ZTXX_ZBXT	字符型		50C	
		专题信息	高程基准	ZTXX_GCJZ	字符型		30C	
		专题信息	地图投影	ZTXX_DTTY	字符型		30C	
		专题信息	等高距		ZTXX_DGJ	短整型		10D		单位：米
		专题信息	政区说明	ZTXX_ZQSM	字符型		500C	缺省：NULL*/

	sZTXXInfo.strItem = "坐标系统"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "50C"; sZTXXInfo.strDetail = "";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_ZBXT;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "高程基准"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "30C"; sZTXXInfo.strDetail = "";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_GCJZ;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "地图投影"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "30C"; sZTXXInfo.strDetail = "";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_DTTY;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "等高距"; sZTXXInfo.strType = "短整型"; sZTXXInfo.strFormat = "10D"; sZTXXInfo.strDetail = "单位：米";
	char szValue1[20] = { 0 };
	sprintf_s(szValue1, "%d", sVectorMetaInfo.iZTXX_DGJ);
	sZTXXInfo.strValue = szValue1;
	vZTXXInfos.push_back(sZTXXInfo);

	sZTXXInfo.strItem = "政区说明"; sZTXXInfo.strType = "字符型"; sZTXXInfo.strFormat = "500C"; sZTXXInfo.strDetail = "缺省：NULL";
	sZTXXInfo.strValue = sVectorMetaInfo.strZTXX_ZQSM;
	vZTXXInfos.push_back(sZTXXInfo);



	/*----------------------------------------------------*/


#pragma endregion

	// 填充5-分析就绪型数据表格
	/*数据项				数据元素简称	数据类型	格式	备注
	  基础格网级别			"grid_level"	短整型		10D		取值范围：0到25
	  分析就绪数据级别		"data_level"	短整型		10D		取值范围：0到25
      数据格式				"data_format"	字符型		20C		shp、gpkg
      数据类型				"data_type"		字符型		20C		geometry
      坐标系统				"crs"			字符型		200C
	  最小X索引				"min_x"			整型		20D	
	  最大X索引				"max_x"			整型		20D	
	  最小Y索引				"min_y"			整型		20D
	  最大Y索引				"max_y"			整型		20D
	  左边界经度            "left"			双精度浮点型	10.10F
	  上边界纬度			"top"			双精度浮点型	10.10F
      右边界经度			"right"			双精度浮点型	10.10F
      下边界纬度			"bottom"		双精度浮点型	10.10F
      数据源索引			"data_source"	短整型			10D		默认为1
      描述信息				"detail"		字符型			500C	分析就绪数据的描述信息，支撑的任务类型、时相信息等
	
	 
	*/

	// 分析就绪型数据元数据
	vector<MetaDataTemplateInfo> vARDMetaDataInfos;
	vARDMetaDataInfos.clear();

	MetaDataTemplateInfo sARDInfo;
	sARDInfo.strItem = "基础格网级别"; sARDInfo.strType = "短整型"; sARDInfo.strFormat = "10D"; sARDInfo.strDetail = "取值范围：0到25";
	char szARDValue[20] = { 0 };
    sprintf_s(szARDValue, "%d", sVectorMetaInfo.i_grid_level);
	sARDInfo.strValue = szARDValue;

	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "分析就绪数据级别"; sARDInfo.strType = "短整型"; sARDInfo.strFormat = "10D"; sARDInfo.strDetail = "取值范围：0到25";
	memset(szARDValue, 0, 20);
	sprintf_s(szARDValue, "%d", sVectorMetaInfo.i_data_level);
	sARDInfo.strValue = szARDValue;	
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "数据格式"; sARDInfo.strType = "字符型"; sARDInfo.strFormat = "20C"; sARDInfo.strDetail = "shp、gpkg";
	sARDInfo.strValue = sVectorMetaInfo.str_data_format;
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "数据类型"; sARDInfo.strType = "字符型"; sARDInfo.strFormat = "20C"; sARDInfo.strDetail = "geometry";
	sARDInfo.strValue = sVectorMetaInfo.str_data_type;
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "坐标系统"; sARDInfo.strType = "字符型"; sARDInfo.strFormat = "200C"; sARDInfo.strDetail = "";
	sARDInfo.strValue = sVectorMetaInfo.str_crs;
	vARDMetaDataInfos.push_back(sARDInfo);

	/*-------------------------------------------------------*/

	sARDInfo.strItem = "最小X索引"; sARDInfo.strType = "整型"; sARDInfo.strFormat = "20D"; sARDInfo.strDetail = "";
	memset(szARDValue, 0, 20);
	sprintf_s(szARDValue, "%d", sVectorMetaInfo.i_min_x);
	sARDInfo.strValue = szARDValue;
	
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "最大X索引"; sARDInfo.strType = "整型"; sARDInfo.strFormat = "20D"; sARDInfo.strDetail = "";
	memset(szARDValue, 0, 20);
	sprintf_s(szARDValue, "%d", sVectorMetaInfo.i_max_x);
	sARDInfo.strValue = szARDValue;
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "最小Y索引"; sARDInfo.strType = "整型"; sARDInfo.strFormat = "20D"; sARDInfo.strDetail = "";
	memset(szARDValue, 0, 20);
	sprintf_s(szARDValue, "%d", sVectorMetaInfo.i_min_y);
	sARDInfo.strValue = szARDValue;
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "最大Y索引"; sARDInfo.strType = "整型"; sARDInfo.strFormat = "20D"; sARDInfo.strDetail = "";
	memset(szARDValue, 0, 20);
	sprintf_s(szARDValue, "%d", sVectorMetaInfo.i_max_y);
	sARDInfo.strValue = szARDValue;
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "左边界经度"; sARDInfo.strType = "双精度浮点型"; sARDInfo.strFormat = "10.10F"; sARDInfo.strDetail = "";
	memset(szARDValue, 0, 20);
	sprintf_s(szARDValue, "%.10f", sVectorMetaInfo.d_left);
	sARDInfo.strValue = szARDValue;
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "上边界纬度"; sARDInfo.strType = "双精度浮点型"; sARDInfo.strFormat = "10.10F"; sARDInfo.strDetail = "";
	memset(szARDValue, 0, 20);
	sprintf_s(szARDValue, "%.10f", sVectorMetaInfo.d_top);
	sARDInfo.strValue = szARDValue;
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "右边界经度"; sARDInfo.strType = "双精度浮点型"; sARDInfo.strFormat = "10.10F"; sARDInfo.strDetail = "";
	memset(szARDValue, 0, 20);
	sprintf_s(szARDValue, "%.10f", sVectorMetaInfo.d_right);
	sARDInfo.strValue = szARDValue;
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "下边界纬度"; sARDInfo.strType = "双精度浮点型"; sARDInfo.strFormat = "10.10F"; sARDInfo.strDetail = "";
	memset(szARDValue, 0, 20);
	sprintf_s(szARDValue, "%.10f", sVectorMetaInfo.d_bottom);
	sARDInfo.strValue = szARDValue;
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "数据源索引"; sARDInfo.strType = "短整型"; sARDInfo.strFormat = "10D"; sARDInfo.strDetail = "默认为1";
	memset(szARDValue, 0, 20);
	sprintf_s(szARDValue, "%d", sVectorMetaInfo.i_data_source);
	sARDInfo.strValue = szARDValue;
	vARDMetaDataInfos.push_back(sARDInfo);

	sARDInfo.strItem = "描述信息"; sARDInfo.strType = "字符型"; sARDInfo.strFormat = "500C"; sARDInfo.strDetail = "分析就绪数据的描述信息，支撑的任务类型、时相信息等";
	sARDInfo.strValue = sVectorMetaInfo.str_detail;
	vARDMetaDataInfos.push_back(sARDInfo);


	// 更新表格控件中的数据
	// 更新管理信息
	UpdateTableWidget(vGLXXInfos, ui.tableWidget_GLXX);

	// 更新标识信息
	UpdateTableWidget(vBSXXInfos, ui.tableWidget_BSXX);

	// 更新质量信息
	UpdateTableWidget(vZLXXInfos, ui.tableWidget_ZLXX);

	// 更新专题信息
	UpdateTableWidget(vZTXXInfos, ui.tableWidget_ZTXX);

	// 更新元数据信息
	UpdateTableWidget(vARDMetaDataInfos, ui.tableWidget_ARD);

}


void CSE_ManageMetaDataDlg::GetMetaDataInfosFromTableWidget(QTableWidget* pTableWidget, vector<MetaDataTemplateInfo>& vInfos)
{
	vInfos.clear();

	// 从pTableWidget中获取提取参数
	for (int i = 0; i < pTableWidget->rowCount(); ++i)
	{
		MetaDataTemplateInfo info;

		// 属性值
		if (pTableWidget->item(i, 4))
		{
			info.strValue = pTableWidget->item(i, 4)->text().toLocal8Bit();
		}

		vInfos.push_back(info);
	}
}

void CSE_ManageMetaDataDlg::pushButton_OK_clicked()
{
	// 如果文件/文件夹不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputMetaDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_InputMetaDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	//----------------------------------------//
	// 获取管理信息表格控件中的内容
	vector<MetaDataTemplateInfo> vGLXX_MetaDataInfos;
	vGLXX_MetaDataInfos.clear();

	// 获取管理信息表格控件中的内容
	GetMetaDataInfosFromTableWidget(ui.tableWidget_GLXX, vGLXX_MetaDataInfos);

	//----------------------------------------//
	// 获取标识信息表格控件中的内容
	vector<MetaDataTemplateInfo> vBSXX_MetaDataInfos;
	vBSXX_MetaDataInfos.clear();

	// 获取标识信息表格控件中的内容
	GetMetaDataInfosFromTableWidget(ui.tableWidget_BSXX, vBSXX_MetaDataInfos);

	//----------------------------------------//

	// 获取质量信息表格控件中的内容
	vector<MetaDataTemplateInfo> vZLXX_MetaDataInfos;
	vZLXX_MetaDataInfos.clear();

	// 获取质量信息表格控件中的内容
	GetMetaDataInfosFromTableWidget(ui.tableWidget_ZLXX, vZLXX_MetaDataInfos);

	//----------------------------------------//

	// 获取专题信息表格控件中的内容
	vector<MetaDataTemplateInfo> vZTXX_MetaDataInfos;
	vZTXX_MetaDataInfos.clear();

	// 获取专题信息表格控件中的内容
	GetMetaDataInfosFromTableWidget(ui.tableWidget_ZTXX, vZTXX_MetaDataInfos);

	//----------------------------------------//

	// 获取分析就绪数据表格控件中的内容
	vector<MetaDataTemplateInfo> vARD_MetaDataInfos;
	vARD_MetaDataInfos.clear();

	// 获取分析就绪数据信息表格控件中的内容
	GetMetaDataInfosFromTableWidget(ui.tableWidget_ARD, vARD_MetaDataInfos);

	string strOutputJsonFile = ui.lineEdit_OutputMetaDataPath->text().toLocal8Bit();

	// 保存元数据信息
	WriteMetaDataToJsonFileFromTables(strOutputJsonFile,
		vGLXX_MetaDataInfos,
		vBSXX_MetaDataInfos,
		vZLXX_MetaDataInfos,
		vZTXX_MetaDataInfos,
		vARD_MetaDataInfos);

	// 提示保存成功
    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
    msgBox.setText(tr("元数据保存成功！"));
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();


	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("MANAGE_VECTOR_ARD_METADATA/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("MANAGE_VECTOR_ARD_METADATA/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("MANAGE_VECTOR_ARD_METADATA/InputARDDataPath"), m_qstrInputARDDataPath, QgsSettings::Section::Plugins);

}



void CSE_ManageMetaDataDlg::pushButton_Cancel_clicked()
{
	reject();
}

void CSE_ManageMetaDataDlg::on_InputDataPath_TextChanged()
{

}

void CSE_ManageMetaDataDlg::pushButton_SaveMetaData_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("保存矢量分析就绪型数据元数据文件");
	QString filter = tr("json 文件(*.json)");
	QString qstrFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!qstrFileName.isEmpty())
	{
		ui.lineEdit_OutputMetaDataPath->setText(qstrFileName);
		m_qstrOutputDataPath = qstrFileName;
    }
}

void CSE_ManageMetaDataDlg::pushButton_UpdateMetadataRange_clicked()
{
	// 根据输入分析就绪数据路径遍历每个文件的范围
	// 获取当前目录下所有的文件夹，即矢量分析就绪数据产品类型
	vector<string> vSubDir;
	vSubDir.clear();
	string strInputPath = ui.lineEdit_InputDataPath->text().toLocal8Bit();
	GetSubDirFromFilePath(strInputPath, vSubDir);

	if (vSubDir.size() == 0)
	{
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
        msgBox.setText(tr("输入路径下没有矢量分析就绪数据产品类型文件夹！"));
        msgBox.setStandardButtons(QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.exec();
        return;
	}

	// 所有shp数据的范围并集
	vector<SE_DRect> vUnionRect;
	vUnionRect.clear();

	// 基础格网级别
	int iGridLevel = 0;

	// 计算每个子目录下shp数据的空间范围并集
	for (int i = 0; i < vSubDir.size(); ++i)
	{
		string strTempPath = strInputPath;
		strTempPath += "/";
		strTempPath += vSubDir[i];

		// 获取所有shp文件路径
		vector<string> vShpFiles;
		vShpFiles.clear();

		GetShpFilesFromFilePath(strTempPath, vShpFiles);

		for (int j = 0; j < vShpFiles.size(); ++j)
		{
			// 获取Z、X、Y
			int iZ, iX, iY;

			if (extractZXY(vShpFiles[j], iZ, iX, iY))
			{
				iGridLevel = iZ;
				// 计算ZXY空间范围
				SE_DRect dTempRect = CalGridExtentByZXY(iZ, iX, iY);
				vUnionRect.push_back(dTempRect);
			}
		}
	}

	double dMaxLon = -9999;     // 最大经度
	double dMinLon = 9999;      // 最小经度
	double dMaxLat = -9999;     // 最大纬度
	double dMinLat = 9999;      // 最小纬度

	// 计算所有矩形范围的并集
	for (int i = 0; i < vUnionRect.size(); ++i)
	{
		SE_DRect dRect = vUnionRect[i];
		if (dMaxLon < dRect.dright)
		{
			dMaxLon = dRect.dright;
		}

		if (dMinLon > dRect.dleft)
		{
			dMinLon = dRect.dleft;
		}

		if (dMaxLat < dRect.dtop)
		{
			dMaxLat = dRect.dtop;
		}

		if (dMinLat > dRect.dbottom)
		{
			dMinLat = dRect.dbottom;
		}
	}

	// 所有矩形范围的并集
	SE_DRect dMBRRect;
	dMBRRect.dleft = dMinLon;
	dMBRRect.dright = dMaxLon;
	dMBRRect.dtop = dMaxLat;
	dMBRRect.dbottom = dMinLat;

	int iMinX = 0;
	int iMaxX = 0;
	int iMinY = 0;
	int iMaxY = 0;
	CalXRangeAndYRangeByGeoRectAndLevel(
		dMBRRect,
		iGridLevel,
		iMinX,
		iMaxX,
		iMinY,
		iMaxY);

	// 更新到表格控件中
	// min_x
	ui.tableWidget_ARD->item(5, 4)->setText(QString::number(iMinX));

    // max_x
    ui.tableWidget_ARD->item(6, 4)->setText(QString::number(iMaxX));

    // min_y
    ui.tableWidget_ARD->item(7, 4)->setText(QString::number(iMinY));

    // max_y
    ui.tableWidget_ARD->item(8, 4)->setText(QString::number(iMaxY));

    // left
    ui.tableWidget_ARD->item(9, 4)->setText(QString::number(dMinLon));

    // top
    ui.tableWidget_ARD->item(10, 4)->setText(QString::number(dMaxLat));

    // right
    ui.tableWidget_ARD->item(11, 4)->setText(QString::number(dMaxLon));

    // bottom
    ui.tableWidget_ARD->item(12, 4)->setText(QString::number(dMinLat));

}

void CSE_ManageMetaDataDlg::pushButton_OpenARD_clicked()
{
	QString curPath = m_qstrInputARDDataPath;
	QString dlgTile = tr("请选择矢量分析就绪数据所在目录");
	QString directory = QFileDialog::getExistingDirectory(this,
		dlgTile,
		curPath, // 默认路径
		QFileDialog::ShowDirsOnly);

	if (!directory.isEmpty())
	{
		m_qstrInputARDDataPath = directory;
		ui.lineEdit_InputDataPath->setText(directory);
	}
	else
	{
		return;
	}
}

string CSE_ManageMetaDataDlg::convertEncoding(const string& input, 
	const string& fromEncoding, 
	const string& toEncoding) 
{
	iconv_t conv = iconv_open(toEncoding.c_str(), fromEncoding.c_str());
	if (conv == (iconv_t)(-1)) {
		perror("iconv_open");
		return "";
	}

	size_t inBytesLeft = input.size();
	size_t outBytesLeft = inBytesLeft * 4; // 预留足够的空间
	std::vector<char> output(outBytesLeft);
	char* inBuf = const_cast<char*>(input.data());
	char* outBuf = output.data();

	size_t result = iconv(conv, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft);
	if (result == (size_t)(-1)) {
		perror("iconv");
		iconv_close(conv);
		return "";
	}

	iconv_close(conv);
	return std::string(output.data(), output.size() - outBytesLeft);
}

void CSE_ManageMetaDataDlg::convertCsv(const string& inputFile, const string& outputFile) 
{
	std::ifstream inFile(inputFile, std::ios::binary);
	std::ofstream outFile(outputFile, std::ios::binary);

	if (!inFile.is_open() || !outFile.is_open()) {
		std::cerr << "Error opening files!" << std::endl;
		return;
	}

	std::string line;
	while (std::getline(inFile, line)) {
		std::string utf8Line = convertEncoding(line, "GB2312", "UTF-8");
		if (!utf8Line.empty()) {
			outFile << utf8Line << "\n";
		}
	}

	inFile.close();
	outFile.close();
}


#pragma region  "检查文件或文件夹是否存在"

bool CSE_ManageMetaDataDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion


bool CSE_ManageMetaDataDlg::ReadMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo)
{
	// 读取JSON文件
	std::ifstream file(strFile.c_str());
	if (!file.is_open())
	{
		// 记录日志
		return false;
	}

	// 解析JSON文件
	ordered_json j;
	file >> j;

	// 访问analysis_ready_data数组中的第一个元素
	// 分析就绪数据元数据
	auto& ard = j["analysis_ready_data"][0];
	metaInfo.i_grid_level = ard["grid_level"];
	metaInfo.i_data_level = ard["data_level"];
	metaInfo.str_data_format = ard["data_format"];
	metaInfo.str_data_type = ard["data_type"];
	metaInfo.str_crs = ard["crs"];
	metaInfo.i_min_x = ard["min_x"];
	metaInfo.i_max_x = ard["max_x"];
	metaInfo.i_min_y = ard["min_y"];
	metaInfo.i_max_y = ard["max_y"];
	metaInfo.d_left = ard["left"];
	metaInfo.d_top = ard["top"];
	metaInfo.d_right = ard["right"];
	metaInfo.d_bottom = ard["bottom"];
	metaInfo.i_data_source = ard["data_source"];
	metaInfo.str_detail = ard["detail"];

	// 数据源元数据
	// 访问data_source_list数组中的第一个元素
	auto& dsl = j["data_source_list"][0];
	// 管理信息
	metaInfo.strGLXX_CPMC = dsl["GLXX_CPMC"];
	metaInfo.strGLXX_BGDW = dsl["GLXX_BGDW"];
	metaInfo.strGLXX_SCDW = dsl["GLXX_SCDW"];
	metaInfo.strGLXX_SCRQ = dsl["GLXX_SCRQ"];
	metaInfo.strGLXX_GXRQ = dsl["GLXX_GXRQ"];
	metaInfo.strGLXX_CPMJ = dsl["GLXX_CPMJ"];
	metaInfo.strGLXX_CPBB = dsl["GLXX_CPBB"];

	// 标识信息
	metaInfo.strBSXX_GJZ = dsl["BSXX_GJZ"];
	metaInfo.strBSXX_SJL = dsl["BSXX_SJL"];
	metaInfo.strBSXX_SJGS = dsl["BSXX_SJGS"];
	metaInfo.strBSXX_SJLY = dsl["BSXX_SJLY"];
	metaInfo.strBSXX_FFMMGF = dsl["BSXX_FFMMGF"];
	metaInfo.strBSXX_TM = dsl["BSXX_TM"];
	metaInfo.strBSXX_TH = dsl["BSXX_TH"];

	metaInfo.iBSXX_BLCFM = dsl["BSXX_BLCFM"];
	metaInfo.strBSXX_SJSZFW = dsl["BSXX_SJSZFW"];
	metaInfo.strBSXX_ZBXT = dsl["BSXX_ZBXT"];
	metaInfo.strBSXX_GCJZ = dsl["BSXX_GCJZ"];
	metaInfo.strBSXX_SDJZ = dsl["BSXX_SDJZ"];
	metaInfo.dBSXX_TQCBZ = dsl["BSXX_TQCBZ"];
	metaInfo.dBSXX_TQBL = dsl["BSXX_TQBL"];
	metaInfo.strBSXX_DTTY = dsl["BSXX_DTTY"];
	metaInfo.iBSXX_ZYJX = dsl["BSXX_ZYJX"];
	metaInfo.dBSXX_BZWX = dsl["BSXX_BZWX"];
	metaInfo.strBSXX_ZBDW = dsl["BSXX_ZBDW"];

	// 质量信息
	metaInfo.strZLXX_WZX = dsl["ZLXX_WZX"];
	metaInfo.strZLXX_SJZL = dsl["ZLXX_SJZL"];

	// 专题信息
	metaInfo.strZTXX_CPMC = dsl["ZTXX_CPMC"];
	metaInfo.strZTXX_BGDW = dsl["ZTXX_BGDW"];
	metaInfo.strZTXX_SCDW = dsl["ZTXX_SCDW"];
	metaInfo.strZTXX_SCRQ = dsl["ZTXX_SCRQ"];
	metaInfo.strZTXX_GXRQ = dsl["ZTXX_GXRQ"];
	metaInfo.strZTXX_CPMJ = dsl["ZTXX_CPMJ"];
	metaInfo.strZTXX_CPBB = dsl["ZTXX_CPBB"];
	metaInfo.strZTXX_GJZ = dsl["ZTXX_GJZ"];
	metaInfo.strZTXX_SJL = dsl["ZTXX_SJL"];
	metaInfo.strZTXX_SJGS = dsl["ZTXX_SJGS"];
	metaInfo.strZTXX_SJLY = dsl["ZTXX_SJLY"];
	metaInfo.strZTXX_ZYQH = dsl["ZTXX_ZYQH"];
	metaInfo.strZTXX_TM = dsl["ZTXX_TM"];
	metaInfo.strZTXX_TH = dsl["ZTXX_TH"];
	metaInfo.iZTXX_BLCFM = dsl["ZTXX_BLCFM"];
	metaInfo.strZTXX_ZBXT = dsl["ZTXX_ZBXT"];
	metaInfo.strZTXX_GCJZ = dsl["ZTXX_GCJZ"];
	metaInfo.strZTXX_DTTY = dsl["ZTXX_DTTY"];
	metaInfo.iZTXX_DGJ = dsl["ZTXX_DGJ"];
	metaInfo.strZTXX_ZQSM = dsl["ZTXX_ZQSM"];
	return true;
}


void CSE_ManageMetaDataDlg::WriteMetaDataToJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo)
{

	// 创建 JSON 对象
	ordered_json j;

	// 添加 data_source_list
	j["data_source_list"] = ordered_json::array();

	ordered_json dsl;

	// 填充字段
	// 管理信息
	dsl["GLXX_CPMC"] = metaInfo.strGLXX_CPMC;
	dsl["GLXX_BGDW"] = metaInfo.strGLXX_BGDW;
	dsl["GLXX_SCDW"] = metaInfo.strGLXX_SCDW;
	dsl["GLXX_SCRQ"] = metaInfo.strGLXX_SCRQ;
	dsl["GLXX_GXRQ"] = metaInfo.strGLXX_GXRQ;
	dsl["GLXX_CPMJ"] = metaInfo.strGLXX_CPMJ;
	dsl["GLXX_CPBB"] = metaInfo.strGLXX_CPBB;

	// 标识信息
	dsl["BSXX_GJZ"] = metaInfo.strBSXX_GJZ;
	dsl["BSXX_SJL"] = metaInfo.strBSXX_SJL;
	dsl["BSXX_SJGS"] = metaInfo.strBSXX_SJGS;
	dsl["BSXX_SJLY"] = metaInfo.strBSXX_SJLY;
	dsl["BSXX_FFMMGF"] = metaInfo.strBSXX_FFMMGF;
	dsl["BSXX_TM"] = metaInfo.strBSXX_TM;
	dsl["BSXX_TH"] = metaInfo.strBSXX_TH;

	dsl["BSXX_BLCFM"] = metaInfo.iBSXX_BLCFM;
	dsl["BSXX_SJSZFW"] = metaInfo.strBSXX_SJSZFW;
	dsl["BSXX_ZBXT"] = metaInfo.strBSXX_ZBXT;
	dsl["BSXX_GCJZ"] = metaInfo.strBSXX_GCJZ;
	dsl["BSXX_SDJZ"] = metaInfo.strBSXX_SDJZ;
	dsl["BSXX_TQCBZ"] = metaInfo.dBSXX_TQCBZ;
	dsl["BSXX_TQBL"] = metaInfo.dBSXX_TQBL;
	dsl["BSXX_DTTY"] = metaInfo.strBSXX_DTTY;
	dsl["BSXX_ZYJX"] = metaInfo.iBSXX_ZYJX;
	dsl["BSXX_BZWX"] = metaInfo.dBSXX_BZWX;
	dsl["BSXX_ZBDW"] = metaInfo.strBSXX_ZBDW;

	// 质量信息
	dsl["ZLXX_WZX"] = metaInfo.strZLXX_WZX;			// 完整性
	dsl["ZLXX_SJZL"] = metaInfo.strZLXX_SJZL;		// 数据质量

	// 专题信息
	dsl["ZTXX_CPMC"] = metaInfo.strZTXX_CPMC;
	dsl["ZTXX_BGDW"] = metaInfo.strZTXX_BGDW;
	dsl["ZTXX_SCDW"] = metaInfo.strZTXX_SCDW;
	dsl["ZTXX_SCRQ"] = metaInfo.strZTXX_SCRQ;
	dsl["ZTXX_GXRQ"] = metaInfo.strZTXX_GXRQ;
	dsl["ZTXX_CPMJ"] = metaInfo.strZTXX_CPMJ;
	dsl["ZTXX_CPBB"] = metaInfo.strZTXX_CPBB;
	dsl["ZTXX_GJZ"] = metaInfo.strZTXX_GJZ;
	dsl["ZTXX_SJL"] = metaInfo.strZTXX_SJL;
	dsl["ZTXX_SJGS"] = metaInfo.strZTXX_SJGS;
	dsl["ZTXX_SJLY"] = metaInfo.strZTXX_SJLY;
	dsl["ZTXX_ZYQH"] = metaInfo.strZTXX_ZYQH;
	dsl["ZTXX_TM"] = metaInfo.strZTXX_TM;
	dsl["ZTXX_TH"] = metaInfo.strZTXX_TH;
	dsl["ZTXX_BLCFM"] = metaInfo.iZTXX_BLCFM;
	dsl["ZTXX_ZBXT"] = metaInfo.strZTXX_ZBXT;
	dsl["ZTXX_GCJZ"] = metaInfo.strZTXX_GCJZ;
	dsl["ZTXX_DTTY"] = metaInfo.strZTXX_DTTY;
	dsl["ZTXX_DGJ"] = metaInfo.iZTXX_DGJ;
	dsl["ZTXX_ZQSM"] = metaInfo.strZTXX_ZQSM;

	// 将数据源添加到列表中
	j["data_source_list"].push_back(dsl);


	// 添加 analysis_ready_data
	j["analysis_ready_data"] = ordered_json::array();

	ordered_json analysis;
	analysis["grid_level"] = metaInfo.i_grid_level;
	analysis["data_level"] = metaInfo.i_data_level;
	analysis["data_format"] = "gpkg";
	analysis["data_type"] = "vector";
	analysis["crs"] = metaInfo.strBSXX_ZBXT;
	analysis["min_x"] = metaInfo.i_min_x;
	analysis["max_x"] = metaInfo.i_max_x;
	analysis["min_y"] = metaInfo.i_min_y;
	analysis["max_y"] = metaInfo.i_max_y;
	analysis["left"] = metaInfo.d_left;
	analysis["top"] = metaInfo.d_top;
	analysis["right"] = metaInfo.d_right;
	analysis["bottom"] = metaInfo.d_bottom;
	analysis["data_source"] = metaInfo.i_data_source;
	analysis["detail"] = metaInfo.str_detail;

	// 将分析数据添加到列表中
	j["analysis_ready_data"].push_back(analysis);


	// 将 JSON 对象写入文件
	std::ofstream file(strFile.c_str());
	if (file.is_open())
	{
		// 以 4 个空格缩进格式化输出
		file << j.dump(4);
		file.close();
	}

}

void CSE_ManageMetaDataDlg::WriteMetaDataToJsonFileFromTables(
	const string& strFile,
	vector<MetaDataTemplateInfo>& vGLXX_MetaInfo,
	vector<MetaDataTemplateInfo>& vBSXX_MetaInfo,
	vector<MetaDataTemplateInfo>& vZLXX_MetaInfo,
	vector<MetaDataTemplateInfo>& vZTXX_MetaInfo,
	vector<MetaDataTemplateInfo>& vARD_MetaInfo)
{

	// 创建 JSON 对象
	ordered_json j;

	// 添加 data_source_list
	j["data_source_list"] = ordered_json::array();

	ordered_json dsl;

	// 填充字段
	// 管理信息
	dsl["GLXX_CPMC"] = convertEncoding(vGLXX_MetaInfo[0].strValue, "gbk", "utf-8");
	dsl["GLXX_BGDW"] = convertEncoding( vGLXX_MetaInfo[1].strValue, "gbk", "utf-8" );
	dsl["GLXX_SCDW"] = convertEncoding(vGLXX_MetaInfo[2].strValue, "gbk", "utf-8");
	dsl["GLXX_SCRQ"] = convertEncoding(vGLXX_MetaInfo[3].strValue, "gbk", "utf-8");
	dsl["GLXX_GXRQ"] = convertEncoding(vGLXX_MetaInfo[4].strValue, "gbk", "utf-8");
	dsl["GLXX_CPMJ"] = convertEncoding(vGLXX_MetaInfo[5].strValue, "gbk", "utf-8");
	dsl["GLXX_CPBB"] = convertEncoding(vGLXX_MetaInfo[6].strValue, "gbk", "utf-8");

	// 标识信息
	dsl["BSXX_GJZ"] = convertEncoding(vBSXX_MetaInfo[0].strValue, "gbk", "utf-8");
	dsl["BSXX_SJL"] = convertEncoding(vBSXX_MetaInfo[1].strValue, "gbk", "utf-8");
	dsl["BSXX_SJGS"] = convertEncoding(vBSXX_MetaInfo[2].strValue, "gbk", "utf-8");
	dsl["BSXX_SJLY"] = convertEncoding(vBSXX_MetaInfo[3].strValue, "gbk", "utf-8");
	dsl["BSXX_FFMMGF"] = convertEncoding(vBSXX_MetaInfo[4].strValue, "gbk", "utf-8");
	dsl["BSXX_TM"] = convertEncoding(vBSXX_MetaInfo[5].strValue, "gbk", "utf-8");
	dsl["BSXX_TH"] = convertEncoding(vBSXX_MetaInfo[6].strValue, "gbk", "utf-8");

	dsl["BSXX_BLCFM"] = atoi(vBSXX_MetaInfo[7].strValue.c_str());
	dsl["BSXX_SJSZFW"] = convertEncoding(vBSXX_MetaInfo[8].strValue, "gbk", "utf-8");
	dsl["BSXX_ZBXT"] = convertEncoding(vBSXX_MetaInfo[9].strValue, "gbk", "utf-8");
	dsl["BSXX_GCJZ"] = convertEncoding(vBSXX_MetaInfo[10].strValue, "gbk", "utf-8");
	dsl["BSXX_SDJZ"] = convertEncoding(vBSXX_MetaInfo[11].strValue, "gbk", "utf-8");
	
	dsl["BSXX_TQCBZ"] = atof(vBSXX_MetaInfo[12].strValue.c_str());
	dsl["BSXX_TQBL"] = atof(vBSXX_MetaInfo[13].strValue.c_str());
	dsl["BSXX_DTTY"] = convertEncoding(vBSXX_MetaInfo[14].strValue, "gbk", "utf-8");
	dsl["BSXX_ZYJX"] = atoi(vBSXX_MetaInfo[15].strValue.c_str());
	dsl["BSXX_BZWX"] = atof(vBSXX_MetaInfo[16].strValue.c_str());
	dsl["BSXX_ZBDW"] = convertEncoding(vBSXX_MetaInfo[17].strValue, "gbk", "utf-8");

	// 质量信息
	dsl["ZLXX_WZX"] = convertEncoding(vZLXX_MetaInfo[0].strValue, "gbk", "utf-8");			// 完整性
	dsl["ZLXX_SJZL"] = convertEncoding(vZLXX_MetaInfo[1].strValue, "gbk", "utf-8");	// 数据质量

	// 专题信息
	dsl["ZTXX_CPMC"] = convertEncoding(vZTXX_MetaInfo[0].strValue, "gbk", "utf-8");
	dsl["ZTXX_BGDW"] = convertEncoding(vZTXX_MetaInfo[1].strValue, "gbk", "utf-8");
	dsl["ZTXX_SCDW"] = convertEncoding(vZTXX_MetaInfo[2].strValue, "gbk", "utf-8");
	dsl["ZTXX_SCRQ"] = convertEncoding(vZTXX_MetaInfo[3].strValue, "gbk", "utf-8");
	dsl["ZTXX_GXRQ"] = convertEncoding(vZTXX_MetaInfo[4].strValue, "gbk", "utf-8");
	dsl["ZTXX_CPMJ"] = convertEncoding(vZTXX_MetaInfo[5].strValue, "gbk", "utf-8");
	dsl["ZTXX_CPBB"] = convertEncoding(vZTXX_MetaInfo[6].strValue, "gbk", "utf-8");
	dsl["ZTXX_GJZ"] = convertEncoding(vZTXX_MetaInfo[7].strValue, "gbk", "utf-8");
	dsl["ZTXX_SJL"] = convertEncoding(vZTXX_MetaInfo[8].strValue, "gbk", "utf-8");
	dsl["ZTXX_SJGS"] = convertEncoding(vZTXX_MetaInfo[9].strValue, "gbk", "utf-8");
	dsl["ZTXX_SJLY"] = convertEncoding(vZTXX_MetaInfo[10].strValue, "gbk", "utf-8");
	dsl["ZTXX_ZYQH"] = convertEncoding(vZTXX_MetaInfo[11].strValue, "gbk", "utf-8");
	dsl["ZTXX_TM"] = convertEncoding(vZTXX_MetaInfo[12].strValue, "gbk", "utf-8");
	dsl["ZTXX_TH"] = convertEncoding(vZTXX_MetaInfo[13].strValue, "gbk", "utf-8");
	dsl["ZTXX_BLCFM"] = atoi(vZTXX_MetaInfo[14].strValue.c_str());
	dsl["ZTXX_ZBXT"] = convertEncoding(vZTXX_MetaInfo[15].strValue, "gbk", "utf-8");
	dsl["ZTXX_GCJZ"] = convertEncoding(vZTXX_MetaInfo[16].strValue, "gbk", "utf-8");
	dsl["ZTXX_DTTY"] = convertEncoding(vZTXX_MetaInfo[17].strValue, "gbk", "utf-8");
	dsl["ZTXX_DGJ"] = atoi(vZTXX_MetaInfo[18].strValue.c_str());
	dsl["ZTXX_ZQSM"] = convertEncoding(vZTXX_MetaInfo[19].strValue, "gbk", "utf-8");

	// 将数据源添加到列表中
	j["data_source_list"].push_back(dsl);


	// 添加 analysis_ready_data
	j["analysis_ready_data"] = ordered_json::array();

	ordered_json analysis;
	analysis["grid_level"] = atoi(vARD_MetaInfo[0].strValue.c_str());
	analysis["data_level"] = atoi(vARD_MetaInfo[1].strValue.c_str());
	analysis["data_format"] = convertEncoding(vARD_MetaInfo[2].strValue, "gbk", "utf-8");
	analysis["data_type"] = convertEncoding(vARD_MetaInfo[3].strValue, "gbk", "utf-8");
	analysis["crs"] = convertEncoding(vARD_MetaInfo[4].strValue, "gbk", "utf-8");
	analysis["min_x"] = atoi(vARD_MetaInfo[5].strValue.c_str());
	analysis["max_x"] = atoi(vARD_MetaInfo[6].strValue.c_str());
	analysis["min_y"] = atoi(vARD_MetaInfo[7].strValue.c_str());
	analysis["max_y"] = atoi(vARD_MetaInfo[8].strValue.c_str());
	analysis["left"] = atof(vARD_MetaInfo[9].strValue.c_str());
	analysis["top"] = atof(vARD_MetaInfo[10].strValue.c_str());
	analysis["right"] = atof(vARD_MetaInfo[11].strValue.c_str());
	analysis["bottom"] = atof(vARD_MetaInfo[12].strValue.c_str());
	analysis["data_source"] = atoi(vARD_MetaInfo[13].strValue.c_str());
	analysis["detail"] = convertEncoding(vARD_MetaInfo[14].strValue, "gbk", "utf-8");

	// 将分析数据添加到列表中
	j["analysis_ready_data"].push_back(analysis);


	// 将 JSON 对象写入文件
	std::ofstream file(strFile.c_str());
	if (file.is_open())
	{
		// 以 4 个空格缩进格式化输出
		file << j.dump(4);
		file.close();
	}

}

/*获取当前目录下所有子目录*/
void CSE_ManageMetaDataDlg::GetSubDirFromFilePath(const string& strFilePath,
	vector<string>& vSubDir)
{
	// 获取当前工作目录
	std::filesystem::path currentPath = strFilePath;
	std::cout << "Current Path: " << currentPath << std::endl;

	// 遍历当前目录下的所有条目
	for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
		// 检查条目是否是目录
		if (std::filesystem::is_directory(entry.status())) {
			vSubDir.push_back(entry.path().filename().string());
			std::cout << "Subdirectory: " << entry.path().filename() << std::endl;
		}
	}
}


// 获取当前路径下所有shp文件路径
void CSE_ManageMetaDataDlg::GetShpFilesFromFilePath(
	const string& strFilePath,
	vector<string>& vShpFilePath)
{
	vShpFilePath.clear();

	try
	{
		std::filesystem::path folderPath = strFilePath;

		if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
		{
			return;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
		{
			if (entry.is_regular_file() && (entry.path().extension() == ".shp" || entry.path().extension() == ".SHP"))
			{
				vShpFilePath.push_back(entry.path().string());
			}
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		return;
	}
}

// 根据文件名提取Z、X、Y
bool CSE_ManageMetaDataDlg::extractZXY(
	const std::string& shpFilePath,
	int& Z,
	int& X,
	int& Y)
{
	string strFileName = CPLGetBasename(shpFilePath.c_str());

	// 找到文件名中的数字部分
	std::stringstream ss(strFileName);
	std::string token;

	// 使用'_'作为分隔符提取数字
	vector<int> vZXY;
	vZXY.clear();

	while (std::getline(ss, token, '_')) {
		try {
			vZXY.push_back(std::stoi(token));
		}
		catch (const std::invalid_argument&)
		{
			return false;
		}
		catch (const std::out_of_range&)
		{
			return false;
		}
	}

	// 确保我们有足够的坐标
	if (vZXY.size() >= 3)
	{
		Z = vZXY[0];
		X = vZXY[1];
		Y = vZXY[2];
	}
	else
	{
		return false;
	}

	return true;
}


/*根据JK级别获取分辨率（地理坐标系，单位：度）*/
double CSE_ManageMetaDataDlg::GetGridResByGridLevel(int iLevel)
{
	switch (iLevel)
	{
	case 0:
		return 512.0;

	case 1:
		return 256.0;

	case 2:
		return 128.0;

	case 3:
		return 64.0;

	case 4:
		return 32.0;

	case 5:
		return 16.0;

	case 6:
		return 8.0;

	case 7:
		return 4.0;

	case 8:
		return 2.0;

	case 9:
		return 1.0;

	case 10:
		return 0.5;

	case 11:
		return 0.25;

	case 12:
		return 1.0 / 12;

	case 13:
		return 1.0 / 60;

	case 14:
		return 1.0 / 120;

	case 15:
		return 1.0 / 240;

	case 16:
		return 1.0 / 720;

	case 17:
		return 1.0 / 3600;

	case 18:
		return 1.0 / 7200;

	case 19:
		return 1.0 / 14400;

	case 20:
		return 1.0 / 28800;

	case 21:
		return 1.0 / 28800;

	case 22:
		return 1.0 / 57600;

	case 23:
		return 1.0 / 115200;

	case 24:
		return 1.0 / 230400;

	case 25:
		return 1.0 / 460800;

	default:
		break;
	}
	return 1.0;
}

/*根据级别、行、列索引计算格网的经纬度范围*/
SE_DRect CSE_ManageMetaDataDlg::CalGridExtentByZXY(int iLevel, int iX, int iY)
{
	// 根据格网获取对应的分辨率
	double dGridRes = GetGridResByGridLevel(iLevel);

	SE_DRect dGridRect;
	dGridRect.dleft = -256 + iX * dGridRes;
	dGridRect.dright = -256 + (iX + 1) * dGridRes;
	dGridRect.dtop = 256 - iY * dGridRes;
	dGridRect.dbottom = 256 - (iY + 1) * dGridRes;
	return dGridRect;
}


/*根据经纬度范围、打包级别计算横向X取值范围、纵向Y取值范围*/
void CSE_ManageMetaDataDlg::CalXRangeAndYRangeByGeoRectAndLevel(
	SE_DRect dRect,
	int iGridLevel,
	int& iMinX,
	int& iMaxX,
	int& iMinY,
	int& iMaxY)
{
	// 根据格网获取对应的分辨率
	double dGridRes = GetGridResByGridLevel(iGridLevel);

	// 计算最小X
	iMinX = int(fabs(dRect.dleft + 256) / dGridRes);

	// 计算最大X
	iMaxX = int(fabs(dRect.dright + 256) / dGridRes);

	// 计算最小Y
	iMinY = int(fabs(256 - dRect.dtop) / dGridRes);

	// 计算最大Y
	iMaxY = int(fabs(256 - dRect.dbottom) / dGridRes);
}