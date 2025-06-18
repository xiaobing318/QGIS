#define _HAS_STD_BYTE 0
#pragma warning(disable: 4005)  // MAX_PATH 重定义
#pragma warning(disable: 4267)  // size_t 到 uint32
// 使用json头文件
#include <nlohmann/json.hpp>
/*--------------SE---------------*/
#include "se_relate_source_data.h"
#include "commontype/se_analysis_ready_data_tool_def.h"
#include <filesystem>
#include <fstream>
#include <iostream>
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

#include "iconv.h"

using ordered_json = nlohmann::ordered_json;

CSE_RelateSourceDataDialog::CSE_RelateSourceDataDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);
	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
	
	ui.tableWidget_SourceDataPath->clearContents();

	ui.tableWidget_AttributesSetting->clearContents();

	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_RelateSourceDataDialog::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_RelateSourceDataDialog::Button_Cancel_rejected);
	connect(ui.Button_SaveRelatedResult, &QPushButton::clicked, this, &CSE_RelateSourceDataDialog::pushButton_SaveResult);
	connect(ui.pushButton_AddSourceDataPath, &QPushButton::clicked, this, &CSE_RelateSourceDataDialog::pushButton_AddSourceDataPath);
	connect(ui.pushButton_ClearSourceDataPath, &QPushButton::clicked, this, &CSE_RelateSourceDataDialog::pushButton_ClearSourceDataPath);
	connect(ui.Button_SaveLogFilePath, &QPushButton::clicked, this, &CSE_RelateSourceDataDialog::pushButton_SaveLog_clicked);


	restoreState();

	// 纬度                                              
	ui.lineEdit_Top->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));
	ui.lineEdit_Bottom->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));

	// 经度
	ui.lineEdit_Left->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));
	ui.lineEdit_Right->setValidator(new QRegExpValidator(QRegExp("^(?!.*[a-zA-Z])[-+]?[0-9]*\.?[0-9]+$")));


	// 输出路径设置
	ui.lineEdit_OutputLogPath->setText(m_qstrOutputLogPath);

	// 设置说明为红色
	QPalette pe;
	pe.setColor(QPalette::WindowText, Qt::red);
	ui.label_detail->setPalette(pe);

	// 该方法会自动调整表格的列宽，使其能够容纳列中的内容
	ui.tableWidget_SourceDataPath->resizeColumnsToContents();
	ui.tableWidget_AttributesSetting->resizeColumnsToContents();

	// 更新属性设置表格
	UpdateAttributesSettingTableWidget();

}

CSE_RelateSourceDataDialog::~CSE_RelateSourceDataDialog()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("RelateSourceData/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

}

void CSE_RelateSourceDataDialog::pushButton_AddSourceDataPath()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择原始数据文件所在路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		// 判断是否已经存在同名的路径
		for (int iRow = 0; iRow < ui.tableWidget_SourceDataPath->rowCount(); ++iRow)
		{
			QString qstrPath = ui.tableWidget_SourceDataPath->item(iRow, 0)->text();

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


		// 添加到ui.tableWidget_SourceDataPath中
		int iRow = ui.tableWidget_SourceDataPath->rowCount();
		QTableWidgetItem* itemDir = new QTableWidgetItem(selectedDir);
		ui.tableWidget_SourceDataPath->insertRow(iRow);
		// 插入路径
		ui.tableWidget_SourceDataPath->setItem(iRow, 0, itemDir);

		// 是否生成为复选框，默认为选中状态
		QTableWidgetItem* itemCheck = new QTableWidgetItem();
		itemCheck->setFlags(itemCheck->flags() | Qt::ItemIsUserCheckable);		// 设置为可复选
		itemCheck->setCheckState(Qt::Checked);								// 初始状态为选中

		// 设置复选框居中
		itemCheck->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_SourceDataPath->setItem(iRow, 1, itemCheck);

		// 该方法会自动调整表格的列宽，使其能够容纳列中的内容
		ui.tableWidget_SourceDataPath->resizeColumnsToContents();
	}
}

void CSE_RelateSourceDataDialog::pushButton_ClearSourceDataPath()
{
	// 删除所有行
	while (ui.tableWidget_SourceDataPath->rowCount() > 0) {
		ui.tableWidget_SourceDataPath->removeRow(0);
	}
}


