#ifndef SE_TEXT_FILES_GBK2UTF8_H
#define SE_TEXT_FILES_GBK2UTF8_H

#include <QDialog>
#include <QString>
#include "ui_se_text_files_gbk2utf8.h"

class TextFilesGBK2UTF8 : public QDialog
{
	Q_OBJECT

public:
	TextFilesGBK2UTF8(QgisInterface* qgisInterface, QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~TextFilesGBK2UTF8() override;

private slots:
	//  打开输入数据目录
	void Button_OpenInputDataDir();
  //  打开输出数据目录
	void Button_OpenOutputDataDir();

	//	进行字符集编码转化处理
	void Button_OK();
	//  不进行字符集编码转化处理
	void Button_Cancel();

private:
	//	恢复保存参数
	void restoreState();
	//	保存参数
	void saveState();

  //  ui界面
  Ui_SeTextFilesGbk2Utf8* m_ui;
	//  输入数据目录路径
	QString m_qstrInputDataDir;
	//  输入数据目录路径
	QString m_qstrOutputDataDir;


};

#endif // SE_TEXT_FILES_GBK2UTF8_H
