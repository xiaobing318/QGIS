#define _HAS_STD_BYTE 0
// 使用json头文件
#include <nlohmann/json.hpp>
#include "se_edit_source_data_metadata.h"

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

using ordered_json = nlohmann::ordered_json;
using namespace std;

CSE_EditSrcDataMetadataDlg::CSE_EditSrcDataMetadataDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.pushButton_OK, &QPushButton::clicked, this, &CSE_EditSrcDataMetadataDlg::pushButton_OK_clicked);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &CSE_EditSrcDataMetadataDlg::pushButton_Cancel_clicked);
	
	connect(ui.pushButton_LoadFile, &QPushButton::clicked, this, &CSE_EditSrcDataMetadataDlg::pushButton_LoadFile_clicked);
	connect(ui.Button_SaveMetaData, &QPushButton::clicked, this, &CSE_EditSrcDataMetadataDlg::pushButton_Save_clicked);

	connect(ui.lineEdit_InputPath, &QLineEdit::textChanged, this, &CSE_EditSrcDataMetadataDlg::on_InputDataPath_TextChanged);
	

	restoreState();
	ui.lineEdit_InputPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputMetaDataPath->setText(m_qstrOutputDataPath);

	// 设置默认第一个选项卡
	ui.tabWidget_DataType->setCurrentIndex(0);

#pragma region "表格控件初始化"
	
	// 表头居中
	ui.tableWidget_ShiLiang->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_ShiLiang->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_ShiLiang->horizontalHeader()->setStretchLastSection(true);

	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_YingXiang->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_YingXiang->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_YingXiang->horizontalHeader()->setStretchLastSection(true);

	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_GaoCheng->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_GaoCheng->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_GaoCheng->horizontalHeader()->setStretchLastSection(true);

	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_DiMing->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_DiMing->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_DiMing->horizontalHeader()->setStretchLastSection(true);


	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_ShiJing->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_ShiJing->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_ShiJing->horizontalHeader()->setStretchLastSection(true);

	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_DiZhi->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_DiZhi->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_DiZhi->horizontalHeader()->setStretchLastSection(true);

	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_HangKongTu->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_HangKongTu->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_HangKongTu->horizontalHeader()->setStretchLastSection(true);

	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_QingXieSheYing->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_QingXieSheYing->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_QingXieSheYing->horizontalHeader()->setStretchLastSection(true);

	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_BiaoHui->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_BiaoHui->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_BiaoHui->horizontalHeader()->setStretchLastSection(true);

	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_GuanWang->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_GuanWang->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_GuanWang->horizontalHeader()->setStretchLastSection(true);


	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_GaoKongQiXiang->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_GaoKongQiXiang->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_GaoKongQiXiang->horizontalHeader()->setStretchLastSection(true);

	//------------------------------------------------//
	// 表头居中
	ui.tableWidget_DiMianQiXiang->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);//表头字体居中

	// 颜色交叉
	ui.tableWidget_DiMianQiXiang->setAlternatingRowColors(true);

	// 最后一列铺满最后
	ui.tableWidget_DiMianQiXiang->horizontalHeader()->setStretchLastSection(true);

#pragma endregion

	m_vMetaDatas.clear();
}

CSE_EditSrcDataMetadataDlg::~CSE_EditSrcDataMetadataDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("EDIT_SOUREC_DATA_METADATA/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("EDIT_SOUREC_DATA_METADATA/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);

}

void CSE_EditSrcDataMetadataDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("EDIT_SOUREC_DATA_METADATA/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("EDIT_SOUREC_DATA_METADATA/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}



// 获取输入数据路径
QString CSE_EditSrcDataMetadataDlg::GetInputDataPath()
{
	return ui.lineEdit_InputPath->text();
}



