/*********************************************************************************************
 * Mozilla License
 * Just a meantime project to see the ability of qt, the framework that my OS might be based on
 * And for those linux users that believe in the power of notes
 *********************************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtGui>
#include <QtCore>
#include <QGroupBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QSplitter>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QProgressDialog>
#include <QAction>
#include <QAutostart>
#include <QtGlobal>
#include <QWidget>
#include <QQuickView>
#include <QQmlContext>
#include <QVariant>
#include <QJsonObject>
#include <QObject>

#include "nodedata.h"
#include "notelistmodel.h"
#include "notelistview.h"
#include "nodetreemodel.h"

#if defined(UPDATE_CHECKER)
#  include "updaterwindow.h"
#endif

#include "dbmanager.h"
#include "customDocument.h"
#include "aboutwindow.h"
#include "framelesswindow.h"
#include "nodetreeview.h"
#include "editorsettingsoptions.h"

#include "markdownhighlighter.h"
#include "blockmodel.h"

namespace Ui {
class MainWindow;
}
class TreeViewLogic;
class ListViewLogic;
class NoteEditorLogic;
class TagPool;
class SplitterStyle;

#if defined(Q_OS_WINDOWS) || defined(Q_OS_WIN)
// #if defined(__MINGW32__) || defined(__GNUC__)
using MainWindowBase = QMainWindow;
// #else
//  using MainWindowBase = CFramelessWindow;
// #endif
#elif defined(Q_OS_MACOS)
using MainWindowBase = CFramelessWindow;
#else
using MainWindowBase = QMainWindow;
#endif
class MainWindow : public MainWindowBase
{
    Q_OBJECT

public:
    enum class ShadowType { Linear = 0, Radial };

    enum class ShadowSide {
        Left = 0,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    enum class StretchSide {
        None = 0,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    Q_ENUM(ShadowType)
    Q_ENUM(ShadowSide)
    Q_ENUM(StretchSide)

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setMainWindowVisibility(bool state);

public slots:
    void createNewNote();
    void saveLastSelectedFolderTags(bool isFolder, const QString &folderPath,
                                    const QSet<int> &tagId);
    void saveExpandedFolder(const QStringList &folderPaths);
    void saveLastSelectedNote(const QSet<int> &notesId);
    void changeEditorFontTypeFromStyleButtons(FontTypeface::Value fontType, int chosenFontIndex);
    void changeEditorFontSizeFromStyleButtons(FontSizeAction::Value fontSizeAction);
    void resetEditorSettings();
    void setTheme(Theme::Value theme);
    void collapseNoteList();
    void expandNoteList();
    void collapseFolderTree();
    void expandFolderTree();
    void setMarkdownEnabled(bool isMarkdownEnabled);
    void stayOnTop(bool checked);
    void moveCurrentNoteToTrash();
    void setEditorSettingsFromQuickViewVisibility(bool isVisible);
    void setEditorSettingsScrollBarPosition(double position);
    void setActivationSuccessful();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    Ui::MainWindow *ui;

    QSettings *m_settingsDatabase;
    QToolButton *m_clearButton;
    QToolButton *m_searchButton;
    QPushButton *m_greenMaximizeButton;
    QPushButton *m_redCloseButton;
    QPushButton *m_yellowMinimizeButton;
    QHBoxLayout m_trafficLightLayout;
    QPushButton *m_newNoteButton;
    QPushButton *m_globalSettingsButton;
    QPushButton *m_toggleTreeViewButton;
    NoteEditorLogic *m_noteEditorLogic;
    QLineEdit *m_searchEdit;
    QSplitter *m_splitter;
    QWidget *m_noteListWidget;
    QWidget *m_foldersWidget;
    QSystemTrayIcon *m_trayIcon;
#if !defined(Q_OS_MAC)
    QAction *m_restoreAction;
    QAction *m_quitAction;
#endif

    NoteListView *m_listView;
    NoteListModel *m_listModel;
    ListViewLogic *m_listViewLogic;
    NodeTreeView *m_treeView;
    NodeTreeModel *m_treeModel;
    TreeViewLogic *m_treeViewLogic;
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    QQuickView m_blockEditorQuickView;
    QWidget *m_blockEditorWidget;
#endif
    QQuickView m_editorSettingsQuickView;
    QWidget *m_editorSettingsWidget;
    TagPool *m_tagPool;
    DBManager *m_dbManager;
    QThread *m_dbThread;
    SplitterStyle *m_splitterStyle;
#if defined(UPDATE_CHECKER)
    UpdaterWindow m_updater;
#endif
    AboutWindow m_aboutWindow;
    StretchSide m_stretchSide;
    Autostart m_autostart;
    int m_mousePressX;
    int m_mousePressY;
    int m_trashCounter;
    int m_layoutMargin;
    int m_shadowWidth;
    int m_nodeTreeWidth;
    int m_smallEditorWidth;
    int m_largeEditorWidth;
    bool m_canMoveWindow;
    bool m_canStretchWindow;
    bool m_isTemp;
    bool m_isListViewScrollBarHidden;
    bool m_isOperationRunning;
#if defined(UPDATE_CHECKER)
    bool m_dontShowUpdateWindow;
#endif
    bool m_alwaysStayOnTop;
    bool m_useNativeWindowFrame;
    bool m_hideToTray;

    QString m_styleSheet;

    QStringList m_listOfSerifFonts;
    QStringList m_listOfSansSerifFonts;
    QStringList m_listOfMonoFonts;
    int m_chosenSerifFontIndex;
    int m_chosenSansSerifFontIndex;
    int m_chosenMonoFontIndex;
    int m_editorMediumFontSize;
    int m_currentFontPointSize;
    struct m_charsLimitPerFont
    {
        int mono;
        int serif;
        int sansSerif;
    } m_currentCharsLimitPerFont;
    FontTypeface::Value m_currentFontTypeface;
    QString m_currentFontFamily;
    QFont m_currentSelectedFont;
    QString m_displayFont;
    Theme::Value m_currentTheme;
    QColor m_currentEditorTextColor;
    bool m_areNonEditorWidgetsVisible;
    bool m_isEditorSettingsFromQuickViewVisible;
    bool m_isProVersionActivated;
    QSettings *m_localLicenseData;
    BlockModel *m_blockModel;

    void setupMainWindow();
    void setupFonts();
    void setupTrayIcon();
    void setupKeyboardShortcuts();
    void setupSplitter();
    void setupButtons();
    void setupSignalsSlots();
#if defined(UPDATE_CHECKER)
    void autoCheckForUpdates();
#endif
    void setupSearchEdit();
    void setupEditorSettings();
    void setupTextEdit();
    void setupBlockEditorView();
    void setupDatabases();
    void setupModelView();
    void initializeSettingsDatabase();
    void setLayoutForScrollArea();
    void setButtonsAndFieldsEnabled(bool doEnable);
    void restoreStates();
    void migrateFromV0_9_0();
    void executeImport(const bool replace);
    void migrateNoteFromV0_9_0(const QString &notePath);
    void migrateTrashFromV0_9_0(const QString &trashPath);
    void setCurrentFontBasedOnTypeface(FontTypeface::Value selectedFontTypeFace);
    void setWindowButtonsVisible(bool isVisible);
    void updateSelectedOptionsEditorSettings();
    void dropShadow(QPainter &painter, ShadowType type, ShadowSide side);
    void fillRectWithGradient(QPainter &painter, QRect rect, QGradient &gradient);
    double gaussianDist(double x, const double center, double sigma) const;
    void checkProVersion();

    void setMargins(QMargins margins);

private slots:
    void InitData();

    void onSystemTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onNewNoteButtonClicked();
    void onGlobalSettingsButtonClicked();
    void onClearButtonClicked();
    void onGreenMaximizeButtonPressed();
    void onYellowMinimizeButtonPressed();
    void onRedCloseButtonPressed();
    void onGreenMaximizeButtonClicked();
    void onYellowMinimizeButtonClicked();
    void onRedCloseButtonClicked();
    void selectNoteDown();
    void selectNoteUp();
    void setFocusOnText();
    void fullscreenWindow();
    void maximizeWindow();
    void minimizeWindow();
    void QuitApplication();
#if defined(UPDATE_CHECKER)
    void checkForUpdates();
#endif
    void toggleNoteList();
    void toggleFolderTree();
    void importNotesFile();
    void exportNotesFile();
    void restoreNotesFile();
    void setUseNativeWindowFrame(bool useNativeWindowFrame);
    void setHideToTray(bool enabled);
    void toggleStayOnTop();
    void onSearchEditReturnPressed();
    void deleteSelectedNote();
    void clearSearch();
    void showErrorMessage(const QString &title, const QString &content);
    void setNoteListLoading();
    void selectAllNotesInList();
    void updateFrame();
    bool isTitleBar(int x, int y) const;

signals:
    void focusOnEditor();
    void mainWindowDeactivated();
    void requestNodesTree();
    void requestOpenDBManager(const QString &path, bool doCreate);
    void requestRestoreNotes(const QString &filePath);
    void requestImportNotes(const QString &filePath);
    void requestExportNotes(QString fileName);
    void requestMigrateNotesFromV0_9_0(QVector<NodeData> &noteList);
    void requestMigrateTrashFromV0_9_0(QVector<NodeData> &noteList);
    void requestMigrateNotesFromV1_5_0(const QString &path);
    void requestChangeDatabasePath(const QString &newPath);
    void themeChanged(QVariant theme);
    void platformSet(QVariant platform);
    void qtVersionSet(QVariant qtVersion);
    void editorSettingsShowed(QVariant data);
    void mainWindowResized(QVariant data);
    void mainWindowMoved(QVariant data);
    void displayFontSet(QVariant data);
    void settingsChanged(QVariant data);
    void fontsChanged(QVariant data);
    void toggleEditorSettingsKeyboardShorcutFired();
    void editorSettingsScrollBarPositionChanged(QVariant data);
    void proVersionCheck(QVariant data);
};

#endif // MAINWINDOW_H
