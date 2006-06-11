
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


#define DEBUG_KP_TOOL_POLYGON 0


#include <kptoolpolygon.h>

#include <float.h>
#include <math.h>

#include <qbitmap.h>
#include <qcursor.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <qpolygon.h>
#include <qpushbutton.h>
#include <qrect.h>
#include <qtooltip.h>

#include <kdebug.h>
#include <klocale.h>

#include <kpbug.h>
#include <kpcommandhistory.h>
#include <kpdocument.h>
#include <kpdefs.h>
#include <kpimage.h>
#include <kpmainwindow.h>
#include <kppainter.h>
#include <kppixmapfx.h>
#include <kptemppixmap.h>
#include <kptooltoolbar.h>
#include <kptoolwidgetlinewidth.h>
#include <kpviewmanager.h>


static kpImage image (const QPixmap &oldImage,
        const QPolygon &points, const QRect &rect,
        const kpColor &foregroundColor, int penWidth,
        kpColor backgroundColor,
        enum kpToolPolygon::Mode mode, bool final = true)
{
#if DEBUG_KP_TOOL_POLYGON
    kDebug () << "kptoolpolygon.cpp: image(): points=" << points.toList () << endl;
#endif

    kpImage image = oldImage;
    
    // Figure out points to draw relative to topLeft of oldPixmap.
    QPolygon pointsInRect = points;
    pointsInRect.detach ();
    pointsInRect.translate (-rect.x (), -rect.y ());

    switch (mode)
    {
    case kpToolPolygon::Line:
    case kpToolPolygon::Polyline:
        kpPainter::drawPolyline (&image,
            pointsInRect,
            foregroundColor, penWidth);
        break;

    case kpToolPolygon::Polygon:
        kpPainter::drawPolygon (&image,
            pointsInRect,
            foregroundColor, penWidth,
            backgroundColor,
            final);
        break;

    case kpToolPolygon::Curve:
    {
        const QPoint startPoint = pointsInRect [0];
        const QPoint endPoint = pointsInRect [1];

        QPoint controlPointP, controlPointQ;
        
        switch (pointsInRect.count ())
        {
        // Just a line?
        case 2:
            controlPointP = startPoint;
            controlPointQ = endPoint;
            break;

        // Single control point?
        case 3:
            controlPointP = controlPointQ = pointsInRect [2];
            break;

        // Two control points?
        case 4:
            controlPointP = pointsInRect [2];
            controlPointQ = pointsInRect [3];
            break;

        default:
            kError () << "kptoolpolygon.cpp:image() pointsInRect.count="
                << pointsInRect.count () << endl;
            break;
        }

        kpPainter::drawCurve (&image,
            startPoint,
            controlPointP, controlPointQ,
            endPoint,
            foregroundColor, penWidth);
    }}

    return image;
}


//
// kpToolPolygon
//

struct kpToolPolygonPrivate
{
    kpToolPolygonPrivate ()
        : toolWidgetFillStyle (0),
          toolWidgetLineWidth (0)
    {
    }
    
    kpToolPolygon::Mode mode;

    kpToolWidgetFillStyle *toolWidgetFillStyle;
    kpToolWidgetLineWidth *toolWidgetLineWidth;

    int originatingMouseButton;

    QPoint toolLineStartPoint, toolLineEndPoint;
    QRect toolLineRect;

    QPolygon points;
};

kpToolPolygon::kpToolPolygon (Mode mode,
                              const QString &text, const QString &description,
                              int key,
                              kpMainWindow *mainWindow, const char *name)
    : kpTool (text, description, key, mainWindow, name),
      d (new kpToolPolygonPrivate ())
{
    d->mode = mode;
}

kpToolPolygon::kpToolPolygon (kpMainWindow *mainWindow)
    : kpTool (i18n ("Polygon"), i18n ("Draws polygons"),
              Qt::Key_G,
              mainWindow, "tool_polygon"),
      d (new kpToolPolygonPrivate ())
{
    d->mode = Polygon;
}

kpToolPolygon::~kpToolPolygon ()
{
    delete d;
}

void kpToolPolygon::setMode (Mode m)
{
    d->mode = m;
}


// private
QString kpToolPolygon::haventBegunShapeUserMessage () const
{
    switch (d->mode)
    {
    case Line:
        return i18n ("Drag to draw.");
    case Polygon:
    case Polyline:
        return i18n ("Drag to draw the first line.");
    case Curve:
        return i18n ("Drag out the start and end points.");
    default:
        return QString::null;
    }
}

// virtual
void kpToolPolygon::begin ()
{
    kpToolToolBar *tb = toolToolBar ();
    Q_ASSERT (tb);

#if DEBUG_KP_TOOL_POLYGON
    kDebug () << "kpToolPolygon::begin() tb=" << tb << endl;
#endif

    if (d->mode == Polygon)
    {
        d->toolWidgetFillStyle = tb->toolWidgetFillStyle ();
        connect (d->toolWidgetFillStyle,
            SIGNAL (fillStyleChanged (kpToolWidgetFillStyle::FillStyle)),
            this,
            SLOT (slotFillStyleChanged ()));
        d->toolWidgetFillStyle->show ();
    }
    else
        d->toolWidgetFillStyle = 0;

    d->toolWidgetLineWidth = tb->toolWidgetLineWidth ();
    connect (d->toolWidgetLineWidth, SIGNAL (lineWidthChanged (int)),
                this, SLOT (slotLineWidthChanged ()));
    d->toolWidgetLineWidth->show ();

    viewManager ()->setCursor (QCursor (Qt::CrossCursor));

    d->originatingMouseButton = -1;

    setUserMessage (haventBegunShapeUserMessage ());
}

// virtual
void kpToolPolygon::end ()
{
    endShape ();

    if (d->toolWidgetFillStyle)
    {
        disconnect (d->toolWidgetFillStyle,
            SIGNAL (fillStyleChanged (kpToolWidgetFillStyle::FillStyle)),
            this,
            SLOT (slotFillStyleChanged ()));
        d->toolWidgetFillStyle = 0;
    }

    if (d->toolWidgetLineWidth)
    {
        disconnect (d->toolWidgetLineWidth,
            SIGNAL (lineWidthChanged (int)),
            this,
            SLOT (slotLineWidthChanged ()));
        d->toolWidgetLineWidth = 0;
    }

    viewManager ()->unsetCursor ();
}


void kpToolPolygon::beginDraw ()
{
#if DEBUG_KP_TOOL_POLYGON
    kDebug () << "kpToolPolygon::beginDraw()  d->points=" << d->points.toList ()
               << ", startPoint=" << m_startPoint << endl;
#endif

    bool endedShape = false;

    // starting with a line...
    if (d->points.count () == 0)
    {
        d->originatingMouseButton = m_mouseButton;
        d->points.putPoints (d->points.count (), 2,
                            m_startPoint.x (), m_startPoint.y (),
                            m_startPoint.x (), m_startPoint.y ());
    }
    // continuing poly*
    else
    {
        if (m_mouseButton != d->originatingMouseButton)
        {
            m_mouseButton = d->originatingMouseButton;
            endShape ();
            endedShape = true;
        }
        else
        {
            int count = d->points.count ();
            d->points.putPoints (count, 1,
                                m_startPoint.x (), m_startPoint.y ());

            // start point = last end point;
            // _not_ the new/current start point
            // (which is disregarded in a poly* as only the end points count
            //  after the initial line)
            //
            // Curve Tool ignores m_startPoint (doesn't call applyModifiers())
            // after the initial has been defined.
            m_startPoint = d->points [count - 1];
        }
    }

#if DEBUG_KP_TOOL_POLYGON
    kDebug () << "\tafterwards, d->points=" << d->points.toList () << endl;
#endif

    if (!endedShape)
    {
        switch (d->mode)
        {
        case Line:
        case Curve:
        case Polygon:
        case Polyline:
            setUserMessage (cancelUserMessage ());
            break;

        default:
            kError () << "kpToolPolygon::beginDraw() shape" << endl;
            break;
        }
    }
}

