#include "attribute_extract.h"

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

#define _atoi64(val)   strtoll(val,NULL,10)

/*--------------STL---------------*/
#include <filesystem>
#include <fstream>
#include <iostream>
/*--------------STL---------------*/

#endif //

using namespace std;

SE_AttributeExtractDlg::SE_AttributeExtractDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	// 默认比例尺不可选
	//ui.comboBox_Scale->setEnabled(false);
	// 默认输入文件
	ui.radioButton_InputFormat_File->setChecked(true);

	connect(ui.Button_OK, &QPushButton::clicked, this, &SE_AttributeExtractDlg::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &SE_AttributeExtractDlg::Button_Cancel_rejected);
	
	connect(ui.pushButton_Open, &QPushButton::clicked, this, &SE_AttributeExtractDlg::pushButton_Open_clicked);
	connect(ui.pushButton_Save, &QPushButton::clicked, this, &SE_AttributeExtractDlg::pushButton_Save_clicked);
	connect(ui.pushButton_SelectAllLayers, &QPushButton::clicked, this, &SE_AttributeExtractDlg::pushButton_SelectAllLayers_clicked);
	connect(ui.pushButton_CancelSelectAllLayers, &QPushButton::clicked, this, &SE_AttributeExtractDlg::pushButton_CancelSelectAllLayers_clicked);
	//connect(ui.checkBox_OutputSheet, &QCheckBox::stateChanged, this, &SE_AttributeExtractDlg::on_checkBox_OutputSheet_stateChanged);

	// 加载配置文件
	connect(ui.pushButton_LoadExtractParams, &QPushButton::clicked, this, &SE_AttributeExtractDlg::pushButton_LoadExtractParams_clicked);

	// 导出配置文件
	connect(ui.pushButton_SaveExtractParams, &QPushButton::clicked, this, &SE_AttributeExtractDlg::pushButton_SaveExtractParams_clicked);

	// 导出配置文件模板
	connect(ui.pushButton_SaveConfigTemplate, &QPushButton::clicked, this, &SE_AttributeExtractDlg::pushButton_SaveExtractParamsTemplate_clicked);
	
	connect(ui.lineEdit_InputDataPath, &QLineEdit::textChanged, this, &SE_AttributeExtractDlg::on_InputDataPath_TextChanged);
	connect(&m_AttributeExtractThread, &SE_AttributeExtractThread::resultProcess, this, &SE_AttributeExtractDlg::handleResults);

	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputDataPath->setText(m_qstrOutputDataPath);

	// 表头居中
	ui.tableWidget_SetAttribute->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中
	
	ui.tableWidget_SetAttribute->setAlternatingRowColors(true);
	// 可设置背景色
	//ui.tableWidget_SetAttribute->setStyleSheet("QTableView{background-color: rgb(250, 250, 115);""alternate-background-color: rgb(50, 110, 255);}");

	// 设置表格内容双击可编辑
	ui.tableWidget_SetAttribute->setEditTriggers(QAbstractItemView::DoubleClicked);     

	// 最后一列铺满最后
	ui.tableWidget_SetAttribute->horizontalHeader()->setStretchLastSection(true);

	m_iTotalLayerCount = 0;
}

SE_AttributeExtractDlg::~SE_AttributeExtractDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("AttributeExtract/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("AttributeExtract/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
}

void SE_AttributeExtractDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("AttributeExtract/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("AttributeExtract/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}

// 获取输入数据类型
int SE_AttributeExtractDlg::GetInputDataType()
{
	// 如果输入数据源为文件
	if (ui.radioButton_InputFormat_File->isChecked())
	{
		return 1;
	}
	// 如果输入数据源为数据库
	else
	{
		return 2;
	}
}

// 获取输入数据路径
QString SE_AttributeExtractDlg::GetInputDataPath()
{
	return ui.lineEdit_InputDataPath->text();
}

QString SE_AttributeExtractDlg::GetOutputDataPath()
{
	return ui.lineEdit_OutputDataPath->text();
}

void SE_AttributeExtractDlg::GetLayers(vector<LayerFileInfo>& vLayers)
{
	vLayers.clear();

	// 从表格控件中获取图层信息
	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		LayerFileInfo fileInfo;

		// 第1列图层名称
		fileInfo.strLayerName = ui.tableWidget_SetAttribute->item(i, 0)->text().toLocal8Bit();

		// 第6列图层全路径
		fileInfo.strLayerFullPath = ui.tableWidget_SetAttribute->item(i, 5)->text().toLocal8Bit();
		
		vLayers.push_back(fileInfo);
	}
}

void SE_AttributeExtractDlg::GetLayerFields(vector<string>& vLayerFields)
{
	vLayerFields.clear();

	// 从表格控件中获取图层信息
	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		string strFields;
		// 如果属性字段设置不为空
		if (ui.tableWidget_SetAttribute->item(i, 2))
		{
			strFields = ui.tableWidget_SetAttribute->item(i, 2)->text().toLocal8Bit();
		}
		// 如果单元格为空，表示属性全部选择，不进行筛选
		else
		{
			strFields = "";
		}

		vLayerFields.push_back(strFields);
	}

}

void SE_AttributeExtractDlg::GetLayerAttributeFilters(vector<string>& vLayerAttributeFilters)
{
	vLayerAttributeFilters.clear();

	// 从表格控件中获取图层信息
	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		string strAttributeFilters;
		// 如果属性过滤条件不为空
		if (ui.tableWidget_SetAttribute->item(i, 3))
		{
			strAttributeFilters = ui.tableWidget_SetAttribute->item(i, 3)->text().toLocal8Bit();
		}
		// 如果单元格为空，表示属性不过滤
		else
		{
			strAttributeFilters = "";
		}

		vLayerAttributeFilters.push_back(strAttributeFilters);
	}
}

void SE_AttributeExtractDlg::GetLayerSelected(vector<bool>& vLayerSelected)
{
	vLayerSelected.clear();

	// 从表格控件中获取图层选择信息
	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		bool bSelected = false;
		// 如果图层选择状态为1，赋值true；否则赋值false
		if (ui.tableWidget_SetAttribute->item(i, 4))
		{
			string strSelected = ui.tableWidget_SetAttribute->item(i, 4)->text().toLocal8Bit();
			
			if (strSelected == "1")
			{
				bSelected = true;
			}
			else
			{
				bSelected = false;
			}

		}
		// 如果单元格为空，表示属性不过滤
		else
		{
			bSelected = false;
		}

		vLayerSelected.push_back(bSelected);
	}
}

void SE_AttributeExtractDlg::pushButton_Open_clicked()
{
	// 清空图层列表
	m_qstrFilePathList.clear();

	m_vOGRLayers.clear();

	// 获取输入数据类型
	int iInputDataType = GetInputDataType();

	// 如果选择数据库类型，则输入路径为gpkg或gdb文件所在全路径
	if (iInputDataType == 2)
	{
		QString curPath = m_qstrInputDataPath;
		QString dlgTitle = "选择数据库文件";
		QString filter = "GeoPackage文件(*.gpkg *.GPKG)";

		// 选择一个或多个gpkg文件
		QStringList qstr_path_list = QFileDialog::getOpenFileNames(
			this,
			dlgTitle,
			curPath,
			filter);

		if (!qstr_path_list.isEmpty())
		{
			m_qstrInputDataPathList = qstr_path_list;

			QString qstrFileNames;
			for (int i = 0; i < qstr_path_list.size(); ++i)
			{
				qstrFileNames += qstr_path_list[i];
				if(i != qstr_path_list.size() - 1)
				{
					qstrFileNames += ";";
				}				
			}

			ui.lineEdit_InputDataPath->setText(qstrFileNames);

		}
		else
		{
			return;
		}
	}
	// 如果选择文件系统，则输入路径为文件夹目录
	else if (iInputDataType == 1)
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
			ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
		}
		else
		{
			return;
		}
	}

	// 根据GPKG或文件数据类型初始化表格控件
	// |图层名	| 图层类型   |	属性字段设置 |  属性过滤  |  是否选择  |图层全路径|

	// 如果选择文件夹
	if (iInputDataType == 1)
	{
		// 获取子文件夹
		QStringList qstrPathList = GetDirPathOfSplDir(m_qstrInputDataPath);

		// 如果是单图幅路径，遍历当前路径下所有的shp文件
		if (qstrPathList.size() == 0)
		{
			QStringList qstrShpList = GetShpFileNames(m_qstrInputDataPath);
			for (int i = 0; i < qstrShpList.size(); ++i)
			{
				QString qstrTemp = m_qstrInputDataPath + QDir::separator() + qstrShpList[i];
				qstrTemp = QDir::toNativeSeparators(qstrTemp);
				m_qstrFilePathList << qstrTemp;
			}
		}

		// 如果是多图幅路径，遍历每个图幅的shp文件
		else
		{
			for (int i = 0; i < qstrPathList.size(); ++i)
			{
				QStringList qstrShpList = GetShpFileNames(qstrPathList[i]);

				for (int j = 0; j < qstrShpList.size(); ++j)
				{
					QString qstrTemp = qstrPathList[i] + QDir::separator() + qstrShpList[j];
					qstrTemp = QDir::toNativeSeparators(qstrTemp);
					m_qstrFilePathList << qstrTemp;
				}
				
			}			
		}
	}

	// 填充TableWidget
	UpdateTableWidget();

}

