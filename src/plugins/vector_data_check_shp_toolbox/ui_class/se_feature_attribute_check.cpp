
/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"
/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>
#include "se_feature_attribute_check.h"

/*-------------------------------*/



#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#endif

typedef map<string, string> MAP_STRING_2_STRING;

CSE_FeatureAttributeCheckDialog::CSE_FeatureAttributeCheckDialog(QgisInterface* qgisInterface, QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl), 
	m_qgisInterface(qgisInterface)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_SaveLog, &QPushButton::clicked, this, &CSE_FeatureAttributeCheckDialog::Button_Save_clicked);
	connect(ui.Button_OpenXml, &QPushButton::clicked, this, &CSE_FeatureAttributeCheckDialog::Button_OpenXml_clicked);
	connect(ui.Button_OpenShp, &QPushButton::clicked, this, &CSE_FeatureAttributeCheckDialog::Button_OpenShp_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_FeatureAttributeCheckDialog::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_FeatureAttributeCheckDialog::Button_Cancel_rejected);

	connect(ui.lineEdit_InputShpDataPath, &QLineEdit::textChanged, this, &CSE_FeatureAttributeCheckDialog::on_lineEdit_InputShpDataPath_TextChanged);
	connect(ui.lineEdit_InputXmlPath, &QLineEdit::textChanged, this, &CSE_FeatureAttributeCheckDialog::on_lineEdit_InputXmlPath_TextChanged);
	connect(ui.lineEdit_OutputLogPath, &QLineEdit::textChanged, this, &CSE_FeatureAttributeCheckDialog::on_lineEdit_OutputLogPath_TextChanged);
	restoreState();
	// 初始化输入路径和输出路径
	ui.lineEdit_InputXmlPath->setText(m_qstrInputXmlPath);
	ui.lineEdit_InputShpDataPath->setText(m_qstrInputShpDataPath);
	ui.lineEdit_OutputLogPath->setText(m_qstrSaveLogPath);

	connect(&m_Thread, &SE_FeatureAttributeThread::resultProcess, this, &CSE_FeatureAttributeCheckDialog::handleResults);
}

CSE_FeatureAttributeCheckDialog::~CSE_FeatureAttributeCheckDialog()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("FeatureAttributeCheck/InputXmlPath"), m_qstrInputXmlPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("FeatureAttributeCheck/InputShpDataPath"), m_qstrInputShpDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("FeatureAttributeCheck/SaveLogPath"), m_qstrSaveLogPath, QgsSettings::Section::Plugins);
}

void CSE_FeatureAttributeCheckDialog::on_lineEdit_InputShpDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_FeatureAttributeCheckDialog::on_lineEdit_InputXmlPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_FeatureAttributeCheckDialog::on_lineEdit_OutputLogPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_FeatureAttributeCheckDialog::handleResults(const double& dPercent, const QString& s)
{
	
	if (dPercent == 1)
	{
		ui.progressBar->setValue(int(100 * dPercent));
		ui.Button_OK->setEnabled(true);
		QString qstrTitle = QString::fromLocal8Bit("线划图要素属性检查");
		QString qstrText = s;
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("FeatureAttributeCheck/InputXmlPath"), m_qstrInputXmlPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("FeatureAttributeCheck/InputShpDataPath"), m_qstrInputShpDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("FeatureAttributeCheck/SaveLogPath"), m_qstrSaveLogPath, QgsSettings::Section::Plugins);

	}
	else if (dPercent == 0)
	{
		QString qstrTitle = QString::fromLocal8Bit("线划图要素属性检查");
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


void CSE_FeatureAttributeCheckDialog::Button_Save_clicked()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = QString::fromLocal8Bit("请选择日志文件存储路径");
	QString filter = QString::fromLocal8Bit("txt 文件(*.txt)");
	QString outputLogFilePath = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);

	if (!outputLogFilePath.isEmpty())
	{
		ui.lineEdit_OutputLogPath->setText(outputLogFilePath);
		m_qstrSaveLogPath = outputLogFilePath;
	}


}

