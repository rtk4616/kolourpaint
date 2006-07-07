
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


#define DEBUG_KP_PIXMAP_FX 0


#include <kppixmapfx.h>

#include <math.h>

#include <qapplication.h>
#include <qbitmap.h>
#include <qdatetime.h>
#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <qpolygon.h>
#include <qrect.h>

#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>

#include <kpcolor.h>
#include <kpdefs.h>
#include <kpselection.h>
#include <kptool.h>


//
// Overflow Resistant Arithmetic:
//

// public static
int kpPixmapFX::addDimensions (int lhs, int rhs)
{
    if (lhs < 0 || rhs < 0 ||
        lhs > INT_MAX - rhs)
    {
        return INT_MAX;
    }

    return lhs + rhs;
}

// public static
int kpPixmapFX::multiplyDimensions (int lhs, int rhs)
{
    if (rhs == 0)
        return 0;

    if (lhs < 0 || rhs < 0 ||
        lhs > INT_MAX / rhs)
    {
        return INT_MAX;
    }

    return lhs * rhs;
}


//
// QPixmap Statistics
//

// public static
int kpPixmapFX::pixmapArea (const QPixmap &pixmap)
{
    return kpPixmapFX::pixmapArea (pixmap.width (), pixmap.height ());
}

// public static
int kpPixmapFX::pixmapArea (const QPixmap *pixmap)
{
    return (pixmap ? kpPixmapFX::pixmapArea (*pixmap) : 0);
}

// public static
int kpPixmapFX::pixmapArea (int width, int height)
{
    return multiplyDimensions (width, height);
}


// public static
int kpPixmapFX::pixmapSize (const QPixmap &pixmap)
{
    return kpPixmapFX::pixmapSize (pixmap.width (), pixmap.height (),
                                   pixmap.depth ());
}

// public static
int kpPixmapFX::pixmapSize (const QPixmap *pixmap)
{
    return (pixmap ? kpPixmapFX::pixmapSize (*pixmap) : 0);
}

// public static
int kpPixmapFX::pixmapSize (int width, int height, int depth)
{
    // handle 15bpp
    int roundedDepth = (depth > 8 ? (depth + 7) / 8 * 8 : depth);

#if DEBUG_KP_PIXMAP_FX && 0
    kDebug () << "kpPixmapFX::pixmapSize() w=" << width
               << " h=" << height
               << " d=" << depth
               << " roundedDepth=" << roundedDepth
               << " ret="
               << multiplyDimensions (kpPixmapFX::pixmapArea (width, height), roundedDepth) / 8
               << endl;
#endif
    return multiplyDimensions (kpPixmapFX::pixmapArea (width, height), roundedDepth) / 8;
}


// public static
int kpPixmapFX::imageSize (const QImage &image)
{
    return kpPixmapFX::imageSize (image.width (), image.height (), image.depth ());
}

// public static
int kpPixmapFX::imageSize (const QImage *image)
{
    return (image ? kpPixmapFX::imageSize (*image) : 0);
}

// public static
int kpPixmapFX::imageSize (int width, int height, int depth)
{
    // handle 15bpp
    int roundedDepth = (depth > 8 ? (depth + 7) / 8 * 8 : depth);

#if DEBUG_KP_PIXMAP_FX && 0
    kDebug () << "kpPixmapFX::imageSize() w=" << width
               << " h=" << height
               << " d=" << depth
               << " roundedDepth=" << roundedDepth
               << " ret="
               << multiplyDimensions (multiplyDimensions (width, height), roundedDepth) / 8
               << endl;
#endif

    return multiplyDimensions (multiplyDimensions (width, height), roundedDepth) / 8;
}


// public static
int kpPixmapFX::selectionSize (const kpSelection &sel)
{
    return sel.size ();
}

// public static
int kpPixmapFX::selectionSize (const kpSelection *sel)
{
    return (sel ? sel->size () : 0);
}


// public static
int kpPixmapFX::stringSize (const QString &string)
{
#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "kpPixmapFX::stringSize(" << string << ")"
               << " len=" << string.length ()
               << " sizeof(QChar)=" << sizeof (QChar)
               << endl;
#endif
    return string.length () * sizeof (QChar);
}


// public static
int kpPixmapFX::pointArraySize (const QPolygon &points)
{
#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "kpPixmapFX::pointArraySize() points.size="
               << points.size ()
               << " sizeof(QPoint)=" << sizeof (QPoint)
               << endl;
#endif

    return (points.size () * sizeof (QPoint));
}


//
// QPixmap/QImage Conversion Functions
//

// public static
QImage kpPixmapFX::convertToImage (const QPixmap &pixmap)
{
    if (pixmap.isNull ())
        return QImage ();

    return pixmap.toImage ();
}


// Returns true if <image> contains translucency (rather than just transparency)
// QPixmap::hasAlphaChannel() appears to give incorrect results
static bool imageHasAlphaChannel (const QImage &image)
{
    if (image.depth () < 32)
        return false;

    for (int y = 0; y < image.height (); y++)
    {
        for (int x = 0; x < image.width (); x++)
        {
            const QRgb rgb = image.pixel (x, y);

            if (qAlpha (rgb) > 0 && qAlpha (rgb) < 255)
                return true;
        }
    }

    return false;
}

static int imageNumColorsUpTo (const QImage &image, int max)
{
    QMap <QRgb, bool> rgbMap;

    if (image.depth () <= 8)
    {
        for (int i = 0; i < image.numColors () && (int) rgbMap.size () < max; i++)
        {
            rgbMap.insert (image.color (i), true);
        }
    }
    else
    {
        for (int y = 0; y < image.height () && (int) rgbMap.size () < max; y++)
        {
            for (int x = 0; x < image.width () && (int) rgbMap.size () < max; x++)
            {
                rgbMap.insert (image.pixel (x, y), true);
            }
        }
    }

    return rgbMap.size ();
}