// private
void kpToolPolygon::applyModifiers ()
{
    int count = d->points.count ();

    d->toolLineStartPoint = m_startPoint;  /* also correct for poly* tool (see beginDraw()) */
    d->toolLineEndPoint = m_currentPoint;

#if DEBUG_KP_TOOL_POLYGON && 1
    kDebug () << "kpToolPolygon::applyModifiers() #pts=" << count
               << "   line: startPt=" << d->toolLineStartPoint
               << " endPt=" << d->toolLineEndPoint
               << "   modifiers: shift=" << m_shiftPressed
               << "   alt=" << m_altPressed
               << "   ctrl=" << m_controlPressed
               << endl;
#endif

    // angles
    if (m_shiftPressed || m_controlPressed)
    {
        int diffx = d->toolLineEndPoint.x () - d->toolLineStartPoint.x ();
        int diffy = d->toolLineEndPoint.y () - d->toolLineStartPoint.y ();

        double ratio;
        if (diffx == 0)
            ratio = DBL_MAX;
        else
            ratio = fabs (double (diffy) / double (diffx));
    #if DEBUG_KP_TOOL_POLYGON && 1
        kDebug () << "\tdiffx=" << diffx << " diffy=" << diffy
                   << " ratio=" << ratio
                   << endl;
    #endif

        // Shift        = 0, 45, 90
        // Alt          = 0, 30, 60, 90
        // Shift + Alt  = 0, 30, 45, 60, 90
        double angles [10];  // "ought to be enough for anybody"
        int numAngles = 0;
        angles [numAngles++] = 0;
        if (m_controlPressed)
            angles [numAngles++] = KP_PI / 6;
        if (m_shiftPressed)
            angles [numAngles++] = KP_PI / 4;
        if (m_controlPressed)
            angles [numAngles++] = KP_PI / 3;
        angles [numAngles++] = KP_PI / 2;

        double angle = angles [numAngles - 1];
        for (int i = 0; i < numAngles - 1; i++)
        {
            double acceptingRatio = tan ((angles [i] + angles [i + 1]) / 2.0);
            if (ratio < acceptingRatio)
            {
                angle = angles [i];
                break;
            }
        }

        // horizontal (dist from start not maintained)
        if (fabs (KP_RADIANS_TO_DEGREES (angle) - 0)
            < kpPixmapFX::AngleInDegreesEpsilon)
        {
            d->toolLineEndPoint =
                QPoint (d->toolLineEndPoint.x (), d->toolLineStartPoint.y ());
        }
        // vertical (dist from start not maintained)
        else if (fabs (KP_RADIANS_TO_DEGREES (angle) - 90)
                 < kpPixmapFX::AngleInDegreesEpsilon)
        {
            d->toolLineEndPoint =
                QPoint (d->toolLineStartPoint.x (), d->toolLineEndPoint.y ());
        }
        // diagonal (dist from start maintained)
        else
        {
            const double dist = sqrt (diffx * diffx + diffy * diffy);

            #define sgn(a) ((a)<0?-1:1)
            // Round distances _before_ adding to any coordinate
            // (ensures consistent rounding behaviour in x & y directions)
            const int newdx = qRound (dist * cos (angle) * sgn (diffx));
            const int newdy = qRound (dist * sin (angle) * sgn (diffy));
            #undef sgn

            d->toolLineEndPoint = QPoint (d->toolLineStartPoint.x () + newdx,
                                         d->toolLineStartPoint.y () + newdy);

        #if DEBUG_KP_TOOL_POLYGON && 1
            kDebug () << "\t\tdiagonal line: dist=" << dist
                       << " angle=" << (angle * 180 / KP_PI)
                       << " endPoint=" << d->toolLineEndPoint
                       << endl;
        #endif
        }
    }    // if (m_shiftPressed || m_controlPressed) {

    // centring
    if (m_altPressed && 0/*ALT is unreliable*/)
    {
        // start = start - diff
        //       = start - (end - start)
        //       = start - end + start
        //       = 2 * start - end
        if (count == 2)
            d->toolLineStartPoint += (d->toolLineStartPoint - d->toolLineEndPoint);
        else
            d->toolLineEndPoint += (d->toolLineEndPoint - d->toolLineStartPoint);
    }    // if (m_altPressed) {

    d->points [count - 2] = d->toolLineStartPoint;
    d->points [count - 1] = d->toolLineEndPoint;

    d->toolLineRect = kpTool::neededRect (
        kpBug::QRect_Normalized (
            QRect (d->toolLineStartPoint, d->toolLineEndPoint)),
                   d->toolWidgetLineWidth->lineWidth ());
}

