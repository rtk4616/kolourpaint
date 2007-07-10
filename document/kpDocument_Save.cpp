
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


#define DEBUG_KP_DOCUMENT 0


#include <kpDocument.h>
#include <kpDocumentPrivate.h>

#include <math.h>

#include <qcolor.h>
#include <qbitmap.h>
#include <qbrush.h>
#include <qfile.h>
#include <qimage.h>
#include <qlist.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qrect.h>
#include <qsize.h>
#include <qmatrix.h>

#include <kdebug.h>
#include <kglobal.h>
#include <kimageio.h>
#include <kio/netaccess.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kmimetype.h>  // TODO: isn't this in KIO?
#include <ktemporaryfile.h>

#include <kpColor.h>
#include <kpColorToolBar.h>
#include <kpDefs.h>
#include <kpDocumentSaveOptions.h>
#include <kpDocumentMetaInfo.h>
#include <kpEffectReduceColors.h>
#include <kpMainWindow.h>
#include <kpPixmapFX.h>
#include <kpSelection.h>
#include <kpTool.h>
#include <kpToolToolBar.h>
#include <kpViewManager.h>


bool kpDocument::save (bool overwritePrompt, bool lossyPrompt)
{
#if DEBUG_KP_DOCUMENT
    kDebug () << "kpDocument::save("
               << "overwritePrompt=" << overwritePrompt
               << ",lossyPrompt=" << lossyPrompt
               << ") url=" << m_url
               << " savedAtLeastOnceBefore=" << savedAtLeastOnceBefore ()
               << endl;
#endif

    // TODO: check feels weak
    if (m_url.isEmpty () || m_saveOptions->mimeType ().isEmpty ())
    {
        KMessageBox::detailedError (m_mainWindow,
            i18n ("Could not save image - insufficient information."),
            i18n ("URL: %1\n"
                  "Mimetype: %2",
                  prettyUrl (),
                  m_saveOptions->mimeType ().isEmpty () ?
                          i18n ("<empty>") :
                          m_saveOptions->mimeType ()),
            i18n ("Internal Error"));
        return false;
    }

    return saveAs (m_url, *m_saveOptions,
                   overwritePrompt,
                   lossyPrompt);
}


// public static
bool kpDocument::lossyPromptContinue (const QPixmap &pixmap,
                                      const kpDocumentSaveOptions &saveOptions,
                                      QWidget *parent)
{
#if DEBUG_KP_DOCUMENT
    kDebug () << "kpDocument::lossyPromptContinue()" << endl;
#endif

#define QUIT_IF_CANCEL(messageBoxCommand)            \
{                                                    \
    if (messageBoxCommand != KMessageBox::Continue)  \
    {                                                \
        return false;                                \
    }                                                \
}

    const int lossyType = saveOptions.isLossyForSaving (pixmap);
    if (lossyType & (kpDocumentSaveOptions::MimeTypeMaximumColorDepthLow |
                     kpDocumentSaveOptions::Quality))
    {
        QUIT_IF_CANCEL (
            KMessageBox::warningContinueCancel (parent,
                i18n ("<qt><p>The <b>%1</b> format may not be able"
                      " to preserve all of the image's color information.</p>"

                      "<p>Are you sure you want to save in this format?</p></qt>",
                      KMimeType::mimeType (saveOptions.mimeType ())->comment ()),
                // TODO: caption misleading for lossless formats that have
                //       low maximum colour depth
                i18n ("Lossy File Format"),
                KStandardGuiItem::save (),
                KStandardGuiItem::cancel(),
                QLatin1String ("SaveInLossyMimeTypeDontAskAgain")));
    }
    else if (lossyType & kpDocumentSaveOptions::ColorDepthLow)
    {
        QUIT_IF_CANCEL (
            KMessageBox::warningContinueCancel (parent,
                i18n ("<qt><p>Saving the image at the low color depth of %1-bit"
                        " may result in the loss of color information."

                        " Any transparency will also be removed.</p>"

                        "<p>Are you sure you want to save at this color depth?</p></qt>",
                      saveOptions.colorDepth ()),
                i18n ("Low Color Depth"),
                KStandardGuiItem::save (),
                KStandardGuiItem::cancel(),
                QLatin1String ("SaveAtLowColorDepthDontAskAgain")));
    }
#undef QUIT_IF_CANCEL

    return true;
}

