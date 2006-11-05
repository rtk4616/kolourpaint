
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


#define DEBUG_KP_TOOL_SELECTION 0


#include <kptoolselection.h>

#include <qapplication.h>
#include <qbitmap.h>
#include <qcursor.h>
#include <qevent.h>
#include <qmenu.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qpolygon.h>
#include <qtimer.h>

#include <kdebug.h>
#include <klocale.h>

#include <kpbug.h>
#include <kpcommandhistory.h>
#include <kpdefs.h>
#include <kpdocument.h>
#include <kpmainwindow.h>
#include <kpselection.h>
#include <kptooltoolbar.h>
#include <kptoolwidgetopaqueortransparent.h>
#include <kpview.h>
#include <kpviewmanager.h>


kpToolSelection::kpToolSelection (Mode mode,
                                  const QString &text,
                                  const QString &description,
                                  int key,
                                  kpMainWindow *mainWindow,
                                  const QString &name)
    : kpTool (text, description, key, mainWindow, name),
      m_mode (mode),
      m_currentPullFromDocumentCommand (0),
      m_currentMoveCommand (0),
      m_currentResizeScaleCommand (0),
      m_toolWidgetOpaqueOrTransparent (0),
      m_currentCreateTextCommand (0),
      m_createNOPTimer (new QTimer (this)),
      m_RMBMoveUpdateGUITimer (new QTimer (this))
{
    m_createNOPTimer->setSingleShot (true);
    connect (m_createNOPTimer, SIGNAL (timeout ()),
             this, SLOT (delayedDraw ()));

    m_RMBMoveUpdateGUITimer->setSingleShot (true);
    connect (m_RMBMoveUpdateGUITimer, SIGNAL (timeout ()),
             this, SLOT (slotRMBMoveUpdateGUI ()));
}

kpToolSelection::~kpToolSelection ()
{
}


// private
void kpToolSelection::pushOntoDocument ()
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelection::pushOntoDocument() CALLED" << endl;
#endif
    mainWindow ()->slotDeselect ();
}


// protected
bool kpToolSelection::onSelectionToMove () const
{
    kpView *v = viewManager ()->viewUnderCursor ();
    if (!v)
        return 0;

    return v->mouseOnSelectionToMove (m_currentViewPoint);
}

// protected
int kpToolSelection::onSelectionResizeHandle () const
{
    kpView *v = viewManager ()->viewUnderCursor ();
    if (!v)
        return 0;

    return v->mouseOnSelectionResizeHandle (m_currentViewPoint);
}

// protected
bool kpToolSelection::onSelectionToSelectText () const
{
    kpView *v = viewManager ()->viewUnderCursor ();
    if (!v)
        return 0;

    return v->mouseOnSelectionToSelectText (m_currentViewPoint);
}


// public
QString kpToolSelection::haventBegunDrawUserMessage () const
{
#if DEBUG_KP_TOOL_SELECTION && 0
    kDebug () << "kpToolSelection::haventBegunDrawUserMessage()"
                  " cancelledShapeButStillHoldingButtons="
               << m_cancelledShapeButStillHoldingButtons
               << endl;
#endif

    if (m_cancelledShapeButStillHoldingButtons)
        return i18n ("Let go of all the mouse buttons.");

    kpSelection *sel = document ()->selection ();
    if (sel && onSelectionResizeHandle () && !controlOrShiftPressed ())
    {
        if (m_mode == Text)
            return i18n ("Left drag to resize text box.");
        else
            return i18n ("Left drag to scale selection.");
    }
    else if (sel && sel->contains (m_currentPoint))
    {
        if (m_mode == Text)
        {
            if (onSelectionToSelectText () && !controlOrShiftPressed ())
                return i18n ("Left click to change cursor position.");
            else
                return i18n ("Left drag to move text box.");
        }
        else
        {
            return i18n ("Left drag to move selection.");
        }
    }
    else
    {
        if (m_mode == Text)
            return i18n ("Left drag to create text box.");
        else
            return i18n ("Left drag to create selection.");
    }
}


// virtual
void kpToolSelection::begin ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelection::begin()" << endl;
#endif

    kpToolToolBar *tb = toolToolBar ();

    if (tb)
    {
        m_toolWidgetOpaqueOrTransparent = tb->toolWidgetOpaqueOrTransparent ();

        if (m_toolWidgetOpaqueOrTransparent)
        {
            connect (m_toolWidgetOpaqueOrTransparent, SIGNAL (isOpaqueChanged (bool)),
                     this, SLOT (slotIsOpaqueChanged ()));
            m_toolWidgetOpaqueOrTransparent->show ();
        }
    }
    else
    {
        m_toolWidgetOpaqueOrTransparent = 0;
    }

    viewManager ()->setQueueUpdates ();
    {
        viewManager ()->setSelectionBorderVisible (true);
        viewManager ()->setSelectionBorderFinished (true);
    }
    viewManager ()->restoreQueueUpdates ();

    m_startDragFromSelectionTopLeft = QPoint ();
    m_dragType = Unknown;
    m_dragHasBegun = false;
    m_hadSelectionBeforeDrag = false;  // arbitrary
    m_resizeScaleType = 0;

    m_currentPullFromDocumentCommand = 0;
    m_currentMoveCommand = 0;
    m_currentResizeScaleCommand = 0;
    m_currentCreateTextCommand = 0;

    m_cancelledShapeButStillHoldingButtons = false;

    setUserMessage (haventBegunDrawUserMessage ());
}

// virtual
void kpToolSelection::end ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelection::end()" << endl;
#endif

    if (document ()->selection ())
        pushOntoDocument ();

    if (m_toolWidgetOpaqueOrTransparent)
    {
        disconnect (m_toolWidgetOpaqueOrTransparent, SIGNAL (isOpaqueChanged (bool)),
                    this, SLOT (slotIsOpaqueChanged ()));
        m_toolWidgetOpaqueOrTransparent = 0;
    }

    viewManager ()->unsetCursor ();
}

// virtual
void kpToolSelection::reselect ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelection::reselect()" << endl;
#endif

    if (document ()->selection ())
        pushOntoDocument ();
}


// virtual
void kpToolSelection::beginDraw ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelection::beginDraw() m_startPoint="
               << m_startPoint
               << " QCursor::pos() view startPoint="
               << m_viewUnderStartPoint->mapFromGlobal (QCursor::pos ())
               << endl;
#endif

    m_createNOPTimer->stop ();
    m_RMBMoveUpdateGUITimer->stop ();


    // In case the cursor was wrong to start with
    // (forgot to call kpTool::somethingBelowTheCursorChanged()),
    // make sure it is correct during this operation.
    hover (m_currentPoint);

    // Currently used only to end the current text
    if (hasBegunShape ())
        endShape (m_currentPoint, kpBug::QRect_Normalized (QRect (m_startPoint/* TODO: wrong */, m_currentPoint)));

    m_dragType = Create;
    m_dragHasBegun = false;

    kpSelection *sel = document ()->selection ();
    m_hadSelectionBeforeDrag = bool (sel);

    if (sel)
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\thas sel region rect=" << sel->boundingRect () << endl;
    #endif
        QRect selectionRect = sel->boundingRect ();

        if (onSelectionResizeHandle () && !controlOrShiftPressed ())
        {
        #if DEBUG_KP_TOOL_SELECTION
            kDebug () << "\t\tis resize/scale" << endl;
        #endif

            m_startDragFromSelectionTopLeft = m_currentPoint - selectionRect.topLeft ();
            m_dragType = ResizeScale;
            m_resizeScaleType = onSelectionResizeHandle ();

            viewManager ()->setQueueUpdates ();
            {
                viewManager ()->setSelectionBorderVisible (true);
                viewManager ()->setSelectionBorderFinished (true);
                viewManager ()->setTextCursorEnabled (false);
            }
            viewManager ()->restoreQueueUpdates ();
        }
        else if (sel->contains (m_currentPoint))
        {
            if (m_mode == Text && onSelectionToSelectText () && !controlOrShiftPressed ())
            {
            #if DEBUG_KP_TOOL_SELECTION
                kDebug () << "\t\tis select cursor pos" << endl;
            #endif

                m_dragType = SelectText;

                viewManager ()->setTextCursorPosition (sel->textRowForPoint (m_currentPoint),
                                                       sel->textColForPoint (m_currentPoint));
            }
            else
            {
            #if DEBUG_KP_TOOL_SELECTION
                kDebug () << "\t\tis move" << endl;
            #endif

                m_startDragFromSelectionTopLeft = m_currentPoint - selectionRect.topLeft ();
                m_dragType = Move;

                if (m_mouseButton == 0)
                {
                    setSelectionBorderForMove ();
                }
                else
                {
                    // Don't hide sel border momentarily if user is just
                    // right _clicking_ selection.
                    // (single shot timer)
                    m_RMBMoveUpdateGUITimer->start (100/*ms*/);
                }
            }
        }
        else
        {
        #if DEBUG_KP_TOOL_SELECTION
            kDebug () << "\t\tis new sel" << endl;
        #endif

            pushOntoDocument ();
        }
    }

    // creating new selection?
    if (m_dragType == Create)
    {
        viewManager ()->setQueueUpdates ();
        {
            viewManager ()->setSelectionBorderVisible (true);
            viewManager ()->setSelectionBorderFinished (false);
            viewManager ()->setTextCursorEnabled (false);
        }
        viewManager ()->restoreQueueUpdates ();

        // (single shot)
        m_createNOPTimer->start (200/*ms*/);
    }

    if (m_dragType != SelectText)
    {
        setUserMessage (cancelUserMessage ());
    }
}


// protected
QCursor kpToolSelection::cursor () const
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelection::cursor()"
               << " m_currentPoint=" << m_currentPoint
               << " QCursor::pos() view under cursor="
               << (viewUnderCursor () ?
                    viewUnderCursor ()->mapFromGlobal (QCursor::pos ()) :
                    KP_INVALID_POINT)
               << " controlOrShiftPressed=" << controlOrShiftPressed ()
               << endl;
#endif

    kpSelection *sel = document () ? document ()->selection () : 0;

    if (sel && onSelectionResizeHandle () && !controlOrShiftPressed ())
    {
    #if DEBUG_KP_TOOL_SELECTION && 1
        kDebug () << "\tonSelectionResizeHandle="
                   << onSelectionResizeHandle () << endl;
    #endif
        switch (onSelectionResizeHandle ())
        {
        case (kpView::Top | kpView::Left):
        case (kpView::Bottom | kpView::Right):
            return Qt::SizeFDiagCursor;

        case (kpView::Bottom | kpView::Left):
        case (kpView::Top | kpView::Right):
            return Qt::SizeBDiagCursor;

        case kpView::Top:
        case kpView::Bottom:
            return Qt::SizeVerCursor;

        case kpView::Left:
        case kpView::Right:
            return Qt::SizeHorCursor;
        }

        return Qt::ArrowCursor;
    }
    else if (sel && sel->contains (m_currentPoint))
    {
    #if DEBUG_KP_TOOL_SELECTION && 1
        kDebug () << "\tsel contains currentPoint; selecting text? "
                   << onSelectionToSelectText () << endl;
    #endif

        if (m_mode == Text && onSelectionToSelectText () && !controlOrShiftPressed ())
            return Qt::IBeamCursor;
        else
            return Qt::SizeAllCursor;
    }
    else
    {
    #if DEBUG_KP_TOOL_SELECTION && 1
        kDebug () << "\tnot on sel" << endl;
    #endif
        return Qt::CrossCursor;
    }
}

// virtual
void kpToolSelection::hover (const QPoint &point)
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelection::hover" << point << endl;
#endif

    viewManager ()->setCursor (cursor ());

    setUserShapePoints (point, KP_INVALID_POINT, false/*don't set size*/);
    if (document () && document ()->selection ())
    {
        setUserShapeSize (document ()->selection ()->width (),
                          document ()->selection ()->height ());
    }
    else
    {
        setUserShapeSize (KP_INVALID_SIZE);
    }

    QString mess = haventBegunDrawUserMessage ();
    if (mess != userMessage ())
        setUserMessage (mess);
}

// protected
void kpToolSelection::popupRMBMenu ()
{
    QMenu *pop = mainWindow () ? mainWindow ()->selectionToolRMBMenu () : 0;
    if (!pop)
    {
        kError () << "kpToolSelection::popupRMBMenu() no QMenu*" << endl;
        return;
    }

    // WARNING: enters event loop - may re-enter view/tool event handlers
    pop->exec (QCursor::pos ());

    // Cursor may have moved while menu up, triggering mouseMoveEvents
    // for the menu - not the view.  Update cursor position now.
    somethingBelowTheCursorChanged ();
}

// protected
void kpToolSelection::setSelectionBorderForMove ()
{
    // don't show border while moving
    viewManager ()->setQueueUpdates ();
    {
        viewManager ()->setSelectionBorderVisible (false);
        viewManager ()->setSelectionBorderFinished (true);
        viewManager ()->setTextCursorEnabled (false);
    }
    viewManager ()->restoreQueueUpdates ();
}

// protected slot
void kpToolSelection::slotRMBMoveUpdateGUI ()
{
    // (just in case not called from single shot)
    m_RMBMoveUpdateGUITimer->stop ();

    setSelectionBorderForMove ();

    kpSelection * const sel = document () ? document ()->selection () : 0;
    if (sel)
        setUserShapePoints (sel->topLeft ());
}

// protected slot
void kpToolSelection::delayedDraw ()
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelection::delayedDraw() hasBegunDraw="
               << hasBegunDraw ()
               << " currentPoint=" << m_currentPoint
               << " lastPoint=" << m_lastPoint
               << " startPoint=" << m_startPoint
               << endl;
#endif

    // (just in case not called from single shot)
    m_createNOPTimer->stop ();

    if (hasBegunDraw ())
    {
        draw (m_currentPoint, m_lastPoint,
              kpBug::QRect_Normalized (QRect (m_startPoint, m_currentPoint)));
    }
}

