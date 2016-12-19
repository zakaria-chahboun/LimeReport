/***************************************************************************
 *   This file is part of the Lime Report project                          *
 *   Copyright (C) 2015 by Alexander Arin                                  *
 *   arin_a@bk.ru                                                          *
 *                                                                         *
 **                   GNU General Public License Usage                    **
 *                                                                         *
 *   This library is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 **                  GNU Lesser General Public License                    **
 *                                                                         *
 *   This library is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation, either version 3 of the    *
 *   License, or (at your option) any later version.                       *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library.                                      *
 *   If not, see <http://www.gnu.org/licenses/>.                           *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 ****************************************************************************/
#include <QtGui>
#include <QTextLayout>
#include <QtScript/QScriptEngine>
#include <QLocale>
#include <QMessageBox>
#include <math.h>

#include "lrpagedesignintf.h"
#include "lrtextitem.h"
#include "lrdesignelementsfactory.h"
#include "lrglobal.h"
#include "lrdatasourcemanager.h"
#include "lrsimpletagparser.h"
#include "lrtextitemeditor.h"
#include "lrreportengine_p.h"

namespace{

const QString xmlTag = "TextItem";

LimeReport::BaseDesignIntf * createTextItem(QObject* owner, LimeReport::BaseDesignIntf*  parent){
    return new LimeReport::TextItem(owner,parent);
}
bool VARIABLE_IS_NOT_USED registred = LimeReport::DesignElementsFactory::instance().registerCreator(xmlTag, LimeReport::ItemAttribs(QObject::tr("Text Item"),"TextItem"), createTextItem);

}

namespace LimeReport{

TextItem::TextItem(QObject *owner, QGraphicsItem *parent)
    : ContentItemDesignIntf(xmlTag,owner,parent), m_angle(Angle0), m_trimValue(true), m_allowHTML(false),
      m_allowHTMLInFields(false), m_followTo(""), m_follower(0)
{
    PageItemDesignIntf* pageItem = dynamic_cast<PageItemDesignIntf*>(parent);
    BaseDesignIntf* parentItem = dynamic_cast<BaseDesignIntf*>(parent);
    while (!pageItem && parentItem){
        parentItem = dynamic_cast<BaseDesignIntf*>(parentItem->parentItem());
        pageItem = dynamic_cast<PageItemDesignIntf*>(parentItem);
    }

    if (pageItem){
        QFont defaultFont = pageItem->font();
        setFont(defaultFont);
    }
    Init();
}

TextItem::~TextItem(){}

int TextItem::fakeMarginSize() const{
    return marginSize()+5;
}

void TextItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* style, QWidget* widget) {
    Q_UNUSED(widget);
    Q_UNUSED(style);


    TextPtr text = textDocument();

    painter->save();

    setupPainter(painter);
    prepareRect(painter,style,widget);

    QSizeF tmpSize = rect().size()-m_textSize;

    if (!painter->clipRegion().isEmpty()){
        QRegion clipReg=painter->clipRegion().xored(painter->clipRegion().subtracted(rect().toRect()));
        painter->setClipRegion(clipReg);
    } else {
        painter->setClipRect(rect());
    }

    qreal hOffset = 0, vOffset = 0;
    switch (m_angle){
        case Angle0:
            hOffset = fakeMarginSize();
            if ((tmpSize.height() > 0) && (m_alignment & Qt::AlignVCenter)){
                vOffset = tmpSize.height() / 2;
            }
            if ((tmpSize.height() > 0) && (m_alignment & Qt::AlignBottom)) // allow html
                vOffset = tmpSize.height();
            painter->translate(hOffset,vOffset);
        break;
        case Angle90:
            hOffset = width() - fakeMarginSize();
            vOffset = fakeMarginSize();
            if (m_alignment & Qt::AlignVCenter){
                hOffset = (width() - text->size().height()) / 2 + text->size().height();
            }

            if (m_alignment & Qt::AlignBottom){
                hOffset = (text->size().height());
            }
            painter->translate(hOffset,vOffset);
            painter->rotate(90);
        break;
        case Angle180:
            hOffset = width() - fakeMarginSize();
            vOffset = height() - fakeMarginSize();
            if ((tmpSize.width()>0) && (m_alignment & Qt::AlignVCenter)){
                vOffset = tmpSize.height() / 2+ text->size().height();
            }
            if ((tmpSize.height()>0) && (m_alignment & Qt::AlignBottom)){
                vOffset = (text->size().height());
            }
            painter->translate(hOffset,vOffset);
            painter->rotate(180);
        break;
        case Angle270:
            hOffset = fakeMarginSize();
            vOffset = height()-fakeMarginSize();
            if (m_alignment & Qt::AlignVCenter){
                hOffset = (width() - text->size().height())/2;
            }

            if (m_alignment & Qt::AlignBottom){
                hOffset = (width() - text->size().height());
            }
            painter->translate(hOffset,vOffset);
            painter->rotate(270);
        break;
        case Angle45:
            painter->translate(width()/2,0);
            painter->rotate(45);
            text->setTextWidth(sqrt(2*(pow(width()/2,2))));
        break;
        case Angle315:
            painter->translate(0,height()/2);
            painter->rotate(315);
            text->setTextWidth(sqrt(2*(pow(height()/2,2))));
        break;
    }

    int lineHeight = painter->fontMetrics().height();
    qreal curpos = 0;

    if (m_underlines){
        QPen pen = painter->pen();
        pen.setWidth(m_underlineLineSize);
        painter->setPen(pen);
    }

    painter->setOpacity(qreal(foregroundOpacity())/100);
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette.setColor(QPalette::Text, fontColor());

    for(QTextBlock it = text->begin(); it != text->end(); it=it.next()){
        it.blockFormat().setLineHeight(m_lineSpacing,QTextBlockFormat::LineDistanceHeight);
        for (int i=0;i<it.layout()->lineCount();i++){
            QTextLine line = it.layout()->lineAt(i);
            if (m_underlines){
                painter->drawLine(QPointF(0,line.rect().bottomLeft().y()),QPoint(rect().width(),line.rect().bottomRight().y()));
                lineHeight = line.height()+m_lineSpacing;
                curpos = line.rect().bottom();
            }
        }
    }

    text->documentLayout()->draw(painter,ctx);

    if (m_underlines){
        if (lineHeight<0) lineHeight = painter->fontMetrics().height();
        for (curpos+=lineHeight; curpos<rect().height();curpos+=lineHeight){
            painter->drawLine(QPointF(0,curpos),QPoint(rect().width(),curpos));
        }
    }

    painter->restore();
    BaseDesignIntf::paint(painter, style, widget);
}

QString TextItem::content() const{
    return m_strText;
}

void TextItem::Init()
{
    m_autoWidth = NoneAutoWidth;
    m_alignment = Qt::AlignLeft|Qt::AlignTop;
    m_autoHeight = false;
    m_textSize = QSizeF();
    m_firstLineSize = 0;
    m_foregroundOpacity = 100;
    m_underlines = false;
    m_adaptFontToSize = false;
    m_underlineLineSize = 1;
    m_lineSpacing = 1;
    m_valueType = Default;
}

void TextItem::setContent(const QString &value)
{
    if (m_strText.compare(value)!=0){
        QString oldValue = m_strText;
        m_strText=value;

        if (itemMode() == DesignMode){
            initTextSizes();
        }

        if (!isLoading()){
            initTextSizes();
            update(rect());
            notify("content",oldValue,value);
        }
    }
}

void TextItem::updateItemSize(DataSourceManager* dataManager, RenderPass pass, int maxHeight)
{
    if (isNeedExpandContent())
        expandContent(dataManager, pass);
    if (!isLoading())
        initTextSizes();

    if (m_textSize.width()>width() && ((m_autoWidth==MaxWordLength)||(m_autoWidth==MaxStringLength))){
        setWidth(m_textSize.width() + fakeMarginSize()*2);
    }

    if (m_textSize.height()>height()) {
        if (m_autoHeight)
            setHeight(m_textSize.height()+borderLineSize()*2);
        else if (hasFollower() && !content().isEmpty()){
            follower()->setContent(getTextPart(0,height()));
            setContent(getTextPart(height(),0));
        }
    }
    BaseDesignIntf::updateItemSize(dataManager, pass, maxHeight);
}

void TextItem::updateLayout()
{
//    m_layout.setFont(transformToSceneFont(font()));
//    m_layout.setText(content());
//    qreal linePos = 0;
//    m_layout.beginLayout();
//    while(true){
//        QTextLine line = m_layout.createLine();
//        if (!line.isValid()) break;
//        line.setLineWidth(width()-marginSize()*2);
//        line.setPosition(QPoint(marginSize(),linePos));
//        linePos+=line.height();
//    }
//    m_layout.endLayout();
}

bool TextItem::isNeedExpandContent() const
{
    QRegExp rx("$*\\{[^{]*\\}");
    return content().contains(rx);
}

QString TextItem::replaceBR(QString text)
{
    return text.replace("<br/>","\n");
}

QString TextItem::replaceReturns(QString text)
{
    QString result = text.replace("\r\n","<br/>");
    result = result.replace("\n","<br/>");
    return result;
}

void TextItem::setTextFont(TextPtr text, const QFont& value) {
    text->setDefaultFont(value);
    if ((m_angle==Angle0)||(m_angle==Angle180)){
        text->setTextWidth(rect().width()-fakeMarginSize()*2);
    } else {
        text->setTextWidth(rect().height()-fakeMarginSize()*2);
    }
}

void TextItem::adaptFontSize(TextPtr text) {
    QFont _font = transformToSceneFont(font());
    do{
        setTextFont(text,_font);
        if (_font.pixelSize()>2)
            _font.setPixelSize(_font.pixelSize()-1);
        else break;
    } while(text->size().height()>this->height() || text->size().width()>(this->width()) - fakeMarginSize() * 2);
}

int TextItem::underlineLineSize() const
{
    return m_underlineLineSize;
}

void TextItem::setUnderlineLineSize(int value)
{
    int oldValue = m_underlineLineSize;
    m_underlineLineSize = value;
    update();
    notify("underlineLineSize",oldValue,value);
}

int TextItem::lineSpacing() const
{
    return m_lineSpacing;
}

void TextItem::setLineSpacing(int value)
{
    int oldValue = m_lineSpacing;
    m_lineSpacing = value;
    initTextSizes();
    update();
    notify("lineSpacing",oldValue,value);
}


void TextItem::initTextSizes()
{
    TextPtr text = textDocument();
    m_textSize= text->size();
    if (text->begin().isValid() && text->begin().layout()->lineAt(0).isValid())
        m_firstLineSize = text->begin().layout()->lineAt(0).height();
}

QString TextItem::formatDateTime(const QDateTime &value)
{
    if (m_format.isEmpty())
    {
        return value.toString();
    }

    return value.toString(m_format);
}

QString TextItem::formatNumber(const double value)
{
    QString str = QString::number(value);

    if (m_format.contains("%"))
    {
        str.sprintf(m_format.toStdString().c_str(), value);
        str = str.replace(",", QLocale::system().groupSeparator());
        str = str.replace(".", QLocale::system().decimalPoint());
    }

    return str;
}

QString TextItem::formatFieldValue()
{
    if (m_format.isEmpty()) {
        return m_varValue.toString();
    }

    QVariant value = m_varValue;

    if (m_valueType != Default) {
        switch (m_valueType) {
        case DateTime:
            {
                QDateTime dt = QDateTime::fromString(value.toString(), Qt::ISODate);
                value = (dt.isValid() ? QVariant(dt) : m_varValue);
                break;
            }
        case Double:
            {
                bool bOk = false;
                double dbl = value.toDouble(&bOk);
                value = (bOk ? QVariant(dbl) : m_varValue);
            }
        default: break;
        }
    }

    switch (value.type()) {
    case QVariant::Date:
    case QVariant::DateTime:
        return formatDateTime(value.toDateTime());
    case QVariant::Double:
        return formatNumber(value.toDouble());
    default:
        return value.toString();
    }
}

TextItem::TextPtr TextItem::textDocument()
{
    TextPtr text(new QTextDocument);

    if (allowHTML())
        text->setHtml(m_strText);
    else
        text->setPlainText(m_strText);

    QTextOption to;
    to.setAlignment(m_alignment);

    if (m_autoWidth!=MaxStringLength)
        if (m_adaptFontToSize && (!(m_autoHeight || m_autoWidth)))
            to.setWrapMode(QTextOption::WordWrap);
        else
            to.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    else to.setWrapMode(QTextOption::NoWrap);

    text->setDocumentMargin(0);
    text->setDefaultTextOption(to);

    QFont _font = transformToSceneFont(font());
    if (m_adaptFontToSize && (!(m_autoHeight || m_autoWidth))){
        adaptFontSize(text);
    } else {
        setTextFont(text,_font);
    }

    text->documentLayout();

    for ( QTextBlock block = text->begin(); block.isValid(); block = block.next())
    {
        QTextCursor tc = QTextCursor(block);
        QTextBlockFormat fmt = block.blockFormat();

        if(fmt.lineHeight() != m_lineSpacing) {
            fmt.setLineHeight(m_lineSpacing,QTextBlockFormat::LineDistanceHeight);
            tc.setBlockFormat( fmt );
        }
    }

    return text;

}

QString TextItem::followTo() const
{
    return m_followTo;
}

void TextItem::setFollowTo(const QString &followTo)
{
    if (m_followTo != followTo){
        QString oldValue = m_followTo;
        m_followTo = followTo;
        if (!isLoading()){
            TextItem* fi = scene()->findChild<TextItem*>(followTo);
            if (fi && initFollower(followTo)){
                notify("followTo",oldValue,followTo);
            } else {
                m_followTo = "";
                QMessageBox::critical(
                    0,
                    tr("Error"),
                    tr("TextItem \" %1 \" already has folower \" %2 \" ")
                            .arg(fi->objectName())
                            .arg(fi->follower()->objectName())
                );
                notify("followTo",followTo,"");
            }
        }
    }
}

void TextItem::setFollower(TextItem *follower)
{
    if (!m_follower){
        m_follower = follower;
    }
}

bool TextItem::hasFollower()
{
    return m_follower != 0;
}

bool TextItem::initFollower(QString follower)
{
    TextItem* fi = scene()->findChild<TextItem*>(follower);
    if (fi){
        if (!fi->hasFollower()){
            fi->setFollower(this);
            return true;
        }
    }
    return false;
}

void TextItem::pageObjectHasBeenLoaded()
{
    if (!m_followTo.isEmpty()){
        initFollower(m_followTo);
    }
}

TextItem::ValueType TextItem::valueType() const
{
    return m_valueType;
}

void TextItem::setValueType(const ValueType valueType)
{
    m_valueType = valueType;
}

QString TextItem::format() const
{
    return m_format;
}

void TextItem::setFormat(const QString &format)
{
    m_format = format;
}

bool TextItem::allowHTMLInFields() const
{
    return m_allowHTMLInFields;
}

void TextItem::setAllowHTMLInFields(bool allowHTMLInFields)
{
    if (m_allowHTMLInFields != allowHTMLInFields){
        m_allowHTMLInFields = allowHTMLInFields;
        notify("allowHTMLInFields",!m_allowHTMLInFields,allowHTMLInFields);
        update();
    }
}

bool TextItem::allowHTML() const
{
    return m_allowHTML;
}

void TextItem::setAllowHTML(bool allowHTML)
{
    if (m_allowHTML!=allowHTML){
        m_allowHTML = allowHTML;
//        if (m_text){
//            if (allowHTML)
//                m_text->setHtml(m_strText);
//            else
//                m_text->setPlainText(m_strText);
//            update();
//        }
        update();
        notify("allowHTML",!m_allowHTML,allowHTML);
    }
}
bool TextItem::trimValue() const
{
    return m_trimValue;
}

void TextItem::setTrimValue(bool value)
{
    bool oldValue = m_trimValue;
    m_trimValue = value;
    notify("trimValue",oldValue,value);
}


void TextItem::geometryChangedEvent(QRectF , QRectF)
{}

bool TextItem::isNeedUpdateSize(RenderPass pass) const
{
    Q_UNUSED(pass)
    bool res =  (m_textSize.height()>geometry().height()&&autoHeight()) ||
                (m_textSize.width()>geometry().width()&&autoWidth()) ||
                 m_follower ||
                isNeedExpandContent();
    return res;
}

