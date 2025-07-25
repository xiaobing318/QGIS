
/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"
/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>
#include "se_annotation_pointer_reverse_extraction.h"
/*-------------------------------*/
/************** - 2024 - 01 - 08：添加多线程并行，增加速度；代码由杨志龙杨老师提供，进行集成）**************/
#ifdef OS_FAMILY_WINDOWS
#include "windows.h"
#else
#include "unistd.h"
#endif

struct FromToIndex
{
	int iFromIndex;		// 起始图幅索引
	int iToIndex;		// 终止图幅索引

	FromToIndex()
	{
		iFromIndex = 0;
		iToIndex = 0;
	}
};
/************** - 2024 - 01 - 08：添加多线程并行，增加速度；代码由杨志龙杨老师提供，进行集成）**************/

CSE_AnnoPointerReverseExtractDialog::CSE_AnnoPointerReverseExtractDialog(
	QgisInterface* qgisInterface, 
	QWidget* parent, 
	Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_OpenInputShpDataPath, &QPushButton::clicked, this, &CSE_AnnoPointerReverseExtractDialog::Button_OpenInputShpDataPath_clicked);
	connect(ui.Button_OpenOutputShpDataPath, &QPushButton::clicked, this, &CSE_AnnoPointerReverseExtractDialog::Button_OpenOutputShpDataPath_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_AnnoPointerReverseExtractDialog::Button_OK_accepted_single_thread);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_AnnoPointerReverseExtractDialog::Button_Cancel_rejected);
	
	connect(ui.lineEdit_InputShpDataPath, &QLineEdit::textChanged, this, &CSE_AnnoPointerReverseExtractDialog::on_lineEdit_InputShpDataPath_TextChanged);
	connect(ui.lineEdit_OutputShpDataPath, &QLineEdit::textChanged, this, &CSE_AnnoPointerReverseExtractDialog::on_lineEdit_OutputShpDataPath_TextChanged);

	restoreState();
	// 初始化输入路径和输出路径
	ui.lineEdit_InputShpDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputShpDataPath->setText(m_qstrOutputDataPath);

	//connect(&m_Thread, &SE_AnnoPointerReverseExtractThread::resultProcess, this, &CSE_AnnoPointerReverseExtractDialog::handleResults);
	
	/************** - 2024 - 01 - 08：添加多线程并行，增加速度；代码由杨志龙杨老师提供，进行集成）**************/
	m_pThread = nullptr;
	/************** - 2024 - 01 - 08：添加多线程并行，增加速度；代码由杨志龙杨老师提供，进行集成）**************/
}

CSE_AnnoPointerReverseExtractDialog::~CSE_AnnoPointerReverseExtractDialog()
{
	QgsSettings settings;
	settings.setValue(QStringLiteral("AnnoPointerReverseExtract/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("AnnoPointerReverseExtract/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);

}

void CSE_AnnoPointerReverseExtractDialog::Button_OpenInputShpDataPath_clicked()
{
	// 选择文件夹（使用上次使用保存下来的路径）
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = tr("请选择shp数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputShpDataPath->setText(selectedDir);

    //  应该将本底数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_qstrInputDataPath = QString::fromUtf8(utf8Path.data());
	}
}

void CSE_AnnoPointerReverseExtractDialog::Button_OpenOutputShpDataPath_clicked()
{
	// 选择文件夹（使用上次使用保存下来的路径）
	QString curPath = m_qstrOutputDataPath;
	QString dlgTile = tr("请选择处理后的数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OutputShpDataPath->setText(selectedDir);

    //  应该将本底数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_qstrOutputDataPath = QString::fromUtf8(utf8Path.data());
	}
}


/*获取在指定目录下的目录的路径*/
QStringList CSE_AnnoPointerReverseExtractDialog::GetSubDirPathOfCurrentDir(QString dirPath)
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

void CSE_AnnoPointerReverseExtractDialog::Button_OK_accepted_multi_thread()
{
	ui.progressBar->reset();
/************** - 2024 - 01 - 08：添加多线程并行，增加速度；代码由杨志龙杨老师提供，进行集成）**************/
/*
问题：
1、可能存在一些内存泄露问题
2、逻辑问题
*/
	ui.progressBar->reset();

	//---------------------begin---------------------//
	// 获取计算机CPU核数
	int iNumberOfProcessors = 0;
#ifdef OS_FAMILY_WINDOWS

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

#else // OS_FAMILY_UNIX

	// 获取当前系统的可用CPU核数
	iNumberOfProcessors = sysconf(_SC_NPROCESSORS_ONLN);

#endif
	m_pThread = new SE_AnnoPointerReverseExtractThread[iNumberOfProcessors];
	if (m_pThread == nullptr)
	{
		QString qstrTitle = tr("注记去重");
		QString qstrText = tr("创建注记指针去重线程失败！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		ui.Button_OK->setEnabled(true);
		return;
	}

/***************计算输入图幅所在目录下图幅个数，按照CPU核数平均分配图幅***************/
	QStringList qstrSubDir = GetSubDirPathOfCurrentDir(ui.lineEdit_InputShpDataPath->text());
	int iSheetCount = qstrSubDir.size();
	m_SheetCount = iSheetCount;
	// 计算每个CPU平均分配的图幅个数 
	int iSheetCountPerCpuKernal = int(iSheetCount / (iNumberOfProcessors * 1.0));
/***************计算输入图幅所在目录下图幅个数，按照CPU核数平均分配图幅***************/

	// 如果图幅数小于核数
	if (iSheetCountPerCpuKernal < 1)
	{
		for (int i = 0; i < iSheetCount; ++i)
		{
			QStringList qstrList;
			qstrList << qstrSubDir[i];

			m_pThread[i].SetMultiThreadParams(
				qstrList,
				ui.lineEdit_OutputShpDataPath->text());
		}
	}
	else
	{
		FromToIndex* pIndex = new FromToIndex[iNumberOfProcessors];

		for (int i = 0; i < iNumberOfProcessors; ++i)
		{
			// 如果不是最后一个
			if (i != iNumberOfProcessors - 1)
			{
				pIndex[i].iFromIndex = iSheetCountPerCpuKernal * i;
				pIndex[i].iToIndex = iSheetCountPerCpuKernal * (i + 1) - 1;
			}
			// 如果是最后一个
			else
			{
				pIndex[i].iFromIndex = iSheetCountPerCpuKernal * i;
				pIndex[i].iToIndex = iSheetCount - 1;
			}
		}

		QStringList* qstrList = new QStringList[iNumberOfProcessors];
		for (int i = 0; i < iNumberOfProcessors; ++i)
		{
			for (int j = pIndex[i].iFromIndex; j <= pIndex[i].iToIndex; ++j)
			{
				qstrList[i] << qstrSubDir[j];
			}
		}

		for (int i = 0; i < iNumberOfProcessors; ++i)
		{
			QString qstrOutputDataPath = ui.lineEdit_OutputShpDataPath->text();
			m_pThread[i].SetMultiThreadParams(
				qstrList[i],
				qstrOutputDataPath);
		}
	}
/************** - 2024 - 01 - 08：添加多线程并行，增加速度；代码由杨志龙杨老师提供，进行集成）**************/

	ui.Button_OK->setEnabled(true);
	//	将这次设置的“参数保存下来”
	setState();

}

void CSE_AnnoPointerReverseExtractDialog::Button_OK_accepted_single_thread()
{
	ui.progressBar->reset();

	m_single_Thread.SetSingleThreadParams(
		ui.lineEdit_InputShpDataPath->text(),
		ui.lineEdit_OutputShpDataPath->text());

	ui.Button_OK->setEnabled(false);
	//	将这次设置的“参数保存下来”
	setState();

}

void CSE_AnnoPointerReverseExtractDialog::Button_Cancel_rejected()
{
	reject();
}

void CSE_AnnoPointerReverseExtractDialog::on_lineEdit_InputShpDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_AnnoPointerReverseExtractDialog::on_lineEdit_OutputShpDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_AnnoPointerReverseExtractDialog::handleResults(const int& iProcessed, const QString& s)
{
	static int iTotalCount = 0;
	iTotalCount += iProcessed;
	if (iTotalCount == m_SheetCount)
	{
		ui.progressBar->setValue(int(100));
		ui.Button_OK->setEnabled(true);
		QString qstrTitle = tr("注记指针");
		QString qstrText = s;
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		// 设置当前保存路径
		setState();

	}
	else if (iTotalCount == 0)
	{
		QString qstrTitle = tr("注记指针");
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
		ui.progressBar->setValue(int(100.0 * iTotalCount / m_SheetCount));
	}
}

void CSE_AnnoPointerReverseExtractDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("AnnoPointerReverseExtract/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("AnnoPointerReverseExtract/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}

void CSE_AnnoPointerReverseExtractDialog::setState()
{
	QgsSettings settings;
	settings.setValue(QStringLiteral("AnnoPointerReverseExtract/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("AnnoPointerReverseExtract/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
}

// 判断目录是否存在
bool CSE_AnnoPointerReverseExtractDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_AnnoPointerReverseExtractDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}



QString CSE_AnnoPointerReverseExtractDialog::GetInputShpDataPath()
{
	return ui.lineEdit_InputShpDataPath->text();
}

QString CSE_AnnoPointerReverseExtractDialog::GetOutputLogPath()
{
	return ui.lineEdit_OutputShpDataPath->text();
}

// 获取指定目录的最后一级目录
void CSE_AnnoPointerReverseExtractDialog::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}

/*获取指定目录的最后一级目录*/
void CSE_AnnoPointerReverseExtractDialog::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


void CSE_AnnoPointerReverseExtractDialog::handleResults(const double& dPercent, const QString& s)
{
	if (dPercent == 1)
	{
		ui.progressBar->setValue(int(100 * dPercent));
		ui.Button_OK->setEnabled(true);
		QString qstrTitle = tr("注记指针反向挂接");
		QString qstrText = s;
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("AnnoPointerReverseExtract/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("AnnoPointerReverseExtract/SaveLogPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);

	}
	else if (dPercent == 0)
	{
		QString qstrTitle = tr("注记指针反向挂接");
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
