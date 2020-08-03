/* This file is KoDocument of the KDE project
  Copyright (C) 2007 Dag Andersen <dag.andersen@kdemail.net>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
* Boston, MA 02110-1301, USA.
*/

#ifndef KPTACCOUNTSEDITOR_H
#define KPTACCOUNTSEDITOR_H

#include "planui_export.h"

#include <kptviewbase.h>
#include "kptaccountsmodel.h"

#include <kpagedialog.h>

class KoPageLayoutWidget;
class KoDocument;

class QPoint;


namespace KPlato
{

class Project;
class Account;
class AccountTreeView;

class AccountseditorConfigDialog : public KPageDialog {
    Q_OBJECT
public:
    AccountseditorConfigDialog(ViewBase *view, AccountTreeView *treeview, QWidget *parent, bool selectPrint = false);

public Q_SLOTS:
    void slotOk();

private:
    ViewBase *m_view;
    AccountTreeView *m_treeview;
    KoPageLayoutWidget *m_pagelayout;
    PrintingHeaderFooter *m_headerfooter;
};

class PLANUI_EXPORT AccountTreeView : public TreeViewBase
{
    Q_OBJECT
public:
    explicit AccountTreeView(QWidget *parent);

    AccountItemModel *model() const { return static_cast<AccountItemModel*>(TreeViewBase::model()); }

    Project *project() const { return model()->project(); }
    void setProject(Project *project) { model()->setProject(project); }

    Account *currentAccount() const;
    Account *selectedAccount() const;
    QList<Account*> selectedAccounts() const;
    
Q_SIGNALS:
    void currentChanged(const QModelIndex&);
    void currentColumnChanged(const QModelIndex&, const QModelIndex&);
    void selectionChanged(const QModelIndexList&);

    void contextMenuRequested(const QModelIndex&, const QPoint&);
    
protected Q_SLOTS:
    void slotHeaderContextMenuRequested(const QPoint &pos);
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
    void currentChanged (const QModelIndex & current, const QModelIndex & previous) override;

protected:
    void contextMenuEvent (QContextMenuEvent * event) override;
    
};

class PLANUI_EXPORT AccountsEditor : public ViewBase
{
    Q_OBJECT
public:
    AccountsEditor(KoPart *part, KoDocument *document, QWidget *parent);
    
    void setupGui();
    Project *project() const override { return m_view->project(); }
    void draw(Project &project) override;
    void draw() override;

    AccountItemModel *model() const { return m_view->model(); }
    
    void updateReadWrite(bool readwrite) override;

    virtual Account *currentAccount() const;
    
    KoPrintJob *createPrintJob() override;

    bool loadContext(const KoXmlElement &context) override;
    void saveContext(QDomElement &context) const override;

Q_SIGNALS:
    void addAccount(KPlato::Account *account);
    void deleteAccounts(const QList<KPlato::Account*>&);
    
public Q_SLOTS:
    /// Activate/deactivate the gui
    void setGuiActive(bool activate) override;
    void slotEditCopy() override;

protected:
    void updateActionsEnabled(bool on);
    void insertAccount(Account *account, Account *parent, int row);

protected Q_SLOTS:
    void slotOptions() override;
    
private Q_SLOTS:
    void slotContextMenuRequested(const QModelIndex &index, const QPoint& pos);
    void slotHeaderContextMenuRequested(const QPoint &pos) override;

    void slotSelectionChanged(const QModelIndexList&);
    void slotCurrentChanged(const QModelIndex&);
    void slotEnableActions(bool on);

    void slotAddAccount();
    void slotAddSubAccount();
    void slotDeleteSelection();

    void slotAccountsOk();

private:
    AccountTreeView *m_view;

    QAction *actionAddAccount;
    QAction *actionAddSubAccount;
    QAction *actionDeleteSelection;

};

}  //KPlato namespace

#endif
