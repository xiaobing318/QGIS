#define _HAS_STD_BYTE 0
#include "se_metadata_template.h"

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

#include "iconv.h"


using namespace std;

CSE_MetaDataTemplateDlg::CSE_MetaDataTemplateDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.pushButton_OK, &QPushButton::clicked, this, &CSE_MetaDataTemplateDlg::pushButton_OK_clicked);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &CSE_MetaDataTemplateDlg::pushButton_Cancel_clicked);
	
	connect(ui.pushButton_LoadFile, &QPushButton::clicked, this, &CSE_MetaDataTemplateDlg::pushButton_LoadFile_clicked);
	connect(ui.pushButton_Save, &QPushButton::clicked, this, &CSE_MetaDataTemplateDlg::pushButton_Save_clicked);

	connect(ui.lineEdit_InputPath, &QLineEdit::textChanged, this, &CSE_MetaDataTemplateDlg::on_InputDataPath_TextChanged);
	

	restoreState();
	ui.lineEdit_InputPath->setText(m_qstrInputDataPath);

	// 表头居中
	ui.tableWidget_MetaDataTemplate->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_MetaDataTemplate->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_MetaDataTemplate->horizontalHeader()->setStretchLastSection(true);

	m_vMetaDatas.clear();
}

CSE_MetaDataTemplateDlg::~CSE_MetaDataTemplateDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("METADATA_TEMPLATE/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("METADATA_TEMPLATE/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);

}

void CSE_MetaDataTemplateDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("METADATA_TEMPLATE/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("METADATA_TEMPLATE/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}



// 获取输入数据路径
QString CSE_MetaDataTemplateDlg::GetInputDataPath()
{
	return ui.lineEdit_InputPath->text();
}



void CSE_MetaDataTemplateDlg::UpdateTableWidget()
{
	// 清空表格内容
	ui.tableWidget_MetaDataTemplate->clearContents();

	// 如果是文件，逐一填充单元格
	for (int i = 0; i < m_vMetaDatas.size(); ++i)
	{
		// 在表格中增加记录
		int iRowCount = 0;
		iRowCount = ui.tableWidget_MetaDataTemplate->rowCount();

		// 增加一行
		ui.tableWidget_MetaDataTemplate->insertRow(iRowCount);
				
		// 【1】元数据类
		ui.tableWidget_MetaDataTemplate->setItem(iRowCount, 0, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strCategory.c_str())));

		// 【2】数据项
		ui.tableWidget_MetaDataTemplate->setItem(iRowCount, 1, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strItem.c_str())));

		// 【3】数据元素简称
		ui.tableWidget_MetaDataTemplate->setItem(iRowCount, 2, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strShortFormItem.c_str())));

		// 【4】数据类型
		ui.tableWidget_MetaDataTemplate->setItem(iRowCount, 3, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strType.c_str())));
		
		// 【5】格式
		ui.tableWidget_MetaDataTemplate->setItem(iRowCount, 4, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strFormat.c_str())));

		// 【6】备注
		ui.tableWidget_MetaDataTemplate->setItem(iRowCount, 5, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strDetail.c_str())));

	}
	
	for (int i = 0; i < ui.tableWidget_MetaDataTemplate->rowCount(); ++i)
	{
		ui.tableWidget_MetaDataTemplate->item(i, 0)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_MetaDataTemplate->item(i, 1)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_MetaDataTemplate->item(i, 2)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_MetaDataTemplate->item(i, 3)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_MetaDataTemplate->item(i, 4)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		ui.tableWidget_MetaDataTemplate->item(i, 5)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	}

	// 自适应宽度
	ui.tableWidget_MetaDataTemplate->resizeColumnToContents(0);
	ui.tableWidget_MetaDataTemplate->resizeColumnToContents(1);
	ui.tableWidget_MetaDataTemplate->resizeColumnToContents(2);
	ui.tableWidget_MetaDataTemplate->resizeColumnToContents(3);
	ui.tableWidget_MetaDataTemplate->resizeColumnToContents(4);
	ui.tableWidget_MetaDataTemplate->resizeColumnToContents(5);
}


void CSE_MetaDataTemplateDlg::pushButton_LoadFile_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择元数据模板文件");
	QString filter = tr("csv 文件(*.csv)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	// 读取csv文件
	// 初始化GDAL
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
	CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
	GDALAllRegister();

	string strCsv = strFileName.toLocal8Bit();

	m_qstrInputDataPath = strFileName;
	ui.lineEdit_InputPath->setText(strFileName);

	// 打开CSV文件
	GDALDataset* poDS = static_cast<GDALDataset*>(GDALOpenEx(strCsv.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));

	if (!poDS) 
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("打开配置文件失败！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return ;
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
		MetaDataTemplateInfo info;

		// 获取要素的所有属性（即CSV文件中的每一列）

		// 第一列：元数据类
		info.strCategory = poFeature->GetFieldAsString(0);

		// 第二列：数据项
		info.strItem = poFeature->GetFieldAsString(1);

		// 第三列：数据元素简称
		info.strShortFormItem = poFeature->GetFieldAsString(2);

		// 第四列：数据类型
		info.strType = poFeature->GetFieldAsString(3);

		// 第五列：格式
		info.strFormat = poFeature->GetFieldAsString(4);

		// 第六列：备注
		info.strDetail = poFeature->GetFieldAsString(5);

		m_vMetaDatas.push_back(info);

		// 释放要素资源
		OGRFeature::DestroyFeature(poFeature);
	}

	// 关闭并释放数据集资源
	GDALClose(poDS);

	// 更新表格控件中的数据
	UpdateTableWidget();

}


void CSE_MetaDataTemplateDlg::GetMetaDataInfosFromTableWidget(vector<MetaDataTemplateInfo>& vInfos)
{
	vInfos.clear();

	// 从tableWidget_MetaDataTemplate中获取提取参数
	for (int i = 0; i < ui.tableWidget_MetaDataTemplate->rowCount(); ++i)
	{
		MetaDataTemplateInfo info;

		// 元数据类
		if (ui.tableWidget_MetaDataTemplate->item(i, 0))
		{
			info.strCategory = ui.tableWidget_MetaDataTemplate->item(i, 0)->text().toLocal8Bit();
		}

		// 数据项
		if (ui.tableWidget_MetaDataTemplate->item(i, 1))
		{
			info.strItem = ui.tableWidget_MetaDataTemplate->item(i, 1)->text().toLocal8Bit();
		}

		// 数据元素简称
		if (ui.tableWidget_MetaDataTemplate->item(i, 2))
		{
			info.strShortFormItem = ui.tableWidget_MetaDataTemplate->item(i, 2)->text().toLocal8Bit();
		}

		// 数据类型
		if (ui.tableWidget_MetaDataTemplate->item(i, 3))
		{
			info.strType = ui.tableWidget_MetaDataTemplate->item(i, 3)->text().toLocal8Bit();
		}

		// 格式
		if (ui.tableWidget_MetaDataTemplate->item(i, 4))
		{
			info.strFormat = ui.tableWidget_MetaDataTemplate->item(i, 4)->text().toLocal8Bit();
		}

		// 备注
		if (ui.tableWidget_MetaDataTemplate->item(i, 5))
		{
			info.strDetail = ui.tableWidget_MetaDataTemplate->item(i, 5)->text().toLocal8Bit();
		}

		vInfos.push_back(info);
	}
}

