#define _HAS_STD_BYTE 0
// 使用json头文件
#include <nlohmann/json.hpp>
#include "se_pack_vector_theme_ARD.h"

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

/*内部*/
#include "../ui_task/se_pack_vector_theme_ARD_task.h"

using ordered_json = nlohmann::ordered_json;


using namespace std;

SE_PackVectorThemeARDDlg::SE_PackVectorThemeARDDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.Button_OK , &QPushButton::clicked, this, &SE_PackVectorThemeARDDlg::pushButton_OK_clicked);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &SE_PackVectorThemeARDDlg::pushButton_Cancel_clicked);
	connect(ui.pushButton_OpenMetaDataFile, &QPushButton::clicked, this, &SE_PackVectorThemeARDDlg::pushButton_OpenMetaData_clicked);
	connect(ui.pushButton_OpenRasterARDPath, &QPushButton::clicked, this, &SE_PackVectorThemeARDDlg::pushButton_Open_clicked);
	connect(ui.pushButton_SaveRasterARDPackage, &QPushButton::clicked, this, &SE_PackVectorThemeARDDlg::pushButton_Save_clicked);
	connect(ui.pushButton_Task_Model_Data_Path, &QPushButton::clicked, this, &SE_PackVectorThemeARDDlg::pushButton_OpenConfigFile_clicked);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &SE_PackVectorThemeARDDlg::pushButton_SaveLog_clicked);


	restoreState();

	// 纬度                                              
	ui.lineEdit_Top->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));
	ui.lineEdit_Bottom->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));

	// 经度
	ui.lineEdit_Left->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));
	ui.lineEdit_Right->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));


	ui.lineEdit_InputVectorARDPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputPackagePath->setText(m_qstrOutputDataPath);
	ui.lineEdit_InputMetaDataPath->setText(m_qstrInputMetaDataPath);
	ui.lineEdit_Task_Model_Data_Path->setText(m_qstrConfigFilePath);
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);
}

