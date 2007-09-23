
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


#include <kpMainWindow.h>
#include <kpMainWindowPrivate.h>

#include <KAction>
#include <KActionCollection>
#include <KFileDialog>
#include <KMessageBox>
#include <KSelectAction>
#include <KStandardGuiItem>

#include <kpColorCells.h>
#include <kpColorCollection.h>
#include <kpColorToolBar.h>
#include <kpUrlFormatter.h>


static QStringList KDEColorCollectionNames ()
{
    return kpColorCollection::installedCollections ();
}


// private
void kpMainWindow::setupColorsMenuActions ()
{
    KActionCollection *ac = actionCollection ();


    d->actionColorsDefault = ac->addAction ("colors_default");
    d->actionColorsDefault->setText (i18n ("Use KolourPaint Defaults"));
    connect (d->actionColorsDefault, SIGNAL (triggered (bool)),
        SLOT (slotColorsDefault ()));

    d->actionColorsKDE = ac->add <KSelectAction> ("colors_kde");
    d->actionColorsKDE->setText (i18n ("Use KDE's"));
    connect (d->actionColorsKDE, SIGNAL (triggered (QAction *)),
        SLOT (slotColorsKDE ()));
    foreach (QString colName, ::KDEColorCollectionNames ())
        d->actionColorsKDE->addAction (colName);

    d->actionColorsOpen = ac->addAction ("colors_open");
    d->actionColorsOpen->setText (i18n ("&Open..."));
    connect (d->actionColorsOpen, SIGNAL (triggered (bool)),
        SLOT (slotColorsOpen ()));

    d->actionColorsReload = ac->addAction ("colors_reload");
    d->actionColorsReload->setText (i18n ("Reloa&d"));
    connect (d->actionColorsReload, SIGNAL (triggered (bool)),
        SLOT (slotColorsReload ()));


    d->actionColorsSave = ac->addAction ("colors_save");
    d->actionColorsSave->setText (i18n ("&Save"));
    connect (d->actionColorsSave, SIGNAL (triggered (bool)),
        SLOT (slotColorsSave ()));

    d->actionColorsSaveAs = ac->addAction ("colors_save_as");
    d->actionColorsSaveAs->setText (i18n ("Save &As..."));
    connect (d->actionColorsSaveAs, SIGNAL (triggered (bool)),
        SLOT (slotColorsSaveAs ()));


    d->actionColorsAppendRow = ac->addAction ("colors_append_row");
    d->actionColorsAppendRow->setText (i18n ("Append Row"));
    connect (d->actionColorsAppendRow, SIGNAL (triggered (bool)),
        SLOT (slotColorsAppendRow ()));

    d->actionColorsDeleteRow = ac->addAction ("colors_delete_row");
    d->actionColorsDeleteRow->setText (i18n ("Delete Last Row"));
    connect (d->actionColorsDeleteRow, SIGNAL (triggered (bool)),
        SLOT (slotColorsDeleteRow ()));


    enableColorsMenuDocumentActions (false);
}

// private
void kpMainWindow::enableColorsMenuDocumentActions (bool enable)
{
    d->actionColorsDefault->setEnabled (enable);
    d->actionColorsKDE->setEnabled (enable);
    d->actionColorsOpen->setEnabled (enable);
    d->actionColorsReload->setEnabled (enable);

    d->actionColorsSave->setEnabled (enable);
    d->actionColorsSaveAs->setEnabled (enable);

    d->actionColorsAppendRow->setEnabled (enable);
    d->actionColorsDeleteRow->setEnabled (enable);
}


// private
void kpMainWindow::deselectActionColorsKDE ()
{
    d->actionColorsKDE->setCurrentItem (-1);
}


// private
bool kpMainWindow::queryCloseColors ()
{
#if DEBUG_KP_MAIN_WINDOW || 1
    kDebug () << "kpMainWindow::queryCloseColors() colorCells.modified="
              << colorCells ()->isModified ();
#endif

    toolEndShape ();

    if (!colorCells ()->isModified ())
        return true;  // ok to close

    int result = KMessageBox::Cancel;


    if (!colorCells ()->url ().isEmpty ())
    {
        result = KMessageBox::warningYesNoCancel (this,
            i18n ("The color palette \"%1\" has been modified.\n"
                  "Do you want to save it?",
                  kpUrlFormatter::PrettyFilename (colorCells ()->url ())),
            QString ()/*caption*/,
            KStandardGuiItem::save (), KStandardGuiItem::discard ());
    }
    else
    {
        const QString name = colorCells ()->colorCollection ()->name ();
        if (!name.isEmpty ())
        {
            result = KMessageBox::warningYesNoCancel (this,
                i18n ("The KDE color palette \"%1\" has been modified.\n"
                      "Do you want to save it to a file?",
                      name),
                QString ()/*caption*/,
                KStandardGuiItem::save (), KStandardGuiItem::discard ());
        }
        else
        {
            result = KMessageBox::warningYesNoCancel (this,
                i18n ("The default color palette has been modified.\n"
                      "Do you want to save it to a file?"),
                QString ()/*caption*/,
                KStandardGuiItem::save (), KStandardGuiItem::discard ());
        }
    }

    switch (result)
    {
    case KMessageBox::Yes:
        return slotColorsSave ();  // close only if save succeeds
    case KMessageBox::No:
        return true;  // close without saving
    default:
        return false;  // don't close current doc
    }
}