// virtual
void kpToolSelection::draw (const QPoint &inThisPoint, const QPoint & /*lastPoint*/,
                            const QRect &inNormalizedRect)
{
    QPoint thisPoint = inThisPoint;
    QRect normalizedRect = inNormalizedRect;

#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelection::draw" << thisPoint
               << " startPoint=" << m_startPoint
               << " normalizedRect=" << normalizedRect << endl;
#endif


    // OPT: return when thisPoint == m_lastPoint so that e.g. when creating
    //      Points sel, press modifiers doesn't add multiple points in same
    //      place


    bool nextDragHasBegun = true;


    if (m_dragType == Create)
    {
    #if DEBUG_KP_TOOL_SELECTION && 1
        kDebug () << "\tnot moving - resizing rect to" << normalizedRect
                   << endl;
        kDebug () << "\t\tcreateNOPTimer->isActive()="
                   << m_createNOPTimer->isActive ()
                   << " viewManhattanLength from startPoint="
                   << m_viewUnderStartPoint->transformDocToViewX ((thisPoint - m_startPoint).manhattanLength ())
                   << endl;
    #endif

        if (m_createNOPTimer->isActive ())
        {
            if (m_viewUnderStartPoint->transformDocToViewX ((thisPoint - m_startPoint).manhattanLength ()) <= 6)
            {
            #if DEBUG_KP_TOOL_SELECTION && 1
                kDebug () << "\t\tsuppress accidental movement" << endl;
            #endif
                thisPoint = m_startPoint;
            }
            else
            {
            #if DEBUG_KP_TOOL_SELECTION && 1
                kDebug () << "\t\tit's a \"big\" intended move - stop timer" << endl;
            #endif
                m_createNOPTimer->stop ();
            }
        }


        // Prevent unintentional 1-pixel selections
        if (!m_dragHasBegun && thisPoint == m_startPoint)
        {
            if (m_mode != kpToolSelection::Text)
            {
            #if DEBUG_KP_TOOL_SELECTION && 1
                kDebug () << "\tnon-text NOP - return" << endl;
            #endif
                setUserShapePoints (thisPoint);
                return;
            }
            else  // m_mode == kpToolSelection::Text
            {
                // Attempt to deselect text box by clicking?
                if (m_hadSelectionBeforeDrag)
                {
                #if DEBUG_KP_TOOL_SELECTION && 1
                    kDebug () << "\ttext box deselect - NOP - return" << endl;
                #endif
                    setUserShapePoints (thisPoint);
                    return;
                }

                // Drag-wise, this is a NOP so we'd normally return (hence
                // m_dragHasBegun would not change).  However, as a special
                // case, allow user to create a text box using a single
                // click.  But don't set m_dragHasBegun for next iteration
                // since it would be untrue.
                //
                // This makes sure that a single click creation of text box
                // works even if draw() is invoked more than once at the
                // same position (esp. with accidental drag suppression
                // (above)).
                nextDragHasBegun = false;
            }
        }


        switch (m_mode)
        {
        case kpToolSelection::Rectangle:
        {
            const QRect usefulRect = normalizedRect.intersect (document ()->rect ());
            document ()->setSelection (kpSelection (kpSelection::Rectangle, usefulRect,
                                                    mainWindow ()->selectionTransparency ()));

            setUserShapePoints (m_startPoint,
                                QPoint (qMax (0, qMin (m_currentPoint.x (), document ()->width () - 1)),
                                        qMax (0, qMin (m_currentPoint.y (), document ()->height () - 1))));
            break;
        }
        case kpToolSelection::Text:
        {
            const kpTextStyle textStyle = mainWindow ()->textStyle ();

            int minimumWidth, minimumHeight;

            // Just a click?
            if (!m_dragHasBegun && thisPoint == m_startPoint)
            {
            #if DEBUG_KP_TOOL_SELECTION && 1
                kDebug () << "\tclick creating text box" << endl;
            #endif

                // (Click creating text box with RMB would not be obvious
                //  since RMB menu most likely hides text box immediately
                //  afterwards)
                if (m_mouseButton == 1)
                    break;


                minimumWidth = kpSelection::preferredMinimumWidthForTextStyle (textStyle);
                if (thisPoint.x () >= m_startPoint.x ())
                {
                    if (m_startPoint.x () + minimumWidth - 1 >= document ()->width ())
                    {
                        minimumWidth = qMax (kpSelection::minimumWidthForTextStyle (textStyle),
                                             document ()->width () - m_startPoint.x ());
                    }
                }
                else
                {
                    if (m_startPoint.x () - minimumWidth + 1 < 0)
                    {
                        minimumWidth = qMax (kpSelection::minimumWidthForTextStyle (textStyle),
                                             m_startPoint.x () + 1);
                    }
                }

                minimumHeight = kpSelection::preferredMinimumHeightForTextStyle (textStyle);
                if (thisPoint.y () >= m_startPoint.y ())
                {
                    if (m_startPoint.y () + minimumHeight - 1 >= document ()->height ())
                    {
                        minimumHeight = qMax (kpSelection::minimumHeightForTextStyle (textStyle),
                                            document ()->height () - m_startPoint.y ());
                    }
                }
                else
                {
                    if (m_startPoint.y () - minimumHeight + 1 < 0)
                    {
                        minimumHeight = qMax (kpSelection::minimumHeightForTextStyle (textStyle),
                                            m_startPoint.y () + 1);
                    }
                }
            }
            else
            {
            #if DEBUG_KP_TOOL_SELECTION && 1
                kDebug () << "\tdrag creating text box" << endl;
            #endif
                minimumWidth = kpSelection::minimumWidthForTextStyle (textStyle);
                minimumHeight = kpSelection::minimumHeightForTextStyle (textStyle);
            }


            if (normalizedRect.width () < minimumWidth)
            {
                if (thisPoint.x () >= m_startPoint.x ())
                    normalizedRect.setWidth (minimumWidth);
                else
                    normalizedRect.setX (normalizedRect.right () - minimumWidth + 1);
            }

            if (normalizedRect.height () < minimumHeight)
            {
                if (thisPoint.y () >= m_startPoint.y ())
                    normalizedRect.setHeight (minimumHeight);
                else
                    normalizedRect.setY (normalizedRect.bottom () - minimumHeight + 1);
            }
        #if DEBUG_KP_TOOL_SELECTION && 1
            kDebug () << "\t\tnormalizedRect=" << normalizedRect
                       << " kpSelection::preferredMinimumSize="
                           << QSize (minimumWidth, minimumHeight)
                       << endl;
        #endif

            QList <QString> textLines;
            textLines.append (QString ());
            kpSelection sel (normalizedRect, textLines, textStyle);

            if (!m_currentCreateTextCommand)
            {
                m_currentCreateTextCommand = new kpToolSelectionCreateCommand (
                    i18n ("Text: Create Box"),
                    sel,
                    mainWindow ());
            }
            else
                m_currentCreateTextCommand->setFromSelection (sel);

            viewManager ()->setTextCursorPosition (0, 0);
            document ()->setSelection (sel);

            QPoint actualEndPoint = KP_INVALID_POINT;
            if (m_startPoint == normalizedRect.topLeft ())
                actualEndPoint = normalizedRect.bottomRight ();
            else if (m_startPoint == normalizedRect.bottomRight ())
                actualEndPoint = normalizedRect.topLeft ();
            else if (m_startPoint == normalizedRect.topRight ())
                actualEndPoint = normalizedRect.bottomLeft ();
            else if (m_startPoint == normalizedRect.bottomLeft ())
                actualEndPoint = normalizedRect.topRight ();

            setUserShapePoints (m_startPoint, actualEndPoint);
            break;
        }
        case kpToolSelection::Ellipse:
            document ()->setSelection (kpSelection (kpSelection::Ellipse, normalizedRect,
                                                    mainWindow ()->selectionTransparency ()));
            setUserShapePoints (m_startPoint, m_currentPoint);
            break;
        case kpToolSelection::FreeForm:
            QPolygon points;

            if (document ()->selection ())
                points = document ()->selection ()->points ();


            // (not detached so will modify "points" directly but
            //  still need to call kpDocument::setSelection() to
            //  update screen)

            if (!m_dragHasBegun)
            {
                // We thought the drag at startPoint was a NOP
                // but it turns out that it wasn't...
                points.putPoints (points.count (), 1, m_startPoint.x (), m_startPoint.y ());
            }

            // TODO: there should be an upper limit on this before drawing the
            //       polygon becomes too slow
            points.putPoints (points.count (), 1, thisPoint.x (), thisPoint.y ());


            document ()->setSelection (kpSelection (points, mainWindow ()->selectionTransparency ()));
        #if DEBUG_KP_TOOL_SELECTION && 1
            kDebug () << "\t\tfreeform; #points=" << document ()->selection ()->points ().count () << endl;
        #endif

            setUserShapePoints (m_currentPoint);
            break;
        }

        viewManager ()->setSelectionBorderVisible (true);
    }
    else if (m_dragType == Move)
    {
    #if DEBUG_KP_TOOL_SELECTION && 1
       kDebug () << "\tmoving selection" << endl;
    #endif

        kpSelection *sel = document ()->selection ();

        QRect targetSelRect = QRect (thisPoint.x () - m_startDragFromSelectionTopLeft.x (),
                                     thisPoint.y () - m_startDragFromSelectionTopLeft.y (),
                                     sel->width (),
                                     sel->height ());

    #if DEBUG_KP_TOOL_SELECTION && 1
        kDebug () << "\t\tstartPoint=" << m_startPoint
                   << " thisPoint=" << thisPoint
                   << " startDragFromSel=" << m_startDragFromSelectionTopLeft
                   << " targetSelRect=" << targetSelRect
                   << endl;
    #endif

        // Try to make sure selection still intersects document so that it's
        // reachable.

        if (targetSelRect.right () < 0)
            targetSelRect.translate (-targetSelRect.right (), 0);
        else if (targetSelRect.left () >= document ()->width ())
            targetSelRect.translate (document ()->width () - targetSelRect.left () - 1, 0);

        if (targetSelRect.bottom () < 0)
            targetSelRect.translate (0, -targetSelRect.bottom ());
        else if (targetSelRect.top () >= document ()->height ())
            targetSelRect.translate (0, document ()->height () - targetSelRect.top () - 1);

    #if DEBUG_KP_TOOL_SELECTION && 1
        kDebug () << "\t\t\tafter ensure sel rect clickable=" << targetSelRect << endl;
    #endif


        if (!m_dragHasBegun &&
            targetSelRect.topLeft () + m_startDragFromSelectionTopLeft == m_startPoint)
        {
        #if DEBUG_KP_TOOL_SELECTION && 1
            kDebug () << "\t\t\t\tnop" << endl;
        #endif


            if (!m_RMBMoveUpdateGUITimer->isActive ())
            {
                // (slotRMBMoveUpdateGUI() calls similar line)
                setUserShapePoints (sel->topLeft ());
            }

            // Prevent both NOP drag-moves
            return;
        }


        if (m_RMBMoveUpdateGUITimer->isActive ())
        {
            m_RMBMoveUpdateGUITimer->stop ();
            slotRMBMoveUpdateGUI ();
        }


        if (!sel->pixmap () && !m_currentPullFromDocumentCommand)
        {
            m_currentPullFromDocumentCommand = new kpToolSelectionPullFromDocumentCommand (
                QString::null/*uninteresting child of macro cmd*/,
                mainWindow ());
            m_currentPullFromDocumentCommand->execute ();
        }

        if (!m_currentMoveCommand)
        {
            m_currentMoveCommand = new kpToolSelectionMoveCommand (
                QString::null/*uninteresting child of macro cmd*/,
                mainWindow ());
            m_currentMoveCommandIsSmear = false;
        }


        //viewManager ()->setQueueUpdates ();
        //viewManager ()->setFastUpdates ();

        if (m_shiftPressed)
            m_currentMoveCommandIsSmear = true;

        if (!m_dragHasBegun && (m_controlPressed || m_shiftPressed))
            m_currentMoveCommand->copyOntoDocument ();

        m_currentMoveCommand->moveTo (targetSelRect.topLeft ());

        if (m_shiftPressed)
            m_currentMoveCommand->copyOntoDocument ();

        //viewManager ()->restoreFastUpdates ();
        //viewManager ()->restoreQueueUpdates ();

        QPoint start = m_currentMoveCommand->originalSelection ().topLeft ();
        QPoint end = targetSelRect.topLeft ();
        setUserShapePoints (start, end, false/*don't set size*/);
        setUserShapeSize (end.x () - start.x (), end.y () - start.y ());
    }
    else if (m_dragType == ResizeScale)
    {
    #if DEBUG_KP_TOOL_SELECTION && 1
        kDebug () << "\tresize/scale" << endl;
    #endif

        kpSelection *sel = document ()->selection ();

        if (!m_dragHasBegun && thisPoint == m_startPoint)
        {
        #if DEBUG_KP_TOOL_SELECTION && 1
            kDebug () << "\t\tnop" << endl;
        #endif

            setUserShapePoints (QPoint (sel->width (), sel->height ()));
            return;
        }


        if (!sel->pixmap () && !m_currentPullFromDocumentCommand)
        {
            m_currentPullFromDocumentCommand = new kpToolSelectionPullFromDocumentCommand (
                QString::null/*uninteresting child of macro cmd*/,
                mainWindow ());
            m_currentPullFromDocumentCommand->execute ();
        }

        if (!m_currentResizeScaleCommand)
        {
            m_currentResizeScaleCommand = new kpToolSelectionResizeScaleCommand (mainWindow ());
        }


        kpSelection originalSelection = m_currentResizeScaleCommand->originalSelection ();
        const int oldWidth = originalSelection.width ();
        const int oldHeight = originalSelection.height ();

        
        // Determine new width.
        
        int userXSign = 0;
        if (m_resizeScaleType & kpView::Left)
            userXSign = -1;
        else if (m_resizeScaleType & kpView::Right)
            userXSign = +1;
            
        int newWidth = oldWidth + userXSign * (thisPoint.x () - m_startPoint.x ());
        
        newWidth = qMax (originalSelection.minimumWidth (), newWidth);
        
        
        // Determine new height.
        
        int userYSign = 0;
        if (m_resizeScaleType & kpView::Top)
            userYSign = -1;
        else if (m_resizeScaleType & kpView::Bottom)
            userYSign = +1;
            
        int newHeight = oldHeight + userYSign * (thisPoint.y () - m_startPoint.y ());
        
        newHeight = qMax (originalSelection.minimumHeight (), newHeight);

                
        // Keep aspect ratio?
        if (m_shiftPressed && !sel->isText ())
        {
            // Width changed more than height?  At equality, favor width.
            // Fix width, change height.
            if ((userXSign ? double (newWidth) / oldWidth : 0) >=
                (userYSign ? double (newHeight) / oldHeight : 0))
            {
                newHeight = newWidth * oldHeight / oldWidth;
                newHeight = qMax (originalSelection.minimumHeight (),
                                  newHeight);
            }
            // Height changed more than width?
            // Fix height, change width.
            else
            {
                newWidth = newHeight * oldWidth / oldHeight;
                newWidth = qMax (originalSelection.minimumWidth (), newWidth);
            }
        }
            
                              
        // Adjust x/y to new width/height for left/top resizes.
            
        int newX = originalSelection.x ();
        int newY = originalSelection.y ();
                                                
        if (m_resizeScaleType & kpView::Left)
        {
            newX -= (newWidth - originalSelection.width ());
        }
        
        if (m_resizeScaleType & kpView::Top)
        {
            newY -= (newHeight - originalSelection.height ());
        }

    #if DEBUG_KP_TOOL_SELECTION && 1
        kDebug () << "\t\tnewX=" << newX
                   << " newY=" << newY
                   << " newWidth=" << newWidth
                   << " newHeight=" << newHeight
                   << endl;
    #endif


        viewManager ()->setFastUpdates ();
        m_currentResizeScaleCommand->resizeAndMoveTo (newWidth, newHeight,
                                                      QPoint (newX, newY),
                                                      true/*smooth scale delayed*/);
        viewManager ()->restoreFastUpdates ();

        setUserShapePoints (QPoint (originalSelection.width (),
                                    originalSelection.height ()),
                            QPoint (newWidth,
                                    newHeight),
                            false/*don't set size*/);
        setUserShapeSize (newWidth - originalSelection.width (),
                          newHeight - originalSelection.height ());
    }


    m_dragHasBegun = nextDragHasBegun;
}

