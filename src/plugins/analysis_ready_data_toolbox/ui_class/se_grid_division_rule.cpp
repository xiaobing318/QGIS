#define _HAS_STD_BYTE 0
#include "se_grid_division_rule.h"

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



using namespace std;

CSE_GridDivisionRuleDlg::CSE_GridDivisionRuleDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.pushButton_OK, &QPushButton::clicked, this, &CSE_GridDivisionRuleDlg::pushButton_OK_clicked);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &CSE_GridDivisionRuleDlg::pushButton_Cancel_clicked);
	
	connect(ui.pushButton_LoadFile, &QPushButton::clicked, this, &CSE_GridDivisionRuleDlg::pushButton_LoadFile_clicked);

	connect(ui.lineEdit_InputPath, &QLineEdit::textChanged, this, &CSE_GridDivisionRuleDlg::on_InputDataPath_TextChanged);
	

	restoreState();
	ui.lineEdit_InputPath->setText(m_qstrInputDataPath);

	// 表头居中
	ui.tableWidget_GridDivisionRule->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中
	
	// 颜色交叉
	ui.tableWidget_GridDivisionRule->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_GridDivisionRule->horizontalHeader()->setStretchLastSection(true);

	m_vGridDivisionRule.clear();
}

CSE_GridDivisionRuleDlg::~CSE_GridDivisionRuleDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("GRID_DIVISION_RULE/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
}

void CSE_GridDivisionRuleDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("GRID_DIVISION_RULE/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}



// 获取输入数据路径
QString CSE_GridDivisionRuleDlg::GetInputDataPath()
{
	return ui.lineEdit_InputPath->text();
}



void CSE_GridDivisionRuleDlg::UpdateTableWidget()
{
	// 清空表格内容
	ui.tableWidget_GridDivisionRule->clearContents();

	// 如果是文件，逐一填充单元格
	for (int i = 0; i < m_vGridDivisionRule.size(); ++i)
	{
		// 在表格中增加记录
		int iRowCount = 0;
		iRowCount = ui.tableWidget_GridDivisionRule->rowCount();

		// 增加一行
		ui.tableWidget_GridDivisionRule->insertRow(iRowCount);
				
		// 插入基础格网划分级别
		ui.tableWidget_GridDivisionRule->setItem(iRowCount, 0, new QTableWidgetItem(QString::number(m_vGridDivisionRule[i].iBaseLevel)));

		// 插入就绪数据格网级别
		ui.tableWidget_GridDivisionRule->setItem(iRowCount, 1, new QTableWidgetItem(QString::number(m_vGridDivisionRule[i].iDataLevel)));

		// 插入就绪数据格网分辨率
		ui.tableWidget_GridDivisionRule->setItem(iRowCount, 2, new QTableWidgetItem(QString::fromLocal8Bit(m_vGridDivisionRule[i].strResolution.c_str())));

		// 插入就绪数据格网像素宽度×高度
		ui.tableWidget_GridDivisionRule->setItem(iRowCount, 3, new QTableWidgetItem(QString::fromLocal8Bit(m_vGridDivisionRule[i].strGridSize.c_str())));
	}
	
	for (int i = 0; i < ui.tableWidget_GridDivisionRule->rowCount(); ++i)
	{
		ui.tableWidget_GridDivisionRule->item(i, 0)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_GridDivisionRule->item(i, 1)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_GridDivisionRule->item(i, 2)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_GridDivisionRule->item(i, 3)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	}

	// 自适应宽度
	ui.tableWidget_GridDivisionRule->resizeColumnToContents(0);
	ui.tableWidget_GridDivisionRule->resizeColumnToContents(1);
	ui.tableWidget_GridDivisionRule->resizeColumnToContents(2);
	ui.tableWidget_GridDivisionRule->resizeColumnToContents(3);

	// 设置表格内容不可编辑
	
	for (int i = 0; i < ui.tableWidget_GridDivisionRule->rowCount(); ++i)
	{
		ui.tableWidget_GridDivisionRule->item(i, 0)->setFlags(Qt::NoItemFlags);
		ui.tableWidget_GridDivisionRule->item(i, 1)->setFlags(Qt::NoItemFlags);
		ui.tableWidget_GridDivisionRule->item(i, 2)->setFlags(Qt::NoItemFlags);
		ui.tableWidget_GridDivisionRule->item(i, 3)->setFlags(Qt::NoItemFlags);
	}
}


void CSE_GridDivisionRuleDlg::pushButton_LoadFile_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择格网划分规则配置文件");
	QString filter = tr("csv 文件(*.csv)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	// 读取csv文件
	// 初始化GDAL
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	string strCsv = strFileName.toLocal8Bit();

	m_qstrInputDataPath = strFileName;

	// 打开CSV文件
	GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(strCsv.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));

	if (!poDS) 
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("打开配置文件失败！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return ;
	}

	// 获取图层（CSV文件被视为一个图层）
	OGRLayer* poLayer = poDS->GetLayer(0);
	if (!poLayer)
	{
		return;
	}

	// 重置图层的读取指针到起始位置
	poLayer->ResetReading();

	// 遍历图层中的所有要素（即CSV文件中的每一行）
	OGRFeature* poFeature = nullptr;
	
	while ((poFeature = poLayer->GetNextFeature()) != NULL) 
	{
		GRID_DIVISION_RULE rule;

		// 获取要素的所有属性（即CSV文件中的每一列）

		// 第一列基础格网划分级别
		rule.iBaseLevel = poFeature->GetFieldAsInteger(0);

		// 第二列就绪数据格网级别
		rule.iDataLevel = poFeature->GetFieldAsInteger(1);

		// 第三列就绪数据格网分辨率
		rule.strResolution = poFeature->GetFieldAsString(2);

		// 第四列就绪数据格网像素宽度×高度
		rule.strGridSize = poFeature->GetFieldAsString(3);

		m_vGridDivisionRule.push_back(rule);

		// 释放要素资源
		OGRFeature::DestroyFeature(poFeature);
	}

	// 关闭并释放数据集资源
	GDALClose(poDS);

	// 更新表格控件中的数据
	UpdateTableWidget();

}

void CSE_GridDivisionRuleDlg::pushButton_OK_clicked()
{

	// 如果文件夹不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_InputPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("GRID_DIVISION_RULE/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	accept();
}

void CSE_GridDivisionRuleDlg::pushButton_Cancel_clicked()
{
	reject();
}

void CSE_GridDivisionRuleDlg::on_InputDataPath_TextChanged()
{

}


#pragma region  "检查文件或文件夹是否存在"

bool CSE_GridDivisionRuleDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion