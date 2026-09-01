/***************************************************************************
  qgsmtplpackagetoolswidget.h
  ---------------------------
  begin                : August 2026
  copyright            : (C) 2026 QGIS contributors
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMTPLPACKAGETOOLSWIDGET_H
#define QGSMTPLPACKAGETOOLSWIDGET_H

#include "qgis.h"

#include <QByteArray>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QgsMtplPackageOperationTask;

/**
 * Persistent package operation page embedded in the MTPL dock.
 *
 * The widget stays alive with the MTPL dock and owns one cancellable package
 * operation at a time.
 */
class QgsMtplPackageToolsWidget final : public QWidget
{
    Q_OBJECT

  public:
    explicit QgsMtplPackageToolsWidget( QWidget *parent = nullptr );
    ~QgsMtplPackageToolsWidget() override;

    void setSuggestedSourcePath( const QString &path );
    void reloadRememberedKeys();
    bool isOperationRunning() const;
    void concealSecrets();
    void shutdown();

  signals:
    void outputsCommitted( const QStringList &paths );
    void messageRequested( const QString &title, const QString &message, Qgis::MessageLevel level );

  private:
    enum class OperationUiState
    {
      Idle,
      Running,
      Canceling,
      Succeeded,
      PartiallySucceeded,
      Failed,
      Canceled
    };

    void updateOperationUi();
    void updateSourceFollowUi();
    void browseSource();
    void browseOutput();
    void restoreSuggestedSource();
    void sourceTextEdited( const QString &text );
    void startOperation();
    void cancelOperation();
    void operationFinished( QgsMtplPackageOperationTask *task, quint64 generation, bool successful );
    QString normalizedOutputPath( const QString &sourcePath ) const;
    void clearPendingSourceKeys();
    void setOperationUiState( OperationUiState state, const QString &status = QString() );
    void showInlineValidationError( const QString &message );
    void setResultDetails( const QString &message, const QString &sidecarPath = QString() );
    void applyDeferredSuggestedSource();

    QComboBox *mOperationCombo = nullptr;
    QLineEdit *mSourceEdit = nullptr;
    QPushButton *mSourceBrowseButton = nullptr;
    QPushButton *mRestoreSourceButton = nullptr;
    QLineEdit *mOutputEdit = nullptr;
    QPushButton *mOutputBrowseButton = nullptr;
    QComboBox *mFormatCombo = nullptr;
    QComboBox *mTileSizeCombo = nullptr;
    QPlainTextEdit *mMetadataEdit = nullptr;
    QWidget *mTileOptions = nullptr;
    QCheckBox *mEncryptCheck = nullptr;
    QGroupBox *mSourceKeysBox = nullptr;
    QLineEdit *mPrivateKeyEdit = nullptr;
    QLineEdit *mDeviceKeyEdit = nullptr;
    QLabel *mStatusLabel = nullptr;
    QPlainTextEdit *mResultDetails = nullptr;
    QProgressBar *mProgressBar = nullptr;
    QPushButton *mRunButton = nullptr;
    QPushButton *mCancelButton = nullptr;
    QWidget *mSidecarActions = nullptr;
    QPushButton *mCopySidecarButton = nullptr;
    QPushButton *mOpenSidecarFolderButton = nullptr;
    QPointer<QgsMtplPackageOperationTask> mTask;
    quint64 mOperationGeneration = 0;
    int mRunningOperation = -1;
    bool mSourceKeysEdited = false;
    bool mFollowsSuggestedSource = true;
    bool mShuttingDown = false;
    bool mReloadRememberedKeysDeferred = false;
    bool mHasDeferredSuggestedSource = false;
    QString mSuggestedSourcePath;
    QString mDeferredSuggestedSourcePath;
    QString mSidecarPath;
    QByteArray mPendingPrivateKey;
    QByteArray mPendingDeviceKey;
    QString mPendingSourcePath;
};

#endif // QGSMTPLPACKAGETOOLSWIDGET_H
