
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


#ifndef KP_TOOL_AUTO_CROP_H
#define KP_TOOL_AUTO_CROP_H


#include <kpCommandHistory.h>


class QPixmap;
class QRect;

class kpMainWindow;
class kpTransformAutoCropBorder;


class kpTransformAutoCropCommand : public kpNamedCommand
{
public:
    kpTransformAutoCropCommand (bool actOnSelection,
        const kpTransformAutoCropBorder &leftBorder,
        const kpTransformAutoCropBorder &rightBorder,
        const kpTransformAutoCropBorder &topBorder,
        const kpTransformAutoCropBorder &botBorder,
        kpMainWindow *mainWindow);
    virtual ~kpTransformAutoCropCommand ();

    enum NameOptions
    {
        DontShowAccel = 0,
        ShowAccel = 1
    };

    static QString name (bool actOnSelection, int options);

    virtual int size () const;

private:
    void getUndoPixmap (const kpTransformAutoCropBorder &border, QPixmap **pixmap);
    void getUndoPixmaps ();
    void deleteUndoPixmaps ();

public:
    virtual void execute ();
    virtual void unexecute ();

private:
    QRect contentsRect () const;

    struct kpTransformAutoCropCommandPrivate *d;
};


// (returns true on success (even if it did nothing) or false on error)
bool kpTransformAutoCrop (kpMainWindow *mainWindow);


#endif  // KP_TOOL_AUTO_CROP_H