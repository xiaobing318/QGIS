#include "DLHJ_Copilots_toolbox_plugin.h"

/*------------QGIS include-----------------*/
#include "qgisinterface.h"
#include "qgsguiutils.h"
#include "qgsproject.h"
#include "qgsmessagebar.h"
#include "qgsmapcanvas.h"
#include "qgsapplication.h"

/*-------------Qt--------------------*/
#include <QAction>
#include <QMessageBox>
#include <QtCore/QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

//#include "DLHJ_Copilot_Chat.h"
//#include "DLHJ_Copilot_Coder.h"
//#include "DLHJ_Copilot_Agent.h"

// 消息日志
#include "qgsmessagelog.h"
#include "qgsmessagebar.h"

static const QString sName = QObject::tr("DLHJ Copilots Toolbox");
static const QString sDescription = QObject::tr("三个功能：DLHJ_Copilot_Chat智能对话；DLHJ_Copilot_Coder智能代码；DLHJ_Copilot_Agent智能代理");
static const QString sCategory = QObject::tr("Copilots");
static const QString sPluginVersion = QObject::tr("Version 0.0");
static const QgisPlugin::PluginType sPluginType = QgisPlugin::UI;
// 插件图标
static const QString sPluginIcon = QStringLiteral(":/DLHJ_Copilots_toolbox/icons/DLHJ_Copilot_plugin.svg");

DLHJ_Copilots_toolbox::DLHJ_Copilots_toolbox(QgisInterface* qgisInterface)
	: QgisPlugin(sName, sDescription, sCategory, sPluginVersion, sPluginType)
	, mQGisIface(qgisInterface)
{

}

DLHJ_Copilots_toolbox::~DLHJ_Copilots_toolbox()
{
    // 确保在插件销毁时正确终止 qcopilot 进程
    if (mServerProcess && mServerProcess->state() == QProcess::Running)
    {
        QgsMessageLog::logMessage("正在终止 qcopilot 进程...", "DLHJ_Copilots", Qgis::Info);
        
        // 发送终止信号（相当于 Ctrl+C）
        mServerProcess->terminate();
        
        // 等待进程正常退出，超时后强制杀死
        if (!mServerProcess->waitForFinished(5000))
        {
            QgsMessageLog::logMessage("qcopilot 进程未能正常退出，强制终止...", "DLHJ_Copilots", Qgis::Warning);
            mServerProcess->kill();
            mServerProcess->waitForFinished(2000);
        }
        else
        {
            QgsMessageLog::logMessage("qcopilot 进程已正常退出", "DLHJ_Copilots", Qgis::Info);
        }
    }
}



