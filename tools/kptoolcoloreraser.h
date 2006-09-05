
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


#ifndef KP_TOOL_COLOR_ERASER_H
#define KP_TOOL_COLOR_ERASER_H


#include <kptoolflowbase.h>


// Color Eraser = Brush that replaces/washes the foreground color with the background color
class kpToolColorEraser : public kpToolFlowBase
{
Q_OBJECT

public:
    kpToolColorEraser (kpMainWindow *mainWindow);
    virtual ~kpToolColorEraser ();


protected:
    bool wash (QPainter *painter, QPainter *maskPainter,
               const QImage &image,
               const kpColor &colorToReplace,
               const QRect &imageRect, int plotx, int ploty);
    bool wash (QPainter *painter, QPainter *maskPainter,
               const QImage &image,
               const kpColor &colorToReplace,
               const QRect &imageRect, const QRect &drawRect);

public:
    virtual void globalDraw ();
    

protected:
    virtual QString haventBegunDrawUserMessage () const;
    
    virtual bool drawShouldProceed (const QPoint &thisPoint,
        const QPoint &lastPoint,
        const QRect &normalizedRect);

    virtual bool haveSquareBrushes () const { return true; }
    virtual bool colorsAreSwapped () const { return true; }
    

    virtual QRect drawLine (const QPoint &thisPoint, const QPoint &lastPoint);
};


#endif  // KP_TOOL_COLOR_ERASER_H
