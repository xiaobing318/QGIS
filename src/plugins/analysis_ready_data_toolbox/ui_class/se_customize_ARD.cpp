#define _HAS_STD_BYTE 0
#include "se_customize_ARD.h"
// 使用json头文件
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "se_customize_ARD.h"
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
#include <direct.h>
#else
#include "unistd.h"
#include <sys/stat.h>
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#endif //
#define _atoi64(val)   strtoll(val,NULL,10)


#include "zchj/cse_zchj_data_process.h"
#include "../ui_task/se_customize_terrain_factor_task.h"
#include "../ui_task/se_customize_rasterize_task.h"
#include "../ui_task/se_customize_vector_task.h"

using namespace std;
using ordered_json = nlohmann::ordered_json;






SE_CustomizeARDDlg::SE_CustomizeARDDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	// 默认地形因子
	ui.radioButton_terrain_factor->setChecked(true);

	connect(ui.Button_OK, &QPushButton::clicked, this, &SE_CustomizeARDDlg::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &SE_CustomizeARDDlg::Button_Cancel_rejected);
	connect(ui.radioButton_terrain_factor, &QRadioButton::clicked, this, &SE_CustomizeARDDlg::radioButton_terrain_factor_clicked);
	connect(ui.radioButton_rasterize, &QRadioButton::clicked, this, &SE_CustomizeARDDlg::radioButton_rasterize_clicked);

	// 栅格分析就绪数据
	connect(ui.pushButton_RasterARD_ConfigFilePath, &QPushButton::clicked, this, &SE_CustomizeARDDlg::pushButton_OpenRasterARD_ConfigFilePath_clicked);
	connect(ui.pushButton_RasterARD_InputDataPath, &QPushButton::clicked, this, &SE_CustomizeARDDlg::pushButton_OpenRasterARD_InputDataPath_clicked);
	connect(ui.pushButton_RasterARD_SavePath, &QPushButton::clicked, this, &SE_CustomizeARDDlg::pushButton_RasterARD_SavePath_clicked);

	// 矢量分析就绪数据
	connect(ui.pushButton_VectorARD_ConfigFilePath, &QPushButton::clicked, this, &SE_CustomizeARDDlg::pushButton_OpenVectorARD_ConfigFilePath_clicked);
	connect(ui.pushButton_VectorARD_InputDataPath, &QPushButton::clicked, this, &SE_CustomizeARDDlg::pushButton_OpenVectorARD_InputDataPath_clicked);
	connect(ui.pushButton_VectorARD_SavePath, &QPushButton::clicked, this, &SE_CustomizeARDDlg::pushButton_VectorARD_SavePath_clicked);

	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &SE_CustomizeARDDlg::pushButton_SaveLog_clicked);

	connect(ui.pushButton_RasterARD_InputMetadataPath, &QPushButton::clicked, this, &SE_CustomizeARDDlg::pushButton_RasterARD_InputMetadataPath_clicked);
	connect(ui.pushButton_VectorARD_InputMetadataPath, &QPushButton::clicked, this, &SE_CustomizeARDDlg::pushButton_VectorARD_InputMetadataPath_clicked);

	restoreState();

	// 设置输入限制

	ui.lineEdit_RasterARD_ConfigFilePath->setText(m_qstrRasterARDConfigPath);
	ui.lineEdit_RasterARD_InputDataPath->setText(m_qstrRasterARD_InputDataPath);
	ui.lineEdit_RasterARD_SavePath->setText(m_qstrRasterARDOutputDataPath);

	ui.lineEdit_VectorARD_ConfigFilePath->setText(m_qstrVectorARDConfigPath);
	ui.lineEdit_VectorARD_InputDataPath->setText(m_qstrVectorARD_InputDataPath);
	ui.lineEdit_VectorARD_SavePath->setText(m_qstrVectorARDOutputDataPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);

	// 表头居中
	ui.tableWidget_RasterARD->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中
	// 颜色交叉
	ui.tableWidget_RasterARD->setAlternatingRowColors(true);

	ui.tableWidget_VectorARD->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中
	ui.tableWidget_VectorARD->setAlternatingRowColors(true);


	// 设置表格内容双击可编辑
	ui.tableWidget_RasterARD->setEditTriggers(QAbstractItemView::DoubleClicked);
	ui.tableWidget_VectorARD->setEditTriggers(QAbstractItemView::DoubleClicked);

	// 最后一列铺满最后
	ui.tableWidget_RasterARD->horizontalHeader()->setStretchLastSection(true);
	ui.tableWidget_VectorARD->horizontalHeader()->setStretchLastSection(true);


	// 默认选择地形因子类
	ui.radioButton_terrain_factor->setChecked(true);
	ui.radioButton_shp_raster->setEnabled(false);
	ui.radioButton_gpkg_raster->setEnabled(false);

	// 默认shp格式
	ui.radioButton_shp_raster->setChecked(true);
	ui.radioButton_shp_vector->setChecked(true);

	// 默认选择栅格分析就绪数据定制页面
	ui.tabWidget_ARD_Setting->setCurrentIndex(0);

	m_vRasterizeConfig.clear();
	m_vTerrainFactorConfig.clear();
	m_vVectorARDConfig.clear();

	// 默认生成元数据
	ui.checkBox_RasterMetaData->setChecked(true);
	ui.checkBox_VectorMetaData->setChecked(true);

	// 默认日志级别：错误
	ui.comboBox_LogLevel->setCurrentText(0);

#pragma	region "第三方测试注释掉gpkg的单选按钮，设置为不可见"
	
	/*修改时间：2025-04-26*/
	/*第三方测试注释掉gpkg的单选按钮，设置为不可见*/
	ui.radioButton_gpkg_raster->setVisible(false);
	ui.radioButton_gpkg_vector->setVisible(false);

#pragma endregion
}

SE_CustomizeARDDlg::~SE_CustomizeARDDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/RasterARDConfigPath"), m_qstrRasterARDConfigPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/VectorARDConfigPath"), m_qstrVectorARDConfigPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/RasterARD_InputDataPath"), m_qstrRasterARD_InputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/VectorARD_InputDataPath"), m_qstrVectorARD_InputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/RasterARDOutputDataPath"), m_qstrRasterARDOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/VectorARDOutputDataPath"), m_qstrVectorARDOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

	// 栅格ARD定制数据源元数据路径
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/InputRasterARDMetadataPath"), m_qstrInputRasterARDMetadataPath, QgsSettings::Section::Plugins);

	// 矢量ARD定制数据源元数据路径
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/InputVectorARDMetadataPath"), m_qstrInputVectorARDMetadataPath, QgsSettings::Section::Plugins);

}