void SE_AttributeExtractDlg::pushButton_Save_clicked()
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
	
	ui.lineEdit_OutputDataPath->setText(m_qstrOutputDataPath);
}


// 设置图层全选状态，即所有图层字段全部选中
void SE_AttributeExtractDlg::pushButton_SelectAllLayers_clicked()
{
	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		if (ui.tableWidget_SetAttribute->item(i, 4))
		{
			ui.tableWidget_SetAttribute->item(i, 4)->setText(tr("1"));
		}	
	}

}

void SE_AttributeExtractDlg::Button_OK_accepted()
{
	// 如果输入是图幅文件
	if (GetInputDataType() == 1)
	{
		if (!FilePathIsExisted(ui.lineEdit_InputDataPath->text()))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据提取工具"));
			msgBox.setText(tr("输入数据路径不存在，请重新输入！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}
	}

	// 如果输入是GPKG数据库
	else if(GetInputDataType() == 2)
	{
		QStringList qstrGPKGList = ui.lineEdit_InputDataPath->text().split(";");

		for (int i = 0; i < qstrGPKGList.size(); ++i)
		{
			if (!FileIsExisted(qstrGPKGList[i]))
			{
				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("数据提取工具"));
				msgBox.setText(tr("输入GPKG数据路径:%1不存在，请重新输入！").arg(qstrGPKGList[i]));
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				return;
			}
		}
	}

	// 备注：先不使用CPU多线程实现，涉及并行写入gpkg数据库
	/*获取输入类型:1——文件；2——GPKG数据库*/
	int iInputFormat = GetInputDataType();

	/*获取图层列表*/
	vector<LayerFileInfo> vLayers;
	vLayers.clear();
	GetLayers(vLayers);

	/*获取图层属性字段设置信息*/
	vector<string> vLayerFields;
	vLayerFields.clear();
	GetLayerFields(vLayerFields);

	/*获取属性过滤条件*/
	vector<string> vLayerAttributeFilters;
	vLayerAttributeFilters.clear();
	GetLayerAttributeFilters(vLayerAttributeFilters);

	/*获取图层选择状态*/
	vector<bool> vLayerSelectedState;
	vLayerSelectedState.clear();
	GetLayerSelected(vLayerSelectedState);

	/*输出数据路径*/
	QString qstrOutputDataPath = ui.lineEdit_OutputDataPath->text();

	ui.progressBar->reset();
	
	m_iTotalLayerCount = 0;

	// 待处理的总的图层个数（选择状态为1的记录数）
	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		if (ui.tableWidget_SetAttribute->item(i, 4))
		{
			if (ui.tableWidget_SetAttribute->item(i, 4)->text() == tr("1"))
			{
				m_iTotalLayerCount++;
			}
		}
	}

	m_AttributeExtractThread.SetThreadParams(
		iInputFormat,
		vLayers,
		vLayerFields,
		vLayerAttributeFilters,
		vLayerSelectedState,
		qstrOutputDataPath,
		spdlog::level::level_enum::err);

	ui.Button_OK->setEnabled(false);

}

void SE_AttributeExtractDlg::Button_Cancel_rejected()
{
	reject();
}


void SE_AttributeExtractDlg::pushButton_CancelSelectAllLayers_clicked()
{
	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		if (ui.tableWidget_SetAttribute->item(i, 4))
		{
			ui.tableWidget_SetAttribute->item(i, 4)->setText(tr("0"));
		}
	}
}


QStringList SE_AttributeExtractDlg::GetDirPathOfSplDir(QString dirPath)
{
	QStringList dirPaths;
	QDir splDir(dirPath);
	QFileInfoList fileInfoListInSplDir = splDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	QFileInfo tempFileInfo;
	foreach(tempFileInfo, fileInfoListInSplDir) {
		dirPaths << tempFileInfo.absoluteFilePath();
	}
	return dirPaths;
}

// 判断目录是否存在
bool SE_AttributeExtractDlg::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_AttributeExtractDlg::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


/*void SE_AttributeExtractDlg::on_checkBox_OutputSheet_stateChanged(int arg1)
{
	if (arg1 == Qt::CheckState::Checked)
	{
		ui.comboBox_Scale->setEnabled(true);
	}
	else
	{
		ui.comboBox_Scale->setEnabled(false);
	}
}*/


QStringList SE_AttributeExtractDlg::GetShpFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.shp" << "*.SHP";
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}


bool SE_AttributeExtractDlg::GetOGRLayerNamesFromGPKG(const QString& path,vector<string> &vLayerNames)
{
	vLayerNames.clear();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	string strInputPath = path.toLocal8Bit();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strInputPath.c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL);
	if (!poDS)
	{
		return false;
	}

	OGRLayer* pLayer = nullptr;
	for (int i = 0; i < poDS->GetLayerCount(); ++i)
	{
		pLayer = poDS->GetLayer(i);
		vLayerNames.push_back(pLayer->GetName());
	}

	GDALClose(poDS);
	return true;
}

