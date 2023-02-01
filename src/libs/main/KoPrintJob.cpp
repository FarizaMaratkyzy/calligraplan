/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

// clazy:excludeall=qstring-arg
#include "KoPrintJob.h"

#include <QWidget>
#include <QPainter>

KoPrintJob::KoPrintJob(QObject *parent)
        : QObject(parent)
{
}

KoPrintJob::~KoPrintJob()
{
}

void KoPrintJob::startPrinting(RemovePolicy removePolicy)
{
    if (removePolicy == DeleteWhenDone)
        deleteLater();
}

QAbstractPrintDialog::PrintDialogOptions KoPrintJob::printDialogOptions() const
{
    return QAbstractPrintDialog::PrintToFile |
           QAbstractPrintDialog::PrintPageRange |
           QAbstractPrintDialog::PrintCollateCopies |
           QAbstractPrintDialog::PrintShowPageSize;
}

bool KoPrintJob::canPrint()
{
    if (! printer().isValid()) {
        return false;
    }

    QPainter testPainter(&printer());
    if (testPainter.isActive()) {
        return true;
    }

    return false;
}
