/***************************************************************************
  qgsmtplpathwidget.cpp
  ---------------------
  copyright            : (C) 2026 QGIS Project
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmtplpathwidget.h"

#include "qgsmtplpackage.h"
#include "qgsapplication.h"

#include <QDir>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QStandardPaths>
#include <QToolButton>
#include <QUrl>

QgsMtplPathWidget::QgsMtplPathWidget( QWidget *parent )
  : QWidget( parent )
{
  setAcceptDrops( true );

  auto *layout = new QHBoxLayout( this );
  layout->setContentsMargins( 0, 0, 0, 0 );
  layout->setSpacing( 4 );

  mLineEdit = new QLineEdit( this );
  mLineEdit->setObjectName( QStringLiteral( "mtplPathEdit" ) );
  mLineEdit->setPlaceholderText( tr( "选择或拖入 MTPL 数据包或文件夹" ) );
  mLineEdit->setClearButtonEnabled( true );
  mLineEdit->setAccessibleName( tr( "MTPL 数据包或文件夹路径" ) );
  layout->addWidget( mLineEdit, 1 );

  mBrowseButton = new QToolButton( this );
  mBrowseButton->setObjectName( QStringLiteral( "mtplBrowseButton" ) );
  mBrowseButton->setText( QStringLiteral( "…" ) );
  mBrowseButton->setIcon( QgsApplication::getThemeIcon( QStringLiteral( "/mActionFolder.svg" ) ) );
  mBrowseButton->setToolTip( tr( "浏览数据包或文件夹" ) );
  mBrowseButton->setAccessibleName( mBrowseButton->toolTip() );
  layout->addWidget( mBrowseButton );

  connect( mLineEdit, &QLineEdit::textChanged, this, &QgsMtplPathWidget::pathChanged );
  connect( mLineEdit, &QLineEdit::editingFinished, this, &QgsMtplPathWidget::commitPath );
  connect( mBrowseButton, &QToolButton::clicked, this, &QgsMtplPathWidget::browse );
}

QString QgsMtplPathWidget::path() const
{
  return normalizedInput( mLineEdit->text() );
}

void QgsMtplPathWidget::setPath( const QString &path )
{
  mLineEdit->setText( QDir::toNativeSeparators( normalizedInput( path ) ) );
}

void QgsMtplPathWidget::setReadOnly( bool readOnly )
{
  mLineEdit->setReadOnly( readOnly );
  mBrowseButton->setEnabled( !readOnly );
  setAcceptDrops( !readOnly );
}

void QgsMtplPathWidget::dragEnterEvent( QDragEnterEvent *event )
{
  if ( acceptMimeData( event->mimeData() ) )
    event->acceptProposedAction();
}

void QgsMtplPathWidget::dropEvent( QDropEvent *event )
{
  if ( !acceptMimeData( event->mimeData() ) )
    return;

  const QString localPath = event->mimeData()->urls().constFirst().toLocalFile();
  setPath( localPath );
  event->acceptProposedAction();
  emit pathSelected( path() );
}

void QgsMtplPathWidget::browse()
{
  QFileInfo currentInfo( path() );
  QString initialDirectory;
  if ( currentInfo.isDir() )
    initialDirectory = currentInfo.absoluteFilePath();
  else if ( currentInfo.dir().exists() )
    initialDirectory = currentInfo.absolutePath();
  else
    initialDirectory = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );

  QFileDialog dialog( this, tr( "选择 MTPL 数据包或文件夹" ), initialDirectory, QgsMtpl::packageFileFilter() );
  dialog.setObjectName( QStringLiteral( "mtplPathDialog" ) );
  dialog.setAcceptMode( QFileDialog::AcceptOpen );
  dialog.setFileMode( QFileDialog::ExistingFile );
  dialog.setOption( QFileDialog::DontUseNativeDialog, true );
  dialog.setOption( QFileDialog::ReadOnly, true );
  dialog.setLabelText( QFileDialog::LookIn, tr( "查找范围" ) );
  dialog.setLabelText( QFileDialog::FileName, tr( "文件名" ) );
  dialog.setLabelText( QFileDialog::FileType, tr( "文件类型" ) );
  dialog.setLabelText( QFileDialog::Accept, tr( "选择" ) );
  dialog.setLabelText( QFileDialog::Reject, tr( "取消" ) );

  QString selectedFolder;
  if ( QDialogButtonBox *buttonBox = dialog.findChild<QDialogButtonBox *>() )
  {
    auto *chooseFolder = new QPushButton( tr( "选择当前文件夹" ), &dialog );
    chooseFolder->setObjectName( QStringLiteral( "mtplChooseFolderButton" ) );
    buttonBox->addButton( chooseFolder, QDialogButtonBox::ActionRole );
    connect( chooseFolder, &QPushButton::clicked, &dialog, [&dialog, &selectedFolder]
    {
      selectedFolder = dialog.directory().absolutePath();
      dialog.QDialog::accept();
    } );
  }

  if ( dialog.exec() != QDialog::Accepted )
    return;

  const QString selectedPath = selectedFolder.isEmpty()
    ? ( dialog.selectedFiles().isEmpty() ? QString() : dialog.selectedFiles().constFirst() )
    : selectedFolder;
  if ( selectedPath.isEmpty() )
    return;

  setPath( selectedPath );
  emit pathSelected( path() );
}

void QgsMtplPathWidget::commitPath()
{
  const QString committed = path();
  if ( !committed.isEmpty() )
    setPath( committed );
  emit pathSelected( committed );
}

QString QgsMtplPathWidget::normalizedInput( const QString &input ) const
{
  QString value = input.trimmed();
  if ( value.size() >= 2 && value.startsWith( QLatin1Char( '"' ) ) && value.endsWith( QLatin1Char( '"' ) ) )
    value = value.mid( 1, value.size() - 2 );

  if ( value.isEmpty() )
    return QString();

  return QDir::cleanPath( QDir::fromNativeSeparators( value ) );
}

bool QgsMtplPathWidget::acceptMimeData( const QMimeData *mimeData ) const
{
  if ( !mimeData || !mimeData->hasUrls() || mimeData->urls().size() != 1 )
    return false;

  const QUrl url = mimeData->urls().constFirst();
  return url.isLocalFile() && QFileInfo::exists( url.toLocalFile() );
}