void CSE_RelateSourceDataDialog::pushButton_SaveResult()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("保存关联结果文件");
	QString filter = tr("txt 文件(*.txt)");
	QString qstrFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!qstrFileName.isEmpty())
	{
		ui.lineEdit_OutputRelatedResultPath->setText(qstrFileName);
		m_qstrOutputPath = qstrFileName;
	}
}



/*获取在指定目录下的目录的路径*/
QStringList CSE_RelateSourceDataDialog::GetSubDirPathOfCurrentDir(QString dirPath)
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

void CSE_RelateSourceDataDialog::Button_OK_accepted()
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

	// 起始时间（生产日期）
	QString qstrStartDataTime = ui.dateEdit_Start->date().toString("yyyyMMdd");

	// 终止时间（生产日期）
	QString qstrEndDataTime = ui.dateEdit_End->date().toString("yyyyMMdd");

	// 起始时间不能大于终止时间
	if (qstrStartDataTime.toInt() > qstrEndDataTime.toInt())
	{
		QString qstrTitle = tr("数据包定制及标准化封装工具");
		QString qstrText = tr("生产日期的起始时间:%1不能超过终止时间:%2！").arg(qstrStartDataTime.toInt()).arg(qstrEndDataTime.toInt());
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 起始时间（更新日期）
	QString qstrStartUpdateDataTime = ui.dateEdit_UpdateStart->date().toString("yyyyMMdd");

	// 终止时间（更新日期）
	QString qstrEndUpdateDataTime = ui.dateEdit_UpdateEnd->date().toString("yyyyMMdd");

	// 起始时间不能大于终止时间（更新日期）
	if (qstrStartUpdateDataTime.toInt() > qstrEndUpdateDataTime.toInt())
	{
		QString qstrTitle = tr("数据包定制及标准化封装工具");
		QString qstrText = tr("更新日期的起始时间:%1不能超过终止时间:%2！").arg(qstrStartUpdateDataTime.toInt()).arg(qstrEndUpdateDataTime.toInt());
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 空间范围
	SE_DRect dInputRect;
	dInputRect.dleft = ui.lineEdit_Left->text().toDouble();
	dInputRect.dtop = ui.lineEdit_Top->text().toDouble();
	dInputRect.dright = ui.lineEdit_Right->text().toDouble();
	dInputRect.dbottom = ui.lineEdit_Bottom->text().toDouble();

	// 输入原始数据所在路径列表
	vector<string> vInputSourceDataPath;
	vInputSourceDataPath.clear();

	// 从原始数据列表中获取路径
	int iRowCount = ui.tableWidget_SourceDataPath->rowCount();
	for (int i = 0; i < iRowCount; i++)
	{
		string strTempPath = ui.tableWidget_SourceDataPath->item(i, 0)->text().toLocal8Bit();

		// 如果是☑状态，则添加到vInputSourceDataPath中
		if (ui.tableWidget_SourceDataPath->item(i, 1)->checkState() == Qt::Checked)
		{
			vInputSourceDataPath.push_back(strTempPath);
		}
	}

	if (vInputSourceDataPath.size() == 0)
	{
		QString qstrTitle = tr("数据包定制及标准化封装工具");
		QString qstrText = tr("请添加原始数据路径并设置为选中状态！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();

		return;
	}

	else if (vInputSourceDataPath.size() == 1)
	{
		QString qstrTitle = tr("数据包定制及标准化封装工具");
		QString qstrText = tr("数据关联至少需要两个原始数据路径并且设置为选中状态，请添加原始数据路径或设置路径为选中状态！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

#pragma region "获取属性设置表格中对应的数据项属性值"

	// 生产单位
	string strTable_SCDW = ui.tableWidget_AttributesSetting->item(0, 3)->text().toLocal8Bit();

	// 产品密级
    string strTable_CPMJ = ui.tableWidget_AttributesSetting->item(1, 3)->text().toLocal8Bit();
	
    // 完整性
	string strTable_WZX = ui.tableWidget_AttributesSetting->item(2, 3)->text().toLocal8Bit();

	// 数据质量
	string strTable_SJZL = ui.tableWidget_AttributesSetting->item(3, 3)->text().toLocal8Bit();

#pragma endregion



#pragma region "遍历读取每个数据路径下的json元数据文件"

	// 元数据与数据源关联
	vector<RelateSourceDataInfo> vRelateInfos;
	vRelateInfos.clear();

	for (int i = 0; i < vInputSourceDataPath.size(); i++)
	{
		RelateSourceDataInfo relateInfo;
		relateInfo.strSourceDataPath = vInputSourceDataPath[i];

        string strSourceDataPath = vInputSourceDataPath[i];

		// 读取每个数据路径下的json格式元数据文件，获取属性设置表格中对应的数据项属性值
		// 获取生产日期
		vector<string> vJsonFilesPath;
		vJsonFilesPath.clear();
		GetJsonFilesFromFilePath(strSourceDataPath, vJsonFilesPath);

		// 元数据项
		vector<MetaDataTemplateInfo> vMetaDataTemplateInfos;
        vMetaDataTemplateInfos.clear();

		// 如果该路径下没有json文件，则跳过
		if (vJsonFilesPath.size() == 0)
		{
			// 记录日志
			continue;
		}

		// 仅有一个json元数据文件
		else if (vJsonFilesPath.size() == 1)
		{
			if (!ReadMetaDataJsonFile(vJsonFilesPath[0], vMetaDataTemplateInfos))
			{
				// 记录日志
				continue;
			}

			relateInfo.strMetadataPath = vJsonFilesPath[0];
		}

		// 如果有两个以上的json文件，只选择其中的一个
		else if(vJsonFilesPath.size() > 1)
		{
			// 记录日志，说明读取的元数据文件
			if (!ReadMetaDataJsonFile(vJsonFilesPath[0], vMetaDataTemplateInfos))
			{
				// 记录日志
				continue;
			}

			relateInfo.strMetadataPath = vJsonFilesPath[0];
		}

		

		// 管理信息-生产单位
		string strGLXX_SCDW = GetValueByKey("GLXX_SCDW", vMetaDataTemplateInfos);

		// 管理信息-生产日期
		string strGLXX_SCRQ = GetValueByKey("GLXX_SCRQ", vMetaDataTemplateInfos);

		// 管理信息-更新日期
		string strGLXX_GXRQ = GetValueByKey("GLXX_GXRQ", vMetaDataTemplateInfos);


		// 标识信息-数据四至范围
		string strBSXX_SJSZFW = GetValueByKey("BSXX_SJSZFW", vMetaDataTemplateInfos);

		// 根据四至范围四至范围用","分隔，获取经纬度范围，左、下、右、上
        vector<string> vRange;
        vRange.clear();
        string strDelimiter = ",";
        size_t pos = 0;
        string token;
        while ((pos = strBSXX_SJSZFW.find(strDelimiter)) != string::npos)
        {
        	token = strBSXX_SJSZFW.substr(0, pos);
        	vRange.push_back(token);
        	strBSXX_SJSZFW.erase(0, pos + strDelimiter.length());
        }
        vRange.push_back(strBSXX_SJSZFW); // 将最后一个token添加到vector中
					
        double dleft = atof(vRange[0].c_str());
        double dbottom = atof(vRange[1].c_str());
        double dright = atof(vRange[2].c_str());
        double dtop = atof(vRange[3].c_str());

		// 管理信息-产品密级
		string strGLXX_CPMJ = GetValueByKey("GLXX_CPMJ", vMetaDataTemplateInfos);

		// 质量信息-完整性
		string strZLXX_WZX = GetValueByKey("ZLXX_WZX", vMetaDataTemplateInfos);

		// 质量信息-数据质量
		string strZLXX_SJZL = GetValueByKey("ZLXX_SJZL", vMetaDataTemplateInfos);

#pragma region "判断属性设置表格中对应的数据项属性值是否与元数据文件中的一致"

		// 各项查询条件均满足查询要求
		if( strTable_SCDW.find(strGLXX_SCDW) != string::npos
			&& atoi(strGLXX_SCRQ.c_str()) >= qstrStartDataTime.toInt()
			&& atoi(strGLXX_SCRQ.c_str()) <= qstrEndDataTime.toInt()
            && atoi(strGLXX_GXRQ.c_str()) >= qstrStartUpdateDataTime.toInt()
            && atoi(strGLXX_GXRQ.c_str()) <= qstrEndUpdateDataTime.toInt()
            && strTable_CPMJ == strGLXX_CPMJ
            && strTable_WZX == strZLXX_WZX
            && strTable_SJZL == strZLXX_SJZL
            && dleft >= ui.lineEdit_Left->text().toDouble()
            && dbottom >= ui.lineEdit_Bottom->text().toDouble()
            && dright <= ui.lineEdit_Right->text().toDouble()
            && dtop <= ui.lineEdit_Top->text().toDouble())
		{
			// 将当前数据加入关联列表中
            vRelateInfos.push_back(relateInfo);
		}

#pragma endregion
	}

#pragma endregion

#pragma region "将关联的数据写入到输出结果文件中"

	if (vRelateInfos.size() > 0)
	{
		string strOutputPath = m_qstrOutputPath.toLocal8Bit();
		FILE *fp = fopen(strOutputPath.c_str(), "w");
		if (!fp)
		{
			QString qstrTitle = tr("数据包定制及标准化封装工具");
			QString qstrText = tr("创建关联结果文件：%1失败！").arg(m_qstrOutputPath);
			QMessageBox msgBox;
			msgBox.setWindowTitle(qstrTitle);
			msgBox.setText(qstrText);
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		// 输出查询条件
		fprintf(fp, "\n-----------------------------------------\n");
        fprintf(fp, "关联条件：\n");
		fprintf(fp, "生产单位：%s\n", convertEncoding( strTable_SCDW, "gbk" ,"UTF-8").c_str());
        fprintf(fp, "起始生产日期：%s\n", qstrStartDataTime.toLocal8Bit().data());
        fprintf(fp, "终止生产日期：%s\n", qstrEndDataTime.toLocal8Bit().data());
        fprintf(fp, "起始更新日期：%s\n", qstrStartUpdateDataTime.toLocal8Bit().data());
        fprintf(fp, "终止更新日期：%s\n", qstrEndUpdateDataTime.toLocal8Bit().data());
        fprintf(fp, "产品密级：%s\n", convertEncoding(strTable_CPMJ, "gbk", "UTF-8").c_str());
        fprintf(fp, "完整性：%s\n", convertEncoding(strTable_WZX, "gbk", "UTF-8").c_str());
        fprintf(fp, "数据质量：%s\n", convertEncoding(strTable_SJZL, "gbk", "UTF-8").c_str());
        fprintf(fp, "左经度：%.6f\n", ui.lineEdit_Left->text().toDouble());
        fprintf(fp, "下纬度：%.6f\n", ui.lineEdit_Bottom->text().toDouble());
        fprintf(fp, "右经度：%.6f\n", ui.lineEdit_Right->text().toDouble());
        fprintf(fp, "上纬度：%.6f\n", ui.lineEdit_Top->text().toDouble());
        fprintf(fp, "\n");

		fprintf(fp, "\n-----------------------------------------\n");
        // 输出关联结果
        fprintf(fp, "关联结果：\n");
        for (int i = 0; i < vRelateInfos.size(); ++i)
        {
        	fprintf(fp, "(%d)原始数据路径：%s\n", i + 1, convertEncoding(vRelateInfos[i].strSourceDataPath, "gbk", "UTF-8").c_str());
        	fprintf(fp, "(%d)元数据路径：%s\n", i + 1, convertEncoding(vRelateInfos[i].strMetadataPath, "gbk", "UTF-8").c_str());
        	fprintf(fp, "\n");
        }
		fprintf(fp, "\n-----------------------------------------\n");
		fclose(fp);

	}


#pragma endregion


	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("RelateSourceData/OutputLogPath"), m_qstrOutputLogPath, QgsSettings::Section::Plugins);

	QString qstrTitle = tr("数据包定制及标准化封装工具");
	QString qstrText = tr("基于原始数据元数据的数据关联完毕！");
	QMessageBox msgBox;
	msgBox.setWindowTitle(qstrTitle);
	msgBox.setText(qstrText);
	msgBox.setStandardButtons(QMessageBox::Yes);
	msgBox.setDefaultButton(QMessageBox::Yes);
	msgBox.exec();
	return;

}

void CSE_RelateSourceDataDialog::Button_Cancel_rejected()
{
	reject();
}


void CSE_RelateSourceDataDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrOutputLogPath = settings.value(QStringLiteral("RelateSourceData/OutputLogPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();


}

// 判断目录是否存在
bool CSE_RelateSourceDataDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_RelateSourceDataDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


// 获取指定目录的最后一级目录
void CSE_RelateSourceDataDialog::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}

/*获取指定目录的最后一级目录*/
void CSE_RelateSourceDataDialog::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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




bool CSE_RelateSourceDataDialog::IsExistedInVector(string strValue, vector<string>& vValue)
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



bool CSE_RelateSourceDataDialog::isDirExist(QString fullPath)
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


void CSE_RelateSourceDataDialog::pushButton_SaveLog_clicked()
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



#pragma region  "检查文件或文件夹是否存在"

bool CSE_RelateSourceDataDialog::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion

// 更新属性设置表格
void CSE_RelateSourceDataDialog::UpdateAttributesSettingTableWidget()
{
	// 清空表格内容
	ui.tableWidget_AttributesSetting->clearContents();

	vector<MetaDataTemplateInfo> vMetaDataTemplateInfo;
    vMetaDataTemplateInfo.clear();

	// TODO:后续根据需要调整
	MetaDataTemplateInfo info;
	info.strCategory = "管理信息";
	info.strItem = "生产单位";
	info.strShortFormItem = "GLXX_SCDW";
	vMetaDataTemplateInfo.push_back(info);

	info.strCategory = "管理信息";
	info.strItem = "产品密级";
	info.strShortFormItem = "GLXX_CPMJ";
	vMetaDataTemplateInfo.push_back(info);

	info.strCategory = "质量信息";
	info.strItem = "完整性";
	info.strShortFormItem = "ZLXX_WZX";
	vMetaDataTemplateInfo.push_back(info);

	info.strCategory = "质量信息";
	info.strItem = "数据质量";
	info.strShortFormItem = "ZLXX_SJZL";
	vMetaDataTemplateInfo.push_back(info);


	for (int i = 0; i < vMetaDataTemplateInfo.size(); ++i)
	{
		// 在表格中增加记录
		int iRowCount = 0;
		iRowCount = ui.tableWidget_AttributesSetting->rowCount();

		// 增加一行
		ui.tableWidget_AttributesSetting->insertRow(iRowCount);

		// 【1】元数据类
		ui.tableWidget_AttributesSetting->setItem(iRowCount, 0, new QTableWidgetItem(QString::fromUtf8(vMetaDataTemplateInfo[i].strCategory.c_str())));

		// 【2】数据项
		ui.tableWidget_AttributesSetting->setItem(iRowCount, 1, new QTableWidgetItem(QString::fromUtf8(vMetaDataTemplateInfo[i].strItem.c_str())));

		// 【3】数据元素简称
		ui.tableWidget_AttributesSetting->setItem(iRowCount, 2, new QTableWidgetItem(QString::fromUtf8(vMetaDataTemplateInfo[i].strShortFormItem.c_str())));

	}


	// 设置表格中某些列内容不可编辑
	for (int i = 0; i < ui.tableWidget_AttributesSetting->rowCount(); ++i)
	{
		if (ui.tableWidget_AttributesSetting->item(i, 0))
		{
			// 数据项不可编辑
			ui.tableWidget_AttributesSetting->item(i, 0)->setFlags(Qt::NoItemFlags);
		}

		if (ui.tableWidget_AttributesSetting->item(i, 1))
		{
			// 数据类型不可编辑
			ui.tableWidget_AttributesSetting->item(i, 1)->setFlags(Qt::NoItemFlags);
		}

		if (ui.tableWidget_AttributesSetting->item(i, 2))
		{
			// 数据类型不可编辑
			ui.tableWidget_AttributesSetting->item(i, 2)->setFlags(Qt::NoItemFlags);
		}
	}


	for (int i = 0; i < ui.tableWidget_AttributesSetting->rowCount(); ++i)
	{
		for (int j = 0; j < ui.tableWidget_AttributesSetting->columnCount(); ++j)
		{
			if (ui.tableWidget_AttributesSetting->item(i, j))
			{
				ui.tableWidget_AttributesSetting->item(i, j)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
			}

		}
	}

	// 调用 pTableWidget 对象的 resizeColumnsToContents() 方法
	// 该方法会自动调整表格的列宽，使其能够容纳列中的内容
	ui.tableWidget_AttributesSetting->resizeColumnsToContents();

}


// 获取当前路径下所有json元数据文件路径
void CSE_RelateSourceDataDialog::GetJsonFilesFromFilePath(
	const string& strFilePath,
	vector<string>& vFilesPath)
{
	vFilesPath.clear();

	try
	{
		std::filesystem::path folderPath = strFilePath;

		if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
		{
			return;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
		{
			if (entry.is_regular_file() && (entry.path().extension() == ".json" || entry.path().extension() == ".JSON"))
			{
				vFilesPath.push_back(entry.path().string());
			}
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		return;
	}
}


bool CSE_RelateSourceDataDialog::ReadMetaDataJsonFile(const string& strFile, 
	vector<MetaDataTemplateInfo>& vMetadatInfo)
{
	vMetadatInfo.clear();

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
	MetaDataTemplateInfo metaInfo;

	// 管理信息-生产单位
	metaInfo.strShortFormItem = "GLXX_SCDW";
    metaInfo.strValue = convertEncoding(dsl["GLXX_SCDW"], "utf-8", "gbk");
	vMetadatInfo.push_back(metaInfo);

	// 管理信息-生产日期
	metaInfo.strShortFormItem = "GLXX_SCRQ";
	metaInfo.strValue = convertEncoding(dsl["GLXX_SCRQ"], "utf-8", "gbk");
	vMetadatInfo.push_back(metaInfo);

	// 管理信息-更新日期
	metaInfo.strShortFormItem = "GLXX_GXRQ";
	metaInfo.strValue = convertEncoding(dsl["GLXX_GXRQ"], "utf-8", "gbk");
	vMetadatInfo.push_back(metaInfo);

	// 标识信息-数据四至范围
	metaInfo.strShortFormItem = "BSXX_SJSZFW";
	metaInfo.strValue = convertEncoding(dsl["BSXX_SJSZFW"], "utf-8", "gbk");
	vMetadatInfo.push_back(metaInfo);
	
	// 管理信息-产品密级
	metaInfo.strShortFormItem = "GLXX_CPMJ";
	metaInfo.strValue = convertEncoding(dsl["GLXX_CPMJ"], "utf-8", "gbk");
	vMetadatInfo.push_back(metaInfo);

	// 质量信息-完整性
	metaInfo.strShortFormItem = "ZLXX_WZX";
	metaInfo.strValue = convertEncoding(dsl["ZLXX_WZX"], "utf-8", "gbk");
	vMetadatInfo.push_back(metaInfo);

	// 质量信息-数据质量
	metaInfo.strShortFormItem = "ZLXX_SJZL";
	metaInfo.strValue = convertEncoding(dsl["ZLXX_SJZL"], "utf-8", "gbk");
	vMetadatInfo.push_back(metaInfo);

	return true;
}


// 根据元数据标识获取属性值
string CSE_RelateSourceDataDialog::GetValueByKey(const string& strKey,
	vector<MetaDataTemplateInfo>& vMetadatInfo)
{
    for (int i = 0; i < vMetadatInfo.size(); ++i)
    {
        if (vMetadatInfo[i].strShortFormItem == strKey)
        {
            // 找到匹配的元素，输出其值
            return vMetadatInfo[i].strValue;
        }
    }
    // 如果没有找到匹配的元素，输出一个默认值或进行其他处理
    return "";
}

string CSE_RelateSourceDataDialog::convertEncoding(const string& input,
	const string& fromEncoding,
	const string& toEncoding)
{
	iconv_t conv = iconv_open(toEncoding.c_str(), fromEncoding.c_str());
	if (conv == (iconv_t)(-1)) {
		perror("iconv_open");
		return "";
	}

	size_t inBytesLeft = input.size();
	size_t outBytesLeft = inBytesLeft * 4; // 预留足够的空间
	std::vector<char> output(outBytesLeft);
	char* inBuf = const_cast<char*>(input.data());
	char* outBuf = output.data();

	size_t result = iconv(conv, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft);
	if (result == (size_t)(-1)) {
		perror("iconv");
		iconv_close(conv);
		return "";
	}

	iconv_close(conv);
	return std::string(output.data(), output.size() - outBytesLeft);
}