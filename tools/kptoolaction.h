
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


#ifndef KP_TOOL_ACTION_H
#define KP_TOOL_ACTION_H


#include <kactionclasses.h>

#include <kpsinglekeytriggersaction.h>


class KActionCollection;


// Same as KToggleAction but shows the first single key trigger in the tooltip.
class kpToolAction : public KToggleAction,
                     public kpSingleKeyTriggersActionInterface
{
Q_OBJECT

public:
    kpToolAction (const QString &text,
        const QString &pic, const KShortcut &shortcut,
        const QObject *receiver, const char *slot,
        KActionCollection *ac, const char *name);
    virtual ~kpToolAction ();


signals:
    // Not emitted when toolTip is manually overriden by setToolTip()
    void toolTipChanged (const QString &string);

protected:
    void updateToolTip ();


    //
    // KToggleAction interface
    //

public slots:
    virtual void setText (const QString &text);
    virtual bool setShortcut (const KShortcut &shortcut);


    //
    // kpSingleKeyTriggersActionInterface
    //

public:
    virtual const char *actionName () const;
    virtual KShortcut actionShortcut () const;
    virtual void actionSetShortcut (const KShortcut &shortcut);
};


#endif  // KP_TOOL_ACTION_H