void CSE_FeatureAttributeCheckDialog::Button_OpenXml_clicked()
{
	// 选择xml属性配置文件
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = QString::fromLocal8Bit("请选择xml属性检查配置文件");
	QString filter = QString::fromLocal8Bit("xml 文件(*.xml)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}
	else
	{
		ui.lineEdit_InputXmlPath->setText(strFileName);
		m_qstrInputXmlPath = strFileName;
	}

}

void CSE_FeatureAttributeCheckDialog::Button_OpenShp_clicked()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = QString::fromLocal8Bit("请选择shp数据路径");
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
QStringList CSE_FeatureAttributeCheckDialog::GetSubDirPathOfCurrentDir(QString dirPath)
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

void CSE_FeatureAttributeCheckDialog::Button_OK_accepted()
{
	ui.progressBar->reset();

	m_Thread.SetThreadParams(ui.lineEdit_InputShpDataPath->text(),
		ui.lineEdit_InputXmlPath->text(),
		ui.lineEdit_OutputLogPath->text());

	ui.Button_OK->setEnabled(false);

	/* TO BE DELETE

	// 点击开始检查后，将按钮置为不可用，直到程序执行结束
	ui.Button_OK->setEnabled(false);


	m_qstrSaveLogPath = GetOutputLogPath();
	string strOutputLogPath = m_qstrSaveLogPath.toLocal8Bit();
	FILE* fpLog = fopen(strOutputLogPath.c_str(), "w");
	if (!fpLog)
	{
		QString qstrTitle = QString::fromLocal8Bit("线划图要素属性检查");
		QString qstrText = QString::fromLocal8Bit("创建日志文件失败！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}

	fprintf(fpLog, "=============================要素属性检查记录=============================\n");
	fflush(fpLog);


	ui.progressBar->reset();

	m_qstrInputXmlPath = ui.lineEdit_InputXmlPath ->text();
	if (!FileIsExisted(m_qstrInputXmlPath))
	{
		QString qstrTitle = QString::fromLocal8Bit("线划图要素属性检查");
		QString qstrText = QString::fromLocal8Bit("输入xml文件不存在，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}

	string strInputXmlPath = m_qstrInputXmlPath.toLocal8Bit();

	// shp数据路径
	m_qstrInputShpDataPath = ui.lineEdit_InputShpDataPath->text();
	if (!FilePathIsExisted(m_qstrInputShpDataPath))
	{
		QString qstrTitle = QString::fromLocal8Bit("线划图要素属性检查");
		QString qstrText = QString::fromLocal8Bit("输入shp数据目录不存在，请重新输入！");
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
		QString qstrTitle = QString::fromLocal8Bit("线划图要素属性检查");
		QString qstrText = QString::fromLocal8Bit("输出日志文件路径不合法，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}
	
	// 获取shp目录下的文件夹个数
	QStringList qstrShpSubDir = GetSubDirPathOfCurrentDir(m_qstrInputShpDataPath);

	QgsStatusBar* sb = m_qgisInterface->statusBarIface();
	sb->activateWindow();

	// 如果当前输入shp目录为单个图幅
	if ( qstrShpSubDir.size() == 0)
	{
		string strInputShpDataPath = m_qstrInputShpDataPath.toLocal8Bit();

		// 获取shp图幅号
		string strShpSheet;
		GetFolderNameFromPath_string(strInputShpDataPath, strShpSheet);

		SE_Error err = SE_ERROR_FAILURE;

		vector<VectorAttributeCheckInfo> vLayerAttrCheckInfo;
		vLayerAttrCheckInfo.clear();

		err = FeatureAttributeCheck(
			strInputShpDataPath.c_str(),
			strInputXmlPath.c_str(),
			vLayerAttrCheckInfo);
		

		if (err != SE_ERROR_NONE)
		{
			QString qstrTitle = QString::fromLocal8Bit("线划图要素属性检查 ");
			char szText[500] = { 0 };
			sprintf(szText, "线划图要素属性检查错误，错误代码：%d", err);
			QString qstrText = QString::fromLocal8Bit(szText);
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
			fprintf(fpLog, "\n					当前文件夹图幅数：1 \n ");
			fflush(fpLog);

			fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
			fflush(fpLog);

			fprintf(fpLog, "					第1个图幅%s要素属性检查信息\n", strShpSheet.c_str());
			fflush(fpLog);

			fprintf(fpLog, "\n	图幅号		图层名称		属性字段		属性检查错误信息\n");
			fflush(fpLog);

			for (int i = 0; i < vLayerAttrCheckInfo.size(); i++)
			{
				for (int j = 0; j < vLayerAttrCheckInfo[i].vFieldAttributeErrorList.size(); j++)
				{
					for (int k = 0; k < vLayerAttrCheckInfo[i].vFieldAttributeErrorList[j].vStrFieldAttrCheckError.size(); k++)
					{

						fprintf(fpLog, "%s		%s			%s			%s\n",
							strShpSheet.c_str(),
							vLayerAttrCheckInfo[i].strLayerType.c_str(),
							vLayerAttrCheckInfo[i].vFieldAttributeErrorList[j].strFieldName.c_str(),
							vLayerAttrCheckInfo[i].vFieldAttributeErrorList[j].vStrFieldAttrCheckError[k].c_str());
						
						fflush(fpLog);
					}
				}
				
			}

			fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
			fflush(fpLog);

			fprintf(fpLog, "\n==========================================================================\n");
			fflush(fpLog);
			fclose(fpLog);

			ui.progressBar->setValue(100);
			
			QString qstrTitle = QString::fromLocal8Bit("线划图要素属性检查");
			QString qstrText = QString::fromLocal8Bit("线划图要素属性检查完成！");
			sb->showMessage(qstrText);
			QMessageBox msgBox;
			msgBox.setWindowTitle(qstrTitle);
			msgBox.setText(qstrText);
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			sb->clearMessage();
			// 设置当前保存路径
			QgsSettings settings;
			settings.setValue(QStringLiteral("FeatureAttributeCheck/InputXmlPath"), m_qstrInputXmlPath, QgsSettings::Section::Plugins);
			settings.setValue(QStringLiteral("FeatureAttributeCheck/InputShpDataPath"), m_qstrInputShpDataPath, QgsSettings::Section::Plugins);
			settings.setValue(QStringLiteral("FeatureAttributeCheck/SaveLogPath"), m_qstrSaveLogPath, QgsSettings::Section::Plugins);

			ui.Button_OK->setEnabled(true);
			return;
		}

	}

	// 如果当前输入shp目录为多个图幅目录
	else
	{
		fprintf(fpLog, "\n					当前文件夹图幅数：%d \n ",qstrShpSubDir.size());
		fflush(fpLog);

		for (int iIndex = 0; iIndex < qstrShpSubDir.size(); iIndex++)
		{
			QByteArray qInputShpPath = qstrShpSubDir[iIndex].toLocal8Bit();
			string strInputShpDataPath = string(qInputShpPath);

			// 获取指定目录的最后一级目录
			// 获取shp图幅号
			string strShpSheet;
			GetFolderNameFromPath_string(strInputShpDataPath, strShpSheet);


			SE_Error err = SE_ERROR_FAILURE;

			vector<VectorAttributeCheckInfo> vLayerAttrCheckInfo;
			vLayerAttrCheckInfo.clear();

			err = CSE_VectorDataCheck::FeatureAttributeCheck(
				strInputShpDataPath.c_str(),
				strInputXmlPath.c_str(),
				vLayerAttrCheckInfo);


			if (err != SE_ERROR_NONE)
			{
				QString qstrTitle = QString::fromLocal8Bit("线划图要素属性检查");
				char szText[500] = { 0 };
				sprintf(szText, "线划图要素属性检查错误，错误代码：%d", err);
				QString qstrText = QString::fromLocal8Bit(szText);
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

				fprintf(fpLog, "					第%d个图幅%s要素属性检查信息\n", iIndex + 1, strShpSheet.c_str());
				fflush(fpLog);

				fprintf(fpLog, "\n	图幅号		图层名称		属性字段		属性检查错误信息\n");
				fflush(fpLog);

				for (int i = 0; i < vLayerAttrCheckInfo.size(); i++)
				{
					for (int j = 0; j < vLayerAttrCheckInfo[i].vFieldAttributeErrorList.size(); j++)
					{
						for (int k = 0; k < vLayerAttrCheckInfo[i].vFieldAttributeErrorList[j].vStrFieldAttrCheckError.size(); k++)
						{
							fprintf(fpLog, "%s		%s			%s			%s\n",
								strShpSheet.c_str(),
								vLayerAttrCheckInfo[i].strLayerType.c_str(),
								vLayerAttrCheckInfo[i].vFieldAttributeErrorList[j].strFieldName.c_str(),
								vLayerAttrCheckInfo[i].vFieldAttributeErrorList[j].vStrFieldAttrCheckError[k].c_str());

							fflush(fpLog);
						}
					}

				}

				fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
				fflush(fpLog);

				ui.progressBar->setValue(int((iIndex + 1) * 100.0 / qstrShpSubDir.size()));
			}
		}

		fprintf(fpLog, "\n==========================================================================\n");
		fflush(fpLog);
		fclose(fpLog);

		
		QString qstrTitle = QString::fromLocal8Bit("线划图要素属性检查");
		QString qstrText = QString::fromLocal8Bit("线划图要素属性检查完成！");
		sb->showMessage(qstrText);
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		sb->clearMessage();
		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("FeatureAttributeCheck/InputXmlPath"), m_qstrInputXmlPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("FeatureAttributeCheck/InputShpDataPath"), m_qstrInputShpDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("FeatureAttributeCheck/SaveLogPath"), m_qstrSaveLogPath, QgsSettings::Section::Plugins);

		ui.Button_OK->setEnabled(true);
		return;
	}*/
}

void CSE_FeatureAttributeCheckDialog::Button_Cancel_rejected()
{
	reject();
}

void CSE_FeatureAttributeCheckDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrInputXmlPath = settings.value(QStringLiteral("FeatureAttributeCheck/InputXmlPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrInputShpDataPath = settings.value(QStringLiteral("FeatureAttributeCheck/InputShpDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrSaveLogPath = settings.value(QStringLiteral("FeatureAttributeCheck/SaveLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}

// 判断目录是否存在
bool CSE_FeatureAttributeCheckDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_FeatureAttributeCheckDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


QString CSE_FeatureAttributeCheckDialog::GetInputXmlPath()
{
	return ui.lineEdit_InputXmlPath->text();
}

QString CSE_FeatureAttributeCheckDialog::GetInputShpDataPath()
{
	return ui.lineEdit_InputShpDataPath->text();
}

QString CSE_FeatureAttributeCheckDialog::GetOutputLogPath()
{
	return ui.lineEdit_OutputLogPath->text();
}

// 获取指定目录的最后一级目录
void CSE_FeatureAttributeCheckDialog::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}

/*获取指定目录的最后一级目录*/
void CSE_FeatureAttributeCheckDialog::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


int CSE_FeatureAttributeCheckDialog::GBK_2_UTF8(char* utfstr, const char* srcstr, int maxutfstrlen)
{
	if (NULL == srcstr)
	{
		printf("bad parameter\n");
		return -1;
	}
	//首先先将gbk编码转换为unicode编码
	if (NULL == setlocale(LC_ALL, "zh_CN.gbk"))//设置转换为unicode前的码,当前为gbk编码
	{
		printf("bad parameter\n");
		return -1;
	}
	int unicodelen = mbstowcs(NULL, srcstr, 0);//计算转换后的长度
	if (unicodelen <= 0)
	{
		printf("can not transfer!!!\n");
		return -1;
	}
	wchar_t* unicodestr = (wchar_t*)calloc(sizeof(wchar_t), unicodelen + 1);
	mbstowcs(unicodestr, srcstr, strlen(srcstr));//将gbk转换为unicode

												 //将unicode编码转换为utf8编码
	if (NULL == setlocale(LC_ALL, "zh_CN.utf8"))//设置unicode转换后的码,当前为utf8
	{
		printf("bad parameter\n");
		return -1;
	}
	int utflen = wcstombs(NULL, unicodestr, 0);//计算转换后的长度
	if (utflen <= 0)
	{
		printf("can not transfer!!!\n");
		return -1;
	}
	else if (utflen >= maxutfstrlen)//判断空间是否足够
	{
		printf("dst str memory not enough\n");
		return -1;
	}
	wcstombs(utfstr, unicodestr, utflen);
	utfstr[utflen] = 0;//添加结束符
	free(unicodestr);
	return utflen;
}



SE_Error CSE_FeatureAttributeCheckDialog::FeatureAttributeCheck(
	const char* szInputShpPath,
	const char* szAttrCheckConfigXmlFile,
	vector<VectorAttributeCheckInfo>& vFeatureAttrCheckInfo)
{
	// 如果输入图层列表为空
	if (!szInputShpPath)
	{
		return SE_ERROR_FILEPATH_IS_INVALID;
	}

	vFeatureAttrCheckInfo.clear();

	// 循环读取输入图层，如果读取不成功，提示错误消息
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(szInputShpPath, GDAL_OF_VECTOR, NULL, NULL, NULL);

	// 文件不存在或打开失败
	if (poDS == nullptr)
	{
		return SE_ERROR_OPEN_SHAPEFILE_FAILED;
	}

	// 获取图层数量
	int iLayerCount = poDS->GetLayerCount();
	if (iLayerCount == 0)
	{
		return SE_ERROR_SHP_FILE_COUNT_IS_ZERO;
	}

	// 所有图层属性字段检查配置信息
	vector<VectorLayerFieldInfo> vLayerAttrConfigInfo;
	vLayerAttrConfigInfo.clear();

	// 读取属性检查配置文件
	bool bResult = LoadFeatureAttrCheckXmlConfigFile(szAttrCheckConfigXmlFile, vLayerAttrConfigInfo);
	if (!bResult)
	{
		return SE_ERROR_OPEN_VECTOR_LAYER_ATTRIBUTE_CONFIGFILE_FAILED;
	}

	// 循环读取输入图层，如果读取不成功，提示错误消息
	for (int iIndex = 0; iIndex < iLayerCount; iIndex++)
	{
		// 输出当前图层属性检查结果
		VectorAttributeCheckInfo outputCheckInfo;

		OGRLayer* poLayer = poDS->GetLayer(iIndex);
		if (poLayer == nullptr)
		{
			return SE_ERROR_OGRLAYER_IS_NULL;
		}

		// 获取图幅号、要素层、几何类型
		string strSheetNumber;
		string strLayerType;
		string strGeometryType;
		GetLayerInfo(poLayer->GetName(), strSheetNumber, strLayerType, strGeometryType);

		// 元数据图层跳过
		if (strLayerType == "S")
		{
			continue;
		}

		outputCheckInfo.strLayerType = strLayerType;

		// 获取对应图层类型的字段及属性检查配置信息
		VectorLayerFieldInfo sLayerFieldCheckInfo;
		GetAttributeCheckInfoByLayerType(
			strLayerType,
			vLayerAttrConfigInfo,
			sLayerFieldCheckInfo);

		// 获取图层属性字段
		vector<LayerFieldInfo> vLayerFieldInfo;
		vLayerFieldInfo.clear();

		OGRFeatureDefn* pFeatureDefn = poLayer->GetLayerDefn();
		int iFieldCount = pFeatureDefn->GetFieldCount();
		for (int iFieldIndex = 0; iFieldIndex < iFieldCount; iFieldIndex++)
		{
			OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iFieldIndex);
			LayerFieldInfo sFieldInfo;
			sFieldInfo.strFieldName = pField->GetNameRef();
			sFieldInfo.iFieldLength = pField->GetWidth();

			// 整型
			if (pField->GetType() == OFTInteger
				|| pField->GetType() == OFTInteger64)
			{
				sFieldInfo.strFieldType = "int";
			}

			// 浮点型
			else if (pField->GetType() == OFTReal)
			{
				sFieldInfo.strFieldType = "float";
			}

			// 字符型
			else if (pField->GetType() == OFTString)
			{
				sFieldInfo.strFieldType = "string";
			}

			vLayerFieldInfo.push_back(sFieldInfo);
		}


		// 图层字段检查
		VectorAttributeCheckInfo outputFieldInfoCheck;
		outputFieldInfoCheck.strLayerType = strLayerType;
		LayerFieldInfoCheck(
			vLayerFieldInfo,
			sLayerFieldCheckInfo,
			outputFieldInfoCheck.vFieldAttributeErrorList);

		// 判断图层类型是否已经存在
		int iLayerTypeIndex = -1;
		for (int i = 0; i < vFeatureAttrCheckInfo.size(); i++)
		{
			// 如果已经存在同类型图层，则在图层类型后追加属性检查错误记录
			if (vFeatureAttrCheckInfo[i].strLayerType == strLayerType)
			{
				iLayerTypeIndex = i;
			}
		}

		// 说明已经存在同名图层
		if (iLayerTypeIndex != -1)
		{
			for (int i = 0; i < outputCheckInfo.vFieldAttributeErrorList.size(); i++)
			{
				vFeatureAttrCheckInfo[iLayerTypeIndex].vFieldAttributeErrorList.push_back(outputFieldInfoCheck.vFieldAttributeErrorList[i]);
			}
		}
		else
		{
			vFeatureAttrCheckInfo.push_back(outputFieldInfoCheck);
		}



		// 重置要素读取顺序
		poLayer->ResetReading();
		OGRFeature* poFeature = nullptr;

		// 循环判断每个要素
		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			// 要素属性值
			MAP_STRING_2_STRING mapFieldValue;
			mapFieldValue.clear();

			// 要素字段信息
			vector<LayerFieldInfo> vLayerFieldInfo;
			vLayerFieldInfo.clear();

			// 获取要素的属性字段信息及属性值
			GetFeatureAttr(poFeature, mapFieldValue, vLayerFieldInfo);

			// 检查属性字段的有效性
			LayerFieldAttrCheck(
				mapFieldValue,
				sLayerFieldCheckInfo,
				outputCheckInfo.vFieldAttributeErrorList);

			// 判断图层类型是否已经存在
			int iLayerIndex = -1;
			for (int i = 0; i < vFeatureAttrCheckInfo.size(); i++)
			{
				// 如果已经存在同类型图层，则在图层类型后追加属性检查错误记录
				if (vFeatureAttrCheckInfo[i].strLayerType == strLayerType)
				{
					iLayerIndex = i;
				}
			}

			// 说明已经存在同名图层
			if (iLayerIndex != -1)
			{
				for (int i = 0; i < outputCheckInfo.vFieldAttributeErrorList.size(); i++)
				{
					vFeatureAttrCheckInfo[iLayerIndex].vFieldAttributeErrorList.push_back(outputCheckInfo.vFieldAttributeErrorList[i]);
				}
			}
			else
			{
				vFeatureAttrCheckInfo.push_back(outputCheckInfo);
			}

			OGRFeature::DestroyFeature(poFeature);
		}
	}

	// 关闭数据源
	GDALClose(poDS);

	return SE_ERROR_NONE;
}


// 读取属性检查配置信息
bool CSE_FeatureAttributeCheckDialog::LoadFeatureAttrCheckXmlConfigFile(const char* szAttrCheckConfigXmlFile, vector<VectorLayerFieldInfo>& vLayerConfigFieldInfo)
{
	// 如果xml文件为空
	if (!szAttrCheckConfigXmlFile)
	{
		return false;
	}

	vLayerConfigFieldInfo.clear();

	// 读取xml文件
	xmlDocPtr doc = nullptr;
	xmlNodePtr pRootNode = nullptr;

	// 属性检查项配置文件
	doc = xmlParseFile(szAttrCheckConfigXmlFile);

	if (nullptr == doc)
	{
		return false;
	}

	// 获取根节点<data_attribute_check>
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
		// data_spec节点
		if (!xmlStrcmp(cur->name, (const xmlChar*)"data_spec"))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strDataSpec = UTF8_To_GBK((char*)key);
			xmlFree(key);
		}

		// data_format节点
		else if (!xmlStrcmp(cur->name, (const xmlChar*)"data_format"))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strDataFormat = UTF8_To_GBK((char*)key);
			xmlFree(key);
		}

		// layer_fields节点，从A-R图层
		else if (!xmlStrcmp(cur->name, (const xmlChar*)"layer_fields"))
		{
			// layer_fields节点
			VectorLayerFieldInfo info;
			Parse_layer_fields(doc, cur, info);
			vLayerConfigFieldInfo.push_back(info);
		}


		cur = cur->next;
	}

	xmlFreeDoc(doc);

	return true;
}