void SE_AttributeExtractDlg::UpdateTableWidget()
{
	// 清空表格内容
	ui.tableWidget_SetAttribute->clearContents();

	// 如果是文件，逐一填充单元格
	if (GetInputDataType() == 1)
	{
		for (int i = 0; i < m_qstrFilePathList.size(); ++i)
		{
			// 在表格中增加记录
			int iRowCount = 0;
			iRowCount = ui.tableWidget_SetAttribute->rowCount();

			//增加一行
			ui.tableWidget_SetAttribute->insertRow(iRowCount);
				
			// 插入图层名
			string strLayerName = CPLGetBasename(m_qstrFilePathList[i].toLocal8Bit());
			ui.tableWidget_SetAttribute->setItem(iRowCount, 0, new QTableWidgetItem(QString::fromLocal8Bit(strLayerName.c_str())));

			// 插入图层类型
			string strSheetNumber;
			string strLayerType;
			string strGeometryType;
			// 如果是J标图层名称
			if (GetLayerInfo(strLayerName, strSheetNumber, strLayerType, strGeometryType))
			{
				// 图层标识
				QString qstrLayerFlag = QString::fromLocal8Bit(strLayerType.c_str()) + tr("_") + QString::fromLocal8Bit(strGeometryType.c_str());
				ui.tableWidget_SetAttribute->setItem(iRowCount, 1, new QTableWidgetItem(qstrLayerFlag));
			}
			// TODO:其它类型图层按照实际情况修改，此处默认图层类型与图层名称相同
			else
			{
				ui.tableWidget_SetAttribute->setItem(iRowCount, 1, new QTableWidgetItem(QString::fromLocal8Bit(strLayerName.c_str())));
			}

			// 是否选择设置，默认选择☑
			ui.tableWidget_SetAttribute->setItem(iRowCount, 4, new QTableWidgetItem(tr("1")));
			
			// 插入图层全路径
			ui.tableWidget_SetAttribute->setItem(iRowCount, 5, new QTableWidgetItem(m_qstrFilePathList[i]));
		}
	}

	// 如果是GPKG数据库
	else if (GetInputDataType() == 2)
	{
		// 遍历每个GPKG数据库
		for (int i = 0; i < m_qstrInputDataPathList.size(); ++i)
		{
			vector<string> vLayerNames;
			vLayerNames.clear();
	
			// 如果获取图层列表成功
			if (GetOGRLayerNamesFromGPKG(m_qstrInputDataPathList[i], vLayerNames))
			{
				// 遍历gpkg数据库中的每个图层
				for (int j = 0; j < vLayerNames.size(); ++j)
				{
					// 在表格中增加记录
					int iRowCount = 0;
					iRowCount = ui.tableWidget_SetAttribute->rowCount();

					//增加一行
					ui.tableWidget_SetAttribute->insertRow(iRowCount);

					// 插入图层名
					ui.tableWidget_SetAttribute->setItem(iRowCount, 0, new QTableWidgetItem(QString::fromLocal8Bit(vLayerNames[j].c_str())));

					// 插入图层类型
					string strSheetNumber;
					string strLayerType;
					string strGeometryType;
					// 如果是J标图层名称
					if (GetLayerInfo(vLayerNames[j], strSheetNumber, strLayerType, strGeometryType))
					{
						// 图层标识
						QString qstrLayerFlag = QString::fromLocal8Bit(strLayerType.c_str()) + tr("_") + QString::fromLocal8Bit(strGeometryType.c_str());
						ui.tableWidget_SetAttribute->setItem(iRowCount, 1, new QTableWidgetItem(qstrLayerFlag));
					}
					// TODO:其它类型图层按照实际情况修改，此处默认图层类型与图层名称相同
					else
					{
						ui.tableWidget_SetAttribute->setItem(iRowCount, 1, new QTableWidgetItem(QString::fromLocal8Bit(vLayerNames[j].c_str())));
					}

					// 是否选择设置，默认选择☑
					ui.tableWidget_SetAttribute->setItem(iRowCount, 4, new QTableWidgetItem(tr("1")));

					// 插入图层全路径
					ui.tableWidget_SetAttribute->setItem(iRowCount, 5, new QTableWidgetItem(m_qstrInputDataPathList[i]));
				}	
			}
		}
	}


	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		ui.tableWidget_SetAttribute->item(i, 1)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_SetAttribute->item(i, 4)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	}

	// 图层名设置自适应宽度
	ui.tableWidget_SetAttribute->resizeColumnToContents(0);


	// 图层全路径自适应宽度																						   // 图层全路径自适应宽度
	ui.tableWidget_SetAttribute->resizeColumnToContents(5);

	// 设置图层名、图层标识、图层全路径不可编辑
	
	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		// 图层名
		ui.tableWidget_SetAttribute->item(i, 0)->setFlags(Qt::NoItemFlags);
		
		// 图层标识
		ui.tableWidget_SetAttribute->item(i, 1)->setFlags(Qt::NoItemFlags);
		
		// 图层名
		ui.tableWidget_SetAttribute->item(i, 5)->setFlags(Qt::NoItemFlags);
	}
}

void SE_AttributeExtractDlg::GetAttributeExtractParam(vector<AttributeExtractParam>& vAttributeExtractParam)
{
	vAttributeExtractParam.clear();

	// 从tableWidget_SetAttribute中获取提取参数
	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		AttributeExtractParam param;

		if (ui.tableWidget_SetAttribute->item(i, 1))
		{
			// 获取图层标识
			param.strLayerFlag = ui.tableWidget_SetAttribute->item(i, 1)->text().toLocal8Bit();
		}

		if (ui.tableWidget_SetAttribute->item(i, 2))
		{
			// 获取字段列表
			param.strFields = ui.tableWidget_SetAttribute->item(i, 2)->text().toLocal8Bit();
		}
		
		if (ui.tableWidget_SetAttribute->item(i, 3))
		{
			// 获取属性过滤条件
			param.strAttributeFilter = ui.tableWidget_SetAttribute->item(i, 3)->text().toLocal8Bit();
		}

		// 判断是否有重复，如果没有则追加
		if (!IsExistedInVector(param, vAttributeExtractParam))
		{
			vAttributeExtractParam.push_back(param);
		}	
	}
}

