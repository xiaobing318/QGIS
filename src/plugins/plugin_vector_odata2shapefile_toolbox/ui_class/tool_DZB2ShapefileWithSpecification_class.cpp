#pragma region "包含头文件（减少重复）"
/*--------------STL---------------*/
#include <filesystem>
#include <fstream>
#include <iostream>
/*--------------STL---------------*/

/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>
#include <QTimer>
/*--------------QT---------------*/

/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"
/*--------------QGIS---------------*/

/*--------------自定义---------------*/
#include "tool_DZB2ShapefileWithSpecification_class.h"
/*--------------自定义---------------*/

/*********添加多线程并行，增加速度*********/
#ifdef OS_FAMILY_WINDOWS
  #include "windows.h"
#else
  #include "unistd.h"
#endif

#pragma endregion

#pragma region "多线程并行处理时使用的结构体"
/*********添加多线程并行，增加速度*********/
struct FromToIndex
{
	int iFromIndex;		// 起始图幅索引
	int iToIndex;		  // 终止图幅索引

	FromToIndex()
	{
		iFromIndex = 0;
		iToIndex = 0;
	}
};
#pragma endregion

#pragma region "DZB2ShapefileWithSpecification 转化对话类相关的函数"
ToolDZB2ShapefileWithSpecificationDialog::ToolDZB2ShapefileWithSpecificationDialog(
  QgisInterface* qgisInterface,
  QWidget* parent,
  Qt::WindowFlags fl)
	: QDialog(parent, fl),
  m_qgisInterface(qgisInterface),
  total_processed_frame_data_count(0),
  total_successfully_processed_frame_data_count(0),
  m_isHandlingResults(false)
{

	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_Save, &QPushButton::clicked, this, &ToolDZB2ShapefileWithSpecificationDialog::Button_Save_clicked);
	connect(ui.Button_Open, &QPushButton::clicked, this, &ToolDZB2ShapefileWithSpecificationDialog::Button_Open_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &ToolDZB2ShapefileWithSpecificationDialog::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &ToolDZB2ShapefileWithSpecificationDialog::Button_Cancel_rejected);
	connect(ui.Button_summary_log_saving_path, &QPushButton::clicked, this, &ToolDZB2ShapefileWithSpecificationDialog::Button_summary_log_saving_path_clicked);

	// 界面输入信息状态改变，需要重置进度条
	connect(ui.lineEdit_InputDataPath, &QLineEdit::textChanged,this, &ToolDZB2ShapefileWithSpecificationDialog::on_InputDataPath_TextChanged);
	connect(ui.lineEdit_OutputDataPath, &QLineEdit::textChanged, this, &ToolDZB2ShapefileWithSpecificationDialog::on_lineEdit_OutputDataPath_TextChanged);
	connect(ui.radioButton_OriginSRS, &QRadioButton::clicked, this, &ToolDZB2ShapefileWithSpecificationDialog::on_radioButton_OriginSRS_clicked);
	connect(ui.radioButton_GeoSRS, &QRadioButton::clicked, this, &ToolDZB2ShapefileWithSpecificationDialog::on_radioButton_GeoSRS_clicked);
	connect(ui.radioButton_ProjSRS, &QRadioButton::clicked, this, &ToolDZB2ShapefileWithSpecificationDialog::on_radioButton_ProjSRS_clicked);
	connect(ui.lineEdit_OffsetX, &QLineEdit::textChanged, this, &ToolDZB2ShapefileWithSpecificationDialog::on_lineEdit_OffsetX_TextChanged);
	connect(ui.lineEdit_OffsetY, &QLineEdit::textChanged, this, &ToolDZB2ShapefileWithSpecificationDialog::on_lineEdit_OffsetY_TextChanged);
	// 获取汇总日志保存路径
	connect(ui.lineEdit_summary_log_saving_path, &QLineEdit::textChanged, this, &ToolDZB2ShapefileWithSpecificationDialog::on_summary_log_saving_path_TextChanged);

	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputDataPath->setText(m_qstrSaveDataPath);
	ui.lineEdit_summary_log_saving_path->setText(m_summary_log_saving_path);

#pragma region "计算比例尺并且自动填充比例尺分母文本框"
	// 根据所选的odata目录，自动填充比例尺分母文本框
	QStringList qstrSubDir = GetSubDirPathOfCurrentDir(m_qstrInputDataPath);
	// 如果当前输入odata目录为单个图幅
	if (qstrSubDir.size() == 0)
	{
		// 获取图幅名称，获取最后一级文件夹目录
		QString qstrFolderName;
		GetFolderNameFromPath(m_qstrInputDataPath, qstrFolderName);

		QByteArray qFolderName = qstrFolderName.toLocal8Bit();
		string strFolderName = string(qFolderName);

		int iScale = GetScale(strFolderName);

		QString qstrScale = tr("%1").arg(iScale);

		ui.lineEdit_Scale->setText(qstrScale);

	}
	// 如果输入目录包括多个图幅，取第一个图幅计算比例尺
	else
	{
		// 获取图幅名称（选取其中的一个子文件夹即分幅名称，从而计算比例尺），获取最后一级文件夹目录
		QString qstrFolderName;
		GetFolderNameFromPath(qstrSubDir[0], qstrFolderName);

		QByteArray qFolderName = qstrFolderName.toLocal8Bit();
		string strFolderName = string(qFolderName);

		int iScale = GetScale(strFolderName);

		QString qstrScale = tr("%1").arg(iScale);

		ui.lineEdit_Scale->setText(qstrScale);
	}
#pragma endregion

	// 默认输出与原始数据空间参考系一致
	ui.radioButton_OriginSRS->setChecked(true);

	// 默认X偏移量1000，Y偏移量1000
	ui.lineEdit_OffsetX->setText(tr("1000"));

	ui.lineEdit_OffsetY->setText(tr("1000"));

	//	默认选择从SMS文件中读取放大系数
	ui.radioButton_read_from_sms_file->setChecked(true);

	//	默认选择从SMS文件中读取图层信息，而不是从实际目录下的图层信息
	ui.radioButton_read_layer_info_from_sms_file->setChecked(true);

	//	默认日志器等级位err(一般错误)
	ui.comboBox_level->setCurrentIndex(5);

	InitLineEdit();

	m_pThread = nullptr;
}

ToolDZB2ShapefileWithSpecificationDialog::~ToolDZB2ShapefileWithSpecificationDialog()
{
	if (m_pThread)
	{
		delete[] m_pThread;
		m_pThread = nullptr;
	}

  //	将这次设置的“参数保存下来”
  saveState();
}


int ToolDZB2ShapefileWithSpecificationDialog::GetOutputSRS()
{
	// 1——与输入数据保持一致；2——输出地理坐标系；3——输出投影坐标系
	if (ui.radioButton_OriginSRS->isChecked())
	{
		return 1;
	}
	else if(ui.radioButton_GeoSRS->isChecked())
	{
		return 2;
	}
	else if (ui.radioButton_ProjSRS->isChecked())
	{
		return 3;
	}

	// 默认为1
	return 1;
}

void ToolDZB2ShapefileWithSpecificationDialog::Button_Open_clicked()
{
	// 选择文件夹（使用上次使用保存下来的路径）
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = tr("请选择odata数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputDataPath->setText(selectedDir);
    // 对获取得到的路径进行处理，应该将输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    m_qstrInputDataPath = QString::fromUtf8(utf8Path.data());

		// 根据所选的odata目录，自动填充比例尺分母文本框
		// 如果当前输入odata目录为单个图幅
		QStringList qstrSubDir = GetSubDirPathOfCurrentDir(m_qstrInputDataPath);
		if (qstrSubDir.size() == 0)
		{
			// 获取图幅名称，获取最后一级文件夹目录
			QString qstrFolderName;
			GetFolderNameFromPath(m_qstrInputDataPath, qstrFolderName);

			//QByteArray qFolderName = qstrFolderName.toLocal8Bit();
      QByteArray qFolderName = qstrFolderName.toUtf8();
      string strFolderName = string(qFolderName);

			int iScale = GetScale(strFolderName);

			QString qstrScale = tr("%1").arg(iScale);

			ui.lineEdit_Scale->setText(qstrScale);

		}

		// 如果输入目录包括多个图幅，取第一个图幅计算比例尺
		else
		{
			// 获取图幅名称，获取最后一级文件夹目录
			QString qstrFolderName;
			GetFolderNameFromPath(qstrSubDir[0], qstrFolderName);

			//QByteArray qFolderName = qstrFolderName.toLocal8Bit();
      QByteArray qFolderName = qstrFolderName.toUtf8();
      string strFolderName = string(qFolderName);

			int iScale = GetScale(strFolderName);

			QString qstrScale = tr("%1").arg(iScale);

			ui.lineEdit_Scale->setText(qstrScale);
		}

	}
}

void ToolDZB2ShapefileWithSpecificationDialog::Button_Save_clicked()
{
	// 选择文件夹（使用上次使用保存下来的路径）
	QString curPath = m_qstrSaveDataPath;
	QString dlgTile = tr("请选择数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OutputDataPath->setText(selectedDir);
    // 对获取得到的路径进行处理，应该将输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();

		m_qstrSaveDataPath = QString::fromUtf8(utf8Path.data());
	}
}