void CSE_EditSrcDataMetadataDlg::UpdateTableWidget(QTableWidget* pWidget)
{
	// 清空表格内容
	pWidget->clearContents();

	// 如果是文件，逐一填充单元格
	for (int i = 0; i < m_vMetaDatas.size(); ++i)
	{
		// 在表格中增加记录
		int iRowCount = 0;
		iRowCount = pWidget->rowCount();

		// 增加一行
		pWidget->insertRow(iRowCount);
				
		// 【1】元数据类
		pWidget->setItem(iRowCount, 0, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strCategory.c_str())));

		// 【2】数据项
		pWidget->setItem(iRowCount, 1, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strItem.c_str())));

		// 【3】数据元素简称
		pWidget->setItem(iRowCount, 2, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strShortFormItem.c_str())));

		// 【4】数据类型
		pWidget->setItem(iRowCount, 3, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strType.c_str())));
		
		// 【5】格式
		pWidget->setItem(iRowCount, 4, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strFormat.c_str())));

		// 【6】备注
		pWidget->setItem(iRowCount, 5, new QTableWidgetItem(QString::fromUtf8(m_vMetaDatas[i].strDetail.c_str())));

	}
	
	for (int i = 0; i < pWidget->rowCount(); ++i)
	{
		pWidget->item(i, 0)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		pWidget->item(i, 1)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		pWidget->item(i, 2)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		pWidget->item(i, 3)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		pWidget->item(i, 4)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		pWidget->item(i, 5)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	}

	// 自适应宽度
	pWidget->resizeColumnToContents(0);
	pWidget->resizeColumnToContents(1);
	pWidget->resizeColumnToContents(2);
	pWidget->resizeColumnToContents(3);
	pWidget->resizeColumnToContents(4);
	pWidget->resizeColumnToContents(5);
}


void CSE_EditSrcDataMetadataDlg::pushButton_LoadFile_clicked()
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
		msgBox.setText(tr("打开元数据模板文件失败！"));
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

	switch (ui.tabWidget_DataType->currentIndex())
	{
		
		// 矢量数据
	case 0:		

		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_ShiLiang);
		break;

        // 影像数据
    case 1:
        // 更新表格控件中的数据
        UpdateTableWidget(ui.tableWidget_YingXiang);
		break;

		// 高程数据
	case 2:
		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_GaoCheng);
		break;

		// 地名数据
	case 3:
		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_DiMing);
		break;

		// 实景数据
	case 4:
		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_ShiJing);
		break;

		// 地质数据
	case 5:
		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_DiZhi);
		break;

		// 航空图数据
	case 6:
		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_HangKongTu);
		break;

		// 倾斜摄影数据
	case 7:
		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_QingXieSheYing);
		break;

		// 专用标绘
	case 8:
		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_BiaoHui);
		break;

		// 地下管网
	case 9:
		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_GuanWang);
		break;

		// 高空气象
	case 10:
		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_GaoKongQiXiang);
		break;

		// 地面气象
	case 11:
		// 更新表格控件中的数据
		UpdateTableWidget(ui.tableWidget_DiMianQiXiang);
		break;

	default:
		break;
	}




}


void CSE_EditSrcDataMetadataDlg::GetMetaDataInfosFromTableWidget(QTableWidget* pWidget, vector<MetaDataTemplateInfo>& vInfos)
{
	vInfos.clear();

	// 从tableWidget_MetaDataTemplate中获取提取参数
	for (int i = 0; i < pWidget->rowCount(); ++i)
	{
		MetaDataTemplateInfo info;

		// 元数据类
		if (pWidget->item(i, 0))
		{
			info.strCategory = pWidget->item(i, 0)->text().toLocal8Bit();
		}

		// 数据项
		if (pWidget->item(i, 1))
		{
			info.strItem = pWidget->item(i, 1)->text().toLocal8Bit();
		}

		// 数据元素简称
		if (pWidget->item(i, 2))
		{
			info.strShortFormItem = pWidget->item(i, 2)->text().toLocal8Bit();
		}

		// 数据类型
		if (pWidget->item(i, 3))
		{
			info.strType = pWidget->item(i, 3)->text().toLocal8Bit();
		}

		// 格式
		if (pWidget->item(i, 4))
		{
			info.strFormat = pWidget->item(i, 4)->text().toLocal8Bit();
		}

		// 备注
		if (pWidget->item(i, 5))
		{
			info.strDetail = pWidget->item(i, 5)->text().toLocal8Bit();
		}

		// 属性值
		if (pWidget->item(i, 6))
		{
			info.strValue = pWidget->item(i, 6)->text().toLocal8Bit();
		}

		vInfos.push_back(info);
	}
}

void CSE_EditSrcDataMetadataDlg::pushButton_Save_clicked()
{
	vector<MetaDataTemplateInfo> vMetaDataInfos;
	vMetaDataInfos.clear();

	switch (ui.tabWidget_DataType->currentIndex())
	{

		// 矢量数据
	case 0:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_ShiLiang, vMetaDataInfos);
		break;

		// 影像数据
	case 1:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_YingXiang, vMetaDataInfos);
		break;

		// 高程数据
	case 2:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_GaoCheng, vMetaDataInfos);
		break;

		// 地名数据
	case 3:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_DiMing, vMetaDataInfos);
		break;

		// 实景数据
	case 4:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_ShiJing, vMetaDataInfos);
		break;

		// 地质数据
	case 5:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_DiZhi, vMetaDataInfos);
		break;

		// 航空图数据
	case 6:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_HangKongTu, vMetaDataInfos);
		break;

		// 倾斜摄影数据
	case 7:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_QingXieSheYing, vMetaDataInfos);
		break;

		// 专用标绘
	case 8:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_BiaoHui, vMetaDataInfos);
		break;

		// 地下管网
	case 9:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_GuanWang, vMetaDataInfos);
		break;

		// 高空气象
	case 10:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_GaoKongQiXiang, vMetaDataInfos);
		break;

		// 地面气象
	case 11:
		// 获取表格控件中的内容
		GetMetaDataInfosFromTableWidget(ui.tableWidget_DiMianQiXiang, vMetaDataInfos);
		break;

	default:
		break;
	}




	// 选择保存路径
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("保存元数据文件");
	QString filter = tr("json 文件(*.json)");
	QString qstrFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!qstrFileName.isEmpty())
	{
		ui.lineEdit_OutputMetaDataPath->setText(qstrFileName);

		/*保存为json元数据文件*/
		string strOutputJsonFile = ui.lineEdit_OutputMetaDataPath->text().toLocal8Bit();

		// 保存元数据信息
		WriteMetaDataToJsonFileFromTables(strOutputJsonFile, vMetaDataInfos, ui.tabWidget_DataType->currentIndex());

		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("保存元数据文件：%1成功！").arg(QString::fromLocal8Bit(strOutputJsonFile.c_str())));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
	}
	else
	{
		return;
	}


}

