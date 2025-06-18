#define _HAS_STD_BYTE 0
// 使用json头文件
#include <nlohmann/json.hpp>

#include "se_pack_shp_ARD_multithread.h"
#include "commontype/se_analysis_ready_data_tool_def.h"
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

SE_PackShpARDMultiThreadDlg::SE_PackShpARDMultiThreadDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.Button_OK , &QPushButton::clicked, this, &SE_PackShpARDMultiThreadDlg::pushButton_OK_clicked);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &SE_PackShpARDMultiThreadDlg::pushButton_Cancel_clicked);
	
	connect(ui.pushButton_Open, &QPushButton::clicked, this, &SE_PackShpARDMultiThreadDlg::pushButton_Open_clicked);
	connect(ui.pushButton_OpenMetaDataFile, &QPushButton::clicked, this, &SE_PackShpARDMultiThreadDlg::pushButton_OpenMetaData_clicked);
	connect(ui.pushButton_Save, &QPushButton::clicked, this, &SE_PackShpARDMultiThreadDlg::pushButton_Save_clicked);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &SE_PackShpARDMultiThreadDlg::pushButton_SaveLog_clicked);

	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputPath->setText(m_qstrOutputDataPath);
	ui.lineEdit_InputMetaDataPath->setText(m_qstrInputMetaDataPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);
}

SE_PackShpARDMultiThreadDlg::~SE_PackShpARDMultiThreadDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("PACK_SHP_ARD/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_SHP_ARD/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_SHP_ARD/InputMetaDataPath"), m_qstrInputMetaDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_SHP_ARD/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);
}

void SE_PackShpARDMultiThreadDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("PACK_SHP_ARD/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("PACK_SHP_ARD/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrInputMetaDataPath = settings.value(QStringLiteral("PACK_SHP_ARD/InputMetaDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputLogPath = settings.value(QStringLiteral("PACK_SHP_ARD/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}



void SE_PackShpARDMultiThreadDlg::pushButton_OpenMetaData_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择矢量分析就绪型数据元数据文件");
	QString filter = tr("json 文件(*.json)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	m_qstrInputMetaDataPath = strFileName;
	ui.lineEdit_InputMetaDataPath->setText(strFileName);
}

void SE_PackShpARDMultiThreadDlg::pushButton_Save_clicked()
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

void SE_PackShpARDMultiThreadDlg::pushButton_OK_clicked()
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

	// 如果文件不存在
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



	ui.progressBar->reset();

	// 输入数据路径
	string strInputPath = ui.lineEdit_InputDataPath->text().toLocal8Bit();

	// 输入元数据路径
	string strInputMetaDataPath = ui.lineEdit_InputMetaDataPath->text().toLocal8Bit();

	// 分析结果保存路径
	string strOutputPath = ui.lineEdit_OutputPath->text().toLocal8Bit();
#pragma region "将封装shp改为并行多任务"

	/*修改时间：2025-05-09
	  修改内容：将封装shp改为并行多任务*/
	// 遍历所有shp文件，计算所属的分包文件名称，按照分包文件进行多任务并行
	vector<Packet_Name_ARD_FileNames_Pairs> vPacketNameFileNamesPairs;
	vPacketNameFileNamesPairs.clear();

	// 获取当前目录下所有的文件夹，即矢量分析就绪数据产品类型
	vector<string> vSubDir;
	vSubDir.clear();
	GetSubDirFromFilePath(strInputPath, vSubDir);

	// 根据空间范围计算gpkg包的ZXY范围

	// 分包规则，通过界面设置
	/*序号	包内基础格网级别	基础分包级别	    数据包名称示例
		1	7级~10级	            4级	            7-10-4-0-0.gpkg
		2	11级~16级	            7级	            11-16-8-0-0.gpkg*/

	vector<ARD_PACKAGE_INFO> vArdPackageInfo;
	vArdPackageInfo.clear();

	ARD_PACKAGE_INFO info;
	info.iMinZ = 7;
	info.iMaxZ = 10;
	info.iBaseZ = 4;
	vArdPackageInfo.push_back(info);

	info.iMinZ = 11;
	info.iMaxZ = 16;
	info.iBaseZ = 8;
	vArdPackageInfo.push_back(info);

	// 循环每个子目录
	for (int i = 0; i < vSubDir.size(); i++)
	{
		std::string strTempPath = strInputPath;
		strTempPath += "/";
		strTempPath += vSubDir[i];

		// 获取所有shp文件路径
		vector<string> vShpFiles;
		vShpFiles.clear();

		GetShpFilesFromFilePath(strTempPath, vShpFiles);
	
		for (int j = 0; j < vShpFiles.size(); ++j)
		{
			// 数据包与数据文件关联信息结构体
			Packet_Name_ARD_FileNames_Pairs pair;


			int Z, X, Y;
			if (extractZXY(vShpFiles[j], Z, X, Y))
			{
				// 计算ZXY的空间范围
				SE_DRect dTempRect = CalGridExtentByZXY(Z, X, Y);

				// 计算中心点
				SE_DPoint dTempPoint;
				dTempPoint.dx = (dTempRect.dleft + dTempRect.dright) * 0.5;
				dTempPoint.dy = (dTempRect.dbottom + dTempRect.dtop) * 0.5;

				// 计算在分包级别的xy
				int	iPackX = 0;
				int iPackY = 0;

				// 数据包最小Z和最大Z
				int iMinZ = 0;
				int iMaxZ = 0;

				// 根据Z计算所在分包级别
				int iPackLevel = 0;
				GetPackLevelByZ(vArdPackageInfo, Z, iPackLevel, iMinZ, iMaxZ);
				CalXAndYByPointAndLevel(
					dTempPoint,
					iPackLevel,
					iPackX,
					iPackY);

				// 分包名称
				char szPacketName[200] = { 0 };
				sprintf(szPacketName, "%d-%d-%d-%d-%d", 
					iMinZ,
					iMaxZ,
					iPackLevel,
					iPackX,
					iPackY);
				pair.strPacketName = szPacketName;

				int iIndex = -1;
				// 判断strPacketName是否在vPacketNameFileNamesPairs已经存在
				if (IsExistedInVector(vPacketNameFileNamesPairs, pair.strPacketName, iIndex))
				{
					vPacketNameFileNamesPairs[iIndex].vARDFilesPath.push_back(vShpFiles[j]);
				}
				else
				{
					pair.vARDFilesPath.push_back(vShpFiles[j]);
                    vPacketNameFileNamesPairs.push_back(pair);
				}

			}
			else
			{				
				continue;
			}
		}

	}


	// 每个gpkg数据包创建一个线程进行数据入库
	for (int i = 0; i < vPacketNameFileNamesPairs.size(); ++i)
	{
		// 创建任务
		SE_PackShpARDTask2* task = new SE_PackShpARDTask2(
			tr("通用矢量分析就绪型数据包:%1封装").arg(vPacketNameFileNamesPairs[i].strPacketName.c_str()),
			vPacketNameFileNamesPairs[i],
			strOutputPath,
			iLogLevel,
			strOutputLogPath);
		QgsApplication::taskManager()->addTask(task);

		connect(task, &SE_PackShpARDTask2::taskFinished, this, &SE_PackShpARDMultiThreadDlg::onTaskFinished);




	}

#pragma endregion










	/*
	// 创建任务
	SE_PackShpARDTask* task = new SE_PackShpARDTask(
		tr("通用矢量分析就绪型数据封装"),
		strInputPath,
		strOutputPath,
		iLogLevel,
		strOutputLogPath);
	QgsApplication::taskManager()->addTask(task);

	connect(task, &SE_PackShpARDTask::taskFinished, this, &SE_PackShpARDMultiThreadDlg::onTaskFinished);
	*/
#pragma region "生成矢量分析就绪数据包元数据"

	// 矢量分析就绪数据元数据
	VectorDataMetadataInfo vectorMetaInfo;
	if (!ReadMetaDataJsonFile(strInputMetaDataPath, vectorMetaInfo))
	{
		// 记录打开元数据失败日志
	}


	// 矢量分析就绪型数据包元数据
	// 数据格式调整
	vectorMetaInfo.strBSXX_SJGS = "gpkg";
	vectorMetaInfo.strZTXX_SJGS = "gpkg";

	// 产品名称赋值为分析就绪数据产品类型，用"|"分割
	vectorMetaInfo.strGLXX_CPMC = "";
	vectorMetaInfo.strZTXX_CPMC = "";
	for (int i = 0; i < vSubDir.size(); ++i)
	{
		vectorMetaInfo.strGLXX_CPMC += vSubDir[i];
		vectorMetaInfo.strZTXX_CPMC += vSubDir[i];
		// 除最后一个外，均加"|"分割
		if (i < vSubDir.size() - 1)
		{		
			vectorMetaInfo.strGLXX_CPMC += "|";
			vectorMetaInfo.strZTXX_CPMC += "|";
		}		
	}
	
	// 更新生产时间
	QDateTime curDateTime = QDateTime::currentDateTime();
	QString qstrDataTime = curDateTime.toString("yyyy-MM-dd hh:mm:ss");
	string strDataTime = qstrDataTime.toLocal8Bit();

	vectorMetaInfo.strGLXX_SCRQ = strDataTime;
	vectorMetaInfo.strZTXX_SCRQ = strDataTime;

	// 元数据结果保存路径
	string strOutputMetaDataPath = ui.lineEdit_OutputPath->text().toLocal8Bit();
	strOutputMetaDataPath += "/vector_gpkg_metadata.json";
	WriteMetaDataToJsonFile(strOutputMetaDataPath, vectorMetaInfo);

#pragma endregion

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("PACK_SHP_ARD/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_SHP_ARD/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_SHP_ARD/InputMetaDataPath"), m_qstrInputMetaDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_SHP_ARD/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_PackShpARDMultiThreadDlg::pushButton_Cancel_clicked()
{
	reject();
}

void SE_PackShpARDMultiThreadDlg::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void SE_PackShpARDMultiThreadDlg::pushButton_SaveLog_clicked()
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

void SE_PackShpARDMultiThreadDlg::CalculateTotalProgress()
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



void SE_PackShpARDMultiThreadDlg::pushButton_Open_clicked()
{
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = tr("请选择矢量分析就绪数据所在目录");
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


bool SE_PackShpARDMultiThreadDlg::ReadMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo)
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


void SE_PackShpARDMultiThreadDlg::WriteMetaDataToJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo)
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



/*获取当前目录下所有子目录*/
void SE_PackShpARDMultiThreadDlg::GetSubDirFromFilePath(const string& strFilePath,
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
			//std::cout << "Subdirectory: " << entry.path().filename() << std::endl;
		}
	}
}