void ToolDZB2ShapefileWithSpecificationDialog::Button_summary_log_saving_path_clicked()
{
	// 选择文件夹(使用QFileDialog::getSaveFileName方法)，选择文件夹（使用上次使用保存下来的路径）
	QString curPath = m_summary_log_saving_path;
	QString dlgTitle = tr("请选择或创建汇总日志数据文件");
	QString filter = "log日志文件(*.txt)";
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
			ui.lineEdit_summary_log_saving_path->setText(strFileName);
      // 对获取得到的路径进行处理，应该将输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
      QString processedPath = strFileName.replace("\\", "/");
      QByteArray utf8Path = processedPath.toUtf8();
			m_summary_log_saving_path = QString::fromUtf8(utf8Path.data());
			return;
		}
		ui.lineEdit_summary_log_saving_path->setText(strFileName);
    //  对获取得到的路径进行处理，应该将输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = strFileName.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    m_summary_log_saving_path = QString::fromUtf8(utf8Path.data());
	}

}

/*获取在指定目录下的目录的路径*/
QStringList ToolDZB2ShapefileWithSpecificationDialog::GetSubDirPathOfCurrentDir(QString dirPath)
{
	QStringList dirPaths;
	QDir splDir(dirPath);
	QFileInfoList fileInfoListInSplDir = splDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	QFileInfo tempFileInfo;
	foreach(tempFileInfo, fileInfoListInSplDir)
  {
    // 应该将输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = tempFileInfo.absoluteFilePath().replace("\\", "/");
		dirPaths << processedPath;
	}
	return dirPaths;
}