void CSE_EditSrcDataMetadataDlg::pushButton_OK_clicked()
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
	if (!CheckFileOrDirExist(ui.lineEdit_OutputMetaDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_OutputMetaDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("EDIT_SOUREC_DATA_METADATA/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("EDIT_SOUREC_DATA_METADATA/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);

	accept();
}

void CSE_EditSrcDataMetadataDlg::pushButton_Cancel_clicked()
{
	reject();
}

void CSE_EditSrcDataMetadataDlg::on_InputDataPath_TextChanged()
{
}


string CSE_EditSrcDataMetadataDlg::convertEncoding(const string& input, 
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

void CSE_EditSrcDataMetadataDlg::convertCsv(const string& inputFile, const string& outputFile) 
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

bool CSE_EditSrcDataMetadataDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion

// iDataTypeIndex为tabWidget_DataType的页面索引
void CSE_EditSrcDataMetadataDlg::WriteMetaDataToJsonFileFromTables(
	const string& strFile,
	vector<MetaDataTemplateInfo>& vMetaInfo,
	int iDataTypeTabIndex)
{
	// 创建 JSON 对象
	ordered_json j;

	// 添加 data_source_list
	j["data_source_list"] = ordered_json::array();

	ordered_json data_source;

	switch (iDataTypeTabIndex)
	{

		// 矢量数据
	case 0:
		// 填充字段
		// 管理信息
		data_source["GLXX_CPMC"] = convertEncoding(vMetaInfo[0].strValue, "gbk", "utf-8");
		data_source["GLXX_BGDW"] = convertEncoding(vMetaInfo[1].strValue, "gbk", "utf-8");
		data_source["GLXX_SCDW"] = convertEncoding(vMetaInfo[2].strValue, "gbk", "utf-8");
		data_source["GLXX_SCRQ"] = convertEncoding(vMetaInfo[3].strValue, "gbk", "utf-8");
		data_source["GLXX_GXRQ"] = convertEncoding(vMetaInfo[4].strValue, "gbk", "utf-8");
		data_source["GLXX_CPMJ"] = convertEncoding(vMetaInfo[5].strValue, "gbk", "utf-8");
		data_source["GLXX_CPBB"] = convertEncoding(vMetaInfo[6].strValue, "gbk", "utf-8");

		// 标识信息
		data_source["BSXX_GJZ"] = convertEncoding(vMetaInfo[7].strValue, "gbk", "utf-8");
		data_source["BSXX_SJL"] = convertEncoding(vMetaInfo[8].strValue, "gbk", "utf-8");
		data_source["BSXX_SJGS"] = convertEncoding(vMetaInfo[9].strValue, "gbk", "utf-8");
		data_source["BSXX_SJLY"] = convertEncoding(vMetaInfo[10].strValue, "gbk", "utf-8");
		data_source["BSXX_FFMMGF"] = convertEncoding(vMetaInfo[11].strValue, "gbk", "utf-8");
		data_source["BSXX_TM"] = convertEncoding(vMetaInfo[12].strValue, "gbk", "utf-8");
		data_source["BSXX_TH"] = convertEncoding(vMetaInfo[13].strValue, "gbk", "utf-8");

		data_source["BSXX_BLCFM"] = atoi(vMetaInfo[14].strValue.c_str());
		data_source["BSXX_SJSZFW"] = convertEncoding(vMetaInfo[15].strValue, "gbk", "utf-8");
		data_source["BSXX_ZBXT"] = convertEncoding(vMetaInfo[16].strValue, "gbk", "utf-8");
		data_source["BSXX_GCJZ"] = convertEncoding(vMetaInfo[17].strValue, "gbk", "utf-8");
		data_source["BSXX_SDJZ"] = convertEncoding(vMetaInfo[18].strValue, "gbk", "utf-8");

		data_source["BSXX_TQCBZ"] = atof(vMetaInfo[19].strValue.c_str());
		data_source["BSXX_TQBL"] = atof(vMetaInfo[20].strValue.c_str());
		data_source["BSXX_DTTY"] = convertEncoding(vMetaInfo[21].strValue, "gbk", "utf-8");
		data_source["BSXX_ZYJX"] = atoi(vMetaInfo[22].strValue.c_str());
		data_source["BSXX_BZWX"] = atof(vMetaInfo[23].strValue.c_str());
		data_source["BSXX_ZBDW"] = convertEncoding(vMetaInfo[24].strValue, "gbk", "utf-8");

		// 质量信息
		data_source["ZLXX_WZX"] = convertEncoding(vMetaInfo[25].strValue, "gbk", "utf-8");			// 完整性
		data_source["ZLXX_SJZL"] = convertEncoding(vMetaInfo[26].strValue, "gbk", "utf-8");	// 数据质量

		// 专题信息
		data_source["ZTXX_CPMC"] = convertEncoding(vMetaInfo[27].strValue, "gbk", "utf-8");
		data_source["ZTXX_BGDW"] = convertEncoding(vMetaInfo[28].strValue, "gbk", "utf-8");
		data_source["ZTXX_SCDW"] = convertEncoding(vMetaInfo[29].strValue, "gbk", "utf-8");
		data_source["ZTXX_SCRQ"] = convertEncoding(vMetaInfo[30].strValue, "gbk", "utf-8");
		data_source["ZTXX_GXRQ"] = convertEncoding(vMetaInfo[31].strValue, "gbk", "utf-8");
		data_source["ZTXX_CPMJ"] = convertEncoding(vMetaInfo[32].strValue, "gbk", "utf-8");
		data_source["ZTXX_CPBB"] = convertEncoding(vMetaInfo[33].strValue, "gbk", "utf-8");
		data_source["ZTXX_GJZ"] = convertEncoding(vMetaInfo[34].strValue, "gbk", "utf-8");
		data_source["ZTXX_SJL"] = convertEncoding(vMetaInfo[35].strValue, "gbk", "utf-8");
		data_source["ZTXX_SJGS"] = convertEncoding(vMetaInfo[36].strValue, "gbk", "utf-8");
		data_source["ZTXX_SJLY"] = convertEncoding(vMetaInfo[37].strValue, "gbk", "utf-8");
		data_source["ZTXX_ZYQH"] = convertEncoding(vMetaInfo[38].strValue, "gbk", "utf-8");
		data_source["ZTXX_TM"] = convertEncoding(vMetaInfo[39].strValue, "gbk", "utf-8");
		data_source["ZTXX_TH"] = convertEncoding(vMetaInfo[40].strValue, "gbk", "utf-8");
		data_source["ZTXX_BLCFM"] = atoi(vMetaInfo[41].strValue.c_str());
		data_source["ZTXX_ZBXT"] = convertEncoding(vMetaInfo[42].strValue, "gbk", "utf-8");
		data_source["ZTXX_GCJZ"] = convertEncoding(vMetaInfo[43].strValue, "gbk", "utf-8");
		data_source["ZTXX_DTTY"] = convertEncoding(vMetaInfo[44].strValue, "gbk", "utf-8");
		data_source["ZTXX_DGJ"] = atoi(vMetaInfo[45].strValue.c_str());
		data_source["ZTXX_ZQSM"] = convertEncoding(vMetaInfo[46].strValue, "gbk", "utf-8");
		break;

		// 影像数据
	case 1:
		// 填充字段
		// 管理信息
		data_source["GLXX_CPMC"] = convertEncoding(vMetaInfo[0].strValue, "gbk", "utf-8");		// 产品名称
		data_source["GLXX_SCDW"] = convertEncoding(vMetaInfo[1].strValue, "gbk", "utf-8");		// 生产单位
		data_source["GLXX_HJDW"] = convertEncoding(vMetaInfo[2].strValue, "gbk", "utf-8");		// 汇交单位
		data_source["GLXX_SCRQ"] = convertEncoding(vMetaInfo[3].strValue, "gbk", "utf-8");		// 生产日期
		data_source["GLXX_GXRQ"] = convertEncoding(vMetaInfo[4].strValue, "gbk", "utf-8");		// 更新日期
		data_source["GLXX_CPMJ"] = convertEncoding(vMetaInfo[5].strValue, "gbk", "utf-8");		// 产品密级
		data_source["GLXX_CPBB"] = convertEncoding(vMetaInfo[6].strValue, "gbk", "utf-8");		// 产品版本

		// 标识信息
		data_source["BSXX_FBL"] = atof(vMetaInfo[7].strValue.c_str());			// 分辨率
		data_source["BSXX_SJSZFW"] = convertEncoding(vMetaInfo[8].strValue, "gbk", "utf-8");
		data_source["BSXX_ZBXT"] = convertEncoding(vMetaInfo[9].strValue, "gbk", "utf-8");			// 坐标系统
		data_source["BSXX_GCJZ"] = convertEncoding(vMetaInfo[10].strValue, "gbk", "utf-8");			// 高程基准
		data_source["BSXX_DTTY"] = convertEncoding(vMetaInfo[11].strValue, "gbk", "utf-8");			// 地图投影
		data_source["BSXX_ZYJX"] = atoi(vMetaInfo[12].strValue.c_str());				// 中央经线
		data_source["BSXX_BZWX"] = atof(vMetaInfo[13].strValue.c_str());				// 标准纬线
		data_source["BSXX_ZBDW"] = convertEncoding(vMetaInfo[14].strValue, "gbk", "utf-8");			// 坐标单位

		// 质量信息
		data_source["ZLXX_WZX"] = convertEncoding(vMetaInfo[15].strValue, "gbk", "utf-8");		// 完整性
		data_source["ZLXX_SJZL"] = convertEncoding(vMetaInfo[16].strValue, "gbk", "utf-8");		// 数据质量

		// 专题信息
		data_source["ZTXX_CPMC"] = convertEncoding(vMetaInfo[17].strValue, "gbk", "utf-8");		// 产品名称
		data_source["ZTXX_SCDW"] = convertEncoding(vMetaInfo[18].strValue, "gbk", "utf-8");		// 生产单位
		data_source["ZTXX_HJDW"] = convertEncoding(vMetaInfo[19].strValue, "gbk", "utf-8");		// 汇交单位
		data_source["ZTXX_SCRQ"] = convertEncoding(vMetaInfo[20].strValue, "gbk", "utf-8");		// 生产日期
		data_source["ZTXX_GXRQ"] = convertEncoding(vMetaInfo[21].strValue, "gbk", "utf-8");		// 更新日期
		data_source["ZTXX_CPMJ"] = convertEncoding(vMetaInfo[22].strValue, "gbk", "utf-8");		// 产品密级
		data_source["ZTXX_CPBB"] = convertEncoding(vMetaInfo[23].strValue, "gbk", "utf-8");		// 产品版本
		data_source["ZTXX_GJZ"] = convertEncoding(vMetaInfo[24].strValue, "gbk", "utf-8");			// 关键字
		data_source["ZTXX_SJL"] = convertEncoding(vMetaInfo[25].strValue, "gbk", "utf-8");			// 数据量
		data_source["ZTXX_SJGS"] = convertEncoding(vMetaInfo[26].strValue, "gbk", "utf-8");		// 数据格式
		data_source["ZTXX_SJLY"] = convertEncoding(vMetaInfo[27].strValue, "gbk", "utf-8");		// 数据来源
		data_source["ZTXX_ZYQH"] = convertEncoding(vMetaInfo[28].strValue, "gbk", "utf-8");		// 作业区号
		data_source["ZTXX_TM"] = convertEncoding(vMetaInfo[29].strValue, "gbk", "utf-8");			// 图名
		data_source["ZTXX_TH"] = convertEncoding(vMetaInfo[30].strValue, "gbk", "utf-8");			// 图号
		data_source["ZTXX_BLCFM"] = atoi(vMetaInfo[31].strValue.c_str());		// 比例尺分母
		data_source["ZTXX_ZBXT"] = convertEncoding(vMetaInfo[32].strValue, "gbk", "utf-8");		// 坐标系统
		data_source["ZTXX_GCJZ"] = convertEncoding(vMetaInfo[33].strValue, "gbk", "utf-8");		// 高程基准
		data_source["ZTXX_DTTY"] = convertEncoding(vMetaInfo[34].strValue, "gbk", "utf-8");		// 地图投影
		data_source["ZTXX_DGJ"] = atoi(vMetaInfo[35].strValue.c_str());			// 等高距
		data_source["ZTXX_ZQSM"] = convertEncoding(vMetaInfo[36].strValue, "gbk", "utf-8");		// 政区说明
		break;

		// 高程数据
	case 2:
		// 填充字段
		// 管理信息
		data_source["GLXX_CPMC"] = convertEncoding(vMetaInfo[0].strValue, "gbk", "utf-8");		// 产品名称
		data_source["GLXX_SCDW"] = convertEncoding(vMetaInfo[1].strValue, "gbk", "utf-8");		// 生产单位
		data_source["GLXX_HJDW"] = convertEncoding(vMetaInfo[2].strValue, "gbk", "utf-8");		// 汇交单位
		data_source["GLXX_SCRQ"] = convertEncoding(vMetaInfo[3].strValue, "gbk", "utf-8");		// 生产日期
		data_source["GLXX_GXRQ"] = convertEncoding(vMetaInfo[4].strValue, "gbk", "utf-8");		// 更新日期
		data_source["GLXX_CPMJ"] = convertEncoding(vMetaInfo[5].strValue, "gbk", "utf-8");		// 产品密级
		data_source["GLXX_CPBB"] = convertEncoding(vMetaInfo[6].strValue, "gbk", "utf-8");		// 产品版本

		// 标识信息
		data_source["BSXX_FBL"] = atof(vMetaInfo[7].strValue.c_str());			// 分辨率
		data_source["BSXX_SJSZFW"] = convertEncoding(vMetaInfo[8].strValue, "gbk", "utf-8");
		data_source["BSXX_ZBXT"] = convertEncoding(vMetaInfo[9].strValue, "gbk", "utf-8");			// 坐标系统
		data_source["BSXX_GCJZ"] = convertEncoding(vMetaInfo[10].strValue, "gbk", "utf-8");			// 高程基准
		data_source["BSXX_DTTY"] = convertEncoding(vMetaInfo[11].strValue, "gbk", "utf-8");			// 地图投影
		data_source["BSXX_ZYJX"] = atoi(vMetaInfo[12].strValue.c_str());				// 中央经线
		data_source["BSXX_BZWX"] = atof(vMetaInfo[13].strValue.c_str());				// 标准纬线
		data_source["BSXX_ZBDW"] = convertEncoding(vMetaInfo[14].strValue, "gbk", "utf-8");			// 坐标单位

		// 质量信息
		data_source["ZLXX_WZX"] = convertEncoding(vMetaInfo[15].strValue, "gbk", "utf-8");		// 完整性
		data_source["ZLXX_SJZL"] = convertEncoding(vMetaInfo[16].strValue, "gbk", "utf-8");		// 数据质量

		// 专题信息
		data_source["ZTXX_CPMC"] = convertEncoding(vMetaInfo[17].strValue, "gbk", "utf-8");		// 产品名称
		data_source["ZTXX_SCDW"] = convertEncoding(vMetaInfo[18].strValue, "gbk", "utf-8");		// 生产单位
		data_source["ZTXX_HJDW"] = convertEncoding(vMetaInfo[19].strValue, "gbk", "utf-8");		// 汇交单位
		data_source["ZTXX_SCRQ"] = convertEncoding(vMetaInfo[20].strValue, "gbk", "utf-8");		// 生产日期
		data_source["ZTXX_GXRQ"] = convertEncoding(vMetaInfo[21].strValue, "gbk", "utf-8");		// 更新日期
		data_source["ZTXX_CPMJ"] = convertEncoding(vMetaInfo[22].strValue, "gbk", "utf-8");		// 产品密级
		data_source["ZTXX_CPBB"] = convertEncoding(vMetaInfo[23].strValue, "gbk", "utf-8");		// 产品版本
		data_source["ZTXX_GJZ"] = convertEncoding(vMetaInfo[24].strValue, "gbk", "utf-8");			// 关键字
		data_source["ZTXX_SJL"] = convertEncoding(vMetaInfo[25].strValue, "gbk", "utf-8");			// 数据量
		data_source["ZTXX_SJGS"] = convertEncoding(vMetaInfo[26].strValue, "gbk", "utf-8");		// 数据格式
		data_source["ZTXX_SJLY"] = convertEncoding(vMetaInfo[27].strValue, "gbk", "utf-8");		// 数据来源
		data_source["ZTXX_ZYQH"] = convertEncoding(vMetaInfo[28].strValue, "gbk", "utf-8");		// 作业区号
		data_source["ZTXX_TM"] = convertEncoding(vMetaInfo[29].strValue, "gbk", "utf-8");			// 图名
		data_source["ZTXX_TH"] = convertEncoding(vMetaInfo[30].strValue, "gbk", "utf-8");			// 图号
		data_source["ZTXX_BLCFM"] = atoi(vMetaInfo[31].strValue.c_str());		// 比例尺分母
		data_source["ZTXX_ZBXT"] = convertEncoding(vMetaInfo[32].strValue, "gbk", "utf-8");		// 坐标系统
		data_source["ZTXX_GCJZ"] = convertEncoding(vMetaInfo[33].strValue, "gbk", "utf-8");		// 高程基准
		data_source["ZTXX_DTTY"] = convertEncoding(vMetaInfo[34].strValue, "gbk", "utf-8");		// 地图投影
		data_source["ZTXX_DGJ"] = atoi(vMetaInfo[35].strValue.c_str());			// 等高距
		data_source["ZTXX_ZQSM"] = convertEncoding(vMetaInfo[36].strValue, "gbk", "utf-8");		// 政区说明
		break;

		// 地名数据
	case 3:

		break;

		// 实景数据
	case 4:

		break;

		// 地质数据
	case 5:

		break;

		// 航空图数据
	case 6:

		break;

		// 倾斜摄影数据
	case 7:

		break;

		// 专用标绘
	case 8:

		break;

		// 地下管网
	case 9:

		break;

		// 高空气象
	case 10:

		break;

		// 地面气象
	case 11:

		break;

	default:
		break;
	}


	

	// 将数据源添加到列表中
	j["data_source_list"].push_back(data_source);


	// 将 JSON 对象写入文件
	std::ofstream file(strFile.c_str());
	if (file.is_open())
	{
		// 以 4 个空格缩进格式化输出
		file << j.dump(4);
		file.close();
	}

}