/*UTF-8转GBK*/
string CSE_FeatureAttributeCheckDialog::UTF8_To_GBK(const string& str)
{
	int nwLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);

	wchar_t* pwBuf = new wchar_t[nwLen + 1];
	memset(pwBuf, 0, nwLen * 2 + 2);

	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = WideCharToMultiByte(CP_ACP, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

	char* pBuf = new char[nLen + 1];
	memset(pBuf, 0, nLen + 1);

	WideCharToMultiByte(CP_ACP, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr = pBuf;

	delete[]pBuf;
	delete[]pwBuf;

	pBuf = NULL;
	pwBuf = NULL;

	return retStr;
}


void CSE_FeatureAttributeCheckDialog::GetLayerInfo(string strLayerName,
	string& strSheetNumber,
	string& strLayerType,
	string& strGeometryType)
{
	int iIndexOfSheet = strLayerName.find_first_of("_");
	strSheetNumber = strLayerName.substr(0, iIndexOfSheet);
	int iIndexOfLayerType = strLayerName.find_last_of("_");
	strLayerType = strLayerName.substr(iIndexOfSheet + 1, 1);
	int iIndexOfExt = strLayerName.find(".");
	strGeometryType = strLayerName.substr(iIndexOfLayerType + 1, iIndexOfExt - (iIndexOfLayerType + 1));
}


// 解析每个layer_fields节点
void CSE_FeatureAttributeCheckDialog::Parse_layer_fields(
	xmlDocPtr doc,
	xmlNodePtr cur,
	VectorLayerFieldInfo& info)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != NULL)
	{
		// layer_name：图层名称
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"layer_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strLayerName = UTF8_To_GBK((char*)key);
			info.strLayerType = strLayerName;
			xmlFree(key);
		}

		// fields：字段列表
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"fields")))
		{
			FieldInfo field_info;
			Parse_fields(doc, cur, field_info);
			info.vLayerFieldInfo.push_back(field_info);
		}
		cur = cur->next;
	}
}


// 解析每个fields节点
void CSE_FeatureAttributeCheckDialog::Parse_fields(
	xmlDocPtr doc,
	xmlNodePtr cur,
	FieldInfo& info)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// field_name：字段名称
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strFieldName = UTF8_To_GBK((char*)key);
			info.strFieldName = strFieldName;
			xmlFree(key);
		}

		// field_type：字段类型
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_type")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strFieldType = (char*)key;
			xmlFree(key);
		}

		// field_length：字段长度
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_length")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.iFieldLength = atoi((char*)key);
			xmlFree(key);
		}

		// field_enum_values_list_simple：简单字段属性枚举值
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values_list_simple")))
		{
			// 遍历field_enum_values_list_simple节点		
			Parse_field_enum_values_list_simple(doc, cur, info.vSimpleEnumFieldValue);
		}

		// field_enum_values_list_complex：复杂字段属性枚举值
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values_list_complex")))
		{
			// 遍历field_enum_values_list_complex节点		
			Parse_field_enum_values_list_complex(doc, cur, info.vComplexEnumFieldValue);
		}

		cur = cur->next;
	}
}


// 解析每个field_enum_values_list_simple节点
void CSE_FeatureAttributeCheckDialog::Parse_field_enum_values_list_simple(
	xmlDocPtr doc,
	xmlNodePtr cur,
	vector<string>& vSimpleEnumFieldValue)
{
	vSimpleEnumFieldValue.clear();

	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// field_enum_values
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			string strEnumValues = UTF8_To_GBK((char*)key);
			vSimpleEnumFieldValue.push_back(strEnumValues);
			xmlFree(key);
		}
		cur = cur->next;
	}
}