void ToolDZB2ShapefileWithSpecificationDialog::Button_OK_accepted()
{
  //	设置了用户界面参数，可能会运行失败，在执行具体的逻辑之前先将参数保存下来
  saveState();

#pragma region "重置统计信息"
  this->total_processed_frame_data_count = 0;
  this->total_successfully_processed_frame_data_count = 0;
  this->m_isHandlingResults = false;
#pragma endregion

#pragma region "【1】获取图形界面上输入的参数并且验证其是否有效"
  //  验证输入数据路径
  if (!FilePathIsExisted(GetInputDataPath()))
  {
    QString qstrTitle = tr("线划图数据格式转换");
    QString qstrText = tr("输入数据路径不存在！");
    QMessageBox msgBox;
    msgBox.setWindowTitle(qstrTitle);
    msgBox.setText(qstrText);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    ui.Button_OK->setEnabled(true);
    //  没有动态使用内存，因此不需要释放内容
    return;
  }
  //  验证输出数据路径
  if (!FilePathIsExisted(GetOutputDataPath()))
  {
    QString qstrTitle = tr("线划图数据格式转换");
    QString qstrText = tr("输出数据路径不存在！");
    QMessageBox msgBox;
    msgBox.setWindowTitle(qstrTitle);
    msgBox.setText(qstrText);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    ui.Button_OK->setEnabled(true);
    //  没有动态使用内存，因此不需要释放内容
    return;
  }
  //  验证输入输出数据路径是否相等
  if (GetInputDataPath() == GetOutputDataPath())
  {
    QString qstrTitle = tr("线划图数据格式转换");
    QString qstrText = tr("输入和输出数据路径不能相同！");
    QMessageBox msgBox;
    msgBox.setWindowTitle(qstrTitle);
    msgBox.setText(qstrText);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    ui.Button_OK->setEnabled(true);
    //  没有动态使用内存，因此不需要释放内容
    return;
  }

  //  获取“输出参考系”参数
	int iOutputSRS = GetOutputSRS();
  //  获取“获取图层信息”参数
	int method_of_obtaining_layer_info = get_the_method_of_obtaining_layer_information();
	//	获得设置放大系数的方式（从sms文件中读取还是自定义输入）
	int iSettingMethod = get_setting_zoomscale_method();
  //  获取“缩放系数”参数
	double dzoomscale = get_dzoomscale();
	//	spdlog日志器等级设置（默认为trace级别：最为详细的级别；全部的级别：trace、debug、info、warn、err、critical、off）
	spdlog::level::level_enum log_level = get_log_level();
  //  重置进度条信息
	ui.progressBar->reset();
#pragma endregion

#pragma region "【2】根据不同的分幅数据数量以及CPU核数来创建不同数量的线程并行处理线划图格式转化"


#pragma region "一、多线程并行处理（1、宏定义控制编译；2、手动控制编译；3、交互界面控制）"
  //  多线程修改为单线程

/*
	//  获取计算机CPU核数
	int iNumberOfProcessors = 0;
#ifdef OS_FAMILY_WINDOWS
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	iNumberOfProcessors = sysInfo.dwNumberOfProcessors;
#else // OS_FAMILY_UNIX
	// 获取当前系统的可用CPU核数
	iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);
#endif
  //  创建多个线程
	m_pThread = new SE_Odata2shpThread[iNumberOfProcessors];
  //  如果没有创建成功，则需要进行提示
	if (!m_pThread)
	{
		QString qstrTitle = tr("线划图数据格式转换");
		QString qstrText = tr("创建格式转换线程失败！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
    //  没有创建多个线程，那么不需要释放内容
		return;
	}

	// 计算输入图幅所在目录下图幅个数，按照CPU核数平均分配图幅，如果当前输入odata目录为单个图幅
  QStringList qstrSubDir = GetSubDirPathOfCurrentDir(m_qstrInputDataPath);
  //  获取输入目录中有多少子文件夹
  int iSheetCount = qstrSubDir.size();
  //  将获取得到的子文件夹数量赋值给类数据成员
	m_SheetCount = iSheetCount;
	//  计算每个CPU平均分配的图幅个数
	int iSheetCountPerCpuKernal = int(iSheetCount / (iNumberOfProcessors * 1.0));
	//  如果图幅数小于核数
	if (iSheetCountPerCpuKernal < 1)
	{
		for (int i = 0; i < iSheetCount; ++i)
		{
			QStringList qstrList;
			qstrList << qstrSubDir[i];

			//  关联线程进度信号槽
			connect(&m_pThread[i], &SE_Odata2shpThread::resultProcess, this, &ToolDZB2ShapefileWithSpecificationDialog::handleResults);
			//
			m_pThread[i].SetThreadParams(
				qstrList,
        m_qstrSaveDataPath,
				iOutputSRS,
				ui.lineEdit_OffsetX->text().toDouble(),
				ui.lineEdit_OffsetY->text().toDouble(),
				method_of_obtaining_layer_info,
				iSettingMethod,
				dzoomscale,
				log_level);
		}
	}
  //  如果图幅数大于核数
	else
	{
    //  创建索引
		FromToIndex* pIndex = new FromToIndex[iNumberOfProcessors];
    //  如果创建索引结构体失败则对用户进行提示
    if (!pIndex)
    {
      QString qstrTitle = tr("线划图数据格式转换");
      QString qstrText = tr("线划图数据格式转换：为不同core创建索引失败！");
      QMessageBox msgBox;
      msgBox.setWindowTitle(qstrTitle);
      msgBox.setText(qstrText);
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      ui.Button_OK->setEnabled(true);
      //  这里需要释放创建出来的多个线程
      delete[] m_pThread;
      return;
    }
    //
		for (int i = 0; i < iNumberOfProcessors; ++i)
		{
			//  如果不是最后一个
			if (i != iNumberOfProcessors - 1)
			{
				pIndex[i].iFromIndex = iSheetCountPerCpuKernal * i;
				pIndex[i].iToIndex = iSheetCountPerCpuKernal * (i + 1) - 1;
			}
			//  如果是最后一个
			else
			{
				pIndex[i].iFromIndex = iSheetCountPerCpuKernal * i;
				pIndex[i].iToIndex = iSheetCount - 1;
			}
		}
    //  创建文件列表，用来存储不同core中的子文件夹
		QStringList* qstrList = new QStringList[iNumberOfProcessors];
    //  如果文件列表失败则对用户进行提示
    if (qstrList)
    {
      QString qstrTitle = tr("线划图数据格式转换");
      QString qstrText = tr("线划图数据格式转换：为不同core创建文件列表失败！");
      QMessageBox msgBox;
      msgBox.setWindowTitle(qstrTitle);
      msgBox.setText(qstrText);
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      ui.Button_OK->setEnabled(true);
      //  这里需要释放创建出来的多个线程
      delete[] m_pThread;
      //  释放之前分配的索引空间
      delete[] pIndex;
      return;

    }
    //  对当前所有的cores循坏进行处理，将对应子文件夹路径存放到对应的文件列表中
		for (int i = 0; i < iNumberOfProcessors; ++i)
		{
			for (int j = pIndex[i].iFromIndex; j <= pIndex[i].iToIndex; ++j)
			{
				qstrList[i] << qstrSubDir[j];
			}
		}
    //  循环创建线程使得多线程处理
		for (int i = 0; i < iNumberOfProcessors; ++i)
		{
			// 关联线程进度信号槽
			connect(&m_pThread[i], &SE_Odata2shpThread::resultProcess, this, &ToolDZB2ShapefileWithSpecificationDialog::handleResults);

			m_pThread[i].SetThreadParams(
				qstrList[i],
        m_qstrSaveDataPath,
				iOutputSRS,
				ui.lineEdit_OffsetX->text().toDouble(),
				ui.lineEdit_OffsetY->text().toDouble(),
				method_of_obtaining_layer_info,
				iSettingMethod,
				dzoomscale,
				log_level);
		}
    //  释放之前分配的索引空间
    delete[] pIndex;
	}
  //  释放分配的多线程空间
  delete[] m_pThread;
*/

#pragma endregion

#pragma region "二、单线程并行处理1、宏定义控制编译；2、手动控制编译；3、交互界面控制）"

#pragma region "1、创建多线程"
  //  获取计算机CPU核数（这里没有不使用多核，只是使用单线程进行处理）
  int iNumberOfProcessors = 0;
#ifdef OS_FAMILY_WINDOWS
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  iNumberOfProcessors = sysInfo.dwNumberOfProcessors;
#else // OS_FAMILY_UNIX
  // 获取当前系统的可用CPU核数
  iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);
