#define _HAS_STD_BYTE 0
// 使用json头文件
#include <nlohmann/json.hpp>
#include "se_pack_tif_ARD.h"

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

using ordered_json = nlohmann::ordered_json;
using namespace std;

SE_PackTifARDDlg::SE_PackTifARDDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.Button_OK , &QPushButton::clicked, this, &SE_PackTifARDDlg::pushButton_OK_clicked);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &SE_PackTifARDDlg::pushButton_Cancel_clicked);
	
	connect(ui.pushButton_Open, &QPushButton::clicked, this, &SE_PackTifARDDlg::pushButton_Open_clicked);
	connect(ui.pushButton_OpenTerrainFactorsMetaData, &QPushButton::clicked, this, &SE_PackTifARDDlg::pushButton_OpenTerrainMetaData_clicked);
	connect(ui.pushButton_OpenRasterizeMetaData, &QPushButton::clicked, this, &SE_PackTifARDDlg::pushButton_OpenRasterizeMetaData_clicked);

	connect(ui.pushButton_Save, &QPushButton::clicked, this, &SE_PackTifARDDlg::pushButton_Save_clicked);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &SE_PackTifARDDlg::pushButton_SaveLog_clicked);


	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputPath->setText(m_qstrOutputDataPath);
	ui.lineEdit_InputTerrainFactorsPath->setText(m_qstrInputTerrainPath);
	ui.lineEdit_InputRasterizePath->setText(m_qstrInputRasterizePath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);
}