static void convertToPixmapWarnAboutLoss (const QImage &image,
                                          const kpPixmapFX::WarnAboutLossInfo &wali)
{
    if (!wali.isValid ())
        return;


    const QString colorDepthTranslucencyDontAskAgain =
        wali.m_dontAskAgainPrefix + QLatin1String ("_ColorDepthTranslucency");
    const QString colorDepthDontAskAgain =
        wali.m_dontAskAgainPrefix + QLatin1String ("_ColorDepth");
    const QString translucencyDontAskAgain =
        wali.m_dontAskAgainPrefix + QLatin1String ("_Translucency");

#if DEBUG_KP_PIXMAP_FX && 1
    QTime timer;
    timer.start ();
#endif

    bool hasAlphaChannel =
        (KMessageBox::shouldBeShownContinue (translucencyDontAskAgain) &&
         imageHasAlphaChannel (image));

#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "\twarnAboutLoss - check hasAlphaChannel took "
               << timer.restart () << "msec" << endl;
#endif

    bool moreColorsThanDisplay =
        (KMessageBox::shouldBeShownContinue (colorDepthDontAskAgain) &&
         image.depth () > QPixmap::defaultDepth() &&
         QPixmap::defaultDepth () < 24);  // 32 indicates alpha channel

    int screenDepthNeeded = 0;

    if (moreColorsThanDisplay)
        screenDepthNeeded = qMin (24, image.depth ());

#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "\ttranslucencyShouldBeShown="
                << KMessageBox::shouldBeShownContinue (translucencyDontAskAgain)
                << endl
                << "\thasAlphaChannel=" << hasAlphaChannel
                << endl
                << "\tcolorDepthShownBeShown="
                << KMessageBox::shouldBeShownContinue (colorDepthDontAskAgain)
                << endl
                << "\timage.depth()=" << image.depth ()
                << endl
                << "\tscreenDepth=" << QPixmap::defaultDepth ()
                << endl
                << "\tmoreColorsThanDisplay=" << moreColorsThanDisplay
                << endl
                << "\tneedDepth=" << screenDepthNeeded
                << endl;
#endif


    QApplication::setOverrideCursor (Qt::ArrowCursor);

    if (moreColorsThanDisplay && hasAlphaChannel)
    {
        KMessageBox::information (wali.m_parent,
            wali.m_moreColorsThanDisplayAndHasAlphaChannelMessage
                .subs (screenDepthNeeded).toString (),
            QString::null,  // or would you prefer "Low Screen Depth and Image Contains Transparency"? :)
            colorDepthTranslucencyDontAskAgain);

        if (!KMessageBox::shouldBeShownContinue (colorDepthTranslucencyDontAskAgain))
        {
            KMessageBox::saveDontShowAgainContinue (colorDepthDontAskAgain);
            KMessageBox::saveDontShowAgainContinue (translucencyDontAskAgain);
        }
    }
    else if (moreColorsThanDisplay)
    {
        KMessageBox::information (wali.m_parent,
            wali.m_moreColorsThanDisplayMessage
                .subs (screenDepthNeeded).toString (),
            i18n ("Low Screen Depth"),
            colorDepthDontAskAgain);
    }
    else if (hasAlphaChannel)
    {
        KMessageBox::information (wali.m_parent,
            wali.m_hasAlphaChannelMessage,
            i18n ("Image Contains Translucency"),
            translucencyDontAskAgain);
    }

    QApplication::restoreOverrideCursor ();
}

// public static
QPixmap kpPixmapFX::convertToPixmap (const QImage &image, bool pretty,
                                     const WarnAboutLossInfo &wali)
{
#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "kpPixmapFX::convertToPixmap(image,pretty=" << pretty
               << ",warnAboutLossInfo.isValid=" << wali.isValid ()
               << ")" << endl;
    QTime timer;
    timer.start ();
#endif

    if (image.isNull ())
        return QPixmap ();


    QPixmap destPixmap;

    if (!pretty)
    {
        destPixmap = QPixmap::fromImage(image,
                                     Qt::ColorOnly/*always display depth*/ |
                                     Qt::ThresholdDither/*no dither*/ |
                                     Qt::ThresholdAlphaDither/*no dither alpha*/|
                                     Qt::AvoidDither);
    }
    else
    {
        destPixmap = QPixmap::fromImage (image,
                                     Qt::ColorOnly/*always display depth*/ |
                                     Qt::DiffuseDither/*hi quality dither*/ |
                                     Qt::ThresholdAlphaDither/*no dither alpha*/ |
                                     Qt::PreferDither/*(dither even if <256 colours)*/);
    }

#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "\tconversion took " << timer.elapsed () << "msec" << endl;
#endif

    kpPixmapFX::ensureNoAlphaChannel (&destPixmap);


    if (wali.isValid ())
        convertToPixmapWarnAboutLoss (image, wali);


    return destPixmap;
}