// 解析每个field_enum_values_list_complex节点
void CSE_FeatureAttributeCheckDialog::Parse_field_enum_values_list_complex(
	xmlDocPtr doc,
	xmlNodePtr cur,
	vector<FieldEnumValue>& vComplexEnumFieldValue)
{
	vComplexEnumFieldValue.clear();


	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// field_enum_values
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_values")))
		{
			FieldEnumValue enumValue;
			// 解析field_enum_values节点
			Parse_field_enum_values(doc, cur, enumValue);
			vComplexEnumFieldValue.push_back(enumValue);
		}

		cur = cur->next;

	}
}


// 解析每个field_enum_values节点
void CSE_FeatureAttributeCheckDialog::Parse_field_enum_values(
	xmlDocPtr doc,
	xmlNodePtr cur,
	FieldEnumValue& enumValue)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// field_enum_value，字段属性值
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_value")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key)
			{
				string strEnumValue = UTF8_To_GBK((char*)key);
				enumValue.strFieldEnumValue = strEnumValue;
				xmlFree(key);
			}
			
		}

		// field_enum_name，字段属性值
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"field_enum_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key)
			{
				string strEnumName = UTF8_To_GBK((char*)key);
				enumValue.vStrFieldEnumName.push_back(strEnumName);
				xmlFree(key);
			}

		}
		cur = cur->next;
	}
}


void CSE_FeatureAttributeCheckDialog::GetAttributeCheckInfoByLayerType(
	string strLayerType,
	vector<VectorLayerFieldInfo>& vLayersFieldInfo,
	VectorLayerFieldInfo& sLayerFieldInfo)
{
	for (int i = 0; i < vLayersFieldInfo.size(); i++)
	{
		if (vLayersFieldInfo[i].strLayerType == strLayerType)
		{
			sLayerFieldInfo = vLayersFieldInfo[i];
		}
	}
}


void CSE_FeatureAttributeCheckDialog::LayerFieldInfoCheck(
	vector<LayerFieldInfo>& vLayerFieldInfo,
	VectorLayerFieldInfo& sLayerFieldCheckInfo,
	vector<VectorFieldAttrCheckError>& vAttributeErrorList)
{
	// 属性检查错误记录
	vAttributeErrorList.clear();

	// 检查属性字段是否在检查列表中
	for (int i = 0; i < vLayerFieldInfo.size(); i++)
	{
		VectorFieldAttrCheckError fieldCheckError;

		LayerFieldInfo field = vLayerFieldInfo[i];

		// 字段名
		bool bFieldExistedFlag_Name = false;
		for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
		{		
			if (field.strFieldName == sLayerFieldCheckInfo.vLayerFieldInfo[j].strFieldName)
			{
				bFieldExistedFlag_Name = true;
				break;
			}
		}

		// 字段类型
		bool bFieldExistedFlag_Type = false;
		for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
		{
			if (bFieldExistedFlag_Name && field.strFieldType == sLayerFieldCheckInfo.vLayerFieldInfo[j].strFieldType)
			{
				bFieldExistedFlag_Type = true;
				break;
			}
		}

		// 字段长度
		bool bFieldExistedFlag_Length = false;
		for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
		{
			if (bFieldExistedFlag_Name && field.iFieldLength == sLayerFieldCheckInfo.vLayerFieldInfo[j].iFieldLength)
			{
				bFieldExistedFlag_Length = true;
				break;
			}
		}

		if (bFieldExistedFlag_Name && bFieldExistedFlag_Type && bFieldExistedFlag_Length)
		{
			continue;
		}

		string strTotal;
		// 如果字段名称不符合要求
		if (!bFieldExistedFlag_Name)
		{
			char szError[500] = { 0 };
			string strFieldNameError;
			sprintf(szError, " %s图层中字段“%s”名称不规范\t", sLayerFieldCheckInfo.strLayerType.c_str(), field.strFieldName.c_str());
			strFieldNameError = szError;
			strTotal += strFieldNameError;
		}

		// 如果字段类型不符合要求
		if (!bFieldExistedFlag_Type)
		{
			char szError[500] = { 0 };
			string strFieldTypeError;
			sprintf(szError, " %s图层中字段[%s]类型“%s”不规范\t", sLayerFieldCheckInfo.strLayerType.c_str(), field.strFieldType.c_str(), field.strFieldType.c_str());
			strFieldTypeError = szError;
			strTotal += strFieldTypeError;
		}

		// 如果字段长度不符合要求
		if (!bFieldExistedFlag_Length)
		{
			char szError[500] = { 0 };
			string strFieldLengthError;
			sprintf(szError, " %s图层中字段[%s]长度“%d”不规范\t", sLayerFieldCheckInfo.strLayerType.c_str(), field.strFieldType.c_str(), field.iFieldLength);
			strFieldLengthError = szError;

			strTotal += strFieldLengthError;
		}

		fieldCheckError.strFieldName = field.strFieldName;
		fieldCheckError.vStrFieldAttrCheckError.push_back(strTotal);
		vAttributeErrorList.push_back(fieldCheckError);
	}
	
}


// 获取要素属性信息
SE_Error CSE_FeatureAttributeCheckDialog::GetFeatureAttr(
	OGRFeature* poFeature,
	map<string, string>& mFieldValue,
	vector<LayerFieldInfo>& vLayerFieldInfo)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));

		LayerFieldInfo sFieldInfo;
		sFieldInfo.strFieldName = strFieldName;
		sFieldInfo.iFieldLength = pField->GetWidth();

		// 整型
		if (pField->GetType() == OFTInteger
			|| pField->GetType() == OFTInteger64)
		{
			sFieldInfo.strFieldType = "int";
		}

		// 浮点型
		else if (pField->GetType() == OFTReal)
		{
			sFieldInfo.strFieldType = "float";
		}

		// 字符型
		else if (pField->GetType() == OFTString)
		{
			sFieldInfo.strFieldType = "string";
		}

		vLayerFieldInfo.push_back(sFieldInfo);
	}

	return SE_ERROR_NONE;
}


void CSE_FeatureAttributeCheckDialog::LayerFieldAttrCheck(
	map<string, string>& mapFieldValue,
	VectorLayerFieldInfo& sLayerFieldCheckInfo,
	vector<VectorFieldAttrCheckError>& vAttributeErrorList)
{
	// 属性检查错误记录
	vAttributeErrorList.clear();

	// 检查属性值是否在检查列表有效范围内
	map<string, string>::iterator iter;
	for (iter = mapFieldValue.begin(); iter != mapFieldValue.end(); iter++)
	{
		VectorFieldAttrCheckError fieldCheckError;
		string strFieldName = iter->first;
		string strFieldValue = iter->second;
		fieldCheckError.strFieldName = strFieldName;

		
		for (int j = 0; j < sLayerFieldCheckInfo.vLayerFieldInfo.size(); j++)
		{
			FieldInfo info = sLayerFieldCheckInfo.vLayerFieldInfo[j];

			if (strFieldName == info.strFieldName)
			{
				// 如果是简单属性枚举值
				if (info.vSimpleEnumFieldValue.size() > 0)
				{
					// 如果不在枚举属性列表中
					if (!FieldValueIsExistedInSimpleEnumList(info.vSimpleEnumFieldValue, strFieldValue))
					{
						char szError[500] = { 0 };
						sprintf(szError, "%s图层中字段[%s]的属性值“%s”不规范", sLayerFieldCheckInfo.strLayerType.c_str(), strFieldName.c_str(), strFieldValue.c_str());
						fieldCheckError.vStrFieldAttrCheckError.push_back(szError);
						vAttributeErrorList.push_back(fieldCheckError);
					}
				}

				// 如果是复杂属性枚举值
				if (info.vComplexEnumFieldValue.size() > 0)
				{
					// 获取编码属性值对应的枚举值
					string strCode;
					map<string, string>::iterator iterCode = mapFieldValue.find("编码");
					if (iterCode != mapFieldValue.end())
					{
						strCode = iterCode->second;
					}

					// 复杂枚举值
					vector<string> vComplexEnumName;
					vComplexEnumName.clear();
					for (int k = 0; k < info.vComplexEnumFieldValue.size(); k++)
					{
						if (strCode == info.vComplexEnumFieldValue[k].strFieldEnumValue)
						{
							vComplexEnumName = info.vComplexEnumFieldValue[k].vStrFieldEnumName;
						}
					}

					// 如果枚举属性列表不为空，才需要判断属性是否规范
					if (vComplexEnumName.size() == 0)
					{
						continue;
					}


					// 如果不在枚举属性列表中
					if (!FieldValueIsExistedInSimpleEnumList(vComplexEnumName, strFieldValue))
					{
						char szError[500] = { 0 };
						sprintf(szError, "%s图层中字段[%s]的属性值“%s”不规范", sLayerFieldCheckInfo.strLayerType.c_str(), strFieldName.c_str(), strFieldValue.c_str());
						fieldCheckError.vStrFieldAttrCheckError.push_back(szError);
						vAttributeErrorList.push_back(fieldCheckError);
					}
				}
			}
		}
	}
}


bool CSE_FeatureAttributeCheckDialog::FieldValueIsExistedInSimpleEnumList(
	vector<string>& vSimpleEnumValue,
	string strValue)
{
	for (int i = 0; i < vSimpleEnumValue.size(); i++)
	{
		if (strValue == vSimpleEnumValue[i])
		{
			return true;
		}
	}

	return false;
}