SE_PackTifARDDlg::~SE_PackTifARDDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("PACK_TIF_ARD/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_TIF_ARD/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_TIF_ARD/InputTerrainPath"), m_qstrInputTerrainPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_TIF_ARD/InputRasterizePath"), m_qstrInputRasterizePath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_TIF_ARD/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_PackTifARDDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("PACK_TIF_ARD/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("PACK_TIF_ARD/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrInputTerrainPath = settings.value(QStringLiteral("PACK_TIF_ARD/InputTerrainPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrInputRasterizePath = settings.value(QStringLiteral("PACK_TIF_ARD/InputRasterizePath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputLogPath = settings.value(QStringLiteral("PACK_TIF_ARD/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}



void SE_PackTifARDDlg::pushButton_OpenTerrainMetaData_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择地形因子元数据文件");
	QString filter = tr("json 文件(*.json)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	m_qstrInputTerrainPath = strFileName;
	ui.lineEdit_InputTerrainFactorsPath->setText(strFileName);

}

void SE_PackTifARDDlg::pushButton_OpenRasterizeMetaData_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择矢量数据栅格化元数据文件");
	QString filter = tr("json 文件(*.json)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	m_qstrInputRasterizePath = strFileName;
	ui.lineEdit_InputRasterizePath->setText(strFileName);
}

void SE_PackTifARDDlg::pushButton_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputDataPath;
	QString dlgTile = tr("请选择分析就绪数据包保存路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		m_qstrOutputDataPath = selectedDir;
		ui.lineEdit_OutputPath->setText(m_qstrOutputDataPath);
	}
}

void SE_PackTifARDDlg::pushButton_OK_clicked()
{
	// 日志级别
	// 0——错误；1——信息；2——调试
	int iLogLevel = ui.comboBox_LogLevel->currentIndex();

	// 日志结果保存路径
	string strOutputLogPath = ui.lineEdit_OutputLogPath->text().toLocal8Bit();

	// 如果不存在
	if (!CheckFileOrDirExist(ui.lineEdit_OutputLogPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_OutputLogPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 如果文件夹不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_InputDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 如果文件不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputTerrainFactorsPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_InputTerrainFactorsPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 如果文件不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputRasterizePath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_InputRasterizePath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 如果文件夹不存在
	if (!CheckFileOrDirExist(ui.lineEdit_OutputPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_OutputPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	ui.progressBar->reset();

	// 输入数据路径
	string strInputPath = ui.lineEdit_InputDataPath->text().toLocal8Bit();

	// 分析结果保存路径
	string strOutputPath = ui.lineEdit_OutputPath->text().toLocal8Bit();

	// 地形因子元数据路径
	string strTerrainFactorPath = ui.lineEdit_InputTerrainFactorsPath->text().toLocal8Bit();

	// 矢量数据栅格化元数据路径
	string strRasterizePath = ui.lineEdit_InputRasterizePath->text().toLocal8Bit();

	// 创建任务
	SE_PackTifARDTask* task = new SE_PackTifARDTask(
		tr("通用栅格分析就绪型数据封装"),
		strInputPath,
		strOutputPath,
		iLogLevel,
		strOutputLogPath);
	QgsApplication::taskManager()->addTask(task);

	connect(task, &SE_PackTifARDTask::taskFinished, this, &SE_PackTifARDDlg::onTaskFinished);


#pragma region "生成栅格分析就绪数据包元数据"

	/*
	1）读取地形因子元数据；
	2）读取矢量栅格化元数据
	3）构建栅格分析就绪数据包元数据项并输出json文件，
	合并分辨率、比例尺分母，修改数据格式

	*/
	// 地形因子元数据
	RasterDataMetadataInfo terrainMetaInfo;
	if (!ReadMetaDataJsonFile(strTerrainFactorPath, terrainMetaInfo))
	{
		// 记录打开元数据失败日志
	}

	// 矢量数据栅格化元数据
	RasterDataMetadataInfo rasterizeMetaInfo;
	if (!ReadMetaDataJsonFile(strRasterizePath, rasterizeMetaInfo))
	{
		// 记录打开元数据失败日志
	}




	// 栅格分析就绪型数据包元数据
	RasterDataMetadataInfo rasterARDPackageMetaData;


	rasterARDPackageMetaData.i_grid_level = terrainMetaInfo.i_grid_level;
	rasterARDPackageMetaData.i_data_level = terrainMetaInfo.i_data_level;
	rasterARDPackageMetaData.str_data_format = terrainMetaInfo.str_data_format;
	rasterARDPackageMetaData.str_data_type = terrainMetaInfo.str_data_type;
	rasterARDPackageMetaData.str_crs = terrainMetaInfo.str_crs;

	// 取最小值
	rasterARDPackageMetaData.i_min_x = std::min(terrainMetaInfo.i_min_x, rasterizeMetaInfo.i_min_x);

	// 取最大值
	rasterARDPackageMetaData.i_max_x = std::max(terrainMetaInfo.i_max_x, rasterizeMetaInfo.i_max_x);

	// 取最小值
	rasterARDPackageMetaData.i_min_y = std::min(terrainMetaInfo.i_min_y, rasterizeMetaInfo.i_min_y);

	// 取最大值
	rasterARDPackageMetaData.i_max_y = std::max(terrainMetaInfo.i_max_y, rasterizeMetaInfo.i_max_y);

	// 取最小值
	rasterARDPackageMetaData.d_left = std::min(terrainMetaInfo.d_left, rasterizeMetaInfo.d_left);

	// 取最大值
	rasterARDPackageMetaData.d_top = std::max(terrainMetaInfo.d_top, rasterizeMetaInfo.d_top);

	// 取最大值
	rasterARDPackageMetaData.d_right = std::max(terrainMetaInfo.d_right, rasterizeMetaInfo.d_right);

	// 取最小值
	rasterARDPackageMetaData.d_bottom = std::min(terrainMetaInfo.d_bottom, rasterizeMetaInfo.d_bottom);
	rasterARDPackageMetaData.i_data_source = terrainMetaInfo.i_data_source;
	rasterARDPackageMetaData.str_detail = terrainMetaInfo.str_detail + "|" + rasterizeMetaInfo.str_detail;


	// 产品名称赋值为分析就绪数据产品类型，用"|"分割
	vector<string> vSubDir;
	vSubDir.clear();
	GetSubDirFromFilePath(strInputPath, vSubDir);
	rasterARDPackageMetaData.strGLXX_CPMC = "";
	rasterARDPackageMetaData.strZTXX_CPMC = "";
	for (int i = 0; i < vSubDir.size(); ++i)
	{
		rasterARDPackageMetaData.strGLXX_CPMC += vSubDir[i];
		rasterARDPackageMetaData.strZTXX_CPMC += vSubDir[i];
		// 除最后一个外，均加"|"分割
		if (i < vSubDir.size() - 1)
		{
			rasterARDPackageMetaData.strGLXX_CPMC += "|";
			rasterARDPackageMetaData.strZTXX_CPMC += "|";
		}
	}

	// 更新生产时间
	QDateTime curDateTime = QDateTime::currentDateTime();
	QString qstrDataTime = curDateTime.toString("yyyy-MM-dd hh:mm:ss");
	string strDataTime = qstrDataTime.toLocal8Bit();

	rasterARDPackageMetaData.strGLXX_SCRQ = strDataTime;
	rasterARDPackageMetaData.strZTXX_SCRQ = strDataTime;


	rasterARDPackageMetaData.strGLXX_SCDW = terrainMetaInfo.strGLXX_SCDW + "|" + rasterizeMetaInfo.strGLXX_SCDW;
	rasterARDPackageMetaData.strGLXX_HJDW = terrainMetaInfo.strGLXX_HJDW + "|" + rasterizeMetaInfo.strGLXX_HJDW;
	rasterARDPackageMetaData.strGLXX_GXRQ = terrainMetaInfo.strGLXX_GXRQ + "|" + rasterizeMetaInfo.strGLXX_GXRQ;
	rasterARDPackageMetaData.strGLXX_CPMJ = terrainMetaInfo.strGLXX_CPMJ + "|" + rasterizeMetaInfo.strGLXX_CPMJ;
	rasterARDPackageMetaData.strGLXX_CPBB = terrainMetaInfo.strGLXX_CPBB + "|" + rasterizeMetaInfo.strGLXX_CPBB;

	rasterARDPackageMetaData.dBSXX_FBL = terrainMetaInfo.dBSXX_FBL;
    rasterARDPackageMetaData.strBSXX_SJSZFW = terrainMetaInfo.strBSXX_SJSZFW + "|" + rasterizeMetaInfo.strBSXX_SJSZFW;
	rasterARDPackageMetaData.strBSXX_ZBXT = terrainMetaInfo.strBSXX_ZBXT + "|" + rasterizeMetaInfo.strBSXX_ZBXT;
	rasterARDPackageMetaData.strBSXX_GCJZ = terrainMetaInfo.strBSXX_GCJZ + "|" + rasterizeMetaInfo.strBSXX_GCJZ;
	rasterARDPackageMetaData.strBSXX_DTTY = terrainMetaInfo.strBSXX_DTTY + "|" + rasterizeMetaInfo.strBSXX_DTTY;

	// 使用矢量数据的中央经线
	rasterARDPackageMetaData.iBSXX_ZYJX = rasterizeMetaInfo.iBSXX_ZYJX;

	// 使用矢量数据的中央经线
	rasterARDPackageMetaData.dBSXX_BZWX = rasterizeMetaInfo.dBSXX_BZWX;
	rasterARDPackageMetaData.strBSXX_ZBDW = terrainMetaInfo.strBSXX_ZBDW + "|" + rasterizeMetaInfo.strBSXX_ZBDW;

	rasterARDPackageMetaData.strZLXX_WZX = terrainMetaInfo.strZLXX_WZX + "|" + rasterizeMetaInfo.strZLXX_WZX;
	rasterARDPackageMetaData.strZLXX_SJZL = terrainMetaInfo.strZLXX_SJZL + "|" + rasterizeMetaInfo.strZLXX_SJZL;

	rasterARDPackageMetaData.strZTXX_SCDW = terrainMetaInfo.strZTXX_SCDW + "|" + rasterizeMetaInfo.strZTXX_SCDW;
	rasterARDPackageMetaData.strZTXX_GXRQ = terrainMetaInfo.strZTXX_GXRQ + "|" + rasterizeMetaInfo.strZTXX_GXRQ;
	rasterARDPackageMetaData.strZTXX_CPMJ = terrainMetaInfo.strZTXX_CPMJ + "|" + rasterizeMetaInfo.strZTXX_CPMJ;
	rasterARDPackageMetaData.strZTXX_CPBB = terrainMetaInfo.strZTXX_CPBB + "|" + rasterizeMetaInfo.strZTXX_CPBB;
	rasterARDPackageMetaData.strZTXX_GJZ = terrainMetaInfo.strZTXX_GJZ + "|" + rasterizeMetaInfo.strZTXX_GJZ;
	rasterARDPackageMetaData.strZTXX_SJL = terrainMetaInfo.strZTXX_SJL + "|" + rasterizeMetaInfo.strZTXX_SJL;
	rasterARDPackageMetaData.strZTXX_SJGS = "gpkg";
	rasterARDPackageMetaData.strZTXX_SJLY = terrainMetaInfo.strZTXX_SJLY + "|" + rasterizeMetaInfo.strZTXX_SJLY;
	rasterARDPackageMetaData.strZTXX_ZYQH = terrainMetaInfo.strZTXX_ZYQH + "|" + rasterizeMetaInfo.strZTXX_ZYQH;
	rasterARDPackageMetaData.strZTXX_TM = terrainMetaInfo.strZTXX_TM + "|" + rasterizeMetaInfo.strZTXX_TM;
	rasterARDPackageMetaData.strZTXX_TH = terrainMetaInfo.strZTXX_TH + "|" + rasterizeMetaInfo.strZTXX_TH;
	rasterARDPackageMetaData.iZTXX_BLCFM = rasterizeMetaInfo.iZTXX_BLCFM;
	rasterARDPackageMetaData.strZTXX_ZBXT = terrainMetaInfo.strZTXX_ZBXT + "|" + rasterizeMetaInfo.strZTXX_ZBXT;
	rasterARDPackageMetaData.strZTXX_GCJZ = terrainMetaInfo.strZTXX_GCJZ + "|" + rasterizeMetaInfo.strZTXX_GCJZ;
	rasterARDPackageMetaData.strZTXX_DTTY = terrainMetaInfo.strZTXX_DTTY + "|" + rasterizeMetaInfo.strZTXX_DTTY;

	// 等高距从矢量数据元数据中获取
	rasterARDPackageMetaData.iZTXX_DGJ = rasterizeMetaInfo.iZTXX_DGJ;

	rasterARDPackageMetaData.strZTXX_ZQSM = terrainMetaInfo.strZTXX_ZQSM + "|" + rasterizeMetaInfo.strZTXX_ZQSM;

	// 写入栅格分析就绪数据包元数据文件中
	// 元数据结果保存路径
	string strOutputMetaDataPath = ui.lineEdit_OutputPath->text().toLocal8Bit();
	strOutputMetaDataPath += "/raster_gpkg_metadata.json";
	WriteMetaDataToJsonFile(strOutputMetaDataPath, rasterARDPackageMetaData);

#pragma endregion


	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("PACK_TIF_ARD/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_TIF_ARD/InputTerrainPath"), m_qstrInputTerrainPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_TIF_ARD/InputRasterizePath"), m_qstrInputRasterizePath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_TIF_ARD/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_TIF_ARD/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_PackTifARDDlg::pushButton_Cancel_clicked()
{
	reject();
}

void SE_PackTifARDDlg::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void SE_PackTifARDDlg::pushButton_SaveLog_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputLogPath;
	QString dlgTile = tr("请选择日志文件保存路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		m_qstrOutputLogPath = selectedDir;
		ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);
	}

}

void SE_PackTifARDDlg::CalculateTotalProgress()
{
	int totalProgress = 0;
	for (const auto& task : QgsApplication::taskManager()->tasks())
	{
		if (task->status() == QgsTask::Complete)
		{
			totalProgress += 100; // 完成的任务进度加100
		}
		else
		{
			totalProgress += task->progress(); // 当前任务进度
		}
	}
	totalProgress /= QgsApplication::taskManager()->count(); // 计算平均进度


	ui.progressBar->setValue(totalProgress);
}



void SE_PackTifARDDlg::pushButton_Open_clicked()
{
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = tr("请选择栅格分析就绪数据所在目录");
	QString directory = QFileDialog::getExistingDirectory(this,
		dlgTile,
		curPath, // 默认路径
		QFileDialog::ShowDirsOnly);

	if (!directory.isEmpty())
	{
		m_qstrInputDataPath = directory;
		ui.lineEdit_InputDataPath->setText(directory);
	}
	else
	{
		return;
	}
	
}

bool SE_PackTifARDDlg::ReadMetaDataJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo)
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
	metaInfo.strGLXX_CPMC = dsl["GLXX_CPMC"];
	metaInfo.strGLXX_SCDW = dsl["GLXX_SCDW"];
	metaInfo.strGLXX_HJDW = dsl["GLXX_HJDW"];
	metaInfo.strGLXX_SCRQ = dsl["GLXX_SCRQ"];
	metaInfo.strGLXX_GXRQ = dsl["GLXX_GXRQ"];
	metaInfo.strGLXX_CPMJ = dsl["GLXX_CPMJ"];
	metaInfo.strGLXX_CPBB = dsl["GLXX_CPBB"];

	metaInfo.dBSXX_FBL = dsl["BSXX_FBL"];
	metaInfo.strBSXX_SJSZFW = dsl["BSXX_SJSZFW"];
	metaInfo.strBSXX_ZBXT = dsl["BSXX_ZBXT"];
	metaInfo.strBSXX_GCJZ = dsl["BSXX_GCJZ"];
	metaInfo.strBSXX_DTTY = dsl["BSXX_DTTY"];
	metaInfo.iBSXX_ZYJX = dsl["BSXX_ZYJX"];
	metaInfo.dBSXX_BZWX = dsl["BSXX_BZWX"];
	metaInfo.strBSXX_ZBDW = dsl["BSXX_ZBDW"];

	metaInfo.strZLXX_WZX = dsl["ZLXX_WZX"];
	metaInfo.strZLXX_SJZL = dsl["ZLXX_SJZL"];

	metaInfo.strZTXX_CPMC = dsl["ZTXX_CPMC"];
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


void SE_PackTifARDDlg::WriteMetaDataToJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo)
{

	// 创建 JSON 对象
	ordered_json j;

	// 添加 data_source_list
	j["data_source_list"] = ordered_json::array();

	ordered_json data_source;

	// 填充字段
	data_source["GLXX_CPMC"] = metaInfo.strGLXX_CPMC;		// 产品名称	
	data_source["GLXX_SCDW"] = metaInfo.strGLXX_SCDW;		// 生产单位
	data_source["GLXX_HJDW"] = metaInfo.strGLXX_HJDW;		// 汇交单位
	data_source["GLXX_SCRQ"] = metaInfo.strGLXX_SCRQ;		// 生产日期
	data_source["GLXX_GXRQ"] = metaInfo.strGLXX_GXRQ;		// 更新日期
	data_source["GLXX_CPMJ"] = metaInfo.strGLXX_CPMJ;		// 产品密级
	data_source["GLXX_CPBB"] = metaInfo.strGLXX_CPBB;		// 产品版本

	data_source["BSXX_FBL"] = metaInfo.dBSXX_FBL;			// 分辨率
	data_source["BSXX_ZBXT"] = metaInfo.strBSXX_ZBXT;		// 坐标系统
	data_source["BSXX_GCJZ"] = metaInfo.strBSXX_GCJZ;		// 高程基准
	data_source["BSXX_DTTY"] = metaInfo.strBSXX_DTTY;		// 地图投影
	data_source["BSXX_ZYJX"] = metaInfo.iBSXX_ZYJX;			// 中央经线
	data_source["BSXX_BZWX"] = metaInfo.dBSXX_BZWX;			// 标准纬线
	data_source["BSXX_ZBDW"] = metaInfo.strBSXX_ZBDW;		// 坐标单位

	data_source["ZLXX_WZX"] = metaInfo.strZLXX_WZX;			// 完整性
	data_source["ZLXX_SJZL"] = metaInfo.strZLXX_SJZL;		// 数据质量

	data_source["ZTXX_CPMC"] = metaInfo.strZTXX_CPMC;		// 产品名称
	data_source["ZTXX_SCDW"] = metaInfo.strZTXX_SCDW;		// 生产单位
	data_source["ZTXX_HJDW"] = metaInfo.strZTXX_HJDW;		// 汇交单位
	data_source["ZTXX_SCRQ"] = metaInfo.strZTXX_SCRQ;		// 生产日期
	data_source["ZTXX_GXRQ"] = metaInfo.strZTXX_GXRQ;		// 更新日期
	data_source["ZTXX_CPMJ"] = metaInfo.strZTXX_CPMJ;		// 产品密级
	data_source["ZTXX_CPBB"] = metaInfo.strZTXX_CPBB;		// 产品版本
	data_source["ZTXX_GJZ"] = metaInfo.strZTXX_GJZ;			// 关键字
	data_source["ZTXX_SJL"] = metaInfo.strZTXX_SJL;			// 数据量
	data_source["ZTXX_SJGS"] = metaInfo.strZTXX_SJGS;		// 数据格式
	data_source["ZTXX_SJLY"] = metaInfo.strZTXX_SJLY;		// 数据来源
	data_source["ZTXX_ZYQH"] = metaInfo.strZTXX_ZYQH;		// 作业区号
	data_source["ZTXX_TM"] = metaInfo.strZTXX_TM;			// 图名
	data_source["ZTXX_TH"] = metaInfo.strZTXX_TH;			// 图号
	data_source["ZTXX_BLCFM"] = metaInfo.iZTXX_BLCFM;		// 比例尺分母
	data_source["ZTXX_ZBXT"] = metaInfo.strZTXX_ZBXT;		// 坐标系统
	data_source["ZTXX_GCJZ"] = metaInfo.strZTXX_GCJZ;		// 高程基准
	data_source["ZTXX_DTTY"] = metaInfo.strZTXX_DTTY;		// 地图投影
	data_source["ZTXX_DGJ"] = metaInfo.iZTXX_DGJ;			// 等高距
	data_source["ZTXX_ZQSM"] = metaInfo.strZTXX_ZQSM;		// 政区说明

	// 将数据源添加到列表中
	j["data_source_list"].push_back(data_source);


	// 添加 analysis_ready_data
	j["analysis_ready_data"] = ordered_json::array();

	ordered_json analysis;
	analysis["grid_level"] = metaInfo.i_grid_level;
	analysis["data_level"] = metaInfo.i_data_level;
	analysis["data_format"] = "gpkg";
	analysis["data_type"] = "raster";
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



/*获取当前目录下所有子目录*/
void SE_PackTifARDDlg::GetSubDirFromFilePath(const string& strFilePath, vector<string>& vSubDir)
{
	// 获取当前工作目录
	std::filesystem::path currentPath = strFilePath;
	std::cout << "Current Path: " << currentPath << std::endl;

	// 遍历当前目录下的所有条目
	for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
		// 检查条目是否是目录
		if (std::filesystem::is_directory(entry.status())) {
			vSubDir.push_back(entry.path().filename().string());
			//std::cout << "Subdirectory: " << entry.path().filename() << std::endl;
		}
	}
}


#pragma region  "检查文件或文件夹是否存在"

bool SE_PackTifARDDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion