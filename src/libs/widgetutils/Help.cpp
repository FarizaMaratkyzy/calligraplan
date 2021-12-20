/* This file is part of the KDE project
  SPDX-FileCopyrightText: 2017 Dag Andersen <dag.andersen@kdemail.net>

  SPDX-License-Identifier: LGPL-2.0-or-later
*/

// clazy:excludeall=qstring-arg
#include "Help.h"

#include <KHelpClient>
#include <KDesktopFile>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QUrlQuery>
#include <QEvent>
#include <QWhatsThisClickedEvent>
#include <QDesktopServices>
#include <QWidget>

using namespace KPlato;

const QLoggingCategory &PLANHELP_LOG()
{
    static const QLoggingCategory category("calligra.plan.help");
    return category;
}


KPlato::Help* KPlato::Help::self = nullptr;

Help::Help()
{
    if (self) {
        delete self;
    }
    self = this;
    m_contentsUrl.setScheme("help");
    m_contextUrl.setScheme("help");
}

Help::~Help()
{
    self = nullptr;
}

KPlato::Help *KPlato::Help::instance()
{
    if (!self) {
        self = new Help();
    }
    return self;
}

void KPlato::Help::setContentsUrl(const QUrl& url)
{
    m_contentsUrl = url;
}

void KPlato::Help::setContextUrl(const QUrl& url)
{
    m_contextUrl = url;
}


void Help::setDocs(const QStringList &docs)
{
    m_docs.clear();
    qInfo()<<Q_FUNC_INFO<<docs;
    for (auto s : docs) {
        int last = s.length() - 1;
        int pos = s.indexOf(':');
        if (pos > 0 && pos < last) {
            m_docs.insert(s.left(pos), s.right(last - pos));
        }
        qInfo()<<Q_FUNC_INFO<<s<<pos<<last<<m_docs;
    }
    qInfo()<<Q_FUNC_INFO<<m_docs;
}

QString Help::doc(const QString &key) const
{
    return m_docs.value(key);
}

bool Help::eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object);
    if (event->type() == QEvent::WhatsThisClicked) {
        QWhatsThisClickedEvent *e = static_cast<QWhatsThisClickedEvent*>(event);
        QUrl url(e->href());
        if (url.isValid()) {
            return Help::instance()->invokeContext(url);
        }
    }
    return false;
}

bool KPlato::Help::invokeContent(QUrl url)
{
    QDesktopServices::openUrl(url);
    return true;
}

bool KPlato::Help::invokeContext(QUrl url)
{
    debugPlanHelp<<"treat:"<<url.scheme()<<url.path()<<url.fragment()<<m_contextUrl.scheme()<<m_contextUrl.host()<<m_contextUrl.path();
    QString document = doc(url.scheme());
    if (document.isEmpty()) {
        return false;
    }
    QString s = m_contextUrl.scheme();
    if (!s.isEmpty()) {
        s += ':';
    }
    if (!m_contextUrl.host().isEmpty()) {
        s += m_contextUrl.host() + '/';
        if (m_contextUrl.host() == "docs.kde.org") {
            auto path = m_contextUrl.path();
            if (path.isEmpty()) {
                s += "trunk5/en/" + document;
            } else {
                if (path.startsWith('/')) {
                    path.remove(0, 1);
                }
                s += path;
            }
            if (!s.endsWith('/')) {
                s += '/';
            }
        }
    }
    s += document + '/';
    s += url.path();
    if (!s.endsWith(".html")) {
        s += ".html";
    }
    if (url.hasFragment()) {
        s += '#' + url.fragment();
    }
    url = QUrl::fromUserInput(s);
    debugPlanHelp<<"open:"<<url;
    QDesktopServices::openUrl(url);
    return true;
}