// virtual
void kpToolSelection::cancelShape ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelection::cancelShape() mouseButton=" << m_mouseButton << endl;
#endif

    m_createNOPTimer->stop ();
    m_RMBMoveUpdateGUITimer->stop ();


    viewManager ()->setQueueUpdates ();
    {
        if (m_dragType == Move)
        {
        #if DEBUG_KP_TOOL_SELECTION
            kDebug () << "\twas drag moving - undo drag and undo acquire" << endl;
        #endif

            if (m_currentMoveCommand)
            {
            #if DEBUG_KP_TOOL_SELECTION
                kDebug () << "\t\tundo currentMoveCommand" << endl;
            #endif
                m_currentMoveCommand->finalize ();
                m_currentMoveCommand->unexecute ();
                delete m_currentMoveCommand;
                m_currentMoveCommand = 0;

                if (document ()->selection ()->isText ())
                    viewManager ()->setTextCursorBlinkState (true);
            }
        }
        else if (m_dragType == Create)
        {
        #if DEBUG_KP_TOOL_SELECTION
            kDebug () << "\twas creating sel - kill" << endl;
        #endif

            // TODO: should we give the user back the selection s/he had before (if any)?
            document ()->selectionDelete ();

            if (m_currentCreateTextCommand)
            {
                delete m_currentCreateTextCommand;
                m_currentCreateTextCommand = 0;
            }
        }
        else if (m_dragType == ResizeScale)
        {
        #if DEBUG_KP_TOOL_SELECTION
            kDebug () << "\twas resize/scale sel - kill" << endl;
        #endif

            if (m_currentResizeScaleCommand)
            {
            #if DEBUG_KP_TOOL_SELECTION
                kDebug () << "\t\tundo currentResizeScaleCommand" << endl;
            #endif
                m_currentResizeScaleCommand->unexecute ();
                delete m_currentResizeScaleCommand;
                m_currentResizeScaleCommand = 0;

                if (document ()->selection ()->isText ())
                    viewManager ()->setTextCursorBlinkState (true);
            }
        }


        if (m_currentPullFromDocumentCommand)
        {
        #if DEBUG_KP_TOOL_SELECTION
            kDebug () << "\t\tundo pullFromDocumentCommand" << endl;
        #endif
            m_currentPullFromDocumentCommand->unexecute ();
            delete m_currentPullFromDocumentCommand;
            m_currentPullFromDocumentCommand = 0;
        }


        viewManager ()->setSelectionBorderVisible (true);
        viewManager ()->setSelectionBorderFinished (true);
        viewManager ()->setTextCursorEnabled (m_mode == Text);
    }
    viewManager ()->restoreQueueUpdates ();


    m_dragType = Unknown;
    m_cancelledShapeButStillHoldingButtons = true;
    setUserMessage (i18n ("Let go of all the mouse buttons."));
}

// virtual
void kpToolSelection::releasedAllButtons ()
{
    m_cancelledShapeButStillHoldingButtons = false;
    setUserMessage (haventBegunDrawUserMessage ());
}

