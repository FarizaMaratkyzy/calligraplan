/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2010 Boudewijn Rempt
   SPDX-FileCopyrightText: 2011 Mojtaba Shahi Senobari <mojtaba.shahi3000@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-only
 */
// clazy:excludeall=qstring-arg
#include "KoOdfNumberDefinition.h"

#include "KoXmlNS.h"
#include "KoXmlWriter.h"

class Q_DECL_HIDDEN KoOdfNumberDefinition::Private
{
public:
    QString prefix;
    QString suffix;
    KoOdfNumberDefinition::FormatSpecification formatSpecification;
    bool letterSynchronization;
};

KoOdfNumberDefinition::KoOdfNumberDefinition()
    : d(new Private())
{
    d->formatSpecification = Numeric;
    d->letterSynchronization = false;
}

KoOdfNumberDefinition::KoOdfNumberDefinition(const KoOdfNumberDefinition &other)
    : d(new Private())
{
    d->prefix = other.d->prefix;
    d->suffix = other.d->suffix;
    d->formatSpecification = other.d->formatSpecification;
    d->letterSynchronization = other.d->letterSynchronization;
}

KoOdfNumberDefinition &KoOdfNumberDefinition::operator=(const KoOdfNumberDefinition &other)
{
    d->prefix = other.d->prefix;
    d->suffix = other.d->suffix;
    d->formatSpecification = other.d->formatSpecification;
    d->letterSynchronization = other.d->letterSynchronization;

    return *this;
}

KoOdfNumberDefinition::~KoOdfNumberDefinition()
{
    delete d;
}

QStringList KoOdfNumberDefinition::userFormatDescriptions()
{
    QStringList list;
    list << QStringLiteral("1, 2, 3, ...")
    << QStringLiteral("a, b, c, ...")
    << QStringLiteral("A, B, C, ...")
    << QStringLiteral("i, ii, iii, ...")
    << QStringLiteral("I, II, III, ...") << QStringLiteral("أ, ب, ت, ...")
    << QStringLiteral("ก, ข, ค, ...")
    << QStringLiteral("౧, ౨, ౩, ...")
    << QStringLiteral("௧, ௨, ௪, ...")
    << QStringLiteral("୧, ୨, ୩, ...")
    << QStringLiteral("൧, ൨, ൩, ...")
    << QStringLiteral("೧, ೨, ೩, ...")
    << QStringLiteral("੧, ੨, ੩, ...")
    << QStringLiteral("૧, ૨, ૩, ...")
    << QStringLiteral("১, ২, ৩, ...");

    return list;
}

void KoOdfNumberDefinition::loadOdf(const KoXmlElement &element)
{
    const QString format = element.attributeNS(KoXmlNS::style, "num-format", QString());
    if (format.isEmpty()) {
        //do nothing fall back to what we had.
    }
    else if (format[0] == QLatin1Char('1')) {
        d->formatSpecification = Numeric;
    }
    else if (format[0] == QLatin1Char('a')) {
        d->formatSpecification = AlphabeticLowerCase;
    }
    else if (format[0] == QLatin1Char('A')) {
        d->formatSpecification = AlphabeticUpperCase;
    }
    else if (format[0] == QLatin1Char('i')) {
        d->formatSpecification = RomanLowerCase;
    }
    else if (format[0] == QLatin1Char('I')) {
        d->formatSpecification = RomanUpperCase;
    }
    else if (format == QString::fromUtf8("أ, ب, ت, ...")){
        d->formatSpecification = ArabicAlphabet;
    }
    else if (format == QString::fromUtf8("ก, ข, ค, ...")){
        d->formatSpecification = Thai;
    }
    else if (format == QString::fromUtf8("أ, ب, ج, ...")) {
        d->formatSpecification = Abjad;
    }
    else if (format == QString::fromUtf8("ﺃ,ﺏ, ﺝ, ... ")) {
        d->formatSpecification = AbjadMinor;
    }
    else if (format == QString::fromUtf8("౧, ౨, ౩, ...")) {
        d->formatSpecification = Telugu;
    }
    else if (format == QString::fromUtf8("௧, ௨, ௪, ...")) {
        d->formatSpecification = Tamil;
    }
    else if (format == QString::fromUtf8("୧, ୨, ୩, ...")) {
        d->formatSpecification = Oriya;
    }
    else if (format == QString::fromUtf8("൧, ൨, ൩, ...")) {
        d->formatSpecification = Malayalam;
    }
    else if (format == QString::fromUtf8("೧, ೨, ೩, ...")) {
        d->formatSpecification = Kannada;
    }
    else if (format == QString::fromUtf8("੧, ੨, ੩, ...")) {
        d->formatSpecification = Gurumukhi;
    }
    else if (format == QString::fromUtf8("૧, ૨, ૩, ...")) {
        d->formatSpecification = Gujarati;
    }
    else if (format == QString::fromUtf8("১, ২, ৩, ...")) {
        d->formatSpecification = Bengali;
    }
    else {
        d->formatSpecification = Numeric;
    }

    //The style:num-prefix and style:num-suffix attributes specify what to display before and after the number.
    d->prefix = element.attributeNS(KoXmlNS::style, "num-prefix", d->prefix);
    d->suffix = element.attributeNS(KoXmlNS::style, "num-suffix", d->suffix);

    d->letterSynchronization = (element.attributeNS(KoXmlNS::style, "num-letter-sync", d->letterSynchronization ? "true" : "false") == QStringLiteral("true"));
}

void KoOdfNumberDefinition::saveOdf(KoXmlWriter *writer) const
{
    if (!d->prefix.isNull()) {
        writer->addAttribute("style:num-prefix", d->prefix);
    }

    if (!d->suffix.isNull()) {
        writer->addAttribute("style:num-suffix", d->suffix);
    }
    QByteArray format;
    switch(d->formatSpecification) {
    case Numeric:
        format = "1";
        break;
    case AlphabeticLowerCase:
        format = "a";
        break;
    case AlphabeticUpperCase:
        format = "A";
        break;
    case RomanLowerCase:
        format = "i";
        break;
    case RomanUpperCase:
        format = "I";
        break;
    case ArabicAlphabet:
        format = "أ, ب, ت, ...";
        break;
    case Thai:
        format = "ก, ข, ค, ...";
        break;
    case Telugu:
        format = "౧, ౨, ౩, ...";
        break;
    case Tamil:
        format = "௧, ௨, ௪, ...";
        break;
    case Oriya:
        format = "୧, ୨, ୩, ...";
        break;
    case Malayalam:
        format = "൧, ൨, ൩, ...";
        break;
    case Kannada:
        format = "೧, ೨, ೩, ...";
        break;
    case Gurumukhi:
        format = "੧, ੨, ੩, ...";
        break;
    case Gujarati:
        format = "૧, ૨, ૩, ...";
        break;
    case Bengali:
        format = "১, ২, ৩, ...";
        break;
    case Empty:
    default:
        ;
    };
    if (!format.isNull()) {
        writer->addAttribute("style:num-format", format);
    }

    if (d->letterSynchronization) {
        writer->addAttribute("style:num-letter-sync", "true");
    }
}

static QString intToRoman(int n)
{
    static const QString RNUnits[] = {QString(), QStringLiteral("i"), QStringLiteral("ii"), QStringLiteral("iii"), QStringLiteral("iv"), QStringLiteral("v"), QStringLiteral("vi"), QStringLiteral("vii"), QStringLiteral("viii"), QStringLiteral("ix")};
    static const QString RNTens[] = {QString(), QStringLiteral("x"), QStringLiteral("xx"), QStringLiteral("xxx"), QStringLiteral("xl"), QStringLiteral("l"), QStringLiteral("lx"), QStringLiteral("lxx"), QStringLiteral("lxxx"), QStringLiteral("xc")};
    static const QString RNHundreds[] = {QString(), QStringLiteral("c"), QStringLiteral("cc"), QStringLiteral("ccc"), QStringLiteral("cd"), QStringLiteral("d"), QStringLiteral("dc"), QStringLiteral("dcc"), QStringLiteral("dccc"), QStringLiteral("cm")};
    static const QString RNThousands[] = {QString(), QStringLiteral("m"), QStringLiteral("mm"), QStringLiteral("mmm"), QStringLiteral("mmmm"), QStringLiteral("mmmmm"), QStringLiteral("mmmmmm"), QStringLiteral("mmmmmmm"), QStringLiteral("mmmmmmmm"), QStringLiteral("mmmmmmmmm")};

    if (n <= 0) {
        return QString::number(n);
    }

    return RNThousands[(n / 1000)] +
           RNHundreds[(n / 100) % 10 ] +
           RNTens[(n / 10) % 10 ] +
           RNUnits[(n) % 10 ];
}

static QString intToAlpha(int n, bool letterSynchronization)
{
    QString answer;
    if (letterSynchronization) {
        int digits = 1;
        for (; n > 26; n -= 26)
            digits += 1;
        for (int i = 0; i < digits; i++)
            answer.prepend(QChar('a' + n - 1));
        return answer;
    } else {
        char bottomDigit;
        while (n > 26) {
            bottomDigit = (n - 1) % 26;
            n = (n - 1) / 26;
            answer.prepend(QChar('a' + bottomDigit));
        }
    }
    answer.prepend(QChar('a' + n - 1));
    return answer;
}

static QString intToScript(int n, int offset)
{
    // 10-base
    QString answer;
    while (n > 0) {
        answer.prepend(QChar(offset + n % 10));
        n = n / 10;
    }
    return answer;
}

static QString intToScriptList(int n, KoOdfNumberDefinition::FormatSpecification formatSpecification)
{
    // 1 time Sequences
    // note; the leading X is to make these 1 based.
    static const char* const Abjad[] = { "أ", "ب", "ج", "د", "ﻫ", "و", "ز", "ح", "ط", "ي", "ك", "ل", "م",
                                   "ن", "س", "ع", "ف", "ص", "ق", "ر", "ش", "ت", "ث", "خ", "ذ", "ض", "ظ", "غ"
                                 };
    static const char* const Abjad2[] = { "ﺃ", "ﺏ", "ﺝ", "ﺩ", "ﻫ", "ﻭ", "ﺯ", "ﺡ", "ﻁ", "ﻱ", "ﻙ", "ﻝ", "ﻡ",
                                    "ﻥ", "ﺹ", "ﻉ", "ﻑ", "ﺽ", "ﻕ", "ﺭ", "ﺱ", "ﺕ", "ﺙ", "ﺥ", "ﺫ", "ﻅ", "ﻍ", "ﺵ"
                                  };
    static const char* const ArabicAlphabet[] = {"ا", "ب", "ت", "ث", "ج", "ح", "خ", "د", "ذ", "ر", "ز",
                                           "س", "ش", "ص", "ض", "ط", "ظ", "ع", "غ", "ف", "ق", "ك", "ل", "م", "ن", "ه", "و", "ي"
                                          };

    /*
    // see this page for the 10, 100, 1000 etc http://en.wikipedia.org/wiki/Chinese_numerals
    static const char* const chinese1[] = { '零','壹','貳','叄','肆','伍','陸','柒','捌','玖' };
    static const char* const chinese2[] = { '〇','一','二','三','四','五','六','七','八','九' };

    TODO: https://en.wikipedia.org/wiki/Korean_numerals
    https://en.wikipedia.org/wiki/Japanese_numerals
    'https://en.wikipedia.org/wiki/Hebrew_numerals'
    'https://en.wikipedia.org/wiki/Armenian_numerals'
    'https://en.wikipedia.org/wiki/Greek_numerals'
    'https://en.wikipedia.org/wiki/Cyrillic_numerals'
    'https://en.wikipedia.org/wiki/Sanskrit_numerals'
    'https://en.wikipedia.org/wiki/Ge%27ez_alphabet#Numerals'
    'https://en.wikipedia.org/wiki/Abjad_numerals'
    */

    switch (formatSpecification) {
    case KoOdfNumberDefinition::Abjad:
        if (n > 22) return QStringLiteral("*");
        return QString::fromUtf8(Abjad[n-1]);
    case KoOdfNumberDefinition::AbjadMinor:
        if (n > 22) return QStringLiteral("*");
        return QString::fromUtf8(Abjad2[n-1]);
    case KoOdfNumberDefinition::ArabicAlphabet:
        if (n > 28) return QStringLiteral("*");
        return QString::fromUtf8(ArabicAlphabet[n-1]);
    default:
        return QString::number(n);
    }
}

QString KoOdfNumberDefinition::formattedNumber(int number, KoOdfNumberDefinition *defaultDefinition) const
{
   switch(d->formatSpecification) {
    case Numeric:
        return QString::number(number);
        break;

    case AlphabeticLowerCase:
        return intToAlpha(number, d->letterSynchronization);
    case AlphabeticUpperCase:
        return intToAlpha(number, d->letterSynchronization).toUpper();

    case RomanLowerCase:
        return intToRoman(number);
    case RomanUpperCase:
        return intToRoman(number).toUpper();

    case Thai:
        return intToScript(number, 0xe50);
    case Tibetan:
        return intToScript(number, 0xf20);
    case Telugu:
        return intToScript(number, 0xc66);
    case Tamil:
        return intToScript(number, 0x0be6);
    case Oriya:
        return intToScript(number, 0xb66);
    case Malayalam:
        return intToScript(number, 0xd66);
    case Kannada:
        return intToScript(number, 0xce6);
    case Gurumukhi:
        return intToScript(number, 0xa66);
    case Gujarati:
        return intToScript(number, 0xae6);
    case Bengali:
        return intToScript(number, 0x9e6);

    case Abjad:
    case AbjadMinor:
    case ArabicAlphabet:
        return intToScriptList(number, d->formatSpecification);

    case Empty:
        if (defaultDefinition) {
            return defaultDefinition->formattedNumber(number);
        }

        break;
    default:
        ;
    };

    return QString();
}


QString KoOdfNumberDefinition::prefix() const
{
    return d->prefix;
}

void KoOdfNumberDefinition::setPrefix(const QString &prefix)
{
    d->prefix = prefix;
}

QString KoOdfNumberDefinition::suffix() const
{
    return d->suffix;
}

void KoOdfNumberDefinition::setSuffix(const QString &suffix)
{
    d->suffix = suffix;
}

KoOdfNumberDefinition::FormatSpecification KoOdfNumberDefinition::formatSpecification() const
{
    return d->formatSpecification;
}

void KoOdfNumberDefinition::setFormatSpecification(FormatSpecification formatSpecification)
{
    d->formatSpecification = formatSpecification;
}

bool KoOdfNumberDefinition::letterSynchronization() const
{
    return d->letterSynchronization;
}

void KoOdfNumberDefinition::setLetterSynchronization(bool letterSynchronization)
{
    d->letterSynchronization = letterSynchronization;
}