void DLHJ_Copilots_toolbox::initGui()
{
	//  确保之前的actions对象指针都被释放了
  if (mAction_DLHJ_Copilot_Chat)
  {
    delete mAction_DLHJ_Copilot_Chat;
  }
  if (mAction_DLHJ_Copilot_Coder)
  {
    delete mAction_DLHJ_Copilot_Coder;
  }
  if (mAction_DLHJ_Copilot_Agent)
  {
    delete mAction_DLHJ_Copilot_Agent;
  }
	

#pragma region "1、DLHJ_Copilot_Chat 	对话补全"
  // Create the action for tool
  mAction_DLHJ_Copilot_Chat = new QAction(QIcon(":/DLHJ_Copilots_toolbox/icons/DLHJ_Copilot_Chat.svg"), tr("DLHJ Copilot智能对话"), this);
  mAction_DLHJ_Copilot_Chat->setObjectName(QStringLiteral("mAction_DLHJ_Copilot_Chat"));
  // Set the what's this text
  mAction_DLHJ_Copilot_Chat->setWhatsThis(tr("DLHJ Copilot智能对话"));
  // Connect the action to the run
  connect(mAction_DLHJ_Copilot_Chat, &QAction::triggered, this, &DLHJ_Copilots_toolbox::DLHJ_Copilot_Chat);
  // Add the icon to the toolbar
  mQGisIface->addVectorToolBarIcon(mAction_DLHJ_Copilot_Chat);
  mQGisIface->addPluginToVectorMenu(tr("&DLHJ_Copilots"), mAction_DLHJ_Copilot_Chat);
  mAction_DLHJ_Copilot_Chat->setEnabled(true);

#pragma endregion


#pragma region "2、DLHJ_Copilot_Coder 代码补全"
	// Create the action for tool
	mAction_DLHJ_Copilot_Coder = new QAction(QIcon(":/DLHJ_Copilots_toolbox/icons/DLHJ_Copilot_Coder.svg"), tr("DLHJ Copilot智能代码"), this);
	mAction_DLHJ_Copilot_Coder->setObjectName(QStringLiteral("mAction_DLHJ_Copilot_Coder"));
	// Set the what's this text
	mAction_DLHJ_Copilot_Coder->setWhatsThis(tr("DLHJ Copilot智能代码"));
	// Connect the action to the run
	connect(mAction_DLHJ_Copilot_Coder, &QAction::triggered, this, &DLHJ_Copilots_toolbox::DLHJ_Copilot_Coder);
	// Add the icon to the toolbar
	mQGisIface->addVectorToolBarIcon(mAction_DLHJ_Copilot_Coder);
	mQGisIface->addPluginToVectorMenu(tr("&DLHJ_Copilots"), mAction_DLHJ_Copilot_Coder);
	mAction_DLHJ_Copilot_Coder->setEnabled(true);

#pragma endregion

#pragma region "3、DLHJ_Copilot_Agent 代理"
	// Create the action for tool
	mAction_DLHJ_Copilot_Agent = new QAction(QIcon(":/DLHJ_Copilots_toolbox/icons/DLHJ_Copilot_Agent.svg"), tr("DLHJ Copilot智能代理"), this);
	mAction_DLHJ_Copilot_Agent->setObjectName(QStringLiteral("mAction_DLHJ_Copilot_Agent"));
	// Set the what's this text
	mAction_DLHJ_Copilot_Agent->setWhatsThis(tr("DLHJ Copilot智能代理"));
	// Connect the action to the run
	connect(mAction_DLHJ_Copilot_Agent, &QAction::triggered, this, &DLHJ_Copilots_toolbox::DLHJ_Copilot_Agent);
	// Add the icon to the toolbar
	mQGisIface->addVectorToolBarIcon(mAction_DLHJ_Copilot_Agent);
	mQGisIface->addPluginToVectorMenu(tr("&DLHJ_Copilots"), mAction_DLHJ_Copilot_Agent);
	mAction_DLHJ_Copilot_Agent->setEnabled(true);

#pragma endregion

#pragma region "启动server进程"
    // 初始化并启动 server 进程（采用相对路径）
    if(!mServerProcess)
    {
        mServerProcess = new QProcess(this);

        // 获取当前应用程序的目录路径
        QString appDir = QCoreApplication::applicationDirPath();

        // 构建 qcopilot.exe 的路径
        QString program = QDir(appDir).filePath("QCopilot/qcopilot.exe");

        // 构建命令行参数 - qcopilot 需要配置文件路径参数
        QStringList arguments;
        QString configFilePath = QDir(appDir).filePath("QCopilot/config.json");
        arguments << "--config-file-path" << configFilePath;

        // 获取当前环境变量
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

        // 将修改后的环境变量设置给 QProcess
        mServerProcess->setProcessEnvironment(env);

        // 检查 qcopilot.exe 文件是否存在
        QFileInfo programFile(program);
        if (!programFile.exists())
        {
            QgsMessageLog::logMessage(QString("qcopilot.exe 文件不存在: %1").arg(program), "DLHJ_Copilots", Qgis::Critical);
            return;
        }

        // 检查 config.json 文件是否存在
        QFileInfo configFile(configFilePath);
        if (!configFile.exists())
        {
            QgsMessageLog::logMessage(QString("config.json 文件不存在: %1").arg(configFilePath), "DLHJ_Copilots", Qgis::Critical);
            return;
        }

        // 启动进程
        mServerProcess->start(program, arguments);

        // 等待进程启动，检查是否成功
        if(!mServerProcess->waitForStarted(5000))
        {
            QString errorMsg = QString("qcopilot 启动失败！错误信息: %1").arg(mServerProcess->errorString());
            QgsMessageLog::logMessage(errorMsg, "DLHJ_Copilots", Qgis::Critical);
        }
        else
        {
            QgsMessageLog::logMessage("qcopilot 启动成功！", "DLHJ_Copilots", Qgis::Info);
            
            // 连接进程结束信号，用于监控进程状态
            connect(mServerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
                        if (exitStatus == QProcess::CrashExit)
                        {
                            QgsMessageLog::logMessage(QString("qcopilot 进程异常退出，退出码: %1").arg(exitCode), 
                                                    "DLHJ_Copilots", Qgis::Warning);
                        }
                        else
                        {
                            QgsMessageLog::logMessage("qcopilot 进程正常退出", "DLHJ_Copilots", Qgis::Info);
                        }
                    });
        }
    }
