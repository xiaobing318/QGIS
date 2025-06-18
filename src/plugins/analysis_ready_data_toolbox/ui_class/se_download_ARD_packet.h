#ifndef SE_DOWNLOAD_ARD_PACKET
#define SE_DOWNLOAD_ARD_PACKET

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_download_ARD_packet.h"
#include "commontype/se_analysis_ready_data_tool_def.h"
#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>


using namespace std;


class SE_DownloadARDPacketDlg : public QDialog
{
	Q_OBJECT

public:
	SE_DownloadARDPacketDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_DownloadARDPacketDlg() override;

	void restoreState();


private:

	Ui_DownloadARDPacketDlg ui;

private slots:

	// 保存分析就绪数据目录
	void pushButton_Save_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 取消
	void pushButton_Cancel_clicked();

	// 测试连接
	void pushButton_TestConnection_clicked();

	// 任务完成
	void onTaskFinished(bool result);

	// 保存日志数据
	void pushButton_SaveLog_clicked();

private:

	// FTP服务器文件路径
	QString m_qstrFTPDataPath;

	// 目标路径
	QString m_qstrOutputDataPath;

	// 服务器或IP地址
	QString m_qstrAddress;

	// 端口号
	QString m_qstrPort;

	// 用户名
	QString m_qstrUserName;

	// 密码
	QString m_qstrPassword;

	// 保存日志文件路径
	QString m_qstrOutputLogPath;

private:

	// 计算进度
	void CalculateTotalProgress();

	// 构建FTP的URL
	string buildFTPURL(const std::string& host, int port, const std::string& username, const std::string& password, const std::string& path);

	// 对字符串进行URL编码
	std::string URLEncode(const std::string& input);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

	// 获取FTP目录列表
	bool GetFTPDirectoryListing(const std::string& url, const std::string& username, const std::string& password);

};

#endif // SE_DOWNLOAD_ARD_PACKET