// TODO: don't dup convertToPixmap() code
// public static
QPixmap kpPixmapFX::convertToPixmapAsLosslessAsPossible (const QImage &image,
    const WarnAboutLossInfo &wali)
{
#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "kpPixmapFX::convertToPixmapAsLosslessAsPossible(image depth="
               << image.depth ()
               << ",warnAboutLossInfo.isValid=" << wali.isValid ()
               << ") screenDepth=" << QPixmap::defaultDepth ()
               << " imageNumColorsUpTo257=" << imageNumColorsUpTo (image, 257)
               << endl;
    QTime timer;
    timer.start ();
#endif

    if (image.isNull ())
        return QPixmap ();


    const int screenDepth = (QPixmap::defaultDepth () >= 24 ?
                                 32 :
                                 QPixmap::defaultDepth ());

    QPixmap destPixmap;
    Qt::ImageConversionFlags ditherFlags = 0;

    if (image.depth () <= screenDepth)
    {
    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "\timage depth <= screen depth - don't dither"
                   << " (AvoidDither | ThresholdDither)" << endl;
    #endif

        ditherFlags = (Qt::AvoidDither | Qt::ThresholdDither);
    }
    // PRE: image.depth() > screenDepth
    // ASSERT: screenDepth < 32
    else if (screenDepth <= 8)
    {
        const int screenNumColors = (1 << screenDepth);

    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "\tscreen depth <= 8; imageNumColorsUpTo"
                   << (screenNumColors + 1)
                   << "=" << imageNumColorsUpTo (image, screenNumColors + 1)
                   << endl;
    #endif

        if (imageNumColorsUpTo (image, screenNumColors + 1) <= screenNumColors)
        {
        #if DEBUG_KP_PIXMAP_FX && 1
            kDebug () << "\t\tcolors fit on screen - don't dither"
                       << " (AvoidDither | ThresholdDither)" << endl;
        #endif
            ditherFlags = (Qt::AvoidDither | Qt::ThresholdDither);
        }
        else
        {
        #if DEBUG_KP_PIXMAP_FX && 1
            kDebug () << "\t\tcolors don't fit on screen - dither"
                       << " (PreferDither | DiffuseDither)" << endl;
        #endif
            ditherFlags = (Qt::PreferDither | Qt::DiffuseDither);
        }
    }
    // PRE: image.depth() > screenDepth &&
    //      screenDepth > 8
    // ASSERT: screenDepth < 32
    else
    {
    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "\tscreen depth > 8 - read config" << endl;
    #endif

        int configDitherIfNumColorsGreaterThan = 323;

        KConfigGroup cfg (KGlobal::config (), kpSettingsGroupGeneral);
        if (cfg.hasKey (kpSettingDitherOnOpen))
        {
            configDitherIfNumColorsGreaterThan = cfg.readEntry (kpSettingDitherOnOpen, 0);
        }
        else
        {
            cfg.writeEntry (kpSettingDitherOnOpen, configDitherIfNumColorsGreaterThan);
            cfg.sync ();
        }

    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "\t\tcfg=" << configDitherIfNumColorsGreaterThan
                   << " image=" << imageNumColorsUpTo (image, configDitherIfNumColorsGreaterThan + 1)
                   << endl;
    #endif

        if (imageNumColorsUpTo (image, configDitherIfNumColorsGreaterThan + 1) >
            configDitherIfNumColorsGreaterThan)
        {
        #if DEBUG_KP_PIXMAP_FX && 1
            kDebug () << "\t\t\talways dither (PreferDither | DiffuseDither)"
                        << endl;
        #endif
            ditherFlags = (Qt::PreferDither | Qt::DiffuseDither);
        }
        else
        {
        #if DEBUG_KP_PIXMAP_FX && 1
            kDebug () << "\t\t\tdon't dither (AvoidDither | ThresholdDither)"
                       << endl;
        #endif
            ditherFlags = (Qt::AvoidDither | Qt::ThresholdDither);
        }
    }


    destPixmap = QPixmap::fromImage (image,
                                 Qt::ColorOnly/*always display depth*/ |
                                 Qt::ThresholdAlphaDither/*no dither alpha*/ |
                                 ditherFlags);

#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "\tconversion took " << timer.elapsed () << "msec" << endl;
#endif

    kpPixmapFX::ensureNoAlphaChannel (&destPixmap);


    if (wali.isValid ())
        convertToPixmapWarnAboutLoss (image, wali);


    return destPixmap;
}


// public static
QPixmap kpPixmapFX::pixmapWithDefinedTransparentPixels (const QPixmap &pixmap,
                                                        const QColor &transparentColor)
{
    if (!pixmap.mask ())
        return pixmap;

    QPixmap retPixmap (pixmap.width (), pixmap.height ());
    retPixmap.fill (transparentColor);

    QPainter p (&retPixmap);
    p.drawPixmap (QPoint (0, 0), pixmap);
    p.end ();

    retPixmap.setMask (pixmap.mask ());
    return retPixmap;
}


//
// Get/Set Parts of Pixmap
//


// public static
QPixmap kpPixmapFX::getPixmapAt (const QPixmap &pm, const QRect &rect)
{
    QPixmap retPixmap (rect.width (), rect.height ());

#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "kpPixmapFX::getPixmapAt(pm.hasMask="
               << !pm.mask ().isNull ()
               << ",rect="
               << rect
               << ")"
               << endl;
#endif

    const QRect validSrcRect = pm.rect ().intersect (rect);
    const bool wouldHaveUndefinedPixels = (validSrcRect != rect);

    // ssss ssss
    // ssss ssss
    //     +--------+
    // ssss|SSSS    |
    // ssss|SSSS    |  <-- "rect"
    //     |        |
    //     +--------+
    //
    // Let 's' and 'S' be source (pm) pixels.
    //
    // If "rect" asks for part of the source (pm) and some more, the "some
    // more" should be transparent - not undefined.
    if (wouldHaveUndefinedPixels)
    {
    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "\tret would contain undefined pixels - setting them to transparent" << endl;
    #endif
        QBitmap transparentMask (rect.width (), rect.height ());
        transparentMask.fill (Qt::color0/*transparent*/);
        retPixmap.setMask (transparentMask);
    }

    if (validSrcRect.isEmpty ())
    {
    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "\tsilly case - completely invalid rect - ret transparent pixmap" << endl;
    #endif
        return retPixmap;
    }

    // If dest (retPixmap) has no mask but the source (pm) has a mask,
    // make sure dest (retPixmap) has a mask so that
    // QPainter::CompositionMode_Source will copy over source's (pm's)
    // mask.
    if (retPixmap.mask ().isNull () && !pm.mask ().isNull ())
    {
         QBitmap retPixmapMask (retPixmap.width (), retPixmap.height ());
         retPixmapMask.fill (Qt::color0/*arbitrary since we're going to be overriden*/);
         retPixmap.setMask (retPixmapMask);
    }

    const QPoint destTopLeft = validSrcRect.topLeft () - rect.topLeft ();

    // Copy data _and_ mask (if avail).
    QPainter retPixmapPainter (&retPixmap);
    retPixmapPainter.setCompositionMode (QPainter::CompositionMode_Source);
    retPixmapPainter.drawPixmap (destTopLeft, pm, validSrcRect);
    retPixmapPainter.end ();


#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "\tretPixmap.hasMask="
               << !retPixmap.mask ().isNull ()
               << endl;
#endif

    return retPixmap;
}