// virtual
void kpToolSelection::endDraw (const QPoint & /*thisPoint*/, const QRect & /*normalizedRect*/)
{
    m_createNOPTimer->stop ();
    m_RMBMoveUpdateGUITimer->stop ();


    viewManager ()->setQueueUpdates ();
    {
        if (m_currentCreateTextCommand)
        {
            commandHistory ()->addCommand (m_currentCreateTextCommand, false/*no exec*/);
            m_currentCreateTextCommand = 0;
        }

        kpMacroCommand *cmd = 0;
        if (m_currentMoveCommand)
        {
            if (m_currentMoveCommandIsSmear)
            {
                cmd = new kpMacroCommand (i18n ("%1: Smear",
                                              document ()->selection ()->name ()),
                                        mainWindow ());
            }
            else
            {
                cmd = new kpMacroCommand ((document ()->selection ()->isText () ?
                                            i18n ("Text: Move Box") :
                                            i18n ("Selection: Move")),
                                        mainWindow ());
            }

            if (document ()->selection ()->isText ())
                viewManager ()->setTextCursorBlinkState (true);
        }
        else if (m_currentResizeScaleCommand)
        {
            cmd = new kpMacroCommand (m_currentResizeScaleCommand->kpNamedCommand::name (),
                                    mainWindow ());

            if (document ()->selection ()->isText ())
                viewManager ()->setTextCursorBlinkState (true);
        }

        if (m_currentPullFromDocumentCommand)
        {
            if (!m_currentMoveCommand && !m_currentResizeScaleCommand)
            {
                kError () << "kpToolSelection::endDraw() pull without move nor resize/scale" << endl;
                delete m_currentPullFromDocumentCommand;
                m_currentPullFromDocumentCommand = 0;
            }
            else
            {
                kpSelection selection;

                if (m_currentMoveCommand)
                    selection = m_currentMoveCommand->originalSelection ();
                else if (m_currentResizeScaleCommand)
                    selection = m_currentResizeScaleCommand->originalSelection ();

                // just the border
                selection.setPixmap (QPixmap ());

                kpCommand *createCommand = new kpToolSelectionCreateCommand (
                    i18n ("Selection: Create"),
                    selection,
                    mainWindow ());

                if (kpToolSelectionCreateCommand::nextUndoCommandIsCreateBorder (commandHistory ()))
                    commandHistory ()->setNextUndoCommand (createCommand);
                else
                    commandHistory ()->addCommand (createCommand,
                                                   false/*no exec - user already dragged out sel*/);


                cmd->addCommand (m_currentPullFromDocumentCommand);
                m_currentPullFromDocumentCommand = 0;
            }
        }

        if (m_currentMoveCommand)
        {
            m_currentMoveCommand->finalize ();
            cmd->addCommand (m_currentMoveCommand);
            m_currentMoveCommand = 0;

            if (document ()->selection ()->isText ())
                viewManager ()->setTextCursorBlinkState (true);
        }

        if (m_currentResizeScaleCommand)
        {
            cmd->addCommand (m_currentResizeScaleCommand);
            m_currentResizeScaleCommand = 0;

            if (document ()->selection ()->isText ())
                viewManager ()->setTextCursorBlinkState (true);
        }

        if (cmd)
            commandHistory ()->addCommand (cmd, false/*no exec*/);

        viewManager ()->setSelectionBorderVisible (true);
        viewManager ()->setSelectionBorderFinished (true);
        viewManager ()->setTextCursorEnabled (m_mode == Text);
    }
    viewManager ()->restoreQueueUpdates ();


    m_dragType = Unknown;
    setUserMessage (haventBegunDrawUserMessage ());


    if (m_mouseButton == 1/*right*/)
        popupRMBMenu ();
}


// protected virtual [base kpTool]
void kpToolSelection::keyPressEvent (QKeyEvent *e)
{
#if DEBUG_KP_TOOL_SELECTION && 0
    kDebug () << "kpToolSelection::keyPressEvent(e->text='" << e->text () << "')" << endl;
#endif


    e->ignore ();


    if (document ()->selection () &&
        !hasBegunDraw () &&
         e->key () == Qt::Key_Escape)
    {
    #if DEBUG_KP_TOOL_SELECTION && 0
        kDebug () << "\tescape pressed with sel when not begun draw - deselecting" << endl;
    #endif

        pushOntoDocument ();
        e->accept ();
    }


    if (!e->isAccepted ())
    {
    #if DEBUG_KP_TOOL_SELECTION && 0
        kDebug () << "\tkey processing did not accept (text was '"
                   << e->text ()
                   << "') - passing on event to kpTool"
                   << endl;
    #endif

        kpTool::keyPressEvent (e);
        return;
    }
}


// private slot
void kpToolSelection::selectionTransparencyChanged (const QString & /*name*/)
{
#if 0
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelection::selectionTransparencyChanged(" << name << ")" << endl;
#endif

    if (mainWindow ()->settingSelectionTransparency ())
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\trecursion - abort setting selection transparency: "
                   << mainWindow ()->settingSelectionTransparency () << endl;
    #endif
        return;
    }

    if (document ()->selection ())
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\thave sel - set transparency" << endl;
    #endif

        kpSelectionTransparency oldST = document ()->selection ()->transparency ();
        kpSelectionTransparency st = mainWindow ()->selectionTransparency ();

        // TODO: This "NOP" check causes us a great deal of trouble e.g.:
        //
        //       Select a solid red rectangle.
        //       Switch to transparent and set red as the background colour.
        //       (the selection is now invisible)
        //       Invert Colours.
        //       (the selection is now cyan)
        //       Change the background colour to green.
        //       (no command is added to undo this as the selection does not change)
        //       Undo.
        //       The rectangle is no longer invisible.
        //
        //if (document ()->selection ()->setTransparency (st, true/*check harder for no change in mask*/))

        document ()->selection ()->setTransparency (st);
        if (true)
        {
        #if DEBUG_KP_TOOL_SELECTION
            kDebug () << "\t\twhich changed the pixmap" << endl;
        #endif

            commandHistory ()->addCommand (new kpToolSelectionTransparencyCommand (
                i18n ("Selection: Transparency"), // name,
                st, oldST,
                mainWindow ()),
                false/* no exec*/);
        }
    }
#endif

    // TODO: I've duplicated the code (see below 3x) to make sure
    //       kpSelectionTransparency(oldST)::transparentColor() is defined
    //       and not taken from kpDocument (where it may not be defined because
    //       the transparency may be opaque).
    //
    //       That way kpToolSelectionTransparencyCommand can force set colours.
}


// protected slot virtual
void kpToolSelection::slotIsOpaqueChanged ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelection::slotIsOpaqueChanged()" << endl;
#endif

    if (mainWindow ()->settingSelectionTransparency ())
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\trecursion - abort setting selection transparency: "
                   << mainWindow ()->settingSelectionTransparency () << endl;
    #endif
        return;
    }

    if (document ()->selection ())
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\thave sel - set transparency" << endl;
    #endif

        QApplication::setOverrideCursor (Qt::WaitCursor);

        if (hasBegunShape ())
            endShapeInternal ();

        kpSelectionTransparency st = mainWindow ()->selectionTransparency ();
        kpSelectionTransparency oldST = st;
        oldST.setOpaque (!oldST.isOpaque ());

        document ()->selection ()->setTransparency (st);
        commandHistory ()->addCommand (new kpToolSelectionTransparencyCommand (
            st.isOpaque () ?
                i18n ("Selection: Opaque") :
                i18n ("Selection: Transparent"),
            st, oldST,
            mainWindow ()),
            false/* no exec*/);

        QApplication::restoreOverrideCursor ();
    }
}

// protected slot virtual [base kpTool]
void kpToolSelection::slotBackgroundColorChanged (const kpColor &)
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelection::slotBackgroundColorChanged()" << endl;
#endif

    if (mainWindow ()->settingSelectionTransparency ())
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\trecursion - abort setting selection transparency: "
                   << mainWindow ()->settingSelectionTransparency () << endl;
    #endif
        return;
    }

    if (document ()->selection ())
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\thave sel - set transparency" << endl;
    #endif

        QApplication::setOverrideCursor (Qt::WaitCursor);

        kpSelectionTransparency st = mainWindow ()->selectionTransparency ();
        kpSelectionTransparency oldST = st;
        oldST.setTransparentColor (oldBackgroundColor ());

        document ()->selection ()->setTransparency (st);
        commandHistory ()->addCommand (new kpToolSelectionTransparencyCommand (
            i18n ("Selection: Transparency Color"),
            st, oldST,
            mainWindow ()),
            false/* no exec*/);

        QApplication::restoreOverrideCursor ();
    }
}

// protected slot virtual [base kpTool]
void kpToolSelection::slotColorSimilarityChanged (double, int)
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelection::slotColorSimilarityChanged()" << endl;
#endif

    if (mainWindow ()->settingSelectionTransparency ())
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\trecursion - abort setting selection transparency: "
                   << mainWindow ()->settingSelectionTransparency () << endl;
    #endif
        return;
    }

    if (document ()->selection ())
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\thave sel - set transparency" << endl;
    #endif

        QApplication::setOverrideCursor (Qt::WaitCursor);

        kpSelectionTransparency st = mainWindow ()->selectionTransparency ();
        kpSelectionTransparency oldST = st;
        oldST.setColorSimilarity (oldColorSimilarity ());

        document ()->selection ()->setTransparency (st);
        commandHistory ()->addCommand (new kpToolSelectionTransparencyCommand (
            i18n ("Selection: Transparency Color Similarity"),
            st, oldST,
            mainWindow ()),
            false/* no exec*/);

        QApplication::restoreOverrideCursor ();
    }
}


/*
 * kpToolSelectionCreateCommand
 */

kpToolSelectionCreateCommand::kpToolSelectionCreateCommand (const QString &name,
                                                            const kpSelection &fromSelection,
                                                            kpMainWindow *mainWindow)
    : kpNamedCommand (name, mainWindow),
      m_fromSelection (0),
      m_textRow (0), m_textCol (0)
{
    setFromSelection (fromSelection);
}

kpToolSelectionCreateCommand::~kpToolSelectionCreateCommand ()
{
    delete m_fromSelection;
}


// public virtual [base kpCommand]
int kpToolSelectionCreateCommand::size () const
{
    return kpPixmapFX::selectionSize (m_fromSelection);
}


// public static
bool kpToolSelectionCreateCommand::nextUndoCommandIsCreateBorder (
    kpCommandHistory *commandHistory)
{
    if (!commandHistory)
        return false;

    kpCommand *cmd = commandHistory->nextUndoCommand ();
    if (!cmd)
        return false;

    kpToolSelectionCreateCommand *c = dynamic_cast <kpToolSelectionCreateCommand *> (cmd);
    if (!c)
        return false;

    const kpSelection *sel = c->fromSelection ();
    if (!sel)
        return false;

    return (!sel->pixmap ());
}


// public
const kpSelection *kpToolSelectionCreateCommand::fromSelection () const
{
    return m_fromSelection;
}

// public
void kpToolSelectionCreateCommand::setFromSelection (const kpSelection &fromSelection)
{
    delete m_fromSelection;
    m_fromSelection = new kpSelection (fromSelection);
}

// public virtual [base kpCommand]
void kpToolSelectionCreateCommand::execute ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelectionCreateCommand::execute()" << endl;
#endif

    kpDocument *doc = document ();
    if (!doc)
    {
        kError () << "kpToolSelectionCreateCommand::execute() without doc" << endl;
        return;
    }

    if (m_fromSelection)
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\tusing fromSelection" << endl;
        kDebug () << "\t\thave sel=" << doc->selection ()
                   << " pixmap=" << (doc->selection () ? doc->selection ()->pixmap () : 0)
                   << endl;
    #endif
        if (!m_fromSelection->isText ())
        {
            if (m_fromSelection->transparency () != m_mainWindow->selectionTransparency ())
                m_mainWindow->setSelectionTransparency (m_fromSelection->transparency ());
        }
        else
        {
            if (m_fromSelection->textStyle () != m_mainWindow->textStyle ())
                m_mainWindow->setTextStyle (m_fromSelection->textStyle ());
        }

        m_mainWindow->viewManager ()->setTextCursorPosition (m_textRow, m_textCol);
        doc->setSelection (*m_fromSelection);

        if (m_mainWindow->tool ())
            m_mainWindow->tool ()->somethingBelowTheCursorChanged ();
    }
}

// public virtual [base kpCommand]
void kpToolSelectionCreateCommand::unexecute ()
{
    kpDocument *doc = document ();
    if (!doc)
    {
        kError () << "kpToolSelectionCreateCommand::unexecute() without doc" << endl;
        return;
    }

    if (!doc->selection ())
    {
        // Was just a border that got deselected?
        if (m_fromSelection && !m_fromSelection->pixmap ())
            return;

        kError () << "kpToolSelectionCreateCommand::unexecute() without sel region" << endl;
        return;
    }

    m_textRow = m_mainWindow->viewManager ()->textCursorRow ();
    m_textCol = m_mainWindow->viewManager ()->textCursorCol ();

    doc->selectionDelete ();

    if (m_mainWindow->tool ())
        m_mainWindow->tool ()->somethingBelowTheCursorChanged ();
}


/*
 * kpToolSelectionPullFromDocumentCommand
 */

kpToolSelectionPullFromDocumentCommand::kpToolSelectionPullFromDocumentCommand (const QString &name,
                                                                                kpMainWindow *mainWindow)
    : kpNamedCommand (name, mainWindow),
      m_backgroundColor (mainWindow ? mainWindow->backgroundColor () : kpColor::invalid),
      m_originalSelectionRegion (0)
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelectionPullFromDocumentCommand::<ctor>() mainWindow="
               << m_mainWindow
               << endl;
#endif
}

kpToolSelectionPullFromDocumentCommand::~kpToolSelectionPullFromDocumentCommand ()
{
    delete m_originalSelectionRegion;
}


// public virtual [base kpCommand]
int kpToolSelectionPullFromDocumentCommand::size () const
{
    return kpPixmapFX::selectionSize (m_originalSelectionRegion);
}


// public virtual [base kpCommand]
void kpToolSelectionPullFromDocumentCommand::execute ()
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelectionPullFromDocumentCommand::execute()" << endl;
#endif

    Q_ASSERT (m_mainWindow);

    kpDocument *doc = document ();
    Q_ASSERT (doc);

    kpViewManager *vm = m_mainWindow->viewManager ();
    Q_ASSERT (vm);

    vm->setQueueUpdates ();

    // In case the user CTRL+Z'ed, selected a random region to throw us off
    // and then CTRL+Shift+Z'ed putting us here.  Make sure we pull from the
    // originally requested region - not the random one.
    if (m_originalSelectionRegion)
    {
        if (m_originalSelectionRegion->transparency () != m_mainWindow->selectionTransparency ())
            m_mainWindow->setSelectionTransparency (m_originalSelectionRegion->transparency ());

        doc->setSelection (*m_originalSelectionRegion);
    }
    else
    {
        // must have selection region but not pixmap
        if (!doc->selection () || doc->selection ()->pixmap ())
        {
            kError () << "kpToolSelectionPullFromDocumentCommand::execute() sel="
                       << doc->selection ()
                       << " pixmap="
                       << (doc->selection () ? doc->selection ()->pixmap () : 0)
                       << endl;
            vm->restoreQueueUpdates ();
            return;
        }
    }

    doc->selectionPullFromDocument (m_backgroundColor);

    vm->restoreQueueUpdates ();
}

// public virtual [base kpCommand]
void kpToolSelectionPullFromDocumentCommand::unexecute ()
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelectionPullFromDocumentCommand::unexecute()" << endl;
#endif

    kpDocument *doc = document ();

    if (!doc)
    {
        kError () << "kpToolSelectionPullFromDocumentCommand::unexecute() without doc" << endl;
        return;
    }

    // must have selection pixmap
    if (!doc->selection () || !doc->selection ()->pixmap ())
    {
        kError () << "kpToolSelectionPullFromDocumentCommand::unexecute() sel="
                   << doc->selection ()
                   << " pixmap="
                   << (doc->selection () ? doc->selection ()->pixmap () : 0)
                   << endl;
        return;
    }


    // We can have faith that this is the state of the selection after
    // execute(), rather than after the user tried to throw us off by
    // simply selecting another region as to do that, a destroy command
    // must have been used.
    doc->selectionCopyOntoDocument (false/*use opaque pixmap*/);
    doc->selection ()->setPixmap (QPixmap ());

    delete m_originalSelectionRegion;
    m_originalSelectionRegion = new kpSelection (*doc->selection ());
}


/*
 * kpToolSelectionTransparencyCommand
 */

