
/* This file is part of the KolourPaint project
   Copyright (c) 2003 Clarence Dang <dang@kde.org>
   All rights reserved.
   
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the names of the copyright holders nor the names of
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define DEBUG_KP_TOOL_PEN 1

#include <qapplication.h>
#include <qbitmap.h>
#include <qcursor.h>
#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>
#if DEBUG_KP_TOOL_PEN
    #include <qdatetime.h>
#endif

#include <kdebug.h>
#include <klocale.h>

#include <kptoolpen.h>
#include <kpdefs.h>
#include <kpdocument.h>
#include <kpmainwindow.h>
#include <kppixmapfx.h>
#include <kptoolclear.h>
#include <kptooltoolbar.h>
#include <kptoolwidgetbrush.h>
#include <kptoolwidgeterasersize.h>
#include <kpviewmanager.h>

/*
 * kpToolPen
 */

kpToolPen::kpToolPen (kpMainWindow *mainWindow)
    : kpTool (i18n ("Pen"), i18n ("Draws dots and freehand strokes"), mainWindow, "tool_pen"),
      m_mode (Pen),
      m_toolWidgetBrush (0),
      m_toolWidgetEraserSize (0),
      m_currentCommand (0)
{
}

void kpToolPen::setMode (Mode mode)
{
    int usesPixmaps = (mode & (DrawsPixmaps | WashesPixmaps));
    int usesBrushes = (mode & (SquareBrushes | DiverseBrushes));
    
    if ((usesPixmaps && !usesBrushes) ||
        (usesBrushes && !usesPixmaps))
    {
        kdError () << "kpToolPen::setMode() passed invalid mode" << endl;
        return;
    }
    
    m_mode = mode;
}

kpToolPen::~kpToolPen ()
{
}


// virtual
void kpToolPen::begin ()
{
    m_toolWidgetBrush = 0;
    m_brushIsDiagonalLine = false;

    kpToolToolBar *tb = toolToolBar ();
    if (!tb)
        return;
    
    if (m_mode & SquareBrushes)
    {
        m_toolWidgetEraserSize = tb->toolWidgetEraserSize ();
        connect (m_toolWidgetEraserSize, SIGNAL (eraserSizeChanged (int)),
                 this, SLOT (slotEraserSizeChanged (int)));
        m_toolWidgetEraserSize->show ();
        
        slotEraserSizeChanged (m_toolWidgetEraserSize->eraserSize ());
        
        viewManager ()->setCursor (QCursor (Qt::CrossCursor));
    }
    
    if (m_mode & DiverseBrushes)
    {
        m_toolWidgetBrush = tb->toolWidgetBrush ();
        connect (m_toolWidgetBrush, SIGNAL (brushChanged (const QPixmap &, bool)),
                 this, SLOT (slotBrushChanged (const QPixmap &, bool)));
        m_toolWidgetBrush->show ();

        slotBrushChanged (m_toolWidgetBrush->brush (),
                          m_toolWidgetBrush->brushIsDiagonalLine ());

        // TODO: light cursor to not obscure brush cursor
        viewManager ()->setCursor (QCursor (Qt::CrossCursor));
    }
}

// virtual
void kpToolPen::end ()
{
    if (m_toolWidgetEraserSize)
    {
        disconnect (m_toolWidgetEraserSize, SIGNAL (eraserSizeChanged (int)),
                    this, SLOT (slotEraserSizeChanged (int)));
        m_toolWidgetEraserSize = 0;
    }
    
    if (m_toolWidgetBrush)
    {
        disconnect (m_toolWidgetBrush, SIGNAL (brushChanged (const QPixmap &, bool)),
                    this, SLOT (slotBrushChanged (const QPixmap &, bool)));
        m_toolWidgetBrush = 0;
    }

    kpViewManager *vm = viewManager ();
    if (vm)
    {
        if (vm->brushActive ())
            vm->invalidateTempPixmap ();
        
        if (m_mode & (SquareBrushes | DiverseBrushes))
            vm->unsetCursor ();
    }
    
    // save memory
    for (int i = 0; i < 2; i++)
        m_brushPixmap [i].resize (0, 0);
    m_cursorPixmap.resize (0, 0);
}

// virtual
void kpToolPen::beginDraw ()
{
    switch (m_mode)
    {
    case Pen:
        m_currentCommand = new kpToolPenCommand (i18n ("Pen"), document (), viewManager ());
        break;
    case Brush:
        m_currentCommand = new kpToolPenCommand (i18n ("Brush"), document (), viewManager ());
        break;
    case Eraser:
        m_currentCommand = new kpToolPenCommand (i18n ("Eraser"), document (), viewManager ());
        break;
    case ColorWasher:
        m_currentCommand = new kpToolPenCommand (i18n ("Color Eraser"), document (), viewManager ());
        break;

    default:
        m_currentCommand = new kpToolPenCommand (i18n ("Custom Pen or Brush"), document (), viewManager ());
        break;
    }
    
    // we normally show the Brush pix in the foreground colour but if the
    // user starts drawing in the background color, we don't want to leave
    // the cursor in the foreground colour -- just hide it in all cases
    // to avoid confusion
    viewManager ()->invalidateTempPixmap ();
}

// virtual
void kpToolPen::hover (const QPoint &point)
{
    if (!m_cursorPixmap.isNull ())
    {
        m_mouseButton = 0;

        viewManager ()->setFastUpdates ();
        // TODO: m_currentPoint/hotPoint() is uninitialised when ::begin() is called
        viewManager ()->setTempPixmapAt (m_cursorPixmap, hotPoint (), kpViewManager::BrushPixmap);
        viewManager ()->restoreFastUpdates ();
    }

#if DEBUG_KP_TOOL_PEN && 0
    if (document ()->rect ().contains (point))
    {
        QImage image = kpPixmapFX::convertToImage (*document ()->pixmap ());

        QRgb v = image.pixel (point.x (), point.y ());
        kdDebug () << "(" << point << "): r=" << qRed (v)
                    << " g=" << qGreen (v)
                    << " b=" << qBlue (v)
                    << " a=" << qAlpha (v)
                    << endl;
    }
#endif
    
    emit mouseMoved (point);
}

bool kpToolPen::wash (QPainter *painter, QPainter *maskPainter,
                      const QImage &image,
                      const QColor &colorToReplace,
                      const QRect &imageRect, int plotx, int ploty)
{
    return wash (painter, maskPainter, image, colorToReplace, imageRect, hotRect (plotx, ploty));
}
    
bool kpToolPen::wash (QPainter *painter, QPainter *maskPainter,
                      const QImage &image,
                      const QColor &colorToReplace,
                      const QRect &imageRect, const QRect &drawRect)
{
    bool didSomething = false;

#if DEBUG_KP_TOOL_PEN && 0
    kdDebug () << "kpToolPen::wash(imageRect=" << imageRect
               << ",drawRect=" << drawRect
               << ")" << endl;
#endif

// make use of scanline coherence
#define FLUSH_LINE()                                     \
{                                                        \
    if (painter && painter->isActive ())                 \
        painter->drawLine (startDrawX, y, x - 1, y);     \
    if (maskPainter && maskPainter->isActive ())         \
        maskPainter->drawLine (startDrawX, y, x - 1, y); \
    didSomething = true;                                 \
    startDrawX = -1;                                     \
}
    
    const int maxY = drawRect.bottom () - imageRect.top ();
    
    const int minX = drawRect.left () - imageRect.left ();
    const int maxX = drawRect.right () - imageRect.left ();
        
    for (int y = drawRect.top () - imageRect.top ();
         y <= maxY;
         y++)
    {
        int startDrawX = -1;
        
        int x;  // for FLUSH_LINE()
        for (x = minX; x <= maxX; x++)
        {
            if (kpTool::colorEq (kpPixmapFX::getColorAtPixel (image, QPoint (x, y)), colorToReplace))
            {
                if (startDrawX < 0)
                    startDrawX = x;
            }
            else
            {
                if (startDrawX >= 0)
                    FLUSH_LINE ();
            }
        }
        
        if (startDrawX >= 0)
            FLUSH_LINE ();
    }
    
#undef FLUSH_LINE
    
    return didSomething;
}

// virtual
void kpToolPen::globalDraw ()
{
    // it's easiest to reimplement globalDraw() here rather than in
    // all the relevant subclasses

    if (m_mode == Eraser)
    {
    #if DEBUG_KP_TOOL_PEN
        kdDebug () << "kpToolPen::globalDraw() eraser" << endl;
    #endif
        mainWindow ()->commandHistory ()->addCommand (
            new kpToolClearCommand (document (), viewManager (),
                                    backgroundColor ()));
    }
    else if (m_mode == ColorWasher)
    {
    #if DEBUG_KP_TOOL_PEN
        kdDebug () << "kpToolPen::globalDraw() colour eraser" << endl;
    #endif
        if (kpTool::colorEq (foregroundColor (), backgroundColor ()))
            return;
        
        QApplication::setOverrideCursor (Qt::waitCursor);

        kpToolPenCommand *cmd = new kpToolPenCommand (
            i18n ("Color Eraser"), document (), viewManager ());
        
        QPainter painter, maskPainter;
        QBitmap maskBitmap;

        if (kpTool::isColorOpaque (backgroundColor ()))
        {
            painter.begin (document ()->pixmap ());
            painter.setPen (backgroundColor ());
        }
        
        if (kpTool::isColorTransparent (backgroundColor ()) ||
            document ()->pixmap ()->mask ())
        {
            maskBitmap = kpPixmapFX::getNonNullMask (*document ()->pixmap ());
            maskPainter.begin (&maskBitmap);

            if (kpTool::isColorTransparent (backgroundColor ()))
                maskPainter.setPen (Qt::color0/*transparent*/);
            else
                maskPainter.setPen (Qt::color1/*opaque*/);
        }
        
        const QImage image = kpPixmapFX::convertToImage (*document ()->pixmap ());
        QRect rect = document ()->rect ();
        
        if (wash (&painter, &maskPainter, image,
                  foregroundColor ()/*replace foreground*/,
                  rect, rect))
        {
            // flush
            if (painter.isActive ())
                painter.end ();
                
            if (maskPainter.isActive ())
                maskPainter.end ();
                
            if (!maskBitmap.isNull ())
                document ()->pixmap ()->setMask (maskBitmap);
            

            // OPT: wash could return rect
            document ()->slotContentsChanged (rect);


            cmd->updateBoundingRect (rect);
            cmd->finalize ();

            mainWindow ()->commandHistory ()->addCommand (cmd, false /* don't exec */);
            
            // don't delete - it's up to the commandHistory
            cmd = 0;
        }
        else
        {
        #if DEBUG_KP_TOOL_PEN
            kdDebug () << "\tisNOP" << endl;
        #endif
            delete cmd;
            cmd = 0;
        }
        
        QApplication::restoreOverrideCursor ();
    }
}

// virtual
void kpToolPen::draw (const QPoint &thisPoint, const QPoint &lastPoint, const QRect &)
{
    if ((m_mode & WashesPixmaps) && (kpTool::colorEq (foregroundColor (), backgroundColor ())))
        return;
 
    // sync: remember to restoreFastUpdates() in all exit paths
    viewManager ()->setFastUpdates ();

    if (m_brushIsDiagonalLine ? currentPointCardinallyNextToLast () : currentPointNextToLast ())
    {
        if (m_mode & DrawsPixels)
        {
            QPixmap pixmap (1, 1);

            const QColor c = color (m_mouseButton);
            
            // OPT: this seems hopelessly inefficient
            if (kpTool::isColorOpaque (c))
            {
                pixmap.fill (c);
            }
            else
            {
                QBitmap mask (1, 1);
                mask.fill (Qt::color0/*transparent*/);
                
                pixmap.setMask (mask);
            }

            // draw onto doc
            document ()->setPixmapAt (pixmap, thisPoint);

            m_currentCommand->updateBoundingRect (thisPoint);
        }
        // Brush & Eraser
        else if (m_mode & DrawsPixmaps)
        {
            if (kpTool::isColorOpaque (color (m_mouseButton)))
                document ()->paintPixmapAt (m_brushPixmap [m_mouseButton], hotPoint ());
            else
            {
                kpPixmapFX::paintMaskTransparentWithBrush (document ()->pixmap (),
                    hotPoint (),
                    kpPixmapFX::getNonNullMask (m_brushPixmap [m_mouseButton]));
                document ()->slotContentsChanged (hotRect ());
            }

            m_currentCommand->updateBoundingRect (hotRect ());
        }
        else if (m_mode & WashesPixmaps)
        {
        #if DEBUG_KP_TOOL_PEN
            kdDebug () << "Washing pixmap (immediate)" << endl;
            QTime timer;
        #endif
            QRect rect = hotRect ();
        #if DEBUG_KP_TOOL_PEN
            timer.start ();
        #endif
            QPixmap pixmap = document ()->getPixmapAt (rect);
        #if DEBUG_KP_TOOL_PEN
            kdDebug () << "\tget from doc: " << timer.restart () << "ms" << endl;
        #endif
            const QImage image = kpPixmapFX::convertToImage (pixmap);
        #if DEBUG_KP_TOOL_PEN
            kdDebug () << "\tconvert to image: " << timer.restart () << "ms" << endl;
        #endif
            QPainter painter, maskPainter;
            QBitmap maskBitmap;
            
            if (kpTool::isColorOpaque (color (m_mouseButton)))
            {
                painter.begin (&pixmap);
                painter.setPen (color (m_mouseButton));
            }
            
            if (kpTool::isColorTransparent (color (m_mouseButton)) ||
                pixmap.mask ())
            {
                maskBitmap = kpPixmapFX::getNonNullMask (pixmap);
                maskPainter.begin (&maskBitmap);

                if (kpTool::isColorTransparent (color (m_mouseButton)))
                    maskPainter.setPen (Qt::color0/*transparent*/);
                else
                    maskPainter.setPen (Qt::color1/*opaque*/);
            }
            
            bool didSomething = wash (&painter, &maskPainter,
                                      image,
                                      color (1 - m_mouseButton)/*color to replace*/,
                                      rect, rect);
            
            if (painter.isActive ())
                painter.end ();
                
            if (maskPainter.isActive ())
                maskPainter.end ();

            if (didSomething)
            {
                if (!maskBitmap.isNull ())
                    pixmap.setMask (maskBitmap);

            #if DEBUG_KP_TOOL_PEN
                kdDebug () << "\twashed: " << timer.restart () << "ms" << endl;
            #endif
                document ()->setPixmapAt (pixmap, hotPoint ());
            #if DEBUG_KP_TOOL_PEN
                kdDebug () << "\tset doc: " << timer.restart () << "ms" << endl;
            #endif
                m_currentCommand->updateBoundingRect (hotRect ());
            #if DEBUG_KP_TOOL_PEN
                kdDebug () << "\tupdate boundingRect: " << timer.restart () << "ms" << endl;
                kdDebug () << "\tdone" << endl;
            #endif
            }
            
        #if DEBUG_KP_TOOL_PEN && 1
            kdDebug () << endl;
        #endif
        }
    }
    // in reality, the system is too slow to give us all the MouseMove events
    // so we "interpolate" the missing points :)
    else
    {
        // find bounding rectangle
        QRect rect = QRect (thisPoint, lastPoint).normalize ();
        if (m_mode != DrawsPixels)
            rect = neededRect (rect, m_brushPixmap [m_mouseButton].width ());

    #if DEBUG_KP_TOOL_PEN
        if (m_mode & WashesPixmaps)
        {
            kdDebug () << "Washing pixmap (w=" << rect.width ()
                       << ",h=" << rect.height () << ")" << endl;
        }
        QTime timer;
        int convAndWashTime;
    #endif

        const QColor c = color (m_mouseButton);
        bool transparent = kpTool::isColorTransparent (c);    
        
        QPixmap pixmap = document ()->getPixmapAt (rect);
        QBitmap maskBitmap;
         
        QPainter painter, maskPainter;

        if (m_mode & (DrawsPixels | WashesPixmaps))
        {
            if (!transparent)
            {
                painter.begin (&pixmap);
                painter.setPen (c);
            }
         
            if (transparent || pixmap.mask ())
            {
                maskBitmap = kpPixmapFX::getNonNullMask (pixmap);
                maskPainter.begin (&maskBitmap);
                
                if (transparent)
                    maskPainter.setPen (Qt::color0/*transparent*/);
                else
                    maskPainter.setPen (Qt::color1/*opaque*/);
            }
        }            
        
        QImage image;
        if (m_mode & WashesPixmaps)
        {
        #if DEBUG_KP_TOOL_PEN
            timer.start ();
        #endif
            image = kpPixmapFX::convertToImage (pixmap);
        #if DEBUG_KP_TOOL_PEN
            convAndWashTime = timer.restart ();
            kdDebug () << "\tconvert to image: " << convAndWashTime << " ms" << endl;    
        #endif
        }
        
        bool didSomething = false;

        if (m_mode & DrawsPixels)
        {
            QPoint sp = lastPoint - rect.topLeft (), ep = thisPoint - rect.topLeft ();
            if (painter.isActive ())
                painter.drawLine (sp, ep);
                
            if (maskPainter.isActive ())
                maskPainter.drawLine (sp, ep);
            
            didSomething = true;
        }
        // Brush & Eraser
        else if (m_mode & (DrawsPixmaps | WashesPixmaps))
        {
            QColor colorToReplace;
            
            if (m_mode & WashesPixmaps)
                colorToReplace = color (1 - m_mouseButton);

            // Sweeps a pixmap along a line (modified Bresenham's line algorithm,
            // see MODIFIED comment below).
            //
            // Derived from the zSprite2 Graphics Engine

            const int x1 = (thisPoint - rect.topLeft ()).x (),
                      y1 = (thisPoint - rect.topLeft ()).y (),
                      x2 = (lastPoint - rect.topLeft ()).x (),
                      y2 = (lastPoint - rect.topLeft ()).y ();

            // Difference of x and y values
            int dx = x2 - x1;
            int dy = y2 - y1;

            // Absolute values of differences
            int ix = kAbs (dx);
            int iy = kAbs (dy);

            // Larger of the x and y differences
            int inc = ix > iy ? ix : iy;

            // Plot location
            int plotx = x1;
            int ploty = y1;

            int x = 0;
            int y = 0;

            if (m_mode & WashesPixmaps)
            {
                if (wash (&painter, &maskPainter, image,
                          colorToReplace,
                          rect, plotx + rect.left (), ploty + rect.top ()))
                {
                    didSomething = true;
                }
            }
            else
            {
                if (!transparent)
                {
                    kpPixmapFX::paintPixmapAt (&pixmap,
                        hotPoint (plotx, ploty),
                        m_brushPixmap [m_mouseButton]);
                }
                else
                {
                    kpPixmapFX::paintMaskTransparentWithBrush (&pixmap,
                        hotPoint (plotx, ploty),
                        kpPixmapFX::getNonNullMask (m_brushPixmap [m_mouseButton]));
                }
                
                didSomething = true;
            }

            for (int i = 0; i <= inc; i++)
            {
                // oldplotx is equally as valid but would look different
                // (but nobody will notice which one it is)
                int oldploty = ploty;
                int plot = 0;

                x += ix;
                y += iy;

                if (x > inc)
                {
                    plot++;
                    x -= inc;

                    if (dx < 0)
                        plotx--;
                    else
                        plotx++;
                }

                if (y > inc)
                {
                    plot++;
                    y -= inc;

                    if (dy < 0)
                        ploty--;
                    else
                        ploty++;
                }

                if (plot)
                {
                    if (m_brushIsDiagonalLine && plot == 2)
                    {
                        // MODIFIED: every point is
                        // horizontally or vertically adjacent to another point (if there
                        // is more than 1 point, of course).  This is in contrast to the
                        // ordinary line algorithm which can create diagonal adjacencies.

                        if (m_mode & WashesPixmaps)
                        {
                            if (wash (&painter, &maskPainter, image,
                                      colorToReplace,
                                      rect, plotx + rect.left (), oldploty + rect.top ()))
                            {
                                didSomething = true;
                            }
                        }
                        else
                        {
                            if (!transparent)
                            {
                                kpPixmapFX::paintPixmapAt (&pixmap,
                                    hotPoint (plotx, oldploty),
                                    m_brushPixmap [m_mouseButton]);
                            }
                            else
                            {
                                kpPixmapFX::paintMaskTransparentWithBrush (&pixmap,
                                    hotPoint (plotx, oldploty),
                                    kpPixmapFX::getNonNullMask (m_brushPixmap [m_mouseButton]));
                            }

                            didSomething = true;
                        }
                    }

                    if (m_mode & WashesPixmaps)
                    {
                        if (wash (&painter, &maskPainter, image,
                                  colorToReplace,
                                  rect, plotx + rect.left (), ploty + rect.top ()))
                        {
                            didSomething = true;
                        }
                    }
                    else
                    {
                        if (!transparent)
                        {
                            kpPixmapFX::paintPixmapAt (&pixmap,
                                hotPoint (plotx, ploty),
                                m_brushPixmap [m_mouseButton]);
                        }
                        else
                        {
                            kpPixmapFX::paintMaskTransparentWithBrush (&pixmap,
                                hotPoint (plotx, ploty),
                                kpPixmapFX::getNonNullMask (m_brushPixmap [m_mouseButton]));
                        }
                        
                        didSomething = true;
                    }
                }
            }

        }

        if (painter.isActive ())
            painter.end ();

        if (maskPainter.isActive ())
            maskPainter.end ();

    #if DEBUG_KP_TOOL_PEN
        if (m_mode & WashesPixmaps)
        {
            int ms = timer.restart ();
            kdDebug () << "\ttried to wash: " << ms << "ms"
                       << " (" << (ms ? (rect.width () * rect.height () / ms) : -1234)
                       << " pixels/ms)"
                       << endl;
            convAndWashTime += ms;
        }
    #endif
            

        if (didSomething)
        {
            if (!maskBitmap.isNull ())
                pixmap.setMask (maskBitmap);

            // draw onto doc
            document ()->setPixmapAt (pixmap, rect.topLeft ());

        #if DEBUG_KP_TOOL_PEN
            if (m_mode & WashesPixmaps)
            {
                int ms = timer.restart ();
                kdDebug () << "\tset doc: " << ms << "ms" << endl;
                convAndWashTime += ms;
            }
        #endif
            
            m_currentCommand->updateBoundingRect (rect);
            
        #if DEBUG_KP_TOOL_PEN
            if (m_mode & WashesPixmaps)
            {
                int ms = timer.restart ();
                kdDebug () << "\tupdate boundingRect: " << ms << "ms" << endl;
                convAndWashTime += ms;
                kdDebug () << "\tdone (" << (convAndWashTime ? (rect.width () * rect.height () / convAndWashTime) : -1234)
                           << " pixels/ms)"
                           << endl;
            }
        #endif
        }
        
    #if DEBUG_KP_TOOL_PEN
        if (m_mode & WashesPixmaps)
            kdDebug () << endl;
    #endif
    }

    viewManager ()->restoreFastUpdates ();
    emit mouseMoved (thisPoint);
}