#endif
  //  创建多个线程
  m_pThread = new TOOLDZB2ShapefileWithSpecificationThread[iNumberOfProcessors];
  //  如果没有创建成功，则需要进行提示
  if (!m_pThread)
  {
    QString qstrTitle = tr("线划图数据格式转换");
    QString qstrText = tr("创建格式转换线程失败！");
    QMessageBox msgBox;
    msgBox.setWindowTitle(qstrTitle);
    msgBox.setText(qstrText);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    ui.Button_OK->setEnabled(true);
    //  没有创建多个线程，那么不需要释放内容
    return;
  }
#pragma endregion

#pragma region "2、循环处理每一个分幅odata数据"
  //  计算输入图幅所在目录下图幅个数，按照CPU核数平均分配图幅，如果当前输入odata目录为单个图幅
  QStringList qstrSubDir = GetSubDirPathOfCurrentDir(m_qstrInputDataPath);
  //  获取输入目录中有多少子文件夹
  int iSheetCount = qstrSubDir.size();
  //  如果没有子文件夹，则提示并退出
  if (iSheetCount == 0)
  {
    QString qstrTitle = tr("线划图数据格式转换");
    QString qstrText = tr("指定文件夹中没有图幅进行转换！");
    QMessageBox msgBox;
    msgBox.setWindowTitle(qstrTitle);
    msgBox.setText(qstrText);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.exec();
    ui.Button_OK->setEnabled(true);
    //  释放资源，需要将释放内存资源后的指针赋值为空（这里暂时是不高效的，因为没有用到多线程，只是创建和销毁了）
    delete[] m_pThread;
    m_pThread = nullptr;
    return;
  }

  //  将获取得到的子文件夹数量赋值给类数据成员
  m_SheetCount = iSheetCount;
  //  计算每个CPU平均分配的图幅个数
  int iSheetCountPerCpuKernal = int(iSheetCount / (iNumberOfProcessors * 1.0));
  //  禁用OK按钮，防止用户在转换过程中关闭对话框
  ui.Button_OK->setEnabled(false);
  //  顺序处理每一个子文件夹
  for (int i = 0; i < iSheetCount; ++i)
  {
    QString currentSubDir = qstrSubDir[i];

    // 创建文件列表
    QStringList qstrList;
    qstrList << currentSubDir;

    // 创建一个临时线程对象（如果 SE_Odata2shpThread 的操作可以同步执行）
    TOOLDZB2ShapefileWithSpecificationThread thread;
    // 关联线程进度信号槽
    connect(&thread, &TOOLDZB2ShapefileWithSpecificationThread::resultProcess, this, &ToolDZB2ShapefileWithSpecificationDialog::handleResults);

    // 设置线程参数
    thread.SetThreadParams(
      qstrList,
      m_qstrSaveDataPath,
      iOutputSRS,
      ui.lineEdit_OffsetX->text().toDouble(),
      ui.lineEdit_OffsetY->text().toDouble(),
      method_of_obtaining_layer_info,
      iSettingMethod,
      dzoomscale,
      log_level);
  }
#pragma endregion

#pragma region "3、销毁多线程资源"
  //  释放资源，需要将释放内存资源后的指针赋值为空（这里暂时是不高效的，因为没有用到多线程，只是创建和销毁了）
  delete[] m_pThread;
  m_pThread = nullptr;
#pragma endregion

#pragma endregion


#pragma endregion

#pragma region "【3】处理完成后的一些结尾工作"
  //  转换完成后，重新启用OK按钮
  ui.Button_OK->setEnabled(true);
#pragma endregion

}

void ToolDZB2ShapefileWithSpecificationDialog::Button_Cancel_rejected()
{
  //	有可能设置了用户界面参数
  saveState();
	reject();
}

void ToolDZB2ShapefileWithSpecificationDialog::on_InputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void ToolDZB2ShapefileWithSpecificationDialog::on_summary_log_saving_path_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void ToolDZB2ShapefileWithSpecificationDialog::on_lineEdit_OutputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void ToolDZB2ShapefileWithSpecificationDialog::on_lineEdit_OffsetX_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void ToolDZB2ShapefileWithSpecificationDialog::on_lineEdit_OffsetY_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void ToolDZB2ShapefileWithSpecificationDialog::on_radioButton_OriginSRS_clicked(bool checked)
{
  ui.progressBar->reset();
}

void ToolDZB2ShapefileWithSpecificationDialog::on_radioButton_GeoSRS_clicked(bool checked)
{
  ui.progressBar->reset();
}

void ToolDZB2ShapefileWithSpecificationDialog::on_radioButton_ProjSRS_clicked(bool checked)
{
  ui.progressBar->reset();
}

void ToolDZB2ShapefileWithSpecificationDialog::handleResults(
  const int& processed_frame_data_flag,
  const int& iProcessed,
  const QString& s)
{
  // 防止递归调用
  if (m_isHandlingResults)
    return;
  m_isHandlingResults = true;

  // 更新处理计数
  total_processed_frame_data_count += processed_frame_data_flag;
  total_successfully_processed_frame_data_count += iProcessed;

  // 更新进度条
  ui.progressBar->setValue(int(100.0 * total_processed_frame_data_count / m_SheetCount));

  // 如果所有处理完成
  if (total_processed_frame_data_count == m_SheetCount)
  {
    // 使用 QTimer::singleShot 延迟执行后续操作
    QTimer::singleShot(0, this, SLOT(onAllProcessingCompleted()));
  }

  m_isHandlingResults = false;
}

void ToolDZB2ShapefileWithSpecificationDialog::mergeLogFiles()
{
  // 获取输入子目录列表
  QStringList qstrSubDir = GetSubDirPathOfCurrentDir(m_qstrInputDataPath);

  // 获取输出路径
  QString qst_output_path = ui.lineEdit_OutputDataPath->text();
  QByteArray qba_output_path = qst_output_path.toLocal8Bit();
  std::string str_output_path = std::string(qba_output_path);

  // 确定日志汇总保存路径
  std::string summary_log_saving_path = "";
  bool flag = FileIsExisted(get_summary_log_saving_path_qstr());
  if (flag)
  {
    // 使用用户指定的日志路径
    summary_log_saving_path = get_summary_log_saving_path_str();
  }
  else
  {
    // 使用默认的日志路径
    summary_log_saving_path = str_output_path + "/" + "summery_log.txt";
  }

  // 打开汇总日志文件（追加模式）
  std::ofstream output_log_file(summary_log_saving_path, std::ios::app);
  if (!output_log_file)
  {
    // 错误处理
    return;
  }

  for (int i = 0; i < m_SheetCount; i++)
  {
    // 获取当前分幅数据的路径
    QString qstr_current_framed_data_path = qstrSubDir[i];
    QByteArray qba_current_framed_data_path = qstr_current_framed_data_path.toLocal8Bit();
    std::string str_current_framed_data_path = std::string(qba_current_framed_data_path);

    // 获取分幅数据的图幅号
    size_t framed_data_starting_index = str_current_framed_data_path.find_last_of("/\\");
    std::string framed_data_sheet_number = str_current_framed_data_path.substr(framed_data_starting_index + 1);

    // 构建当前分幅数据的日志文件路径
    std::string str_current_output_path = str_output_path + "/" + framed_data_sheet_number;
    std::string str_current_output_log_path = str_current_output_path + "/" + framed_data_sheet_number + "_log.txt";

    // 检查日志文件是否存在
    std::filesystem::path fs_current_output_log_path = str_current_output_log_path;
    if (!std::filesystem::exists(fs_current_output_log_path))
    {
      // 文件不存在，跳过
      continue;
    }

    // 打开当前分幅数据的日志文件
    std::ifstream current_framed_data_log_file(fs_current_output_log_path);
    if (!current_framed_data_log_file)
    {
      // 无法打开文件，跳过
      continue;
    }

    // 将内容追加到汇总日志文件
    output_log_file << current_framed_data_log_file.rdbuf();

    // 关闭当前分幅数据的日志文件
    current_framed_data_log_file.close();
  }

  // 关闭汇总日志文件
  output_log_file.close();
}

