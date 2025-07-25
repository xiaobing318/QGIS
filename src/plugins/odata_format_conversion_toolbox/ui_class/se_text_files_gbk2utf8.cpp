
/*--------------QGIS---------------*/
#include "qgisinterface.h"
#include "qgsgui.h"
#include "qgssettings.h"
/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
/*-------------------------------*/
#include "se_text_files_gbk2utf8.h"

#pragma region "构造函数和析构函数"
TextFilesGBK2UTF8::TextFilesGBK2UTF8(
	QgisInterface* qgisInterface, 
	QWidget* parent, 
	Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
  //  给UI界面分配内存空间
  this->m_ui = new Ui_SeTextFilesGbk2Utf8();
  //  用于初始化和布置由Qt Designer创建的用户界面组件
	this->m_ui->setupUi(this);
  //  用于自动保存和恢复窗口的几何布局
  QgsGui::enableAutoGeometryRestore(this);
  //  用于恢复上一次使用保存的文件路径等状态
  restoreState();
  //  将恢复的UI设置显示出来
  m_ui->lineEdit_InputDataDir->setText(m_qstrInputDataDir);
  m_ui->lineEdit_OutputDataDir->setText(m_qstrOutputDataDir);

  //  将UI界面元素同响应函数连接(打开输入数据浏览)
  connect(
    m_ui->Button_OpenInputDataDir, &QPushButton::clicked,
    this, &TextFilesGBK2UTF8::Button_OpenInputDataDir);
  //  将UI界面元素同响应函数连接(打开输出数据浏览)
  connect(
    m_ui->Button_OpenOutputDataDir, &QPushButton::clicked,
    this, &TextFilesGBK2UTF8::Button_OpenOutputDataDir);

  //  将UI界面元素同响应函数连接(开始运行工具)
  connect(
    m_ui->Button_OK, &QPushButton::clicked,
    this, &TextFilesGBK2UTF8::Button_OK);
  //  将UI界面元素同响应函数连接(关闭工具)
  connect(
    m_ui->Button_Cancel, &QPushButton::clicked,
    this, &TextFilesGBK2UTF8::Button_Cancel); 
}

TextFilesGBK2UTF8::~TextFilesGBK2UTF8()
{
  //  释放界面所占有的内存
  if (this->m_ui)
  {
    delete this->m_ui;
  }
  saveState();
}
#pragma endregion