// virtual
void kpToolPolygon::draw (const QPoint &, const QPoint &, const QRect &)
{
    if (d->points.count () == 0)
        return;

#if DEBUG_KP_TOOL_POLYGON
    kDebug () << "kpToolPolygon::draw()  d->points=" << d->points.toList ()
               << ", endPoint=" << m_currentPoint << endl;
#endif

    bool drawingALine = (d->mode != Curve) ||
                        (d->mode == Curve && d->points.count () == 2);

    if (drawingALine)
        applyModifiers ();
    else
        d->points [d->points.count () - 1] = m_currentPoint;

#if DEBUG_KP_TOOL_POLYGON
    kDebug () << "\tafterwards, d->points=" << d->points.toList () << endl;
#endif

    updateShape ();

    if (drawingALine)
        setUserShapePoints (d->toolLineStartPoint, d->toolLineEndPoint);
    else
        setUserShapePoints (m_currentPoint);
}


// TODO: code dup with kpToolRectangle
// private
kpColor kpToolPolygon::drawingForegroundColor () const
{
    return color (m_mouseButton);
}

// private
kpColor kpToolPolygon::drawingBackgroundColor () const
{
    if (!d->toolWidgetFillStyle)
        return kpColor::invalid;
        
    const kpColor foregroundColor = color (m_mouseButton);
    const kpColor backgroundColor = color (1 - m_mouseButton);

    return d->toolWidgetFillStyle->drawingBackgroundColor (
        foregroundColor, backgroundColor);
}

// private
void kpToolPolygon::updateShape ()
{
    if (d->points.count () == 0)
        return;

    QRect boundingRect = kpTool::neededRect (d->points.boundingRect (),
        d->toolWidgetLineWidth->lineWidth ());

#if DEBUG_KP_TOOL_POLYGON
    kDebug () << "kpToolPolygon::updateShape() boundingRect="
               << boundingRect
               << " lineWidth="
               << d->toolWidgetLineWidth->lineWidth ()
               << endl;
#endif

    QPixmap oldPixmap = document ()->getPixmapAt (boundingRect);
    QPixmap newPixmap = ::image (oldPixmap,
        d->points, boundingRect,
        drawingForegroundColor (), d->toolWidgetLineWidth->lineWidth (),
        drawingBackgroundColor (),
        d->mode, false/*not final*/);

    viewManager ()->setFastUpdates ();
    viewManager ()->setTempPixmap (kpTempPixmap (false/*always display*/,
                                                 kpTempPixmap::SetPixmap/*render mode*/,
                                                 boundingRect.topLeft (),
                                                 newPixmap));
    viewManager ()->restoreFastUpdates ();
}

// virtual
void kpToolPolygon::cancelShape ()
{
    viewManager ()->invalidateTempPixmap ();
    d->points.resize (0);

    setUserMessage (i18n ("Let go of all the mouse buttons."));
}

void kpToolPolygon::releasedAllButtons ()
{
    if (!hasBegunShape ())
        setUserMessage (haventBegunShapeUserMessage ());

    // --- else case already handled by endDraw() ---
}

// virtual
void kpToolPolygon::endDraw (const QPoint &, const QRect &)
{
#if DEBUG_KP_TOOL_POLYGON
    kDebug () << "kpToolPolygon::endDraw()  d->points="
        << d->points.toList () << endl;
#endif

    if (d->points.count () == 0)
        return;

    if (d->mode == Line ||
        (d->mode == Curve && d->points.count () >= 4) ||
        d->points.count () >= 50)
    {
        endShape ();
    }
    else
    {
        switch (d->mode)
        {
        case Line:
            kError () << "kpToolPolygon::endDraw() - line not ended" << endl;
            setUserMessage ();
            break;

        case Polygon:
        case Polyline:
            if (d->points.isEmpty ())
            {
                kError () << "kpToolPolygon::endDraw() exception - poly without points" << endl;
                setUserMessage ();
            }
            else
            {
                if (m_mouseButton == 0)
                {
                    setUserMessage (i18n ("Left drag another line or right click to finish."));
                }
                else
                {
                    setUserMessage (i18n ("Right drag another line or left click to finish."));
                }
            }

            break;

        case Curve:
            if (d->points.size () == 2)
            {
                if (m_mouseButton == 0)
                {
                    setUserMessage (i18n ("Left drag to set the first control point or right click to finish."));
                }
                else
                {
                    setUserMessage (i18n ("Right drag to set the first control point or left click to finish."));
                }
            }
            else if (d->points.size () == 3)
            {
                if (m_mouseButton == 0)
                {
                    setUserMessage (i18n ("Left drag to set the last control point or right click to finish."));
                }
                else
                {
                    setUserMessage (i18n ("Right drag to set the last control point or left click to finish."));
                }
            }
            else
            {
                kError () << "kpToolPolygon::endDraw() exception - points" << endl;
                setUserMessage ();
            }

            break;

        default:
            kError () << "kpToolPolygon::endDraw() - clueless" << endl;
            setUserMessage ();
            break;
        }
    }
}

