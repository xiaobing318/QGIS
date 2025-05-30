#include "se_geocoordsys_trans.h"
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

CSE_GeoCoordSysTransDialog::CSE_GeoCoordSysTransDialog(QgisInterface* qgisInterface, QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
	, m_qgisInterface(qgisInterface)
{

	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_Save, &QPushButton::clicked, this, &CSE_GeoCoordSysTransDialog::Button_Save_clicked);
	connect(ui.Button_Open, &QPushButton::clicked, this, &CSE_GeoCoordSysTransDialog::Button_Open_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &CSE_GeoCoordSysTransDialog::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &CSE_GeoCoordSysTransDialog::Button_Cancel_rejected);

	// 界面输入信息状态改变，需要重置进度条
	connect(ui.lineEdit_InputDataPath, &QLineEdit::textChanged,this, &CSE_GeoCoordSysTransDialog::on_lineEdit_InputDataPath_TextChanged);
	connect(ui.lineEdit_OutputDataPath, &QLineEdit::textChanged, this, &CSE_GeoCoordSysTransDialog::on_lineEdit_OutputDataPath_TextChanged);
	
	connect(ui.radioButton_Foreign_From, &QRadioButton::clicked, this, &CSE_GeoCoordSysTransDialog::on_radioButton_From_Foreign_clicked);
	connect(ui.radioButton_CGCS2000_From, &QRadioButton::clicked, this, &CSE_GeoCoordSysTransDialog::on_radioButton_From_CGCS2000_clicked);
	connect(ui.radioButton_BJS54_From, &QRadioButton::clicked, this, &CSE_GeoCoordSysTransDialog::on_radioButton_From_BJS54_clicked);
	connect(ui.radioButton_XAS80_From, &QRadioButton::clicked, this, &CSE_GeoCoordSysTransDialog::on_radioButton_From_XAS80_clicked);
	connect(ui.radioButton_WGS84_From, &QRadioButton::clicked, this, &CSE_GeoCoordSysTransDialog::on_radioButton_From_WGS84_clicked);
	
	connect(ui.radioButton_CGCS2000_To, &QRadioButton::clicked, this, &CSE_GeoCoordSysTransDialog::on_radioButton_To_CGCS2000_clicked);
	connect(ui.radioButton_BJS54_To, &QRadioButton::clicked, this, &CSE_GeoCoordSysTransDialog::on_radioButton_To_BJS54_clicked);
	connect(ui.radioButton_XAS80_To, &QRadioButton::clicked, this, &CSE_GeoCoordSysTransDialog::on_radioButton_To_XAS80_clicked);
	connect(ui.radioButton_WGS84_To, &QRadioButton::clicked, this, &CSE_GeoCoordSysTransDialog::on_radioButton_To_WGS84_clicked);

	connect(ui.lineEdit_SemiMajorAxis, &QLineEdit::textChanged, this, &CSE_GeoCoordSysTransDialog::on_lineEdit_SemiMajorAxis_TextChanged);
	connect(ui.lineEdit_SemiMinorAxis, &QLineEdit::textChanged, this, &CSE_GeoCoordSysTransDialog::on_lineEdit_SemiMinorAxis_TextChanged);

	connect(ui.lineEdit_dX, &QLineEdit::textChanged, this, &CSE_GeoCoordSysTransDialog::on_lineEdit_dX_TextChanged);
	connect(ui.lineEdit_dY, &QLineEdit::textChanged, this, &CSE_GeoCoordSysTransDialog::on_lineEdit_dY_TextChanged);
	connect(ui.lineEdit_dZ, &QLineEdit::textChanged, this, &CSE_GeoCoordSysTransDialog::on_lineEdit_dZ_TextChanged);
	connect(ui.lineEdit_rX, &QLineEdit::textChanged, this, &CSE_GeoCoordSysTransDialog::on_lineEdit_rX_TextChanged);
	connect(ui.lineEdit_rY, &QLineEdit::textChanged, this, &CSE_GeoCoordSysTransDialog::on_lineEdit_rX_TextChanged);
	connect(ui.lineEdit_rZ, &QLineEdit::textChanged, this, &CSE_GeoCoordSysTransDialog::on_lineEdit_rZ_TextChanged);
	connect(ui.lineEdit_K, &QLineEdit::textChanged, this, &CSE_GeoCoordSysTransDialog::on_lineEdit_K_TextChanged);

	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);

	ui.lineEdit_OutputDataPath->setText(m_qstrSaveDataPath);

	// 默认CGCS2000坐标系
	ui.radioButton_CGCS2000_From->setChecked(true);

	// 默认CGCS2000参考椭球参数
	ui.lineEdit_SemiMajorAxis->setText(QString::number(CGCS2000_SEMIMAJORAXIS, 'f', 15));

	ui.lineEdit_SemiMinorAxis->setText(QString::number(CGCS2000_SEMIMINORAXIS, 'f', 15));

	// 默认输出CGCS2000坐标系
	ui.radioButton_CGCS2000_To->setChecked(true);

	// 默认转换参数
	ui.lineEdit_dX->setText(QString::number(0));
	ui.lineEdit_dY->setText(QString::number(0));
	ui.lineEdit_dZ->setText(QString::number(0));
	ui.lineEdit_rX->setText(QString::number(0));
	ui.lineEdit_rY->setText(QString::number(0));
	ui.lineEdit_rZ->setText(QString::number(0));
	ui.lineEdit_K->setText(QString::number(0));

	InitLineEdit();

	m_pThread = nullptr;
}

CSE_GeoCoordSysTransDialog::~CSE_GeoCoordSysTransDialog()
{
	if (m_pThread)
	{
		delete[]m_pThread;
		m_pThread = nullptr;
	}

	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("GeoCoordSysTrans/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("GeoCoordSysTrans/SaveDataPath"), m_qstrSaveDataPath, QgsSettings::Section::Plugins);
}



void CSE_GeoCoordSysTransDialog::Button_Save_clicked()
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
    //  应该将本底数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
		m_qstrSaveDataPath = QString::fromUtf8(utf8Path.data());
	}
}

void CSE_GeoCoordSysTransDialog::Button_Open_clicked()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTile = tr("请选择输入数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputDataPath->setText(selectedDir);
    //  应该将本底数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
		m_qstrInputDataPath = QString::fromUtf8(utf8Path.data());
	}
}

/*获取在指定目录下的目录的路径*/
QStringList CSE_GeoCoordSysTransDialog::GetSubDirPathOfCurrentDir(QString dirPath)
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

void CSE_GeoCoordSysTransDialog::Button_OK_accepted()
{
	// 获取输入坐标系
	int iInputGeoSys = GetFromGeo();

	// 获取输出坐标系
	int iOutputGeoSys = GetToGeo();

	// 获取长短半轴
	double dSemiMajorAxis = 0;
	double dSemiMinorAxis = 0;

	GetEllipseParams(dSemiMajorAxis, dSemiMinorAxis);

	// 获取转换参数
	CoordTransParams transParams;
	GetTransParams(transParams);

  //	杨小兵-2024-04-02：spdlog日志器等级设置（默认为trace级别：最为详细的级别；全部的级别：trace、debug、info、warn、err、critical、off）
  spdlog::level::level_enum log_level = get_log_level();

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

	m_pThread = new CSE_GeoCoordSysTransThread[iNumberOfProcessors];
	if (!m_pThread)
	{
		QString qstrTitle = tr("地理坐标系转换");
		QString qstrText = tr("创建地理坐标系转换线程失败！");
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
			
			// 关联线程进度信号槽
			connect(&m_pThread[i], &CSE_GeoCoordSysTransThread::resultProcess, this, &CSE_GeoCoordSysTransDialog::handleResults);
			
			m_pThread[i].SetThreadParams(
				qstrList,
        m_qstrSaveDataPath,
				iInputGeoSys,
				dSemiMajorAxis,
				dSemiMinorAxis,
				iOutputGeoSys,
        transParams);
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
			// 关联线程进度信号槽
			connect(&m_pThread[i], &CSE_GeoCoordSysTransThread::resultProcess, this, &CSE_GeoCoordSysTransDialog::handleResults);

			m_pThread[i].SetThreadParams(
				qstrList[i],
        m_qstrSaveDataPath,
				iInputGeoSys,
				dSemiMajorAxis,
				dSemiMinorAxis,
				iOutputGeoSys,
        transParams);
		}
	}

	ui.Button_OK->setEnabled(false);

}

void CSE_GeoCoordSysTransDialog::Button_Cancel_rejected()
{
	reject();
}

void CSE_GeoCoordSysTransDialog::on_summary_log_saving_path_TextChanged(const QString& qstr)
{
  ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::Button_summary_log_saving_path_clicked()
{
  // 定位到上一次保存的匹配数据所在的文件
  QString curPath = this->m_summary_log_saving_path;
  // 设置对话框标题
  QString dlgTitle = QObject::tr("保存日志文件");
  // 弹出保存文件的对话框
  QString filter = QObject::tr("Log files (*.log);;All files (*)");
  QString fileName = QFileDialog::getSaveFileName(this, dlgTitle, curPath, filter);

  // 检查用户是否选中了文件
  if (!fileName.isEmpty())
  {
    QFile file(fileName);

    // 打开文件进行写操作（此操作会清空已有内容）
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      file.close();
    }

    // 显示选中的日志文件路径
    ui.lineEdit_summary_log_saving_path->setText(fileName);
    // 处理路径，替换路径分隔符，转换编码
    QString processedPath = fileName.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    m_summary_log_saving_path = QString::fromUtf8(utf8Path.data());
  }

}

//	（杨小兵-2024-01-24）spdlog日志器等级设置（默认为trace级别：最为详细的级别；全部的级别：trace、debug、info、warn、err、critical、off）
spdlog::level::level_enum CSE_GeoCoordSysTransDialog::get_log_level()
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


void CSE_GeoCoordSysTransDialog::on_lineEdit_InputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_lineEdit_OutputDataPath_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_radioButton_From_CGCS2000_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_radioButton_From_WGS84_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_radioButton_From_BJS54_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_radioButton_From_XAS80_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_radioButton_From_Foreign_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_radioButton_To_CGCS2000_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_radioButton_To_WGS84_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_radioButton_To_BJS54_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_radioButton_To_XAS80_clicked(bool checked)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_lineEdit_SemiMajorAxis_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_lineEdit_SemiMinorAxis_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_lineEdit_dX_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_lineEdit_dY_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_lineEdit_dZ_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_lineEdit_rX_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_lineEdit_rY_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_lineEdit_rZ_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}

void CSE_GeoCoordSysTransDialog::on_lineEdit_K_TextChanged(const QString& qstr)
{
	ui.progressBar->reset();
}


void CSE_GeoCoordSysTransDialog::handleResults(
	const int& processed_frame_data_flag, 
	const int& iSuccessProcessed, 
	const QString& s)
{
	// 总的已经被处理的分幅数据个数
	static int total_processed_frame_data_count = 0;

	//	总的已经被成功处理的分幅数据个数
	static int total_successfully_processed_frame_data_count = 0;

	total_processed_frame_data_count += processed_frame_data_flag;
	total_successfully_processed_frame_data_count += iSuccessProcessed;

	// 
	if (total_processed_frame_data_count == m_SheetCount)
	{
		ui.progressBar->setValue(int(100));
		ui.Button_OK->setEnabled(true);
		QString qstrTitle = tr("地理坐标系转换");

		QString qstrText = tr("共计") + QString::number(m_SheetCount)
			+ tr("幅分幅数据；其中成功处理") + QString::number(total_successfully_processed_frame_data_count)
			+ tr("幅分幅数据；其中") + QString::number(total_processed_frame_data_count - total_successfully_processed_frame_data_count)
			+ tr("幅分幅数据处理失败！");

		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();

		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("GeoCoordSysTrans/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("GeoCoordSysTrans/SaveDataPath"), m_qstrSaveDataPath, QgsSettings::Section::Plugins);

	}
	else if (total_processed_frame_data_count == 0)
	{
		QString qstrTitle = tr("地理坐标系转换");
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
		ui.progressBar->setValue(int(100.0 * total_processed_frame_data_count / m_SheetCount));
	}
}

void CSE_GeoCoordSysTransDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("GeoCoordSysTrans/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrSaveDataPath = settings.value(QStringLiteral("GeoCoordSysTrans/SaveDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}


// 判断目录是否存在
bool CSE_GeoCoordSysTransDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool CSE_GeoCoordSysTransDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}

QString CSE_GeoCoordSysTransDialog::GetInputDataPath()
{
	return ui.lineEdit_InputDataPath->text();
}

QString CSE_GeoCoordSysTransDialog::GetOutputDataPath()
{
	return ui.lineEdit_OutputDataPath->text();
}


int CSE_GeoCoordSysTransDialog::GetFromGeo()
{
	// 4—境外坐标系
	if (ui.radioButton_CGCS2000_From->isChecked())
	{
		return CGCS2000;
	}
	else if (ui.radioButton_BJS54_From->isChecked())
	{
		return BJS54;
	}
	else if (ui.radioButton_XAS80_From->isChecked())
	{
		return XAS80;
	}
	else if (ui.radioButton_WGS84_From->isChecked())
	{
		return WGS84;
	}
	else if (ui.radioButton_Foreign_From->isChecked())
	{
		return 4;
	}
	// 默认为CGCS2000
	return CGCS2000;
}

int CSE_GeoCoordSysTransDialog::GetToGeo()
{
	if (ui.radioButton_CGCS2000_To->isChecked())
	{
		return CGCS2000;
	}
	else if (ui.radioButton_BJS54_To->isChecked())
	{
		return BJS54;
	}
	else if (ui.radioButton_XAS80_To->isChecked())
	{
		return XAS80;
	}
	else if (ui.radioButton_WGS84_To->isChecked())
	{
		return WGS84;
	}

	// 默认为CGCS2000
	return CGCS2000;
}

void CSE_GeoCoordSysTransDialog::GetTransParams(CoordTransParams& params)
{
	params.dX = ui.lineEdit_dX->text().toDouble();
	params.dY = ui.lineEdit_dY->text().toDouble();
	params.dZ = ui.lineEdit_dZ->text().toDouble();
	params.dEx = ui.lineEdit_rX->text().toDouble();
	params.dEy = ui.lineEdit_rY->text().toDouble();
	params.dEz = ui.lineEdit_rZ->text().toDouble();
	params.dM = ui.lineEdit_K->text().toDouble();
}

void CSE_GeoCoordSysTransDialog::GetEllipseParams(double& dSemiMajorAxis, double& dSemiMinorAxis)
{
	dSemiMajorAxis = ui.lineEdit_SemiMajorAxis->text().toDouble();
	dSemiMinorAxis = ui.lineEdit_SemiMinorAxis->text().toDouble();
}

void CSE_GeoCoordSysTransDialog::InitLineEdit()
{
	// 长半轴非负浮点数
	ui.lineEdit_SemiMajorAxis->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

	// 短半轴非负浮点数
	ui.lineEdit_SemiMinorAxis->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

}