kpToolSelectionTransparencyCommand::kpToolSelectionTransparencyCommand (const QString &name,
    const kpSelectionTransparency &st,
    const kpSelectionTransparency &oldST,
    kpMainWindow *mainWindow)
    : kpNamedCommand (name, mainWindow),
      m_st (st),
      m_oldST (oldST)
{
}

kpToolSelectionTransparencyCommand::~kpToolSelectionTransparencyCommand ()
{
}


// public virtual [base kpCommand]
int kpToolSelectionTransparencyCommand::size () const
{
    return 0;
}


// public virtual [base kpCommand]
void kpToolSelectionTransparencyCommand::execute ()
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelectionTransparencyCommand::execute()" << endl;
#endif
    kpDocument *doc = document ();
    if (!doc)
        return;

    QApplication::setOverrideCursor (Qt::WaitCursor);

    m_mainWindow->setSelectionTransparency (m_st, true/*force colour change*/);

    if (doc->selection ())
        doc->selection ()->setTransparency (m_st);

    QApplication::restoreOverrideCursor ();
}

// public virtual [base kpCommand]
void kpToolSelectionTransparencyCommand::unexecute ()
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelectionTransparencyCommand::unexecute()" << endl;
#endif

    kpDocument *doc = document ();
    if (!doc)
        return;

    QApplication::setOverrideCursor (Qt::WaitCursor);

    m_mainWindow->setSelectionTransparency (m_oldST, true/*force colour change*/);

    if (doc->selection ())
        doc->selection ()->setTransparency (m_oldST);

    QApplication::restoreOverrideCursor ();
}


/*
 * kpToolSelectionMoveCommand
 */

kpToolSelectionMoveCommand::kpToolSelectionMoveCommand (const QString &name,
                                                        kpMainWindow *mainWindow)
    : kpNamedCommand (name, mainWindow)
{
    kpDocument *doc = document ();
    if (doc && doc->selection ())
    {
        m_startPoint = m_endPoint = doc->selection ()->topLeft ();
    }
}

kpToolSelectionMoveCommand::~kpToolSelectionMoveCommand ()
{
}


// public
kpSelection kpToolSelectionMoveCommand::originalSelection () const
{
    kpDocument *doc = document ();
    if (!doc || !doc->selection ())
    {
        kError () << "kpToolSelectionMoveCommand::originalSelection() doc="
                   << doc
                   << " sel="
                   << (doc ? doc->selection () : 0)
                   << endl;
        return kpSelection (kpSelection::Rectangle, QRect ());
    }

    kpSelection selection = *doc->selection();
    selection.moveTo (m_startPoint);

    return selection;
}


// public virtual [base kpComand]
int kpToolSelectionMoveCommand::size () const
{
    return kpPixmapFX::pixmapSize (m_oldDocumentPixmap) +
           kpPixmapFX::pointArraySize (m_copyOntoDocumentPoints);
}


// public virtual [base kpCommand]
void kpToolSelectionMoveCommand::execute ()
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelectionMoveCommand::execute()" << endl;
#endif

    Q_ASSERT (m_mainWindow);

    kpDocument *doc = document ();
    Q_ASSERT (doc);

    kpSelection *sel = doc->selection ();

    // have to have pulled pixmap by now
    if (!sel || !sel->pixmap ())
    {
        kError () << "kpToolSelectionMoveCommand::execute() but haven't pulled pixmap yet: "
                   << "sel=" << sel << " sel->pixmap=" << (sel ? sel->pixmap () : 0)
                   << endl;
        return;
    }

    kpViewManager *vm = m_mainWindow->viewManager ();
    Q_ASSERT (vm);

    vm->setQueueUpdates ();

    QPolygon::ConstIterator copyOntoDocumentPointsEnd = m_copyOntoDocumentPoints.end ();
    for (QPolygon::ConstIterator it = m_copyOntoDocumentPoints.begin ();
         it != copyOntoDocumentPointsEnd;
         it++)
    {
        sel->moveTo (*it);
        doc->selectionCopyOntoDocument ();
    }

    sel->moveTo (m_endPoint);

    if (m_mainWindow->tool ())
        m_mainWindow->tool ()->somethingBelowTheCursorChanged ();

    vm->restoreQueueUpdates ();
}

// public virtual [base kpCommand]
void kpToolSelectionMoveCommand::unexecute ()
{
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "kpToolSelectionMoveCommand::unexecute()" << endl;
#endif

    Q_ASSERT (m_mainWindow);

    kpDocument *doc = document ();
    Q_ASSERT (doc);

    kpSelection *sel = doc->selection ();

    // have to have pulled pixmap by now
    if (!sel || !sel->pixmap ())
    {
        kError () << "kpToolSelectionMoveCommand::unexecute() but haven't pulled pixmap yet: "
                   << "sel=" << sel << " sel->pixmap=" << (sel ? sel->pixmap () : 0)
                   << endl;
        return;
    }

    kpViewManager *vm = m_mainWindow->viewManager ();
    Q_ASSERT (vm);

    vm->setQueueUpdates ();

    if (!m_oldDocumentPixmap.isNull ())
        doc->setPixmapAt (m_oldDocumentPixmap, m_documentBoundingRect.topLeft ());
#if DEBUG_KP_TOOL_SELECTION && 1
    kDebug () << "\tmove to startPoint=" << m_startPoint << endl;
#endif
    sel->moveTo (m_startPoint);

    if (m_mainWindow->tool ())
        m_mainWindow->tool ()->somethingBelowTheCursorChanged ();

    vm->restoreQueueUpdates ();
}

// public
void kpToolSelectionMoveCommand::moveTo (const QPoint &point, bool moveLater)
{
#if DEBUG_KP_TOOL_SELECTION && 0
    kDebug () << "kpToolSelectionMoveCommand::moveTo" << point
               << " moveLater=" << moveLater
               <<endl;
#endif

    if (!moveLater)
    {
        kpDocument *doc = document ();
        if (!doc)
        {
            kError () << "kpToolSelectionMoveCommand::moveTo() without doc" << endl;
            return;
        }

        kpSelection *sel = doc->selection ();

        // have to have pulled pixmap by now
        if (!sel)
        {
            kError () << "kpToolSelectionMoveCommand::moveTo() no sel region" << endl;
            return;
        }

        if (!sel->pixmap ())
        {
            kError () << "kpToolSelectionMoveCommand::moveTo() no sel pixmap" << endl;
            return;
        }

        if (point == sel->topLeft ())
            return;

        sel->moveTo (point);
    }

    m_endPoint = point;
}

// public
void kpToolSelectionMoveCommand::moveTo (int x, int y, bool moveLater)
{
    moveTo (QPoint (x, y), moveLater);
}

// public
void kpToolSelectionMoveCommand::copyOntoDocument ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelectionMoveCommand::copyOntoDocument()" << endl;
#endif

    kpDocument *doc = document ();
    if (!doc)
        return;

    kpSelection *sel = doc->selection ();

    // have to have pulled pixmap by now
    if (!sel)
    {
        kError () << "\tkpToolSelectionMoveCommand::copyOntoDocument() without sel region" << endl;
        return;
    }

    if (!sel->pixmap ())
    {
        kError () << "kpToolSelectionMoveCommand::moveTo() no sel pixmap" << endl;
        return;
    }

    if (m_oldDocumentPixmap.isNull ())
        m_oldDocumentPixmap = *doc->pixmap ();

    QRect selBoundingRect = sel->boundingRect ();
    m_documentBoundingRect.unite (selBoundingRect);

    doc->selectionCopyOntoDocument ();

    m_copyOntoDocumentPoints.putPoints (m_copyOntoDocumentPoints.count (),
                                        1,
                                        selBoundingRect.x (),
                                        selBoundingRect.y ());
}

// public
void kpToolSelectionMoveCommand::finalize ()
{
    if (!m_oldDocumentPixmap.isNull () && !m_documentBoundingRect.isNull ())
    {
        m_oldDocumentPixmap = kpTool::neededPixmap (m_oldDocumentPixmap,
                                                    m_documentBoundingRect);
    }
}


/*
 * kpToolSelectionResizeScaleCommand
 */

kpToolSelectionResizeScaleCommand::kpToolSelectionResizeScaleCommand (
        kpMainWindow *mainWindow)
    : kpNamedCommand (mainWindow->document ()->selection ()->isText () ?
                         i18n ("Text: Resize Box") :
                         i18n ("Selection: Smooth Scale"),
                      mainWindow),
      m_smoothScaleTimer (new QTimer (this))
{
    m_originalSelection = *selection ();

    m_newTopLeft = selection ()->topLeft ();
    m_newWidth = selection ()->width ();
    m_newHeight = selection ()->height ();

    m_smoothScaleTimer->setSingleShot (true);
    connect (m_smoothScaleTimer, SIGNAL (timeout ()),
             this, SLOT (resizeScaleAndMove ()));
}