void ToolDZB2ShapefileWithSpecificationDialog::onAllProcessingCompleted()
{
  // 合并日志文件
  mergeLogFiles();

  // 显示统计信息
  QString qstrTitle = tr("DZB--->Shapefile:");
  QString qstrText = tr("一共") + QString::number(m_SheetCount)
    + tr("幅分幅数据；其中成功处理了") + QString::number(total_successfully_processed_frame_data_count)
    + tr("幅分幅数据；其中") + QString::number(total_processed_frame_data_count - total_successfully_processed_frame_data_count)
    + tr("幅分幅数据处理失败了。");

  QMessageBox::information(this, qstrTitle, qstrText, QMessageBox::Yes);

  // 更新界面
  ui.Button_OK->setEnabled(true);
  ui.progressBar->setValue(100);
}

void ToolDZB2ShapefileWithSpecificationDialog::restoreState()
{
  QgsSettings settings;
  // 指定从Plugins节读取设置
  settings.beginGroup("DZB2ShapefileWithSpecification");
  m_qstrInputDataPath = settings.value(QStringLiteral("DZB2ShapefileWithSpecification/InputDataPath"), QDir::homePath()).toString();
  m_qstrSaveDataPath = settings.value(QStringLiteral("DZB2ShapefileWithSpecification/SaveDataPath"), QDir::homePath()).toString();
  m_summary_log_saving_path = settings.value(QStringLiteral("DZB2ShapefileWithSpecification/summary_log_saving_path"), QDir::homePath()).toString();
}

void ToolDZB2ShapefileWithSpecificationDialog::saveState()
{
  QgsSettings settings;
  // 指定设置保存到Plugins节
  settings.beginGroup("DZB2ShapefileWithSpecification");
  settings.setValue(QStringLiteral("DZB2ShapefileWithSpecification/InputDataPath"), m_qstrInputDataPath);
  settings.setValue(QStringLiteral("DZB2ShapefileWithSpecification/SaveDataPath"), m_qstrSaveDataPath);
  settings.setValue(QStringLiteral("DZB2ShapefileWithSpecification/summary_log_saving_path"), m_summary_log_saving_path);
  // 结束分组，非常重要，确保设置正确保存在指定的节中
  settings.endGroup();
}



QStringList ToolDZB2ShapefileWithSpecificationDialog::GetShpFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.shp" << "*.SHP";
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}

//  判断目录是否存在
bool ToolDZB2ShapefileWithSpecificationDialog::FilePathIsExisted(const QString& qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}
//  判断文件是否存在
bool ToolDZB2ShapefileWithSpecificationDialog::FileIsExisted(const QString& qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}

QString ToolDZB2ShapefileWithSpecificationDialog::GetInputDataPath()
{
	return ui.lineEdit_InputDataPath->text();
}

QString ToolDZB2ShapefileWithSpecificationDialog::GetOutputDataPath()
{
	return ui.lineEdit_OutputDataPath->text();
}


QString ToolDZB2ShapefileWithSpecificationDialog::GetScale()
{
	return ui.lineEdit_Scale->text();
}

double ToolDZB2ShapefileWithSpecificationDialog::GetOffsetX()
{
	return ui.lineEdit_OffsetX->text().toDouble();
}

double ToolDZB2ShapefileWithSpecificationDialog::GetOffsetY()
{
	return ui.lineEdit_OffsetY->text().toDouble();
}

//	获得是否选择手动输入放大系数
int ToolDZB2ShapefileWithSpecificationDialog::get_setting_zoomscale_method()
{
	//	如果点击了从SMS文件中读取选项
	if (ui.radioButton_read_from_sms_file->isChecked())
	{
		return 0;
	}
	//	如果点击了从自定义中读取选项
	else if (ui.radioButton_user_defined->isChecked())
	{
		return 1;
	}
	//	默认返回的是0：即认为是从SMS文件中读取放大系数
	return 0;
}

//	-2023-12-07：获得手动输入的放大系数数值
double ToolDZB2ShapefileWithSpecificationDialog::get_dzoomscale()
{
	return ui.lineEdit_zoom_scale->text().toDouble();
}