void SE_CustomizeARDDlg::restoreState()
{
	const QgsSettings settings;

	// 输入栅格分析就绪数据定制参数配置文件数据路径
	m_qstrRasterARDConfigPath = settings.value(QStringLiteral("CUSTOMIZE_ARD/RasterARDConfigPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 输入矢量分析就绪数据定制参数配置文件数据路径
	m_qstrVectorARDConfigPath = settings.value(QStringLiteral("CUSTOMIZE_ARD/VectorARDConfigPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 输入栅格分析就绪数据源路径
	m_qstrRasterARD_InputDataPath = settings.value(QStringLiteral("CUSTOMIZE_ARD/RasterARD_InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 输入矢量分析就绪数据源路径
	m_qstrVectorARD_InputDataPath = settings.value(QStringLiteral("CUSTOMIZE_ARD/VectorARD_InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 输出栅格分析就绪数据保存路径
	m_qstrRasterARDOutputDataPath = settings.value(QStringLiteral("CUSTOMIZE_ARD/RasterARDOutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 输出矢量分析就绪数据保存路径
	m_qstrVectorARDOutputDataPath = settings.value(QStringLiteral("CUSTOMIZE_ARD/VectorARDOutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 输出日志保存路径
	m_qstrOutputLogPath = settings.value(QStringLiteral("CUSTOMIZE_ARD/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 栅格ARD定制数据源元数据路径
	m_qstrInputRasterARDMetadataPath = settings.value(QStringLiteral("CUSTOMIZE_ARD/InputRasterARDMetadataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 矢量ARD定制数据源元数据路径
	m_qstrInputVectorARDMetadataPath = settings.value(QStringLiteral("CUSTOMIZE_ARD/InputVectorARDMetadataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}





void SE_CustomizeARDDlg::pushButton_OpenRasterARD_ConfigFilePath_clicked()
{
	m_vTerrainFactorConfig.clear();
	m_vRasterizeConfig.clear();

	// 读取csv文件
	// 初始化GDAL
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	// 如果选择地形因子，加载地形因子定制参数配置文件
	if (ui.radioButton_terrain_factor->isChecked())
	{
		QString curPath = QCoreApplication::applicationDirPath();
		QString dlgTitle = tr("选择地形因子类分析就绪数据定制参数配置文件");
		QString filter = tr("csv 文件(*.csv)");
		QString strFileName = QFileDialog::getOpenFileName(this,
			dlgTitle, curPath, filter);
		if (strFileName.isEmpty())
		{
			return;
		}

		string strCsv = strFileName.toLocal8Bit();
		ui.lineEdit_RasterARD_ConfigFilePath->setText(strFileName);
		m_qstrRasterARDConfigPath = strFileName;

		// 打开CSV文件
		GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(strCsv.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));

		if (!poDS)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("打开地形因子类分析就绪数据定制参数配置文件失败！"));
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

		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			RasterARD_TerrainFactorConfigInfo info;

			// 获取要素的所有属性（即CSV文件中的每一列）

			// 第一列“图层类型”
			info.strLayerType = QString::fromUtf8(poFeature->GetFieldAsString(0));

			// 第二列“分析就绪数据名称”
			info.strARDName = QString::fromUtf8(poFeature->GetFieldAsString(1));		

			// 第三列“分析就绪数据标识”
			info.strARDIdentify = QString::fromUtf8(poFeature->GetFieldAsString(2));

			m_vTerrainFactorConfig.push_back(info);

			// 释放要素资源
			OGRFeature::DestroyFeature(poFeature);
		}

		// 关闭并释放数据集资源
		GDALClose(poDS);

		// 删除所有行
		while (ui.tableWidget_RasterARD->rowCount() > 0)
		{
			ui.tableWidget_RasterARD->removeRow(0);
		}

		// 更新表格控件中的数据
		UpdateTableWidget(1);
	}

	// 如果选择矢量数据栅格化，加载矢量数据栅格化定制参数配置文件
	else if (ui.radioButton_rasterize->isChecked())
	{
		QString curPath = QCoreApplication::applicationDirPath();
		QString dlgTitle = tr("选择矢量数据栅格化类分析就绪数据定制参数配置文件");
		QString filter = tr("csv 文件(*.csv)");
		QString strFileName = QFileDialog::getOpenFileName(this,
			dlgTitle, curPath, filter);
		if (strFileName.isEmpty())
		{
			return;
		}

		string strCsv = strFileName.toLocal8Bit();
		ui.lineEdit_RasterARD_ConfigFilePath->setText(strFileName);
		m_qstrRasterARDConfigPath = strFileName;

		// 打开CSV文件
		GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(strCsv.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));

		if (!poDS)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("打开地形因子类分析就绪数据定制参数配置文件失败！"));
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

		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			RasterARD_RasterizeConfigInfo info;

			// 获取要素的所有属性（即CSV文件中的每一列）

			// 第一列“图层名称”
			info.strLayerName = QString::fromUtf8(poFeature->GetFieldAsString(0));

			// 第二列“图层类型”
			info.strLayerType = QString::fromUtf8(poFeature->GetFieldAsString(1));

			// 第三列“分析就绪数据名称”
			info.strARDName = QString::fromUtf8(poFeature->GetFieldAsString(2));

			// 第四列“分析就绪数据标识”
			info.strARDIdentify = QString::fromUtf8(poFeature->GetFieldAsString(3));

			// 第五列“字段”
			info.strField = QString::fromUtf8(poFeature->GetFieldAsString(4));

			m_vRasterizeConfig.push_back(info);

			// 释放要素资源
			OGRFeature::DestroyFeature(poFeature);
		}

		// 关闭并释放数据集资源
		GDALClose(poDS);

		// 删除所有行
		while (ui.tableWidget_RasterARD->rowCount() > 0)
		{
			ui.tableWidget_RasterARD->removeRow(0);
		}

		// 更新表格控件中的数据
		UpdateTableWidget(2);
	}

	AdjustRasterARDTableWidgetStyle();
}

void SE_CustomizeARDDlg::pushButton_OpenVectorARD_ConfigFilePath_clicked()
{
	m_vVectorARDConfig.clear();

	// 读取csv文件
	// 初始化GDAL
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();


	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择矢量分析就绪数据定制参数配置文件");
	QString filter = tr("csv 文件(*.csv)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	string strCsv = strFileName.toLocal8Bit();
	ui.lineEdit_VectorARD_ConfigFilePath->setText(strFileName);
	m_qstrVectorARDConfigPath = strFileName;

	// 打开CSV文件
	GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(strCsv.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));

	if (!poDS)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("打开矢量分析就绪数据定制参数配置文件失败！"));
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

	while ((poFeature = poLayer->GetNextFeature()) != NULL)
	{
		VectorARD_ConfigInfo info;

		// 获取要素的所有属性（即CSV文件中的每一列）

		// 第一列“图层名称”
		info.strLayerName = QString::fromUtf8(poFeature->GetFieldAsString(0));

		// 第二列“分析就绪数据名称”
		info.strARDName = QString::fromUtf8(poFeature->GetFieldAsString(1));

		// 第三列“分析就绪数据标识”
		info.strARDIdentify = QString::fromUtf8(poFeature->GetFieldAsString(2));

		m_vVectorARDConfig.push_back(info);

		// 释放要素资源
		OGRFeature::DestroyFeature(poFeature);
	}

	// 关闭并释放数据集资源
	GDALClose(poDS);

	// 删除所有行
	while (ui.tableWidget_VectorARD->rowCount() > 0)
	{
		ui.tableWidget_VectorARD->removeRow(0);
	}

	// 更新表格控件中的数据
	UpdateVectorARDTableWidget();
	
	AdjustVectorARDTableWidgetStyle();
}

/*栅格分析就绪数据源路径*/
void SE_CustomizeARDDlg::pushButton_OpenRasterARD_InputDataPath_clicked()
{
	// 如果是地形因子类栅格分析就绪数据，则选择单个tif格式DEM路径
	if(ui.radioButton_terrain_factor->isChecked())
	{
		QString curPath = QCoreApplication::applicationDirPath();
		QString dlgTitle = tr("选择DEM文件");
		QString filter = tr("tif 文件(*.tif)");
		QString strFileName = QFileDialog::getOpenFileName(this,
			dlgTitle, curPath, filter);

		if (!strFileName.isEmpty())
		{
			m_qstrRasterARD_InputDataPath = strFileName;
		}
		else
		{
			return;
		}
		ui.lineEdit_RasterARD_InputDataPath->setText(strFileName);
		
		// 读取栅格DEM信息（图层名称、图层分辨率），填充到tablewidget中
		// 支持中文路径
		CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");			// 支持中文路径

		// 加载驱动
		GDALAllRegister();

		// 打开数据
		GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strFileName.toLocal8Bit(), GDAL_OF_RASTER, NULL, NULL, NULL);
		if (!poDS)
		{
			// TODO写日志：打开DEM数据失败
			return;
		}

		double dSrcGeoTrans[6] = { 0 };
		poDS->GetGeoTransform(dSrcGeoTrans);

		// 获取X方向分辨率
		double dCellSizeX = fabs(dSrcGeoTrans[1]);

		// 空间参考名称
		m_strDemCRSName = poDS->GetSpatialRef()->GetName();

		// 数据范围

		// 获取Y方向分辨率
		double dCellSizeY = fabs(dSrcGeoTrans[5]);

		// 东西方向列数
		int iRasterXSize = poDS->GetRasterXSize();

		// 南北方向行数
		int iRasterYSize = poDS->GetRasterYSize();

		// 源数据左上角坐标
		// 图层左边界
		m_dDemLayerRect.dleft = dSrcGeoTrans[0];

		// 图层上边界
		m_dDemLayerRect.dtop = dSrcGeoTrans[3];

		// 图层右边界
		m_dDemLayerRect.dright = m_dDemLayerRect.dleft + iRasterXSize * dCellSizeX;

		// 图层下边界
		m_dDemLayerRect.dbottom = m_dDemLayerRect.dtop - iRasterYSize * dCellSizeY;

		// 关闭数据源
		GDALClose(poDS);

		// 创建 QFileInfo 对象
		QFileInfo fileInfo(strFileName);

		// 获取文件的基本名（不带扩展名）
		QString baseName = fileInfo.baseName(); 

		// 更新数据到tablewidget中
		for (int iRowIndex = 0; iRowIndex < ui.tableWidget_RasterARD->rowCount(); ++iRowIndex)
		{
			// 插入图层名称（0）
			ui.tableWidget_RasterARD->setItem(iRowIndex, 0, new QTableWidgetItem(baseName));

			// 插入DEM数据分辨率（5）
			ui.tableWidget_RasterARD->setItem(iRowIndex, 5, new QTableWidgetItem(QString::number(dCellSizeX)));
		}

		AdjustRasterARDTableWidgetStyle();
	}

	// 如果是矢量数据栅格化，并且选择shp，则打开shp所在文件夹，如果选择gpkg，则选择单个gpkg数据
	else if (ui.radioButton_rasterize->isChecked())
	{
		// 如果选择shp
		if (ui.radioButton_shp_raster->isChecked())
		{
			QString curPath = QCoreApplication::applicationDirPath();
			QString dlgTile = tr("请选择shp数据所在目录");
			QString directory = QFileDialog::getExistingDirectory(this,
				dlgTile,
				curPath, // 默认路径
				QFileDialog::ShowDirsOnly);

			if (!directory.isEmpty()) 
			{
				m_qstrRasterARD_InputDataPath = directory;
			}
			else
			{
				return;
			}
			ui.lineEdit_RasterARD_InputDataPath->setText(directory);
		}

		// 如果选择gpkg文件
		else if (ui.radioButton_gpkg_raster->isChecked())
		{
			QString curPath = QCoreApplication::applicationDirPath();
			QString dlgTitle = tr("选择gpkg文件");
			QString filter = tr("gpkg 文件(*.gpkg)");
			QString strFileName = QFileDialog::getOpenFileName(this,
				dlgTitle, curPath, filter);
			if (!strFileName.isEmpty())
			{
				m_qstrRasterARD_InputDataPath = strFileName;
			}
			else
			{
				return;
			}
			ui.lineEdit_RasterARD_InputDataPath->setText(strFileName);
		}

		AdjustRasterARDTableWidgetStyle();
	}
}

void SE_CustomizeARDDlg::pushButton_OpenVectorARD_InputDataPath_clicked()
{
	// 如果是矢量数据栅格化，并且选择shp，则打开shp所在文件夹，如果选择gpkg，则选择单个gpkg数据
	// 如果选择shp
	if (ui.radioButton_shp_vector->isChecked())
	{
		QString curPath = QCoreApplication::applicationDirPath();
		QString dlgTile = tr("请选择shp数据所在目录");
		QString directory = QFileDialog::getExistingDirectory(this,
			dlgTile,
			curPath, // 默认路径
			QFileDialog::ShowDirsOnly);

		if (!directory.isEmpty())
		{
			m_qstrVectorARD_InputDataPath = directory;
		}
		else
		{
			return;
		}
		ui.lineEdit_VectorARD_InputDataPath->setText(directory);
	}

	// 如果选择gpkg文件
	else if (ui.radioButton_gpkg_vector->isChecked())
	{
		QString curPath = QCoreApplication::applicationDirPath();
		QString dlgTitle = tr("选择gpkg文件");
		QString filter = tr("gpkg 文件(*.gpkg)");
		QString strFileName = QFileDialog::getOpenFileName(this,
			dlgTitle, curPath, filter);
		if (!strFileName.isEmpty())
		{
			m_qstrVectorARD_InputDataPath = strFileName;
		}
		else
		{
			return;
		}
		ui.lineEdit_VectorARD_InputDataPath->setText(strFileName);
	}
	
	AdjustVectorARDTableWidgetStyle();
}

void SE_CustomizeARDDlg::pushButton_RasterARD_SavePath_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrRasterARDOutputDataPath;
	QString dlgTile = tr("请选择栅格分析就绪数据保存路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		m_qstrRasterARDOutputDataPath = selectedDir;
		ui.lineEdit_RasterARD_SavePath->setText(m_qstrRasterARDOutputDataPath);
	}

}

void SE_CustomizeARDDlg::pushButton_VectorARD_SavePath_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrVectorARDOutputDataPath;
	QString dlgTile = tr("请选择矢量分析就绪数据保存路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		m_qstrVectorARDOutputDataPath = selectedDir;
		ui.lineEdit_VectorARD_SavePath->setText(m_qstrVectorARDOutputDataPath);
	}

	
}



void SE_CustomizeARDDlg::Button_OK_accepted()
{
	// 重置进度条
	ui.progressBar->setValue(0);

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

	// 如果当前选择定制栅格分析就绪数据
	if (ui.tabWidget_ARD_Setting->currentIndex() == 0)
	{
		// 如果不存在
		if (!CheckFileOrDirExist(ui.lineEdit_RasterARD_ConfigFilePath->text()))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_RasterARD_ConfigFilePath->text()));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		// 如果不存在
		if (!CheckFileOrDirExist(ui.lineEdit_RasterARD_InputDataPath->text()))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_RasterARD_InputDataPath->text()));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		// 如果不存在
		if (!CheckFileOrDirExist(ui.lineEdit_RasterARD_SavePath->text()))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_RasterARD_SavePath->text()));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		// 如果元数据文件不存在
		if (!CheckFileOrDirExist(ui.lineEdit_RasterARD_InputMetadataPath->text()))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_RasterARD_InputMetadataPath->text()));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		// 如果选择“地形因子类”
		if (ui.radioButton_terrain_factor->isChecked())
		{
			/* 将所有定制参数发送到任务类，包括：
			1、tif文件全路径；
			2、栅格分析就绪数据生成参数（分析就绪数据产品标识）；
			3、分析结果保存路径；*/

			// 遍历表格的每一行
			for (int i = 0; i < ui.tableWidget_RasterARD->rowCount(); ++i)
			{
				// tif文件全路径
				string strTifFilePath = ui.lineEdit_RasterARD_InputDataPath->text().toLocal8Bit();

				// 分析就绪数据产品标识
				RasterAnalysisParam param;
				string strARDIdentify = ui.tableWidget_RasterARD->item(i, 3)->text().toLocal8Bit();
				param.vType.push_back(strARDIdentify);

				// 如果选择生产
				if (ui.tableWidget_RasterARD->item(i, 7)->checkState() == Qt::Checked)
				{
					param.vFlag.push_back(true);
				}
				else
				{
					param.vFlag.push_back(false);
				}

				// 分析结果保存路径
				string strOutputPath = ui.lineEdit_RasterARD_SavePath->text().toLocal8Bit();

				// 创建任务
				SE_CustomizeTerrainFactorTask *task = new SE_CustomizeTerrainFactorTask(
					tr("生成%1：%2分析就绪数据").arg(ui.tableWidget_RasterARD->item(i, 2)->text()).arg(ui.tableWidget_RasterARD->item(i, 3)->text()),
					strTifFilePath,
					param,
					strOutputPath,
					strARDIdentify,
					iLogLevel,
					strOutputLogPath);
				QgsApplication::taskManager()->addTask(task);

				connect(task, &SE_CustomizeTerrainFactorTask::taskFinished, this, &SE_CustomizeARDDlg::onTaskFinished);
				
			}

#pragma region "增加栅格分析就绪数据元数据输出"

			// 如果选择生成栅格分析就绪数据元数据，读取DEM的元数据信息，按照栅格数据元数据模板输出到目标路径下，
			// 元数据命名为：raster_ARD_terrain_factors.json
			if (ui.checkBox_RasterMetaData->isChecked()
				&& ui.tableWidget_RasterARD->rowCount() > 0)
			{
				/*修改时间：20250430
				  修改内容：增加读取DEM数据的json元数据
				*/
#pragma region "读取DEM数据对应的元数据信息"
				string strInputMetadataPath = ui.lineEdit_RasterARD_InputMetadataPath->text().toLocal8Bit();

				// 数据源元数据
				RasterDataMetadataInfo sSourceDataMetaInfo;

				if (!ReadRasterMetaDataJsonFile(strInputMetadataPath, sSourceDataMetaInfo))
				{
					QMessageBox msgBox;
					msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
					msgBox.setText(tr("读取元数据文件：%1失败，请检查元数据文件的正确性！").arg(ui.lineEdit_RasterARD_InputMetadataPath->text()));
					msgBox.setStandardButtons(QMessageBox::Yes);
					msgBox.setDefaultButton(QMessageBox::Yes);
					msgBox.exec();
					return;
				}


#pragma endregion


				// 输出的元数据信息基本来自于数据源元数据


				// 创建 JSON 对象
				ordered_json j;

				// 添加 data_source_list
				j["data_source_list"] = ordered_json::array();

				ordered_json data_source;

				// 填充字段
				data_source["GLXX_CPMC"] = "";		// 产品名称	
				data_source["GLXX_SCDW"] = "";		// 生产单位
				data_source["GLXX_HJDW"] = "";		// 汇交单位
				data_source["GLXX_SCRQ"] = "";		// 生产日期
				data_source["GLXX_GXRQ"] = "";		// 更新日期
				data_source["GLXX_CPMJ"] = "";		// 产品密级
				data_source["GLXX_CPBB"] = "";		// 产品版本
					
				// 高程分辨率
				double dCellSize = ui.tableWidget_RasterARD->item(0, 5)->text().toDouble();

				data_source["BSXX_FBL"] = dCellSize;		// 分辨率
				
				//--------------------------------------------//											
				// 2025-04-28 增加四至范围
				char szSJSZFW[100] = { 0 };
				sprintf(szSJSZFW, "%.6f,%.6f,%.6f,%.6f",
					m_dDemLayerRect.dleft,
					m_dDemLayerRect.dbottom,
					m_dDemLayerRect.dright,
					m_dDemLayerRect.dtop);

				data_source["BSXX_SJSZFW"] = szSJSZFW;		// 数据四至范围，数组有4个元素，依次为左,下,右,上地理坐标，度为单位
				//--------------------------------------------//
				data_source["BSXX_ZBXT"] = m_strDemCRSName;		// 坐标系统
				data_source["BSXX_GCJZ"] = sSourceDataMetaInfo.strBSXX_GCJZ;		// 高程基准
				data_source["BSXX_DTTY"] = sSourceDataMetaInfo.strBSXX_DTTY;		// 地图投影
				data_source["BSXX_ZYJX"] = sSourceDataMetaInfo.iBSXX_ZYJX;			// 中央经线
				data_source["BSXX_BZWX"] = sSourceDataMetaInfo.dBSXX_BZWX;			// 标准纬线
				data_source["BSXX_ZBDW"] = sSourceDataMetaInfo.strBSXX_ZBDW;		// 坐标单位
					
				data_source["ZLXX_WZX"] = "";		// 完整性
				data_source["ZLXX_SJZL"] = "";		// 数据质量
					
				data_source["ZTXX_CPMC"] = "";		// 产品名称
				data_source["ZTXX_SCDW"] = "";		// 生产单位
				data_source["ZTXX_HJDW"] = "";		// 汇交单位
				data_source["ZTXX_SCRQ"] = "";		// 生产日期
				data_source["ZTXX_GXRQ"] = "";		// 更新日期
				data_source["ZTXX_CPMJ"] = "";		// 产品密级
				data_source["ZTXX_CPBB"] = "";		// 产品版本
				data_source["ZTXX_GJZ"] = "";		// 关键字
				data_source["ZTXX_SJL"] = "";		// 数据量
				data_source["ZTXX_SJGS"] = "tif";		// 数据格式
				data_source["ZTXX_SJLY"] = sSourceDataMetaInfo.strZTXX_SJLY;		// 数据来源
				data_source["ZTXX_ZYQH"] = sSourceDataMetaInfo.strZTXX_ZYQH;		// 作业区号
				data_source["ZTXX_TM"] = sSourceDataMetaInfo.strZTXX_TM;			// 图名
				data_source["ZTXX_TH"] = sSourceDataMetaInfo.strZTXX_TH;			// 图号
				data_source["ZTXX_BLCFM"] = 0;		// 比例尺分母
				data_source["ZTXX_ZBXT"] = m_strDemCRSName;		// 坐标系统
				data_source["ZTXX_GCJZ"] = sSourceDataMetaInfo.strZTXX_GCJZ;		// 高程基准
				data_source["ZTXX_DTTY"] = sSourceDataMetaInfo.strZTXX_DTTY;		// 地图投影
				data_source["ZTXX_DGJ"] = sSourceDataMetaInfo.iZTXX_DGJ;			// 等高距
				data_source["ZTXX_ZQSM"] = sSourceDataMetaInfo.strZTXX_ZQSM;		// 政区说明

				// 将数据源添加到列表中
				j["data_source_list"].push_back(data_source);
				

				// 添加 analysis_ready_data
				j["analysis_ready_data"] = ordered_json::array();

				// 根据高程分辨率获取对应分析就绪数据级别

				// 就绪数据格网级别
				int iAnalysisDataLevel = GalAnalysisReadyDataLevelByDemCellSize(dCellSize);

				// 就绪数据格网分辨率
				double dAnalysisiDataRes = GetGridResByGridLevel(iAnalysisDataLevel);

				// 打包格网级别
				int iPackLevel = CalPackLevelByDataLevel(iAnalysisDataLevel);
				
				// 格网级别
				int iGridLevel = iPackLevel;

				// 分析就绪数据级别
				int iDataLevel = iAnalysisDataLevel;

				int iMinX, iMaxX, iMinY, iMaxY;
				CalXRangeAndYRangeByGeoRectAndLevel(
					m_dDemLayerRect,
					iGridLevel,
					iMinX,
					iMaxX,
					iMinY,
					iMaxY);

				ordered_json analysis;
				analysis["grid_level"] = iPackLevel;
				analysis["data_level"] = iDataLevel;
				analysis["data_format"] = "tif";
				analysis["data_type"] = "float";
				analysis["crs"] = m_strDemCRSName;
				analysis["min_x"] = iMinX;
				analysis["max_x"] = iMaxX;
				analysis["min_y"] = iMinY;
				analysis["max_y"] = iMaxY;
				analysis["left"] = m_dDemLayerRect.dleft;
				analysis["top"] = m_dDemLayerRect.dtop;
				analysis["right"] = m_dDemLayerRect.dright;
				analysis["bottom"] = m_dDemLayerRect.dbottom;
				analysis["data_source"] = 1;
				analysis["detail"] = "";

				// 将分析数据添加到列表中
				j["analysis_ready_data"].push_back(analysis);
				

				// 元数据结果保存路径
				string strOutputMetaDataPath = ui.lineEdit_RasterARD_SavePath->text().toLocal8Bit();
				strOutputMetaDataPath += "/terrain_factors_metadata.json";


				// 将 JSON 对象写入文件
				std::ofstream file(strOutputMetaDataPath.c_str());
				if (file.is_open()) 
				{
					// 以 4 个空格缩进格式化输出
					file << j.dump(4); 
					file.close();				
				}
			}

#pragma endregion
		}

		// 如果选择“矢量数据栅格化”
		else if (ui.radioButton_rasterize->isChecked())
		{
			/* 将所有定制参数发送到任务类，包括：
			1、VectorLayerParam定制参数
			a）图层路径，从输入数据路径中根据图层名称获取
			b）图层类型，从表格控件中第1列获取
			c）分析就绪数据标识，从表格控件中第4列获取
			d）栅格化字段名称，从表格控件中第5列获取
			e）比例尺分母，从表格控件中第7列获取
			f）是否生成，从表格控件中第8列获取
		
			矢量数据全路径，
			如果是shp，则输入shp的全路径；如果是gpkg，则输入gpkg的单文件路径；
			2、分析就绪数据结果保存路径；*/

			// 如果选择shp数据
			if (ui.radioButton_shp_raster->isChecked())
			{
					
				string strInputDataPath = ui.lineEdit_RasterARD_InputDataPath->text().toLocal8Bit();
				vector<string> vShpPaths;
				vShpPaths.clear();
				
				// 获取当前输入数据源路径下所有的shp文件
				GetShpFilesFromFilePath(strInputDataPath, vShpPaths);

				// 提示加载定制规则文件
				if (ui.tableWidget_RasterARD->rowCount() == 0)
				{
					QMessageBox msgBox;
					msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
					msgBox.setText(tr("请加载矢量数据栅格化类定制参数配置文件！"));
					msgBox.setStandardButtons(QMessageBox::Yes);
					msgBox.setDefaultButton(QMessageBox::Yes);
					msgBox.exec();
					return;
				}

				// 如果比例尺未填写，则提示
				for(int i = 0; i < ui.tableWidget_RasterARD->rowCount(); ++i)
				{
					if (!ui.tableWidget_RasterARD->item(i, 6))
					{
						QMessageBox msgBox;
						msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
						msgBox.setText(tr("请填写比例尺分母！"));
						msgBox.setStandardButtons(QMessageBox::Yes);
						msgBox.setDefaultButton(QMessageBox::Yes);
						msgBox.exec();
						return;
					}
				}

				// 遍历表格的每一行
				for (int i = 0; i < ui.tableWidget_RasterARD->rowCount(); ++i)
				{
					VectorLayerParam param;
					
					// 读取图层名称
					QString qstrLayerName = ui.tableWidget_RasterARD->item(i, 0)->text();
					string strLayerName = qstrLayerName.toLocal8Bit();
	
					// 根据图层名称从shp数据路径中获取图层全路径
					// 修改日期：2025-2-7
					// 修改内容：增加对同名图层分块后数据的定制，例如：L_polygon会分成N个图层L_polygon_{z}_{x}_{y}
					
					// 存储所有同名图层的索引
					vector<int> vLayerIndex;
					vLayerIndex.clear();
					for (int iIndex = 0; iIndex < vShpPaths.size(); ++iIndex)
					{
						string strInputShpName = CPLGetBasename(vShpPaths[iIndex].c_str());
						// 如果包含表格中第一列图层名称
						if (strstr(strInputShpName.c_str(), strLayerName.c_str()) != nullptr)
						{
							vLayerIndex.push_back(iIndex);
						}
					}

					// 如果输入数据源中无此图层，跳过
					if (vLayerIndex.size() == 0)
					{
						continue;
					}
					
					// 循环每个包含表格第一列图层名称的所有图层
					for (const auto& layerIndex : vLayerIndex)
					{
						// 图层全路径
						param.strLayerPath = vShpPaths[layerIndex];

						// 分析就绪数据产品标识					
						string strARDIdentify = ui.tableWidget_RasterARD->item(i, 3)->text().toLocal8Bit();
						param.vAnalysisReadyDataCode.push_back(strARDIdentify);

						// 栅格化字段名称
						string strField = ui.tableWidget_RasterARD->item(i, 4)->text().toLocal8Bit();
						param.vAnalysisReadyDataField.push_back(strField);

						// 比例尺分母
						param.dScale = ui.tableWidget_RasterARD->item(i, 6)->text().toDouble();
						m_dScale = param.dScale;

						// 如果选择生产
						if (ui.tableWidget_RasterARD->item(i, 7)->checkState() == Qt::Checked)
						{
							param.bFlag = true;
						}
						else
						{
							param.bFlag = false;
						}

						// 分析结果保存路径
						string strOutputPath = ui.lineEdit_RasterARD_SavePath->text().toLocal8Bit();

						// 创建任务
						SE_CustomizeRasterizeTask* task = new SE_CustomizeRasterizeTask(
							tr("生成%1：%2分析就绪数据").arg(ui.tableWidget_RasterARD->item(i, 2)->text()).arg(ui.tableWidget_RasterARD->item(i, 3)->text()),
							param,
							strOutputPath,
							strARDIdentify,
							iLogLevel,
							strOutputLogPath);
						QgsApplication::taskManager()->addTask(task);

						connect(task, &SE_CustomizeRasterizeTask::taskFinished, this, &SE_CustomizeARDDlg::onTaskFinished);
					}			
				}


#pragma region "增加栅格分析就绪数据元数据输出（矢量数据栅格化）"

				// 如果选择生成矢量数据栅格化数据元数据，读取矢量图层的信息，按照栅格分析就绪数据元数据模板输出到目标路径下，
				// 元数据命名为：rasterize_metadata.json
				if (ui.checkBox_RasterMetaData->isChecked()
					&& ui.tableWidget_RasterARD->rowCount() > 0)
				{
					/*修改时间：20250501
					修改内容：增加读取shp数据的json元数据
					*/
#pragma region "读取shp数据对应的元数据信息"
					string strInputMetadataPath = ui.lineEdit_RasterARD_InputMetadataPath->text().toLocal8Bit();

					// 数据源元数据
					VectorDataMetadataInfo sSourceDataMetaInfo;

					if (!ReadVectorMetaDataJsonFile(strInputMetadataPath, sSourceDataMetaInfo))
					{
						QMessageBox msgBox;
						msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
						msgBox.setText(tr("读取元数据文件：%1失败，请检查元数据文件的正确性！").arg(ui.lineEdit_RasterARD_InputMetadataPath->text()));
						msgBox.setStandardButtons(QMessageBox::Yes);
						msgBox.setDefaultButton(QMessageBox::Yes);
						msgBox.exec();
						return;
					}

#pragma endregion



					// 注册所有驱动
					GDALAllRegister();

					// 所有图层的范围
					OGREnvelope TotalEnvelop;

					// 读取shp数据的空间参考信息
					if (vShpPaths.size() > 0)
					{
						// 打开Shapefile
						GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(vShpPaths[0].c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
						if (poSrcDS == nullptr)
						{
							// TODO:记录日志，打开shp图层失败
							return ;
						}

						OGRLayer* pLayer = poSrcDS->GetLayer(0);
						if (!pLayer)
						{
							GDALClose(poSrcDS);
							return;
						}

						pLayer->GetExtent(&TotalEnvelop);

						m_strVectorLayerCRSName = pLayer->GetSpatialRef()->GetName();
						
						GDALClose(poSrcDS);
					}

					// 计算所有图层的范围
					for (const auto& shpFile : vShpPaths)
					{
						// 打开Shapefile
						GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(shpFile.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
						if (poSrcDS == nullptr)
						{
							// TODO:记录日志，打开shp图层失败
							continue;
						}

						OGRLayer* pLayer = poSrcDS->GetLayer(0);
						if (!pLayer)
						{
							GDALClose(poSrcDS);
							continue;
						}
						OGREnvelope envelop;
						pLayer->GetExtent(&envelop);

						// 合并所有的图层空间范围
						TotalEnvelop.Merge(envelop);

						GDALClose(poSrcDS);
					}

					// 创建 JSON 对象
					ordered_json j;

					// 添加 data_source_list
					j["data_source_list"] = ordered_json::array();

					ordered_json data_source;

					// 填充字段
					data_source["GLXX_CPMC"] = "";		// 产品名称	
					data_source["GLXX_SCDW"] = "";		// 生产单位
					data_source["GLXX_HJDW"] = "";		// 汇交单位
					data_source["GLXX_SCRQ"] = "";		// 生产日期
					data_source["GLXX_GXRQ"] = "";		// 更新日期
					data_source["GLXX_CPMJ"] = "";		// 产品密级
					data_source["GLXX_CPBB"] = "";		// 产品版本

					data_source["BSXX_FBL"] = 0;		// 分辨率

					//--------------------------------------------//											
					// 2025-04-28 增加四至范围
					char szSJSZFW[100] = { 0 };
					sprintf(szSJSZFW, "%.6f,%.6f,%.6f,%.6f",
						TotalEnvelop.MinX,
						TotalEnvelop.MinY,
						TotalEnvelop.MaxX,
						TotalEnvelop.MaxY);

					data_source["BSXX_SJSZFW"] = szSJSZFW;		// 数据四至范围，数组有4个元素，依次为左,下,右,上地理坐标，度为单位
					//--------------------------------------------//

					data_source["BSXX_ZBXT"] = m_strVectorLayerCRSName;		// 坐标系统
					data_source["BSXX_GCJZ"] = sSourceDataMetaInfo.strBSXX_GCJZ;		// 高程基准
					data_source["BSXX_DTTY"] = sSourceDataMetaInfo.strBSXX_DTTY;		// 地图投影
					data_source["BSXX_ZYJX"] = sSourceDataMetaInfo.iBSXX_ZYJX;		// 中央经线
					data_source["BSXX_BZWX"] = sSourceDataMetaInfo.dBSXX_BZWX;		// 标准纬线
					data_source["BSXX_ZBDW"] = sSourceDataMetaInfo.strBSXX_ZBDW;		// 坐标单位

					data_source["ZLXX_WZX"] = "";		// 完整性
					data_source["ZLXX_SJZL"] = "";		// 数据质量

					data_source["ZTXX_CPMC"] = "";		// 产品名称
					data_source["ZTXX_SCDW"] = "";		// 生产单位
					data_source["ZTXX_HJDW"] = "";		// 汇交单位
					data_source["ZTXX_SCRQ"] = "";		// 生产日期
					data_source["ZTXX_GXRQ"] = "";		// 更新日期
					data_source["ZTXX_CPMJ"] = "";		// 产品密级
					data_source["ZTXX_CPBB"] = "";		// 产品版本
					data_source["ZTXX_GJZ"] = "";		// 关键字
					data_source["ZTXX_SJL"] = "";		// 数据量
					data_source["ZTXX_SJGS"] = "shp";		// 数据格式
					data_source["ZTXX_SJLY"] = sSourceDataMetaInfo.strZTXX_SJLY;		// 数据来源
					data_source["ZTXX_ZYQH"] = sSourceDataMetaInfo.strZTXX_ZYQH;		// 作业区号
					data_source["ZTXX_TM"] = sSourceDataMetaInfo.strZTXX_TM;		// 图名
					data_source["ZTXX_TH"] = sSourceDataMetaInfo.strZTXX_TH;		// 图号
					data_source["ZTXX_BLCFM"] = m_dScale;		// 比例尺分母
					data_source["ZTXX_ZBXT"] = m_strVectorLayerCRSName;		// 坐标系统
					data_source["ZTXX_GCJZ"] = sSourceDataMetaInfo.strZTXX_GCJZ;		// 高程基准
					data_source["ZTXX_DTTY"] = sSourceDataMetaInfo.strZTXX_DTTY;		// 地图投影
					data_source["ZTXX_DGJ"] = sSourceDataMetaInfo.iZTXX_DGJ;		// 等高距
					data_source["ZTXX_ZQSM"] = sSourceDataMetaInfo.strZTXX_ZQSM;		// 政区说明

					// 将数据源添加到列表中
					j["data_source_list"].push_back(data_source);


					// 添加 analysis_ready_data
					j["analysis_ready_data"] = ordered_json::array();

					// 根据比例尺计算分析就绪数据格网级别
					int iAnalysisDataLevel = GalAnalysisReadyDataLevelByScale(m_dScale);

					// 打包格网级别
					int iPackLevel = CalPackLevelByDataLevel(iAnalysisDataLevel);

					// 格网级别
					int iGridLevel = iPackLevel;

					// 分析就绪数据级别
					int iDataLevel = iAnalysisDataLevel;
					SE_DRect dTotalRect;
					dTotalRect.dleft = TotalEnvelop.MinX;
					dTotalRect.dright = TotalEnvelop.MaxX;
					dTotalRect.dtop = TotalEnvelop.MaxY;
					dTotalRect.dbottom = TotalEnvelop.MinY;

					int iMinX, iMaxX, iMinY, iMaxY;
					CalXRangeAndYRangeByGeoRectAndLevel(
						dTotalRect,
						iGridLevel,
						iMinX,
						iMaxX,
						iMinY,
						iMaxY);

					ordered_json analysis;
					analysis["grid_level"] = iPackLevel;
					analysis["data_level"] = iDataLevel;
					analysis["data_format"] = "tif";
					analysis["data_type"] = "float";
					analysis["crs"] = m_strVectorLayerCRSName;
					analysis["min_x"] = iMinX;
					analysis["max_x"] = iMaxX;
					analysis["min_y"] = iMinY;
					analysis["max_y"] = iMaxY;
					analysis["left"] = dTotalRect.dleft;
					analysis["top"] = dTotalRect.dtop;
					analysis["right"] = dTotalRect.dright;
					analysis["bottom"] = dTotalRect.dbottom;
					analysis["data_source"] = 1;
					analysis["detail"] = "";

					// 将分析数据添加到列表中
					j["analysis_ready_data"].push_back(analysis);


					// 元数据结果保存路径
					string strOutputMetaDataPath = ui.lineEdit_RasterARD_SavePath->text().toLocal8Bit();
					strOutputMetaDataPath += "/rasterize_metadata.json";

					// 将 JSON 对象写入文件
					std::ofstream file(strOutputMetaDataPath.c_str());
					if (file.is_open())
					{
						// 以 4 个空格缩进格式化输出
						file << j.dump(4);
						file.close();
					}
				}
#pragma endregion

			}

			// 如果选择gpkg数据库
			else if (ui.radioButton_gpkg_raster->isChecked())
			{
				// 提示加载定制规则文件
				if (ui.tableWidget_RasterARD->rowCount() == 0)
				{
					QMessageBox msgBox;
					msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
					msgBox.setText(tr("请加载矢量数据栅格化类定制参数配置文件！"));
					msgBox.setStandardButtons(QMessageBox::Yes);
					msgBox.setDefaultButton(QMessageBox::Yes);
					msgBox.exec();
					return;
				}

				// 如果比例尺未填写，则提示
				for (int i = 0; i < ui.tableWidget_RasterARD->rowCount(); ++i)
				{
					if (!ui.tableWidget_RasterARD->item(i, 6))
					{
						QMessageBox msgBox;
						msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
						msgBox.setText(tr("请填写比例尺分母！"));
						msgBox.setStandardButtons(QMessageBox::Yes);
						msgBox.setDefaultButton(QMessageBox::Yes);
						msgBox.exec();
						return;
					}
				}

				// 遍历表格的每一行
				for (int i = 0; i < ui.tableWidget_RasterARD->rowCount(); ++i)
				{
					VectorLayerParam param;

					// 读取图层名称
					QString qstrLayerName = ui.tableWidget_RasterARD->item(i, 0)->text();
					string strLayerName = qstrLayerName.toLocal8Bit();
					param.strLayerType = strLayerName;

					// 数据源输入路径
					string strInputDataPath = ui.lineEdit_RasterARD_InputDataPath->text().toLocal8Bit();

					// 图层全路径
					param.strLayerPath = strInputDataPath;


					// 分析就绪数据产品标识					
					string strARDIdentify = ui.tableWidget_RasterARD->item(i, 3)->text().toLocal8Bit();
					param.vAnalysisReadyDataCode.push_back(strARDIdentify);

					// 栅格化字段名称
					string strField = ui.tableWidget_RasterARD->item(i, 4)->text().toLocal8Bit();
					param.vAnalysisReadyDataField.push_back(strField);

					// 比例尺分母
					param.dScale = ui.tableWidget_RasterARD->item(i, 6)->text().toDouble();

					// 如果选择生产
					if (ui.tableWidget_RasterARD->item(i, 7)->checkState() == Qt::Checked)
					{
						param.bFlag = true;
					}
					else
					{
						param.bFlag = false;
					}

					// 分析结果保存路径
					string strOutputPath = ui.lineEdit_RasterARD_SavePath->text().toLocal8Bit();

					// 创建任务
					SE_CustomizeRasterizeTask* task = new SE_CustomizeRasterizeTask(
						tr("生成%1：%2分析就绪数据").arg(ui.tableWidget_RasterARD->item(i, 2)->text()).arg(ui.tableWidget_RasterARD->item(i, 3)->text()),
						param,
						strOutputPath,
						strARDIdentify,
						iLogLevel,
						strOutputLogPath);
					QgsApplication::taskManager()->addTask(task);

					connect(task, &SE_CustomizeRasterizeTask::taskFinished, this, &SE_CustomizeARDDlg::onTaskFinished);
				}

#pragma region "增加栅格分析就绪数据元数据输出（矢量数据栅格化）"

				// 如果选择生成矢量数据栅格化数据元数据，读取矢量图层的信息，按照栅格分析就绪数据元数据模板输出到目标路径下，
				// 元数据命名为：rasterize_metadata.json
				if (ui.checkBox_RasterMetaData->isChecked()
					&& ui.tableWidget_RasterARD->rowCount() > 0)
				{
					// 注册所有驱动
					GDALAllRegister();

					// 所有图层的范围
					OGREnvelope TotalEnvelop;

					// gpkg文件
					string strGpkgFilePath = ui.lineEdit_RasterARD_InputDataPath->text().toLocal8Bit();

					// 打开数据
					GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strGpkgFilePath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
					if (!poDS)
					{
						// 记录日志：打开gpkg文件失败
						return;
					}

					// 读取shp数据的空间参考信息
					if (poDS->GetLayerCount() > 0)
					{

						OGRLayer* pLayer = poDS->GetLayer(0);
						if (!pLayer)
						{
							GDALClose(poDS);
							return;
						}

						pLayer->GetExtent(&TotalEnvelop);

						m_strVectorLayerCRSName = pLayer->GetSpatialRef()->GetName();				
					}



					// 计算所有图层的范围
					for (int i = 0; i < poDS->GetLayerCount(); ++i)
					{

						OGRLayer* pLayer = poDS->GetLayer(i);
						if (!pLayer)
						{
							continue;
						}
						OGREnvelope envelop;
						pLayer->GetExtent(&envelop);

						// 合并所有的图层空间范围
						TotalEnvelop.Merge(envelop);

					}


					GDALClose(poDS);

					// 创建 JSON 对象
					ordered_json j;

					// 添加 data_source_list
					j["data_source_list"] = ordered_json::array();

					ordered_json data_source;

					// 填充字段
					data_source["GLXX_CPMC"] = "";		// 产品名称	
					data_source["GLXX_SCDW"] = "";		// 生产单位
					data_source["GLXX_HJDW"] = "";		// 汇交单位
					data_source["GLXX_SCRQ"] = "";		// 生产日期
					data_source["GLXX_GXRQ"] = "";		// 更新日期
					data_source["GLXX_CPMJ"] = "";		// 产品密级
					data_source["GLXX_CPBB"] = "";		// 产品版本

					data_source["BSXX_FBL"] = 0;		// 分辨率
					//--------------------------------------------//											
					// 2025-04-28 增加四至范围
					char szSJSZFW[100] = { 0 };
					sprintf(szSJSZFW, "%.6f,%.6f,%.6f,%.6f",
						TotalEnvelop.MinX,
						TotalEnvelop.MinY,
						TotalEnvelop.MaxX,
						TotalEnvelop.MaxY);

					data_source["BSXX_SJSZFW"] = szSJSZFW;		// 数据四至范围，数组有4个元素，依次为左,下,右,上地理坐标，度为单位
					//--------------------------------------------//
					data_source["BSXX_ZBXT"] = m_strVectorLayerCRSName;		// 坐标系统
					data_source["BSXX_GCJZ"] = "";		// 高程基准
					data_source["BSXX_DTTY"] = "";		// 地图投影
					data_source["BSXX_ZYJX"] = 0;		// 中央经线
					data_source["BSXX_BZWX"] = 0;		// 标准纬线
					data_source["BSXX_ZBDW"] = "";		// 坐标单位

					data_source["ZLXX_WZX"] = "";		// 完整性
					data_source["ZLXX_SJZL"] = "";		// 数据质量

					data_source["ZTXX_CPMC"] = "";		// 产品名称
					data_source["ZTXX_SCDW"] = "";		// 生产单位
					data_source["ZTXX_HJDW"] = "";		// 汇交单位
					data_source["ZTXX_SCRQ"] = "";		// 生产日期
					data_source["ZTXX_GXRQ"] = "";		// 更新日期
					data_source["ZTXX_CPMJ"] = "";		// 产品密级
					data_source["ZTXX_CPBB"] = "";		// 产品版本
					data_source["ZTXX_GJZ"] = "";		// 关键字
					data_source["ZTXX_SJL"] = "";		// 数据量
					data_source["ZTXX_SJGS"] = "";		// 数据格式
					data_source["ZTXX_SJLY"] = "";		// 数据来源
					data_source["ZTXX_ZYQH"] = "";		// 作业区号
					data_source["ZTXX_TM"] = "";		// 图名
					data_source["ZTXX_TH"] = "";		// 图号
					data_source["ZTXX_BLCFM"] = m_dScale;		// 比例尺分母
					data_source["ZTXX_ZBXT"] = m_strVectorLayerCRSName;		// 坐标系统
					data_source["ZTXX_GCJZ"] = "";		// 高程基准
					data_source["ZTXX_DTTY"] = "";		// 地图投影
					data_source["ZTXX_DGJ"] = 0;		// 等高距
					data_source["ZTXX_ZQSM"] = "";		// 政区说明

					// 将数据源添加到列表中
					j["data_source_list"].push_back(data_source);


					// 添加 analysis_ready_data
					j["analysis_ready_data"] = ordered_json::array();

					// 根据比例尺计算分析就绪数据格网级别
					int iAnalysisDataLevel = GalAnalysisReadyDataLevelByScale(m_dScale);

					// 打包格网级别
					int iPackLevel = CalPackLevelByDataLevel(iAnalysisDataLevel);

					// 格网级别
					int iGridLevel = iPackLevel;

					// 分析就绪数据级别
					int iDataLevel = iAnalysisDataLevel;
					SE_DRect dTotalRect;
					dTotalRect.dleft = TotalEnvelop.MinX;
					dTotalRect.dright = TotalEnvelop.MaxX;
					dTotalRect.dtop = TotalEnvelop.MaxY;
					dTotalRect.dbottom = TotalEnvelop.MinY;

					int iMinX, iMaxX, iMinY, iMaxY;
					CalXRangeAndYRangeByGeoRectAndLevel(
						dTotalRect,
						iGridLevel,
						iMinX,
						iMaxX,
						iMinY,
						iMaxY);

					ordered_json analysis;
					analysis["grid_level"] = iPackLevel;
					analysis["data_level"] = iDataLevel;
					analysis["data_format"] = "tif";
					analysis["data_type"] = "float";
					analysis["crs"] = m_strVectorLayerCRSName;
					analysis["min_x"] = iMinX;
					analysis["max_x"] = iMaxX;
					analysis["min_y"] = iMinY;
					analysis["max_y"] = iMaxY;
					analysis["left"] = dTotalRect.dleft;
					analysis["top"] = dTotalRect.dtop;
					analysis["right"] = dTotalRect.dright;
					analysis["bottom"] = dTotalRect.dbottom;
					analysis["data_source"] = 1;
					analysis["detail"] = "";

					// 将分析数据添加到列表中
					j["analysis_ready_data"].push_back(analysis);


					// 元数据结果保存路径
					string strOutputMetaDataPath = ui.lineEdit_RasterARD_SavePath->text().toLocal8Bit();
					strOutputMetaDataPath += "/rasterize_metadata.json";

					// 将 JSON 对象写入文件
					std::ofstream file(strOutputMetaDataPath.c_str());
					if (file.is_open())
					{
						// 以 4 个空格缩进格式化输出
						file << j.dump(4);
						file.close();
					}
				}
#pragma endregion
			}
		}

	}

	// 如果当前选择定制矢量分析就绪数据
	else if (ui.tabWidget_ARD_Setting->currentIndex() == 1)
	{
		/* 将所有定制参数发送到任务类，包括：
		1、VectorLayerParam定制参数
		a）图层路径，从输入数据路径中根据图层名称获取
		b）分析就绪数据名称，从表格控件中第2列获取
		c）分析就绪数据标识，从表格控件中第3列获取
		d）比例尺分母，从表格控件中第4列获取
		e）是否生成，从表格控件中第5列获取

		矢量数据全路径，
		如果是shp，则输入shp的全路径；如果是gpkg，则输入gpkg的单文件路径；
		2、分析就绪数据结果保存路径；*/


		// 如果不存在
		if (!CheckFileOrDirExist(ui.lineEdit_VectorARD_ConfigFilePath->text()))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_VectorARD_ConfigFilePath->text()));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		// 如果不存在
		if (!CheckFileOrDirExist(ui.lineEdit_VectorARD_InputDataPath->text()))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_VectorARD_InputDataPath->text()));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		// 如果不存在
		if (!CheckFileOrDirExist(ui.lineEdit_VectorARD_SavePath->text()))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_VectorARD_SavePath->text()));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		// 如果元数据文件不存在
		if (!CheckFileOrDirExist(ui.lineEdit_VectorARD_InputMetadataPath->text()))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_RasterARD_InputMetadataPath->text()));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

				
		if (ui.tableWidget_VectorARD->rowCount() > 0)
		{
			// 比例尺分母
			m_dScale = ui.tableWidget_VectorARD->item(0, 3)->text().toDouble();
		}

		// 如果选择shp数据
		if (ui.radioButton_shp_vector->isChecked())
		{
			// 获取当前输入数据源路径下所有的shp文件		
			string strInputDataPath = ui.lineEdit_VectorARD_InputDataPath->text().toLocal8Bit();
			vector<string> vShpPaths;
			vShpPaths.clear();

			// 获取当前输入数据源路径下所有的shp文件
			GetShpFilesFromFilePath(strInputDataPath, vShpPaths);
			

			// 提示加载定制规则文件
			if (ui.tableWidget_VectorARD->rowCount() == 0)
			{
				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
				msgBox.setText(tr("请加载矢量分析就绪数据定制参数配置文件！"));
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				return;
			}

			// 如果比例尺未填写，则提示
			for (int i = 0; i < ui.tableWidget_VectorARD->rowCount(); ++i)
			{
				if (!ui.tableWidget_VectorARD->item(i, 3))
				{
					QMessageBox msgBox;
					msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
					msgBox.setText(tr("请填写比例尺分母！"));
					msgBox.setStandardButtons(QMessageBox::Yes);
					msgBox.setDefaultButton(QMessageBox::Yes);
					msgBox.exec();
					return;
				}
			}

			// 遍历表格的每一行
			for (int i = 0; i < ui.tableWidget_VectorARD->rowCount(); ++i)
			{
				VectorLayerParam param;

				// 读取图层名称
				QString qstrLayerName = ui.tableWidget_VectorARD->item(i, 0)->text();
				
				string strLayerName = qstrLayerName.toLocal8Bit();

				// 根据图层名称从shp数据路径中获取图层全路径
				// 根据图层名称从shp数据路径中获取图层全路径
				// 修改日期：2025-2-7
				// 修改内容：增加对同名图层分块后数据的定制，例如：L_polygon会分成N个图层L_polygon_{z}_{x}_{y}

				// 存储所有同名图层的索引
				vector<int> vLayerIndex;
				vLayerIndex.clear();
				for (int iIndex = 0; iIndex < vShpPaths.size(); ++iIndex)
				{
					string strInputShpName = CPLGetBasename(vShpPaths[iIndex].c_str());
					// 如果包含表格中第一列图层名称
					if (strstr(strInputShpName.c_str(), strLayerName.c_str()) != nullptr)
					{
						vLayerIndex.push_back(iIndex);
					}
				}

				// 如果输入数据源中无此图层，跳过
				if (vLayerIndex.size() == 0)
				{
					continue;
				}

				// 循环每个包含表格第一列图层名称的所有图层
				for (const auto& layerIndex : vLayerIndex)
				{
					// 图层全路径
					param.strLayerPath = vShpPaths[layerIndex];

					// 分析就绪数据产品标识					
					string strARDIdentify = ui.tableWidget_VectorARD->item(i, 2)->text().toLocal8Bit();
					param.vAnalysisReadyDataCode.push_back(strARDIdentify);

					// 比例尺分母
					param.dScale = ui.tableWidget_VectorARD->item(i, 3)->text().toDouble();

					// 如果选择生产
					if (ui.tableWidget_VectorARD->item(i, 4)->checkState() == Qt::Checked)
					{
						param.bFlag = true;
					}
					else
					{
						param.bFlag = false;
					}

					// 分析结果保存路径
					string strOutputPath = ui.lineEdit_VectorARD_SavePath->text().toLocal8Bit();

					// 创建任务
					SE_CustomizeVectorTask* task = new SE_CustomizeVectorTask(
						tr("生成%1：%2分析就绪数据").arg(ui.tableWidget_VectorARD->item(i, 1)->text()).arg(ui.tableWidget_VectorARD->item(i, 2)->text()),
						param,
						strOutputPath,
						strARDIdentify,
						iLogLevel,
						strOutputLogPath);
					QgsApplication::taskManager()->addTask(task);

					connect(task, &SE_CustomizeVectorTask::taskFinished, this, &SE_CustomizeARDDlg::onTaskFinished);

				}
			}


#pragma region "增加矢量分析就绪数据元数据输出"

			// 读取矢量图层的信息，按照栅格分析就绪数据元数据模板输出到目标路径下，
			// 元数据命名为：vector_metadata.json
			if (ui.checkBox_VectorMetaData->isChecked()
				&& ui.tableWidget_VectorARD->rowCount() > 0)
			{

				/*修改时间：20250501
				修改内容：读取shp数据对应的元数据信息
				*/
#pragma region "读取shp数据对应的元数据信息"
				string strInputMetadataPath = ui.lineEdit_VectorARD_InputMetadataPath->text().toLocal8Bit();

				// 数据源元数据
				VectorDataMetadataInfo sSourceDataMetaInfo;

				if (!ReadVectorMetaDataJsonFile(strInputMetadataPath, sSourceDataMetaInfo))
				{
					QMessageBox msgBox;
					msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
					msgBox.setText(tr("读取元数据文件：%1失败，请检查元数据文件的正确性！").arg(ui.lineEdit_RasterARD_InputMetadataPath->text()));
					msgBox.setStandardButtons(QMessageBox::Yes);
					msgBox.setDefaultButton(QMessageBox::Yes);
					msgBox.exec();
					return;
				}

#pragma endregion

				// 注册所有驱动
				GDALAllRegister();

				// 所有图层的范围
				OGREnvelope TotalEnvelop;

				// 读取shp数据的空间参考信息
				if (vShpPaths.size() > 0)
				{
					// 打开Shapefile
					GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(vShpPaths[0].c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
					if (poSrcDS == nullptr)
					{
						// TODO:记录日志，打开shp图层失败
						return;
					}

					OGRLayer* pLayer = poSrcDS->GetLayer(0);
					if (!pLayer)
					{
						GDALClose(poSrcDS);
						return;
					}

					pLayer->GetExtent(&TotalEnvelop);

					m_strVectorLayerCRSName = pLayer->GetSpatialRef()->GetName();

					GDALClose(poSrcDS);
				}



				// 计算所有图层的范围
				for (const auto& shpFile : vShpPaths)
				{
					// 打开Shapefile
					GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(shpFile.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
					if (poSrcDS == nullptr)
					{
						// TODO:记录日志，打开shp图层失败
						continue;
					}

					OGRLayer* pLayer = poSrcDS->GetLayer(0);
					if (!pLayer)
					{
						GDALClose(poSrcDS);
						continue;
					}
					OGREnvelope envelop;
					pLayer->GetExtent(&envelop);

					// 合并所有的图层空间范围
					TotalEnvelop.Merge(envelop);

					GDALClose(poSrcDS);
				}



				// 创建 JSON 对象
				ordered_json j;

				// 添加 data_source_list
				j["data_source_list"] = ordered_json::array();

				ordered_json data_source;

				// 填充字段
				// 管理信息
				data_source["GLXX_CPMC"] = "";		// 产品名称	
				data_source["GLXX_BGDW"] = "";		// 保管单位
				data_source["GLXX_SCDW"] = "";		// 生产单位
				data_source["GLXX_SCRQ"] = "";		// 生产日期
				data_source["GLXX_GXRQ"] = "";		// 更新日期
				data_source["GLXX_CPMJ"] = "";		// 产品密级
				data_source["GLXX_CPBB"] = "";		// 产品版本

				// 标识信息
				data_source["BSXX_GJZ"] = "";		// 关键字
				data_source["BSXX_SJL"] = "";		// 数据量
				data_source["BSXX_SJGS"] = "shp";		// 数据格式
				data_source["BSXX_SJLY"] = sSourceDataMetaInfo.strBSXX_SJLY;		// 数据来源
				data_source["BSXX_FFMMGF"] = sSourceDataMetaInfo.strBSXX_FFMMGF;	// 分幅命名规范
				data_source["BSXX_TM"] = sSourceDataMetaInfo.strBSXX_TM;		// 图名
				data_source["BSXX_TH"] = sSourceDataMetaInfo.strBSXX_TH;		// 图号
				data_source["BSXX_BLCFM"] = m_dScale;		// 比例尺分母

				//--------------------------------------------//											
				// 2025-04-28 增加四至范围
				char szSJSZFW[100] = { 0 };
				sprintf(szSJSZFW, "%.6f,%.6f,%.6f,%.6f",
					TotalEnvelop.MinX,
					TotalEnvelop.MinY,
					TotalEnvelop.MaxX,
					TotalEnvelop.MaxY);

				data_source["BSXX_SJSZFW"] = szSJSZFW;		// 数据四至范围，数组有4个元素，依次为左,下,右,上地理坐标，度为单位
				//--------------------------------------------//
				data_source["BSXX_ZBXT"] = m_strVectorLayerCRSName;		// 坐标系统
				data_source["BSXX_GCJZ"] = sSourceDataMetaInfo.strBSXX_GCJZ;		// 高程基准
				data_source["BSXX_SDJZ"] = sSourceDataMetaInfo.strBSXX_SDJZ;		// 深度基准
				data_source["BSXX_TQCBZ"] = sSourceDataMetaInfo.dBSXX_TQCBZ;		// 椭球长半径
				data_source["BSXX_TQBL"] = sSourceDataMetaInfo.dBSXX_TQBL;		// 椭球扁率
				data_source["BSXX_DTTY"] = sSourceDataMetaInfo.strBSXX_DTTY;		// 地图投影
				data_source["BSXX_ZYJX"] = sSourceDataMetaInfo.iBSXX_ZYJX;		// 中央经线
				data_source["BSXX_BZWX"] = sSourceDataMetaInfo.dBSXX_BZWX;		// 标准纬线
				data_source["BSXX_ZBDW"] = sSourceDataMetaInfo.strBSXX_ZBDW;		// 坐标单位

				// 质量信息
				data_source["ZLXX_WZX"] = "";		// 完整性
				data_source["ZLXX_SJZL"] = "";		// 数据质量

				// 专题信息
				data_source["ZTXX_CPMC"] = "";		// 产品名称
				data_source["ZTXX_BGDW"] = "";		// 保管单位
				data_source["ZTXX_SCDW"] = "";		// 生产单位
				data_source["ZTXX_SCRQ"] = "";		// 生产日期
				data_source["ZTXX_GXRQ"] = "";		// 更新日期
				data_source["ZTXX_CPMJ"] = "";		// 产品密级
				data_source["ZTXX_CPBB"] = "";		// 产品版本
				data_source["ZTXX_GJZ"] = "";		// 关键字
				data_source["ZTXX_SJL"] = "";		// 数据量
				data_source["ZTXX_SJGS"] = "shp";		// 数据格式
				data_source["ZTXX_SJLY"] = sSourceDataMetaInfo.strZTXX_SJLY;		// 数据来源
				data_source["ZTXX_ZYQH"] = sSourceDataMetaInfo.strZTXX_ZYQH;		// 作业区号
				data_source["ZTXX_TM"] = sSourceDataMetaInfo.strZTXX_TM;		// 图名
				data_source["ZTXX_TH"] = sSourceDataMetaInfo.strZTXX_TH;		// 图号
				data_source["ZTXX_BLCFM"] = m_dScale;		// 比例尺分母
				data_source["ZTXX_ZBXT"] = m_strVectorLayerCRSName;		// 坐标系统
				data_source["ZTXX_GCJZ"] = sSourceDataMetaInfo.strZTXX_GCJZ;		// 高程基准
				data_source["ZTXX_DTTY"] = sSourceDataMetaInfo.strZTXX_DTTY;		// 地图投影
				data_source["ZTXX_DGJ"] = sSourceDataMetaInfo.iZTXX_DGJ;		// 等高距
				data_source["ZTXX_ZQSM"] = sSourceDataMetaInfo.strZTXX_ZQSM;		// 政区说明

				// 将数据源添加到列表中
				j["data_source_list"].push_back(data_source);


				// 添加 analysis_ready_data
				j["analysis_ready_data"] = ordered_json::array();

				// 根据比例尺计算分析就绪数据格网级别
				int iAnalysisDataLevel = GalAnalysisReadyDataLevelByScale(m_dScale);

				// 打包格网级别
				int iPackLevel = CalPackLevelByDataLevel(iAnalysisDataLevel);

				// 格网级别
				int iGridLevel = iPackLevel;

				// 分析就绪数据级别
				int iDataLevel = iAnalysisDataLevel;
				SE_DRect dTotalRect;
				dTotalRect.dleft = TotalEnvelop.MinX;
				dTotalRect.dright = TotalEnvelop.MaxX;
				dTotalRect.dtop = TotalEnvelop.MaxY;
				dTotalRect.dbottom = TotalEnvelop.MinY;

				int iMinX, iMaxX, iMinY, iMaxY;
				CalXRangeAndYRangeByGeoRectAndLevel(
					dTotalRect,
					iGridLevel,
					iMinX,
					iMaxX,
					iMinY,
					iMaxY);

				ordered_json analysis;
				analysis["grid_level"] = iPackLevel;
				analysis["data_level"] = iDataLevel;
				analysis["data_format"] = "shp";
				analysis["data_type"] = "geometry";
				analysis["crs"] = m_strVectorLayerCRSName;
				analysis["min_x"] = iMinX;
				analysis["max_x"] = iMaxX;
				analysis["min_y"] = iMinY;
				analysis["max_y"] = iMaxY;
				analysis["left"] = dTotalRect.dleft;
				analysis["top"] = dTotalRect.dtop;
				analysis["right"] = dTotalRect.dright;
				analysis["bottom"] = dTotalRect.dbottom;
				analysis["data_source"] = 1;
				analysis["detail"] = "";

				// 将分析数据添加到列表中
				j["analysis_ready_data"].push_back(analysis);


				// 元数据结果保存路径
				string strOutputMetaDataPath = ui.lineEdit_VectorARD_SavePath->text().toLocal8Bit();
				strOutputMetaDataPath += "/vector_metadata.json";

				// 将 JSON 对象写入文件
				std::ofstream file(strOutputMetaDataPath.c_str());
				if (file.is_open())
				{
					// 以 4 个空格缩进格式化输出
					file << j.dump(4);
					file.close();
				}
			}
#pragma endregion


		}

		// 如果选择gpkg
		else if (ui.radioButton_gpkg_vector->isChecked())
		{
			// 提示加载定制规则文件
			if (ui.tableWidget_VectorARD->rowCount() == 0)
			{
				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
				msgBox.setText(tr("请加载矢量分析就绪数据定制参数配置文件！"));
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				return;
			}

			// 如果比例尺未填写，则提示
			for (int i = 0; i < ui.tableWidget_VectorARD->rowCount(); ++i)
			{
				if (!ui.tableWidget_VectorARD->item(i, 3))
				{
					QMessageBox msgBox;
					msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
					msgBox.setText(tr("请填写比例尺分母！"));
					msgBox.setStandardButtons(QMessageBox::Yes);
					msgBox.setDefaultButton(QMessageBox::Yes);
					msgBox.exec();
					return;
				}
			}

			// 遍历表格的每一行
			for (int i = 0; i < ui.tableWidget_VectorARD->rowCount(); ++i)
			{
				VectorLayerParam param;

				// 读取图层名称
				QString qstrLayerName = ui.tableWidget_VectorARD->item(i, 0)->text();
		
				// 数据源输入路径
				string strInputDataPath = ui.lineEdit_VectorARD_InputDataPath->text().toLocal8Bit();

				// 图层名称
				string strLayerName = qstrLayerName.toLocal8Bit();

				// 数据库全路径
				param.strLayerPath = strInputDataPath;

				// 图层类型，如：L_Polygon
				param.strLayerType = strLayerName;

				// 分析就绪数据产品标识					
				string strARDIdentify = ui.tableWidget_VectorARD->item(i, 2)->text().toLocal8Bit();
				param.vAnalysisReadyDataCode.push_back(strARDIdentify);

				// 比例尺分母
				param.dScale = ui.tableWidget_VectorARD->item(i, 3)->text().toDouble();

				// 如果选择生产
				if (ui.tableWidget_VectorARD->item(i, 4)->checkState() == Qt::Checked)
				{
					param.bFlag = true;
				}
				else
				{
					param.bFlag = false;
				}

				// 分析结果保存路径
				string strOutputPath = ui.lineEdit_VectorARD_SavePath->text().toLocal8Bit();

				// 创建任务
				SE_CustomizeVectorTask* task = new SE_CustomizeVectorTask(
					tr("生成%1：%2分析就绪数据").arg(ui.tableWidget_VectorARD->item(i, 1)->text()).arg(ui.tableWidget_VectorARD->item(i, 2)->text()),
					param,
					strOutputPath,
					strARDIdentify,
					iLogLevel,
					strOutputLogPath);
				QgsApplication::taskManager()->addTask(task);

				connect(task, &SE_CustomizeVectorTask::taskFinished, this, &SE_CustomizeARDDlg::onTaskFinished);

			}

#pragma region "增加矢量分析就绪数据元数据输出"

			// 读取矢量图层的信息，按照矢量分析就绪数据元数据模板输出到目标路径下，
			// 元数据命名为：vector_metadata.json
			if (ui.checkBox_VectorMetaData->isChecked()
				&& ui.tableWidget_VectorARD->rowCount() > 0)
			{
				// 注册所有驱动
				GDALAllRegister();

				// 所有图层的范围
				OGREnvelope TotalEnvelop;

				// gpkg文件
				string strGpkgFilePath = ui.lineEdit_VectorARD_InputDataPath->text().toLocal8Bit();

				// 打开数据
				GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strGpkgFilePath.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
				if (!poDS)
				{
					// 记录日志：打开gpkg文件失败
					return;
				}

				// 读取shp数据的空间参考信息
				if (poDS->GetLayerCount() > 0)
				{

					OGRLayer* pLayer = poDS->GetLayer(0);
					if (!pLayer)
					{
						GDALClose(poDS);
						return;
					}

					pLayer->GetExtent(&TotalEnvelop);

					m_strVectorLayerCRSName = pLayer->GetSpatialRef()->GetName();
				}



				// 计算所有图层的范围
				for (int i = 0; i < poDS->GetLayerCount(); ++i)
				{

					OGRLayer* pLayer = poDS->GetLayer(i);
					if (!pLayer)
					{
						continue;
					}
					OGREnvelope envelop;
					pLayer->GetExtent(&envelop);

					// 合并所有的图层空间范围
					TotalEnvelop.Merge(envelop);

				}


				GDALClose(poDS);

				// 创建 JSON 对象
				ordered_json j;

				// 添加 data_source_list
				j["data_source_list"] = ordered_json::array();

				ordered_json data_source;

				// 填充字段
				// 管理信息
				data_source["GLXX_CPMC"] = "";		// 产品名称	
				data_source["GLXX_BGDW"] = "";		// 保管单位
				data_source["GLXX_SCDW"] = "";		// 生产单位
				data_source["GLXX_SCRQ"] = "";		// 生产日期
				data_source["GLXX_GXRQ"] = "";		// 更新日期
				data_source["GLXX_CPMJ"] = "";		// 产品密级
				data_source["GLXX_CPBB"] = "";		// 产品版本

				// 标识信息
				data_source["BSXX_GJZ"] = "";		// 关键字
				data_source["BSXX_SJL"] = "";		// 数据量
				data_source["BSXX_SJGS"] = "";		// 数据格式
				data_source["BSXX_SJLY"] = "";		// 数据来源
				data_source["BSXX_FFMMGF"] = "";	// 分幅命名规范
				data_source["BSXX_TM"] = "";		// 图名
				data_source["BSXX_TH"] = "";		// 图号
				data_source["BSXX_BLCFM"] = m_dScale;		// 比例尺分母
				data_source["BSXX_SJSZFW"] = "";	// 数据四至范围
				data_source["BSXX_ZBXT"] = m_strVectorLayerCRSName;		// 坐标系统
				data_source["BSXX_GCJZ"] = "";		// 高程基准
				data_source["BSXX_SDJZ"] = "";		// 深度基准
				data_source["BSXX_TQCBZ"] = 0;		// 椭球长半径
				data_source["BSXX_TQBL"] = 0;		// 椭球扁率
				data_source["BSXX_DTTY"] = "";		// 地图投影
				data_source["BSXX_ZYJX"] = 0;		// 中央经线
				data_source["BSXX_BZWX"] = 0;		// 标准纬线
				data_source["BSXX_ZBDW"] = "";		// 坐标单位

				// 质量信息
				data_source["ZLXX_WZX"] = "";		// 完整性
				data_source["ZLXX_SJZL"] = "";		// 数据质量

				// 专题信息
				data_source["ZTXX_CPMC"] = "";		// 产品名称
				data_source["ZTXX_BGDW"] = "";		// 保管单位
				data_source["ZTXX_SCDW"] = "";		// 生产单位
				data_source["ZTXX_SCRQ"] = "";		// 生产日期
				data_source["ZTXX_GXRQ"] = "";		// 更新日期
				data_source["ZTXX_CPMJ"] = "";		// 产品密级
				data_source["ZTXX_CPBB"] = "";		// 产品版本
				data_source["ZTXX_GJZ"] = "";		// 关键字
				data_source["ZTXX_SJL"] = "";		// 数据量
				data_source["ZTXX_SJGS"] = "";		// 数据格式
				data_source["ZTXX_SJLY"] = "";		// 数据来源
				data_source["ZTXX_ZYQH"] = "";		// 作业区号
				data_source["ZTXX_TM"] = "";		// 图名
				data_source["ZTXX_TH"] = "";		// 图号
				data_source["ZTXX_BLCFM"] = m_dScale;		// 比例尺分母
				data_source["ZTXX_ZBXT"] = m_strVectorLayerCRSName;		// 坐标系统
				data_source["ZTXX_GCJZ"] = "";		// 高程基准
				data_source["ZTXX_DTTY"] = "";		// 地图投影
				data_source["ZTXX_DGJ"] = 0;		// 等高距
				data_source["ZTXX_ZQSM"] = "";		// 政区说明

				// 将数据源添加到列表中
				j["data_source_list"].push_back(data_source);


				// 添加 analysis_ready_data
				j["analysis_ready_data"] = ordered_json::array();

				// 根据比例尺计算分析就绪数据格网级别
				int iAnalysisDataLevel = GalAnalysisReadyDataLevelByScale(m_dScale);

				// 打包格网级别
				int iPackLevel = CalPackLevelByDataLevel(iAnalysisDataLevel);

				// 格网级别
				int iGridLevel = iPackLevel;

				// 分析就绪数据级别
				int iDataLevel = iAnalysisDataLevel;
				SE_DRect dTotalRect;
				dTotalRect.dleft = TotalEnvelop.MinX;
				dTotalRect.dright = TotalEnvelop.MaxX;
				dTotalRect.dtop = TotalEnvelop.MaxY;
				dTotalRect.dbottom = TotalEnvelop.MinY;

				int iMinX, iMaxX, iMinY, iMaxY;
				CalXRangeAndYRangeByGeoRectAndLevel(
					dTotalRect,
					iGridLevel,
					iMinX,
					iMaxX,
					iMinY,
					iMaxY);

				ordered_json analysis;
				analysis["grid_level"] = iPackLevel;
				analysis["data_level"] = iDataLevel;
				analysis["data_format"] = "shp";
				analysis["data_type"] = "geometry";
				analysis["crs"] = m_strVectorLayerCRSName;
				analysis["min_x"] = iMinX;
				analysis["max_x"] = iMaxX;
				analysis["min_y"] = iMinY;
				analysis["max_y"] = iMaxY;
				analysis["left"] = dTotalRect.dleft;
				analysis["top"] = dTotalRect.dtop;
				analysis["right"] = dTotalRect.dright;
				analysis["bottom"] = dTotalRect.dbottom;
				analysis["data_source"] = 1;
				analysis["detail"] = "";

				// 将分析数据添加到列表中
				j["analysis_ready_data"].push_back(analysis);


				// 元数据结果保存路径
				string strOutputMetaDataPath = ui.lineEdit_VectorARD_SavePath->text().toLocal8Bit();
				strOutputMetaDataPath += "/vector_metadata.json";

				// 将 JSON 对象写入文件
				std::ofstream file(strOutputMetaDataPath.c_str());
				if (file.is_open())
				{
					// 以 4 个空格缩进格式化输出
					file << j.dump(4);
					file.close();
				}
			}
#pragma endregion
		}
		
	}

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/RasterARDConfigPath"), m_qstrRasterARDConfigPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/VectorARDConfigPath"), m_qstrVectorARDConfigPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/RasterARD_InputDataPath"), m_qstrRasterARD_InputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/VectorARD_InputDataPath"), m_qstrVectorARD_InputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/RasterARDOutputDataPath"), m_qstrRasterARDOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/VectorARDOutputDataPath"), m_qstrVectorARDOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

	// 栅格ARD定制数据源元数据路径
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/InputRasterARDMetadataPath"), m_qstrInputRasterARDMetadataPath, QgsSettings::Section::Plugins);

	// 矢量ARD定制数据源元数据路径
	settings.setValue(QStringLiteral("CUSTOMIZE_ARD/InputVectorARDMetadataPath"), m_qstrInputVectorARDMetadataPath, QgsSettings::Section::Plugins);

}