#pragma region  "检查文件或文件夹是否存在"

bool SE_PackShpARDMultiThreadDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion



// 获取当前路径下所有shp文件路径
void SE_PackShpARDMultiThreadDlg::GetShpFilesFromFilePath(
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
bool SE_PackShpARDMultiThreadDlg::extractZXY(
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


/*根据级别、行、列索引计算格网的经纬度范围*/
SE_DRect SE_PackShpARDMultiThreadDlg::CalGridExtentByZXY(int iLevel, int iX, int iY)
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

/*根据JK级别获取分辨率（地理坐标系，单位：度）*/
double SE_PackShpARDMultiThreadDlg::GetGridResByGridLevel(int iLevel)
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


/*根据经纬度范围、打包级别计算横向X取值范围、纵向Y取值范围*/
void SE_PackShpARDMultiThreadDlg::CalXRangeAndYRangeByGeoRectAndLevel(
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


/*根据Z计算分包级别*/
void SE_PackShpARDMultiThreadDlg::GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel,int& iMinZ, int& iMaxZ)
{
	for (int i = 0; i < vInfo.size(); ++i)
	{
		if (iZ >= vInfo[i].iMinZ && iZ <= vInfo[i].iMaxZ)
		{
			iPackLevel = vInfo[i].iBaseZ;
            iMinZ = vInfo[i].iMinZ;
            iMaxZ = vInfo[i].iMaxZ;
			break;
		}
	}
}


/*根据经纬度、打包级别计算行列索引*/
void SE_PackShpARDMultiThreadDlg::CalXAndYByPointAndLevel(
	SE_DPoint dPoint,
	int iPackageLevel,
	int& iX,
	int& iY)
{
	// 根据格网获取对应的分辨率
	double dGridRes = GetGridResByGridLevel(iPackageLevel);

	// 计算X
	iX = int(fabs(dPoint.dx + 256) / dGridRes);

	// 计算Y
	iY = int(fabs(256 - dPoint.dy) / dGridRes);

}


// 判断vector中是否存在当前元素
bool SE_PackShpARDMultiThreadDlg::IsExistedInVector(
	vector<Packet_Name_ARD_FileNames_Pairs> &vPairs, 
	string strValue,
	int &iIndex)
{
	for (int i =  0; i < vPairs.size(); ++i)
	{
		if (vPairs[i].strPacketName == strValue)
		{
			iIndex = i;
			return true;
		}
	}

	iIndex = -1;
	return false;
}
