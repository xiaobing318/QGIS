#define _HAS_STD_BYTE 0
#pragma warning(disable: 4005)  // MAX_PATH 重定义
#pragma warning(disable: 4267)  // size_t 到 uint32

/*--------------SE---------------*/
#include "se_merge_raster_ARD_package.h"



/*-------------------------------*/


/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgslayertreeview.h"
#include "qgslayertreemodel.h"
#include "qgslayertree.h"
#include "qgsrasterlayer.h"
#include "qgsmapcanvas.h"
#include "qgsmessagelog.h"
#include "qgsapplication.h"

/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>
#include <qdatetime.h>
#include <qimage.h>
#include <qpainter.h>
#include <qbytearray.h>
#include <qbuffer.h>

#include "../ui_task/se_merge_raster_gpkg_task.h"


CSE_MergeRasterARDPackageDialog::CSE_MergeRasterARDPackageDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);
	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
	
	ui.tableWidget_ARDPath->clearContents();
	
	// 设置自动扩展单元格
	ui.tableWidget_ARDPath->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	
	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_MergeRasterARDPackageDialog::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_MergeRasterARDPackageDialog::Button_Cancel_rejected);
	connect(ui.Button_Save, &QPushButton::clicked, this, &CSE_MergeRasterARDPackageDialog::Button_Save_clicked);
	connect(ui.pushButton_AddARDPackPath, &QPushButton::clicked, this, &CSE_MergeRasterARDPackageDialog::pushButton_AddARDPackPath);
	connect(ui.pushButton_ClearARDPackPath, &QPushButton::clicked, this, &CSE_MergeRasterARDPackageDialog::pushButton_ClearARDPackPath);
	connect(ui.pushButton_Up, &QPushButton::clicked, this, &CSE_MergeRasterARDPackageDialog::pushButton_Up);
	connect(ui.pushButton_Down, &QPushButton::clicked, this, &CSE_MergeRasterARDPackageDialog::pushButton_Down);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &CSE_MergeRasterARDPackageDialog::pushButton_SaveLog_clicked);


	restoreState();

	// 输出路径设置
	ui.lineEdit_OutputPath->setText(m_qstrOutputPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);

	// 设置默认合包策略为“融合”
	ui.radioButton_Fusion->setChecked(true);

	// 设置默认融合规则：最大值
	ui.comboBox_MergeMethod->setCurrentIndex(0);
	
	// 设置说明为红色
	QPalette pe;
	pe.setColor(QPalette::WindowText, Qt::red);
	ui.label_detail->setPalette(pe);
	
}

CSE_MergeRasterARDPackageDialog::~CSE_MergeRasterARDPackageDialog()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("MergeRasterARDPackage/OutputPtpPath"), m_qstrOutputPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("MergeRasterARDPackage/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);


}

void CSE_MergeRasterARDPackageDialog::pushButton_AddARDPackPath()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择栅格分析就绪型数据包所在路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		// 判断是否已经存在同名的路径
		for (int iRow = 0; iRow < ui.tableWidget_ARDPath->rowCount(); ++iRow)
		{
			QString qstrPath = ui.tableWidget_ARDPath->item(iRow, 0)->text();

			// 如果存在同名的
			if (selectedDir == qstrPath)
			{
				QString qstrTitle = tr("栅格分析就绪型数据包合包");
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


		// 添加到ui.tableWidget_ARDPath中
		int iRow = ui.tableWidget_ARDPath->rowCount();
		QTableWidgetItem* itemDir = new QTableWidgetItem(selectedDir);
		ui.tableWidget_ARDPath->insertRow(iRow);
		// 插入路径
		ui.tableWidget_ARDPath->setItem(iRow, 0, itemDir);
	}
}

void CSE_MergeRasterARDPackageDialog::pushButton_ClearARDPackPath()
{
	// 删除所有行
	while (ui.tableWidget_ARDPath->rowCount() > 0) {
		ui.tableWidget_ARDPath->removeRow(0);
	}
}

