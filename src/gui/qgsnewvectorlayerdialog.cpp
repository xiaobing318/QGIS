/***************************************************************************
                         qgsnewvectorlayerdialog.cpp  -  description
                             -------------------
    begin                : October 2004
    copyright            : (C) 2004 by Marco Hugentobler
    email                : marco.hugentobler@autoform.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsnewvectorlayerdialog.h"
#include "qgsapplication.h"
#include "qgsfilewidget.h"
#include "qgis.h"
#include "qgslogger.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsproviderregistry.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorfilewriter.h"
#include "qgssettings.h"
#include "qgsogrprovider.h"
#include "qgsgui.h"
#include "qgsiconutils.h"
#include "qgsfileutils.h"
#include "qgsvariantutils.h"

#include <QPushButton>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>

QgsNewVectorLayerDialog::QgsNewVectorLayerDialog( QWidget *parent, Qt::WindowFlags fl )
  : QDialog( parent, fl )
{
  setupUi( this );
  QgsGui::enableAutoGeometryRestore( this );

  connect( mAddAttributeButton, &QToolButton::clicked, this, &QgsNewVectorLayerDialog::mAddAttributeButton_clicked );
  connect( mRemoveAttributeButton, &QToolButton::clicked, this, &QgsNewVectorLayerDialog::mRemoveAttributeButton_clicked );
  connect( mFileFormatComboBox, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsNewVectorLayerDialog::mFileFormatComboBox_currentIndexChanged );
  connect( mTypeBox, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsNewVectorLayerDialog::mTypeBox_currentIndexChanged );
  connect( buttonBox, &QDialogButtonBox::helpRequested, this, &QgsNewVectorLayerDialog::showHelp );

  mAddAttributeButton->setIcon( QgsApplication::getThemeIcon( QStringLiteral( "/mActionNewAttribute.svg" ) ) );
  mRemoveAttributeButton->setIcon( QgsApplication::getThemeIcon( QStringLiteral( "/mActionDeleteAttribute.svg" ) ) );

  mTypeBox->addItem( QgsFields::iconForFieldType( QVariant::String ), QgsVariantUtils::typeToDisplayString( QVariant::String ), "String" );
  mTypeBox->addItem( QgsFields::iconForFieldType( QVariant::Int ), QgsVariantUtils::typeToDisplayString( QVariant::Int ), "Integer" );
  mTypeBox->addItem( QgsFields::iconForFieldType( QVariant::Double ), QgsVariantUtils::typeToDisplayString( QVariant::Double ), "Real" );
  mTypeBox->addItem( QgsFields::iconForFieldType( QVariant::Date ), QgsVariantUtils::typeToDisplayString( QVariant::Date ), "Date" );

  mWidth->setValidator( new QIntValidator( 1, 255, this ) );
  mPrecision->setValidator( new QIntValidator( 0, 15, this ) );

  const QgsWkbTypes::Type geomTypes[] =
  {
    QgsWkbTypes::NoGeometry,
    QgsWkbTypes::Point,
    QgsWkbTypes::MultiPoint,
    QgsWkbTypes::LineString,
    QgsWkbTypes::Polygon,
  };

  for ( const auto type : geomTypes )
    mGeometryTypeBox->addItem( QgsIconUtils::iconForWkbType( type ), QgsWkbTypes::translatedDisplayString( type ), type );
  mGeometryTypeBox->setCurrentIndex( -1 );

  mOkButton = buttonBox->button( QDialogButtonBox::Ok );
  mOkButton->setEnabled( false );

  mFileFormatComboBox->addItem( tr( "ESRI Shapefile" ), "ESRI Shapefile" );
#if 0
  // Disabled until provider properly supports editing the created file formats
  // When enabling this, adapt the window-title of the dialog and the title of all actions showing this dialog.
  mFileFormatComboBox->addItem( tr( "Comma Separated Value" ), "Comma Separated Value" );
  mFileFormatComboBox->addItem( tr( "GML" ), "GML" );
  mFileFormatComboBox->addItem( tr( "Mapinfo File" ), "Mapinfo File" );
#endif
  if ( mFileFormatComboBox->count() == 1 )
  {
    mFileFormatComboBox->setVisible( false );
    mFileFormatLabel->setVisible( false );
  }

  mCrsSelector->setShowAccuracyWarnings( true );

  mFileFormatComboBox->setCurrentIndex( 0 );

  mFileEncoding->addItems( QgsVectorDataProvider::availableEncodings() );

  // Use default encoding if none supplied
  const QString enc = QgsSettings().value( QStringLiteral( "/UI/encoding" ), "System" ).toString();

  // The specified decoding is added if not existing already, and then set current.
  // This should select it.
  int encindex = mFileEncoding->findText( enc );
  if ( encindex < 0 )
  {
    mFileEncoding->insertItem( 0, enc );
    encindex = 0;
  }
  mFileEncoding->setCurrentIndex( encindex );

  mAttributeView->addTopLevelItem( new QTreeWidgetItem( QStringList() << QStringLiteral( "id" ) << QStringLiteral( "Integer" ) << QStringLiteral( "10" ) << QString() ) );
  connect( mNameEdit, &QLineEdit::textChanged, this, &QgsNewVectorLayerDialog::nameChanged );
  connect( mAttributeView, &QTreeWidget::itemSelectionChanged, this, &QgsNewVectorLayerDialog::selectionChanged );
  connect( mGeometryTypeBox, static_cast<void( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, [ = ]( int )
  {
    updateExtension();
    checkOk();
  } );

  mAddAttributeButton->setEnabled( false );
  mRemoveAttributeButton->setEnabled( false );

  mFileName->setStorageMode( QgsFileWidget::SaveFile );
  mFileName->setFilter( QgsVectorFileWriter::filterForDriver( mFileFormatComboBox->currentData( Qt::UserRole ).toString() ) );
  mFileName->setConfirmOverwrite( false );
  mFileName->setDialogTitle( tr( "Save Layer As" ) );
  const QgsSettings settings;
  mFileName->setDefaultRoot( settings.value( QStringLiteral( "UI/lastVectorFileFilterDir" ), QDir::homePath() ).toString() );
  connect( mFileName, &QgsFileWidget::fileChanged, this, [ = ]
  {
    QgsSettings settings;
    const QFileInfo tmplFileInfo( mFileName->filePath() );
    settings.setValue( QStringLiteral( "UI/lastVectorFileFilterDir" ), tmplFileInfo.absolutePath() );
    checkOk();
  } );
}

void QgsNewVectorLayerDialog::mFileFormatComboBox_currentIndexChanged( int index )
{
  Q_UNUSED( index )
  if ( mFileFormatComboBox->currentText() == tr( "ESRI Shapefile" ) )
    mNameEdit->setMaxLength( 10 );
  else
    mNameEdit->setMaxLength( 32767 );
}

void QgsNewVectorLayerDialog::mTypeBox_currentIndexChanged( int index )
{
  // FIXME: sync with providers/ogr/qgsogrprovider.cpp
  switch ( index )
  {
    case 0: // Text data
      if ( mWidth->text().toInt() < 1 || mWidth->text().toInt() > 255 )
        mWidth->setText( QStringLiteral( "80" ) );
      mPrecision->setEnabled( false );
      mWidth->setValidator( new QIntValidator( 1, 255, this ) );
      break;

    case 1: // Whole number
      if ( mWidth->text().toInt() < 1 || mWidth->text().toInt() > 10 )
        mWidth->setText( QStringLiteral( "10" ) );
      mPrecision->setEnabled( false );
      mWidth->setValidator( new QIntValidator( 1, 10, this ) );
      break;

    case 2: // Decimal number
      if ( mWidth->text().toInt() < 1 || mWidth->text().toInt() > 20 )
        mWidth->setText( QStringLiteral( "20" ) );
      if ( mPrecision->text().toInt() < 1 || mPrecision->text().toInt() > 15 )
        mPrecision->setText( QStringLiteral( "6" ) );

      mPrecision->setEnabled( true );
      mWidth->setValidator( new QIntValidator( 1, 20, this ) );
      break;

    default:
      QgsDebugMsg( QStringLiteral( "unexpected index" ) );
      break;
  }
}

QgsWkbTypes::Type QgsNewVectorLayerDialog::selectedType() const
{
  QgsWkbTypes::Type wkbType = QgsWkbTypes::Unknown;
  wkbType = static_cast<QgsWkbTypes::Type>
            ( mGeometryTypeBox->currentData( Qt::UserRole ).toInt() );

  if ( mGeometryWithZRadioButton->isChecked() )
    wkbType = QgsWkbTypes::addZ( wkbType );

  if ( mGeometryWithMRadioButton->isChecked() )
    wkbType = QgsWkbTypes::addM( wkbType );

  return wkbType;
}

QgsCoordinateReferenceSystem QgsNewVectorLayerDialog::crs() const
{
  return mCrsSelector->crs();
}

void QgsNewVectorLayerDialog::setCrs( const QgsCoordinateReferenceSystem &crs )
{
  mCrsSelector->setCrs( crs );
}

void QgsNewVectorLayerDialog::mAddAttributeButton_clicked()
{
  const QString myName = mNameEdit->text();
  const QString myWidth = mWidth->text();
  const QString myPrecision = mPrecision->isEnabled() ? mPrecision->text() : QString();
  //use userrole to avoid translated type string
  const QString myType = mTypeBox->currentData( Qt::UserRole ).toString();
  mAttributeView->addTopLevelItem( new QTreeWidgetItem( QStringList() << myName << myType << myWidth << myPrecision ) );
  checkOk();
  mNameEdit->clear();
}

void QgsNewVectorLayerDialog::mRemoveAttributeButton_clicked()
{
  delete mAttributeView->currentItem();
  checkOk();
}

void QgsNewVectorLayerDialog::attributes( QList< QPair<QString, QString> > &at ) const
{
  QTreeWidgetItemIterator it( mAttributeView );
  while ( *it )
  {
    QTreeWidgetItem *item = *it;
    const QString type = QStringLiteral( "%1;%2;%3" ).arg( item->text( 1 ), item->text( 2 ), item->text( 3 ) );
    at.push_back( qMakePair( item->text( 0 ), type ) );
    QgsDebugMsg( QStringLiteral( "appending %1//%2" ).arg( item->text( 0 ), type ) );
    ++it;
  }
}

QString QgsNewVectorLayerDialog::selectedFileFormat() const
{
  //use userrole to avoid translated type string
  QString myType = mFileFormatComboBox->currentData( Qt::UserRole ).toString();
  return myType;
}

QString QgsNewVectorLayerDialog::selectedFileEncoding() const
{
  return mFileEncoding->currentText();
}

void QgsNewVectorLayerDialog::nameChanged( const QString &name )
{
  mAddAttributeButton->setDisabled( name.isEmpty() || !mAttributeView->findItems( name, Qt::MatchExactly ).isEmpty() );
}

void QgsNewVectorLayerDialog::selectionChanged()
{
  mRemoveAttributeButton->setDisabled( mAttributeView->selectedItems().isEmpty() );
}

QString QgsNewVectorLayerDialog::filename() const
{
  return mFileName->filePath();
}

void QgsNewVectorLayerDialog::setFilename( const QString &filename )
{
  mFileName->setFilePath( filename );
}

void QgsNewVectorLayerDialog::checkOk()
{
  const bool ok = ( !mFileName->filePath().isEmpty() && mAttributeView->topLevelItemCount() > 0 && mGeometryTypeBox->currentIndex() != -1 );
  mOkButton->setEnabled( ok );
}

// this is static
QString QgsNewVectorLayerDialog::runAndCreateLayer( QWidget *parent, QString *pEnc, const QgsCoordinateReferenceSystem &crs, const QString &initialPath )
{
  QString error;
  QString res = execAndCreateLayer( error, parent, initialPath, pEnc, crs );
  if ( res.isEmpty() && error.isEmpty() )
    res = QString( "" ); // maintain gross earlier API compatibility
  return res;
}

void QgsNewVectorLayerDialog::updateExtension()
{
  QString fileName = filename();
  const QString fileformat = selectedFileFormat();
  const QgsWkbTypes::Type geometrytype = selectedType();
  if ( fileformat == QLatin1String( "ESRI Shapefile" ) )
  {
    if ( geometrytype != QgsWkbTypes::NoGeometry )
    {
      fileName = fileName.replace( fileName.lastIndexOf( QLatin1String( ".dbf" ), -1, Qt::CaseInsensitive ), 4, QLatin1String( ".shp" ) );
      fileName = QgsFileUtils::ensureFileNameHasExtension( fileName, { QStringLiteral( "shp" ) } );
    }
    else
    {
      fileName = fileName.replace( fileName.lastIndexOf( QLatin1String( ".shp" ), -1, Qt::CaseInsensitive ), 4, QLatin1String( ".dbf" ) );
      fileName = QgsFileUtils::ensureFileNameHasExtension( fileName, { QStringLiteral( "dbf" ) } );
    }
  }
  setFilename( fileName );

}

void QgsNewVectorLayerDialog::accept()
{
  updateExtension();

  if ( QFile::exists( filename() ) && QMessageBox::warning( this, tr( "New ShapeFile Layer" ), tr( "The layer already exists. Are you sure you want to overwrite the existing file?" ),
       QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel ) != QMessageBox::Yes )
    return;

  QDialog::accept();
}

/*
* 杨小兵-2024-03-04
一、解释
#### 数据结构和算法
  在计算机实现方面，此函数主要涉及以下几个关键步骤和数据结构：
1. **初始化和参数设置**：函数开始时，清除任何已存在的错误消息，然后创建一个`QgsNewVectorLayerDialog`对话框实例用于收集用户输入的图层创建参数。
这包括设置图层的CRS、文件名等。

2. **用户交互**：通过`geomDialog.exec()`显示对话框，等待用户完成输入。如果用户取消操作，函数返回一个空字符串。

3. **读取用户输入**：从对话框中读取用户指定的文件格式、几何类型、文件名和编码。

4. **创建矢量图层**：使用用户提供的参数，尝试创建一个空的矢量数据源。这里用到`QgsOgrProviderUtils::createEmptyDataSource`方法，该方法基于OGR库
（一种支持多种矢量文件格式的开源库）实现，可以根据指定的几何类型和属性创建新的矢量图层。

5. **错误处理**：如果在创建图层的过程中遇到错误（如指定了未知的几何类型），函数会设置错误消息并返回一个空字符串。

6. **保存并返回文件名**：如果图层成功创建，函数最后更新配置设置，记录最后一次使用的文件目录和编码，并返回新创建的文件名。

#### 关键的类和方法
- `QgsNewVectorLayerDialog`：一个对话框类，用于收集用户关于新图层的输入。
- `QgsWkbTypes`：包含几何类型枚举的类。
- `QgsOgrProviderUtils`：一个工具类，提供创建空数据源的静态方法。
- `QgsSettings`：用于读写应用程序设置的类。

二、参数
errorMessage： 用来向调用者传递错误信息
parent：       挂接到父窗口
initialPath：  文件路径
encoding：     文件字符编码
crs：          参考坐标系统

三、show函数和exec函数之间的区别
  在Qt框架中，`show()`函数和`exec()`函数都可以用来显示一个对话框，但它们在行为上有明显的区别：
- `show()`函数用于非模态对话框，它会显示对话框但不会阻塞程序的其他部分。当使用`show()`时，对话框显示后，代码会立即继续执行到`show()`调用之后的部分。
这意味着，用户可以在对话框打开的同时与程序的其他部分互动。
- `exec()`函数用于模态对话框，它会显示对话框并阻塞程序的其他部分，直到对话框被关闭。模态对话框要求用户在继续使用程序之前必须首先处理对话框，这是通过
在对话框关闭之前暂停代码执行来实现的。`exec()`会在对话框关闭时返回一个结果，通常用于判断用户是接受了对话框（比如按下了OK按钮），还是拒绝了对话框（比
如按下了Cancel按钮或关闭了对话框）。
  在`QgsNewVectorLayerDialog::execAndCreateLayer`函数中，使用`geomDialog.exec()`而不是`geomDialog.show()`是因为需要创建矢量图层的操作要求用户
必须完成对话框中的输入，并做出明确的选择（如确定或取消）之后，程序才能根据这些输入继续执行。这种情况下，模态对话框比非模态对话框更合适，因为它可以确保
在用户提供必要的输入之前，程序的其他部分不会继续执行，从而避免了可能的错误或不一致状态。


三、总结
1、之前自己写的插件交互界面是通过指针的方式完成的，并且是在一个统一的插件中管理多个交互界面，但是这里是通过创建对象的方式来调用交互界面
2、通过`geomDialog.exec()`显示对话框，等待用户完成输入
3、使用QgsPluginRegistry::loadCppPlugin函数在加载插件的时候，将会调用initGui函数，这里同插件的处理方式就是有无initGui的差别，本质上都是一样
4、QT中的交互界面对象中有两种函数（show和exec函数）：show是非模态的函数，而exec则是模态函数
*/
QString QgsNewVectorLayerDialog::execAndCreateLayer( QString &errorMessage, QWidget *parent, const QString &initialPath, QString *encoding, const QgsCoordinateReferenceSystem &crs )
{
  errorMessage.clear();
  QgsNewVectorLayerDialog geomDialog( parent );
  geomDialog.setCrs( crs );
  if ( !initialPath.isEmpty() )
    geomDialog.setFilename( initialPath );
  if ( geomDialog.exec() == QDialog::Rejected )
  {
    return QString();
  }

  const QString fileformat = geomDialog.selectedFileFormat();
  const QgsWkbTypes::Type geometrytype = geomDialog.selectedType();
  QString fileName = geomDialog.filename();

  const QString enc = geomDialog.selectedFileEncoding();
  QgsDebugMsg( QStringLiteral( "New file format will be: %1" ).arg( fileformat ) );

  QList< QPair<QString, QString> > attributes;
  geomDialog.attributes( attributes );

  QgsSettings settings;
  settings.setValue( QStringLiteral( "UI/lastVectorFileFilterDir" ), QFileInfo( fileName ).absolutePath() );
  settings.setValue( QStringLiteral( "UI/encoding" ), enc );

  //try to create the new layer with OGRProvider instead of QgsVectorFileWriter
  if ( geometrytype != QgsWkbTypes::Unknown )
  {
    const QgsCoordinateReferenceSystem srs = geomDialog.crs();
    const bool success = QgsOgrProviderUtils::createEmptyDataSource( fileName, fileformat, enc, geometrytype, attributes, srs, errorMessage );
    if ( !success )
    {
      return QString();
    }
  }
  else
  {
    errorMessage = QObject::tr( "Geometry type not recognised" );
    QgsDebugMsg( errorMessage );
    return QString();
  }

  if ( encoding )
    *encoding = enc;

  return fileName;
}

void QgsNewVectorLayerDialog::showHelp()
{
  QgsHelp::openHelp( QStringLiteral( "managing_data_source/create_layers.html#creating-a-new-shapefile-layer" ) );
}
