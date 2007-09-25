
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


#ifndef kpColorCells_H
#define kpColorCells_H


#include <KUrl>

#include <kpColorCellsBase.h>


class QDropEvent;

class kpColorCollection;

class kpColor;


// TODO: For now, only horizontal orientation is supported.
class kpColorCells : public kpColorCellsBase
{
Q_OBJECT

public:
    kpColorCells (QWidget *parent,
                  Qt::Orientation o = Qt::Horizontal);
    virtual ~kpColorCells ();

    static kpColorCollection DefaultColorCollection ();

    Qt::Orientation orientation () const;
    void setOrientation (Qt::Orientation o);

protected:
    void makeCellsMatchColorCollection ();

public:
    bool isModified () const;
    // (this emits isModifiedChanged() if the modified state changes)
    void setModified (bool yes);
public slots:
    // (this emits isModifiedChanged() if the modified state changes)
    void setModified ();

public:
    KUrl url () const;

    const kpColorCollection *colorCollection () const;
  
private:
    // Ensures there's a least one row of cells, to avoid a confusing UI.
    void ensureHaveAtLeastOneRow ();
public:
    void setColorCollection (const kpColorCollection &colorCol,
        const KUrl &url = KUrl ());

    bool openColorCollection (const KUrl &url);
    bool saveColorCollectionAs (const KUrl &url);
    bool saveColorCollection ();

    void appendRow ();
    void deleteLastRow ();

signals:
    void foregroundColorChanged (const kpColor &color);
    void backgroundColorChanged (const kpColor &color);

    void rowCountChanged (int rowCount);

    // Emitted when setModified() is called and the modified state changes.
    // It may be called at other times, even when the modified state did
    // not change.
    void isModifiedChanged (bool isModified);

protected:
    virtual void contextMenuEvent (QContextMenuEvent *e);

protected slots:
    void slotColorSelected (int cell, const QColor &color, Qt::MouseButton button);
    void slotColorDoubleClicked (int cell, const QColor &color);
    void slotColorChanged (int cell, const QColor &color);

private:
    struct kpColorCellsPrivate * const d;
};


#endif  // kpColorCells_H
