#ifndef SE_UNZIP_PACKET_H
#define SE_UNZIP_PACKET_H

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_unzip_packet.h"
#include "commontype/se_analysis_ready_data_tool_def.h"
#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>
#include <sqlite3.h>

using namespace std;


class SE_UnzipPacketDlg : public QDialog
{
	Q_OBJECT

public:
	SE_UnzipPacketDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_UnzipPacketDlg() override;

	void restoreState();


private:

	Ui_UnzipPacketDialog ui;

private slots:

	// 打开分析就绪数据目录
	void pushButton_Open_clicked();

	// 保存结果
	void pushButton_Save_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();


	// 任务完成
	void onTaskFinished(bool result);

	// 保存日志数据
	void pushButton_SaveLog_clicked();

private:

	// 输入文件路径
	QString m_qstrInputDataPath;

	// 保存文件路径
	QString m_qstrOutputDataPath;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

private:

	// 计算进度
	void CalculateTotalProgress();

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

};

#endif // SE_UNZIP_PACKET_H
