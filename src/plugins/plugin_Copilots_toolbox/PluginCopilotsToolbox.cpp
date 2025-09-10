/*-------------Qt--------------------*/
#include <QAction>
#include <QMessageBox>
#include <QtCore/QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

/*-------------Windows--------------------*/
#ifdef Q_OS_WIN
  #include <windows.h>
#endif

/*------------QGIS include-----------------*/
#include "qgisinterface.h"
#include "qgsguiutils.h"
#include "qgsproject.h"
#include "qgsmessagebar.h"
#include "qgsmapcanvas.h"
#include "qgsapplication.h"
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"

#include "PluginCopilotsToolbox.h"

//#include "Copilot_Chat.h"
//#include "Copilot_Coder.h"
//#include "Copilot_Agent.h"


static const QString sName = QObject::tr("Copilots Toolbox");
static const QString sDescription = QObject::tr("三个功能：Copilot_Chat 智能对话； Copilot_Coder 智能代码；Copilot_Agent 智能体");
static const QString sCategory = QObject::tr("Copilots");
static const QString sPluginVersion = QObject::tr("Version 1.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
// 插件图标
static const QString sPluginIcon = QStringLiteral(":/plugin_Copilots_toolbox/icons/PluginCopilot.svg");

PluginCopilotsToolbox::PluginCopilotsToolbox(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{

}

PluginCopilotsToolbox::~PluginCopilotsToolbox()
{
    // 确保在插件销毁时正确终止 qcopilot 进程及其子进程
    if (mServerProcess && mServerProcess->state() == QProcess::Running)
    {
        QgsMessageLog::logMessage("正在终止 qcopilot 进程及其子进程...", "Copilots", Qgis::Info);

#ifdef Q_OS_WIN
        // 在Windows上，使用taskkill命令强制终止进程树
        QProcess killProcess;
        QString processId = QString::number(mServerProcess->processId());
        killProcess.start("taskkill", QStringList() << "/F" << "/T" << "/PID" << processId);
        killProcess.waitForFinished(3000);

        if (killProcess.exitCode() == 0)
        {
            QgsMessageLog::logMessage("已通过 taskkill 终止 qcopilot 进程树", "Copilots", Qgis::Info);
        }
        else
        {
            QgsMessageLog::logMessage(" taskkill 命令执行失败，尝试标准终止方式", "Copilots", Qgis::Warning);
        }
#endif

        // 发送终止信号（相当于 Ctrl+C）
        mServerProcess->terminate();

        // 等待进程正常退出，超时后强制杀死
        if (!mServerProcess->waitForFinished(5000))
        {
            QgsMessageLog::logMessage(" qcopilot 进程未能正常退出，强制终止...", "Copilots", Qgis::Warning);
            mServerProcess->kill();
            mServerProcess->waitForFinished(2000);
        }
        else
        {
            QgsMessageLog::logMessage(" qcopilot 进程已正常退出", "Copilots", Qgis::Info);
        }
    }
}



void PluginCopilotsToolbox::initGui()
{
	//  确保之前的actions对象指针都被释放了
  if (mAction_Copilot_Chat)
  {
    delete mAction_Copilot_Chat;
  }
  if (mAction_Copilot_Coder)
  {
    delete mAction_Copilot_Coder;
  }
  if (mAction_Copilot_Agent)
  {
    delete mAction_Copilot_Agent;
  }


#pragma region "1、Copilot_Chat 	对话补全"
  // Create the action for tool
  mAction_Copilot_Chat = new QAction(QIcon(":/plugin_Copilots_toolbox/icons/Copilot_Chat.svg"), tr(" Copilot 智能对话"), this);
  mAction_Copilot_Chat->setObjectName(QStringLiteral("mAction_Copilot_Chat"));
  // Set the what's this text
  mAction_Copilot_Chat->setWhatsThis(tr(" Copilot 智能对话"));
  // Connect the action to the run
  connect(mAction_Copilot_Chat, &QAction::triggered, this, &PluginCopilotsToolbox::Copilot_Chat);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_Copilot_Chat);
  mQGisIface->addPluginToVectorMenu(tr("&Copilots"), mAction_Copilot_Chat);
  mAction_Copilot_Chat->setEnabled(true);

#pragma endregion

#pragma region "2、Copilot_Coder 代码补全"
	// Create the action for tool
	mAction_Copilot_Coder = new QAction(QIcon(":/plugin_Copilots_toolbox/icons/Copilot_Coder.svg"), tr(" Copilot 智能代码"), this);
	mAction_Copilot_Coder->setObjectName(QStringLiteral("mAction_Copilot_Coder"));
	// Set the what's this text
	mAction_Copilot_Coder->setWhatsThis(tr(" Copilot 智能代码"));
	// Connect the action to the run
	connect(mAction_Copilot_Coder, &QAction::triggered, this, &PluginCopilotsToolbox::Copilot_Coder);
	// Add the icon to the toolbar
	mQGisIface->addVectorToolBarIcon(mAction_Copilot_Coder);
	mQGisIface->addPluginToVectorMenu(tr("&Copilots"), mAction_Copilot_Coder);
	mAction_Copilot_Coder->setEnabled(true);

#pragma endregion

#pragma region "3、Copilot_Agent 代理"
	// Create the action for tool
	mAction_Copilot_Agent = new QAction(QIcon(":/plugin_Copilots_toolbox/icons/Copilot_Agent.svg"), tr(" Copilot 智能代理"), this);
	mAction_Copilot_Agent->setObjectName(QStringLiteral("mAction_Copilot_Agent"));
	// Set the what's this text
	mAction_Copilot_Agent->setWhatsThis(tr(" Copilot 智能代理"));
	// Connect the action to the run
	connect(mAction_Copilot_Agent, &QAction::triggered, this, &PluginCopilotsToolbox::Copilot_Agent);
	// Add the icon to the toolbar
	mQGisIface->addVectorToolBarIcon(mAction_Copilot_Agent);
	mQGisIface->addPluginToVectorMenu(tr("&Copilots"), mAction_Copilot_Agent);
	mAction_Copilot_Agent->setEnabled(true);

#pragma endregion


	//	更新菜单状态
  updateActions();
}

//  按需启动server进程
void PluginCopilotsToolbox::startServerIfNeeded()
{
    // 如果进程已经运行，显示提示信息并返回
    if(mServerProcess && mServerProcess->state() == QProcess::Running)
    {
        mQGisIface->messageBar()->pushMessage("Copilots", " qcopilot 服务器已在运行中", Qgis::Info, 3);
        return;
    }

    // 获取当前应用程序的目录路径
    QString appDir = QCoreApplication::applicationDirPath();

    // 构建 qcopilot.exe 的路径
    QString program = QDir(appDir).filePath("QCopilot/qcopilot.exe");

    // 构建命令行参数 - qcopilot 需要配置文件路径参数
    QStringList arguments;
    QString configFilePath = QDir(appDir).filePath("QCopilot/QCopilotConfig-windows.json");
    arguments << "--config-file-path" << configFilePath;

    // 检查 qcopilot.exe 文件是否存在
    QFileInfo programFile(program);
    if (!programFile.exists())
    {
        QgsMessageLog::logMessage(QString(" qcopilot 可执行文件不存在: %1").arg(program), "Copilots", Qgis::Critical);
        mQGisIface->messageBar()->pushMessage("Copilots", QString(" qcopilot 可执行文件不存在: %1").arg(program), Qgis::Critical, 10);
        return;
    }

    // 检查 config.json 文件是否存在
    QFileInfo configFile(configFilePath);
    if (!configFile.exists())
    {
        QgsMessageLog::logMessage(QString(" config.json 配置文件不存在: %1").arg(configFilePath), "Copilots", Qgis::Critical);
        mQGisIface->messageBar()->pushMessage("Copilots", QString(" config.json 配置文件不存在: %1").arg(configFilePath), Qgis::Critical, 10);
        return;
    }

    // 初始化并启动 server 进程（采用相对路径）
    if(!mServerProcess)
    {
        mServerProcess = new QProcess(this);

        // 获取当前环境变量
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

        // 将修改后的环境变量设置给 QProcess
        mServerProcess->setProcessEnvironment(env);

#ifdef Q_OS_WIN
        // 在Windows上隐藏控制台窗口
        mServerProcess->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#endif

        // 连接进程结束信号，用于监控进程状态
        connect(mServerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitStatus == QProcess::CrashExit)
                    {
                        QString msg = QString("qcopilot 进程异常退出，退出码: %1").arg(exitCode);
                        QgsMessageLog::logMessage(msg, "Copilots", Qgis::Warning);
                        mQGisIface->messageBar()->pushMessage("Copilots", msg, Qgis::Warning, 8);
                    }
                    else
                    {
                        QgsMessageLog::logMessage("qcopilot 进程正常退出", "Copilots", Qgis::Info);
                        mQGisIface->messageBar()->pushMessage("Copilots", "qcopilot 服务器已停止", Qgis::Info, 5);
                    }
                });
    }

    // 在启动前显示提示信息
    mQGisIface->messageBar()->pushMessage("Copilots", "正在启动 qcopilot 服务器...", Qgis::Info, 5);

    // 启动进程
    mServerProcess->start(program, arguments);

    // 等待进程启动，检查是否成功
    if(!mServerProcess->waitForStarted(5000))
    {
        QString errorMsg = QString(" qcopilot 启动失败！错误信息: %1").arg(mServerProcess->errorString());
        QgsMessageLog::logMessage(errorMsg, "Copilots", Qgis::Critical);
        mQGisIface->messageBar()->pushMessage("Copilots", errorMsg, Qgis::Critical, 10);
    }
    else
    {
        QgsMessageLog::logMessage("qcopilot 启动成功！", "Copilots", Qgis::Info);
        mQGisIface->messageBar()->pushMessage("Copilots", " qcopilot 服务器启动成功！正在准备智能对话、智能代码、智能体服务...", Qgis::Success, 5);
    }
}

