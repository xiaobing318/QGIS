#ifndef URL_JUMP_H
#define URL_JUMP_H

#include <QDialog>
#include <QDesktopServices>
#include <QUrl>

#include "qgisinterface.h"
#include "ui_URL_jump.h"
#include "URL_jump.h"


class url_jump_dialog : public QDialog
{
	Q_OBJECT

public:
	url_jump_dialog(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~url_jump_dialog() override;

public:

public slots:

private:

  Ui_url_jump_dialog ui;

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

#endif // URL_JUMP_H
