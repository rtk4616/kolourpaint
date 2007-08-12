
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


#ifndef KP_MAIN_WINDOW_H
#define KP_MAIN_WINDOW_H


#include <kxmlguiwindow.h>
#include <kurl.h>

#include <kpDefs.h>
#include <kpPixmapFX.h>


class QAction;
class QActionGroup;
class QDragEnterEvent;
class QDropEvent;
class QMenu;
class QMoveEvent;
class QPixmap;
class QPoint;
class QRect;
class QSize;
class QStringList;

class KConfigGroup;
class KFontAction;
class KFontSizeAction;
class KSelectAction;
class KToggleAction;
class KToolBar;
class KPrinter;
class KRecentFilesAction;
class KScanDialog;
class KToggleFullScreenAction;

class kpColor;
class kpColorCells;
class kpColorToolBar;
class kpCommand;
class kpCommandEnvironment;
class kpCommandHistory;
class kpDocument;
class kpDocumentEnvironment;
class kpDocumentMetaInfo;
class kpDocumentSaveOptions;
class kpViewManager;
class kpViewScrollableContainer;
class kpImageSelectionTransparency;
class kpSqueezedTextLabel;
class kpTextStyle;
class kpThumbnail;
class kpThumbnailView;
class kpTool;
class kpToolEnvironment;
class kpToolSelectionEnvironment;
class kpToolText;
class kpToolToolBar;
class kpTransformDialogEnvironment;
class kpZoomedView;


class kpMainWindow : public KXmlGuiWindow
{
Q_OBJECT

public:
    // Opens a new window with a blank document.
    kpMainWindow ();

    // Opens a new window with the document specified by <url>
    // or creates a blank document if <url> could not be opened.
    kpMainWindow (const KUrl &url);

    // Opens a new window with the document <newDoc>
    // (<newDoc> can be 0 although this would result in a new
    //  window without a document at all).
    kpMainWindow (kpDocument *newDoc);

private:
    void readGeneralSettings ();
    void readThumbnailSettings ();
    void init ();

    // (only called for restoring a previous session e.g. starting KDE with
    //  a previously saved session; it's not called on normal KolourPaint
    //  startup)
    virtual void readProperties (const KConfigGroup &configGroup);
    // (only called for saving the current session e.g. logging out of KDE
    //  with the KolourPaint window open; it's not called on normal KolourPaint
    //  exit)
    virtual void saveProperties (KConfigGroup &configGroup);

public:
    ~kpMainWindow ();

public:
    kpDocument *document () const;
    kpDocumentEnvironment *documentEnvironment ();
    kpViewManager *viewManager () const;
    kpColorToolBar *colorToolBar () const;
    kpColorCells *colorCells () const;
    kpToolToolBar *toolToolBar () const;
    kpCommandHistory *commandHistory () const;
    kpCommandEnvironment *commandEnvironment ();

private:
    void setupActions ();
    void enableDocumentActions (bool enable = true);

private:
    void setDocument (kpDocument *newDoc);

    virtual bool queryClose ();

    virtual void dragEnterEvent (QDragEnterEvent *e);
    virtual void dropEvent (QDropEvent *e);

private slots:
    void slotScrollViewAboutToScroll ();
    void slotScrollViewAfterScroll ();

private:
    virtual void moveEvent (QMoveEvent *e);

private slots:
    void slotUpdateCaption ();
    void slotDocumentRestored ();


    //
    // Tools
    //

private:
    kpToolSelectionEnvironment *toolSelectionEnvironment ();
    kpToolEnvironment *toolEnvironment ();

    void setupToolActions ();
    void createToolBox ();
    void enableToolsDocumentActions (bool enable = true);

private slots:
    void updateToolOptionPrevNextActionsEnabled ();
    void updateActionDrawOpaqueChecked ();
private:
    void updateActionDrawOpaqueEnabled ();

public:
    QActionGroup *toolsActionGroup ();

    kpTool *tool () const;