// public static
bool kpDocument::savePixmapToDevice (const QPixmap &pixmap,
                                     QIODevice *device,
                                     const kpDocumentSaveOptions &saveOptions,
                                     const kpDocumentMetaInfo &metaInfo,
                                     bool lossyPrompt,
                                     QWidget *parent,
                                     bool *userCancelled)
{
    if (userCancelled)
        *userCancelled = false;

    QString type = KImageIO::typeForMime (saveOptions.mimeType ()) [0];  // COMPAT: dangerous [0]
#if DEBUG_KP_DOCUMENT
    kDebug () << "\tmimeType=" << saveOptions.mimeType ()
               << " type=" << type << endl;
#endif

    if (lossyPrompt && !lossyPromptContinue (pixmap, saveOptions, parent))
    {
        if (userCancelled)
            *userCancelled = true;

    #if DEBUG_KP_DOCUMENT
        kDebug () << "\treturning false because of lossyPrompt" << endl;
    #endif
        return false;
    }


    QPixmap pixmapToSave =
        kpPixmapFX::pixmapWithDefinedTransparentPixels (pixmap,
            Qt::white);  // CONFIG
    QImage imageToSave = kpPixmapFX::convertToImage (pixmapToSave);


    // TODO: fix dup with kpDocumentSaveOptions::isLossyForSaving()
    const bool useSaveOptionsColorDepth =
        (saveOptions.mimeTypeHasConfigurableColorDepth () &&
         !saveOptions.colorDepthIsInvalid ());
    const bool useSaveOptionsQuality =
        (saveOptions.mimeTypeHasConfigurableQuality () &&
         !saveOptions.qualityIsInvalid ());


    //
    // Reduce colors if required
    //

    if (useSaveOptionsColorDepth &&
        imageToSave.depth () != saveOptions.colorDepth ())
    {
        // TODO: I think this erases the alpha channel!
        //       See comment in kpEffectReduceColors::applyEffect().
        imageToSave = kpEffectReduceColors::convertImageDepth (imageToSave,
                                           saveOptions.colorDepth (),
                                           saveOptions.dither ());
    }


    //
    // Write Meta Info
    //

    imageToSave.setDotsPerMeterX (metaInfo.dotsPerMeterX ());
    imageToSave.setDotsPerMeterY (metaInfo.dotsPerMeterY ());
    imageToSave.setOffset (metaInfo.offset ());

    QList <QImageTextKeyLang> keyList = metaInfo.textList ();
    for (QList <QImageTextKeyLang>::const_iterator it = keyList.begin ();
         it != keyList.end ();
         it++)
    {
        imageToSave.setText ((*it).key, (*it).lang, metaInfo.text (*it));
    }


    //
    // Save at required quality
    //

    int quality = -1;  // default

    if (useSaveOptionsQuality)
        quality = saveOptions.quality ();

    if (!imageToSave.save (device, type.toLatin1 (), quality))
    {
    #if DEBUG_KP_DOCUMENT
        kDebug () << "\tQImage::save() returned false" << endl;
    #endif
        return false;
    }


#if DEBUG_KP_DOCUMENT
    kDebug () << "\tsave OK" << endl;
#endif
    return true;
}