#pragma endregion

	//	更新菜单状态
  updateActions();
}

//	1、DLHJ_Copilot_Chat 	对话补全
void DLHJ_Copilots_toolbox::DLHJ_Copilot_Chat()
{
	//  打开网页
  QDesktopServices::openUrl(QUrl(QString("http://127.0.0.1:8080")));
}
//	2、DLHJ_Copilot_Coder 代码补全
void DLHJ_Copilots_toolbox::DLHJ_Copilot_Coder()
{
	//  打开网页
	QDesktopServices::openUrl(QUrl(QString("http://127.0.0.1:8080")));
}
//	3、DLHJ_Copilot_Agent 代理
void DLHJ_Copilots_toolbox::DLHJ_Copilot_Agent()
{
	//  打开网页
	QDesktopServices::openUrl(QUrl(QString("http://127.0.0.1:8080")));
}


void DLHJ_Copilots_toolbox::unload()
{
	// 首先终止 qcopilot 进程
	if (mServerProcess && mServerProcess->state() == QProcess::Running)
	{
		QgsMessageLog::logMessage("插件卸载时终止 qcopilot 进程...", "DLHJ_Copilots", Qgis::Info);
		mServerProcess->terminate();
		
		if (!mServerProcess->waitForFinished(3000))
		{
			mServerProcess->kill();
			mServerProcess->waitForFinished(1000);
		}
	}

	// remove the GUI
	//  1、DLHJ_Copilot_Chat 	对话补全
	mQGisIface->removePluginVectorMenu(tr("&DLHJ_Copilots"), mAction_DLHJ_Copilot_Chat);
	mQGisIface->removeVectorToolBarIcon(mAction_DLHJ_Copilot_Chat);
	//	2、DLHJ_Copilot_Coder 代码补全
	mQGisIface->removePluginVectorMenu(tr("&DLHJ_Copilots"), mAction_DLHJ_Copilot_Coder);
	mQGisIface->removeVectorToolBarIcon(mAction_DLHJ_Copilot_Coder);
	//	3、DLHJ_Copilot_Agent 代理
	mQGisIface->removePluginVectorMenu(tr("&DLHJ_Copilots"), mAction_DLHJ_Copilot_Agent);	
	mQGisIface->removeVectorToolBarIcon(mAction_DLHJ_Copilot_Agent);

	// 释放指针
	delete mAction_DLHJ_Copilot_Chat;
	delete mAction_DLHJ_Copilot_Coder;
	delete mAction_DLHJ_Copilot_Agent;
	delete mServerProcess;
}


void DLHJ_Copilots_toolbox::updateActions()
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
	return new DLHJ_Copilots_toolbox(qgisInterfacePointer);
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