    bool toolHasBegunShape () const;
    bool toolIsASelectionTool (bool includingTextTool = true) const;
    bool toolIsTextTool () const;

private:
    // Ends the current shape.  If there is no shape currently being drawn,
    // it does nothing.
    //
    // In general, call this at the start of every kpMainWindow slot,
    // directly invoked by the _user_ (by activating an action or via another
    // way), so that:
    //
    // 1. The document contains the pixels of that shape:
    //
    //    Most tools have the shape, currently being drawn, layered above the
    //    document as a kpTempImage.  In other words, the document does not
    //    yet contain the pixels of that shape.  By ending the shape, the layer
    //    is pushed down onto the document so that it now contains those
    //    pixels.  Your slot can now safely read the document as it's now
    //    consistent with what's on the screen.
    //
    //    Note that selection layers are not pushed down by this method.
    //    This is a feature, not a bug.  The user would be annoyed if e.g.
    //    slotSave() happened to push down the selection.  Use
    //    kpDocument::imageWithSelection() to get around this problem.  You
    //    should still call toolEndShape() even if a selection is active
    //    -- this ends selection "shapes", which are actually things like
    //    selection moves or smearing operations, rather than the selections
    //    themselves.
    //
    // AND/OR:
    //
    // 2. The current tool is no longer in a drawing state:
    //
    //    If your slot is going to bring up a new main window or modal dialog
    //    or at least some widget that acquires mouse or keyboard focus, this
    //    could confuse the tool if the tool is in the middle of a drawing
    //    operation.
    //
    // Do not call this in slots not invoked by the user.  For instance,
    // calling this method in response to an internal timer tick would be
    // wrong.  The user's drawing operation would unexpectedly finish and
    // this would bewilder and irritate the user.
    //
    // TODO: Help / KolourPaint Handbook does not call this.  I'm sure there
    //       are a few other actions that don't call this but should.
    void toolEndShape ();

public:
    kpImageSelectionTransparency imageSelectionTransparency () const;
    void setImageSelectionTransparency (const kpImageSelectionTransparency &transparency,
                                   bool forceColorChange = false);
    int settingImageSelectionTransparency () const;

private slots:
    void slotToolSelected (kpTool *tool);

private:
    void readLastTool ();
    int toolNumber () const;
    void saveLastTool ();

private:
    bool maybeDragScrollingMainView () const;
private slots:
    bool slotDragScroll (const QPoint &docPoint,
                         const QPoint &docLastPoint,
                         int zoomLevel,
                         bool *didSomething);
    bool slotEndDragScroll ();

private slots:
    void slotBeganDocResize ();
    void slotContinuedDocResize (const QSize &size);
    void slotCancelledDocResize ();
    void slotEndedDocResize (const QSize &size);

    void slotDocResizeMessageChanged (const QString &string);

private slots:
    void slotActionPrevToolOptionGroup1 ();
    void slotActionNextToolOptionGroup1 ();
    void slotActionPrevToolOptionGroup2 ();
    void slotActionNextToolOptionGroup2 ();

    void slotActionDrawOpaqueToggled ();
    void slotActionDrawColorSimilarity ();

public slots:
    void slotToolSpraycan ();
    void slotToolBrush ();
    void slotToolColorEraser ();
    void slotToolColorPicker ();
    void slotToolCurve ();
    void slotToolEllipse ();
    void slotToolEllipticalSelection ();
    void slotToolEraser ();
    void slotToolFloodFill ();
    void slotToolFreeFormSelection ();
    void slotToolLine ();
    void slotToolPen ();
    void slotToolPolygon ();
    void slotToolPolyline ();
    void slotToolRectangle ();
    void slotToolRectSelection ();
    void slotToolRoundedRectangle ();
    void slotToolText ();
    void slotToolZoom ();


    //
    // File Menu
    //

private:
    void setupFileMenuActions ();
    void enableFileMenuDocumentActions (bool enable = true);

    void addRecentURL (const KUrl &url);

private slots:
    void slotNew ();

private:
    QSize defaultDocSize () const;
    void saveDefaultDocSize (const QSize &size);

private:
    bool shouldOpen ();
    void setDocumentChoosingWindow (kpDocument *doc);

private:
    kpDocument *openInternal (const KUrl &url,
        const QSize &fallbackDocSize,
        bool newDocSameNameIfNotExist);
    // Same as above except that it:
    //
    // 1. Assumes a default fallback document size.
    // 2. If the URL is successfully opened (with the special exception of
    //    the "kolourpaint doesnotexist.png" case), it is bubbled up to the
    //    top in the Recent Files Action.
    //
    // As a result of this behavior, this should only be called in response
    // to a user open request e.g. File / Open or "kolourpaint doesexist.png".
    // It should not be used for session restore - in that case, it does not
    // make sense to bubble the Recent Files list.
    bool open (const KUrl &url, bool newDocSameNameIfNotExist = false);

    KUrl::List askForOpenURLs (const QString &caption,
                               const QString &startURL,
                               bool allowMultipleURLs = true);

private slots:
    void slotOpen ();
    void slotOpenRecent (const KUrl &url);