// virtual
void kpToolPen::cancelShape ()
{
#if 0
    endDraw (QPoint (), QRect ());
    mainWindow ()->commandHistory ()->undo ();
#else
    // prevent Brush Cursor at starting pos from flickering
    viewManager ()->invalidateTempPixmap ();
    m_currentCommand->cancel ();

    delete m_currentCommand;
    m_currentCommand = 0;
#endif

    updateBrushCursor (false/*no recalc*/);
}

// virtual
void kpToolPen::endDraw (const QPoint &, const QRect &)
{
    m_currentCommand->finalize ();
    mainWindow ()->commandHistory ()->addCommand (m_currentCommand, false /* don't exec */);

    // don't delete - it's up to the commandHistory
    m_currentCommand = 0;
    
    updateBrushCursor (false/*no recalc*/);
}


// TODO: maybe the base should be virtual?
QColor kpToolPen::color (int which)
{
#if DEBUG_KP_TOOL_PEN && 0
    kdDebug () << "kpToolPen::color (" << which << ")" << endl;
#endif

    // Pen & Brush
    if ((m_mode & SwappedColors) == 0)
        return kpTool::color (which);
    // only the (Color) Eraser uses the opposite color
    else
        return kpTool::color (which ? 0 : 1);  // don't trust !0 == 1
}

// virtual private slot
void kpToolPen::slotForegroundColorChanged (const QColor &col)
{
#if DEBUG_KP_TOOL_PEN
    kdDebug () << "kpToolPen::slotForegroundColorChanged()" << endl;
#endif
    m_brushPixmap [(m_mode & SwappedColors) ? 1 : 0].fill (col);

    updateBrushCursor ();
}

// virtual private slot
void kpToolPen::slotBackgroundColorChanged (const QColor &col)
{
#if DEBUG_KP_TOOL_PEN
    kdDebug () << "kpToolPen::slotBackgroundColorChanged()" << endl;
#endif
    m_brushPixmap [(m_mode & SwappedColors) ? 0 : 1].fill (col);
    
    updateBrushCursor ();
}

// private slot
void kpToolPen::slotBrushChanged (const QPixmap &pixmap, bool isDiagonalLine)
{
#if DEBUG_KP_TOOL_PEN
    kdDebug () << "kpToolPen::slotBrushChanged()" << endl;
#endif
    for (int i = 0; i < 2; i++)
    {
        m_brushPixmap [i] = pixmap;
        m_brushPixmap [i].fill (color (i));
    }
    
    m_brushIsDiagonalLine = isDiagonalLine;

    updateBrushCursor ();
}

// private slot
void kpToolPen::slotEraserSizeChanged (int size)
{
#if DEBUG_KP_TOOL_PEN
    kdDebug () << "KpToolPen::slotEraserSizeChanged(size=" << size << ")" << endl;
#endif

    for (int i = 0; i < 2; i++)
    {
        m_brushPixmap [i].resize (size, size);
        m_brushPixmap [i].fill (color (i));
    }

    updateBrushCursor ();
}

QPoint kpToolPen::hotPoint () const
{
    return hotPoint (m_currentPoint);
}

QPoint kpToolPen::hotPoint (int x, int y) const
{
    return hotPoint (QPoint (x, y));
}

QPoint kpToolPen::hotPoint (const QPoint &point) const
{
    /*
     * e.g.
     *    Width 5:
     *    0 1 2 3 4
     *        ^
     *        |
     *      Center
     */
    return point -
           QPoint (m_brushPixmap [m_mouseButton].width () / 2,
                   m_brushPixmap [m_mouseButton].height () / 2);
}

QRect kpToolPen::hotRect () const
{
    return hotRect (m_currentPoint);
}

QRect kpToolPen::hotRect (int x, int y) const
{
    return hotRect (QPoint (x, y));
}

QRect kpToolPen::hotRect (const QPoint &point) const
{
    QPoint topLeft = hotPoint (point);
    return QRect (topLeft.x (),
                  topLeft.y (),
                  m_brushPixmap [m_mouseButton].width (),
                  m_brushPixmap [m_mouseButton].height ());
}

// private
void kpToolPen::updateBrushCursor (bool recalc)
{
    if (recalc)
    {
        if (m_mode & SquareBrushes)
            m_cursorPixmap = m_toolWidgetEraserSize->cursorPixmap (color (0));
        else if (m_mode & DiverseBrushes)
            m_cursorPixmap = m_brushPixmap [0];
    }

    hover (m_currentPoint);
}


/*
 * kpToolPenCommand
 */

kpToolPenCommand::kpToolPenCommand (const QString &name, kpDocument *document, kpViewManager *viewManager)
    : m_name (name),
      m_document (document),
      m_viewManager (viewManager)
{
    m_pixmap = *document->pixmap ();
}

kpToolPenCommand::~kpToolPenCommand ()
{
}

// virtual
void kpToolPenCommand::execute ()
{
    swapOldAndNew ();
}

// virtual
void kpToolPenCommand::unexecute ()
{
    swapOldAndNew ();
}

void kpToolPenCommand::swapOldAndNew ()
{
    if (m_boundingRect.isValid ())
    {
        QPixmap oldPixmap = m_document->getPixmapAt (m_boundingRect);
        
        m_document->setPixmapAt (m_pixmap, m_boundingRect.topLeft ());

        m_pixmap = oldPixmap;
    }
}

void kpToolPenCommand::updateBoundingRect (const QPoint &point)
{
    updateBoundingRect (QRect (point, point));
}

void kpToolPenCommand::updateBoundingRect (const QRect &rect)
{
#if DEBUG_KP_TOOL_PEN & 0
    kdDebug () << "kpToolPenCommand::updateBoundingRect()  existing="
               << m_boundingRect
               << " plus="
               << rect
               << endl;
#endif
    m_boundingRect = m_boundingRect.unite (rect);
#if DEBUG_KP_TOOL_PEN & 0
    kdDebug () << "\tresult=" << m_boundingRect << endl;
#endif
}

void kpToolPenCommand::finalize ()
{
    if (m_boundingRect.isValid ())
    {
        // store only needed part of doc pixmap
        m_pixmap = kpTool::neededPixmap (m_pixmap, m_boundingRect);
    }
    else
    {
        m_pixmap.resize (0, 0);
    }
}

void kpToolPenCommand::cancel ()
{
    if (m_boundingRect.isValid ())
    {
        m_document->setPixmapAt (m_pixmap, QPoint (0, 0));
    }
}

#include <kptoolpen.moc>
