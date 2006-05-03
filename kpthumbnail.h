
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


#ifndef KP_THUMBNAIL_H
#define KP_THUMBNAIL_H


#include <q3dockwindow.h>


class QMoveEvent;
class QResizeEvent;

class kpMainWindow;
class kpThumbnailView;


class kpThumbnail : public Q3DockWindow
{
Q_OBJECT

public:
    kpThumbnail (kpMainWindow *parent, const char *name = 0);
    virtual ~kpThumbnail ();

public:
    kpThumbnailView *view () const;
    void setView (kpThumbnailView *view);

public slots:
    void updateCaption ();

    virtual void dock ();

protected slots:
    void slotViewDestroyed ();

protected:
    virtual void resizeEvent (QResizeEvent *e);
    virtual void moveEvent (QMoveEvent *e);

private:
    kpMainWindow *m_mainWindow;
    kpThumbnailView *m_view;
};


#endif  // KP_THUMBNAIL_H