#pragma region "私有槽函数"
//  打开输入数据目录
void TextFilesGBK2UTF8::Button_OpenInputDataDir()
{
  // 定位到上一次保存的输出数据所在的文件
  QString curPath = this->m_qstrInputDataDir;
  QString dlgTile = QObject::tr("选择将要进行字符集编码转化的输入目录");
  // 使用 QFileDialog 弹出窗口让用户选择目录
  QString selectedDir = QFileDialog::getExistingDirectory(this, dlgTile, curPath, QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    //  将选择的输出数据所在的文件目录显示出来
    this->m_ui->lineEdit_InputDataDir->setText(selectedDir);
    //  应该将输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_qstrInputDataDir = QString::fromUtf8(utf8Path.data());
  }
}
//  打开输出数据目录
void TextFilesGBK2UTF8::Button_OpenOutputDataDir()
{
  // 定位到上一次保存的输出数据所在的文件
  QString curPath = this->m_qstrOutputDataDir;
  QString dlgTile = QObject::tr("选择将要进行字符集编码转化的输出目录");
  // 使用 QFileDialog 弹出窗口让用户选择目录
  QString selectedDir = QFileDialog::getExistingDirectory(this, dlgTile, curPath, QFileDialog::ShowDirsOnly);

  if (!selectedDir.isEmpty())
  {
    //  将选择的输出数据所在的文件目录显示出来
    this->m_ui->lineEdit_OutputDataDir->setText(selectedDir);
    //  应该将输出数据所在的文件夹路径进行处理（windows上使用的为\转化为/，然后将路径字符编码转化为UTF-8之后再保存起来）
    QString processedPath = selectedDir.replace("\\", "/");
    QByteArray utf8Path = processedPath.toUtf8();
    // 将处理后的路径保存到内部数据结构中
    this->m_qstrOutputDataDir = QString::fromUtf8(utf8Path.data());
  }
}
//	进行字符集编码转化处理
void TextFilesGBK2UTF8::Button_OK()
{
  // 创建 QProcess 对象
  QProcess* pProcess = new QProcess(this);
  // 构建python程序的完整路径和命令行参数
  QString appDir = QCoreApplication::applicationDirPath();
  QDir dir(appDir); // 将当前应用程序目录封装成QDir对象
  QString parentDir = dir.absolutePath(); // 先获取绝对路径
  dir.cdUp(); // 移动到上级目录
  parentDir = dir.absolutePath(); // 获取上级目录的路径
  QString pythonDir = parentDir + "/apps/Python39"; // Python安装目录
  QString program = pythonDir + "/python.exe"; // 使用上级目录的路径

  // 拼接参数
  QStringList arguments;
  arguments << appDir + "/text_files_gbk2utf8_scripts/GBK2UTF8.py"
    << m_qstrInputDataDir
    << m_qstrOutputDataDir;

  // 设置当前进程环境变量
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  // 构建新的PATH，包含Python目录
  QString newPath = "C:/Windows/system32;C:/Windows;C:/Windows/system32/WBem;" + pythonDir + ";" + env.value("PATH");
  env.insert("PATH", newPath);
  pProcess->setProcessEnvironment(env);

  // 启动进程
  pProcess->start(program, arguments);




  // 启动python程序进行文本文件字符集编码转化
  pProcess->start(program, arguments);
  // 等待程序启动，如果有需要也可以等待程序执行结束
  if (!pProcess->waitForStarted())
  {
      //qDebug() << "文本文件字符集编码转化失败！";
  }
  bool bResult = false;
  bResult = pProcess->waitForFinished(-1);
  if (!bResult)
  {
      //  获取标准错误输出
      QString errorOutput = pProcess->readAllStandardError();
      //  释放资源
      delete pProcess;
      //  文本文件字符集编码转化失败！
      QMessageBox msgBox;
      msgBox.setWindowTitle(tr("文本文件字符集编码转化工具"));
      msgBox.setText(tr("文本文件字符集编码转化失败！"));
      msgBox.setStandardButtons(QMessageBox::Yes);
      msgBox.setDefaultButton(QMessageBox::Yes);
      msgBox.exec();
      return;
  }
  //	释放资源
  delete pProcess;

  //  文本文件字符集编码转化成功！
  QMessageBox msgBox;
  msgBox.setWindowTitle(tr("文本文件字符集编码转化工具"));
  msgBox.setText(tr("文本文件字符集编码转化成功！"));
  msgBox.setStandardButtons(QMessageBox::Yes);
  msgBox.setDefaultButton(QMessageBox::Yes);
  msgBox.exec();


	//	将这次设置的“参数保存下来”
	saveState();
  return;
}
//  不进行字符集编码转化处理
void TextFilesGBK2UTF8::Button_Cancel()
{
  //  有可能在界面上设置了路径，但是不小心关掉了，这里所做的处理就是将设置了的内容保存起来以便下一次使用的时候恢复
  saveState();
  reject();
}

#pragma endregion

#pragma region "私有成员函数"
//	恢复保存参数
void TextFilesGBK2UTF8::restoreState()
{
  QgsSettings settings;
  // 指定从Plugins节读取设置
  settings.beginGroup("TextFilesGBK2UTF8_tool");
  this->m_qstrInputDataDir = settings.value(QStringLiteral("TextFilesGBK2UTF8_tool/m_qstrInputDataDir"), QDir::homePath()).toString();
  this->m_qstrOutputDataDir = settings.value(QStringLiteral("TextFilesGBK2UTF8_tool/m_qstrOutputDataDir"), QDir::homePath()).toString();
  // 结束分组
  settings.endGroup();

}
//	保存参数
void TextFilesGBK2UTF8::saveState()
{
  QgsSettings settings;
  // 指定设置保存到Plugins节
  settings.beginGroup("TextFilesGBK2UTF8_tool");
  settings.setValue(QStringLiteral("TextFilesGBK2UTF8_tool/m_qstrInputDataDir"), m_qstrInputDataDir);
  settings.setValue(QStringLiteral("TextFilesGBK2UTF8_tool/m_qstrOutputDataDir"), m_qstrOutputDataDir);
  // 结束分组，非常重要，确保设置正确保存在指定的节中
  settings.endGroup();
}
#pragma endregion