    void slotScan ();
    void slotScanned (const QImage &image, int);

    void slotProperties ();

    bool save (bool localOnly = false);
    bool slotSave ();

private:
    KUrl askForSaveURL (const QString &caption,
                        const QString &startURL,
                        const QPixmap &pixmapToBeSaved,
                        const kpDocumentSaveOptions &startSaveOptions,
                        const kpDocumentMetaInfo &docMetaInfo,
                        const QString &forcedSaveOptionsGroup,
                        bool localOnly,
                        kpDocumentSaveOptions *chosenSaveOptions,
                        bool isSavingForFirstTime,
                        bool *allowOverwritePrompt,
                        bool *allowLossyPrompt);

private slots:
    bool saveAs (bool localOnly = false);
    bool slotSaveAs ();

    bool slotExport ();

    void slotEnableReload ();
    bool slotReload ();

private:
    void sendFilenameToPrinter (KPrinter *printer);
    void sendPixmapToPrinter (KPrinter *printer, bool showPrinterSetupDialog);

private slots:
    void slotPrint ();
    void slotPrintPreview ();

    void slotMail ();

private:
    void setAsWallpaper (bool centered);
private slots:
    void slotSetAsWallpaperCentered ();
    void slotSetAsWallpaperTiled ();

    void slotClose ();
    void slotQuit ();


    //
    // Edit Menu
    //

private:
    kpPixmapFX::WarnAboutLossInfo pasteWarnAboutLossInfo ();
    void setupEditMenuActions ();
    void enableEditMenuDocumentActions (bool enable = true);

public:
    QMenu *selectionToolRMBMenu ();

private slots:
    void slotCut ();
    void slotCopy ();
    void slotEnablePaste ();
private:
    QRect calcUsefulPasteRect (int pixmapWidth, int pixmapHeight);
    void paste (const kpAbstractSelection &sel,
                bool forceTopLeft = false);
public:
    // (<forceNewTextSelection> is ignored if <text> is empty)
    void pasteText (const QString &text,
                    bool forceNewTextSelection = false,
                    const QPoint &newTextSelectionTopLeft = KP_INVALID_POINT);
    void pasteTextAt (const QString &text, const QPoint &point,
                      // Allow tiny adjustment of <point> so that mouse
                      // pointer is not exactly on top of the topLeft of
                      // any new text selection (so that it doesn't look
                      // weird by being on top of a resize handle just after
                      // a paste).
                      bool allowNewTextSelectionPointShift = false);
public slots:
    void slotPaste ();
private slots:
    void slotPasteInNewWindow ();
public slots:
    void slotDelete ();

    void slotSelectAll ();
private:
    void addDeselectFirstCommand (kpCommand *cmd);
public slots:
    void slotDeselect ();
private slots:
    void slotCopyToFile ();
    void slotPasteFromFile ();


    //
    // View Menu
    //

private:
    void setupViewMenuActions ();

    bool viewMenuDocumentActionsEnabled () const;
    void enableViewMenuDocumentActions (bool enable = true);

private:
    void actionShowGridUpdate ();
private slots:
    void slotShowGridToggled ();
private:
    void updateMainViewGrid ();

private:
    QRect mapToGlobal (const QRect &rect) const;
    QRect mapFromGlobal (const QRect &rect) const;


    //
    // View Menu - Zoom
    //

private:
    void setupViewMenuZoomActions ();
    void enableViewMenuZoomDocumentActions (bool enable);

private:
    void sendZoomListToActionZoom ();

    void zoomToPre (int zoomLevel);
    void zoomToPost ();

public:
    void zoomTo (int zoomLevel, bool centerUnderCursor = false);
    void zoomToRect (const QRect &normalizedDocRect);

private slots:
    void slotActualSize ();
    void slotFitToPage ();
    void slotFitToWidth ();
    void slotFitToHeight ();

public:
    void zoomIn (bool centerUnderCursor = false);
    void zoomOut (bool centerUnderCursor = false);

public slots:
    void slotZoomIn ();
    void slotZoomOut ();

private:
    void zoomAccordingToZoomAction (bool centerUnderCursor = false);

private slots:
    void slotZoom ();