void SE_CustomizeARDDlg::Button_Cancel_rejected()
{
	reject();
}

void SE_CustomizeARDDlg::radioButton_terrain_factor_clicked()
{
	// 矢量数据格式的单选按钮不可用
	ui.radioButton_shp_raster->setEnabled(false);
	ui.radioButton_gpkg_raster->setEnabled(false);

	if (ui.lineEdit_RasterARD_ConfigFilePath->text().length() > 0)
	{
		ui.lineEdit_RasterARD_ConfigFilePath->clear();
	}


	// 删除所有行
	while (ui.tableWidget_RasterARD->rowCount() > 0)
	{
		ui.tableWidget_RasterARD->removeRow(0);
	}
}

void SE_CustomizeARDDlg::radioButton_rasterize_clicked()
{
	// 矢量数据格式的单选按钮可用
	ui.radioButton_shp_raster->setEnabled(true);
	ui.radioButton_gpkg_raster->setEnabled(true);

	if (ui.lineEdit_RasterARD_ConfigFilePath->text().length() > 0)
	{
		ui.lineEdit_RasterARD_ConfigFilePath->clear();
	}

	// 删除所有行
	while (ui.tableWidget_RasterARD->rowCount() > 0)
	{
		ui.tableWidget_RasterARD->removeRow(0);
	}
}




QStringList SE_CustomizeARDDlg::GetDirPathOfSplDir(QString dirPath)
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
bool SE_CustomizeARDDlg::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_CustomizeARDDlg::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}



QStringList SE_CustomizeARDDlg::GetShpFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.shp" << "*.SHP";
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}

// 获取当前路径下所有shp文件路径
void SE_CustomizeARDDlg::GetShpFilesFromFilePath(
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


void SE_CustomizeARDDlg::UpdateTableWidget(int iType)
{
	// 如果是地形因子
	if (iType == 1)
	{
		// 清空表格内容
		ui.tableWidget_RasterARD->clearContents();

		// 填充图层类型（1）|分析就绪数据产品名称（2）|分析就绪数据产品标识（3）|是否生成（7）
		for (int i = 0; i < m_vTerrainFactorConfig.size(); ++i)
		{
			// 在表格中增加记录
			int iRowCount = 0;
			iRowCount = ui.tableWidget_RasterARD->rowCount();

			// 增加一行
			ui.tableWidget_RasterARD->insertRow(iRowCount);

			// 插入图层类型（1）
			ui.tableWidget_RasterARD->setItem(iRowCount, 1, new QTableWidgetItem(m_vTerrainFactorConfig[i].strLayerType));

			// 插入分析就绪数据产品名称（2）
			ui.tableWidget_RasterARD->setItem(iRowCount, 2, new QTableWidgetItem(m_vTerrainFactorConfig[i].strARDName));

			// 插入分析就绪数据产品标识（3）
			ui.tableWidget_RasterARD->setItem(iRowCount, 3, new QTableWidgetItem(m_vTerrainFactorConfig[i].strARDIdentify));

			// 是否生成为复选框，默认为选中状态
			QTableWidgetItem* item = new QTableWidgetItem();
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);		// 设置为可复选
			item->setCheckState(Qt::Checked);								// 初始状态为选中
			
			// 设置复选框居中
			item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
			ui.tableWidget_RasterARD->setItem(iRowCount, 7, item); 
		
		}


	}

	// 如果是矢量数据栅格化
	else if (iType == 2)
	{
		// 清空表格内容
		ui.tableWidget_RasterARD->clearContents();

		// 填充图层名称（0）|图层类型（1）|分析就绪数据产品名称（2）|分析就绪数据产品标识（3）|栅格化字段名称（4）|比例尺分母（5）|是否生成（7）
		for (int i = 0; i < m_vRasterizeConfig.size(); ++i)
		{
			// 在表格中增加记录
			int iRowCount = 0;
			iRowCount = ui.tableWidget_RasterARD->rowCount();

			// 增加一行
			ui.tableWidget_RasterARD->insertRow(iRowCount);

			// 插入图层名称（0）
			ui.tableWidget_RasterARD->setItem(iRowCount, 0, new QTableWidgetItem(m_vRasterizeConfig[i].strLayerName));

			// 插入图层类型（1）
			ui.tableWidget_RasterARD->setItem(iRowCount, 1, new QTableWidgetItem(m_vRasterizeConfig[i].strLayerType));

			// 插入分析就绪数据产品名称（2）
			ui.tableWidget_RasterARD->setItem(iRowCount, 2, new QTableWidgetItem(m_vRasterizeConfig[i].strARDName));

			// 插入分析就绪数据产品标识（3）
			ui.tableWidget_RasterARD->setItem(iRowCount, 3, new QTableWidgetItem(m_vRasterizeConfig[i].strARDIdentify));

			// 插入栅格化字段名称（4）
			ui.tableWidget_RasterARD->setItem(iRowCount, 4, new QTableWidgetItem(m_vRasterizeConfig[i].strField));

			// 是否生成为复选框，默认为选中状态
			QTableWidgetItem* item = new QTableWidgetItem();
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);		// 设置为可复选
			item->setCheckState(Qt::Checked);								// 初始状态为选中
			// 设置复选框居中
			item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
			ui.tableWidget_RasterARD->setItem(iRowCount, 7, item);
		
		}


	}

	for (int i = 0; i < ui.tableWidget_RasterARD->rowCount(); ++i)
	{
		for (int j = 0; j < ui.tableWidget_RasterARD->columnCount(); ++j)
		{
			if (ui.tableWidget_RasterARD->item(i, j))
			{
				ui.tableWidget_RasterARD->item(i, j)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
			}		
		}
	}

	for (int i = 0; i < ui.tableWidget_RasterARD->columnCount(); ++i)
	{
		// 自适应宽度
		ui.tableWidget_RasterARD->resizeColumnToContents(i);
	}


	// 设置表格中某些列内容不可编辑
	for (int i = 0; i < ui.tableWidget_RasterARD->rowCount(); ++i)
	{
		if (ui.tableWidget_RasterARD->item(i, 1))
		{
			// 图层类型不可编辑
			ui.tableWidget_RasterARD->item(i, 1)->setFlags(Qt::NoItemFlags);
		}
		
		if (ui.tableWidget_RasterARD->item(i, 2))
		{
			// 分析就绪产品名称不可编辑
			ui.tableWidget_RasterARD->item(i, 2)->setFlags(Qt::NoItemFlags);
		}

		if (ui.tableWidget_RasterARD->item(i, 3))
		{		
			// 分析就绪产品标识不可编辑
			ui.tableWidget_RasterARD->item(i, 3)->setFlags(Qt::NoItemFlags);
		}

		if (ui.tableWidget_RasterARD->item(i, 5))
		{
			// DEM分辨率不可编辑
			ui.tableWidget_RasterARD->item(i, 5)->setFlags(Qt::NoItemFlags);
		}
	}
}

/*更新矢量分析就绪数据定制参数表格*/
void SE_CustomizeARDDlg::UpdateVectorARDTableWidget()
{
	// 清空表格内容
	ui.tableWidget_VectorARD->clearContents();

	// 填充图层名称（0）|分析就绪数据产品名称（1）|分析就绪数据产品标识（2）|是否生成（3）
	for (int i = 0; i < m_vVectorARDConfig.size(); ++i)
	{
		// 在表格中增加记录
		int iRowCount = 0;
		iRowCount = ui.tableWidget_VectorARD->rowCount();

		// 增加一行
		ui.tableWidget_VectorARD->insertRow(iRowCount);

		// 插入图层名称（0）
		ui.tableWidget_VectorARD->setItem(iRowCount, 0, new QTableWidgetItem(m_vVectorARDConfig[i].strLayerName));

		// 插入分析就绪数据产品名称（1）
		ui.tableWidget_VectorARD->setItem(iRowCount, 1, new QTableWidgetItem(m_vVectorARDConfig[i].strARDName));

		// 插入分析就绪数据产品标识（2）
		ui.tableWidget_VectorARD->setItem(iRowCount, 2, new QTableWidgetItem(m_vVectorARDConfig[i].strARDIdentify));



		// 是否生成为复选框，默认为选中状态
		QTableWidgetItem* item = new QTableWidgetItem();
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);		// 设置为可复选
		item->setCheckState(Qt::Checked);								// 初始状态为选中
		// 设置复选框居中
		item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_VectorARD->setItem(iRowCount, 4, item);

	}
}

/*调整栅格分析就绪数据定制参数表格*/
void SE_CustomizeARDDlg::AdjustRasterARDTableWidgetStyle()
{
	for (int i = 0; i < ui.tableWidget_RasterARD->rowCount(); ++i)
	{
		for (int j = 0; j < ui.tableWidget_RasterARD->columnCount(); ++j)
		{
			if (ui.tableWidget_RasterARD->item(i, j))
			{
				ui.tableWidget_RasterARD->item(i, j)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
			}
		}
	}

	for (int i = 0; i < ui.tableWidget_RasterARD->columnCount(); ++i)
	{
		// 自适应宽度
		ui.tableWidget_RasterARD->resizeColumnToContents(i);
	}
}

/*调整矢量分析就绪数据定制参数表格*/
void SE_CustomizeARDDlg::AdjustVectorARDTableWidgetStyle()
{
	for (int i = 0; i < ui.tableWidget_VectorARD->rowCount(); ++i)
	{
		for (int j = 0; j < ui.tableWidget_VectorARD->columnCount(); ++j)
		{
			if (ui.tableWidget_VectorARD->item(i, j))
			{
				ui.tableWidget_VectorARD->item(i, j)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
			}
		}
	}

	for (int i = 0; i < ui.tableWidget_VectorARD->columnCount(); ++i)
	{
		// 自适应宽度
		ui.tableWidget_VectorARD->resizeColumnToContents(i);
	}
}

int SE_CustomizeARDDlg::GetIndexFromFileListByLayerName(const QStringList& qstrList, const QString& qstrLayerName)
{
	for (int i = 0; i < qstrList.length(); ++i)
	{
		if (qstrList[i].contains(qstrLayerName))
		{
			return i;
		}
	}
	return -1;
}

void SE_CustomizeARDDlg::CalculateTotalProgress()
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




QString SE_CustomizeARDDlg::GetRasterARDInputDataPath()
{
	return ui.lineEdit_RasterARD_InputDataPath->text();
}

QString SE_CustomizeARDDlg::GetRasterARDOutputDataPath()
{
	return ui.lineEdit_RasterARD_SavePath->text();
}

QString SE_CustomizeARDDlg::GetVectorARDInputDataPath()
{
	return ui.lineEdit_VectorARD_InputDataPath->text();
}

QString SE_CustomizeARDDlg::GetVectorARDOutputDataPath()
{
	return ui.lineEdit_VectorARD_SavePath->text();
}



void SE_CustomizeARDDlg::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void SE_CustomizeARDDlg::pushButton_SaveLog_clicked()
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

void SE_CustomizeARDDlg::pushButton_VectorARD_InputMetadataPath_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择数据源元数据文件");
	QString filter = tr("json 文件(*.json)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	ui.lineEdit_VectorARD_InputMetadataPath->setText(strFileName);
	m_qstrInputVectorARDMetadataPath = strFileName;
}