// private slot
void kpMainWindow::slotColorsDefault ()
{
    // Call just in case.
    toolEndShape ();

    if (!queryCloseColors ())
        return;

    colorCells ()->setColorCollection (
        kpColorCells::DefaultColorCollection ());

    deselectActionColorsKDE ();
}

// private
bool kpMainWindow::openKDEColors (const QString &name)
{
    kpColorCollection colorCol;
    if (colorCol.openKDE (name, this))
    {
        colorCells ()->setColorCollection (colorCol);
        return true;
    }
    else
    {
        return false;
    }
}

// private slot
void kpMainWindow::slotColorsKDE ()
{
    // Call in case an error dialog appears.
    toolEndShape ();

    if (!queryCloseColors ())
    {
        deselectActionColorsKDE ();
        return;
    }

    const QStringList colNames = ::KDEColorCollectionNames ();
    const int selected = d->actionColorsKDE->currentItem ();
    Q_ASSERT (selected >= 0 && selected < colNames.size ());

    if (!openKDEColors (colNames [selected]))
        deselectActionColorsKDE ();
}

// private
bool kpMainWindow::openColors (const KUrl &url)
{
    if (!colorCells ()->openColorCollection (url))
        return false;

    deselectActionColorsKDE ();
    return true;
}

// private slot
void kpMainWindow::slotColorsOpen ()
{
    // Call due to dialog.
    toolEndShape ();

    KFileDialog fd (colorCells ()->url (), QString()/*filter*/, this);
    fd.setCaption (i18n ("Open Color Palette"));
    fd.setOperationMode (KFileDialog::Opening);

    if (fd.exec ())
    {
        if (!queryCloseColors ())
            return;

        openColors (fd.selectedUrl ());
    }
}

// private slot
void kpMainWindow::slotColorsReload ()
{
    toolEndShape ();

    if (colorCells ()->isModified ())
    {
        int result = KMessageBox::Cancel;

        if (!colorCells ()->url ().isEmpty ())
        {
            result = KMessageBox::warningContinueCancel (this,
                i18n ("The color palette \"%1\" has been modified.\n"
                      "Reloading will lose all changes since you last saved it.\n"
                      "Are you sure?",
                      kpUrlFormatter::PrettyFilename (colorCells ()->url ())),
                QString ()/*caption*/,
                KGuiItem(i18n ("&Reload")));
        }
        else
        {
            const QString name = colorCells ()->colorCollection ()->name ();
            if (!name.isEmpty ())
            {
                result = KMessageBox::warningContinueCancel (this,
                    i18n ("The KDE color palette \"%1\" has been modified.\n"
                          "Reloading will lose all changes.\n"
                          "Are you sure?",
                          colorCells ()->colorCollection ()->name ()),
                    QString ()/*caption*/,
                    KGuiItem (i18n ("&Reload")));
            }
            else
            {
                result = KMessageBox::warningYesNoCancel (this,
                    i18n ("The default color palette has been modified.\n"
                          "Reloading will lose all changes.\n"
                          "Are you sure?"),
                    QString ()/*caption*/,
                    KGuiItem (i18n ("&Reload")));
            }
        }

        if (result != KMessageBox::Continue)
            return;
    }


    if (!colorCells ()->url ().isEmpty ())
    {
        openColors (colorCells ()->url ());
    }
    else
    {
        openKDEColors (colorCells ()->colorCollection ()->name ());
    }
}


// private slot
bool kpMainWindow::slotColorsSave ()
{
    // Call due to dialog.
    toolEndShape ();

    if (colorCells ()->url ().isEmpty ())
    {
        return slotColorsSaveAs ();
    }

    return colorCells ()->saveColorCollection ();
}

// private slot
bool kpMainWindow::slotColorsSaveAs ()
{
    // Call due to dialog.
    toolEndShape ();

    KFileDialog fd (colorCells ()->url (), QString()/*filter*/, this);
    fd.setCaption (i18n ("Save Color Palette As"));
    fd.setOperationMode (KFileDialog::Saving);

    if (fd.exec ())
    {
        if (!colorCells ()->saveColorCollectionAs (fd.selectedUrl ()))
            return false;

        // We're definitely using our own color collection now.
        deselectActionColorsKDE ();
    }

    return true;
}


// private slot
void kpMainWindow::slotColorsAppendRow ()
{
    // Call just in case.
    toolEndShape ();

    kpColorCells *colorCells = d->colorToolBar->colorCells ();
    colorCells->appendRow ();
}

// private slot
void kpMainWindow::slotColorsDeleteRow ()
{
    // Call just in case.
    toolEndShape ();

    kpColorCells *colorCells = d->colorToolBar->colorCells ();
    colorCells->deleteLastRow ();
}
