/* This file is part of the KDE project
  Copyright (C) 2009, 2011 Dag Andersen <dag.andersen@kdemail.net>

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

#ifndef KPTHTMLVIEW_H
#define KPTHTMLVIEW_H

#include "planui_export.h"

#include "kptviewbase.h"

#include "khtml_part.h"

class KoDocument;

class QUrl;
class QPoint;


namespace KPlato
{


class PLANUI_EXPORT HtmlView : public ViewBase
{
    Q_OBJECT
public:
    HtmlView(KoPart *part, KoDocument *doc, QWidget *parent);

    bool openHtml(const QUrl &url);

    void setupGui();

    void updateReadWrite(bool readwrite) override;

    KoPrintJob *createPrintJob() override;

    KHTMLPart &htmlPart() { return *m_htmlPart; }
    const KHTMLPart &htmlPart() const { return *m_htmlPart; }

Q_SIGNALS:
    void openUrlRequest(KPlato::HtmlView*, const QUrl&);

public Q_SLOTS:
    /// Activate/deactivate the gui
    void setGuiActive(bool activate) override;

    void slotOpenUrlRequest(const QUrl &url, const KParts::OpenUrlArguments &arguments=KParts::OpenUrlArguments(), const KParts::BrowserArguments &browserArguments=KParts::BrowserArguments());

protected:
    void updateActionsEnabled(bool on = true);

private Q_SLOTS:
    void slotContextMenuRequested(const QModelIndex &index, const QPoint& pos);
    
    void slotEnableActions(bool on);

private:
    KHTMLPart *m_htmlPart;

};

}  //KPlato namespace

#endif
