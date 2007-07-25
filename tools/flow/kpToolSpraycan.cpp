
// COMPAT: This spray is too thick compared to KolourPaint in KDE 3.5.
//       Check commit history to see when it was broken (probably during conversion to flow-based tool).  I swear I tried to fix this at one point already -- Clarence

/*
   Copyright (c) 2003-2007 Clarence Dang <dang@kde.org>
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


#define DEBUG_KP_TOOL_SPRAYCAN 1


#include <cstdlib>

#include <qbitmap.h>
#include <qpainter.h>
#include <qpen.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <qpolygon.h>
#include <qrect.h>
#include <qtimer.h>

#include <kdebug.h>
#include <klocale.h>

#include <kpBug.h>
#include <kpDefs.h>
#include <kpDocument.h>
#include <kpPainter.h>
#include <kpPixmapFX.h>
#include <kpToolEnvironment.h>
#include <kpToolFlowCommand.h>
#include <kpToolSpraycan.h>
#include <kpToolToolBar.h>
#include <kpToolWidgetSpraycanSize.h>
#include <kpView.h>
#include <kpViewManager.h>


kpToolSpraycan::kpToolSpraycan (kpToolEnvironment *environ, QObject *parent)
    : kpToolFlowBase (i18n ("Spraycan"), i18n ("Sprays graffiti"),
        Qt::Key_Y,
        environ, parent, "tool_spraycan")
{
    m_timer = new QTimer (this);
    connect (m_timer, SIGNAL (timeout ()), this, SLOT (timeoutDraw ()));
}

kpToolSpraycan::~kpToolSpraycan ()
{
}


// protected virtual [base kpToolFlowBase]
QString kpToolSpraycan::haventBegunDrawUserMessage () const
{
    return i18n ("Click or drag to spray graffiti.");
}


// public virtual [base kpToolFlowBase]
void kpToolSpraycan::begin ()
{
    kpToolToolBar *tb = toolToolBar ();
    Q_ASSERT (tb);

    m_toolWidgetSpraycanSize = tb->toolWidgetSpraycanSize ();
    connect (m_toolWidgetSpraycanSize, SIGNAL (spraycanSizeChanged (int)),
            this, SLOT (slotSpraycanSizeChanged (int)));
    m_toolWidgetSpraycanSize->show ();

    kpToolFlowBase::begin ();
}

// public virtual [base kpToolFlowBase]
void kpToolSpraycan::end ()
{
    kpToolFlowBase::end ();

    disconnect (m_toolWidgetSpraycanSize, SIGNAL (spraycanSizeChanged (int)),
                this, SLOT (slotSpraycanSizeChanged (int)));
    m_toolWidgetSpraycanSize = 0;
}


// public virtual [base kpToolFlowBase]
void kpToolSpraycan::beginDraw ()
{
#if DEBUG_KP_TOOL_SPRAYCAN
    kDebug () << "kpToolSpraycan::beginDraw()" << endl;
#endif

    kpToolFlowBase::beginDraw ();

    // We draw even if the user doesn't move the mouse.
    m_timer->start (25/*ms*/);
}


