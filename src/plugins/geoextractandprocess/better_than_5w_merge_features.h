#ifndef QGS_BETTER_THAN_5W_MERGE_FEATURES_H
#define QGS_BETTER_THAN_5W_MERGE_FEATURES_H

#include <QDialog>

#include "ui_better_than_5w_merge_features.h"

class QgsBetterThan5wMergeFeaturesDlg : public QDialog
{
	Q_OBJECT

public:
	QgsBetterThan5wMergeFeaturesDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsBetterThan5wMergeFeaturesDlg() override;

	// 获取军标数据存储路径
	QString GetJBDataPath();

	// 获取国标数据存储路径
	QString GetGBDataPath();

	// 获取匹配数据库文件路径
	QString GetMatchDBFilePath();

	// 获取保存数据路径
	QString GetSaveDataPath();

public slots:

private:

	Ui_QgsBetterThan5wMergeFeaturesDlg ui;

	// 恢复保存参数
	void restoreState();

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	// 国标数据存储路径
	QString m_qstrGBDataPath;

	// 军标数据存储路径
	QString m_qstrJBDataPath;

	// 匹配数据库文件路径
	QString m_qstrMatchDBPath;

	// 输出数据存储路径
	QString m_qstrSaveDataPath;

private slots:

	// 打开国标数据
	void Button_OpenGBDataPath_clicked();

	// 打开军标数据
	void Button_OpenJBDataPath_clicked();

	// 打开数据库
	void Button_OpenMatchDBPath_clicked();

	// 保存数据
	void Button_SavePath_clicked();

	void Button_OK_accepted();
	void Button_Cancel_rejected();
};

#endif // QGS_BETTER_THAN_5W_MERGE_FEATURES_H