//	1、Copilot_Chat 	对话补全
void PluginCopilotsToolbox::Copilot_Chat()
{
    // 确保服务器已启动
    startServerIfNeeded();

    //  打开网页
    QDesktopServices::openUrl(QUrl(QString("http://127.0.0.1:8081")));
}
//	2、Copilot_Coder 代码补全
void PluginCopilotsToolbox::Copilot_Coder()
{
    // 确保服务器已启动
    startServerIfNeeded();

    //  打开网页
    QDesktopServices::openUrl(QUrl(QString("http://127.0.0.1:8081")));
}
//	3、Copilot_Agent 代理
void PluginCopilotsToolbox::Copilot_Agent()
{
    // 确保服务器已启动
    startServerIfNeeded();

    //  打开网页
    QDesktopServices::openUrl(QUrl(QString("http://127.0.0.1:8081")));
}


void PluginCopilotsToolbox::unload()
{
	// 首先终止 qcopilot 进程及其子进程
	if (mServerProcess && mServerProcess->state() == QProcess::Running)
	{
		QgsMessageLog::logMessage("插件卸载时终止 qcopilot 进程及其子进程...", "Copilots", Qgis::Info);

#ifdef Q_OS_WIN
        // 在Windows上，使用taskkill命令强制终止进程树
        QProcess killProcess;
        QString processId = QString::number(mServerProcess->processId());
        killProcess.start("taskkill", QStringList() << "/F" << "/T" << "/PID" << processId);
        killProcess.waitForFinished(3000);

        if (killProcess.exitCode() == 0)
        {
            QgsMessageLog::logMessage("已通过 taskkill 终止 qcopilot 进程树", "Copilots", Qgis::Info);
        }
        else
        {
            QgsMessageLog::logMessage(" taskkill 命令执行失败，尝试标准终止方式", "Copilots", Qgis::Warning);
        }
#endif

		mServerProcess->terminate();

		if (!mServerProcess->waitForFinished(3000))
		{
			QgsMessageLog::logMessage(" qcopilot 进程未能正常退出，强制终止...", "Copilots", Qgis::Warning);
			mServerProcess->kill();
			mServerProcess->waitForFinished(1000);
		}
		else
		{
			QgsMessageLog::logMessage(" qcopilot 进程已正常退出", "Copilots", Qgis::Info);
		}
	}

	// remove the GUI
	//  1、Copilot_Chat 	对话补全
	mQGisIface->removePluginVectorMenu(tr("&Copilots"), mAction_Copilot_Chat);
	mQGisIface->removeVectorToolBarIcon(mAction_Copilot_Chat);
	//	2、Copilot_Coder 代码补全
	mQGisIface->removePluginVectorMenu(tr("&Copilots"), mAction_Copilot_Coder);
	mQGisIface->removeVectorToolBarIcon(mAction_Copilot_Coder);
	//	3、Copilot_Agent 代理
	mQGisIface->removePluginVectorMenu(tr("&Copilots"), mAction_Copilot_Agent);
	mQGisIface->removeVectorToolBarIcon(mAction_Copilot_Agent);

	// 释放指针
	delete mAction_Copilot_Chat;
	delete mAction_Copilot_Coder;
	delete mAction_Copilot_Agent;
	delete mServerProcess;
}


void PluginCopilotsToolbox::updateActions()
{
	// 更新菜单状态
}

/**
 * Required extern functions needed  for every plugin
 * These functions can be called prior to creating an instance
 * of the plugin class
 */
 // Class factory to return a new instance of the plugin class
QGISEXTERN QgisPlugin* classFactory(QgisInterface* qgisInterfacePointer)
{
	return new PluginCopilotsToolbox(qgisInterfacePointer);
}

// Return the name of the plugin - note that we do not user class members as
// the class may not yet be insantiated when this method is called.
QGISEXTERN const QString* name()
{
	return &sName;
}

// Return the description
QGISEXTERN const QString* description()
{
	return &sDescription;
}

// Return the category
QGISEXTERN const QString* category()
{
	return &sCategory;
}

// Return the type (either UI or MapLayer plugin)
QGISEXTERN int type()
{
	return sPluginType;
}

// Return the version number for the plugin
QGISEXTERN const QString* version()
{
	return &sPluginVersion;
}

QGISEXTERN const QString* icon()
{
	return &sPluginIcon;
}

// Delete ourself
QGISEXTERN void unload(QgisPlugin* pluginPointer)
{
	delete pluginPointer;
}


