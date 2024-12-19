#include "se_pbf2gpkg.h"
/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"
#include "qgsmessagelog.h"

/*--------------第三方库---------------*/
#include <iconv.h>

/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>

#include <QtCore/QProcess>

#include "commontype/se_config.h"
/*--------------标准库---------------*/
#include <filesystem>
#include <vector>
#include <tchar.h>
#include <algorithm>
/*-------------------------------*/
#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>

#define _atoi64(val)   strtoll(val,NULL,10)

#endif // 

#ifdef UNICODE
#include <codecvt>
#include <locale>
#endif


CSE_pbf2gpkg_dialog::CSE_pbf2gpkg_dialog(QgisInterface* qgisInterface, QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
	, m_qgisInterface(qgisInterface)
{

	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	//	界面点击的“信号”需要对应的函数来响应
	connect(ui.Button_pbf_data_path_open, &QPushButton::clicked, this, &CSE_pbf2gpkg_dialog::Button_pbf_data_path_open);
	connect(ui.Button_gpkg_data_path_open, &QPushButton::clicked, this, &CSE_pbf2gpkg_dialog::Button_gpkg_data_path_open);
	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_pbf2gpkg_dialog::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_pbf2gpkg_dialog::Button_Cancel_rejected);

	// 界面输入信息状态改变，需要重置进度条
	connect(ui.lineEdit_pbf_data_input_path, &QLineEdit::textChanged,this, &CSE_pbf2gpkg_dialog::on_pbf_InputDataPath_TextChanged);
	connect(ui.lineEdit_gpkg_data_output_path, &QLineEdit::textChanged, this, &CSE_pbf2gpkg_dialog::on_gpkg_OutputDataPath_TextChanged);

	//	恢复上一次保存的状态（输入输出路径等等）
	restoreState();

#pragma region "工具：pbf地理空间数据文件格式向GeoPackage地理空间数据库文件格式转化"
	//	设置界面上的pbf地理空间数据输入路径
	ui.lineEdit_pbf_data_input_path->setText(m_qstr_pbf_data_input_path);

	//	设置界面上的gpkg地理空间数据输出路径
	ui.lineEdit_gpkg_data_output_path->setText(m_qstr_gpkg_data_output_path);


#pragma endregion

#pragma region "工具：其他工具"

#pragma endregion


}

CSE_pbf2gpkg_dialog::~CSE_pbf2gpkg_dialog()
{

#pragma region "工具：pbf地理空间数据文件格式向GeoPackage地理空间数据库文件格式转化"
	// 设置当前保存路径
	setState();
#pragma endregion

#pragma region "工具：其他工具"

#pragma endregion

}

void CSE_pbf2gpkg_dialog::Button_pbf_data_path_open()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择pbf矢量瓦片数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_pbf_data_input_path->setText(selectedDir);
		m_qstr_pbf_data_input_path = selectedDir;
	}
}

void CSE_pbf2gpkg_dialog::Button_gpkg_data_path_open()
{
	//	首先假设没有存在gpkg文件
	create_empty_gpkg_flag = 0;

	// 选择文件夹(使用QFileDialog::getSaveFileName方法)
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("请选择或创建gpkg数据文件");
	QString filter = "GeoPackage文件(*.gpkg)";
	QString strFileName = QFileDialog::getSaveFileName(this, dlgTitle, curPath, filter);

	if (!strFileName.isEmpty())
	{
		QFile file(strFileName);
		if (!file.exists())
		{
			// 如果文件不存在，则创建文件
			if (!file.open(QIODevice::WriteOnly))
			{
				return;
			}
			file.close();
			ui.lineEdit_gpkg_data_output_path->setText(strFileName);
			m_qstr_gpkg_data_output_path = strFileName;
			return;
		}

		create_empty_gpkg_flag = 1;
		ui.lineEdit_gpkg_data_output_path->setText(strFileName);
		m_qstr_gpkg_data_output_path = strFileName;
	}
}