void CSE_MetaDataTemplateDlg::pushButton_Save_clicked()
{
	vector<MetaDataTemplateInfo> vMetaDataInfos;
	vMetaDataInfos.clear();

	// 获取表格控件中的内容
	GetMetaDataInfosFromTableWidget(vMetaDataInfos);

	// 选择保存路径
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("保存元数据模板文件");
	QString filter = tr("csv 文件(*.csv)");
	QString qstrFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!qstrFileName.isEmpty())
	{
		ui.lineEdit_OutputPath->setText(qstrFileName);

		// 初始化GDAL
		GDALAllRegister();
		CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
		CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段

		// 获取CSV驱动
		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("CSV");
		if (poDriver == NULL) {
			//std::cout << "CSV driver not available.\n";
			return;
		}
		// 创建GB2312的临时csv文件，转码UTF-8为设置的文件名
		// 获取存储路径		
		
		string strOutputCSV = qstrFileName.toLocal8Bit();
		string strOutputPath = CPLGetPath(strOutputCSV.c_str());
		string strCSVName = CPLGetBasename(strOutputCSV.c_str());

		// GB2312编码的临时文件路径
		string strTempCsvFilePath = strOutputPath + "/" + strCSVName + "_GB2312.csv";
		string strTempCsvFileName = strCSVName + "_GB2312.csv";




		// 创建CSV文件
		GDALDataset* poDS;
		poDS = poDriver->Create(strTempCsvFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
		if (poDS == NULL)
		{
			//std::cout << "Creation of output file failed.\n";
			return;
		}

		

		// 获取图层，如果没有则创建
		OGRLayer* poLayer = poDS->CreateLayer(strTempCsvFileName.c_str(), NULL, wkbNone, NULL);
		if (poLayer == NULL)
		{
			//std::cout << "Layer creation failed.\n";
			return;
		}


		// 创建“元数据类”字段
		QString qstrCategory = tr("元数据类");
		string strCategory = qstrCategory.toLocal8Bit();
		OGRFieldDefn oCategoryField(strCategory.c_str(), OFTString);
		oCategoryField.SetWidth(50);
		if (poLayer->CreateField(&oCategoryField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		// 创建“数据项”字段
		QString qstrItem = tr("数据项");
		string strItem = qstrItem.toLocal8Bit();
		OGRFieldDefn oItemField(strItem.c_str(), OFTString);
		oItemField.SetWidth(100);
		if (poLayer->CreateField(&oItemField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		// 创建“数据元素简称”字段
		QString qstrShortform = tr("数据元素简称");
		string strShortform = qstrShortform.toLocal8Bit();
		OGRFieldDefn oShortformField(strShortform.c_str(), OFTString);
		oShortformField.SetWidth(50);
		if (poLayer->CreateField(&oShortformField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		// 创建“数据类型”字段
		QString qstrType = tr("数据类型");
		string strType = qstrType.toLocal8Bit();
		OGRFieldDefn oTypeField(strType.c_str(), OFTString);
		oTypeField.SetWidth(50);
		if (poLayer->CreateField(&oTypeField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		// 创建“格式”字段
		QString qstrFormat = tr("格式");
		string strFormat = qstrFormat.toLocal8Bit();
		OGRFieldDefn oFormatField(strFormat.c_str(), OFTString);
		oFormatField.SetWidth(50);
		if (poLayer->CreateField(&oFormatField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		// 创建“备注”字段
		QString qstrDetail = tr("备注");
		string strDetail = qstrDetail.toLocal8Bit();
		OGRFieldDefn oDetailField(strDetail.c_str(), OFTString);
		oDetailField.SetWidth(1000);

		if (poLayer->CreateField(&oDetailField) != OGRERR_NONE)
		{
			//std::cout << "Creating Name field failed.\n";
			return;
		}

		for (int i = 0; i < vMetaDataInfos.size(); ++i)
		{
			// 创建要素
			OGRFeature* poFeature;
			poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
			poFeature->SetField(0, vMetaDataInfos[i].strCategory.c_str());
			poFeature->SetField(1, vMetaDataInfos[i].strItem.c_str());
			poFeature->SetField(2, vMetaDataInfos[i].strShortFormItem.c_str());
			poFeature->SetField(3, vMetaDataInfos[i].strType.c_str());
			poFeature->SetField(4, vMetaDataInfos[i].strFormat.c_str());
			poFeature->SetField(5, vMetaDataInfos[i].strDetail.c_str());

			if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
			{
				//std::cout << "Failed to create feature in shapefile.\n";
				return;
			}

			// 清理
			OGRFeature::DestroyFeature(poFeature);
		}

		GDALClose(poDS);


		/*在windows平台上生成的csv文件为GB2312编码，需转换成UTF-8编码*/
		convertCsv(strTempCsvFilePath, strOutputCSV);

		// 删除临时csv文件
		std::filesystem::remove(strTempCsvFilePath);


		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("保存模板文件成功！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
	}
	else
	{
		return;
	}


}

void CSE_MetaDataTemplateDlg::pushButton_OK_clicked()
{
	// 如果文件/文件夹不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_InputPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 如果文件/文件夹不存在
	if (!CheckFileOrDirExist(ui.lineEdit_OutputPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_OutputPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("METADATA_TEMPLATE/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	accept();
}

void CSE_MetaDataTemplateDlg::pushButton_Cancel_clicked()
{
	reject();
}

void CSE_MetaDataTemplateDlg::on_InputDataPath_TextChanged()
{
}


string CSE_MetaDataTemplateDlg::convertEncoding(const string& input, 
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

void CSE_MetaDataTemplateDlg::convertCsv(const string& inputFile, const string& outputFile) 
{
	std::ifstream inFile(inputFile, std::ios::binary);
	std::ofstream outFile(outputFile, std::ios::binary);

	if (!inFile.is_open() || !outFile.is_open()) {
		std::cerr << "Error opening files!" << std::endl;
		return;
	}

	std::string line;
	while (std::getline(inFile, line)) {
		std::string utf8Line = convertEncoding(line, "GB2312", "UTF-8");
		if (!utf8Line.empty()) {
			outFile << utf8Line << "\n";
		}
	}

	inFile.close();
	outFile.close();
}


#pragma region  "检查文件或文件夹是否存在"

bool CSE_MetaDataTemplateDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion