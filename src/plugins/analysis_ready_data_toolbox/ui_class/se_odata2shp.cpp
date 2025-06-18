#include "se_odata2shp.h"
/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"

/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>

/*-------------------------------*/

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



CSE_VectorFormatConversionDialog::CSE_VectorFormatConversionDialog(QgisInterface* qgisInterface, QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
	, m_qgisInterface(qgisInterface)
{

	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_Save, &QPushButton::clicked, this, &CSE_VectorFormatConversionDialog::Button_Save_clicked);
	connect(ui.Button_Open, &QPushButton::clicked, this, &CSE_VectorFormatConversionDialog::Button_Open_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_VectorFormatConversionDialog::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_VectorFormatConversionDialog::Button_Cancel_rejected);

	// 界面输入信息状态改变，需要重置进度条
	connect(ui.lineEdit_InputDataPath, &QLineEdit::textChanged,this, &CSE_VectorFormatConversionDialog::on_InputDataPath_TextChanged);
	connect(ui.lineEdit_OutputDataPath, &QLineEdit::textChanged, this, &CSE_VectorFormatConversionDialog::on_lineEdit_OutputDataPath_TextChanged);
	connect(ui.radioButton_OriginSRS, &QRadioButton::clicked, this, &CSE_VectorFormatConversionDialog::on_radioButton_OriginSRS_clicked);
	connect(ui.radioButton_GeoSRS, &QRadioButton::clicked, this, &CSE_VectorFormatConversionDialog::on_radioButton_GeoSRS_clicked);
	connect(ui.radioButton_ProjSRS, &QRadioButton::clicked, this, &CSE_VectorFormatConversionDialog::on_radioButton_ProjSRS_clicked);
	connect(ui.lineEdit_OffsetX, &QLineEdit::textChanged, this, &CSE_VectorFormatConversionDialog::on_lineEdit_OffsetX_TextChanged);
	connect(ui.lineEdit_OffsetY, &QLineEdit::textChanged, this, &CSE_VectorFormatConversionDialog::on_lineEdit_OffsetY_TextChanged);

	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);

	// 根据所选的odata目录，自动填充比例尺分母文本框 
		// 如果当前输入odata目录为单个图幅
	QStringList qstrSubDir = GetSubDirPathOfCurrentDir(m_qstrInputDataPath);
	if (qstrSubDir.size() == 0)
	{
		// 获取图幅名称
		// 获取最后一级文件夹目录
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
		// 获取图幅名称
		// 获取最后一级文件夹目录
		QString qstrFolderName;
		GetFolderNameFromPath(qstrSubDir[0], qstrFolderName);

		QByteArray qFolderName = qstrFolderName.toLocal8Bit();
		string strFolderName = string(qFolderName);

		int iScale = GetScale(strFolderName);

		QString qstrScale = tr("%1").arg(iScale);

		ui.lineEdit_Scale->setText(qstrScale);
	}


	ui.lineEdit_OutputDataPath->setText(m_qstrSaveDataPath);

	// 默认输出与原始数据空间参考系一致
	ui.radioButton_OriginSRS->setChecked(true);

	// 默认X偏移量1000，Y偏移量1000
	ui.lineEdit_OffsetX->setText(tr("1000"));

	ui.lineEdit_OffsetY->setText(tr("1000"));

	InitLineEdit();

	m_pThread = nullptr;

}

CSE_VectorFormatConversionDialog::~CSE_VectorFormatConversionDialog()
{
	if (m_pThread)
	{
		delete[]m_pThread;
		m_pThread = nullptr;
	}

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("Odata2Shp/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("Odata2Shp/SaveDataPath"), m_qstrSaveDataPath, QgsSettings::Section::Plugins);
}


int CSE_VectorFormatConversionDialog::GetOutputSRS()
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

void CSE_VectorFormatConversionDialog::Button_Save_clicked()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OutputDataPath->setText(selectedDir);
		m_qstrSaveDataPath = selectedDir;
	}
}

void CSE_VectorFormatConversionDialog::Button_Open_clicked()
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
		ui.lineEdit_InputDataPath->setText(selectedDir);
		m_qstrInputDataPath = selectedDir;

		// 根据所选的odata目录，自动填充比例尺分母文本框 
		// 如果当前输入odata目录为单个图幅
		QStringList qstrSubDir = GetSubDirPathOfCurrentDir(m_qstrInputDataPath);
		if (qstrSubDir.size() == 0)
		{
			// 获取图幅名称
			// 获取最后一级文件夹目录
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
			// 获取图幅名称
			// 获取最后一级文件夹目录
			QString qstrFolderName;
			GetFolderNameFromPath(qstrSubDir[0], qstrFolderName);

			QByteArray qFolderName = qstrFolderName.toLocal8Bit();
			string strFolderName = string(qFolderName);

			int iScale = GetScale(strFolderName);

			QString qstrScale = tr("%1").arg(iScale);

			ui.lineEdit_Scale->setText(qstrScale);
		}

	}
}

/*获取在指定目录下的目录的路径*/
QStringList CSE_VectorFormatConversionDialog::GetSubDirPathOfCurrentDir(QString dirPath)
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

void CSE_VectorFormatConversionDialog::Button_OK_accepted()
{
	int iOutputSRS = GetOutputSRS();
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

	m_pThread = new SE_Odata2shpThread[iNumberOfProcessors];
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
		return;
	}

	// 计算输入图幅所在目录下图幅个数，按照CPU核数平均分配图幅
	
	// 如果当前输入odata目录为单个图幅
	QStringList qstrSubDir = GetSubDirPathOfCurrentDir(ui.lineEdit_InputDataPath->text());
	
	int iSheetCount = qstrSubDir.size();

	m_SheetCount = iSheetCount;
	
	// 计算每个CPU平均分配的图幅个数 
	int iSheetCountPerCpuKernal = int(iSheetCount / (iNumberOfProcessors * 1.0));
	
	// 如果图幅数小于核数
	if (iSheetCountPerCpuKernal < 1)
	{
		for (int i = 0; i < iSheetCount; ++i)
		{
			QStringList qstrList;
			qstrList << qstrSubDir[i];
			
			connect(&m_pThread[i], &SE_Odata2shpThread::resultProcess, this, &CSE_VectorFormatConversionDialog::handleResults);
			
			m_pThread[i].SetThreadParams(
				qstrList,
				ui.lineEdit_OutputDataPath->text(),
				iOutputSRS,
				ui.lineEdit_OffsetX->text().toDouble(),
				ui.lineEdit_OffsetY->text().toDouble());
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

		QStringList *qstrList = new QStringList[iNumberOfProcessors];
		for (int i = 0; i < iNumberOfProcessors; ++i)
		{		
			for (int j = pIndex[i].iFromIndex; j <= pIndex[i].iToIndex; ++j)
			{
				qstrList[i] << qstrSubDir[j];
			}
		}

		for (int i = 0; i < iNumberOfProcessors; ++i)
		{
			connect(&m_pThread[i], &SE_Odata2shpThread::resultProcess, this, &CSE_VectorFormatConversionDialog::handleResults);
			m_pThread[i].SetThreadParams(
				qstrList[i],
				ui.lineEdit_OutputDataPath->text(),
				iOutputSRS,
				ui.lineEdit_OffsetX->text().toDouble(),
				ui.lineEdit_OffsetY->text().toDouble());
		}


	}




	//---------------------end---------------------//

	ui.Button_OK->setEnabled(false);

}

void CSE_VectorFormatConversionDialog::Button_Cancel_rejected()
{
	reject();
}

void CSE_VectorFormatConversionDialog::on_InputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_VectorFormatConversionDialog::on_lineEdit_OutputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_VectorFormatConversionDialog::on_radioButton_OriginSRS_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_VectorFormatConversionDialog::on_radioButton_GeoSRS_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_VectorFormatConversionDialog::on_radioButton_ProjSRS_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_VectorFormatConversionDialog::on_lineEdit_OffsetX_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_VectorFormatConversionDialog::on_lineEdit_OffsetY_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_VectorFormatConversionDialog::handleResults(const int& iProcessed, const QString& s)
{
	static int iTotalCount = 0;
	iTotalCount += iProcessed;
	
	if (iTotalCount == m_SheetCount)
	{
		ui.progressBar->setValue(int(100));
		ui.Button_OK->setEnabled(true);
		QString qstrTitle = tr("线划图数据格式转换");
		QString qstrText = s;
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("Odata2Shp/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("Odata2Shp/SaveDataPath"), m_qstrSaveDataPath, QgsSettings::Section::Plugins);

	}
	else if (iTotalCount == 0)
	{
		QString qstrTitle = tr("线划图数据格式转换");
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

void CSE_VectorFormatConversionDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("Odata2Shp/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrSaveDataPath = settings.value(QStringLiteral("Odata2Shp/SaveDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}


QStringList CSE_VectorFormatConversionDialog::GetShpFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.shp" << "*.SHP";
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}

// 判断目录是否存在
bool CSE_VectorFormatConversionDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_VectorFormatConversionDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}

QString CSE_VectorFormatConversionDialog::GetInputDataPath()
{
	return ui.lineEdit_InputDataPath->text();
}

QString CSE_VectorFormatConversionDialog::GetOutputDataPath()
{
	return ui.lineEdit_OutputDataPath->text();
}


QString CSE_VectorFormatConversionDialog::GetScale()
{
	return ui.lineEdit_Scale->text();
}

double CSE_VectorFormatConversionDialog::GetOffsetX()
{
	return ui.lineEdit_OffsetX->text().toDouble();
}

double CSE_VectorFormatConversionDialog::GetOffsetY()
{
	return ui.lineEdit_OffsetY->text().toDouble();
}


// 获取指定目录的最后一级目录
void CSE_VectorFormatConversionDialog::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}


int CSE_VectorFormatConversionDialog::GetScale(string strCode)
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
	string strCalCode;		// 待计算图幅号的编码
	if (iIndex != -1)		// 如果找到N，则说明是北半球编码
	{
		if (iIndex != 0)	// 如果首位不是0，则需要把N前面的D去掉
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}
	else    // 说明是南半球
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


void CSE_VectorFormatConversionDialog::InitLineEdit()
{
	// X偏移量非负浮点数
	ui.lineEdit_OffsetX->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

	// Y偏移量非负浮点数
	ui.lineEdit_OffsetY->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

}