#ifndef DLHJ_COPILOTS_H
#define DLHJ_COPILOTS_H

#include <QDialog>
#include <QDesktopServices>
#include <QUrl>

#include "qgisinterface.h"



class DLHJ_Copilot_Coder_Dialog : public QDialog
{
	Q_OBJECT

public:
	DLHJ_Copilot_Coder_Dialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~DLHJ_Copilot_Coder_Dialog() override;

public:

public slots:

private:

	// 	恢复保存参数
	void restoreState();

	//	插件接口
	QgisInterface* m_qgisInterface;

private slots:

	//	打开
	void Button_open_accepted();

	//  关闭
	void Button_cancel_rejected();
};

#endif // DLHJ_COPILOTS_H