void SE_CustomizeARDDlg::pushButton_RasterARD_InputMetadataPath_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择数据源元数据文件");
	QString filter = tr("json 文件(*.json)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	ui.lineEdit_RasterARD_InputMetadataPath->setText(strFileName);
	m_qstrInputRasterARDMetadataPath = strFileName;

}


/*根据DEM分辨率获取对应的分析就绪数据格网级别*/
int SE_CustomizeARDDlg::GalAnalysisReadyDataLevelByDemCellSize(double dCellSize)
{
	// 度为单位，0.0008333333333333333325约90米，对应16级
	if (fabs(dCellSize - 0.0008333333333333333325) < 1e-6)
	{
		return 16;
	}

	// 度为单位，0.0002777777777777777775约30米，对应17级
	else if (fabs(dCellSize - 0.0002777777777777777775) < 1e-6)
	{
		return 17;
	}

	// 度为单位，0.0001182721551944400037约12.5米，对应18级
	else if (fabs(dCellSize - 0.0001182721551944400037) < 1e-6)
	{
		return 18;
	}

	// 修改时间：2024-11-25
	// 修改内容：增加10米分辨率的选项
	else if (fabs(dCellSize - 9.01922353e-5) < 1e-6)
	{
		return 19;
	}

	else
	{
		return 17;
	}
}

/*根据级别获取分辨率（地理坐标系，单位：度）*/
double SE_CustomizeARDDlg::GetGridResByGridLevel(int iLevel)
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
}


int SE_CustomizeARDDlg::CalPackLevelByDataLevel(int iDataLevel)
{
	switch (iDataLevel)
	{
	case 14:
		return 7;

	case 15:
		return 8;

	case 16:
		return 9;

	case 17:
		return 10;

	case 18:
		return 11;

	case 19:
		return 12;

	case 20:
		return 13;

	case 21:
		return 14;

	case 22:
		return 15;

	case 23:
		return 16;

	default:
		break;
	}

	return 0;
}


/*根据经纬度范围、打包级别计算横向X取值范围、纵向Y取值范围*/
void SE_CustomizeARDDlg::CalXRangeAndYRangeByGeoRectAndLevel(
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


/*根据矢量数据分辨率获取对应的分析就绪数据格网级别*/
int SE_CustomizeARDDlg::GalAnalysisReadyDataLevelByScale(double dScale)
{
	// 1:5千，对应23级
	if (fabs(dScale - 5000) < 1e-6)
	{
		return 23;
	}

	// 1:1万，对应22级
	else if (fabs(dScale - 10000) < 1e-6)
	{
		return 22;
	}

	// 1:2万，对应21级
	else if (fabs(dScale - 20000) < 1e-6)
	{
		return 21;
	}

	// 1:2.5万，对应20级
	else if (fabs(dScale - 25000) < 1e-6)
	{
		return 20;
	}

	// 1:5万，对应19级
	else if (fabs(dScale - 50000) < 1e-6)
	{
		return 19;
	}

	// 1:10万，对应18级
	else if (fabs(dScale - 100000) < 1e-6)
	{
		return 18;
	}

	// 1:25万，对应17级
	else if (fabs(dScale - 250000) < 1e-6)
	{
		return 17;
	}

	// 1:50万，对应17级
	else if (fabs(dScale - 500000) < 1e-6)
	{
		return 17;
	}

	// 1:100万，对应16级
	else if (fabs(dScale - 1000000) < 1e-6)
	{
		return 16;
	}

	// 1:400万，对应15级
	else if (fabs(dScale - 4000000) < 1e-6)
	{
		return 15;
	}
}



#pragma region  "检查文件或文件夹是否存在"

bool SE_CustomizeARDDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion


bool SE_CustomizeARDDlg::ReadRasterMetaDataJsonFile(const string& strFile, RasterDataMetadataInfo& metaInfo)
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



bool SE_CustomizeARDDlg::ReadVectorMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo)
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
