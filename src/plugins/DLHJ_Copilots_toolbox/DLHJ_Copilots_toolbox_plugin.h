#ifndef DLHJ_COPILOTS_TOOLBOX_H
#define DLHJ_COPILOTS_TOOLBOX_H

#include "qgisplugin.h"
#include <QObject>
#include <QProcess>

#include <string>
#include <vector>


class QAction;
class QgisInterface;

class DLHJ_Copilots_toolbox : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit DLHJ_Copilots_toolbox(QgisInterface* qgisInterface);
	~DLHJ_Copilots_toolbox() override;

public slots:

	//! 初始化Gui
	void initGui() override;
	//! actions
	
	// 	1、DLHJ_Copilot_Chat 	对话补全
	void DLHJ_Copilot_Chat();
	// 	2、DLHJ_Copilot_Coder 代码补全
	void DLHJ_Copilot_Coder();
	// 	3、DLHJ_Copilot_Agent 代理
	void DLHJ_Copilot_Agent();

	//! 卸载插件
	void unload() override;

private:
	
	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;

	//!pointer to the qaction for this plugin
	
	// 1、DLHJ_Copilot_Chat  对话补全
	QAction* mAction_DLHJ_Copilot_Chat = nullptr;
	// 2、DLHJ_Copilot_Coder 代码补全
	QAction* mAction_DLHJ_Copilot_Coder = nullptr;
	// 3、DLHJ_Copilot_Agent 代理
	QAction* mAction_DLHJ_Copilot_Agent = nullptr;

  // 成员变量用于管理server进程
	QProcess* mServerProcess = nullptr;

private slots:
	void updateActions();

private:



};

#endif // DLHJ_COPILOTS_TOOLBOX_H
