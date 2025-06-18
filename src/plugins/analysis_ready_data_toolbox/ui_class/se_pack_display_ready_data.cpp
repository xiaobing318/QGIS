#define _HAS_STD_BYTE 0

// 使用json头文件
#include <nlohmann/json.hpp>
#include "se_pack_display_ready_data.h"
#include "commontype/se_commondef.h"
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

#include <QDateTime>
/*内部*/
#include "../ui_task/se_pack_display_ready_data_task.h"

using ordered_json = nlohmann::ordered_json;


using namespace std;

SE_PackDisplayReadyDataDlg::SE_PackDisplayReadyDataDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.Button_OK , &QPushButton::clicked, this, &SE_PackDisplayReadyDataDlg::pushButton_OK_clicked);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &SE_PackDisplayReadyDataDlg::pushButton_Cancel_clicked);

	connect(ui.pushButton_SavePackage, &QPushButton::clicked, this, &SE_PackDisplayReadyDataDlg::pushButton_Save_clicked);
	connect(ui.pushButton_Task_Model_Data_Path, &QPushButton::clicked, this, &SE_PackDisplayReadyDataDlg::pushButton_OpenConfigFile_clicked);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &SE_PackDisplayReadyDataDlg::pushButton_SaveLog_clicked);
	connect(ui.pushButton_AddDataPath, &QPushButton::clicked, this, &SE_PackDisplayReadyDataDlg::pushButton_AddDisplayReadyDataPath);
	connect(ui.pushButton_ClearDataPath, &QPushButton::clicked, this, &SE_PackDisplayReadyDataDlg::pushButton_ClearDisplayReadyDataPath);


	restoreState();

	// 纬度                                              
	ui.lineEdit_Top->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));
	ui.lineEdit_Bottom->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));

	// 经度
	ui.lineEdit_Left->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));
	ui.lineEdit_Right->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));

	ui.lineEdit_OutputPackagePath->setText(m_qstrOutputDataPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);
	ui.lineEdit_Task_Model_Data_Path->setText(m_qstrConfigFilePath);
}

SE_PackDisplayReadyDataDlg::~SE_PackDisplayReadyDataDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("PACK_DISPLAY_READY_DATA/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_DISPLAY_READY_DATA/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_DISPLAY_READY_DATA/ConfigFilePath"), m_qstrConfigFilePath, QgsSettings::Section::Plugins);

}

void SE_PackDisplayReadyDataDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrOutputDataPath = settings.value(QStringLiteral("PACK_DISPLAY_READY_DATA/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputLogPath = settings.value(QStringLiteral("PACK_DISPLAY_READY_DATA/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrConfigFilePath = settings.value(QStringLiteral("PACK_DISPLAY_READY_DATA/ConfigFilePath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}



void SE_PackDisplayReadyDataDlg::pushButton_OpenConfigFile_clicked()
{
	// 选择xml属性配置文件
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("请选择任务-模型-数据映射规则配置文件");
	QString filter = tr("xml 文件(*.xml)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}
	else
	{
		ui.lineEdit_Task_Model_Data_Path->setText(strFileName);
		m_qstrConfigFilePath = strFileName;
		if (!LoadXmlConfigFile(strFileName.toLocal8Bit(), m_vTaskInfos))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("打开任务-模型-数据映射规则配置文件失败！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		if (m_vTaskInfos.size() == 0)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("配置文件中没有任务信息，请重新选择配置文件！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}


		// 将任务信息添加到任务列表框中
		for (const auto& taskInfo : m_vTaskInfos)
		{
			ui.comboBox_Task->addItem(taskInfo.strTaskName);
		}
	}
}

void SE_PackDisplayReadyDataDlg::pushButton_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputDataPath;
	QString dlgTile = tr("请选择显示就绪型数据包保存路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		m_qstrOutputDataPath = selectedDir;
		ui.lineEdit_OutputPackagePath->setText(m_qstrOutputDataPath);
	}
}

void SE_PackDisplayReadyDataDlg::pushButton_AddDisplayReadyDataPath()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择显示就绪型数据文件所在路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		// 判断是否已经存在同名的路径
		for (int iRow = 0; iRow < ui.tableWidget_DisplayReadyDataPath->rowCount(); ++iRow)
		{
			QString qstrPath = ui.tableWidget_DisplayReadyDataPath->item(iRow, 0)->text();

			// 如果存在同名的
			if (selectedDir == qstrPath)
			{
				QString qstrTitle = tr("数据包定制及标准化封装工具");
				QString qstrText = tr("当前路径:%1在路径列表中已存在！").arg(selectedDir);
				QMessageBox msgBox;
				msgBox.setWindowTitle(qstrTitle);
				msgBox.setText(qstrText);
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				ui.Button_OK->setEnabled(true);
				return;
			}
		}


		// 添加到ui.tableWidget_DisplayReadyDataPath中
		int iRow = ui.tableWidget_DisplayReadyDataPath->rowCount();
		QTableWidgetItem* itemDir = new QTableWidgetItem(selectedDir);
		ui.tableWidget_DisplayReadyDataPath->insertRow(iRow);
		// 插入路径
		ui.tableWidget_DisplayReadyDataPath->setItem(iRow, 0, itemDir);

		// 是否生成为复选框，默认为选中状态
		QTableWidgetItem* itemCheck = new QTableWidgetItem();
		itemCheck->setFlags(itemCheck->flags() | Qt::ItemIsUserCheckable);		// 设置为可复选
		itemCheck->setCheckState(Qt::Checked);								// 初始状态为选中

		// 设置复选框居中
		itemCheck->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_DisplayReadyDataPath->setItem(iRow, 1, itemCheck);

		// 该方法会自动调整表格的列宽，使其能够容纳列中的内容
		ui.tableWidget_DisplayReadyDataPath->resizeColumnsToContents();
	}
}

void SE_PackDisplayReadyDataDlg::pushButton_ClearDisplayReadyDataPath()
{
	// 删除所有行
	while (ui.tableWidget_DisplayReadyDataPath->rowCount() > 0) {
		ui.tableWidget_DisplayReadyDataPath->removeRow(0);
	}
}


void SE_PackDisplayReadyDataDlg::pushButton_OK_clicked()
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


#pragma region "判断经纬度是否在有效值范围内"

	// 判断经纬度是否在有效值范围内
	if (fabs(ui.lineEdit_Left->text().toDouble()) > 180.0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("左边界经度：%1超出[-180°,180°]范围，请重新输入！").arg(ui.lineEdit_Left->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 判断经纬度是否在有效值范围内
	if (fabs(ui.lineEdit_Right->text().toDouble()) > 180.0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("右边界经度：%1超出[-180°,180°]范围，请重新输入！").arg(ui.lineEdit_Right->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 判断经纬度是否在有效值范围内
	if (fabs(ui.lineEdit_Top->text().toDouble()) > 90.0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("上边界纬度：%1超出[-90°,90°]范围，请重新输入！").arg(ui.lineEdit_Top->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 判断经纬度是否在有效值范围内
	if (fabs(ui.lineEdit_Bottom->text().toDouble()) > 90.0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("下边界纬度：%1超出[-90°,90°]范围，请重新输入！").arg(ui.lineEdit_Bottom->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 左边界必须小于右边界
	if (ui.lineEdit_Left->text().toDouble() >= ui.lineEdit_Right->text().toDouble())
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("左边界经度：%1须小于右边界经度：%2").arg(ui.lineEdit_Left->text().toDouble()).arg(ui.lineEdit_Right->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 下边界必须小于上边界
	if (ui.lineEdit_Bottom->text().toDouble() >= ui.lineEdit_Top->text().toDouble())
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("下边界纬度：%1须小于上边界纬度：%2").arg(ui.lineEdit_Bottom->text().toDouble()).arg(ui.lineEdit_Top->text().toDouble()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

#pragma endregion

	// 如果文件夹不存在
	if (!CheckFileOrDirExist(ui.lineEdit_OutputPackagePath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_OutputPackagePath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 如果文件不存在
	if (!CheckFileOrDirExist(ui.lineEdit_Task_Model_Data_Path->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_Task_Model_Data_Path->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}



	ui.progressBar->reset();

#pragma region "获取选择的数据类型"

	// ☑的数据类型列表
	vector<string> vDataTypes;
	vDataTypes.clear();
	GetDataTypeCheckStatus(vDataTypes);

	if (vDataTypes.size() == 0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("请至少选择一种数据类型！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


#pragma endregion

#pragma region "获取选择的要素类型"

	// ☑的要素类型列表
	vector<string> vFeatureTypes;
	vFeatureTypes.clear();
	GetFeatureTypeCheckStatus(vFeatureTypes);

	// 如果要素类型不选择，默认全部选取矢量要素图层


#pragma endregion


	// 结果保存路径
	string strOutputPath = ui.lineEdit_OutputPackagePath->text().toLocal8Bit();

	// 获取当前所选任务下的所有显示就绪数据类型
	int iTaskIndex = ui.comboBox_Task->currentIndex();

	THEME_TaskInfo taskInfo = m_vTaskInfos[iTaskIndex];

	vector<string> vDisplayReadyDataIdentifys;
	vDisplayReadyDataIdentifys.clear();

	// 遍历每个模型
	for (const auto& model : taskInfo.vModelInfos)
	{
		// 遍历每个数据
		for (const auto& ard : model.vARDInfos)
		{
			string strARDIdentify = ard.strARDIdentify.toLocal8Bit();
			if (!IsExistedInVector(vDisplayReadyDataIdentifys, strARDIdentify))
			{
				// 显示就绪数据标识集合
				vDisplayReadyDataIdentifys.push_back(strARDIdentify);
			}
		}
	}

	// 空间范围
	SE_DRect dInputRect;
	dInputRect.dleft = ui.lineEdit_Left->text().toDouble();
	dInputRect.dtop = ui.lineEdit_Top->text().toDouble();
	dInputRect.dright = ui.lineEdit_Right->text().toDouble();
	dInputRect.dbottom = ui.lineEdit_Bottom->text().toDouble();

	// 输入显示就绪型数据所在路径列表
	vector<string> vInputDisplayReadyDataPath;
	vInputDisplayReadyDataPath.clear();

	// 从原始数据列表中获取路径
	int iRowCount = ui.tableWidget_DisplayReadyDataPath->rowCount();
	for (int i = 0; i < iRowCount; i++)
	{
		string strTempPath = ui.tableWidget_DisplayReadyDataPath->item(i, 0)->text().toLocal8Bit();

		// 如果是☑状态，则添加到vInputDisplayReadyDataPath中
		if (ui.tableWidget_DisplayReadyDataPath->item(i, 1)->checkState() == Qt::Checked)
		{
			vInputDisplayReadyDataPath.push_back(strTempPath);
		}
	}

    // 如果没有选择显示就绪数据路径，则提示用户
	if (vInputDisplayReadyDataPath.size() == 0)
	{
		QString qstrTitle = tr("数据包定制及标准化封装工具");
		QString qstrText = tr("请添加显示就绪型数据路径并设置为选中状态！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();

		return;
	}


	/* 封装条件包括：
	1）任务和模型需求的数据类型
    2）数据类型（交互设置☑的类型）
    3）要素类型（交互设置☑的要素）
    4）空间范围
	*/
	// 遍历每个显示就绪型数据路径，根据条件进行封装
	for (const auto& strInputPath : vInputDisplayReadyDataPath)
	{
		// 创建任务
		SE_PackDisplayReadyDataTask* task = new SE_PackDisplayReadyDataTask(
			tr("基础通用显示就绪型数据封装"),
			strInputPath,
			vDisplayReadyDataIdentifys,
			vDataTypes,
			vFeatureTypes,
			dInputRect,
			strOutputPath,
			iLogLevel,
			strOutputLogPath);


		QgsApplication::taskManager()->addTask(task);

		connect(task, &SE_PackDisplayReadyDataTask::taskFinished, this, &SE_PackDisplayReadyDataDlg::onTaskFinished);






	}



#pragma region "生成显示就绪型数据包元数据"

	/*
	1）读取输入数据路径下的所有json元数据文件；
	2）构建显示就绪型数据包元数据项并输出json文件，

	*/
	/*
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
	rasterARDPackageMetaData.strGLXX_CPMC = "";
	rasterARDPackageMetaData.strZTXX_CPMC = "";
	for (int i = 0; i < vARDIdentifys.size(); ++i)
	{
		// 如果标识前两位为01开头的，才是栅格分析就绪数据
		string strARDType = vARDIdentifys[i].substr(0, 2);
		if (strARDType != "01")
		{
			continue;
		}

		rasterARDPackageMetaData.strGLXX_CPMC += vARDIdentifys[i];
		rasterARDPackageMetaData.strZTXX_CPMC += vARDIdentifys[i];
		// 除最后一个外，均加"|"分割
		if (i < vARDIdentifys.size() - 1)
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
	string strOutputMetaDataPath = ui.lineEdit_OutputPackagePath->text().toLocal8Bit();
	strOutputMetaDataPath += "/raster_gpkg_metadata.json";
	WriteMetaDataToJsonFile(strOutputMetaDataPath, rasterARDPackageMetaData);

#pragma endregion
*/


	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("PACK_DISPLAY_READY_DATA/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_DISPLAY_READY_DATA/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_DISPLAY_READY_DATA/ConfigFilePath"), m_qstrConfigFilePath, QgsSettings::Section::Plugins);

}

void SE_PackDisplayReadyDataDlg::pushButton_Cancel_clicked()
{
	reject();
}

void SE_PackDisplayReadyDataDlg::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void SE_PackDisplayReadyDataDlg::pushButton_SaveLog_clicked()
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

void SE_PackDisplayReadyDataDlg::CalculateTotalProgress()
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





// 读取属性检查配置信息
bool SE_PackDisplayReadyDataDlg::LoadXmlConfigFile(const char* szConfigXmlFile, vector<THEME_TaskInfo>& vTaskInfos)
{
	// 如果xml文件为空
	if (!szConfigXmlFile)
	{
		return false;
	}

	vTaskInfos.clear();

	// 读取xml文件
	xmlDocPtr doc = nullptr;
	xmlNodePtr pRootNode = nullptr;

	// 配置文件
	doc = xmlParseFile(szConfigXmlFile);

	if (nullptr == doc)
	{
		return false;
	}

	// 获取根节点<task_model_data_list>
	pRootNode = xmlDocGetRootElement(doc);

	if (NULL == pRootNode) {
		xmlFreeDoc(doc);
		return false;
	}

	// 遍历所有根节点的子节点
	xmlNodePtr cur;

	//遍历处理根节点的每一个子节点
	cur = pRootNode->xmlChildrenNode;
	xmlChar* key;
	while (cur != NULL)
	{

		// 任务task节点
		if (!xmlStrcmp(cur->name, (const xmlChar*)"task"))
		{
			THEME_TaskInfo info;
			Parse_task(doc, cur, info);
			vTaskInfos.push_back(info);
		}

		cur = cur->next;
	}

	xmlFreeDoc(doc);

	return true;
}

// 解析每个task节点
void SE_PackDisplayReadyDataDlg::Parse_task(
	xmlDocPtr doc,
	xmlNodePtr cur,
	THEME_TaskInfo& info)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != NULL)
	{
		// task_identify：任务标识
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"task_identify")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strTaskIdentify = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		// task_name：任务名称
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"task_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strTaskName = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		// model：模型列表
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"model")))
		{
			THEME_ModelInfo model_info;
			Parse_model(doc, cur, model_info);
			info.vModelInfos.push_back(model_info);
		}
		cur = cur->next;
	}
}

// 解析每个model节点
void SE_PackDisplayReadyDataDlg::Parse_model(
	xmlDocPtr doc,
	xmlNodePtr cur,
	THEME_ModelInfo& info)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// model_identify：模型标识
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"model_identify")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strModelIdentify = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		// model_name：模型名称
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"model_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strModelName = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		// data：数据
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"data")))
		{
			THEME_ARDInfo ard_info;
			Parse_data(doc, cur, ard_info);
			info.vARDInfos.push_back(ard_info);
		}



		cur = cur->next;
	}
}


// 解析每个data节点
void SE_PackDisplayReadyDataDlg::Parse_data(
	xmlDocPtr doc,
	xmlNodePtr cur,
	THEME_ARDInfo& info)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// data_identify：分析就绪型数据标识
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"data_identify")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strARDIdentify = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		// data_name：分析就绪型数据名称
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"data_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strARDName = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		cur = cur->next;
	}
}


// 判断vector中是否存在当前元素
bool SE_PackDisplayReadyDataDlg::IsExistedInVector(vector<string>& vValues, string strValue)
{
	for (const auto& value : vValues)
	{
		if (value == strValue)
		{
			return true;
		}
	}

	return false;
}



/*获取当前目录下所有子目录*/
void SE_PackDisplayReadyDataDlg::GetSubDirFromFilePath(const string& strFilePath,	vector<string>& vSubDir)
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


bool SE_PackDisplayReadyDataDlg::ReadMetaDataJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo)
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


void SE_PackDisplayReadyDataDlg::WriteMetaDataToJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo)
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


#pragma region  "检查文件或文件夹是否存在"

bool SE_PackDisplayReadyDataDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion

void SE_PackDisplayReadyDataDlg::GetDataTypeCheckStatus(vector<string>& vDataType)
{
	vDataType.clear();
	// 矢量
	if (ui.checkBox_DataType_ShiLiang->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_SHILIANG);
	}

	// 影像
	if (ui.checkBox_DataType_YingXiang->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_YINGXIANG);
	}

    // 高程
	if (ui.checkBox_DataType_GaoCheng->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_GAOCHENG);
	}

	// 地名
	if (ui.checkBox_DataType_DiMing->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_DIMING);
	}

	// 实景
	if (ui.checkBox_DataType_ShiJing->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_SHIJING);
	}

	// 地质
	if (ui.checkBox_DataType_DiZhi->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_DIZHI);
	}

	// 航空图
	if (ui.checkBox_DataType_HangKongTu->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_HANGKONGTU);
	}

	// 倾斜摄影
	if (ui.checkBox_DataType_QingXieSheYing->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_QINGXIESHEYING);
	}

	// 专用标绘
	if (ui.checkBox_DataType_BiaoHui->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_BIAOHUI);
	}

	// 地下管网
	if (ui.checkBox_DataType_DiXiaGuanWang->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_DIXIAGUANWANG);
	}

	// 地面气象
	if (ui.checkBox_DataType_DiMianQiXiang->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_DIMIANQIXIANG);
	}

	// 高空气象
	if (ui.checkBox_DataType_GaoKongQiXiang->isChecked())
	{
		vDataType.push_back(SE_DATA_TYPE_GAOKONGQIXIANG);
	}
}

void SE_PackDisplayReadyDataDlg::GetFeatureTypeCheckStatus(vector<string>& vFeatureType)
{
    vFeatureType.clear();

	// 工农业社会文化设施
	if (ui.checkBox_FeatureType_GongNongYe->isChecked())
	{
		vFeatureType.push_back(SE_FEATURE_TYPE_GONGNONGYE);
	}

	// 居民地
	if (ui.checkBox_FeatureType_JuMingDi->isChecked())
	{
		vFeatureType.push_back(SE_FEATURE_TYPE_JUMINGDI);
	}

	// 陆地交通
	if (ui.checkBox_FeatureType_LuDiJiaoTong->isChecked())
	{
		vFeatureType.push_back(SE_FEATURE_TYPE_LUDIJIAOTONG);
	}

	// 管线
	if (ui.checkBox_FeatureType_GuanXian->isChecked())
	{
		vFeatureType.push_back(SE_FEATURE_TYPE_GUANXIAN);
	}

	// 植被
	if (ui.checkBox_FeatureType_ZhiBei->isChecked())
	{
		vFeatureType.push_back(SE_FEATURE_TYPE_ZHIBEI);
	}

	// 水域陆地
	if (ui.checkBox_FeatureType_ShuiYuLuDi->isChecked())
	{
		vFeatureType.push_back(SE_FEATURE_TYPE_SHUIYULUDI);
	}

	// 陆地地貌及土质
	if (ui.checkBox_FeatureType_LuDiDiMao->isChecked())
	{
		vFeatureType.push_back(SE_FEATURE_TYPE_LUDIDIMIAO);
	}
}