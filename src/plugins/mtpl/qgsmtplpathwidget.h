/***************************************************************************
  qgsmtplpathwidget.h
  -------------------
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

#ifndef QGSMTPLPATHWIDGET_H
#define QGSMTPLPATHWIDGET_H

#include <QWidget>

class QLineEdit;
class QToolButton;
class QDragEnterEvent;
class QDropEvent;
class QMimeData;

class QgsMtplPathWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit QgsMtplPathWidget( QWidget *parent = nullptr );

    QString path() const;
    void setPath( const QString &path );
    void setReadOnly( bool readOnly );

  signals:
    void pathChanged( const QString &path );
    void pathSelected( const QString &path );

  protected:
    void dragEnterEvent( QDragEnterEvent *event ) override;
    void dropEvent( QDropEvent *event ) override;

  private slots:
    void browse();
    void commitPath();

  private:
    QString normalizedInput( const QString &input ) const;
    bool acceptMimeData( const QMimeData *mimeData ) const;

    QLineEdit *mLineEdit = nullptr;
    QToolButton *mBrowseButton = nullptr;
};

#endif // QGSMTPLPATHWIDGET_H