SE_PackVectorThemeARDDlg::~SE_PackVectorThemeARDDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("PACK_VECTOR_THEME_ARD/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_VECTOR_THEME_ARD/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_VECTOR_THEME_ARD/InputMetaDataPath"), m_qstrInputMetaDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_VECTOR_THEME_ARD/ConfigFilePath"), m_qstrConfigFilePath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_VECTOR_THEME_ARD/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_PackVectorThemeARDDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("PACK_VECTOR_THEME_ARD/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("PACK_VECTOR_THEME_ARD/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrInputMetaDataPath = settings.value(QStringLiteral("PACK_VECTOR_THEME_ARD/InputMetaDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrConfigFilePath = settings.value(QStringLiteral("PACK_VECTOR_THEME_ARD/ConfigFilePath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputLogPath = settings.value(QStringLiteral("PACK_VECTOR_THEME_ARD/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}



void SE_PackVectorThemeARDDlg::pushButton_OpenMetaData_clicked()
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

void SE_PackVectorThemeARDDlg::pushButton_OpenConfigFile_clicked()
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

void SE_PackVectorThemeARDDlg::pushButton_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputDataPath;
	QString dlgTile = tr("请选择专题矢量分析就绪型数据包保存路径");
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

void SE_PackVectorThemeARDDlg::pushButton_OK_clicked()
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
	if (!CheckFileOrDirExist(ui.lineEdit_InputVectorARDPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_InputVectorARDPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


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

	// 输入数据路径
	string strInputPath = ui.lineEdit_InputVectorARDPath->text().toLocal8Bit();

	// 输入元数据路径
	string strInputMetaDataPath = ui.lineEdit_InputMetaDataPath->text().toLocal8Bit();

	// 结果保存路径
	string strOutputPath = ui.lineEdit_OutputPackagePath->text().toLocal8Bit();

	// 获取当前所选任务下的所有分析就绪数据类型
	int iTaskIndex = ui.comboBox_Task->currentIndex();

	THEME_TaskInfo taskInfo = m_vTaskInfos[iTaskIndex];

	vector<string> vARDIdentifys;
	vARDIdentifys.clear();

	// 遍历每个模型
	for (const auto& model : taskInfo.vModelInfos)
	{
		// 遍历每个数据
		for (const auto& ard : model.vARDInfos)
		{
			string strARDIdentify = ard.strARDIdentify.toLocal8Bit();
			if (!IsExistedInVector(vARDIdentifys, strARDIdentify))
			{
				// 分析就绪数据标识集合
				vARDIdentifys.push_back(strARDIdentify);
			}
		}
	}

	// 格网最小级别
	int iMinGridZ = ui.comboBox_MinZ->currentText().toInt();

	// 格网最大级别
	int iMaxGridZ = ui.comboBox_MaxZ->currentText().toInt();

	// 空间范围
	SE_DRect dInputRect;
	dInputRect.dleft = ui.lineEdit_Left->text().toDouble();
	dInputRect.dtop = ui.lineEdit_Top->text().toDouble();
	dInputRect.dright = ui.lineEdit_Right->text().toDouble();
	dInputRect.dbottom = ui.lineEdit_Bottom->text().toDouble();



	// 创建任务
	SE_PackVectorThemeARDTask* task = new SE_PackVectorThemeARDTask(
		tr("专题矢量分析就绪型数据封装"),
		strInputPath,
		vARDIdentifys,
		iMinGridZ,
		iMaxGridZ,
		dInputRect,
		strOutputPath,
		iLogLevel,
		strOutputLogPath);


	QgsApplication::taskManager()->addTask(task);

	connect(task, &SE_PackVectorThemeARDTask::taskFinished, this, &SE_PackVectorThemeARDDlg::onTaskFinished);

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
	for (int i = 0; i < vARDIdentifys.size(); ++i)
	{
		// 如果标识前两位为02开头的，才是矢量分析就绪数据
		string strARDType = vARDIdentifys[i].substr(0, 2);
		if (strARDType != "02")
		{
			continue;
		}

		vectorMetaInfo.strGLXX_CPMC += vARDIdentifys[i];
		vectorMetaInfo.strZTXX_CPMC += vARDIdentifys[i];
		// 除最后一个外，均加"|"分割
		if (i < vARDIdentifys.size() - 1)
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
	string strOutputMetaDataPath = ui.lineEdit_OutputPackagePath->text().toLocal8Bit();
	strOutputMetaDataPath += "/vector_gpkg_metadata.json";
	WriteMetaDataToJsonFile(strOutputMetaDataPath, vectorMetaInfo);

#pragma endregion

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("PACK_VECTOR_THEME_ARD/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_VECTOR_THEME_ARD/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_VECTOR_THEME_ARD/InputMetaDataPath"), m_qstrInputMetaDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_VECTOR_THEME_ARD/ConfigFilePath"), m_qstrConfigFilePath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("PACK_VECTOR_THEME_ARD/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void SE_PackVectorThemeARDDlg::pushButton_Cancel_clicked()
{
	reject();
}

void SE_PackVectorThemeARDDlg::onTaskFinished(bool result)
{
	// 计算并打印总进度
	CalculateTotalProgress();
}

void SE_PackVectorThemeARDDlg::pushButton_SaveLog_clicked()
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

void SE_PackVectorThemeARDDlg::CalculateTotalProgress()
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



void SE_PackVectorThemeARDDlg::pushButton_Open_clicked()
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
		ui.lineEdit_InputVectorARDPath->setText(directory);
	}
	else
	{
		return;
	}
	
}


// 读取属性检查配置信息
bool SE_PackVectorThemeARDDlg::LoadXmlConfigFile(const char* szConfigXmlFile, vector<THEME_TaskInfo>& vTaskInfos)
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
void SE_PackVectorThemeARDDlg::Parse_task(
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
void SE_PackVectorThemeARDDlg::Parse_model(
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
void SE_PackVectorThemeARDDlg::Parse_data(
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
bool SE_PackVectorThemeARDDlg::IsExistedInVector(vector<string>& vValues, string strValue)
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


bool SE_PackVectorThemeARDDlg::ReadMetaDataJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo)
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


void SE_PackVectorThemeARDDlg::WriteMetaDataToJsonFile(const string& strFile, VectorDataMetadataInfo& metaInfo)
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


#pragma region  "检查文件或文件夹是否存在"

bool SE_PackVectorThemeARDDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion