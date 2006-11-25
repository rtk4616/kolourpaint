
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


#ifndef kpToolPolygonalCommand_H
#define kpToolPolygonalCommand_H


#include <qbrush.h>
#include <qpen.h>
#include <qobject.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <qpolygon.h>
#include <qrect.h>

#include <kpcolor.h>
#include <kpcommandhistory.h>
#include <kptool.h>
#include <kpToolPolygonalBase.h>
#include <kptoolwidgetfillstyle.h>


class QMouseEvent;
class QPen;
class QPoint;
class QPolygon;
class QRect;
class QString;

class kpView;
class kpDocument;
class kpMainWindow;

class kpToolWidgetFillStyle;
class kpToolWidgetLineWidth;
class kpViewManager;


struct kpToolPolygonalCommandPrivate;

// TODO: merge with kpToolRectangularCommand due to code duplication.
class kpToolPolygonalCommand : public kpNamedCommand
{
public:
    // <boundingRect> = the bounding rectangle for <points> including <penWidth>.
    kpToolPolygonalCommand (const QString &name,
        kpToolPolygonalBase::DrawShapeFunc drawShapeFunc,
        const QPolygon &points,
        const QRect &boundingRect,
        const kpColor &fcolor, int penWidth,
        const kpColor &bcolor,
        kpMainWindow *mainWindow);
    virtual ~kpToolPolygonalCommand ();

    virtual int size () const;
    
    virtual void execute ();
    virtual void unexecute ();

private:
    kpToolPolygonalCommandPrivate * const d;
    kpToolPolygonalCommand &operator= (const kpToolPolygonalCommand &) const;
};


#endif  // kpToolPolygonalCommand_H
