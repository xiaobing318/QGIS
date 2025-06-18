#define _HAS_STD_BYTE 0

// 使用json头文件
#include <nlohmann/json.hpp>
#include "se_display_statistic_data.h"
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

#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QLegend>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtWidgets/QToolTip>

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

#include <fstream>
#include <iostream>

/*--------------STL---------------*/

#include <QDateTime>


using ordered_json = nlohmann::ordered_json;


using namespace std;

QT_CHARTS_USE_NAMESPACE



SE_DisplayStatisticDataDlg::SE_DisplayStatisticDataDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.Button_OK , &QPushButton::clicked, this, &SE_DisplayStatisticDataDlg::pushButton_OK_clicked);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &SE_DisplayStatisticDataDlg::pushButton_Cancel_clicked);
	connect(ui.pushButton_AddDataPath, &QPushButton::clicked, this, &SE_DisplayStatisticDataDlg::pushButton_AddDataPath);
	connect(ui.pushButton_ClearDataPath, &QPushButton::clicked, this, &SE_DisplayStatisticDataDlg::pushButton_ClearDataPath);

}

SE_DisplayStatisticDataDlg::~SE_DisplayStatisticDataDlg()
{

}

void SE_DisplayStatisticDataDlg::restoreState()
{

}



void SE_DisplayStatisticDataDlg::pushButton_AddDataPath()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择数据包文件所在路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		// 判断是否已经存在同名的路径
		for (int iRow = 0; iRow < ui.tableWidget_InputDataPath->rowCount(); ++iRow)
		{
			QString qstrPath = ui.tableWidget_InputDataPath->item(iRow, 0)->text();

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


		// 添加到ui.tableWidget_InputDataPath中
		int iRow = ui.tableWidget_InputDataPath->rowCount();
		QTableWidgetItem* itemDir = new QTableWidgetItem(selectedDir);
		ui.tableWidget_InputDataPath->insertRow(iRow);
		// 插入路径
		ui.tableWidget_InputDataPath->setItem(iRow, 0, itemDir);

		// 是否选择为复选框，默认为选中状态
		QTableWidgetItem* itemCheck = new QTableWidgetItem();
		itemCheck->setFlags(itemCheck->flags() | Qt::ItemIsUserCheckable);		// 设置为可复选
		itemCheck->setCheckState(Qt::Checked);								// 初始状态为选中

		// 设置复选框居中
		itemCheck->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_InputDataPath->setItem(iRow, 1, itemCheck);



		// 是否通过下载方式获取为复选框，默认为选中状态
		QTableWidgetItem* itemDownloadCheck = new QTableWidgetItem();
		itemDownloadCheck->setFlags(itemDownloadCheck->flags() | Qt::ItemIsUserCheckable);		// 设置为可复选
		itemDownloadCheck->setCheckState(Qt::Checked);								// 初始状态为选中

		// 设置复选框居中
		itemDownloadCheck->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_InputDataPath->setItem(iRow, 2, itemDownloadCheck);

		// 该方法会自动调整表格的列宽，使其能够容纳列中的内容
		ui.tableWidget_InputDataPath->resizeColumnsToContents();
	}
}

void SE_DisplayStatisticDataDlg::pushButton_ClearDataPath()
{
	// 删除所有行
	while (ui.tableWidget_InputDataPath->rowCount() > 0) {
		ui.tableWidget_InputDataPath->removeRow(0);
	}
}


void SE_DisplayStatisticDataDlg::pushButton_OK_clicked()
{
	// 输入路径列表
	vector<string> vInputDataPath;
	vInputDataPath.clear();

	vector<bool> vDownloadFlag;
	vDownloadFlag.clear();

	// 从数据列表中获取路径
	int iRowCount = ui.tableWidget_InputDataPath->rowCount();
	for (int i = 0; i < iRowCount; i++)
	{
		string strTempPath = ui.tableWidget_InputDataPath->item(i, 0)->text().toLocal8Bit();

		// 如果是☑状态，则添加到vInputDataPath中
		if (ui.tableWidget_InputDataPath->item(i, 1)->checkState() == Qt::Checked)
		{
			vInputDataPath.push_back(strTempPath);
			// 如果通过下载方式获取
			if (ui.tableWidget_InputDataPath->item(i, 2)->checkState() == Qt::Checked)
			{
				vDownloadFlag.push_back(true);
			}
			else
			{
				vDownloadFlag.push_back(false);
			}
		}
	}

    // 如果没有选择显示就绪数据路径，则提示用户
	if (vInputDataPath.size() == 0)
	{
		QString qstrTitle = tr("数据包定制及标准化封装工具");
		QString qstrText = tr("请添加数据包路径并设置为选中状态！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();

		return;
	}

	// 遍历路径，并填充到表格控件中
	for (int i = 0; i < vInputDataPath.size(); i++)
	{
		// 添加到ui.tableWidget_Display中
		int iRow = ui.tableWidget_Display->rowCount();
		QTableWidgetItem* itemDir = new QTableWidgetItem(tr("%1").arg(vInputDataPath[i].c_str()));
		ui.tableWidget_Display->insertRow(iRow);
		// 插入路径
		ui.tableWidget_Display->setItem(iRow, 0, itemDir);

		// TODO:数据包类型从json元数据文件中读取
        QTableWidgetItem* itemDataType = new QTableWidgetItem(tr("显示就绪型"));
		itemDataType->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        ui.tableWidget_Display->setItem(iRow, 1, itemDataType);
		
#pragma region "数据量计算"
		// 
		// 检查指定的路径是否是一个有效的目录
		fs::path directory(vInputDataPath[i]);
		if (!fs::is_directory(directory)) 
		{
			//std::cerr << "指定的路径不是一个有效的目录: " << folderPath << std::endl;
			continue;
		}

		// 调用函数进行文件统计
		FileStats stats = countFilesAndSize(directory);
		// 单位GB
		QTableWidgetItem* itemDataVolume = new QTableWidgetItem(tr("%1").arg(stats.totalSize / (1024 * 1024 * 1024 * 1.0)));
		itemDataVolume->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_Display->setItem(iRow, 2, itemDataVolume);


#pragma endregion

#pragma region "条目数"

		// 文件个数
		QTableWidgetItem* itemDataCount = new QTableWidgetItem(tr("%1").arg(stats.fileCount));
		itemDataCount->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_Display->setItem(iRow, 3, itemDataCount);


#pragma endregion

#pragma region "下载量"

		// 如果是通过下载方式获取
		if (vDownloadFlag[i] == true)
		{
			// 单位GB
			QTableWidgetItem* itemDataDownloadVolume = new QTableWidgetItem(tr("%1").arg(stats.totalSize / (1024 * 1024 * 1024 * 1.0)));
			itemDataDownloadVolume->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
			ui.tableWidget_Display->setItem(iRow, 4, itemDataDownloadVolume);
		}
		else
		{
			// 单位GB
			QTableWidgetItem* itemDataDownloadVolume = new QTableWidgetItem(tr("0"));
			itemDataDownloadVolume->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
			ui.tableWidget_Display->setItem(iRow, 4, itemDataDownloadVolume);
		}

#pragma endregion

		// 该方法会自动调整表格的列宽，使其能够容纳列中的内容
		ui.tableWidget_Display->resizeColumnsToContents();
	}


#pragma region "柱状图展示"

// 创建柱状图
	QBarSeries* series = new QBarSeries();

	// 为每个记录创建一个QBarSet
	for (int row = 0; row < ui.tableWidget_Display->rowCount(); ++row) {
		QBarSet* set = new QBarSet(ui.tableWidget_Display->item(row, 0)->text());
		for (int col = 2; col < ui.tableWidget_Display->columnCount(); ++col) {
			set->append(ui.tableWidget_Display->item(row, col)->text().toDouble());
		}
		series->append(set);
	}

	// 创建图表
	QChart* chart = new QChart();
	chart->addSeries(series);
	chart->setTitle("统计信息柱状图");
	chart->setAnimationOptions(QChart::SeriesAnimations);

	// 设置坐标轴
	QStringList categories;
	categories << "数据量（GB）" << "条目数" << "下载量（GB）";
	QBarCategoryAxis* axisX = new QBarCategoryAxis();
	axisX->append(categories);
	chart->addAxis(axisX, Qt::AlignBottom);
	series->attachAxis(axisX);

	QValueAxis* axisY = new QValueAxis();
	chart->addAxis(axisY, Qt::AlignLeft);
	series->attachAxis(axisY);

	// 显示图例
	chart->legend()->setVisible(true);
	chart->legend()->setAlignment(Qt::AlignBottom);

	// 创建图表视图
	QChartView* chartView = new QChartView(chart);
	chartView->setRenderHint(QPainter::Antialiasing);
	ui.gridLayout_Chart->addWidget(chartView);

	// 连接hovered信号到槽函数
	for (auto barSet : series->barSets()) {
		QObject::connect(barSet, &QBarSet::hovered, [barSet](bool status, int index) {
			if (status) {
				double value = barSet->at(index);
				QToolTip::showText(QCursor::pos(), QString::number(value));
			}
			else {
				QToolTip::hideText();
			}
			});
	}

#pragma endregion

}

void SE_DisplayStatisticDataDlg::pushButton_Cancel_clicked()
{
	reject();
}


// 判断vector中是否存在当前元素
bool SE_DisplayStatisticDataDlg::IsExistedInVector(vector<string>& vValues, string strValue)
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
void SE_DisplayStatisticDataDlg::GetSubDirFromFilePath(const string& strFilePath,	vector<string>& vSubDir)
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


// 递归统计文件夹下文件信息的函数
FileStats SE_DisplayStatisticDataDlg::countFilesAndSize(const fs::path& directory) {
	FileStats stats;
	try {
		// 遍历目录中的每个条目
		for (const auto& entry : fs::recursive_directory_iterator(directory)) {
			if (entry.is_regular_file()) {
				// 如果是普通文件，增加文件计数并累加文件大小
				stats.fileCount++;
				stats.totalSize += entry.file_size();
			}
		}
	}
	catch (const fs::filesystem_error& e)
	{
		// 捕获并处理文件系统操作可能出现的错误
		// std::cerr << "文件系统错误: " << e.what() << std::endl;
	}
	return stats;
}