void CSE_MergeRasterARDPackageDialog::pushButton_Up()
{
	// 获取选中的行
	QList<QTableWidgetItem*> selectedItems = ui.tableWidget_ARDPath->selectedItems();
	if (selectedItems.isEmpty()) 
	{
		QString qstrTitle = tr("栅格分析就绪型数据包合包");
		QString qstrText = tr("请选择要上移的项！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return; 
	}

	// 使用 QSet 来避免重复处理同一行
	QSet<int> rowsToMove;
	for (QTableWidgetItem* item : selectedItems) 
	{
		rowsToMove.insert(item->row());
	}

	// 从高到低处理行，以免影响后续行的索引
	for (int row : rowsToMove.values()) 
	{
		if (row > 0) 
		{ 
			// 确保不是第一行
			for (int column = 0; column < ui.tableWidget_ARDPath->columnCount(); ++column) 
			{
				QTableWidgetItem* currentItem = ui.tableWidget_ARDPath->takeItem(row, column);
				QTableWidgetItem* aboveItem = ui.tableWidget_ARDPath->takeItem(row - 1, column);
				ui.tableWidget_ARDPath->setItem(row - 1, column, currentItem);
				ui.tableWidget_ARDPath->setItem(row, column, aboveItem);
			}
			// 重新选中移动后的行
			ui.tableWidget_ARDPath->selectRow(row - 1);
		}
	}
}

void CSE_MergeRasterARDPackageDialog::pushButton_Down()
{
	// 获取选中的行
	QList<QTableWidgetItem*> selectedItems = ui.tableWidget_ARDPath->selectedItems();
	if (selectedItems.isEmpty()) 
	{
		return; // 如果没有选中行，直接返回
	}

	// 使用 QSet 来避免重复处理同一行
	QSet<int> rowsToMove;
	for (QTableWidgetItem* item : selectedItems) 
	{
		rowsToMove.insert(item->row());
	}

	// 从低到高处理行，以免影响后续行的索引
	for (int row : rowsToMove.values())
	{
		if (row < ui.tableWidget_ARDPath->rowCount() - 1) 
		{ 
			// 确保不是最后一行
			for (int column = 0; column < ui.tableWidget_ARDPath->columnCount(); ++column) 
			{
				QTableWidgetItem* currentItem = ui.tableWidget_ARDPath->takeItem(row, column);
				QTableWidgetItem* belowItem = ui.tableWidget_ARDPath->takeItem(row + 1, column);
				ui.tableWidget_ARDPath->setItem(row + 1, column, currentItem);
				ui.tableWidget_ARDPath->setItem(row, column, belowItem);
			}
			// 重新选中移动后的行
			ui.tableWidget_ARDPath->selectRow(row + 1);
		}
	}
}


void CSE_MergeRasterARDPackageDialog::Button_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputPath;
	QString dlgTile = tr("请选择数据包合包结果数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OutputPath->setText(selectedDir);
		m_qstrOutputPath = selectedDir;
	}
}



/*获取在指定目录下的目录的路径*/
QStringList CSE_MergeRasterARDPackageDialog::GetSubDirPathOfCurrentDir(QString dirPath)
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

void CSE_MergeRasterARDPackageDialog::Button_OK_accepted()
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


	if (!FilePathIsExisted(ui.lineEdit_OutputPath->text()))
	{
		QString qstrTitle = tr("栅格分析就绪型数据包合包");
		QString qstrText = tr("输出数据目录不存在，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 融合
	if (ui.radioButton_Fusion->isChecked())
	{
		m_MergeStrategy = MergePackageStrategy::FUSION;

		// 最大值
		if (ui.comboBox_MergeMethod->currentIndex() == 0)
		{
			m_FusionType = FusionType::MAX;
		}
		// 最小值
		else if (ui.comboBox_MergeMethod->currentIndex() == 1)
		{
			m_FusionType = FusionType::MIN;
		}
		// 平均值
		if (ui.comboBox_MergeMethod->currentIndex() == 2)
		{
			m_FusionType = FusionType::MEAN;
		}
	}
	// 替换
	else 
	{
		m_MergeStrategy = MergePackageStrategy::REPLACE;
	}

	// 从瓦片包数据列表中获取路径
	int iRowCount = ui.tableWidget_ARDPath->rowCount();
	if (iRowCount == 0)
	{
		QString qstrTitle = tr("栅格分析就绪型数据包合包");
		QString qstrText = tr("请添加数据包路径！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();

		return;
	}
	else if (iRowCount == 1)
	{
		QString qstrTitle = tr("栅格分析就绪型数据包合包");
		QString qstrText = tr("栅格分析就绪数据包合包至少需要两个数据包路径，请添加数据包路径！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 输入gpkg数据包所在路径列表
	vector<string> vInputGpkgPath;
	vInputGpkgPath.clear();

	// 数据路径列表，记录gpkg文件所在路径
	for (int i = 0; i < iRowCount; i++)
	{
		string strTempPath = ui.tableWidget_ARDPath->item(i, 0)->text().toLocal8Bit();
		vInputGpkgPath.push_back(strTempPath);
	}

	// 输出时间
	QDateTime curDateTime = QDateTime::currentDateTime();
	QString qstrDataTime = curDateTime.toString("yyyy-MM-dd hh:mm:ss");
	ui.lineEdit_DateTime->setText(qstrDataTime);
	string strDataTime = qstrDataTime.toLocal8Bit();

	// 输出路径
	string strOutputPath = ui.lineEdit_OutputPath->text().toLocal8Bit();

	// 创建任务
	SE_MergeRasterGpkgTask* task = new SE_MergeRasterGpkgTask(
		tr("栅格分析就绪型数据包合包"),
		vInputGpkgPath,
		m_MergeStrategy,
		m_FusionType,
		strDataTime,
		strOutputPath,
		iLogLevel,
		strOutputLogPath);

	QgsApplication::taskManager()->addTask(task);

	connect(task, &SE_MergeRasterGpkgTask::taskFinished, this, &CSE_MergeRasterARDPackageDialog::onTaskFinished);

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("MergeRasterARDPackage/OutputDataPath"), m_qstrOutputPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("MergeRasterARDPackage/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void CSE_MergeRasterARDPackageDialog::Button_Cancel_rejected()
{
	reject();
}


void CSE_MergeRasterARDPackageDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrOutputPath = settings.value(QStringLiteral("MergeRasterARDPackage/OutputPtpPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputLogPath = settings.value(QStringLiteral("MergeRasterARDPackage/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();


}

// 判断目录是否存在
bool CSE_MergeRasterARDPackageDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_MergeRasterARDPackageDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


QString CSE_MergeRasterARDPackageDialog::GetOutputPath()
{
	return ui.lineEdit_OutputPath->text();
}

// 获取指定目录的最后一级目录
void CSE_MergeRasterARDPackageDialog::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}

/*获取指定目录的最后一级目录*/
void CSE_MergeRasterARDPackageDialog::GetFolderNameFromPath_string(string strPath, string& strFolderName)
{
	// 获取图幅文件夹名称
	int iIndex = strPath.find_last_of("/");
	if (iIndex != string::npos)
	{
		strFolderName = strPath.substr(iIndex + 1, strPath.length() - 1);
	}
	else
	{
		strFolderName = "";
	}
}




void CSE_MergeRasterARDPackageDialog::GetPackageInfo(string strPtpFileName,
	int &minz,
	int &maxz,
	int &basez,
	int &x,
	int &y)
{
	QString qstrPtpFileName = QString::fromLocal8Bit(strPtpFileName.c_str());

	QStringList qstrInfoList = qstrPtpFileName.split("_");

	minz = qstrInfoList[0].toInt();
	maxz = qstrInfoList[1].toInt();
	basez = qstrInfoList[2].toInt();
	x = qstrInfoList[3].toInt();
	y = qstrInfoList[4].toInt();
	
}


bool CSE_MergeRasterARDPackageDialog::IsExistedInVector(string strValue, vector<string>& vValue)
{
	for (int i = 0; i < vValue.size(); ++i)
	{
		if (strValue == vValue[i])
		{
			return true;
		}
	}

	return false;
}



bool CSE_MergeRasterARDPackageDialog::isDirExist(QString fullPath)
{
	QDir dir(fullPath);
	if (dir.exists())
	{
		return true;
	}
	else
	{
		bool ok = dir.mkpath(fullPath);//创建多级目录
		return ok;
	}
}


void CSE_MergeRasterARDPackageDialog::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void CSE_MergeRasterARDPackageDialog::pushButton_SaveLog_clicked()
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

void CSE_MergeRasterARDPackageDialog::CalculateTotalProgress()
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


#pragma region  "检查文件或文件夹是否存在"

bool CSE_MergeRasterARDPackageDialog::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion