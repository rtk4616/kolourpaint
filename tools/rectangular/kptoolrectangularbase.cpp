
/*
   Copyright (c) 2003-2006 Clarence Dang <dang@kde.org>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#define DEBUG_KP_TOOL_RECTANGULAR_BASE 0


#include <kptoolrectangularbase.h>

#include <qbitmap.h>
#include <qcursor.h>
#include <qevent.h>
#include <qpainter.h>
#include <qpixmap.h>

#include <kdebug.h>
#include <klocale.h>

#include <kpbug.h>
#include <kpcolor.h>
#include <kpcommandhistory.h>
#include <kpdefs.h>
#include <kpdocument.h>
#include <kpmainwindow.h>
#include <kppainter.h>
#include <kppixmapfx.h>
#include <kptemppixmap.h>
#include <kptoolrectangularcommand.h>
#include <kptooltoolbar.h>
#include <kptoolwidgetfillstyle.h>
#include <kptoolwidgetlinewidth.h>
#include <kpview.h>
#include <kpviewmanager.h>


struct kpToolRectangularBasePrivate
{
    kpToolRectangularBase::DrawShapeFunc drawShapeFunc;
    
    kpToolWidgetLineWidth *toolWidgetLineWidth;
    kpToolWidgetFillStyle *toolWidgetFillStyle;

    QRect toolRectangleRect;
};


kpToolRectangularBase::kpToolRectangularBase (const QString &text, const QString &description,
        DrawShapeFunc drawShapeFunc,
        int key,
        kpMainWindow *mainWindow, const QString &name)
        
    : kpTool (text, description, key, mainWindow, name),
      d (new kpToolRectangularBasePrivate ())
{
    d->drawShapeFunc = drawShapeFunc;
    
    d->toolWidgetLineWidth = 0, d->toolWidgetFillStyle = 0;
}


kpToolRectangularBase::~kpToolRectangularBase ()
{
    delete d;
}


// private slot virtual
void kpToolRectangularBase::slotLineWidthChanged ()
{
    if (hasBegunDraw ())
        updateShape ();
}

// private slot virtual
void kpToolRectangularBase::slotFillStyleChanged ()
{
    if (hasBegunDraw ())
        updateShape ();
}


// private
QString kpToolRectangularBase::haventBegunDrawUserMessage () const
{
    return i18n ("Drag to draw.");
}

// virtual
void kpToolRectangularBase::begin ()
{
#if DEBUG_KP_TOOL_RECTANGULAR_BASE
    kDebug () << "kpToolRectangularBase::begin ()" << endl;
#endif

    kpToolToolBar *tb = toolToolBar ();
    Q_ASSERT (tb);

#if DEBUG_KP_TOOL_RECTANGULAR_BASE
    kDebug () << "\ttoolToolBar=" << tb << endl;
#endif

    d->toolWidgetLineWidth = tb->toolWidgetLineWidth ();
    connect (d->toolWidgetLineWidth,
        SIGNAL (lineWidthChanged (int)),
        this,
        SLOT (slotLineWidthChanged ()));
    d->toolWidgetLineWidth->show ();

    d->toolWidgetFillStyle = tb->toolWidgetFillStyle ();
    connect (d->toolWidgetFillStyle,
        SIGNAL (fillStyleChanged (kpToolWidgetFillStyle::FillStyle)),
        this,
        SLOT (slotFillStyleChanged ()));
    d->toolWidgetFillStyle->show ();

    viewManager ()->setCursor (QCursor (Qt::CrossCursor));

    setUserMessage (haventBegunDrawUserMessage ());
}

// virtual
void kpToolRectangularBase::end ()
{
#if DEBUG_KP_TOOL_RECTANGULAR_BASE
    kDebug () << "kpToolRectangularBase::end ()" << endl;
#endif

    if (d->toolWidgetLineWidth)
    {
        disconnect (d->toolWidgetLineWidth,
            SIGNAL (lineWidthChanged (int)),
            this,
            SLOT (slotLineWidthChanged ()));
        d->toolWidgetLineWidth = 0;
    }

    if (d->toolWidgetFillStyle)
    {
        disconnect (d->toolWidgetFillStyle,
            SIGNAL (fillStyleChanged (kpToolWidgetFillStyle::FillStyle)),
            this,
            SLOT (slotFillStyleChanged ()));
        d->toolWidgetFillStyle = 0;
    }

    viewManager ()->unsetCursor ();
}

void kpToolRectangularBase::applyModifiers ()
{
    QRect rect = kpBug::QRect_Normalized (QRect (m_startPoint, m_currentPoint));

#if DEBUG_KP_TOOL_RECTANGULAR_BASE
    kDebug () << "kpToolRectangularBase::applyModifiers(" << rect
               << ") shift=" << m_shiftPressed
               << " ctrl=" << m_controlPressed
               << endl;
#endif

    // user wants to m_startPoint == center
    if (m_controlPressed)
    {
        int xdiff = qAbs (m_startPoint.x () - m_currentPoint.x ());
        int ydiff = qAbs (m_startPoint.y () - m_currentPoint.y ());
        rect = QRect (m_startPoint.x () - xdiff, m_startPoint.y () - ydiff,
                      xdiff * 2 + 1, ydiff * 2 + 1);
    }

    // user wants major axis == minor axis:
    //   rectangle --> square
    //   rounded rectangle --> rounded square
    //   ellipse --> circle
    if (m_shiftPressed)
    {
        if (!m_controlPressed)
        {
            if (rect.width () < rect.height ())
            {
                if (m_startPoint.y () == rect.y ())
                    rect.setHeight (rect.width ());
                else
                    rect.setY (rect.bottom () - rect.width () + 1);
            }
            else
            {
                if (m_startPoint.x () == rect.x ())
                    rect.setWidth (rect.height ());
                else
                    rect.setX (rect.right () - rect.height () + 1);
            }
        }
        // have to maintain the center
        else
        {
            if (rect.width () < rect.height ())
            {
                QPoint center = rect.center ();
                rect.setHeight (rect.width ());
                rect.moveCenter (center);
            }
            else
            {
                QPoint center = rect.center ();
                rect.setWidth (rect.height ());
                rect.moveCenter (center);
            }
        }
    }

    d->toolRectangleRect = rect;
}

void kpToolRectangularBase::beginDraw ()
{
    setUserMessage (cancelUserMessage ());
}


// private
kpColor kpToolRectangularBase::drawingForegroundColor () const
{
    return color (m_mouseButton);
}

// private
kpColor kpToolRectangularBase::drawingBackgroundColor () const
{
    const kpColor foregroundColor = color (m_mouseButton);
    const kpColor backgroundColor = color (1 - m_mouseButton);

    return d->toolWidgetFillStyle->drawingBackgroundColor (
        foregroundColor, backgroundColor);
}

// private
void kpToolRectangularBase::updateShape ()
{
    viewManager ()->setFastUpdates ();

    kpImage image = document ()->getPixmapAt (d->toolRectangleRect);

    // Invoke shape drawing function passed in ctor.
    (*d->drawShapeFunc) (&image,
        0, 0, d->toolRectangleRect.width (), d->toolRectangleRect.height (),
        drawingForegroundColor (), d->toolWidgetLineWidth->lineWidth (),
        drawingBackgroundColor ());
    kpTempPixmap newTempPixmap (false/*always display*/,
                                kpTempPixmap::SetPixmap/*render mode*/,
                                d->toolRectangleRect.topLeft (),
                                image);
    viewManager ()->setTempPixmap (newTempPixmap);

    viewManager ()->restoreFastUpdates ();
}


void kpToolRectangularBase::draw (const QPoint &, const QPoint &, const QRect &)
{
    applyModifiers ();


    updateShape ();


    // Recover the start and end points from the transformed & normalized d->toolRectangleRect

    // S. or S or SC or S == C
    // .C    C
    if (m_currentPoint.x () >= m_startPoint.x () &&
        m_currentPoint.y () >= m_startPoint.y ())
    {
        setUserShapePoints (d->toolRectangleRect.topLeft (),
                            d->toolRectangleRect.bottomRight ());
    }
    // .C or C
    // S.    S
    else if (m_currentPoint.x () >= m_startPoint.x () &&
             m_currentPoint.y () < m_startPoint.y ())
    {
        setUserShapePoints (d->toolRectangleRect.bottomLeft (),
                            d->toolRectangleRect.topRight ());
    }
    // .S or CS
    // C.
    else if (m_currentPoint.x () < m_startPoint.x () &&
             m_currentPoint.y () >= m_startPoint.y ())
    {
        setUserShapePoints (d->toolRectangleRect.topRight (),
                            d->toolRectangleRect.bottomLeft ());
    }
    // C.
    // .S
    else
    {
        setUserShapePoints (d->toolRectangleRect.bottomRight (),
                            d->toolRectangleRect.topLeft ());
    }
}

void kpToolRectangularBase::cancelShape ()
{
    viewManager ()->invalidateTempPixmap ();

    setUserMessage (i18n ("Let go of all the mouse buttons."));
}

void kpToolRectangularBase::releasedAllButtons ()
{
    setUserMessage (haventBegunDrawUserMessage ());
}

void kpToolRectangularBase::endDraw (const QPoint &, const QRect &)
{
    applyModifiers ();

    // TODO: flicker
    viewManager ()->invalidateTempPixmap ();

    mainWindow ()->commandHistory ()->addCommand (
        new kpToolRectangularCommand (text (),
            d->drawShapeFunc, d->toolRectangleRect,
            drawingForegroundColor (), d->toolWidgetLineWidth->lineWidth (),
            drawingBackgroundColor (),
            mainWindow ()));

    setUserMessage (haventBegunDrawUserMessage ());
}


#include <kptoolrectangularbase.moc>