void CSE_pbf2gpkg_dialog::Button_OK_accepted()
{
#pragma region "判断输入路径是否存在想要层级的子文件夹"
	//	判断当前目录下是否存在m_qstr_pbf_data_input_level对应级数的子文件夹（例如m_qstr_pbf_data_input_level = 2，那么子文件夹的名称为2）
	//	将存在的子文件夹的全路径存放到full_path_of_existing_subfolder
	std::string full_path_of_existing_subfolder = "";
	int is_exist_pbf_data_input_level_flag = is_exist_pbf_data_input_level(
		get_pbf_input_data_path(),
		get_pbf_data_input_level(),
		full_path_of_existing_subfolder);
	change_path_symbol(full_path_of_existing_subfolder);
	//	如果存在对应级数的子文件夹，那么循环处理子文件夹中的每一个文件夹中的数据
	if (is_exist_pbf_data_input_level_flag == 1)
	{
		//	错误管理
		QString qstrTitle = tr("地理空间数据处理小工具集合");
		QString qstrText = tr("pbf地理空间数据路径不存在！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}
	else if(is_exist_pbf_data_input_level_flag == 2)
	{
		//	错误管理
		QString qstrTitle = tr("地理空间数据处理小工具集合");
		QString qstrText = tr("pbf路径下为没有输入层级对应的子文件夹！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}
	else if(is_exist_pbf_data_input_level_flag == 3)
	{
		//	错误管理
		QString qstrTitle = tr("地理空间数据处理小工具集合");
		QString qstrText = tr("输入pbf数据的组织方式可能不对，需要三级目录方式！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}

	//	代码执行到这里说明存在对应的子文件夹，对full_path_of_existing_subfolder文件中的每一个子文件夹循环进行处理
	std::vector<std::vector<std::string>> all_pbf_file_paths;
	//	首先将full_path_of_existing_subfolder文件夹中的所有子文件夹中的pbf文件路径保存在all_pbf_file_paths中
	int get_all_pbf_file_paths_flag = get_all_pbf_file_paths(full_path_of_existing_subfolder, all_pbf_file_paths);
	
	//	pbf文件总数（为了计算总的pbf总数）
	int pbf_file_total_number = 0;
	for (int i = 0; i < all_pbf_file_paths.size(); i++)
	{
		for (int j = 0; j < all_pbf_file_paths[i].size(); j++)
		{
			pbf_file_total_number++;
		}
	}

	if (all_pbf_file_paths.size() > 0)
	{
		int pbf_file_number = 0;
		//	循环处理每一个子文件夹中的pbf文件
		for (int index_subfolder = 0; index_subfolder < all_pbf_file_paths.size(); index_subfolder++)
		{
			for (int index_subfolder_file = 0; index_subfolder_file < all_pbf_file_paths[index_subfolder].size(); index_subfolder_file++)
			{
				if (create_empty_gpkg_flag == 0)
				{
					/*
						首先删除使用QFileDialog::getSaveFileName方法创建的文件,因为使用QFileDialog::getSaveFileName方法创建的文件
					只是一个文本文件，内部没有特别的“标识”说明文件是一个*.gpkg文件，这里使用GDAL中的ogr2ogr工具创建一个空的*.gpkg文件
					*/
					std::string delete_file_path = get_gpkg_output_data_path();
					int result_deleteFile = deleteFile(delete_file_path);
					if (result_deleteFile == 0)
					{
						QProcess* pProcess = new QProcess();
						if (!pProcess)
						{
							//	进程创建失败
							std::string strMsg1 = "pbf文件格式向GeoPackage文件格式转化！";
							QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
							std::string strTag1 = "地理空间数据处理小工具集合插件";
							QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
							QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
							return;
						}
						std::string to_unicode_pbf_input_data_path = all_pbf_file_paths[index_subfolder][index_subfolder_file];
						std::string utf8_pbf_input_data_path = ConvertEncoding(to_unicode_pbf_input_data_path, "GBK", "UTF-8");

						std::string to_unicode_gpkg_output_data_path = get_gpkg_output_data_path();
						std::string utf8_gpkg_output_data_path = ConvertEncoding(to_unicode_gpkg_output_data_path, "GBK", "UTF-8");

						//	删除使用QFileDialog::getSaveFileName方法创建的文件成功，现在需要使用GDAL中的ogr2ogr工具创建一个空的*.gpkg文件
						QString qstr_pbf_data_input_path = QString::fromStdString(utf8_pbf_input_data_path.c_str());
						QString qstr_gpkg_data_output_path = QString::fromStdString(utf8_gpkg_output_data_path.c_str());

						QStringList strList;
						strList << "-f";
						strList << "GPKG";
						strList << "-overwrite";
						strList << "-progress";	//	需要能够进行快速统计feature个数的能力
						strList << qstr_gpkg_data_output_path;
						strList << qstr_pbf_data_input_path;
						strList << "******";	//	为了创建一个空的*.gpkg文件，使用这种方式在数据中查找这种“属性”的要素
						strList << "-t_srs";
						strList << "EPSG:4326";
						strList << "-s_srs";
						strList << "EPSG:3857";
						strList << "-skipfailures";

						// 获取"ogr2ogr.exe"所在路径
						TCHAR szModuleFileName[_MAX_PATH];

						if (NULL == GetModuleFileName(NULL, szModuleFileName, MAX_PATH)) //获得当前进程的文件路径
						{
							delete pProcess;
							return;
						}

						std::string strModuleFileName = TChar2String(szModuleFileName);

						change_path_symbol(strModuleFileName);

						int iIndex = strModuleFileName.find_last_of("/");

						std::string strModulePath = strModuleFileName.substr(0, iIndex);

						strModulePath += "/ogr2ogr.exe";

						// ogr2ogr.exe"全路径
						QString strExePath(strModulePath.c_str());

						pProcess->start(strExePath, strList);

						bool bResult = false;
						bResult = pProcess->waitForFinished(-1);
						if (!bResult)
						{
							// 获取标准错误输出
							QString errorOutput = pProcess->readAllStandardError();

							// 在这里处理错误信息，例如记录到日志
							QgsMessageLog::logMessage(errorOutput, "ogr2ogr错误", Qgis::Critical);

							delete pProcess;
							return;
						}
						//	释放资源（不要返回）
						delete pProcess;
					}
					else if (result_deleteFile == 1)
					{
						//	错误处理
						QString qstrTitle = tr("地理空间数据处理小工具集合");
						QString qstrText = tr("删除使用QFileDialog::getSaveFileName方法创建的文件不存在！");
						QMessageBox msgBox;
						msgBox.setWindowTitle(qstrTitle);
						msgBox.setText(qstrText);
						msgBox.setStandardButtons(QMessageBox::Yes);
						msgBox.setDefaultButton(QMessageBox::Yes);
						msgBox.exec();
						ui.Button_OK->setEnabled(true);
						return;
					}
					else if (result_deleteFile == 2)
					{
						//	错误处理
						QString qstrTitle = tr("地理空间数据处理小工具集合");
						QString qstrText = tr("删除使用QFileDialog::getSaveFileName方法创建的文件失败！");
						QMessageBox msgBox;
						msgBox.setWindowTitle(qstrTitle);
						msgBox.setText(qstrText);
						msgBox.setStandardButtons(QMessageBox::Yes);
						msgBox.setDefaultButton(QMessageBox::Yes);
						msgBox.exec();
						ui.Button_OK->setEnabled(true);
						return;
					}

					//	改变flag，意味着已经创建好了空的gpkg文件
					create_empty_gpkg_flag = 1;

					//	利用GDAL提供的ogr2ogr工具对第一个pbf文件再处理一次，前面的处理是为了创建空的gpkg文件(进行字符串编码转化)
					std::string to_unicode_pbf_input_data_path = all_pbf_file_paths[index_subfolder][index_subfolder_file];
					std::string utf8_pbf_input_data_path = ConvertEncoding(to_unicode_pbf_input_data_path, "GBK", "UTF-8");

					std::string to_unicode_gpkg_output_data_path = get_gpkg_output_data_path();
					std::string utf8_gpkg_output_data_path = ConvertEncoding(to_unicode_gpkg_output_data_path, "GBK", "UTF-8");
					
					convert_each_pbf2gpkg(utf8_pbf_input_data_path, utf8_gpkg_output_data_path);

					pbf_file_number++;
					ui.progressBar->setValue(((double)pbf_file_number) / (double)pbf_file_total_number * 100);
				}
				else
				{
					std::string to_unicode_pbf_input_data_path = all_pbf_file_paths[index_subfolder][index_subfolder_file];
					std::string utf8_pbf_input_data_path = ConvertEncoding(to_unicode_pbf_input_data_path, "GBK", "UTF-8");
	
					std::string to_unicode_gpkg_output_data_path = get_gpkg_output_data_path();
					std::string utf8_gpkg_output_data_path = ConvertEncoding(to_unicode_gpkg_output_data_path, "GBK", "UTF-8");
					
					convert_each_pbf2gpkg(utf8_pbf_input_data_path, utf8_gpkg_output_data_path);

					pbf_file_number++;
					ui.progressBar->setValue(((double)pbf_file_number) / (double)pbf_file_total_number * 100);

				}
			}
		}
	}


#pragma endregion
	ui.Button_OK->setEnabled(true);
	//	将这次设置的“参数保存下来”
	setState();

}

void CSE_pbf2gpkg_dialog::Button_Cancel_rejected()
{
	reject();
}

void CSE_pbf2gpkg_dialog::on_pbf_InputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_pbf2gpkg_dialog::on_gpkg_OutputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}


void CSE_pbf2gpkg_dialog::setState()
{
	QgsSettings settings;
	settings.setValue(QStringLiteral("pbf2gpkg/pbf_input_data_path"), m_qstr_pbf_data_input_path, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("pbf2gpkg/gpkg_output_data_path"), m_qstr_gpkg_data_output_path, QgsSettings::Section::Plugins);
}

void CSE_pbf2gpkg_dialog::restoreState()
{
	QgsSettings settings;
	m_qstr_pbf_data_input_path = settings.value(QStringLiteral("pbf2gpkg/pbf_input_data_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstr_gpkg_data_output_path = settings.value(QStringLiteral("pbf2gpkg/gpkg_output_data_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}

// 判断目录是否存在
bool CSE_pbf2gpkg_dialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_pbf2gpkg_dialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}

std::string CSE_pbf2gpkg_dialog::get_pbf_input_data_path()
{
	QString qstr = m_qstr_pbf_data_input_path;
	QByteArray byteArray = qstr.toLocal8Bit();
	std::string stdString = byteArray.data();
	change_path_symbol(stdString);
	return stdString;
}

std::string CSE_pbf2gpkg_dialog::get_gpkg_output_data_path()
{
	QString qstr = m_qstr_gpkg_data_output_path;
	QByteArray byteArray = qstr.toLocal8Bit();
	std::string stdString = byteArray.data();
	change_path_symbol(stdString);
	return stdString;
}

std::string CSE_pbf2gpkg_dialog::get_pbf_data_input_level()
{
	//	设置输入pbf瓦片级数
	m_qstr_pbf_data_input_level = ui.comboBox->currentText();
	QString qstr = m_qstr_pbf_data_input_level;
	QByteArray byteArray = qstr.toLocal8Bit();
	std::string stdString = byteArray.data();
	change_path_symbol(stdString);
	return stdString;
}

std::string CSE_pbf2gpkg_dialog::get_specific_layer_name()
{
	//	设置输入数据源中的一个特定图层或要素类
	m_qstr_specific_layer_name = ui.comboBox_layer_name->currentText();
	QString qstr = m_qstr_specific_layer_name;
	QByteArray byteArray = qstr.toLocal8Bit();
	std::string stdString = byteArray.data();
	change_path_symbol(stdString);
	return stdString;
}

int CSE_pbf2gpkg_dialog::is_exist_pbf_data_input_level(
	const std::string& pbf_input_data_path,
	const std::string& pbf_data_input_level,
	std::string& full_path_of_existing_subfolder)
{
/*
错误管理：
	0	存在
	1	pbf_input_data_path路径不存在
	2	pbf_input_data_path路径下为没有pbf_data_input_level对应的子文件夹
	3	其他错误
*/
	//	如果pbf_input_data_path路径下存在名为pbf_data_input_level子文件夹，则将这个子文件夹的全路径存放到full_path_of_existing_subfolder

	// 检查pbf_input_data_path是否存在
	if (!(std::filesystem::exists(pbf_input_data_path)))
	{
		return 1; // 路径不存在
	}

	// 检查pbf_input_data_path是否是一个目录
	if (!(std::filesystem::is_directory(pbf_input_data_path)))
	{
		return 2; // 路径不是目录，或没有子文件夹
	}

	// 遍历pbf_input_data_path路径下的所有条目
	for (const auto& entry : std::filesystem::directory_iterator(pbf_input_data_path))
	{
		if (is_directory(entry) && entry.path().filename() == pbf_data_input_level)
		{
			// 找到了匹配的子文件夹
			full_path_of_existing_subfolder = entry.path().string();
			return 0; // 成功找到子文件夹
		}
	}

	return 3; // 其他错误，如子文件夹不存在
}

int CSE_pbf2gpkg_dialog::get_all_pbf_file_paths(
	const std::string& input_data_path,
	std::vector<std::vector<std::string>>& all_pbf_file_paths)
{
/*
* 1、首先判断input_data_path是否存在，如果不存在则返回1
* 2、判断input_data_path中是否存在子文件夹，如果不存在，则返回2
* 3、将input_data_path中子文件夹中的所有*.pbf文件的路径存放到all_pbf_file_paths中，每一个vector<string>代表input_data_path中的一个子文件夹
*/

	// 1. 检查input_data_path是否存在
	if (!(std::filesystem::exists(input_data_path)))
	{
		// 路径不存在
		return 1; 
	}

	// 2. 检查input_data_path是否包含子文件夹
	bool has_subdirectory = false;
	for (const auto& entry : std::filesystem::directory_iterator(input_data_path))
	{
		if (is_directory(entry)) {
			has_subdirectory = true;
			break;
		}
	}
	if (!has_subdirectory) {
		// 没有子文件夹
		return 2;
	}

	// 3. 遍历子文件夹并收集.pbf文件的路径
	for (const auto& entry : std::filesystem::directory_iterator(input_data_path))
	{
		if (is_directory(entry))
		{
			std::vector<std::string> pbf_files;
			for (const auto& file : std::filesystem::directory_iterator(entry.path()))
			{
				if (file.path().extension() == ".pbf")
				{
					std::string single_pbf_file_path = file.path().string();
					change_path_symbol(single_pbf_file_path);
					pbf_files.push_back(single_pbf_file_path);
				}
			}
			if (!pbf_files.empty()) {
				all_pbf_file_paths.push_back(pbf_files);
			}
		}
	}
	// 成功
	return 0;
}

int CSE_pbf2gpkg_dialog::convert_each_pbf2gpkg(
	const std::string &pbf_data_input_path, 
	const std::string &gpkg_data_output_path)
{
	QProcess* pProcess = new QProcess();
	if (!pProcess)
	{
		//	进程创建失败
		std::string strMsg1 = "pbf文件格式向GeoPackage文件格式转化！";
		QString qstrMsg1 = QString::fromLocal8Bit(strMsg1.c_str());
		std::string strTag1 = "地理空间数据处理小工具集合插件";
		QString qstrTag1 = QString::fromLocal8Bit(strTag1.c_str());
		QgsMessageLog::logMessage(qstrMsg1, qstrTag1, Qgis::Critical);
		return 1;
	}
	//	将输出的gpkg文件路径进行转化
	std::string m_pbf_data_input_path = pbf_data_input_path;
	std::string m_gpkg_data_output_path = gpkg_data_output_path;
	change_path_symbol(m_pbf_data_input_path);
	change_path_symbol(m_gpkg_data_output_path);

	QString qstr_pbf_data_input_path = QString::fromStdString(m_pbf_data_input_path.c_str());
	QString qstr_gpkg_data_output_path = QString::fromStdString(m_gpkg_data_output_path.c_str());
	QString qstr_specific_layer_name = QString::fromStdString(get_specific_layer_name());

	QStringList strList;
	strList << "-f";
	strList << "GPKG";
	strList << "-append";
	strList << "-update";
	strList << "-progress";
	strList << qstr_gpkg_data_output_path;
	strList << qstr_pbf_data_input_path;
	strList << qstr_specific_layer_name;	//	设定数据源中的一个特定图层或要素
	strList << "-t_srs";
	strList << "EPSG:4326";
	strList << "-s_srs";
	strList << "EPSG:3857";
	strList << "-skipfailures";
	

	// 获取"ogr2ogr.exe"所在路径
	TCHAR szModuleFileName[_MAX_PATH];

	if (NULL == GetModuleFileName(NULL, szModuleFileName, MAX_PATH)) //获得当前进程的文件路径
	{
		delete pProcess;
		return 3;
	}

	std::string strModuleFileName = TChar2String(szModuleFileName);

	change_path_symbol(strModuleFileName);

	int iIndex = strModuleFileName.find_last_of("/");

	std::string strModulePath = strModuleFileName.substr(0, iIndex);

	strModulePath += "/ogr2ogr.exe";

	// ogr2ogr.exe"全路径
	QString strExePath(strModulePath.c_str());

	pProcess->start(strExePath, strList);

	bool bResult = false;
	bResult = pProcess->waitForFinished(-1);
	if (!bResult)
	{
		// 获取标准错误输出
		QString errorOutput = pProcess->readAllStandardError();

		// 在这里处理错误信息，例如记录到日志
		QgsMessageLog::logMessage(errorOutput, "ogr2ogr错误", Qgis::Critical);

		delete pProcess;
		return 3;
	}

	delete pProcess;
	return 0;
}

std::string CSE_pbf2gpkg_dialog::TChar2String(TCHAR* tcharArray)
{

	std::string str;

#ifdef UNICODE
	// 使用 wstring_convert 来转换 wchar_t* 到 string
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	str = converter.to_bytes(tcharArray);
#else
	// 对于多字节字符集，直接赋值即可
	str = tcharArray;
#endif
	return str;
}

void CSE_pbf2gpkg_dialog::change_path_symbol(std::string &path)
{
/*
* 想要将path中存在的\全部替换成/，例如D:\gitdir\qgis\plugins替换成D:/gitdir/qgis/plugins
*/
	std::replace(path.begin(), path.end(), '\\', '/');
	return;
}

int CSE_pbf2gpkg_dialog::deleteFile(const std::string& filePath)
{

	// 检查文件是否存在
	if (!(std::filesystem::exists(filePath)))
	{
		//	文件不存在
		return 1;
	}

	// 尝试删除文件
	if (!(std::filesystem::remove(filePath)))
	{
		//	文件删除失败
		return 2;
	}
	//	文件删除成功
	return 0;
}

std::string CSE_pbf2gpkg_dialog::ConvertEncoding(const std::string& input, const std::string& fromEncoding, const std::string& toEncoding)
{
	iconv_t cd = iconv_open(toEncoding.c_str(), fromEncoding.c_str());
	if (cd == (iconv_t)-1) {
		throw std::runtime_error("iconv_open failed");
	}

	size_t inBytesLeft = input.size();
	size_t outBytesLeft = input.size() * 4; // 大致估算输出缓冲区大小
	std::vector<char> outputBuffer(outBytesLeft);

	char* inBuf = (char*)input.data();
	char* outBuf = outputBuffer.data();

	size_t result = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft);
	if (result == (size_t)-1) {
		iconv_close(cd);
		throw std::runtime_error("iconv failed");
	}

	iconv_close(cd);
	return std::string(outputBuffer.data());
}