// protected
QRect kpToolSpraycan::drawLineWithProbability (const QPoint &thisPoint,
         const QPoint &lastPoint,
         double probability)
{
#if DEBUG_KP_TOOL_SPRAYCAN
    kDebug () << "kpToolSpraycan::drawLine(thisPoint=" << thisPoint
               << ",lastPoint=" << lastPoint
               << ")" << endl;
#endif

    QRect docRect = kpBug::QRect_Normalized (QRect (thisPoint, lastPoint));
    docRect = neededRect (docRect, spraycanSize ());
    QPixmap pixmap = document ()->getImageAt (docRect);


    QList <QPoint> docPoints = kpPainter::interpolatePoints (lastPoint, thisPoint,
        probability);


    // Drawing a line (not just a point) starting at lastPoint?
    // TODO: this code is hard to read.  Need to verify again.
    if (thisPoint != lastPoint &&
        docPoints.size () > 0 && docPoints [0] == lastPoint)
    {
    #if DEBUG_KP_TOOL_SPRAYCAN
        kDebug () << "\tis a line starting at lastPoint - erasing="
                   << docPoints [0] << endl;
    #endif

        // We're not expecting a duplicate 2nd interpolation point.
        Q_ASSERT (docPoints.size () <= 1 || docPoints [1] != lastPoint);

        // lastPoint was drawn previously so don't draw over it again or
        // it will (theoretically) be denser than expected.
        //
        // Unlike other tools such as the Brush, drawing over the same
        // point does result in a different appearance.
        //
        // Having said this, the user probably won't notice either way
        // since spraying on nearby document interpolation points will
        // spray around this document point anyway (due to the
        // spraycanSize() radius).
        // TODO: what if docPoints becomes empty?
        docPoints.erase (docPoints.begin ());
    }

    // By chance no points to draw?
    if (docPoints.empty ())
        return QRect ();


    // Spray at each point, onto the pixmap.
    QList <QPoint> pixmapPoints;
    foreach (QPoint dp, docPoints)
        pixmapPoints.append (dp - docRect.topLeft ());
    kpPainter::sprayPoints (&pixmap,
        pixmapPoints,
        color (mouseButton ()),
        spraycanSize ());


    viewManager ()->setFastUpdates ();
    document ()->setImageAt (pixmap, docRect.topLeft ());
    viewManager ()->restoreFastUpdates ();

    return docRect;
}

// public virtual [base kpToolFlowBase]
QRect kpToolSpraycan::drawPoint (const QPoint &point)
{
#if DEBUG_KP_TOOL_SPRAYCAN
    kDebug () << "kpToolSpraycan::drawPoint" << point
               << " lastPoint=" << lastPoint ()
               << endl;
#endif

    // If this is the first in the flow or if the user is moving the spray,
    // make the spray line continuous.
    if (point != lastPoint ())
    {
        // Draw without delay.
        return drawLineWithProbability (point, point,
            1.0/*100% chance of drawing*/);
    }

    return QRect ();
}

// public virtual [base kpToolFlowBase]
QRect kpToolSpraycan::drawLine (const QPoint &thisPoint, const QPoint &lastPoint)
{
    // Draw only every so often in response to movement.
    return drawLineWithProbability (thisPoint, lastPoint,
        0.1/*less dense: select 10% of adjacent pixels - not all*/);
}

// protected slot
void kpToolSpraycan::timeoutDraw ()
{
#if DEBUG_KP_TOOL_SPRAYCAN
    kDebug () << "kpToolSpraycan::timeoutDraw()" << endl;
#endif

    const QRect drawnRect = drawLineWithProbability (currentPoint (), currentPoint (),
        1.0/*100% chance of drawing*/);

    // kpToolFlowBase() does this after calling drawPoint() and drawLine() so
    // we need to do it too.
    currentCommand ()->updateBoundingRect (drawnRect);
}


// public virtual [base kpToolFlowBase]
void kpToolSpraycan::cancelShape ()
{
#if DEBUG_KP_TOOL_SPRAYCAN
    kDebug () << "kpToolSpraycan::cancelShape()" << endl;
#endif

    m_timer->stop ();
    kpToolFlowBase::cancelShape ();
}

// public virtual [base kpToolFlowBase]
void kpToolSpraycan::endDraw (const QPoint &thisPoint,
    const QRect &normalizedRect)
{
#if DEBUG_KP_TOOL_SPRAYCAN
    kDebug () << "kpToolSpraycan::endDraw(thisPoint=" << thisPoint
               << ")" << endl;
#endif

    m_timer->stop ();
    kpToolFlowBase::endDraw (thisPoint, normalizedRect);
}


// protected
int kpToolSpraycan::spraycanSize () const
{
    return m_toolWidgetSpraycanSize->spraycanSize ();
}

// protected slot
void kpToolSpraycan::slotSpraycanSizeChanged (int size)
{
    (void) size;
}


#include <kpToolSpraycan.moc>