void SE_AttributeExtractDlg::UpdateTableWidgetExtractParams()
{
	for (int i = 0; i < ui.tableWidget_SetAttribute->rowCount(); ++i)
	{
		// 根据图层类型填充属性字段和属性过滤条件
		string strLayerName = ui.tableWidget_SetAttribute->item(i, 1)->text().toLocal8Bit();
		
		AttributeExtractParam param;
		GetExtractParamByLayerName(strLayerName, m_vAttributeExtractParam, param);

		// 插入图层属性字段
		// 构造属性字符串
		ui.tableWidget_SetAttribute->setItem(i, 2, new QTableWidgetItem(QString::fromLocal8Bit(param.strFields.c_str())));

		// 插入属性筛选条件
		ui.tableWidget_SetAttribute->setItem(i, 3, new QTableWidgetItem(QString::fromLocal8Bit(param.strAttributeFilter.c_str())));

	}

	// 属性字段设置自适应宽度
	ui.tableWidget_SetAttribute->resizeColumnToContents(2);

	// 属性过滤自适应宽度
	ui.tableWidget_SetAttribute->resizeColumnToContents(3);
}

void SE_AttributeExtractDlg::pushButton_LoadExtractParams_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择数据提取配置文件");
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
	// 打开CSV文件
	GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(strCsv.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));

	if (!poDS) 
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据提取工具"));
		msgBox.setText(tr("打开数据提取配置文件失败！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return ;
	}

	// 获取图层（CSV文件被视为一个图层）
	OGRLayer* poLayer = poDS->GetLayer(0);

	// 重置图层的读取指针到起始位置
	poLayer->ResetReading();

	// 遍历图层中的所有要素（即CSV文件中的每一行）
	OGRFeature* poFeature = nullptr;
	
	while ((poFeature = poLayer->GetNextFeature()) != NULL) 
	{
		// 获取要素的所有属性（即CSV文件中的每一列）
		AttributeExtractParam param;

		// 第一列图层标识
		param.strLayerFlag = poFeature->GetFieldAsString(0);

		// 第二列字段列表
		param.strFields = poFeature->GetFieldAsString(1);

		// 第三列属性过滤条件
		param.strAttributeFilter = poFeature->GetFieldAsString(2);

		m_vAttributeExtractParam.push_back(param);

		// 释放要素资源
		OGRFeature::DestroyFeature(poFeature);
	}

	// 关闭并释放数据集资源
	GDALClose(poDS);

	// 更新表格控件中的数据提取参数
	UpdateTableWidgetExtractParams();

}