// public virtual
void kpToolPolygon::endShape (const QPoint &, const QRect &)
{
#if DEBUG_KP_TOOL_POLYGON
    kDebug () << "kpToolPolygon::endShape()  d->points="
        << d->points.toList () << endl;
#endif

    if (!hasBegunShape ())
        return;

    viewManager ()->invalidateTempPixmap ();

    QRect boundingRect = kpTool::neededRect (d->points.boundingRect (), d->toolWidgetLineWidth->lineWidth ());

    kpToolPolygonCommand *lineCommand =
        new kpToolPolygonCommand (
            text (),
            d->points, boundingRect,
            drawingForegroundColor (), d->toolWidgetLineWidth->lineWidth (),
            drawingBackgroundColor (),
            document ()->getPixmapAt (boundingRect),
            d->mode,
            mainWindow ());

    commandHistory ()->addCommand (lineCommand);

    d->points.resize (0);
    setUserMessage (haventBegunShapeUserMessage ());

}

// public virtual
bool kpToolPolygon::hasBegunShape () const
{
    return (d->points.count () > 0);
}


// public slot
void kpToolPolygon::slotLineWidthChanged ()
{
    updateShape ();
}

// public slot
void kpToolPolygon::slotFillStyleChanged ()
{
    updateShape ();
}

// virtual protected slot
void kpToolPolygon::slotForegroundColorChanged (const kpColor &)
{
    updateShape ();
}

// virtual protected slot
void kpToolPolygon::slotBackgroundColorChanged (const kpColor &)
{
    updateShape ();
}


//
// kpToolPolygonCommand
//

struct kpToolPolygonCommandPrivate
{
    QPolygon points;
    QRect normalizedRect;

    kpColor foregroundColor, backgroundColor;
    int penWidth;

    QPixmap originalArea;
    kpToolPolygon::Mode mode;
};

kpToolPolygonCommand::kpToolPolygonCommand (const QString &name,
        const QPolygon &points,
        const QRect &normalizedRect,
        const kpColor &foregroundColor, int penWidth,
        const kpColor &backgroundColor,
        const QPixmap &originalArea,
        enum kpToolPolygon::Mode mode,
        kpMainWindow *mainWindow)
    : kpNamedCommand (name, mainWindow),
      d (new kpToolPolygonCommandPrivate ())
{
      d->points = points;
      d->normalizedRect = normalizedRect;
      d->foregroundColor = foregroundColor; d->backgroundColor = backgroundColor;
      d->penWidth = penWidth;
      d->originalArea = originalArea;
      d->mode = mode;
}

kpToolPolygonCommand::~kpToolPolygonCommand ()
{
    delete d;
}


// public virtual [base kpCommand]
int kpToolPolygonCommand::size () const
{
    return kpPixmapFX::pointArraySize (d->points) +
           kpPixmapFX::pixmapSize (d->originalArea);
}


// public virtual [base kpCommand]
void kpToolPolygonCommand::execute ()
{
    QPixmap p = ::image (d->originalArea,
                        d->points, d->normalizedRect,
                        d->foregroundColor, d->penWidth,
                        d->backgroundColor,
                        d->mode);
    document ()->setPixmapAt (p, d->normalizedRect.topLeft ());
}

// public virtual [base kpCommand]
void kpToolPolygonCommand::unexecute ()
{
    document ()->setPixmapAt (d->originalArea, d->normalizedRect.topLeft ());
}


#include <kptoolpolygon.moc>