// public static
void kpPixmapFX::setPixmapAt (QPixmap *destPixmapPtr, const QRect &destRect,
                              const QPixmap &srcPixmap)
{
    if (!destPixmapPtr)
        return;

#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "kpPixmapFX::setPixmapAt(destPixmap->rect="
               << destPixmapPtr->rect ()
               << ",destPixmap->hasMask="
               << !destPixmapPtr->mask ().isNull ()
               << ",destRect="
               << destRect
               << ",srcPixmap.rect="
               << srcPixmap.rect ()
               << ",srcPixmap.hasMask="
               << !srcPixmap.mask ().isNull ()
               << ")"
               << endl;
#endif

#if DEBUG_KP_PIXMAP_FX && 0
    if (!destPixmapPtr->mask ().isNull ())
    {
        QImage image = kpPixmapFX::convertToImage (*destPixmapPtr);
        int numTrans = 0;

        for (int y = 0; y < image.height (); y++)
        {
            for (int x = 0; x < image.width (); x++)
            {
                if (qAlpha (image.pixel (x, y)) == 0)
                    numTrans++;
            }
        }

        kDebug () << "\tdestPixmapPtr numTrans=" << numTrans << endl;
    }
#endif

    // If dest (*destPixmapPtr) has no mask but the source (srcPixmap) has a mask,
    // make sure dest (*destPixmapPtr) has a mask so that
    // QPainter::CompositionMode_Source will copy over source's (srcPixmap's)
    // mask.
    if (destPixmapPtr->mask ().isNull () && !srcPixmap.mask ().isNull ())
    {
         QBitmap destPixmapMask (destPixmapPtr->width (), destPixmapPtr->height ());
         destPixmapMask.fill (Qt::color0/*arbitrary since we're going to be overriden*/);
         destPixmapPtr->setMask (destPixmapMask);
    }

    // You cannot copy more than what you have.
    Q_ASSERT (destRect.width () <= srcPixmap.width () &&
              destRect.height () <= srcPixmap.height ());

    // Copy data _and_ mask (if avail).
    QPainter destPixmapPainter (destPixmapPtr);
    destPixmapPainter.setCompositionMode (QPainter::CompositionMode_Source);
    destPixmapPainter.drawPixmap (destRect.topLeft (),
        srcPixmap, QRect (0, 0, destRect.width (), destRect.height ()));
    destPixmapPainter.end ();


#if DEBUG_KP_PIXMAP_FX && 0
    kDebug () << "\tdestPixmap->hasMask="
               << !destPixmapPtr->mask ().isNull ()
               << endl;
    if (!destPixmapPtr->mask ().isNull ())
    {
        QImage image = kpPixmapFX::convertToImage (*destPixmapPtr);
        int numTrans = 0;

        for (int y = 0; y < image.height (); y++)
        {
            for (int x = 0; x < image.width (); x++)
            {
                if (qAlpha (image.pixel (x, y)) == 0)
                    numTrans++;
            }
        }

        kDebug () << "\tdestPixmapPtr numTrans=" << numTrans << endl;
    }
#endif
}

// public static
void kpPixmapFX::setPixmapAt (QPixmap *destPixmapPtr, const QPoint &destAt,
                              const QPixmap &srcPixmap)
{
    kpPixmapFX::setPixmapAt (destPixmapPtr,
                             QRect (destAt.x (), destAt.y (),
                                    srcPixmap.width (), srcPixmap.height ()),
                             srcPixmap);
}

// public static
void kpPixmapFX::setPixmapAt (QPixmap *destPixmapPtr, int destX, int destY,
                              const QPixmap &srcPixmap)
{
    kpPixmapFX::setPixmapAt (destPixmapPtr, QPoint (destX, destY), srcPixmap);
}


// public static
void kpPixmapFX::paintPixmapAt (QPixmap *destPixmapPtr, const QPoint &destAt,
                                const QPixmap &srcPixmap)
{
    if (!destPixmapPtr)
        return;

    // Copy src (masked by src's mask) on top of dest.
    QPainter p (destPixmapPtr);
    p.drawPixmap (destAt, srcPixmap);
}

// public static
void kpPixmapFX::paintPixmapAt (QPixmap *destPixmapPtr, int destX, int destY,
                                const QPixmap &srcPixmap)
{
    kpPixmapFX::paintPixmapAt (destPixmapPtr, QPoint (destX, destY), srcPixmap);
}


// public static
kpColor kpPixmapFX::getColorAtPixel (const QPixmap &pm, const QPoint &at)
{
#if DEBUG_KP_PIXMAP_FX && 0
    kDebug () << "kpToolColorPicker::colorAtPixel" << p << endl;
#endif

    if (at.x () < 0 || at.x () >= pm.width () ||
        at.y () < 0 || at.y () >= pm.height ())
    {
        return kpColor::invalid;
    }

    QPixmap pixmap = getPixmapAt (pm, QRect (at, at));
    QImage image = kpPixmapFX::convertToImage (pixmap);
    if (image.isNull ())
    {
        kError () << "kpPixmapFX::getColorAtPixel(QPixmap) could not convert to QImage" << endl;
        return kpColor::invalid;
    }

    return getColorAtPixel (image, QPoint (0, 0));
}

// public static
kpColor kpPixmapFX::getColorAtPixel (const QPixmap &pm, int x, int y)
{
    return kpPixmapFX::getColorAtPixel (pm, QPoint (x, y));
}

// public static
kpColor kpPixmapFX::getColorAtPixel (const QImage &img, const QPoint &at)
{
    if (!img.valid (at.x (), at.y ()))
        return kpColor::invalid;

    QRgb rgba = img.pixel (at.x (), at.y ());
    return kpColor (rgba);
}

// public static
kpColor kpPixmapFX::getColorAtPixel (const QImage &img, int x, int y)
{
    return kpPixmapFX::getColorAtPixel (img, QPoint (x, y));
}


//
// Mask Operations
//


// public static
void kpPixmapFX::ensureNoAlphaChannel (QPixmap *destPixmapPtr)
{
    if (destPixmapPtr->hasAlphaChannel ())
        destPixmapPtr->setMask (kpPixmapFX::getNonNullMask/*just in case*/ (*destPixmapPtr));
}


// public static
QBitmap kpPixmapFX::getNonNullMask (const QPixmap &pm)
{
    if (!pm.mask ().isNull ())
        return pm.mask ();
    else
    {
        QBitmap maskBitmap (pm.width (), pm.height ());
        maskBitmap.fill (Qt::color1/*opaque*/);

        return maskBitmap;
    }
}