//  获取图层信息方式：1——从*.SMS文件中获取图层信息；2——从实际odata数据目录中获取图层信息
int ToolDZB2ShapefileWithSpecificationDialog::get_the_method_of_obtaining_layer_information()
{
	// 1——从*.SMS文件中获取图层信息；2——从实际odata数据目录中获取图层信息
	if (ui.radioButton_read_layer_info_from_sms_file->isChecked())
	{
		return 1;
	}
	else if (ui.radioButton_read_layer_info_from_odata_dir->isChecked())
	{
		return 2;
	}
	// 默认为1
	return 1;
}

//	spdlog 日志器等级设置（默认为trace级别：最为详细的级别；全部的级别：trace、debug、info、warn、err、critical、off）
spdlog::level::level_enum ToolDZB2ShapefileWithSpecificationDialog::get_log_level()
{
	//	设置输入的日志等级
	QString qstr_log_level = ui.comboBox_level->currentText();
	QByteArray byteArray = qstr_log_level.toLocal8Bit();
	std::string str_log_level = byteArray.data();
	if (str_log_level == "详细")
	{
		return spdlog::level::level_enum::trace;
	}
	else if (str_log_level == "调试")
	{
		return spdlog::level::level_enum::debug;
	}
	else if (str_log_level == "消息")
	{
		return spdlog::level::level_enum::info;
	}
	else if (str_log_level == "警告")
	{
		return spdlog::level::level_enum::warn;
	}
	else if (str_log_level == "一般错误")
	{
		return spdlog::level::level_enum::err;
	}
	else if (str_log_level == "致命错误")
	{
		return spdlog::level::level_enum::critical;
	}
	else if (str_log_level == "关闭")
	{
		return spdlog::level::level_enum::off;
	}
	else
	{
		//	默认是消息
		return spdlog::level::level_enum::err;
	}
}

//	获取汇总日志保存路径
std::string ToolDZB2ShapefileWithSpecificationDialog::get_summary_log_saving_path_str()
{
	QString qstr_summary_log_saving_path = ui.lineEdit_summary_log_saving_path->text();
	QByteArray qba_summary_log_saving_path = qstr_summary_log_saving_path.toLocal8Bit();
	return std::string(qba_summary_log_saving_path);
}

//	获取汇总日志保存路径
QString ToolDZB2ShapefileWithSpecificationDialog::get_summary_log_saving_path_qstr()
{
	m_summary_log_saving_path = ui.lineEdit_summary_log_saving_path->text();
	return QString(m_summary_log_saving_path);
}

//  获取指定目录的最后一级目录
void ToolDZB2ShapefileWithSpecificationDialog::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}

int ToolDZB2ShapefileWithSpecificationDialog::GetScale(string strCode)
{
	if (strCode.find("N") == string::npos &&
		strCode.find("S") == string::npos)
	{
		return 0;
	}

	if ((strCode.find("LN") != -1 || strCode.find("LS") != -1)
		&& strCode.length() == 10)
	{
		return 4000000;
	}

	// 查找图幅号中的N或S，如果是DN或DS开头的图幅号，则去掉D
	int iIndex = strCode.find('N');
  // 待计算图幅号的编码
	string strCalCode;
  // 如果找到N，则说明是北半球编码
	if (iIndex != -1)
	{
    // 如果首位不是0，则需要把N前面的D去掉
		if (iIndex != 0)
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}
  // 说明是南半球
	else
	{
		iIndex = strCode.find('S');
		if (iIndex != 0)
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}

	// 如果是100万比例尺（N1050）
	if (strCalCode.length() == 5)
	{
		return 1000000;
	}
	// 如果是50万比例尺
	else if (strCalCode.length() == 6)
	{
		return 500000;
	}
	// 如果是25万比例尺
	else if (strCalCode.length() == 7)
	{
		return 250000;
	}
	// 如果是10万比例尺
	else if (strCalCode.length() == 8)
	{
		return 100000;
	}
	// 如果是5万比例尺
	else if (strCalCode.length() == 9)
	{
		return 50000;
	}
	// 如果是2.5万比例尺
	else if (strCalCode.length() == 10)
	{
		return 25000;
	}
	// 如果是1万比例尺
	else if (strCalCode.length() == 11)
	{
		return 10000;
	}
	// 如果是5千比例尺
	else if (strCalCode.length() == 12)
	{
		return 5000;
	}
	else if (strCalCode.length() > 12)
	{
		return 0;
	}
	return 0;
}

void ToolDZB2ShapefileWithSpecificationDialog::InitLineEdit()
{
	// X偏移量非负浮点数
	ui.lineEdit_OffsetX->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

	// Y偏移量非负浮点数
	ui.lineEdit_OffsetY->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

}
#pragma endregion