void TextItem::setAlignment(Qt::Alignment value)
{
    if (m_alignment!=value){
        Qt::Alignment oldValue = m_alignment;
        m_alignment=value;
        //m_layout.setTextOption(QTextOption(m_alignment));
        if (!isLoading()){
            update(rect());
            notify("alignment",QVariant(oldValue),QVariant(value));
        }
    }
}

void TextItem::expandContent(DataSourceManager* dataManager, RenderPass pass)
{
    QString context=content();
    ExpandType expandType = (allowHTML() && !allowHTMLInFields())?ReplaceHTMLSymbols:NoEscapeSymbols;
    switch(pass){
    case FirstPass:
        context=expandUserVariables(context, pass, expandType, dataManager);
        context=expandScripts(context, dataManager);
        context=expandDataFields(context, expandType, dataManager);
        break;
    case SecondPass:;
        context=expandUserVariables(context, pass, expandType, dataManager);
        context=expandScripts(context, dataManager);
    }

    if (expandType == NoEscapeSymbols && !m_varValue.isNull() &&m_valueType!=Default) {
        setContent(formatFieldValue());
    } else {
        setContent(context);
    }

}

void TextItem::setAutoHeight(bool value)
{
    if (m_autoHeight!=value){
        bool oldValue = m_autoHeight;
        m_autoHeight=value;
        notify("autoHeight",oldValue,value);
    }
}

void TextItem::setAutoWidth(TextItem::AutoWidth value)
{
    if (m_autoWidth!=value){
        TextItem::AutoWidth oldValue = m_autoWidth;
        m_autoWidth=value;
        notify("autoWidth",oldValue,value);
    }
}

void TextItem::setAdaptFontToSize(bool value)
{
    if (m_adaptFontToSize!=value){
        bool oldValue = m_adaptFontToSize;
        m_adaptFontToSize=value;
//        initText();
        invalidateRect(rect());
        notify("updateFontToSize",oldValue,value);
    }
}

bool TextItem::canBeSplitted(int height) const
{
    QFontMetrics fm(font());
    return height > m_firstLineSize;
}

QString TextItem::getTextPart(int height, int skipHeight){
    int linesHeight = 0;
    int curLine = 0;
    int textPos = 0;

    TextPtr text = textDocument();

    QTextBlock curBlock = text->begin();
    QString resultText = "";

    if (skipHeight > 0){
        for (;curBlock != text->end(); curBlock=curBlock.next()){
            for (curLine = 0; curLine < curBlock.layout()->lineCount(); curLine++){
                linesHeight += curBlock.layout()->lineAt(curLine).height() + lineSpacing();
                if (linesHeight > (skipHeight-(/*fakeMarginSize()*2+*/borderLineSize() * 2))) {goto loop_exit;}
            }
        }
        loop_exit:;
    }

    linesHeight = 0;

    for (;curBlock != text->end() || curLine<curBlock.lineCount(); curBlock = curBlock.next(), curLine = 0, resultText += '\n'){
        for (; curLine<curBlock.layout()->lineCount(); curLine++){
          if (resultText == "") textPos= curBlock.layout()->lineAt(curLine).textStart();
          linesHeight += curBlock.layout()->lineAt(curLine).height() + lineSpacing();
          if ( (height>0) && (linesHeight>(height-(/*fakeMarginSize()*2+*/borderLineSize()*2))) ) {
              linesHeight-=curBlock.layout()->lineAt(curLine).height();
              goto loop_exit1;
          }
          resultText+=curBlock.text().mid(curBlock.layout()->lineAt(curLine).textStart(),
            curBlock.layout()->lineAt(curLine).textLength());
        }
    }
    loop_exit1:;

    resultText.chop(1);

    QScopedPointer<HtmlContext> context(new HtmlContext(m_strText));
    return context->extendTextByTags(resultText,textPos);
}

void TextItem::restoreLinksEvent()
{
    if (!followTo().isEmpty()){
        BaseDesignIntf* pi = dynamic_cast<BaseDesignIntf*>(parentItem());
        if (pi){
            foreach (BaseDesignIntf* bi, pi->childBaseItems()) {
                if (bi->patternName().compare(followTo())==0){
                    TextItem* ti = dynamic_cast<TextItem*>(bi);
                    if (ti){
                        ti->setFollower(this);
                    }
                }
            }
        }
    }
}