// public static
bool kpDocument::savePixmapToFile (const QPixmap &pixmap,
                                   const KUrl &url,
                                   const kpDocumentSaveOptions &saveOptions,
                                   const kpDocumentMetaInfo &metaInfo,
                                   bool overwritePrompt,
                                   bool lossyPrompt,
                                   QWidget *parent)
{
#if DEBUG_KP_DOCUMENT
    kDebug () << "kpDocument::savePixmapToFile ("
               << url
               << ",overwritePrompt=" << overwritePrompt
               << ",lossyPrompt=" << lossyPrompt
               << ")" << endl;
    saveOptions.printDebug (QLatin1String ("\tsaveOptions"));
    metaInfo.printDebug (QLatin1String ("\tmetaInfo"));
#endif

    if (overwritePrompt && KIO::NetAccess::exists (url, false/*write*/, parent))
    {
        int result = KMessageBox::warningContinueCancel (parent,
            i18n ("A document called \"%1\" already exists.\n"
                  "Do you want to overwrite it?",
                  prettyFilenameForURL (url)),
            QString::null,
            KGuiItem(i18n ("Overwrite")));

        if (result != KMessageBox::Continue)
        {
        #if DEBUG_KP_DOCUMENT
            kDebug () << "\tuser doesn't want to overwrite" << endl;
        #endif

            return false;
        }
    }


    if (lossyPrompt && !lossyPromptContinue (pixmap, saveOptions, parent))
    {
    #if DEBUG_KP_DOCUMENT
        kDebug () << "\treturning false because of lossyPrompt" << endl;
    #endif
        return false;
    }

    KTemporaryFile tempFile;
    QString filename;

    if (!url.isLocalFile ())
    {
        if (!tempFile.open())
        {
        #if DEBUG_KP_DOCUMENT
            kDebug () << "\treturning false because tempFile empty" << endl;
        #endif
            KMessageBox::error (parent,
                                i18n ("Could not save image - unable to create temporary file."));
            return false;
        }else{
            filename = tempFile.fileName ();
        }
    }
    else
        filename = url.path ();


    QFile file (filename);
    bool fileOpenOK = false;
    if (!(fileOpenOK = file.open (QIODevice::WriteOnly)) ||
        !savePixmapToDevice (pixmap, &file,
                             saveOptions, metaInfo,
                             false/*no lossy prompt*/,
                             parent) ||
        (file.close (),
         file.error () != QFile::NoError))
    {
    #if DEBUG_KP_DOCUMENT
        if (!fileOpenOK)
        {
            kDebug () << "\treturning false because fileOpenOK=false"
                       << " errorString=" << file.errorString () << endl;
        }
        else
        {
            kDebug () << "\treturning false because could not save pixmap to device"
                       << endl;
        }
    #endif

        // TODO: use file.errorString()
        KMessageBox::error (parent,
                            i18n ("Could not save as \"%1\".",
                                  kpDocument::prettyFilenameForURL (url)));

        return false;
    }


    if (!url.isLocalFile ())
    {
        if (!KIO::NetAccess::upload (filename, url, parent))
        {
        #if DEBUG_KP_DOCUMENT
            kDebug () << "\treturning false because could not upload" << endl;
        #endif
            KMessageBox::error (parent,
                                i18n ("Could not save image - failed to upload."));
            return false;
        }
    }


    return true;
}

bool kpDocument::saveAs (const KUrl &url,
                         const kpDocumentSaveOptions &saveOptions,
                         bool overwritePrompt,
                         bool lossyPrompt)
{
#if DEBUG_KP_DOCUMENT
    kDebug () << "kpDocument::saveAs (" << url << ","
               << saveOptions.mimeType () << ")" << endl;
#endif

    if (kpDocument::savePixmapToFile (pixmapWithSelection (),
                                      url,
                                      saveOptions, *metaInfo (),
                                      overwritePrompt,
                                      lossyPrompt,
                                      m_mainWindow))
    {
        setURL (url, true/*is from url*/);
        *m_saveOptions = saveOptions;
        m_modified = false;

        m_savedAtLeastOnceBefore = true;

        emit documentSaved ();
        return true;
    }
    else
    {
        return false;
    }
}