// public static
void kpPixmapFX::ensureTransparentAt (QPixmap *destPixmapPtr, const QRect &destRect)
{
    if (!destPixmapPtr)
        return;

    QBitmap maskBitmap = getNonNullMask (*destPixmapPtr);

    QPainter p (&maskBitmap);
    p.fillRect (destRect, Qt::color0/*transparent*/);
    p.end ();

    destPixmapPtr->setMask (maskBitmap);
}


// public static
void kpPixmapFX::paintMaskTransparentWithBrush (QPixmap *destPixmapPtr, const QPoint &destAt,
                                                const QPixmap &brushBitmap)
{
    if (!destPixmapPtr)
        return;

    if (brushBitmap.depth () > 1)
    {
        kError () << "kpPixmapFX::paintMaskTransparentWidthBrush() passed brushPixmap with depth > 1" << endl;
        return;
    }

    if (destPixmapPtr->mask ().isNull ())
        destPixmapPtr->setMask (kpPixmapFX::getNonNullMask (*destPixmapPtr));

    //                  Src
    //  Dest Mask   Brush Bitmap   =   Result
    //  -------------------------------------
    //      0            0               0
    //      0            1               0
    //      1            0               1
    //      1            1               0
    //
    // Dest Bitmap / Result value of 1 = opaque
    //                               0 = transparent
    // Src Brush Bitmap value of 1 means "make transparent"
    //                           0 means "leave it as it is"

    QPixmap brushPixmap (brushBitmap.width (), brushBitmap.height ());
    brushPixmap.fill (Qt::yellow/*arbitrary since source pixels ignored*/);
    brushPixmap.setMask (brushBitmap);  // Mask is not ignored though.

    QPainter painter (destPixmapPtr);
    painter.setCompositionMode (QPainter::CompositionMode_DestinationOut);
    painter.drawPixmap (destAt, brushPixmap);
    painter.end ();
}

// public static
void kpPixmapFX::paintMaskTransparentWithBrush (QPixmap *destPixmapPtr, int destX, int destY,
                                                const QPixmap &brushBitmap)
{
    kpPixmapFX::paintMaskTransparentWithBrush (destPixmapPtr,
                                               QPoint (destX, destY),
                                               brushBitmap);
}


// public static
void kpPixmapFX::ensureOpaqueAt (QPixmap *destPixmapPtr, const QRect &destRect)
{
    if (!destPixmapPtr || !destPixmapPtr->mask ()/*already opaque*/)
        return;

    QBitmap maskBitmap = destPixmapPtr->mask ();

    QPainter p (&maskBitmap);
    p.fillRect (destRect, Qt::color1/*opaque*/);
    p.end ();

    destPixmapPtr->setMask (maskBitmap);
}


//
// Effects
//

// public static
void kpPixmapFX::convertToGrayscale (QPixmap *destPixmapPtr)
{
    QImage image = kpPixmapFX::convertToImage (*destPixmapPtr);
    kpPixmapFX::convertToGrayscale (&image);
    *destPixmapPtr = kpPixmapFX::convertToPixmap (image);
}

// public static
QPixmap kpPixmapFX::convertToGrayscale (const QPixmap &pm)
{
    QImage image = kpPixmapFX::convertToImage (pm);
    kpPixmapFX::convertToGrayscale (&image);
    return kpPixmapFX::convertToPixmap (image);
}

static QRgb toGray (QRgb rgb)
{
    // naive way that doesn't preserve brightness
    // int gray = (qRed (rgb) + qGreen (rgb) + qBlue (rgb)) / 3;

    // over-exaggerates red & blue
    // int gray = qGray (rgb);

    int gray = (212671 * qRed (rgb) + 715160 * qGreen (rgb) + 72169 * qBlue (rgb)) / 1000000;
    return qRgba (gray, gray, gray, qAlpha (rgb));
}

// public static
void kpPixmapFX::convertToGrayscale (QImage *destImagePtr)
{
    if (destImagePtr->depth () > 8)
    {
        // hmm, why not just write to the pixmap directly???

        for (int y = 0; y < destImagePtr->height (); y++)
        {
            for (int x = 0; x < destImagePtr->width (); x++)
            {
                destImagePtr->setPixel (x, y, toGray (destImagePtr->pixel (x, y)));
            }
        }
    }
    else
    {
        // 1- & 8- bit images use a color table
        for (int i = 0; i < destImagePtr->numColors (); i++)
            destImagePtr->setColor (i, toGray (destImagePtr->color (i)));
    }
}

// public static
QImage kpPixmapFX::convertToGrayscale (const QImage &img)
{
    QImage retImage = img;
    kpPixmapFX::convertToGrayscale (&retImage);
    return retImage;
}


// public static
void kpPixmapFX::fill (QPixmap *destPixmapPtr, const kpColor &color)
{
    if (!destPixmapPtr)
        return;

    if (color.isOpaque ())
    {
        destPixmapPtr->setMask (QBitmap ());  // no mask = opaque
        destPixmapPtr->fill (color.toQColor ());
    }
    else
    {
        kpPixmapFX::ensureTransparentAt (destPixmapPtr, destPixmapPtr->rect ());
    }
}

// public static
QPixmap kpPixmapFX::fill (const QPixmap &pm, const kpColor &color)
{
    QPixmap ret = pm;
    kpPixmapFX::fill (&ret, color);
    return ret;
}


// public static
void kpPixmapFX::resize (QPixmap *destPixmapPtr, int w, int h,
                         const kpColor &backgroundColor)
{
#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "kpPixmapFX::resize()" << endl;
#endif

    if (!destPixmapPtr)
        return;

    const int oldWidth = destPixmapPtr->width ();
    const int oldHeight = destPixmapPtr->height ();

    if (w == oldWidth && h == oldHeight)
        return;


    QPixmap newPixmap (w, h);

    // Would have new undefined areas?
    if (w > oldWidth || h > oldHeight)
    {
    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "\tbacking with fill opqaque="
                  << backgroundColor.isOpaque () << endl;
    #endif
        if (backgroundColor.isOpaque ())
            newPixmap.fill (backgroundColor.toQColor ());
        else
        {
            QBitmap newPixmapMask (w, h);
            newPixmapMask.fill (Qt::color0/*transparent*/);
        }
    }

    // Copy over old pixmap.
    setPixmapAt (&newPixmap, 0, 0, *destPixmapPtr);

    // Replace pixmap with new one.
    *destPixmapPtr = newPixmap;
}

// public static
QPixmap kpPixmapFX::resize (const QPixmap &pm, int w, int h,
                            const kpColor &backgroundColor)
{
    QPixmap ret = pm;
    kpPixmapFX::resize (&ret, w, h, backgroundColor);
    return ret;
}


// public static
void kpPixmapFX::scale (QPixmap *destPixmapPtr, int w, int h, bool pretty)
{
    if (!destPixmapPtr)
        return;

    *destPixmapPtr = kpPixmapFX::scale (*destPixmapPtr, w, h, pretty);
}

// public static
QPixmap kpPixmapFX::scale (const QPixmap &pm, int w, int h, bool pretty)
{
#if DEBUG_KP_PIXMAP_FX && 0
    kDebug () << "kpPixmapFX::scale(oldRect=" << pm.rect ()
               << ",w=" << w
               << ",h=" << h
               << ",pretty=" << pretty
               << ")"
               << endl;
#endif

    if (w == pm.width () && h == pm.height ())
        return pm;

    if (pretty)
    {
        QImage image = kpPixmapFX::convertToImage (pm);

    #if DEBUG_KP_PIXMAP_FX && 0
        kDebug () << "\tBefore smooth scale:" << endl;
        for (int y = 0; y < image.height (); y++)
        {
            for (int x = 0; x < image.width (); x++)
            {
                fprintf (stderr, " %08X", image.pixel (x, y));
            }
            fprintf (stderr, "\n");
        }
    #endif

        image = image.scaled (w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    #if DEBUG_KP_PIXMAP_FX && 0
        kDebug () << "\tAfter smooth scale:" << endl;
        for (int y = 0; y < image.height (); y++)
        {
            for (int x = 0; x < image.width (); x++)
            {
                fprintf (stderr, " %08X", image.pixel (x, y));
            }
            fprintf (stderr, "\n");
        }
    #endif

        return kpPixmapFX::convertToPixmap (image, false/*let's not smooth it again*/);
    }
    else
    {
        QMatrix matrix;

        matrix.scale (double (w) / double (pm.width ()),
                      double (h) / double (pm.height ()));

        return pm.transformed (matrix);
    }
}


// public static
double kpPixmapFX::AngleInDegreesEpsilon =
    KP_RADIANS_TO_DEGREES (atan (1.0 / 10000.0))
        / (2.0/*max error allowed*/ * 2.0/*for good measure*/);


static QMatrix matrixWithZeroOrigin (const QMatrix &matrix, int width, int height)
{
    QRect newRect = matrix.mapRect (QRect (0, 0, width, height));

    QMatrix translatedMatrix (matrix.m11 (), matrix.m12 (), matrix.m21 (), matrix.m22 (),
                               matrix.dx () - newRect.left (), matrix.dy () - newRect.top ());

    return translatedMatrix;
}

static QPixmap xForm (const QPixmap &pm, const QMatrix &transformMatrix_,
                      const kpColor &backgroundColor,
                      int targetWidth, int targetHeight)
{
    QMatrix transformMatrix = transformMatrix_;

#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "kppixmapfx.cpp: xForm(pm.size=" << pm.size ()
               << ",targetWidth=" << targetWidth
               << ",targetHeight=" << targetHeight
               << ")"
               << endl;
#endif
    QRect newRect = transformMatrix.mapRect (pm.rect ());
#if DEBUG_KP_PIXMAP_FX && 1
    kDebug () << "\tmappedRect=" << newRect << endl;

#endif

    QMatrix scaleMatrix;
    if (targetWidth > 0 && targetWidth != newRect.width ())
    {
    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "\tadjusting for targetWidth" << endl;
    #endif
        scaleMatrix.scale (double (targetWidth) / double (newRect.width ()), 1);
    }

    if (targetHeight > 0 && targetHeight != newRect.height ())
    {
    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "\tadjusting for targetHeight" << endl;
    #endif
        scaleMatrix.scale (1, double (targetHeight) / double (newRect.height ()));
    }

    if (!scaleMatrix.isIdentity ())
    {
    #if DEBUG_KP_PIXMAP_FX && 1
        // TODO: What is going on here???  Why isn't matrix * working properly?
        QMatrix wrongMatrix = transformMatrix * scaleMatrix;
        QMatrix oldHat = transformMatrix;
        if (targetWidth > 0 && targetWidth != newRect.width ())
            oldHat.scale (double (targetWidth) / double (newRect.width ()), 1);
        if (targetHeight > 0 && targetHeight != newRect.height ())
            oldHat.scale (1, double (targetHeight) / double (newRect.height ()));
        QMatrix altHat = transformMatrix;
        altHat.scale ((targetWidth > 0 && targetWidth != newRect.width ()) ? double (targetWidth) / double (newRect.width ()) : 1,
                      (targetHeight > 0 && targetHeight != newRect.height ()) ? double (targetHeight) / double (newRect.height ()) : 1);
        QMatrix correctMatrix = scaleMatrix * transformMatrix;

        kDebug () << "\tsupposedlyWrongMatrix: m11=" << wrongMatrix.m11 ()  // <<<---- this is the correct matrix???
                   << " m12=" << wrongMatrix.m12 ()
                   << " m21=" << wrongMatrix.m21 ()
                   << " m22=" << wrongMatrix.m22 ()
                   << " dx=" << wrongMatrix.dx ()
                   << " dy=" << wrongMatrix.dy ()
                   << " rect=" << wrongMatrix.mapRect (pm.rect ())
                   << endl
                   << "\ti_used_to_use_thisMatrix: m11=" << oldHat.m11 ()
                   << " m12=" << oldHat.m12 ()
                   << " m21=" << oldHat.m21 ()
                   << " m22=" << oldHat.m22 ()
                   << " dx=" << oldHat.dx ()
                   << " dy=" << oldHat.dy ()
                   << " rect=" << oldHat.mapRect (pm.rect ())
                   << endl
                   << "\tabove but scaled at the same time: m11=" << altHat.m11 ()
                   << " m12=" << altHat.m12 ()
                   << " m21=" << altHat.m21 ()
                   << " m22=" << altHat.m22 ()
                   << " dx=" << altHat.dx ()
                   << " dy=" << altHat.dy ()
                   << " rect=" << altHat.mapRect (pm.rect ())
                   << endl
                   << "\tsupposedlyCorrectMatrix: m11=" << correctMatrix.m11 ()
                   << " m12=" << correctMatrix.m12 ()
                   << " m21=" << correctMatrix.m21 ()
                   << " m22=" << correctMatrix.m22 ()
                   << " dx=" << correctMatrix.dx ()
                   << " dy=" << correctMatrix.dy ()
                   << " rect=" << correctMatrix.mapRect (pm.rect ())
                   << endl;
    #endif

        transformMatrix = transformMatrix * scaleMatrix;

        newRect = transformMatrix.mapRect (pm.rect ());
    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "\tnewRect after targetWidth,targetHeight adjust=" << newRect << endl;
    #endif
    }


    QPixmap newPixmap (targetWidth > 0 ? targetWidth : newRect.width (),
                       targetHeight > 0 ? targetHeight : newRect.height ());
    if ((targetWidth > 0 && targetWidth != newRect.width ()) ||
        (targetHeight > 0 && targetHeight != newRect.height ()))
    {
    #if DEBUG_KP_PIXMAP_FX && 1
        kDebug () << "kppixmapfx.cpp: xForm(pm.size=" << pm.size ()
                   << ",targetWidth=" << targetWidth
                   << ",targetHeight=" << targetHeight
                   << ") newRect=" << newRect
                   << " (you are a victim of rounding error)"
                   << endl;
    #endif
    }

    QBitmap newBitmapMask;

    if (backgroundColor.isOpaque ())
        newPixmap.fill (backgroundColor.toQColor ());

    if (backgroundColor.isTransparent () || !pm.mask ().isNull ())
    {
        newBitmapMask = QPixmap (newPixmap.width (), newPixmap.height ());
        newBitmapMask.fill (backgroundColor.maskColor ());
    }

    QPainter painter (&newPixmap);
#if DEBUG_KP_PIXMAP_FX && 0
    kDebug () << "\tmatrix: m11=" << transformMatrix.m11 ()
            << " m12=" << transformMatrix.m12 ()
            << " m21=" << transformMatrix.m21 ()
            << " m22=" << transformMatrix.m22 ()
            << " dx=" << transformMatrix.dx ()
            << " dy=" << transformMatrix.dy ()
            << endl;
#endif
    painter.setMatrix (transformMatrix);
#if DEBUG_KP_PIXMAP_FX && 0
    kDebug () << "\ttranslate top=" << painter.xForm (QPoint (0, 0)) << endl;
    kDebug () << "\tmatrix: m11=" << painter.worldMatrix ().m11 ()
               << " m12=" << painter.worldMatrix ().m12 ()
               << " m21=" << painter.worldMatrix ().m21 ()
               << " m22=" << painter.worldMatrix ().m22 ()
               << " dx=" << painter.worldMatrix ().dx ()
               << " dy=" << painter.worldMatrix ().dy ()
               << endl;
#endif
    painter.drawPixmap (QPoint (0, 0), pm);
    painter.end ();

    if (!newBitmapMask.isNull ())
    {
        QPainter maskPainter (&newBitmapMask);
        maskPainter.setMatrix (transformMatrix);
        maskPainter.drawPixmap (QPoint (0, 0), kpPixmapFX::getNonNullMask (pm));
        maskPainter.end ();
        newPixmap.setMask (newBitmapMask);
    }

    return newPixmap;
}

// public static
QMatrix kpPixmapFX::skewMatrix (int width, int height, double hangle, double vangle)
{
    if (fabs (hangle - 0) < kpPixmapFX::AngleInDegreesEpsilon &&
        fabs (vangle - 0) < kpPixmapFX::AngleInDegreesEpsilon)
    {
        return QMatrix ();
    }


    /* Diagram for completeness :)
     *
     *       |---------- w ----------|
     *     (0,0)
     *  _     _______________________ (w,0)
     *  |    |\~_ va                 |
     *  |    | \ ~_                  |
     *  |    |ha\  ~__               |
     *       |   \    ~__            | dy
     *  h    |    \      ~___        |
     *       |     \         ~___    |
     *  |    |      \            ~___| (w,w*tan(va)=dy)
     *  |    |       \         *     \
     *  _    |________\________|_____|\                                     vertical shear factor
     *     (0,h) dx   ^~_      |       \                                             |
     *                |  ~_    \________\________ General Point (x,y)                V
     *                |    ~__           \        Skewed Point (x + y*tan(ha),y + x*tan(va))
     *      (h*tan(ha)=dx,h)  ~__         \                             ^
     *                           ~___      \                            |
     *                               ~___   \                   horizontal shear factor
     *   Key:                            ~___\
     *    ha = hangle                         (w + h*tan(ha)=w+dx,h + w*tan(va)=w+dy)
     *    va = vangle
     *
     * Skewing really just twists a rectangle into a parallelogram.
     *
     */

    //QWMatrix matrix (1, tan (KP_DEGREES_TO_RADIANS (vangle)), tan (KP_DEGREES_TO_RADIANS (hangle)), 1, 0, 0);
    // I think this is clearer than above :)
    QMatrix matrix;
    matrix.shear (tan (KP_DEGREES_TO_RADIANS (hangle)),
                  tan (KP_DEGREES_TO_RADIANS (vangle)));

    return matrixWithZeroOrigin (matrix, width, height);
}

// public static
QMatrix kpPixmapFX::skewMatrix (const QPixmap &pixmap, double hangle, double vangle)
{
    return kpPixmapFX::skewMatrix (pixmap.width (), pixmap.height (), hangle, vangle);
}


// public static
void kpPixmapFX::skew (QPixmap *destPixmapPtr, double hangle, double vangle,
                       const kpColor &backgroundColor,
                       int targetWidth, int targetHeight)
{
    if (!destPixmapPtr)
        return;

    *destPixmapPtr = kpPixmapFX::skew (*destPixmapPtr, hangle, vangle,
                                       backgroundColor,
                                       targetWidth, targetHeight);
}

// public static
QPixmap kpPixmapFX::skew (const QPixmap &pm, double hangle, double vangle,
                          const kpColor &backgroundColor,
                          int targetWidth, int targetHeight)
{
#if DEBUG_KP_PIXMAP_FX
    kDebug () << "kpPixmapFX::skew() pm.width=" << pm.width ()
               << " pm.height=" << pm.height ()
               << " hangle=" << hangle
               << " vangle=" << vangle
               << " targetWidth=" << targetWidth
               << " targetHeight=" << targetHeight
               << endl;
#endif

    if (fabs (hangle - 0) < kpPixmapFX::AngleInDegreesEpsilon &&
        fabs (vangle - 0) < kpPixmapFX::AngleInDegreesEpsilon &&
        (targetWidth <= 0 && targetHeight <= 0)/*don't want to scale?*/)
    {
        return pm;
    }

    if (fabs (hangle) > 90 - kpPixmapFX::AngleInDegreesEpsilon ||
        fabs (vangle) > 90 - kpPixmapFX::AngleInDegreesEpsilon)
    {
        kError () << "kpPixmapFX::skew() passed hangle and/or vangle out of range (-90 < x < 90)" << endl;
        return pm;
    }


    QMatrix matrix = skewMatrix (pm, hangle, vangle);

    return ::xForm (pm, matrix, backgroundColor, targetWidth, targetHeight);
}


// public static
QMatrix kpPixmapFX::rotateMatrix (int width, int height, double angle)
{
    if (fabs (angle - 0) < kpPixmapFX::AngleInDegreesEpsilon)
    {
        return QMatrix ();
    }

    QMatrix matrix;
    matrix.translate (width / 2, height / 2);
    matrix.rotate (angle);

    return matrixWithZeroOrigin (matrix, width, height);
}

// public static
QMatrix kpPixmapFX::rotateMatrix (const QPixmap &pixmap, double angle)
{
    return kpPixmapFX::rotateMatrix (pixmap.width (), pixmap.height (), angle);
}


// public static
bool kpPixmapFX::isLosslessRotation (double angle)
{
    const double angleIn = angle;

    // Reflect angle into positive if negative
    if (angle < 0)
        angle = -angle;

    // Remove multiples of 90 to make sure 0 <= angle <= 90
    angle -= ((int) angle) / 90 * 90;

    // "Impossible" situation?
    if (angle < 0 || angle > 90)
    {
        kError () << "kpPixmapFX::isLosslessRotation(" << angleIn
                   << ") result=" << angle
                   << endl;
        return false;  // better safe than sorry
    }

    const bool ret = (angle < kpPixmapFX::AngleInDegreesEpsilon ||
                      90 - angle < kpPixmapFX::AngleInDegreesEpsilon);
#if DEBUG_KP_PIXMAP_FX
    kDebug () << "kpPixmapFX::isLosslessRotation(" << angleIn << ")"
               << "  residual angle=" << angle
               << "  returning " << ret
               << endl;
#endif
    return ret;
}


// public static
void kpPixmapFX::rotate (QPixmap *destPixmapPtr, double angle,
                         const kpColor &backgroundColor,
                         int targetWidth, int targetHeight)
{
    if (!destPixmapPtr)
        return;

    *destPixmapPtr = kpPixmapFX::rotate (*destPixmapPtr, angle,
                                         backgroundColor,
                                         targetWidth, targetHeight);
}

// public static
QPixmap kpPixmapFX::rotate (const QPixmap &pm, double angle,
                            const kpColor &backgroundColor,
                            int targetWidth, int targetHeight)
{
    if (fabs (angle - 0) < kpPixmapFX::AngleInDegreesEpsilon &&
        (targetWidth <= 0 && targetHeight <= 0)/*don't want to scale?*/)
    {
        return pm;
    }


    QMatrix matrix = rotateMatrix (pm, angle);

    return ::xForm (pm, matrix, backgroundColor, targetWidth, targetHeight);
}


// public static
QMatrix kpPixmapFX::flipMatrix (int width, int height, bool horz, bool vert)
{
    if (width <= 0 || height <= 0)
    {
        kError () << "kpPixmapFX::flipMatrix() passed invalid dimensions" << endl;
        return QMatrix ();
    }

    return QMatrix (horz ? -1 : +1,  // m11
                     0,  // m12
                     0,  // m21
                     vert ? -1 : +1,  // m22
                     horz ? (width - 1) : 0,  // dx
                     vert ? (height - 1) : 0);  // dy
}

// public static
QMatrix kpPixmapFX::flipMatrix (const QPixmap &pixmap, bool horz, bool vert)
{
    return kpPixmapFX::flipMatrix (pixmap.width (), pixmap.height (),
                                   horz, vert);
}


// public static
void kpPixmapFX::flip (QPixmap *destPixmapPtr, bool horz, bool vert)
{
    if (!horz && !vert)
        return;

    *destPixmapPtr = kpPixmapFX::flip (*destPixmapPtr, horz, vert);
}

// public static
QPixmap kpPixmapFX::flip (const QPixmap &pm, bool horz, bool vert)
{
    if (!horz && !vert)
        return pm;

    return pm.transformed (flipMatrix (pm, horz, vert));
}

// public static
void kpPixmapFX::flip (QImage *destImagePtr, bool horz, bool vert)
{
    if (!horz && !vert)
        return;

    *destImagePtr = kpPixmapFX::flip (*destImagePtr, horz, vert);
}

// public static
QImage kpPixmapFX::flip (const QImage &img, bool horz, bool vert)
{
    if (!horz && !vert)
        return img;

    return img.mirrored (horz, vert);
}


// public static
QPen kpPixmapFX::QPainterDrawRectPen (const QColor &color, int qtWidth)
{
    return QPen (color, qtWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
}

// public static
QPen kpPixmapFX::QPainterDrawLinePen (const QColor &color, int qtWidth)
{
    return QPen (color, qtWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}
