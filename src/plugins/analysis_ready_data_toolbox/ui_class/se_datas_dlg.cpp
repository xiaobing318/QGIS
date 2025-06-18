#define _HAS_STD_BYTE 0

#include "se_datas_dlg.h"
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

SE_DatasDlg::SE_DatasDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.pushButton_OK, &QPushButton::clicked, this, &SE_DatasDlg::pushButton_OK_clicked);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &SE_DatasDlg::pushButton_Cancel_clicked);
	
	connect(ui.pushButton_LoadFile, &QPushButton::clicked, this, &SE_DatasDlg::pushButton_LoadFile_clicked);

	connect(ui.lineEdit_InputPath, &QLineEdit::textChanged, this, &SE_DatasDlg::on_InputDataPath_TextChanged);
	

	restoreState();
	ui.lineEdit_InputPath->setText(m_qstrInputDataPath);

	// 表头居中
	ui.tableWidget_Datas->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中
	
	// 颜色交叉
	ui.tableWidget_Datas->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_Datas->horizontalHeader()->setStretchLastSection(true);

}

SE_DatasDlg::~SE_DatasDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("DATAS/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
}

void SE_DatasDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("DATAS/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}



// 获取输入数据路径
QString SE_DatasDlg::GetInputDataPath()
{
	return ui.lineEdit_InputPath->text();
}



void SE_DatasDlg::UpdateTableWidget()
{
	// 清空表格内容
	ui.tableWidget_Datas->clearContents();

	// 填充模型标识（0）|模型名称（1）|是否生成（2）
	for (int i = 0; i < m_vDataIdentifys.size(); ++i)
	{
		// 在表格中增加记录
		int iRowCount = 0;
		iRowCount = ui.tableWidget_Datas->rowCount();

		// 增加一行
		ui.tableWidget_Datas->insertRow(iRowCount);

		// 插入标识（0）
		ui.tableWidget_Datas->setItem(iRowCount, 0, new QTableWidgetItem(m_vDataIdentifys[i]));

		// 插入名称（1）
		ui.tableWidget_Datas->setItem(iRowCount, 1, new QTableWidgetItem(m_vDataNames[i]));

		// 是否生成为复选框，默认为选中状态
		QTableWidgetItem* item = new QTableWidgetItem();
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);		// 设置为可复选
		item->setCheckState(Qt::Checked);								// 初始状态为选中

		// 设置复选框居中
		item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_Datas->setItem(iRowCount, 2, item);
	}
}

void SE_DatasDlg::GetDataInfosFromDataDlg(vector<QString>& vModelIdentifys, vector<QString>& vModelNames)
{
	// 遍历表格的每一行
	for (int i = 0; i < ui.tableWidget_Datas->rowCount(); ++i)
	{
		// 如果选择生产
		if (ui.tableWidget_Datas->item(i, 2)->checkState() == Qt::Checked)
		{
			// 模型标识
			vModelIdentifys.push_back(ui.tableWidget_Datas->item(i, 0)->text());

			// 模型名称
			vModelNames.push_back(ui.tableWidget_Datas->item(i, 1)->text());
			
		}	
	}
}


void SE_DatasDlg::pushButton_LoadFile_clicked()
{
	// 读取csv文件
	// 初始化GDAL
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择分析模型配置文件");
	QString filter = tr("csv 文件(*.csv)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	string strCsv = strFileName.toLocal8Bit();
	ui.lineEdit_InputPath->setText(strFileName);
	m_qstrInputFilePath = strFileName;

	// 打开CSV文件
	GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(strCsv.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));

	if (!poDS)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("打开分析模型配置文件失败！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
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
	m_vDataIdentifys.clear();
	m_vDataNames.clear();
	while ((poFeature = poLayer->GetNextFeature()) != NULL)
	{	
		// 获取要素的所有属性（即CSV文件中的每一列）

		// 第一列“模型标识”
		QString qstrModelIdentify = QString::fromUtf8(poFeature->GetFieldAsString(0));
		m_vDataIdentifys.push_back(qstrModelIdentify);

		// 第二列“模型名称名称”
		QString qstrModelName = QString::fromUtf8(poFeature->GetFieldAsString(1));
		m_vDataNames.push_back(qstrModelName);

		// 释放要素资源
		OGRFeature::DestroyFeature(poFeature);
	}

	// 关闭并释放数据集资源
	GDALClose(poDS);

	// 删除所有行
	while (ui.tableWidget_Datas->rowCount() > 0)
	{
		ui.tableWidget_Datas->removeRow(0);
	}

	// 更新表格控件中的数据
	UpdateTableWidget();
	
	// 调整表格样式
	AdjustTableWidgetStyle();

}

void SE_DatasDlg::pushButton_OK_clicked()
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

	GetDataInfosFromDataDlg(m_vDataIdentifys, m_vDataNames);

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("DATAS/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	accept();
}

void SE_DatasDlg::pushButton_Cancel_clicked()
{
	reject();
}

void SE_DatasDlg::on_InputDataPath_TextChanged()
{

}


/*调整表格*/
void SE_DatasDlg::AdjustTableWidgetStyle()
{
	for (int i = 0; i < ui.tableWidget_Datas->rowCount(); ++i)
	{
		for (int j = 0; j < ui.tableWidget_Datas->columnCount(); ++j)
		{
			if (ui.tableWidget_Datas->item(i, j))
			{
				ui.tableWidget_Datas->item(i, j)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
			}
		}
	}

	for (int i = 0; i < ui.tableWidget_Datas->columnCount(); ++i)
	{
		// 自适应宽度
		ui.tableWidget_Datas->resizeColumnToContents(i);
	}
}


#pragma region  "检查文件或文件夹是否存在"

bool SE_DatasDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion