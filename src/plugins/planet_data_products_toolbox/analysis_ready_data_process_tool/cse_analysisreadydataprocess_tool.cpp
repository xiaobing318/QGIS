#pragma region "包含头文件（减少重复）"
/*-----------------------QT-----------------------*/
#include <QComboBox>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>

/*-----------------------QT-----------------------*/

#include "cse_analysisreadydataprocess_tool.h"
#include "cse_config_file_info_table.h"
#include "ui_cse_analysisreadydataprocess_tool.h"
#include "ui_cse_config_file_info_table.h"

#pragma endregion

#pragma region "1、分析就绪数据工具公开API接口"
CSE_AnalysisReadyDataProcess::CSE_AnalysisReadyDataProcess(QWidget* parent, Qt::WindowFlags fl)
  :QDialog(parent, fl), m_poCSE_AnalysisReadyDataProcess_UI(new Ui::CSE_AnalysisReadyDataProcess)
{
    this->m_poCSE_AnalysisReadyDataProcess_UI->setupUi(this);
    connect(this->m_poCSE_AnalysisReadyDataProcess_UI->pushButton_vector_data_input_path_browse, &QPushButton::clicked,
      this, &CSE_AnalysisReadyDataProcess::pushButton_vector_data_input_path_browse);

    connect(this->m_poCSE_AnalysisReadyDataProcess_UI->pushButton_DEM_data_input_path_browse, &QPushButton::clicked,
      this, &CSE_AnalysisReadyDataProcess::pushButton_DEM_data_input_path_browse);

    connect(this->m_poCSE_AnalysisReadyDataProcess_UI->pushButton_vector_config_input_path_browse, &QPushButton::clicked,
      this, &CSE_AnalysisReadyDataProcess::pushButton_vector_config_input_path_browse);

    connect(this->m_poCSE_AnalysisReadyDataProcess_UI->pushButton_vector_config_input_path_edit, &QPushButton::clicked,
      this, &CSE_AnalysisReadyDataProcess::pushButton_vector_config_input_path_edit);

    connect(this->m_poCSE_AnalysisReadyDataProcess_UI->pushButton_data_output_path_browse, &QPushButton::clicked,
      this, &CSE_AnalysisReadyDataProcess::pushButton_data_output_path_browse);

    connect(this->m_poCSE_AnalysisReadyDataProcess_UI->pushButton_run, &QPushButton::clicked,
      this, &CSE_AnalysisReadyDataProcess::pushButton_run);

    connect(this->m_poCSE_AnalysisReadyDataProcess_UI->pushButton_cancel, &QPushButton::clicked,
      this, &CSE_AnalysisReadyDataProcess::pushButton_cancel);
}

CSE_AnalysisReadyDataProcess::~CSE_AnalysisReadyDataProcess()
{
    delete this->m_poCSE_AnalysisReadyDataProcess_UI;
}
#pragma endregion

#pragma region "2、分析就绪数据工具私有API接口"
void CSE_AnalysisReadyDataProcess::pushButton_vector_data_input_path_browse()
{
  // 定位到上一次保存的矢量输入数据所在的文件
  QString curPath = this->m_lineEdit_vector_data_input_path;
  QString dlgTile = QObject::tr("选择矢量数据所在的文件夹");
  // 使用 QFileDialog 弹出窗口让用户选择目录
  QString selectedDir = QFileDialog::getExistingDirectory(this, dlgTile, curPath, QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    //  将选择输入数据所在的文件目录显示出来
    this->m_poCSE_AnalysisReadyDataProcess_UI->lineEdit_vector_data_input_path->setText(selectedDir);
    //  矢量输入数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_lineEdit_vector_data_input_path = QString::fromUtf8(utf8Path.data());
  }
}

void CSE_AnalysisReadyDataProcess::pushButton_DEM_data_input_path_browse()
{
  // 定位到上一次保存的DEM数据所在的文件
  QString curPath = this->m_lineEdit_DEM_data_input_path;
  // 设置对话框标题
  QString dlgTitle = QObject::tr("选择DEM数据");
  // 弹出选择文件的对话框
  QString filter = QObject::tr("单个TIF文件 (*.tif);;All files (*)");
  QString fileName = QFileDialog::getOpenFileName(this, dlgTitle, curPath, filter);

  // 检查用户是否选中了文件
  if (!fileName.isEmpty())
  {
    // 显示选中的DEM文件文件路径
    this->m_poCSE_AnalysisReadyDataProcess_UI->lineEdit_DEM_data_input_path->setText(fileName);
    // 更新内部存储的路径
    this->m_lineEdit_DEM_data_input_path = fileName;
  }
}


void CSE_AnalysisReadyDataProcess::pushButton_vector_config_input_path_browse()
{
  // 定位到上一次保存的配置文件的路径
  QString curPath = this->m_lineEdit_vector_config_input_path;
  // 设置对话框标题
  QString dlgTitle = QObject::tr("选择配置文件");
  // 弹出保存文件的对话框
  QString filter = QObject::tr("单个CSV配置文件 (*.csv);;All files (*)");
  QString fileName = QFileDialog::getOpenFileName(this, dlgTitle, curPath, filter);

  // 检查用户是否选中了文件
  if (!fileName.isEmpty())
  {
    // 显示选中的ttf文件文件路径
    this->m_poCSE_AnalysisReadyDataProcess_UI->lineEdit_vector_config_input_path->setText(fileName);
    // 处理路径，替换路径分隔符，转换编码
    QString processedPath = fileName.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    this->m_lineEdit_vector_config_input_path = QString::fromUtf8(utf8Path.data());
  }
}

void CSE_AnalysisReadyDataProcess::pushButton_vector_config_input_path_edit()
{
  cse_config_file_info_table* pDlg = new cse_config_file_info_table(this, Qt::Dialog, this->m_lineEdit_vector_config_input_path);

  // 用户点击了确认：显示对话框并等待用户响应（目前得到的UI交互界面中没有确认按钮）
  if (pDlg->exec() == QDialog::Accepted)
  {
    // 如果用户接受对话框中的操作，可以在这里处理用户接受的相关逻辑，目前的工具的功能在另外的地方实现
  }
  // 用户点击了取消：删除对话框对象，释放内存
  delete pDlg;
}

void CSE_AnalysisReadyDataProcess::pushButton_data_output_path_browse()
{
  // 定位到上一次保存的本底数据所在的文件
  QString curPath = this->m_lineEdit_data_output_path;
  QString dlgTile = QObject::tr("选择数据输出所在的文件夹");
  // 使用 QFileDialog 弹出窗口让用户选择目录
  QString selectedDir = QFileDialog::getExistingDirectory(this, dlgTile, curPath, QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    //  将选择输出数据所在的文件目录显示出来
    this->m_poCSE_AnalysisReadyDataProcess_UI->lineEdit_data_output_path->setText(selectedDir);
    //  输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_lineEdit_data_output_path = QString::fromUtf8(utf8Path.data());
  }
}

void CSE_AnalysisReadyDataProcess::pushButton_run()
{
  reject();
}

void CSE_AnalysisReadyDataProcess::pushButton_cancel()
{
  reject();
}
#pragma endregion
