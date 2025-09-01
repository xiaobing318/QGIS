#ifndef PLUGIN_COPILOTS_TOOLBOX_H
#define PLUGIN_COPILOTS_TOOLBOX_H

#include "qgisplugin.h"
#include <QObject>
#include <QProcess>

#include <string>
#include <vector>


class QAction;
class QgisInterface;

class PluginCopilotsToolbox : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit PluginCopilotsToolbox(QgisInterface* qgisInterface);
	~PluginCopilotsToolbox() override;

public slots:

	//! 初始化Gui
	void initGui() override;
	//! actions

	// 	1、Copilot_Chat 	对话补全
	void Copilot_Chat();
	// 	2、Copilot_Coder 代码补全
	void Copilot_Coder();
	// 	3、Copilot_Agent 智能体
	void Copilot_Agent();

	//! 卸载插件
	void unload() override;

private:

	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;

	//!pointer to the qaction for this plugin

	// 1、Copilot_Chat  对话补全
	QAction* mAction_Copilot_Chat = nullptr;
	// 2、Copilot_Coder 代码补全
	QAction* mAction_Copilot_Coder = nullptr;
	// 3、Copilot_Agent 代理
	QAction* mAction_Copilot_Agent = nullptr;

  // 成员变量用于管理server进程
	QProcess* mServerProcess = nullptr;

private slots:
	void updateActions();
	void startServerIfNeeded();

private:



};

#endif // PLUGIN_COPILOTS_TOOLBOX_H
