/* This file is part of the KDE libraries
   SPDX-FileCopyrightText: 2001 Werner Trobin <trobin@kde.org>
                 2002 Werner Trobin <trobin@kde.org>

SPDX-License-Identifier: LGPL-2.0-or-later
*/

// clazy:excludeall=qstring-arg
#include "KoFilter.h"

#include <QFile>
#include <QUrl>
#include <MainDebug.h>
#include <QStack>
#include "KoFilterManager.h"
#include "KoUpdater.h"

class Q_DECL_HIDDEN KoFilter::Private
{
public:
    QPointer<KoUpdater> updater;

    Private() :updater(nullptr) {}
};

KoFilter::KoFilter(QObject *parent)
    : QObject(parent), m_chain(nullptr), d(new Private)
{
}

KoFilter::~KoFilter()
{
    if (d->updater) d->updater->setProgress(100);
    delete d;
}

void KoFilter::setUpdater(const QPointer<KoUpdater>& updater)
{
    if (d->updater && !updater) {
        connect(this, &KoFilter::sigProgress, this, &KoFilter::slotProgress);
    } else if (!d->updater && updater) {
        connect(this, &KoFilter::sigProgress, this, &KoFilter::slotProgress);
    }
    d->updater = updater;
}

void KoFilter::slotProgress(int value)
{
    if (d->updater) {
        d->updater->setValue(value);
    }
}
