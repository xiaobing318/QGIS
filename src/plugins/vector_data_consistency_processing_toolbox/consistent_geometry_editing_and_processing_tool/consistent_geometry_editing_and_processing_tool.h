#ifndef QGS_CONSISTENT_GEOMETRY_EDITING_AND_PROCESSING_TOOL_H
#define QGS_CONSISTENT_GEOMETRY_EDITING_AND_PROCESSING_TOOL_H

#include <vector>
#include <string>

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QWidget>

class qgs_consistent_geometry_editing_and_processing : public QDialog, private Ui::SE_ConsistencyMatchingGuiBase
{
    Q_OBJECT

  public:
    qgs_consistent_geometry_editing_and_processing( QWidget *parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
    ~qgs_consistent_geometry_editing_and_processing() override;

}


#endif // QGS_CONSISTENT_GEOMETRY_EDITING_AND_PROCESSING_TOOL_H
