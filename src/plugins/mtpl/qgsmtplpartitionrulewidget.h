/***************************************************************************
  qgsmtplpartitionrulewidget.h
  ----------------------------
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

#ifndef QGSMTPLPARTITIONRULEWIDGET_H
#define QGSMTPLPARTITIONRULEWIDGET_H

#include "services/qgsmtplpartitionrule.h"

#include <QHash>
#include <QSet>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;

/**
 * Master-detail editor for MTPL partition rules.
 *
 * The widget manages an in-memory working copy. The owner supplies rules and
 * persists the values emitted by ruleSaveRequested() and
 * ruleDeleteRequested(). Built-in rules are always presented read-only.
 */
class QgsMtplPartitionRuleWidget final : public QWidget
{
    Q_OBJECT

  public:
    explicit QgsMtplPartitionRuleWidget( QWidget *parent = nullptr );

    QList<QgsMtpl::PartitionRule> rules() const;
    QgsMtpl::PartitionRule currentRule() const;
    QString currentRuleId() const;
    bool currentRuleIsDirty() const;

    void setRules( const QList<QgsMtpl::PartitionRule> &rules, const QString &currentRuleId = QString() );
    void setCurrentRuleId( const QString &ruleId );
    void ruleSaveFailed( const QString &ruleId );
    void ruleDeleteSucceeded( const QString &ruleId );
    void ruleDeleteFailed( const QString &ruleId );

  signals:
    void currentRuleChanged( const QString &ruleId );
    void ruleDraftChanged( const QgsMtpl::PartitionRule &rule );
    void ruleSaveRequested( const QgsMtpl::PartitionRule &rule );
    void ruleDeleteRequested( const QString &ruleId );

  private:
    void addRule();
    void copyRule();
    void deleteRule();
    void saveRule();
    void saveRuleAs();
    void addBand();
    void deleteBand();
    void moveBandUp();
    void moveBandDown();
    void ruleSelectionChanged();
    void editorChanged();
    void bandSelectionChanged();

    void populateRuleList( const QString &preferredRuleId = QString() );
    void populateEditor();
    void populateBandTable( const QList<QgsMtpl::PartitionBand> &bands );
    void clearEditor();
    void updateRuleListItem( int ruleIndex );
    void updateSummaryAndActions();
    void updateBandActions();
    void markCurrentRuleDirty();
    QgsMtpl::PartitionRule ruleFromEditor() const;
    QString validationError( const QgsMtpl::PartitionRule &rule ) const;
    QString summaryText( const QgsMtpl::PartitionRule &rule ) const;
    QString uniqueRuleName( const QString &baseName ) const;
    QString newRuleId() const;
    int ruleIndexForId( const QString &ruleId ) const;
    bool currentRuleIsEditable() const;

    QList<QgsMtpl::PartitionRule> mRules;
    QSet<QString> mDirtyRuleIds;
    QHash<QString, QgsMtpl::PartitionRule> mRecentlyDeletedRules;
    QHash<QString, int> mRecentlyDeletedRuleIndexes;
    QSet<QString> mRecentlyDeletedDirtyRuleIds;
    int mCurrentRuleIndex = -1;
    bool mPopulating = false;

    QListWidget *mRuleList = nullptr;
    QPushButton *mAddRuleButton = nullptr;
    QPushButton *mCopyRuleButton = nullptr;
    QPushButton *mDeleteRuleButton = nullptr;
    QStackedWidget *mDetailsStack = nullptr;
    QLineEdit *mNameEdit = nullptr;
    QLabel *mRuleTypeLabel = nullptr;
    QWidget *mSummaryBar = nullptr;
    QLabel *mSummaryLabel = nullptr;
    QTableWidget *mBandTable = nullptr;
    QPushButton *mAddBandButton = nullptr;
    QPushButton *mDeleteBandButton = nullptr;
    QPushButton *mMoveBandUpButton = nullptr;
    QPushButton *mMoveBandDownButton = nullptr;
    QLabel *mValidationLabel = nullptr;
    QPushButton *mSaveButton = nullptr;
    QPushButton *mSaveAsButton = nullptr;
};

#endif // QGSMTPLPARTITIONRULEWIDGET_H
