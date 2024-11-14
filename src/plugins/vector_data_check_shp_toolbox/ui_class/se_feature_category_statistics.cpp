
/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"
/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>
#include "se_feature_category_statistics.h"

/*-------------------------------*/

CSE_FeatureCategoryStatisticsDialog::CSE_FeatureCategoryStatisticsDialog(QgisInterface* qgisInterface, QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl),
	m_qgisInterface(qgisInterface)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_SaveLog, &QPushButton::clicked, this, &CSE_FeatureCategoryStatisticsDialog::Button_Save_clicked);
	connect(ui.Button_OpenOdata, &QPushButton::clicked, this, &CSE_FeatureCategoryStatisticsDialog::Button_OpenOdata_clicked);
	connect(ui.Button_OpenShp, &QPushButton::clicked, this, &CSE_FeatureCategoryStatisticsDialog::Button_OpenShp_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_FeatureCategoryStatisticsDialog::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_FeatureCategoryStatisticsDialog::Button_Cancel_rejected);


	connect(ui.lineEdit_InputOdataDataPath, &QLineEdit::textChanged, this, &CSE_FeatureCategoryStatisticsDialog::on_lineEdit_InputOdataDataPath_TextChanged);
	connect(ui.lineEdit_InputShpDataPath, &QLineEdit::textChanged, this, &CSE_FeatureCategoryStatisticsDialog::on_lineEdit_InputShpDataPath_TextChanged);
	connect(ui.lineEdit_OutputLogPath, &QLineEdit::textChanged, this, &CSE_FeatureCategoryStatisticsDialog::on_lineEdit_OutputLogPath_TextChanged);


	restoreState();
	// 初始化输入路径和输出路径
	ui.lineEdit_InputOdataDataPath->setText(m_qstrInputOdataDataPath);
	ui.lineEdit_InputShpDataPath->setText(m_qstrInputShpDataPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrSaveLogPath);

	connect(&m_Thread, &SE_FeatureCategoryThread::resultProcess, this, &CSE_FeatureCategoryStatisticsDialog::handleResults);
}

CSE_FeatureCategoryStatisticsDialog::~CSE_FeatureCategoryStatisticsDialog()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("FeatureCategoryStatistics/InputOdataDataPath"), m_qstrInputOdataDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("FeatureCategoryStatistics/InputShpDataPath"), m_qstrInputShpDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("FeatureCategoryStatistics/SaveLogPath"), m_qstrSaveLogPath, QgsSettings::Section::Plugins);
}


void CSE_FeatureCategoryStatisticsDialog::Button_Save_clicked()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("请选择日志文件存储路径");
	QString filter = tr("txt 文件(*.txt)"); 
	QString outputLogFilePath = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);

	if (!outputLogFilePath.isEmpty())
	{
		ui.lineEdit_OutputLogPath->setText(outputLogFilePath);
		m_qstrSaveLogPath = outputLogFilePath;
	}


}

void CSE_FeatureCategoryStatisticsDialog::Button_OpenOdata_clicked()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择odata数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputOdataDataPath->setText(selectedDir);
		m_qstrInputOdataDataPath = selectedDir;		
	}
}

void CSE_FeatureCategoryStatisticsDialog::Button_OpenShp_clicked()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择shp数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputShpDataPath->setText(selectedDir);
		m_qstrInputShpDataPath = selectedDir;
	}
}

/*获取在指定目录下的目录的路径*/
QStringList CSE_FeatureCategoryStatisticsDialog::GetSubDirPathOfCurrentDir(QString dirPath)
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

void CSE_FeatureCategoryStatisticsDialog::Button_OK_accepted()
{
	ui.progressBar->reset();

	m_Thread.SetThreadParams(
		ui.lineEdit_InputOdataDataPath->text(),
		ui.lineEdit_InputShpDataPath->text(),
		ui.lineEdit_OutputLogPath->text());

	// 点击开始统计后，将按钮置为不可用，直到程序执行结束
	ui.Button_OK->setEnabled(false);

/*
	m_qstrSaveLogPath = GetOutputLogPath();
	QByteArray qOutputLogPath = m_qstrSaveLogPath.toLocal8Bit();
	string strOutputLogPath = string(qOutputLogPath);
	FILE* fpLog = fopen(strOutputLogPath.c_str(), "w");
	if (!fpLog)
	{
		QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("创建日志文件失败！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}

	fprintf(fpLog, "=============================要素分类统计记录=============================\n");
	fflush(fpLog);


	ui.progressBar->reset();

	m_qstrInputOdataDataPath = ui.lineEdit_InputOdataDataPath ->text();
	if (!FilePathIsExisted(m_qstrInputOdataDataPath))
	{
		QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("输入odata数据目录不存在，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}

	m_qstrInputShpDataPath = ui.lineEdit_InputShpDataPath->text();
	if (!FilePathIsExisted(m_qstrInputShpDataPath))
	{
		QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("输入shp数据目录不存在，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}

	// 日志文件路径
	m_qstrSaveLogPath = ui.lineEdit_OutputLogPath->text();
	if (!FileIsExisted(m_qstrSaveLogPath))
	{
		QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("输出日志文件路径不合法，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}
	
	// 获取odata目录下的文件夹个数
	QStringList qstrOdataSubDir = GetSubDirPathOfCurrentDir(m_qstrInputOdataDataPath);

	// 获取shp目录下的文件夹个数
	QStringList qstrShpSubDir = GetSubDirPathOfCurrentDir(m_qstrInputShpDataPath);

	// 如果输入odata目录的子文件夹数和输入shp目录的子文件夹数不同
	if (qstrOdataSubDir.size() != qstrShpSubDir.size())
	{
		QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("odata目录下文件夹数目和shp目录下文件夹数目不一致，请重新选择输入数据！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}

	QgsStatusBar* sb = m_qgisInterface->statusBarIface();
	sb->activateWindow();

	// 如果当前输入odata目录为单个图幅
	if (qstrOdataSubDir.size() == 0 
		&& qstrShpSubDir.size() == 0)
	{
		QByteArray qInputOdataPath = m_qstrInputOdataDataPath.toLocal8Bit();
		string strInputOdataDataPath =  string(qInputOdataPath);

		QByteArray qInputShpPath = m_qstrInputShpDataPath.toLocal8Bit();
		string strInputShpDataPath = string(qInputShpPath);
	
		// 获取指定目录的最后一级目录
		// 获取odata图幅号
		string strOdataSheet;
		GetFolderNameFromPath_string(strInputOdataDataPath, strOdataSheet);

		// 获取shp图幅号
		string strShpSheet;
		GetFolderNameFromPath_string(strInputShpDataPath, strShpSheet);

		if (strOdataSheet != strShpSheet)
		{
			QString qstrTitle = tr("线划图要素分类统计");
			QString qstrText = tr("当前选择的odata数据图幅号与shp数据图幅号不相同，请重新选择数据！");
			QMessageBox msgBox;
			msgBox.setWindowTitle(qstrTitle);
			msgBox.setText(qstrText);
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			ui.Button_OK->setEnabled(true);
			return;
		}

		SE_Error err = SE_ERROR_FAILURE;

		vector<VectorLayerInfo> vLayerInfo;
		vLayerInfo.clear();

		err = CSE_VectorDataCheck::FeatureCategoryStatistics(strInputOdataDataPath.c_str(),
				strInputShpDataPath.c_str(),
				vLayerInfo);
		

		if (err != SE_ERROR_NONE)
		{
			QString qstrTitle = tr("线划图要素分类统计");
			char szText[500] = { 0 };
			sprintf(szText, "线划图要素分类统计错误，错误代码：%d", err);
			QString qstrText = tr(szText);
			QMessageBox msgBox;
			msgBox.setWindowTitle(qstrTitle);
			msgBox.setText(qstrText);
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			ui.Button_OK->setEnabled(true);
			return;
		}
		else
		{
			fprintf(fpLog, "\n					当前文件夹图幅数：1个\n");
			fflush(fpLog);

			fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
			fflush(fpLog);

			fprintf(fpLog, "					第1个图幅%s统计信息\n", strOdataSheet.c_str());
			fflush(fpLog);

			fprintf(fpLog, "\n	图幅号		图层名称	几何类型	要素个数	与元数据对比情况\n");
			fflush(fpLog);

			for (int i = 0; i < vLayerInfo.size(); i++)
			{
				// 如果shp数据点要素个数与元数据一致
				if (vLayerInfo[i].iPointCount == vLayerInfo[i].iPointCount_SMS)
				{
					fprintf(fpLog, "	%s		%s		point			%d			1\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iPointCount);
					fflush(fpLog);
				}
				else
				{
					fprintf(fpLog, "	%s		%s		point			%d			0:元数据中为%d个\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iPointCount,
						vLayerInfo[i].iPointCount_SMS);
					fflush(fpLog);
				}

				// 如果shp数据线要素个数与元数据一致
				if (vLayerInfo[i].iLineCount == vLayerInfo[i].iLineCount_SMS)
				{
					fprintf(fpLog, "	%s		%s		line			%d			1\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iLineCount);
				}
				else
				{
					fprintf(fpLog, "	%s		%s		line			%d			0:元数据中为%d个\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iLineCount,
						vLayerInfo[i].iLineCount_SMS);
					fflush(fpLog);
				}

				// 如果shp数据面要素个数与元数据一致
				if (vLayerInfo[i].iPolygonCount == vLayerInfo[i].iPolygonCount_SMS)
				{
					fprintf(fpLog, "	%s		%s		polygon			%d			1\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iPolygonCount);
				}
				else
				{
					fprintf(fpLog, "	%s		%s		polygon			%d			0:元数据中为%d个\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iPolygonCount,
						vLayerInfo[i].iPolygonCount_SMS);
					fflush(fpLog);
				}
			}

			fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
			fflush(fpLog);

			fprintf(fpLog, "\n==========================================================================\n");
			fflush(fpLog);
			fclose(fpLog);

			ui.progressBar->setValue(100);
			QString qstrTitle = tr("线划图要素分类统计");
			QString qstrText = tr("线划图要素分类统计完成！");
			QMessageBox msgBox;
			msgBox.setWindowTitle(qstrTitle);
			msgBox.setText(qstrText);
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();

			// 设置当前保存路径
			QgsSettings settings;
			settings.setValue(QStringLiteral("FeatureCategoryStatistics/InputOdataDataPath"), m_qstrInputOdataDataPath, QgsSettings::Section::Plugins);
			settings.setValue(QStringLiteral("FeatureCategoryStatistics/InputShpDataPath"), m_qstrInputShpDataPath, QgsSettings::Section::Plugins);
			settings.setValue(QStringLiteral("FeatureCategoryStatistics/SaveLogPath"), m_qstrSaveLogPath, QgsSettings::Section::Plugins);

			ui.Button_OK->setEnabled(true);

			return;
		}

	}

	// 如果当前输入odata目录为多个图幅目录
	else
	{
		fprintf(fpLog, "\n					当前文件夹图幅数：%d个\n",qstrOdataSubDir.size());
		fflush(fpLog);

		for (int iIndex = 0; iIndex < qstrOdataSubDir.size(); iIndex++)
		{
			QByteArray qInputOdataPath = qstrOdataSubDir[iIndex].toLocal8Bit();
			string strInputOdataDataPath = string(qInputOdataPath);

			QByteArray qInputShpPath = qstrShpSubDir[iIndex].toLocal8Bit();
			string strInputShpDataPath = string(qInputShpPath);

			// 获取指定目录的最后一级目录
			// 获取odata图幅号
			string strOdataSheet;
			GetFolderNameFromPath_string(strInputOdataDataPath, strOdataSheet);

			// 获取shp图幅号
			string strShpSheet;
			GetFolderNameFromPath_string(strInputShpDataPath, strShpSheet);

			if (strOdataSheet != strShpSheet)
			{
				QString qstrTitle = tr("线划图要素分类统计");
				QString qstrText = tr("当前选择的odata数据图幅号与shp数据图幅号不相同，请重新选择数据！");
				QMessageBox msgBox;
				msgBox.setWindowTitle(qstrTitle);
				msgBox.setText(qstrText);
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				ui.Button_OK->setEnabled(true);
				return;
			}

			SE_Error err = SE_ERROR_FAILURE;

			vector<VectorLayerInfo> vLayerInfo;
			vLayerInfo.clear();

			err = CSE_VectorDataCheck::FeatureCategoryStatistics(strInputOdataDataPath.c_str(),
				strInputShpDataPath.c_str(),
				vLayerInfo);


			if (err != SE_ERROR_NONE)
			{
				QString qstrTitle = tr("线划图要素分类统计");
				char szText[500] = { 0 };
				sprintf(szText, "线划图要素分类统计错误，错误代码：%d", err);
				QString qstrText = tr(szText);
				QMessageBox msgBox;
				msgBox.setWindowTitle(qstrTitle);
				msgBox.setText(qstrText);
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				ui.Button_OK->setEnabled(true);
				return;
			}
			else
			{
				fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
				fflush(fpLog);

				fprintf(fpLog, "					第%d个图幅%s统计信息\n", iIndex + 1, strOdataSheet.c_str());
				fflush(fpLog);

				fprintf(fpLog, "\n	图幅号		图层名称	几何类型	要素个数	与元数据对比情况\n");
				fflush(fpLog);

				for (int i = 0; i < vLayerInfo.size(); i++)
				{
					// 如果shp数据点要素个数与元数据一致
					if (vLayerInfo[i].iPointCount == vLayerInfo[i].iPointCount_SMS)
					{
						fprintf(fpLog, "	%s		%s		point			%d			1\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iPointCount);
						fflush(fpLog);
					}
					else
					{
						fprintf(fpLog, "	%s		%s		point			%d			0:元数据中为%d个\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iPointCount,
							vLayerInfo[i].iPointCount_SMS);
						fflush(fpLog);
					}

					// 如果shp数据线要素个数与元数据一致
					if (vLayerInfo[i].iLineCount == vLayerInfo[i].iLineCount_SMS)
					{
						fprintf(fpLog, "	%s		%s		line			%d			1\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iLineCount);
					}
					else
					{
						fprintf(fpLog, "	%s		%s		line			%d			0:元数据中为%d个\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iLineCount,
							vLayerInfo[i].iLineCount_SMS);
						fflush(fpLog);
					}

					// 如果shp数据面要素个数与元数据一致
					if (vLayerInfo[i].iPolygonCount == vLayerInfo[i].iPolygonCount_SMS)
					{
						fprintf(fpLog, "	%s		%s		polygon			%d			1\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iPolygonCount);
					}
					else
					{
						fprintf(fpLog, "	%s		%s		polygon			%d			0:元数据中为%d个\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iPolygonCount,
							vLayerInfo[i].iPolygonCount_SMS);
						fflush(fpLog);
					}
				}

				fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
				fflush(fpLog);

				

				ui.progressBar->setValue(int((iIndex + 1) * 100.0 / qstrOdataSubDir.size()));

			}
		}

		fprintf(fpLog, "\n==========================================================================\n");
		fflush(fpLog);
		fclose(fpLog);
		sb->showMessage(tr("线划图要素分类统计完成！"));
		QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("线划图要素分类统计完成！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		sb->clearMessage();
		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("FeatureCategoryStatistics/InputOdataDataPath"), m_qstrInputOdataDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("FeatureCategoryStatistics/InputShpDataPath"), m_qstrInputShpDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("FeatureCategoryStatistics/SaveLogPath"), m_qstrSaveLogPath, QgsSettings::Section::Plugins);

		ui.Button_OK->setEnabled(true);
		return;
	}

	*/

}

void CSE_FeatureCategoryStatisticsDialog::Button_Cancel_rejected()
{
	reject();
}

void CSE_FeatureCategoryStatisticsDialog::on_lineEdit_InputOdataDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_FeatureCategoryStatisticsDialog::on_lineEdit_InputShpDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_FeatureCategoryStatisticsDialog::on_lineEdit_OutputLogPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_FeatureCategoryStatisticsDialog::handleResults(const double& dPercent, const QString& s)
{
	if (dPercent == 1)
	{
		ui.progressBar->setValue(int(100 * dPercent));
		ui.Button_OK->setEnabled(true);
		QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = s;
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("FeatureCategoryStatistics/InputOdataDataPath"), m_qstrInputOdataDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("FeatureCategoryStatistics/InputShpDataPath"), m_qstrInputShpDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("FeatureCategoryStatistics/SaveLogPath"), m_qstrSaveLogPath, QgsSettings::Section::Plugins);
	}
	else if (dPercent == 0)
	{
		QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = s;
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.progressBar->setValue(0);
		ui.Button_OK->setEnabled(true);
		return;
	}
	else
	{
		ui.progressBar->setValue(int(100 * dPercent));
	}

}

void CSE_FeatureCategoryStatisticsDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrInputOdataDataPath = settings.value(QStringLiteral("FeatureCategoryStatistics/InputOdataDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrInputShpDataPath = settings.value(QStringLiteral("FeatureCategoryStatistics/InputShpDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrSaveLogPath = settings.value(QStringLiteral("FeatureCategoryStatistics/SaveLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}

// 判断目录是否存在
bool CSE_FeatureCategoryStatisticsDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_FeatureCategoryStatisticsDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


QString CSE_FeatureCategoryStatisticsDialog::GetInputOdataDataPath()
{
	return ui.lineEdit_InputOdataDataPath->text();
}

QString CSE_FeatureCategoryStatisticsDialog::GetInputShpDataPath()
{
	return ui.lineEdit_InputShpDataPath->text();
}

QString CSE_FeatureCategoryStatisticsDialog::GetOutputLogPath()
{
	return ui.lineEdit_OutputLogPath->text();
}

// 获取指定目录的最后一级目录
void CSE_FeatureCategoryStatisticsDialog::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}

/*获取指定目录的最后一级目录*/
void CSE_FeatureCategoryStatisticsDialog::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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