BaseDesignIntf *TextItem::cloneUpperPart(int height, QObject *owner, QGraphicsItem *parent)
{
    TextItem* upperPart = dynamic_cast<TextItem*>(cloneItem(itemMode(),owner,parent));
    upperPart->setContent(getTextPart(height,0));
    upperPart->initTextSizes();
    upperPart->setHeight(upperPart->textSize().height()+borderLineSize()*2);
    return upperPart;
}

BaseDesignIntf *TextItem::cloneBottomPart(int height, QObject *owner, QGraphicsItem *parent)
{
    TextItem* bottomPart = dynamic_cast<TextItem*>(cloneItem(itemMode(),owner,parent));
    bottomPart->setContent(getTextPart(0,height));
    bottomPart->initTextSizes();
    bottomPart->setHeight(bottomPart->textSize().height()+borderLineSize()*2);
    return bottomPart;
}

BaseDesignIntf *TextItem::createSameTypeItem(QObject *owner, QGraphicsItem *parent)
{
    return new TextItem(owner,parent);
}

BaseDesignIntf *TextItem::cloneEmpty(int height, QObject *owner, QGraphicsItem *parent)
{
    TextItem* empty=dynamic_cast<TextItem*>(cloneItem(itemMode(),owner,parent));
    empty->setContent("");
    empty->setHeight(height);
    return empty;
}

void TextItem::objectLoadFinished()
{
    ItemDesignIntf::objectLoadFinished();
    if (itemMode() == DesignMode || !isNeedExpandContent()){
        initTextSizes();
    }
}

void TextItem::setTextItemFont(QFont value)
{
    if (font()!=value){
        QFont oldValue = font();
        setFont(value);
        update();
        notify("font",oldValue,value);
    }
}

QWidget *TextItem::defaultEditor()
{
    QSettings* l_settings = (page()->settings() != 0) ?
                                 page()->settings() :
                                 (page()->reportEditor()!=0) ? page()->reportEditor()->settings() : 0;
    QWidget* editor = new TextItemEditor(this,page(),l_settings);
    editor->setAttribute(Qt::WA_DeleteOnClose);
    return editor;
}

void TextItem::setBackgroundOpacity(int value)
{
    if (opacity()!=value){
        int oldValue = opacity();
        setOpacity(value);
        notify("backgroundOpacity",oldValue,value);
    }
}

void TextItem::setBackgroundModeProperty(BaseDesignIntf::BGMode value)
{
    if (value!=backgroundMode()){
        BaseDesignIntf::BGMode oldValue = backgroundMode();
        setBackgroundMode(value);
        notify("backgroundMode",oldValue,value);
    }
}

void TextItem::setBackgroundColorProperty(QColor value)
{
    if(value!=backgroundColor()){
        QColor oldValue = backgroundColor();
        setBackgroundColor(value);
        notify("backgroundColor",oldValue,value);
    }
}

void TextItem::setFontColorProperty(QColor value)
{
    if(value!=fontColor()){
        QColor oldValue = fontColor();
        setFontColor(value);
        notify("fontColor",oldValue,value);
    }
}

TextItem::AngleType TextItem::angle() const
{
    return m_angle;
}

void TextItem::setAngle(const AngleType& value)
{
    if (m_angle!=value){
        AngleType oldValue = m_angle;
        m_angle = value;
        if (!isLoading()){
            update();
            notify("angle",oldValue,value);
        }
    }
}

void TextItem::setForegroundOpacity(int value)
{
    if (value>100){
        value=100;
    } else if(value<0){
        value=0;
    }
    if (m_foregroundOpacity != value){
        int oldValue = m_foregroundOpacity;
        m_foregroundOpacity = value;
        update();
        notify("foregroundOpacity",oldValue,value);
    }
}

void TextItem::setUnderlines(bool value)
{
    if (m_underlines != value){
        bool oldValue = m_underlines;
        m_underlines = value;
        update();
        notify("underlines",oldValue,value);
    }
}

} //namespace LimeReport