void SE_AttributeExtractDlg::pushButton_SaveExtractParams_clicked()
{
	vector<AttributeExtractParam> vParams;
	vParams.clear();

	// 获取表格控件中的内容
	GetAttributeExtractParam(vParams);

	// 选择保存路径
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("保存数据提取工具配置文件");
	QString filter = tr("csv 文件(*.csv)");
	QString qstrFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!qstrFileName.isEmpty())
	{
		// 初始化GDAL
		GDALAllRegister();
		CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
		CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段

		// 获取CSV驱动
		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("CSV");
		if (poDriver == NULL) {
			//std::cout << "CSV driver not available.\n";
			return;
		}

		string strOutputCSV = qstrFileName.toLocal8Bit();

		// 创建CSV文件
		GDALDataset* poDS;
		poDS = poDriver->Create(strOutputCSV.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL) 
		{
			//std::cout << "Creation of output file failed.\n";
			return;
		}

		string strCSVName = CPLGetBasename(strOutputCSV.c_str());

		// 获取图层，如果没有则创建
		OGRLayer* poLayer = poDS->CreateLayer(strCSVName.c_str(), NULL, wkbNone, NULL);
		if (poLayer == NULL) 
		{
			//std::cout << "Layer creation failed.\n";
			return;
		}
		

		// 创建“图层标识”字段
		QString qstrLayerFlag = tr("图层标识");
		string strLayerFlag = qstrLayerFlag.toLocal8Bit();
		OGRFieldDefn oLayerFlagField(strLayerFlag.c_str(), OFTString);
		oLayerFlagField.SetWidth(50);
		if (poLayer->CreateField(&oLayerFlagField) != OGRERR_NONE) 
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		// 创建“字段列表”字段
		QString qstrLayerField = tr("字段列表");
		string strLayerField = qstrLayerField.toLocal8Bit();
		OGRFieldDefn oLayerField(strLayerField.c_str(), OFTString);
		oLayerField.SetWidth(1000);
		if (poLayer->CreateField(&oLayerField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		// 创建“属性过滤条件”字段
		QString qstrLayerAttrFilter = tr("属性过滤条件");
		string strLayerAttrFilter = qstrLayerAttrFilter.toLocal8Bit();
		OGRFieldDefn oLayerAttrFilterField(strLayerAttrFilter.c_str(), OFTString);
		oLayerAttrFilterField.SetWidth(500);
		if (poLayer->CreateField(&oLayerAttrFilterField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		for (int i = 0; i < vParams.size(); ++i)
		{
			// 创建要素
			OGRFeature* poFeature;
			poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
			poFeature->SetField(0, vParams[i].strLayerFlag.c_str());
			poFeature->SetField(1, vParams[i].strFields.c_str());
			poFeature->SetField(2, vParams[i].strAttributeFilter.c_str());
			
			if (poLayer->CreateFeature(poFeature) != OGRERR_NONE) 
			{
				//std::cout << "Failed to create feature in shapefile.\n";
				return;
			}

			// 清理
			OGRFeature::DestroyFeature(poFeature);
		}
	
		GDALClose(poDS);

		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据提取工具"));
		msgBox.setText(tr("导出配置文件成功！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
	}
	else
	{
		return;
	}

}

void SE_AttributeExtractDlg::pushButton_SaveExtractParamsTemplate_clicked()
{
	// 选择保存路径
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("保存数据提取工具配置文件模板");
	QString filter = tr("csv 文件(*.csv)");
	QString qstrFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!qstrFileName.isEmpty())
	{
		// 初始化GDAL
		GDALAllRegister();
		CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
		CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段

		// 获取CSV驱动
		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("CSV");
		if (poDriver == NULL) {
			//std::cout << "CSV driver not available.\n";
			return;
		}

		string strOutputCSV = qstrFileName.toLocal8Bit();

		// 创建CSV文件
		GDALDataset* poDS;
		poDS = poDriver->Create(strOutputCSV.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL)
		{
			//std::cout << "Creation of output file failed.\n";
			return;
		}

		string strCSVName = CPLGetBasename(strOutputCSV.c_str());

		// 获取图层，如果没有则创建
		OGRLayer* poLayer = poDS->CreateLayer(strCSVName.c_str(), NULL, wkbNone, NULL);
		if (poLayer == NULL)
		{
			//std::cout << "Layer creation failed.\n";
			return;
		}


		// 创建“图层标识”字段
		QString qstrLayerFlag = tr("图层标识");
		string strLayerFlag = qstrLayerFlag.toLocal8Bit();
		OGRFieldDefn oLayerFlagField(strLayerFlag.c_str(), OFTString);
		oLayerFlagField.SetWidth(50);
		if (poLayer->CreateField(&oLayerFlagField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		// 创建“字段列表”字段
		QString qstrLayerField = tr("字段列表");
		string strLayerField = qstrLayerField.toLocal8Bit();
		OGRFieldDefn oLayerField(strLayerField.c_str(), OFTString);
		oLayerField.SetWidth(1000);
		if (poLayer->CreateField(&oLayerField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		// 创建“属性过滤条件”字段
		QString qstrLayerAttrFilter = tr("属性过滤条件");
		string strLayerAttrFilter = qstrLayerAttrFilter.toLocal8Bit();
		OGRFieldDefn oLayerAttrFilterField(strLayerAttrFilter.c_str(), OFTString);
		oLayerAttrFilterField.SetWidth(500);
		if (poLayer->CreateField(&oLayerAttrFilterField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		// 样例
		vector<AttributeExtractParam> vParams;
		vParams.clear();

		AttributeExtractParam param1;
		param1.strLayerFlag = "A_point";
		param1.strFields = "Field1|Field2|Field3";
		QString qstrFilter1 = tr("\"编码\" = 110101");
		param1.strAttributeFilter = qstrFilter1.toLocal8Bit();
		vParams.push_back(param1);

		AttributeExtractParam param2;
		param2.strLayerFlag = "B_point";
		param2.strFields = "Field1|Field2|Field3";
		param2.strAttributeFilter = "";
		vParams.push_back(param2);

		AttributeExtractParam param3;
		param3.strLayerFlag = "C_point";
		param3.strFields = "";
		QString qstrFilter3 = tr("\"编码\" = 220101");
		param3.strAttributeFilter = qstrFilter3.toLocal8Bit();
		vParams.push_back(param3);

		AttributeExtractParam param4;
		param4.strLayerFlag = "D_line";
		param4.strFields = "";	
		param4.strAttributeFilter = "";
		vParams.push_back(param4);

		for (int i = 0; i < vParams.size(); ++i)
		{
			// 创建要素
			OGRFeature* poFeature;
			poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
			poFeature->SetField(0, vParams[i].strLayerFlag.c_str());
			poFeature->SetField(1, vParams[i].strFields.c_str());
			poFeature->SetField(2, vParams[i].strAttributeFilter.c_str());

			if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
			{
				//std::cout << "Failed to create feature in shapefile.\n";
				return;
			}

			// 清理
			OGRFeature::DestroyFeature(poFeature);
		}

		GDALClose(poDS);

		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据提取工具"));
		msgBox.setText(tr("导出配置文件模板成功！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
	}
	else
	{
		return;
	}


}

void SE_AttributeExtractDlg::GetExtractParamByLayerName(
	string strLayerName,
	vector<AttributeExtractParam>& vAttributeExtractParam,
	AttributeExtractParam& param)
{
	for (int i = 0; i < vAttributeExtractParam.size(); ++i)
	{
		if (strLayerName == vAttributeExtractParam[i].strLayerFlag)
		{
			param = vAttributeExtractParam[i];
			break;
		}
	}
}


bool SE_AttributeExtractDlg::GetLayerInfo(string strLayerName,
	string& strSheetNumber,
	string& strLayerType,
	string& strGeometryType)
{
	int iIndexOfSheet = strLayerName.find_first_of("_");
	// 如果图层非标准图层
	if (iIndexOfSheet == string::npos)
	{
		return false;
	}

	strSheetNumber = strLayerName.substr(0, iIndexOfSheet);
	int iIndexOfLayerType = strLayerName.find_last_of("_");
	strLayerType = strLayerName.substr(iIndexOfSheet + 1, 1);
	strGeometryType = strLayerName.substr(iIndexOfLayerType + 1, strLayerName.length() - 1);
	
	return true;
}


void SE_AttributeExtractDlg::on_InputDataPath_TextChanged(const QString& qstr)
{
	// 清空表格控件
	// 删除所有行
	while (ui.tableWidget_SetAttribute->rowCount() > 0) 
	{
		ui.tableWidget_SetAttribute->removeRow(0);
	}
	ui.progressBar->reset();
}



bool SE_AttributeExtractDlg::IsExistedInVector(AttributeExtractParam param, vector<AttributeExtractParam>& vAttributeExtractParam)
{
	for (int i = 0; i < vAttributeExtractParam.size(); ++i)
	{
		if (param.strAttributeFilter == vAttributeExtractParam[i].strAttributeFilter
			&& param.strFields == vAttributeExtractParam[i].strFields
			&& param.strLayerFlag == vAttributeExtractParam[i].strLayerFlag)
		{
			return true;
		}
	}
	return false;
}

void SE_AttributeExtractDlg::handleResults(const int& iTotalProcessed, const QString& s)
{

	//	如果总的已经被处理的分幅数据个数与总的图层数据相同的话，说明已经处理完成了（成功处理的分幅数据+没有成功处理的分幅数据）
	if (iTotalProcessed == m_iTotalLayerCount)
	{
		ui.progressBar->setValue(int(100));
		ui.Button_OK->setEnabled(true);
		QString qstrTitle = tr("数据提取工具");
		//	拼接窗口显示的信息
		QString qstrText = tr("数据提取完成！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("AttributeExtract/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("AttributeExtract/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);

	}
	else
	{
		ui.progressBar->setValue(int(100.0 * iTotalProcessed / m_iTotalLayerCount));
	}
}