kpToolSelectionResizeScaleCommand::~kpToolSelectionResizeScaleCommand ()
{
}


// public virtual
int kpToolSelectionResizeScaleCommand::size () const
{
    return m_originalSelection.size ();
}


// public
kpSelection kpToolSelectionResizeScaleCommand::originalSelection () const
{
    return m_originalSelection;
}


// public
QPoint kpToolSelectionResizeScaleCommand::topLeft () const
{
    return m_newTopLeft;
}

// public
void kpToolSelectionResizeScaleCommand::moveTo (const QPoint &point)
{
    if (point == m_newTopLeft)
        return;

    m_newTopLeft = point;
    selection ()->moveTo (m_newTopLeft);
}


// public
int kpToolSelectionResizeScaleCommand::width () const
{
    return m_newWidth;
}

// public
int kpToolSelectionResizeScaleCommand::height () const
{
    return m_newHeight;
}

// public
void kpToolSelectionResizeScaleCommand::resize (int width, int height,
                                                bool delayed)
{
    if (width == m_newWidth && height == m_newHeight)
        return;

    m_newWidth = width;
    m_newHeight = height;

    resizeScaleAndMove (delayed);
}


// public
void kpToolSelectionResizeScaleCommand::resizeAndMoveTo (int width, int height,
                                                         const QPoint &point,
                                                         bool delayed)
{
    if (width == m_newWidth && height == m_newHeight &&
        point == m_newTopLeft)
    {
        return;
    }

    m_newWidth = width;
    m_newHeight = height;
    m_newTopLeft = point;

    resizeScaleAndMove (delayed);
}


// protected
void kpToolSelectionResizeScaleCommand::killSmoothScaleTimer ()
{
    m_smoothScaleTimer->stop ();
}


// protected
void kpToolSelectionResizeScaleCommand::resizeScaleAndMove (bool delayed)
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelectionResizeScaleCommand::resizeScaleAndMove(delayed="
               << delayed << ")" << endl;
#endif

    killSmoothScaleTimer ();

    kpSelection newSel;

    if (selection ()->isText ())
    {
        newSel = m_originalSelection;
        newSel.textResize (m_newWidth, m_newHeight);
    }
    else
    {
        newSel = kpSelection (kpSelection::Rectangle,
            QRect (m_originalSelection.x (),
                   m_originalSelection.y (),
                   m_newWidth,
                   m_newHeight),
            kpPixmapFX::scale (*m_originalSelection.pixmap (),
                               m_newWidth, m_newHeight,
                               !delayed/*if not delayed, smooth*/),
            m_originalSelection.transparency ());

        if (delayed)
        {
            // Call self (once) with delayed==false in 200ms
            m_smoothScaleTimer->start (200/*ms*/);
        }
    }

    newSel.moveTo (m_newTopLeft);

    m_mainWindow->document ()->setSelection (newSel);
}

// protected slots
void kpToolSelectionResizeScaleCommand::resizeScaleAndMove ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelectionResizeScaleCommand::resizeScaleAndMove()" << endl;
#endif
    resizeScaleAndMove (false/*no delay*/);
}


// public virtual [base kpToolResizeScaleCommand]
void kpToolSelectionResizeScaleCommand::execute ()
{
    QApplication::setOverrideCursor (Qt::WaitCursor);

    killSmoothScaleTimer ();

    resizeScaleAndMove ();

    if (m_mainWindow->tool ())
        m_mainWindow->tool ()->somethingBelowTheCursorChanged ();

    QApplication::restoreOverrideCursor ();
}

// public virtual [base kpToolResizeScaleCommand]
void kpToolSelectionResizeScaleCommand::unexecute ()
{
    QApplication::setOverrideCursor (Qt::WaitCursor);

    killSmoothScaleTimer ();

    m_mainWindow->document ()->setSelection (m_originalSelection);

    if (m_mainWindow->tool ())
        m_mainWindow->tool ()->somethingBelowTheCursorChanged ();

    QApplication::restoreOverrideCursor ();
}


/*
 * kpToolSelectionDestroyCommand
 */

kpToolSelectionDestroyCommand::kpToolSelectionDestroyCommand (const QString &name,
                                                              bool pushOntoDocument,
                                                              kpMainWindow *mainWindow)
    : kpNamedCommand (name, mainWindow),
      m_pushOntoDocument (pushOntoDocument),
      m_oldSelection (0)
{
}

kpToolSelectionDestroyCommand::~kpToolSelectionDestroyCommand ()
{
    delete m_oldSelection;
}


// public virtual [base kpCommand]
int kpToolSelectionDestroyCommand::size () const
{
    return kpPixmapFX::pixmapSize (m_oldDocPixmap) +
           kpPixmapFX::selectionSize (m_oldSelection);
}


// public virtual [base kpCommand]
void kpToolSelectionDestroyCommand::execute ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelectionDestroyCommand::execute () CALLED" << endl;
#endif

    kpDocument *doc = document ();
    if (!doc)
    {
        kError () << "kpToolSelectionDestroyCommand::execute() without doc" << endl;
        return;
    }

    if (!doc->selection ())
    {
        kError () << "kpToolSelectionDestroyCommand::execute() without sel region" << endl;
        return;
    }

    m_textRow = m_mainWindow->viewManager ()->textCursorRow ();
    m_textCol = m_mainWindow->viewManager ()->textCursorCol ();

    m_oldSelection = new kpSelection (*doc->selection ());
    if (m_pushOntoDocument)
    {
        m_oldDocPixmap = doc->getPixmapAt (doc->selection ()->boundingRect ());
        doc->selectionPushOntoDocument ();
    }
    else
        doc->selectionDelete ();

    if (m_mainWindow->tool ())
        m_mainWindow->tool ()->somethingBelowTheCursorChanged ();
}

// public virtual [base kpCommand]
void kpToolSelectionDestroyCommand::unexecute ()
{
#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "kpToolSelectionDestroyCommand::unexecute () CALLED" << endl;
#endif

    kpDocument *doc = document ();
    if (!doc)
    {
        kError () << "kpToolSelectionDestroyCommand::unexecute() without doc" << endl;
        return;
    }

    if (doc->selection ())
    {
        // not error because it's possible that the user dragged out a new
        // region (without pulling pixmap), and then CTRL+Z
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "kpToolSelectionDestroyCommand::unexecute() already has sel region" << endl;
    #endif

        if (doc->selection ()->pixmap ())
        {
            kError () << "kpToolSelectionDestroyCommand::unexecute() already has sel pixmap" << endl;
            return;
        }
    }

    if (!m_oldSelection)
    {
        kError () << "kpToolSelectionDestroyCommand::unexecute() without old sel" << endl;
        return;
    }

    if (m_pushOntoDocument)
    {
    #if DEBUG_KP_TOOL_SELECTION
        kDebug () << "\tunpush oldDocPixmap onto doc first" << endl;
    #endif
        doc->setPixmapAt (m_oldDocPixmap, m_oldSelection->topLeft ());
    }

#if DEBUG_KP_TOOL_SELECTION
    kDebug () << "\tsetting selection to: rect=" << m_oldSelection->boundingRect ()
               << " pixmap=" << m_oldSelection->pixmap ()
               << " pixmap.isNull()=" << (m_oldSelection->pixmap ()
                                              ?
                                          m_oldSelection->pixmap ()->isNull ()
                                              :
                                          true)
               << endl;
#endif
    if (!m_oldSelection->isText ())
    {
        if (m_oldSelection->transparency () != m_mainWindow->selectionTransparency ())
            m_mainWindow->setSelectionTransparency (m_oldSelection->transparency ());
    }
    else
    {
        if (m_oldSelection->textStyle () != m_mainWindow->textStyle ())
            m_mainWindow->setTextStyle (m_oldSelection->textStyle ());
    }

    m_mainWindow->viewManager ()->setTextCursorPosition (m_textRow, m_textCol);
    doc->setSelection (*m_oldSelection);

    if (m_mainWindow->tool ())
        m_mainWindow->tool ()->somethingBelowTheCursorChanged ();

    delete m_oldSelection;
    m_oldSelection = 0;
}


#include <kptoolselection.moc>