    //
    // View Menu - Thumbnail
    //

private:
    void setupViewMenuThumbnailActions ();
    void enableViewMenuThumbnailDocumentActions (bool enable);

private slots:
    void slotDestroyThumbnail ();
    void slotDestroyThumbnailInitatedByUser ();
    void slotCreateThumbnail ();

public:
    void notifyThumbnailGeometryChanged ();

private slots:
    void slotSaveThumbnailGeometry ();
    void slotShowThumbnailToggled ();
    void updateThumbnailZoomed ();
    void slotZoomedThumbnailToggled ();
    void slotThumbnailShowRectangleToggled ();

private:
    void enableViewZoomedThumbnail (bool enable = true);
    void enableViewShowThumbnailRectangle (bool enable = true);
    void enableThumbnailOptionActions (bool enable = true);
    void createThumbnailView ();
    void destroyThumbnailView ();
    void updateThumbnail ();


    //
    // Image Menu
    //

private:
    kpTransformDialogEnvironment *transformDialogEnvironment ();

    bool isSelectionActive () const;
    bool isTextSelection () const;

    QString autoCropText () const;

    void setupImageMenuActions ();
    void enableImageMenuDocumentActions (bool enable = true);

private slots:
    void slotImageMenuUpdateDueToSelection ();

public:
    kpColor backgroundColor (bool ofSelection = false) const;
    void addImageOrSelectionCommand (kpCommand *cmd,
                                     bool addSelCreateCmdIfSelAvail = true,
                                     bool addSelPullCmdIfSelAvail = true);

private slots:
    void slotResizeScale ();
public slots:
    void slotCrop ();
private slots:
    void slotAutoCrop ();
    void slotFlip ();

    void slotRotate ();
    void slotRotate270 ();
    void slotRotate90 ();

    void slotSkew ();
    void slotConvertToBlackAndWhite ();
    void slotConvertToGrayscale ();
    void slotInvertColors ();
    void slotClear ();
    void slotMoreEffects ();


    //
    // Colors Menu
    //

private:
    void setupColorsMenuActions ();
    void enableColorsMenuDocumentActions (bool enable);

private:
    void deselectActionColorsKDE ();

private slots:
    void slotColorsDefault ();
    void slotColorsKDE ();
    void slotColorsOpen ();

    void slotColorsSaveAs ();

    void slotColorsAppendRow ();
    void slotColorsDeleteRow ();


    //
    // Settings Menu
    //

private:
    void setupSettingsMenuActions ();
    void enableSettingsMenuDocumentActions (bool enable = true);

private slots:
    void slotFullScreen ();

    void slotEnableSettingsShowPath ();
    void slotShowPathToggled ();

    void slotKeyBindings ();

    void slotConfigureToolBars ();
    void slotNewToolBarConfig ();

    void slotConfigure ();


    //
    // Status Bar
    //

private:
    enum
    {
        StatusBarItemMessage,
        StatusBarItemShapePoints,
        StatusBarItemShapeSize,
        StatusBarItemDocSize,
        StatusBarItemDocDepth,
        StatusBarItemZoom
    };

    void addPermanentStatusBarItem (int id, int maxTextLen);
    void createStatusBar ();

private slots:
    void setStatusBarMessage (const QString &message = QString::null);
    void setStatusBarShapePoints (const QPoint &startPoint = KP_INVALID_POINT,
                                  const QPoint &endPoint = KP_INVALID_POINT);
    void setStatusBarShapeSize (const QSize &size = KP_INVALID_SIZE);
    void setStatusBarDocSize (const QSize &size = KP_INVALID_SIZE);
    void setStatusBarDocDepth (int depth = 0);
    void setStatusBarZoom (int zoom = 0);

    void recalculateStatusBarMessage ();
    void recalculateStatusBarShape ();

    void recalculateStatusBar ();


    //
    // Text ToolBar
    //

private:
    void setupTextToolBarActions ();
    void readAndApplyTextSettings ();

public:
    void enableTextToolBarActions (bool enable = true);

private slots:
    void slotTextFontFamilyChanged ();
    void slotTextFontSizeChanged ();
    void slotTextBoldChanged ();
    void slotTextItalicChanged ();
    void slotTextUnderlineChanged ();
    void slotTextStrikeThruChanged ();

public:
    KToolBar *textToolBar ();
    bool isTextStyleBackgroundOpaque () const;
    kpTextStyle textStyle () const;
    void setTextStyle (const kpTextStyle &textStyle_);
    int settingTextStyle () const;


    //
    // Help Menu
    //
private:
    void setupHelpMenuActions ();
    void enableHelpMenuDocumentActions (bool enable = true);

private slots:
    void slotHelpTakingScreenshots ();
    void slotHelpTakingScreenshotsFollowLink (const QString &link);


private:
    struct kpMainWindowPrivate *d;
};


#endif  // KP_MAIN_WINDOW_H