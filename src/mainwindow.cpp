/**************************************************************************************
 * We believe in the power of notes to help us record ideas and thoughts.
 * We want people to have an easy, beautiful and simple way of doing that.
 * And so we have Notes.
 ***************************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qxtglobalshortcut.h"
#include "treeviewlogic.h"
#include "listviewlogic.h"
#include "noteeditorlogic.h"
#include "tagpool.h"
#include "splitterstyle.h"
#include "editorsettingsoptions.h"

#include <QScrollBar>
#include <QShortcut>
#include <QTextStream>
#include <QScrollArea>
#include <QtConcurrent>
#include <QProgressDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QList>
#include <QWidgetAction>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>

#define DEFAULT_DATABASE_NAME "default_database"

/*!
 * \brief MainWindow::MainWindow
 * \param parent
 */
MainWindow::MainWindow(QWidget *parent)
    : MainWindowBase(parent),
      ui(new Ui::MainWindow),
      m_settingsDatabase(nullptr),
      m_clearButton(nullptr),
      m_searchButton(nullptr),
      m_greenMaximizeButton(nullptr),
      m_redCloseButton(nullptr),
      m_yellowMinimizeButton(nullptr),
      m_trafficLightLayout(nullptr),
      m_newNoteButton(nullptr),
      m_globalSettingsButton(nullptr),
      m_noteEditorLogic(nullptr),
      m_searchEdit(nullptr),
      m_splitter(nullptr),
      m_trayIcon(new QSystemTrayIcon(this)),
#if !defined(Q_OS_MAC)
      m_restoreAction(new QAction(tr("&Hide Notes"), this)),
      m_quitAction(new QAction(tr("&Quit"), this)),
#endif
      m_listView(nullptr),
      m_listModel(nullptr),
      m_listViewLogic(nullptr),
      m_treeView(nullptr),
      m_treeModel(new NodeTreeModel(this)),
      m_treeViewLogic(nullptr),
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
      m_blockEditorQuickView(nullptr),
      m_blockEditorWidget(this),
#endif
      m_editorSettingsQuickView(nullptr),
      m_editorSettingsWidget(new QWidget(this)),
      m_tagPool(nullptr),
      m_dbManager(nullptr),
      m_dbThread(nullptr),
      m_aboutWindow(this),
      m_trashCounter(0),
      m_layoutMargin(10),
      m_shadowWidth(10),
      m_smallEditorWidth(420),
      m_largeEditorWidth(1250),
      m_canMoveWindow(false),
      m_canStretchWindow(false),
      m_isTemp(false),
      m_isListViewScrollBarHidden(true),
      m_isOperationRunning(false),
#if defined(UPDATE_CHECKER)
      m_dontShowUpdateWindow(false),
#endif
      m_alwaysStayOnTop(false),
      m_useNativeWindowFrame(false),
      m_hideToTray(false),
      m_listOfSerifFonts(
              { QStringLiteral("Trykker"), QStringLiteral("PT Serif"), QStringLiteral("Mate") }),
      m_listOfSansSerifFonts({ QStringLiteral("Source Sans Pro"), QStringLiteral("Roboto") }),
      m_listOfMonoFonts({ QStringLiteral("iA Writer Mono S"), QStringLiteral("iA Writer Duo S"),
                          QStringLiteral("iA Writer Quattro S") }),
      m_chosenSerifFontIndex(0),
      m_chosenSansSerifFontIndex(0),
      m_chosenMonoFontIndex(0),
      m_currentCharsLimitPerFont({ 64, // Mono    TODO: is this the proper way to initialize?
                                   80, // Serif
                                   80 }), // SansSerif
      m_currentFontTypeface(FontTypeface::SansSerif),
#ifdef __APPLE__
      m_displayFont(QFont(QStringLiteral("SF Pro Text")).exactMatch()
                            ? QStringLiteral("SF Pro Text")
                            : QStringLiteral("Roboto")),
#elif _WIN32
      m_displayFont(QFont(QStringLiteral("Segoe UI")).exactMatch() ? QStringLiteral("Segoe UI")
                                                                   : QStringLiteral("Roboto")),
#else
      m_displayFont(QStringLiteral("Roboto")),
#endif
      m_currentTheme(Theme::Light),
      m_currentEditorTextColor(26, 26, 26),
      m_areNonEditorWidgetsVisible(true),
      m_isEditorSettingsFromQuickViewVisible(false),
      m_isProVersionActivated(false),
      m_localLicenseData(nullptr),
      m_blockModel(new BlockModel(this))
{
    ui->setupUi(this);
    setupBlockEditorView();
    setupMainWindow();
    setupFonts();
    setupSplitter();
    setupSearchEdit();
    setupEditorSettings();
    setupKeyboardShortcuts();
    setupDatabases();
    setupModelView();
    setupTextEdit();
    restoreStates();
    setupButtons();
    setupSignalsSlots();
#if defined(UPDATE_CHECKER)
    autoCheckForUpdates();
#endif
    checkProVersion();

    QTimer::singleShot(200, this, SLOT(InitData()));
}

/*!
 * \brief MainWindow::InitData
 * Init the data from database and select the first note if there is one
 */
void MainWindow::InitData()
{
    QFileInfo fi(m_settingsDatabase->fileName());
    QDir dir(fi.absolutePath());
    QString oldNoteDBPath(dir.path() + QStringLiteral("/Notes.ini"));
    QString oldTrashDBPath(dir.path() + QStringLiteral("/Trash.ini"));

    bool isV0_9_0 = (QFile::exists(oldNoteDBPath) || QFile::exists(oldTrashDBPath));
    if (isV0_9_0) {
        QProgressDialog *pd =
                new QProgressDialog(tr("Migrating database, please wait."), QString(), 0, 0, this);
        pd->setCancelButton(nullptr);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        pd->setWindowFlags(Qt::Window);
#else
        pd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
#endif
        pd->setMinimumDuration(0);
        pd->show();

        setButtonsAndFieldsEnabled(false);

        QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
        connect(watcher, &QFutureWatcher<void>::finished, this, [&, pd]() {
            pd->deleteLater();
            setButtonsAndFieldsEnabled(true);
        });

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QFuture<void> migration = QtConcurrent::run(&MainWindow::migrateFromV0_9_0, this);
#else
        QFuture<void> migration = QtConcurrent::run(this, &MainWindow::migrateFromV0_9_0);
#endif
        watcher->setFuture(migration);
    }
    /// Check if it is running with an argument (ex. hide)
    if (qApp->arguments().contains(QStringLiteral("--autostart"))
        && QSystemTrayIcon::isSystemTrayAvailable()) {
        setMainWindowVisibility(false);
    }

    // init tree view
    emit requestNodesTree();
}

/*!
 * \brief Toggles visibility of the main window upon system tray activation
 * \param reason The reason the system tray was activated
 */
void MainWindow::onSystemTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        setMainWindowVisibility(!isVisible());
    }
}

/*!
 * \brief MainWindow::setMainWindowVisibility
 * \param state
 */
void MainWindow::setMainWindowVisibility(bool state)
{
    if (state) {
        show();
        raise();
        activateWindow();
#if !defined(Q_OS_MAC)
        m_restoreAction->setText(tr("&Hide Notes"));
#endif
    } else {
#if !defined(Q_OS_MAC)
        m_restoreAction->setText(tr("&Show Notes"));
#endif
        hide();
    }
}

void MainWindow::saveLastSelectedFolderTags(bool isFolder, const QString &folderPath,
                                            const QSet<int> &tagId)
{
    m_settingsDatabase->setValue("isSelectingFolder", isFolder);
    m_settingsDatabase->setValue("currentSelectFolder", folderPath);
    QStringList sl;
    for (const auto &id : tagId) {
        sl.append(QString::number(id));
    }
    m_settingsDatabase->setValue("currentSelectTagsId", sl);
}

void MainWindow::saveExpandedFolder(const QStringList &folderPaths)
{
    m_settingsDatabase->setValue("currentExpandedFolder", folderPaths);
}

void MainWindow::saveLastSelectedNote(const QSet<int> &notesId)
{
    QStringList sl;
    for (const auto &id : notesId) {
        sl.append(QString::number(id));
    }
    m_settingsDatabase->setValue("currentSelectNotesId", sl);
}

/*!
 * \brief MainWindow::paintEvent
 * \param event
 */
void MainWindow::paintEvent(QPaintEvent *event)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (!m_useNativeWindowFrame) {
        QPainter painter(this);
        painter.save();

        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);

        dropShadow(painter, ShadowType::Linear, ShadowSide::Left);
        dropShadow(painter, ShadowType::Linear, ShadowSide::Top);
        dropShadow(painter, ShadowType::Linear, ShadowSide::Right);
        dropShadow(painter, ShadowType::Linear, ShadowSide::Bottom);

        dropShadow(painter, ShadowType::Radial, ShadowSide::TopLeft);
        dropShadow(painter, ShadowType::Radial, ShadowSide::TopRight);
        dropShadow(painter, ShadowType::Radial, ShadowSide::BottomRight);
        dropShadow(painter, ShadowType::Radial, ShadowSide::BottomLeft);

        painter.restore();
    }
#endif

    QMainWindow::paintEvent(event);
}

/*!
 * \brief MainWindow::resizeEvent
 * \param event
 */
void MainWindow::resizeEvent(QResizeEvent *event)
{
    if (m_splitter) {
        // restore note list width
        updateFrame();
    }

    QJsonObject dataToSendToView{ { "parentWindowHeight", height() },
                                  { "parentWindowWidth", width() } };
    emit mainWindowResized(QVariant(dataToSendToView));

    QMainWindow::resizeEvent(event);
}

/*!
 * \brief MainWindow::~MainWindow
 * Deconstructor of the class
 */
MainWindow::~MainWindow()
{
    delete ui;
    m_dbThread->quit();
    m_dbThread->wait();
    delete m_dbThread;
}

/*!
 * \brief MainWindow::setupMainWindow
 * Setting up main window prefrences like frameless window and the minimum size of the window
 * Setting the window background color to be white
 */
void MainWindow::setupMainWindow()
{
#if !defined(Q_OS_MAC)
    auto flags = Qt::Window | Qt::CustomizeWindowHint;
#  if defined(Q_OS_UNIX)
    //    flags |= Qt::FramelessWindowHint;
    flags = Qt::Window;
#  endif
    setWindowFlags(flags);
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    setAttribute(Qt::WA_TranslucentBackground);
#endif

    // load stylesheet
    QFile mainWindowStyleFile(QStringLiteral(":/styles/main-window.css"));
    mainWindowStyleFile.open(QFile::ReadOnly);
    m_styleSheet = QString::fromLatin1(mainWindowStyleFile.readAll());
    setStyleSheet(m_styleSheet);
    /**** Apply the stylesheet for all children we change classes for ****/

    // left frame
    ui->frameLeft->setStyleSheet(m_styleSheet);

    // middle frame
    ui->searchEdit->setStyleSheet(m_styleSheet);
    ui->verticalSpacer_upSearchEdit->setStyleSheet(m_styleSheet);
    ui->verticalSpacer_upSearchEdit2->setStyleSheet(m_styleSheet);
    ui->listviewLabel1->setStyleSheet(m_styleSheet);
    ui->listviewLabel2->setStyleSheet(m_styleSheet);

    // splitters
    ui->verticalSplitterLine_left->setStyleSheet(m_styleSheet);
    ui->verticalSplitterLine_middle->setStyleSheet(m_styleSheet);

    // buttons
    ui->toggleTreeViewButton->setStyleSheet(m_styleSheet);
    ui->newNoteButton->setStyleSheet(m_styleSheet);
    ui->globalSettingsButton->setStyleSheet(m_styleSheet);

    // custom scrollbars on Linux and Windows
#if !defined(Q_OS_MACOS)
    QFile scollBarStyleFile(QStringLiteral(":/styles/components/custom-scrollbar.css"));
    scollBarStyleFile.open(QFile::ReadOnly);
    QString scrollbarStyleSheet = QString::fromLatin1(scollBarStyleFile.readAll());
#endif

    m_greenMaximizeButton = new QPushButton(this);
    m_redCloseButton = new QPushButton(this);
    m_yellowMinimizeButton = new QPushButton(this);
#ifndef __APPLE__
    //    If we want to align window buttons with searchEdit and notesList
    //    QSpacerItem *horizontialSpacer = new QSpacerItem(3, 0, QSizePolicy::Minimum,
    //    QSizePolicy::Minimum); m_trafficLightLayout.addSpacerItem(horizontialSpacer);
    m_trafficLightLayout.addWidget(m_redCloseButton);
    m_trafficLightLayout.addWidget(m_yellowMinimizeButton);
    m_trafficLightLayout.addWidget(m_greenMaximizeButton);
#else
    setCloseBtnQuit(false);
    m_layoutMargin = 0;
    m_greenMaximizeButton->setVisible(false);
    m_redCloseButton->setVisible(false);
    m_yellowMinimizeButton->setVisible(false);
#endif

#if defined(Q_OS_WINDOWS)
    m_layoutMargin = 0;
    m_trafficLightLayout.setSpacing(0);
    m_trafficLightLayout.setContentsMargins(QMargins(0, 0, 0, 0));
    m_trafficLightLayout.setGeometry(QRect(2, 2, 90, 16));
#endif

    m_newNoteButton = ui->newNoteButton;
    m_globalSettingsButton = ui->globalSettingsButton;
    m_searchEdit = ui->searchEdit;
    m_splitter = ui->splitter;
    m_foldersWidget = ui->frameLeft;
    m_noteListWidget = ui->frameMiddle;
    m_toggleTreeViewButton = ui->toggleTreeViewButton;
    // don't resize first two panes when resizing
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setStretchFactor(2, 1);

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QMargins margins(m_layoutMargin, m_layoutMargin, m_layoutMargin, m_layoutMargin);
    setMargins(margins);
#endif
    ui->frame->installEventFilter(this);
    ui->centralWidget->setMouseTracking(true);
    setMouseTracking(true);
    QPalette pal(palette());
    pal.setColor(QPalette::Window, QColor(248, 248, 248));
    setAutoFillBackground(true);
    setPalette(pal);

    m_newNoteButton->setToolTip(tr("Create New Note"));
    m_globalSettingsButton->setToolTip(tr("Open App Settings"));
    m_toggleTreeViewButton->setToolTip("Toggle Folders Pane");

    ui->listviewLabel2->setMinimumSize({ 40, 25 });
    ui->listviewLabel2->setMaximumSize({ 40, 25 });

#ifdef __APPLE__
    QFont titleFont(m_displayFont, 13, QFont::DemiBold);
#else
    QFont titleFont(m_displayFont, 10, QFont::DemiBold);
#endif
    ui->listviewLabel1->setFont(titleFont);
    ui->listviewLabel2->setFont(titleFont);
    m_splitterStyle = new SplitterStyle();
    m_splitter->setStyle(m_splitterStyle);
    m_splitter->setHandleWidth(0);
    setNoteListLoading();
#ifdef __APPLE__
    ui->searchEdit->setFocus();
#endif
    setWindowIcon(QIcon(QStringLiteral(":images/notes_icon.ico")));
}

/*!
 * \brief MainWindow::setupFonts
 */
void MainWindow::setupFonts()
{
#ifdef __APPLE__
    m_searchEdit->setFont(QFont(m_displayFont, 12));
#else
    m_searchEdit->setFont(QFont(m_displayFont, 10));
#endif
}

/*!
 * \brief MainWindow::setupTrayIcon
 */
void MainWindow::setupTrayIcon()
{
#if !defined(Q_OS_MAC)
    auto trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(m_restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(m_quitAction);
    m_trayIcon->setContextMenu(trayIconMenu);
#endif

    QIcon icon(QStringLiteral(":images/notes_system_tray_icon.png"));
    m_trayIcon->setIcon(icon);
    m_trayIcon->show();
}

/*!
 * \brief MainWindow::setupKeyboardShortcuts
 * Setting up the keyboard shortcuts
 */
void MainWindow::setupKeyboardShortcuts()
{
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_N), this, SLOT(onNewNoteButtonClicked()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this, SLOT(deleteSelectedNote()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), m_searchEdit, SLOT(setFocus()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), m_searchEdit, SLOT(clear()));
    //    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down), this, SLOT(selectNoteDown()));
    //    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up), this, SLOT(selectNoteUp()));
    new QShortcut(QKeySequence(Qt::Key_Down), this, SLOT(selectNoteDown()));
    new QShortcut(QKeySequence(Qt::Key_Up), this, SLOT(selectNoteUp()));
    //    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), this, SLOT(setFocusOnText()));
    //    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this, SLOT(setFocusOnText()));
    new QShortcut(QKeySequence(Qt::Key_Enter), this, SLOT(setFocusOnText()));
    new QShortcut(QKeySequence(Qt::Key_Return), this, SLOT(setFocusOnText()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), this, SLOT(fullscreenWindow()));
    new QShortcut(Qt::Key_F11, this, SLOT(fullscreenWindow()));
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L), this),
            &QShortcut::activated, this, [=]() { m_listView->setFocus(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M), this, SLOT(minimizeWindow()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q), this, SLOT(QuitApplication()));
#if defined(Q_OS_MACOS) || defined(Q_OS_WINDOWS)
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this, SLOT(toggleStayOnTop()));
#endif
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_J), this, SLOT(toggleNoteList()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_J), this, SLOT(toggleFolderTree()));
    //    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_A), this, SLOT(selectAllNotesInList()));
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), this),
            &QShortcut::activated, this, [=]() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
                if (m_blockEditorWidget->isHidden()) {
                } else {
                    emit toggleEditorSettingsKeyboardShorcutFired();
                };
#else
            if (m_editorSettingsWidget->isHidden()) {
            } else {
                m_editorSettingsWidget->close();
            }
#endif
            });
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), m_editorSettingsWidget),
            &QShortcut::activated, this, [=]() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
                if (m_blockEditorWidget->isHidden()) {
                    if (!m_editorSettingsWidget->isHidden()) {
                        m_editorSettingsWidget->close();
                    }
                } else {
                    emit toggleEditorSettingsKeyboardShorcutFired();
                };
#else
                if (m_editorSettingsWidget->isVisible()) {
                    m_editorSettingsWidget->close();
                };
#endif
            });

    QxtGlobalShortcut *shortcut = new QxtGlobalShortcut(this);
#if defined(Q_OS_MACOS)
    shortcut->setShortcut(QKeySequence(Qt::META | Qt::Key_N));
#else
    shortcut->setShortcut(QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_N));
#endif
    connect(shortcut, &QxtGlobalShortcut::activated, this, [=]() {
        // workaround prevent textEdit and searchEdit
        // from taking 'N' from shortcut
        m_searchEdit->setDisabled(true);
        setMainWindowVisibility(isHidden() || windowState() == Qt::WindowMinimized
                                || qApp->applicationState() == Qt::ApplicationInactive);
        if (isHidden() || windowState() == Qt::WindowMinimized
            || qApp->applicationState() == Qt::ApplicationInactive)
#ifdef __APPLE__
            raise();
#else
            activateWindow();
#endif
        m_searchEdit->setDisabled(false);
    });
}

/*!
 * \brief MainWindow::setupSplitter
 * Set up the splitter that control the size of the scrollArea and the textEdit
 */
void MainWindow::setupSplitter()
{
    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, false);
    m_splitter->setCollapsible(2, false);
}

/*!
 * \brief MainWindow::setupButtons
 * Setting up the red (close), yellow (minimize), and green (maximize) buttons
 * Make only the buttons icon visible
 * And install this class event filter to them, to act when hovering on one of them
 */
void MainWindow::setupButtons()
{
    QString ss = QStringLiteral("QPushButton { "
                                "  border: none; "
                                "  padding: 0px; "
                                "}");

    m_redCloseButton->setStyleSheet(ss);
    m_yellowMinimizeButton->setStyleSheet(ss);
    m_greenMaximizeButton->setStyleSheet(ss);

#ifdef _WIN32
    m_redCloseButton->setIcon(QIcon(QStringLiteral(":images/windows_close_regular.png")));
    m_yellowMinimizeButton->setIcon(QIcon(QStringLiteral(":images/windows_maximize_regular.png")));
    m_greenMaximizeButton->setIcon(QIcon(QStringLiteral(":images/windows_minimize_regular.png")));

    m_redCloseButton->setIconSize(QSize(34, 16));
    m_yellowMinimizeButton->setIconSize(QSize(28, 16));
    m_greenMaximizeButton->setIconSize(QSize(28, 16));
#endif

    m_redCloseButton->installEventFilter(this);
    m_yellowMinimizeButton->installEventFilter(this);
    m_greenMaximizeButton->installEventFilter(this);

    QFont fontAwesomeIcon("Font Awesome 6 Free Solid");
    QFont materialSymbols("Material Symbols Outlined");
#if defined(Q_OS_MACOS)
    int pointSizeOffset = 0;
#else
    int pointSizeOffset = -4;
#endif

    fontAwesomeIcon.setPointSize(16 + pointSizeOffset);
    m_globalSettingsButton->setFont(fontAwesomeIcon);
    m_globalSettingsButton->setText(u8"\uf013"); // fa-gear

#if defined(Q_OS_MACOS)
    materialSymbols.setPointSize(30 + pointSizeOffset);
#else
    materialSymbols.setPointSize(30 + pointSizeOffset - 3);
#endif

#if defined(Q_OS_MACOS)
    materialSymbols.setPointSize(24 + pointSizeOffset);
#else
    materialSymbols.setPointSize(21 + pointSizeOffset);
#endif

    materialSymbols.setPointSize(20 + pointSizeOffset);
    m_toggleTreeViewButton->setFont(materialSymbols);
    if (m_foldersWidget->isHidden()) {
        m_toggleTreeViewButton->setText(u8"\ue31c"); // keyboard_tab_rtl
    } else {
        m_toggleTreeViewButton->setText(u8"\uec73"); // keyboard_tab_rtl
    }

    fontAwesomeIcon.setPointSize(17 + pointSizeOffset);
    m_newNoteButton->setFont(fontAwesomeIcon);
    m_newNoteButton->setText(u8"\uf067"); // fa_plus
}

/*!
 * \brief MainWindow::setupSignalsSlots
 * connect between signals and slots
 */
void MainWindow::setupSignalsSlots()
{
#if defined(UPDATE_CHECKER)
    connect(&m_updater, &UpdaterWindow::dontShowUpdateWindowChanged, this,
            [=](bool state) { m_dontShowUpdateWindow = state; });
#endif
    // actions
    // connect(rightToLeftActionion, &QAction::triggered, this, );
    // connect(checkForUpdatesAction, &QAction::triggered, this, );
    // green button
    connect(m_greenMaximizeButton, &QPushButton::pressed, this,
            &MainWindow::onGreenMaximizeButtonPressed);
    connect(m_greenMaximizeButton, &QPushButton::clicked, this,
            &MainWindow::onGreenMaximizeButtonClicked);
    // red button
    connect(m_redCloseButton, &QPushButton::pressed, this, &MainWindow::onRedCloseButtonPressed);
    connect(m_redCloseButton, &QPushButton::clicked, this, &MainWindow::onRedCloseButtonClicked);
    // yellow button
    connect(m_yellowMinimizeButton, &QPushButton::pressed, this,
            &MainWindow::onYellowMinimizeButtonPressed);
    connect(m_yellowMinimizeButton, &QPushButton::clicked, this,
            &MainWindow::onYellowMinimizeButtonClicked);
    // new note button
    connect(m_newNoteButton, &QPushButton::clicked, this, &MainWindow::onNewNoteButtonClicked);
    // global settings button
    connect(m_globalSettingsButton, &QPushButton::clicked, this,
            &MainWindow::onGlobalSettingsButtonClicked);
    // line edit text changed
    connect(m_searchEdit, &QLineEdit::textChanged, m_listViewLogic,
            &ListViewLogic::onSearchEditTextChanged);
    // line edit enter key pressed
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearchEditReturnPressed);
    // clear button
    connect(m_clearButton, &QToolButton::clicked, this, &MainWindow::onClearButtonClicked);
    // System tray activation
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onSystemTrayIconActivated);

#if !defined(Q_OS_MAC)
    // System tray context menu action: "Show/Hide Notes"
    connect(m_restoreAction, &QAction::triggered, this, [this]() {
        setMainWindowVisibility(isHidden() || windowState() == Qt::WindowMinimized
                                || (qApp->applicationState() == Qt::ApplicationInactive));
    });
    // System tray context menu action: "Quit"
    connect(m_quitAction, &QAction::triggered, this, &MainWindow::QuitApplication);
    // Application state changed
    connect(qApp, &QApplication::applicationStateChanged, this,
            [this]() { m_listView->update(m_listView->currentIndex()); });
#endif

    // MainWindow <-> DBManager
    connect(this, &MainWindow::requestNodesTree, m_dbManager, &DBManager::onNodeTagTreeRequested,
            Qt::BlockingQueuedConnection);
    connect(this, &MainWindow::requestRestoreNotes, m_dbManager,
            &DBManager::onRestoreNotesRequested, Qt::BlockingQueuedConnection);
    connect(this, &MainWindow::requestImportNotes, m_dbManager, &DBManager::onImportNotesRequested,
            Qt::BlockingQueuedConnection);
    connect(this, &MainWindow::requestExportNotes, m_dbManager, &DBManager::onExportNotesRequested,
            Qt::BlockingQueuedConnection);
    connect(this, &MainWindow::requestMigrateNotesFromV0_9_0, m_dbManager,
            &DBManager::onMigrateNotesFromV0_9_0Requested, Qt::BlockingQueuedConnection);
    connect(this, &MainWindow::requestMigrateTrashFromV0_9_0, m_dbManager,
            &DBManager::onMigrateTrashFrom0_9_0Requested, Qt::BlockingQueuedConnection);

    connect(m_listViewLogic, &ListViewLogic::showNotesInEditor, m_noteEditorLogic,
            &NoteEditorLogic::showNotesInEditor);
    connect(m_listViewLogic, &ListViewLogic::closeNoteEditor, m_noteEditorLogic,
            &NoteEditorLogic::closeEditor);
    connect(m_noteEditorLogic, &NoteEditorLogic::moveNoteToListViewTop, m_listViewLogic,
            &ListViewLogic::moveNoteToTop);
    connect(m_noteEditorLogic, &NoteEditorLogic::updateNoteDataInList, m_listViewLogic,
            &ListViewLogic::setNoteData);
    connect(m_noteEditorLogic, &NoteEditorLogic::deleteNoteRequested, m_listViewLogic,
            &ListViewLogic::deleteNoteRequested);
    connect(m_listViewLogic, &ListViewLogic::noteTagListChanged, m_noteEditorLogic,
            &NoteEditorLogic::onNoteTagListChanged);
    connect(m_noteEditorLogic, &NoteEditorLogic::noteEditClosed, m_listViewLogic,
            &ListViewLogic::onNoteEditClosed);
    connect(m_listViewLogic, &ListViewLogic::requestClearSearchUI, this, &MainWindow::clearSearch);
    // Handle search in block model
    connect(m_listViewLogic, &ListViewLogic::requestClearSearchUI, m_blockModel,
            &BlockModel::clearSearch);
    connect(m_searchEdit, &QLineEdit::textChanged, m_blockModel,
            &BlockModel::onSearchEditTextChanged);
    connect(m_treeViewLogic, &TreeViewLogic::addNoteToTag, m_listViewLogic,
            &ListViewLogic::onAddTagRequestD);
    connect(m_listViewLogic, &ListViewLogic::listViewLabelChanged, this,
            [this](const QString &l1, const QString &l2) {
                ui->listviewLabel1->setText(l1);
                ui->listviewLabel2->setText(l2);
                m_splitter->setHandleWidth(0);
            });
    connect(m_toggleTreeViewButton, &QPushButton::clicked, this, &MainWindow::toggleFolderTree);
    connect(m_dbManager, &DBManager::showErrorMessage, this, &MainWindow::showErrorMessage,
            Qt::QueuedConnection);
    connect(m_listViewLogic, &ListViewLogic::requestNewNote, this,
            &MainWindow::onNewNoteButtonClicked);
    connect(m_listViewLogic, &ListViewLogic::moveNoteRequested, this, [this](int id, int target) {
        m_treeViewLogic->onMoveNodeRequested(id, target);
        m_treeViewLogic->openFolder(target);
    });
    connect(m_listViewLogic, &ListViewLogic::setNewNoteButtonVisible, this,
            [this](bool visible) { ui->newNoteButton->setVisible(visible); });
    connect(m_treeViewLogic, &TreeViewLogic::noteMoved, m_listViewLogic,
            &ListViewLogic::onNoteMovedOut);

    connect(m_listViewLogic, &ListViewLogic::requestClearSearchDb, this,
            &MainWindow::setNoteListLoading);
    connect(m_treeView, &NodeTreeView::loadNotesInTagsRequested, this,
            &MainWindow::setNoteListLoading);
    connect(m_treeView, &NodeTreeView::loadNotesInFolderRequested, this,
            &MainWindow::setNoteListLoading);
    connect(m_treeView, &NodeTreeView::saveExpand, this, &MainWindow::saveExpandedFolder);
    connect(m_treeView, &NodeTreeView::saveSelected, this, &MainWindow::saveLastSelectedFolderTags);
    connect(m_listView, &NoteListView::saveSelectedNote, this, &MainWindow::saveLastSelectedNote);
    connect(m_treeView, &NodeTreeView::saveLastSelectedNote, m_listViewLogic,
            &ListViewLogic::setLastSelectedNote);
    connect(m_treeView, &NodeTreeView::requestLoadLastSelectedNote, m_listViewLogic,
            &ListViewLogic::loadLastSelectedNoteRequested);
    connect(m_treeView, &NodeTreeView::loadNotesInFolderRequested, m_listViewLogic,
            &ListViewLogic::onNotesListInFolderRequested);
    connect(m_treeView, &NodeTreeView::loadNotesInTagsRequested, m_listViewLogic,
            &ListViewLogic::onNotesListInTagsRequested);
    connect(this, &MainWindow::requestChangeDatabasePath, m_dbManager,
            &DBManager::onChangeDatabasePathRequested, Qt::QueuedConnection);

#if defined(Q_OS_MACOS)
    connect(this, &MainWindowBase::toggleFullScreen, this, [this](bool isFullScreen) {
        if (isFullScreen) {
            ui->verticalSpacer_upSearchEdit->setMinimumHeight(0);
            ui->verticalSpacer_upSearchEdit->setMaximumHeight(0);
        } else {
            if (m_foldersWidget->isHidden()) {
                ui->verticalSpacer_upSearchEdit->setMinimumHeight(25);
                ui->verticalSpacer_upSearchEdit->setMaximumHeight(25);
            }
        }
    });
#endif
}

/*!
 * \brief MainWindow::autoCheckForUpdates
 * Checks for updates, if an update is found, then the updater dialog will show
 * up, otherwise, no notification shall be showed
 */
#if defined(UPDATE_CHECKER)
void MainWindow::autoCheckForUpdates()
{
    m_updater.installEventFilter(this);
    m_updater.setShowWindowDisable(m_dontShowUpdateWindow);
    m_updater.checkForUpdates(false);
}
#endif

/*!
 * \brief MainWindow::setupSearchEdit
 * Set the lineedit to start a bit to the right and end a bit to the left (pedding)
 */
void MainWindow::setupSearchEdit()
{
    //    QLineEdit* searchEdit = m_searchEdit;

    m_searchEdit->setAttribute(Qt::WA_MacShowFocusRect, 0);

    QFont fontAwesomeIcon("Font Awesome 6 Free Solid");
#if defined(Q_OS_MACOS)
    int pointSizeOffset = 0;
#else
    int pointSizeOffset = -4;
#endif

    // clear button
    m_clearButton = new QToolButton(m_searchEdit);
    fontAwesomeIcon.setPointSize(15 + pointSizeOffset);
    m_clearButton->setStyleSheet("QToolButton { color: rgb(114, 114, 114) }");
    m_clearButton->setFont(fontAwesomeIcon);
    m_clearButton->setText(u8"\uf057"); // fa-circle-xmark
    m_clearButton->setCursor(Qt::ArrowCursor);
    m_clearButton->hide();

    // search button
    m_searchButton = new QToolButton(m_searchEdit);
    fontAwesomeIcon.setPointSize(9 + pointSizeOffset);
    m_searchButton->setStyleSheet("QToolButton { color: rgb(205, 205, 205) }");
    m_searchButton->setFont(fontAwesomeIcon);
    m_searchButton->setText(u8"\uf002"); // fa-magnifying-glass
    m_searchButton->setCursor(Qt::ArrowCursor);

    // layout
    QBoxLayout *layout = new QBoxLayout(QBoxLayout::RightToLeft, m_searchEdit);
    layout->setContentsMargins(2, 0, 3, 0);
    layout->addWidget(m_clearButton);
    layout->addStretch();
    layout->addWidget(m_searchButton);
    m_searchEdit->setLayout(layout);

    m_searchEdit->installEventFilter(this);
}

void MainWindow::setupEditorSettings()
{
    FontTypeface::registerEnum("nuttyartist.notes", 1, 0);
    FontSizeAction::registerEnum("nuttyartist.notes", 1, 0);
    EditorTextWidth::registerEnum("nuttyartist.notes", 1, 0);
    Theme::registerEnum("nuttyartist.notes", 1, 0);
    View::registerEnum("nuttyartist.notes", 1, 0);

#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    QUrl source("qrc:/qt/qml/EditorSettings.qml");
#elif QT_VERSION > QT_VERSION_CHECK(5, 12, 8)
    QUrl source("qrc:/qml/EditorSettings.qml");
#else
    QUrl source("qrc:/qml/EditorSettingsQt512.qml");
#endif

    m_editorSettingsQuickView.rootContext()->setContextProperty("mainWindow", this);
    m_editorSettingsQuickView.rootContext()->setContextProperty("noteEditorLogic",
                                                                m_noteEditorLogic);
    m_editorSettingsQuickView.setSource(source);
    m_editorSettingsQuickView.setResizeMode(QQuickView::SizeViewToRootObject);
    m_editorSettingsQuickView.setFlags(Qt::FramelessWindowHint);
    m_editorSettingsQuickView.setColor(Qt::transparent);
    m_editorSettingsWidget = QWidget::createWindowContainer(&m_editorSettingsQuickView, nullptr);
#if defined(Q_OS_MACOS)
#  if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    m_editorSettingsWidget->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint
                                           | Qt::NoDropShadowWindowHint);
#  else
    m_editorSettingsWidget->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
    m_editorSettingsWidget->setAttribute(Qt::WA_AlwaysStackOnTop);
#  endif
#elif _WIN32
    m_editorSettingsWidget->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
    m_editorSettingsWidget->setAttribute(Qt::WA_AlwaysStackOnTop);
#else
    m_editorSettingsWidget->setWindowFlags(Qt::Tool);
    m_editorSettingsWidget->setAttribute(Qt::WA_AlwaysStackOnTop);
#endif
    m_editorSettingsWidget->setStyleSheet("background:transparent;");
    m_editorSettingsWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_editorSettingsWidget->hide();
    m_editorSettingsWidget->installEventFilter(this);

    QJsonObject dataToSendToView{ { "displayFont",
                                    QFont(QStringLiteral("SF Pro Text")).exactMatch()
                                            ? QStringLiteral("SF Pro Text")
                                            : QStringLiteral("Roboto") } };
    emit displayFontSet(QVariant(dataToSendToView));

#if defined(Q_OS_WINDOWS)
    emit platformSet(QVariant(QString("Windows")));
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    emit platformSet(QVariant(QString("Unix")));
#elif defined(Q_OS_MACOS)
    emit platformSet(QVariant(QString("Apple")));
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    emit qtVersionSet(QVariant(6));
#else
    emit qtVersionSet(QVariant(5));
#endif
}

/*!
 * \brief MainWindow::setCurrentFontBasedOnTypeface
 * Set the current font based on a given typeface
 */
void MainWindow::setCurrentFontBasedOnTypeface(FontTypeface::Value selectedFontTypeFace)
{
    m_currentFontTypeface = selectedFontTypeFace;
    switch (selectedFontTypeFace) {
    case FontTypeface::Mono:
        m_currentFontFamily = m_listOfMonoFonts.at(m_chosenMonoFontIndex);
        break;
    case FontTypeface::Serif:
        m_currentFontFamily = m_listOfSerifFonts.at(m_chosenSerifFontIndex);
        break;
    case FontTypeface::SansSerif:
        m_currentFontFamily = m_listOfSansSerifFonts.at(m_chosenSansSerifFontIndex);
        break;
    }

    m_currentFontPointSize = m_editorMediumFontSize;

    m_currentSelectedFont = QFont(m_currentFontFamily, m_currentFontPointSize, QFont::Normal);

    QJsonObject dataToSendToView;
    dataToSendToView["listOfSansSerifFonts"] = QJsonArray::fromStringList(m_listOfSansSerifFonts);
    dataToSendToView["listOfSerifFonts"] = QJsonArray::fromStringList(m_listOfSerifFonts);
    dataToSendToView["listOfMonoFonts"] = QJsonArray::fromStringList(m_listOfMonoFonts);
    dataToSendToView["chosenSansSerifFontIndex"] = m_chosenSansSerifFontIndex;
    dataToSendToView["chosenSerifFontIndex"] = m_chosenSerifFontIndex;
    dataToSendToView["chosenMonoFontIndex"] = m_chosenMonoFontIndex;
    dataToSendToView["currentFontTypeface"] = to_string(m_currentFontTypeface).c_str();
    dataToSendToView["currentFontPointSize"] = m_currentFontPointSize;
    dataToSendToView["currentEditorTextColor"] = m_currentEditorTextColor.name();
    emit fontsChanged(QVariant(dataToSendToView));
}

/*!
 * \brief MainWindow::resetEditorSettings
 * Reset editor settings to default options
 */
void MainWindow::resetEditorSettings()
{
    m_currentFontTypeface = FontTypeface::SansSerif;
    m_chosenMonoFontIndex = 0;
    m_chosenSerifFontIndex = 0;
    m_chosenSansSerifFontIndex = 0;
#ifdef __APPLE__
    m_editorMediumFontSize = 17;
#else
    m_editorMediumFontSize = 13;
#endif
    m_currentFontPointSize = m_editorMediumFontSize;
    m_currentCharsLimitPerFont.mono = 64;
    m_currentCharsLimitPerFont.serif = 80;
    m_currentCharsLimitPerFont.sansSerif = 80;
    m_currentTheme = Theme::Light;

    setCurrentFontBasedOnTypeface(m_currentFontTypeface);
    setTheme(m_currentTheme);
}

/*!
 * \brief MainWindow::setupTextEdit
 * Setting up textEdit:
 * Setup the style of the scrollBar and set textEdit background to an image
 * Make the textEdit pedding few pixels right and left, to compel with a beautiful proportional grid
 * And install this class event filter to catch when text edit is having focus
 */
void MainWindow::setupTextEdit()
{
#ifdef __APPLE__
    if (QFont("Helvetica Neue").exactMatch()) {
        m_listOfSansSerifFonts.push_front("Helvetica Neue");
    } else if (QFont("Helvetica").exactMatch()) {
        m_listOfSansSerifFonts.push_front("Helvetica");
    }

    if (QFont("SF Pro Text").exactMatch()) {
        m_listOfSansSerifFonts.push_front("SF Pro Text");
    }

    if (QFont("Avenir Next").exactMatch()) {
        m_listOfSansSerifFonts.push_front("Avenir Next");
    } else if (QFont("Avenir").exactMatch()) {
        m_listOfSansSerifFonts.push_front("Avenir");
    }
#elif _WIN32
    if (QFont("Calibri").exactMatch())
        m_listOfSansSerifFonts.push_front("Calibri");

    if (QFont("Arial").exactMatch())
        m_listOfSansSerifFonts.push_front("Arial");

    if (QFont("Segoe UI").exactMatch())
        m_listOfSansSerifFonts.push_front("Segoe UI");
#endif
}

void MainWindow::setupBlockEditorView()
{
    qmlRegisterSingletonInstance("com.company.BlockModel", 1, 0, "BlockModel", m_blockModel);
    qmlRegisterType<BlockInfo>("nuttyartist.notes", 1, 0, "BlockInfo");
    qmlRegisterType<BlockModel>("nuttyartist.notes", 1, 0, "BlockModel");

    qmlRegisterType<MarkdownHighlighter>("MarkdownHighlighter", 1, 0, "MarkdownHighlighter");

    QUrl source("qrc:/qt/qml/EditorMain.qml");
    m_blockEditorQuickView.rootContext()->setContextProperty("noteEditorLogic", m_noteEditorLogic);
    m_blockEditorQuickView.rootContext()->setContextProperty("mainWindow", this);
    m_blockEditorQuickView.setSource(source);
    m_blockEditorQuickView.setResizeMode(QQuickView::SizeRootObjectToView);
    m_blockEditorWidget = QWidget::createWindowContainer(&m_blockEditorQuickView);
    m_blockEditorWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_blockEditorWidget->show();
    ui->verticalLayout_textEdit->insertWidget(0, m_blockEditorWidget);
#if defined(Q_OS_WINDOWS)
    emit platformSet(QVariant(QString("Windows")));
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    emit platformSet(QVariant(QString("Unix")));
#elif defined(Q_OS_MACOS)
    emit platformSet(QVariant(QString("Apple")));
#endif

    QJsonObject dataToSendToView{ { "displayFont",
                                    QFont(QStringLiteral("SF Pro Text")).exactMatch()
                                            ? QStringLiteral("SF Pro Text")
                                            : QStringLiteral("Roboto") } };
    emit displayFontSet(QVariant(dataToSendToView));
}

/*!
 * \brief MainWindow::initializeSettingsDatabase
 */
void MainWindow::initializeSettingsDatabase()
{
    // Why are we not updating the app version in Settings?
    if (m_settingsDatabase->value(QStringLiteral("version"), "NULL") == "NULL")
        m_settingsDatabase->setValue(QStringLiteral("version"), qApp->applicationVersion());

#if defined(UPDATE_CHECKER)
    if (m_settingsDatabase->value(QStringLiteral("dontShowUpdateWindow"), "NULL") == "NULL")
        m_settingsDatabase->setValue(QStringLiteral("dontShowUpdateWindow"),
                                     m_dontShowUpdateWindow);
#endif

    if (m_settingsDatabase->value(QStringLiteral("windowGeometry"), "NULL") == "NULL") {
        int initWidth = 1106;
        int initHeight = 694;
        QPoint center = qApp->primaryScreen()->geometry().center();
        QRect rect(center.x() - initWidth / 2, center.y() - initHeight / 2, initWidth, initHeight);
        setGeometry(rect);
        m_settingsDatabase->setValue(QStringLiteral("windowGeometry"), saveGeometry());
    }

    if (m_settingsDatabase->value(QStringLiteral("splitterSizes"), "NULL") == "NULL") {
        m_splitter->resize(width() - 2 * m_layoutMargin, height() - 2 * m_layoutMargin);
        updateFrame();
        m_settingsDatabase->setValue(QStringLiteral("splitterSizes"), m_splitter->saveState());
    }
}

/*!
 * \brief MainWindow::setActivationSuccessful
 */
void MainWindow::setActivationSuccessful()
{
    m_isProVersionActivated = true;
    emit proVersionCheck(QVariant(m_isProVersionActivated));
    m_localLicenseData->setValue(QStringLiteral("isLicenseActivated"), true);
    m_aboutWindow.setProVersion(m_isProVersionActivated);
}

/*!
 * \brief MainWindow::checkProVersion
 */
void MainWindow::checkProVersion()
{
#if defined(PRO_VERSION)
    m_isProVersionActivated = true;
#else
    if (m_localLicenseData->value(QStringLiteral("isLicenseActivated"), "NULL") != "NULL") {
        m_isProVersionActivated =
                m_localLicenseData->value(QStringLiteral("isLicenseActivated")).toBool();
    }
#endif
    emit proVersionCheck(QVariant(m_isProVersionActivated));
    m_aboutWindow.setProVersion(m_isProVersionActivated);
}

/*!
 * \brief MainWindow::setupDatabases
 * Setting up the database:
 */
void MainWindow::setupDatabases()
{
    m_settingsDatabase =
            new QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Awesomeness"),
                          QStringLiteral("Settings"), this);

#if !defined(PRO_VERSION)
#  if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    m_localLicenseData =
            new QSettings(QSettings::NativeFormat, QSettings::UserScope,
                          QStringLiteral("Awesomeness"), QStringLiteral(".notesLicenseData"), this);
#  else
    m_localLicenseData =
            new QSettings(QSettings::NativeFormat, QSettings::UserScope,
                          QStringLiteral("Awesomeness"), QStringLiteral("notesLicenseData"), this);
#  endif
#endif

    m_settingsDatabase->setFallbacksEnabled(false);
    bool needMigrateFromV1_5_0 = false;
    if (m_settingsDatabase->value(QStringLiteral("version"), "NULL") == "NULL") {
        needMigrateFromV1_5_0 = true;
    }
    auto versionString = m_settingsDatabase->value(QStringLiteral("version")).toString();
    auto major = versionString.split(".").first().toInt();
    if (major < 2) {
        needMigrateFromV1_5_0 = true;
    }
    initializeSettingsDatabase();

    bool doCreate = false;
    QFileInfo fi(m_settingsDatabase->fileName());
    QDir dir(fi.absolutePath());
    bool folderCreated = dir.mkpath(QStringLiteral("."));
    if (!folderCreated)
        qFatal("ERROR: Can't create settings folder : %s",
               dir.absolutePath().toStdString().c_str());
    QString defaultDBPath = dir.path() + QDir::separator() + QStringLiteral("notes.db");

    QString noteDBFilePath =
            m_settingsDatabase->value(QStringLiteral("noteDBFilePath"), QString()).toString();
    if (noteDBFilePath.isEmpty()) {
        noteDBFilePath = defaultDBPath;
    }
    QFileInfo noteDBFilePathInf(noteDBFilePath);
    QFileInfo defaultDBPathInf(defaultDBPath);
    if ((!noteDBFilePathInf.exists()) && (defaultDBPathInf.exists())) {
        QDir().mkpath(noteDBFilePathInf.absolutePath());
        QFile defaultDBFile(defaultDBPath);
        defaultDBFile.rename(noteDBFilePath);
    }
    if (QFile::exists(noteDBFilePath) && needMigrateFromV1_5_0) {
        {
            auto m_db = QSqlDatabase::addDatabase("QSQLITE", DEFAULT_DATABASE_NAME);
            m_db.setDatabaseName(noteDBFilePath);
            if (m_db.open()) {
                QSqlQuery query(m_db);
                if (query.exec("SELECT name FROM sqlite_master WHERE type='table' AND "
                               "name='tag_table';")) {
                    if (query.next() && query.value(0).toString() == "tag_table") {
                        needMigrateFromV1_5_0 = false;
                    }
                }
                m_db.close();
            }
            m_db = QSqlDatabase::database();
        }
        QSqlDatabase::removeDatabase(DEFAULT_DATABASE_NAME);
    }
    if (!QFile::exists(noteDBFilePath)) {
        QFile noteDBFile(noteDBFilePath);
        if (!noteDBFile.open(QIODevice::WriteOnly))
            qFatal("ERROR : Can't create database file");

        noteDBFile.close();
        doCreate = true;
        needMigrateFromV1_5_0 = false;
    } else if (needMigrateFromV1_5_0) {
        QFile noteDBFile(noteDBFilePath);
        noteDBFile.rename(dir.path() + QDir::separator() + QStringLiteral("oldNotes.db"));
        noteDBFile.setFileName(noteDBFilePath);
        if (!noteDBFile.open(QIODevice::WriteOnly))
            qFatal("ERROR : Can't create database file");

        noteDBFile.close();
        doCreate = true;
    }

    if (needMigrateFromV1_5_0) {
        m_settingsDatabase->setValue(QStringLiteral("version"), qApp->applicationVersion());
    }
    m_dbManager = new DBManager;
    m_dbThread = new QThread;
    m_dbThread->setObjectName(QStringLiteral("dbThread"));
    m_dbManager->moveToThread(m_dbThread);
    connect(m_dbThread, &QThread::started, this, [=]() {
        setTheme(m_currentTheme);
        emit requestOpenDBManager(noteDBFilePath, doCreate);
        if (needMigrateFromV1_5_0) {
            emit requestMigrateNotesFromV1_5_0(dir.path() + QDir::separator()
                                               + QStringLiteral("oldNotes.db"));
        }
    });
    connect(this, &MainWindow::requestOpenDBManager, m_dbManager,
            &DBManager::onOpenDBManagerRequested, Qt::QueuedConnection);
    connect(this, &MainWindow::requestMigrateNotesFromV1_5_0, m_dbManager,
            &DBManager::onMigrateNotesFrom1_5_0Requested, Qt::QueuedConnection);
    connect(m_dbThread, &QThread::finished, m_dbManager, &QObject::deleteLater);
    m_dbThread->start();
}

/*!
 * \brief MainWindow::setupModelView
 */
void MainWindow::setupModelView()
{
    m_listView = ui->listView;
    m_tagPool = new TagPool(m_dbManager);
    m_listModel = new NoteListModel(m_listView);
    m_listView->setTagPool(m_tagPool);
    m_listView->setModel(m_listModel);
    m_listViewLogic = new ListViewLogic(m_listView, m_listModel, m_searchEdit, m_clearButton,
                                        m_tagPool, m_dbManager, this);
    m_treeView = static_cast<NodeTreeView *>(ui->treeView);
    m_treeView->setModel(m_treeModel);
    m_treeViewLogic = new TreeViewLogic(m_treeView, m_treeModel, m_dbManager, m_listView, this);
    m_noteEditorLogic =
            new NoteEditorLogic(m_searchEdit, static_cast<TagListView *>(ui->tagListView),
                                m_tagPool, m_dbManager, m_blockModel, this);
    m_editorSettingsQuickView.rootContext()->setContextProperty("noteEditorLogic",
                                                                m_noteEditorLogic);
}

/*!
 * \brief MainWindow::restoreStates
 * Restore the latest states (if there are any) of the window and the splitter from
 * the settings database
 */
void MainWindow::restoreStates()
{
#if defined(Q_OS_MACOS)
    bool nativeByDefault = false;
#else
    bool nativeByDefault = true;
#endif
    setUseNativeWindowFrame(
            m_settingsDatabase->value(QStringLiteral("useNativeWindowFrame"), nativeByDefault)
                    .toBool());

    setHideToTray(m_settingsDatabase->value(QStringLiteral("hideToTray"), true).toBool());
    if (m_hideToTray) {
        setupTrayIcon();
    }

    if (m_settingsDatabase->value(QStringLiteral("windowGeometry"), "NULL") != "NULL")
        restoreGeometry(m_settingsDatabase->value(QStringLiteral("windowGeometry")).toByteArray());

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // Set margin to zero if the window is maximized
    if (isMaximized()) {
        setMargins(QMargins());
    }
#endif

#if defined(UPDATE_CHECKER)
    if (m_settingsDatabase->value(QStringLiteral("dontShowUpdateWindow"), "NULL") != "NULL")
        m_dontShowUpdateWindow =
                m_settingsDatabase->value(QStringLiteral("dontShowUpdateWindow")).toBool();
#endif

    m_splitter->setCollapsible(0, true);
    m_splitter->setCollapsible(1, true);
    m_splitter->resize(width() - m_layoutMargin, height() - m_layoutMargin);

    if (m_settingsDatabase->contains(QStringLiteral("splitterSizes"))) {
        m_splitter->restoreState(
                m_settingsDatabase->value(QStringLiteral("splitterSizes")).toByteArray());
        // in rare cases, the splitter sizes can be zero, which causes bugs (issue #531)
        auto splitterSizes = m_splitter->sizes();
        splitterSizes[0] = std::max(splitterSizes[0], m_foldersWidget->minimumWidth());
        splitterSizes[1] = std::max(splitterSizes[1], m_noteListWidget->minimumWidth());
        m_splitter->setSizes(splitterSizes);
    }

    m_foldersWidget->setHidden(
            m_settingsDatabase->value(QStringLiteral("isTreeCollapsed")).toBool());
    m_noteListWidget->setHidden(
            m_settingsDatabase->value(QStringLiteral("isNoteListCollapsed")).toBool());

#if defined(Q_OS_MACOS)
    if (m_foldersWidget->isHidden()) {
        ui->verticalSpacer_upSearchEdit->setMinimumHeight(25);
        ui->verticalSpacer_upSearchEdit->setMaximumHeight(25);
    }
#else
    if (!m_useNativeWindowFrame && m_foldersWidget->isHidden()) {
        ui->verticalSpacer_upSearchEdit->setMinimumHeight(25);
        ui->verticalSpacer_upSearchEdit->setMaximumHeight(25);
    }
#endif

    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, false);

    QString selectedFontTypefaceFromDatabase =
            m_settingsDatabase->value(QStringLiteral("selectedFontTypeface"), "NULL").toString();
    if (selectedFontTypefaceFromDatabase != "NULL") {
        if (selectedFontTypefaceFromDatabase == "Mono") {
            m_currentFontTypeface = FontTypeface::Mono;
        } else if (selectedFontTypefaceFromDatabase == "Serif") {
            m_currentFontTypeface = FontTypeface::Serif;
        } else if (selectedFontTypefaceFromDatabase == "SansSerif") {
            m_currentFontTypeface = FontTypeface::SansSerif;
        }
    }

    if (m_settingsDatabase->value(QStringLiteral("editorMediumFontSize"), "NULL") != "NULL") {
        m_editorMediumFontSize =
                m_settingsDatabase->value(QStringLiteral("editorMediumFontSize")).toInt();
    } else {
#ifdef __APPLE__
        m_editorMediumFontSize = 17;
#else
        m_editorMediumFontSize = 13;
#endif
    }
    m_currentFontPointSize = m_editorMediumFontSize;

    bool isTextFullWidth = false;
    if (m_settingsDatabase->value(QStringLiteral("isTextFullWidth"), "NULL") != "NULL") {
        isTextFullWidth = m_settingsDatabase->value(QStringLiteral("isTextFullWidth")).toBool();
        //        if (isTextFullWidth) {
        //            m_textEdit->setLineWrapMode(QTextEdit::WidgetWidth);
        //        } else {
        //            m_textEdit->setLineWrapMode(QTextEdit::FixedColumnWidth);
        //        }
    } else {
        // Default option, Focus Mode (FixedColumnWidth) or Full Width (WidgetWidth)
        //        m_textEdit->setLineWrapMode(QTextEdit::FixedColumnWidth);
    }

    if (m_settingsDatabase->value(QStringLiteral("charsLimitPerFontMono"), "NULL") != "NULL")
        m_currentCharsLimitPerFont.mono =
                m_settingsDatabase->value(QStringLiteral("charsLimitPerFontMono")).toInt();
    if (m_settingsDatabase->value(QStringLiteral("charsLimitPerFontSerif"), "NULL") != "NULL")
        m_currentCharsLimitPerFont.serif =
                m_settingsDatabase->value(QStringLiteral("charsLimitPerFontSerif")).toInt();
    if (m_settingsDatabase->value(QStringLiteral("charsLimitPerFontSansSerif"), "NULL") != "NULL")
        m_currentCharsLimitPerFont.sansSerif =
                m_settingsDatabase->value(QStringLiteral("charsLimitPerFontSansSerif")).toInt();

    if (m_settingsDatabase->value(QStringLiteral("chosenMonoFont"), "NULL") != "NULL") {
        QString fontName = m_settingsDatabase->value(QStringLiteral("chosenMonoFont")).toString();
        int fontIndex = m_listOfMonoFonts.indexOf(fontName);
        if (fontIndex != -1) {
            m_chosenMonoFontIndex = fontIndex;
        }
    }
    if (m_settingsDatabase->value(QStringLiteral("chosenSerifFont"), "NULL") != "NULL") {
        QString fontName = m_settingsDatabase->value(QStringLiteral("chosenSerifFont")).toString();
        int fontIndex = m_listOfSerifFonts.indexOf(fontName);
        if (fontIndex != -1) {
            m_chosenSerifFontIndex = fontIndex;
        }
    }
    if (m_settingsDatabase->value(QStringLiteral("chosenSansSerifFont"), "NULL") != "NULL") {
        QString fontName =
                m_settingsDatabase->value(QStringLiteral("chosenSansSerifFont")).toString();
        int fontIndex = m_listOfSansSerifFonts.indexOf(fontName);
        if (fontIndex != -1) {
            m_chosenSansSerifFontIndex = fontIndex;
        }
    }

    if (m_settingsDatabase->value(QStringLiteral("theme"), "NULL") != "NULL") {
        QString chosenTheme = m_settingsDatabase->value(QStringLiteral("theme")).toString();
        if (chosenTheme == "Light") {
            m_currentTheme = Theme::Light;
        } else if (chosenTheme == "Dark") {
            m_currentTheme = Theme::Dark;
        } else if (chosenTheme == "Sepia") {
            m_currentTheme = Theme::Sepia;
        }
    }

    setCurrentFontBasedOnTypeface(m_currentFontTypeface);
    setTheme(m_currentTheme);

    auto expandedFolder =
            m_settingsDatabase->value(QStringLiteral("currentExpandedFolder"), QStringList{})
                    .toStringList();
    auto isSelectingFolder =
            m_settingsDatabase->value(QStringLiteral("isSelectingFolder"), true).toBool();
    auto currentSelectFolder =
            m_settingsDatabase->value(QStringLiteral("currentSelectFolder"), QString{}).toString();
    auto currentSelectTagsId =
            m_settingsDatabase->value(QStringLiteral("currentSelectTagsId"), QStringList{})
                    .toStringList();
    QSet<int> tags;
    for (const auto &tagId : qAsConst(currentSelectTagsId)) {
        tags.insert(tagId.toInt());
    }
    m_treeViewLogic->setLastSavedState(isSelectingFolder, currentSelectFolder, tags,
                                       expandedFolder);
    auto currentSelectNotes =
            m_settingsDatabase->value(QStringLiteral("currentSelectNotesId"), QStringList{})
                    .toStringList();
    QSet<int> notesId;
    for (const auto &id : qAsConst(currentSelectNotes)) {
        notesId.insert(id.toInt());
    }
    m_listViewLogic->setLastSavedState(notesId);

    updateSelectedOptionsEditorSettings();
    updateFrame();
}

/*!
 * \brief MainWindow::setButtonsAndFieldsEnabled
 * \param doEnable
 */
void MainWindow::setButtonsAndFieldsEnabled(bool doEnable)
{
    m_greenMaximizeButton->setEnabled(doEnable);
    m_redCloseButton->setEnabled(doEnable);
    m_yellowMinimizeButton->setEnabled(doEnable);
    m_newNoteButton->setEnabled(doEnable);
    m_searchEdit->setEnabled(doEnable);
    m_globalSettingsButton->setEnabled(doEnable);
}

/*!
 * \brief MainWindow::setEditorSettingsFromQuickViewVisibility
 * \param isVisible
 */
void MainWindow::setEditorSettingsFromQuickViewVisibility(bool isVisible)
{
    m_isEditorSettingsFromQuickViewVisible = isVisible;
}

/*!
 * \brief MainWindow::setEditorSettingsScrollBarPosition
 * \param isVisible
 */
void MainWindow::setEditorSettingsScrollBarPosition(double position)
{
    emit editorSettingsScrollBarPositionChanged(QVariant(position));
}

/*!
 * \brief MainWindow::toggleMarkdown
 * Enable or disable markdown
 */
void MainWindow::setMarkdownEnabled(bool isMarkdownEnabled)
{
    updateSelectedOptionsEditorSettings();
}

/*!
 * \brief MainWindow::onNewNoteButtonClicked
 * Create a new note when clicking the 'new note' button
 */
void MainWindow::onNewNoteButtonClicked()
{
    if (!m_newNoteButton->isVisible()) {
        return;
    }
    if (m_listViewLogic->isAnimationRunning()) {
        return;
    }

    // save the data of the previous selected
    m_noteEditorLogic->saveNoteToDB();

    if (!m_searchEdit->text().isEmpty()) {
        m_listViewLogic->clearSearch(true);
    } else {
        createNewNote();
    }
}

/*!
 * \brief MainWindow::onGlobalSettingsButtonClicked
 * Open up the menu when clicking the global settings button
 */
void MainWindow::onGlobalSettingsButtonClicked()
{
    QMenu mainMenu;

#if !defined(Q_OS_MACOS)
    QMenu *viewMenu = mainMenu.addMenu(tr("&View"));
    viewMenu->setToolTipsVisible(true);
#endif

    QMenu *importExportNotesMenu = mainMenu.addMenu(tr("&Import/Export Notes"));
    importExportNotesMenu->setToolTipsVisible(true);
    mainMenu.setToolTipsVisible(true);

    QShortcut *closeMenu = new QShortcut(Qt::Key_F10, &mainMenu);
    closeMenu->setContext(Qt::ApplicationShortcut);
    connect(closeMenu, &QShortcut::activated, &mainMenu, &QMenu::close);

#if defined(Q_OS_WINDOWS) || defined(Q_OS_WIN)
    setStyleSheet(m_styleSheet);
    setCSSClassesAndUpdate(&mainMenu, "menu");
#endif

#ifdef __APPLE__
    mainMenu.setFont(QFont(m_displayFont, 13));
    importExportNotesMenu->setFont(QFont(m_displayFont, 13));
#else
    mainMenu.setFont(QFont(m_displayFont, 10, QFont::Normal));
    viewMenu->setFont(QFont(m_displayFont, 10, QFont::Normal));
    importExportNotesMenu->setFont(QFont(m_displayFont, 10, QFont::Normal));
#endif

#if defined(UPDATE_CHECKER)
    // Check for update action
    QAction *checkForUpdatesAction = mainMenu.addAction(tr("Check For &Updates"));
    connect(checkForUpdatesAction, &QAction::triggered, this, &MainWindow::checkForUpdates);
#endif

    // Autostart
    QAction *autostartAction = mainMenu.addAction(tr("&Start automatically"));
    connect(autostartAction, &QAction::triggered, this,
            [=]() { m_autostart.setAutostart(autostartAction->isChecked()); });
    autostartAction->setCheckable(true);
    autostartAction->setChecked(m_autostart.isAutostart());

    // hide to tray
    QAction *hideToTrayAction = mainMenu.addAction(tr("&Hide to tray"));
    connect(hideToTrayAction, &QAction::triggered, this, [=]() {
        m_settingsDatabase->setValue(QStringLiteral("hideToTray"), hideToTrayAction->isChecked());
    });
    hideToTrayAction->setCheckable(true);
    hideToTrayAction->setChecked(m_hideToTray);
    connect(hideToTrayAction, &QAction::triggered, this, [this]() {
        setHideToTray(!m_hideToTray);
        if (m_hideToTray) {
            setupTrayIcon();
        } else {
            m_trayIcon->hide();
        }
    });

    QAction *changeDBPathAction = mainMenu.addAction(tr("&Change database path"));
    connect(changeDBPathAction, &QAction::triggered, this, [=]() {
        auto btn = QMessageBox::question(this, "Are you sure you want to change the database path?",
                                         "Are you sure you want to change the database path?");
        if (btn == QMessageBox::Yes) {
            auto newDbPath = QFileDialog::getSaveFileName(this, "New Database path", "notes.db");
            if (!newDbPath.isEmpty()) {
                m_settingsDatabase->setValue(QStringLiteral("noteDBFilePath"), newDbPath);
                QFileInfo noteDBFilePathInf(newDbPath);
                QDir().mkpath(noteDBFilePathInf.absolutePath());
                emit requestChangeDatabasePath(newDbPath);
            }
        }
    });

    // About Notes
    QAction *aboutAction = mainMenu.addAction(tr("&About Notes"));
    connect(aboutAction, &QAction::triggered, this, [&]() { m_aboutWindow.show(); });

    mainMenu.addSeparator();

    // Close the app
    QAction *quitAppAction = mainMenu.addAction(tr("&Quit"));
    connect(quitAppAction, &QAction::triggered, this, &MainWindow::QuitApplication);

    // Export notes action
    QAction *exportNotesFileAction = importExportNotesMenu->addAction(tr("&Export"));
    exportNotesFileAction->setToolTip(tr("Save notes to a file"));
    connect(exportNotesFileAction, &QAction::triggered, this, &MainWindow::exportNotesFile);

    // Import notes action
    QAction *importNotesFileAction = importExportNotesMenu->addAction(tr("&Import"));
    importNotesFileAction->setToolTip(tr("Add notes from a file"));
    connect(importNotesFileAction, &QAction::triggered, this, &MainWindow::importNotesFile);

    // Restore notes action
    QAction *restoreNotesFileAction = importExportNotesMenu->addAction(tr("&Restore"));
    restoreNotesFileAction->setToolTip(tr("Replace all notes with notes from a file"));
    connect(restoreNotesFileAction, &QAction::triggered, this, &MainWindow::restoreNotesFile);

#if !defined(Q_OS_MACOS)
    // Use native frame action
    QAction *useNativeFrameAction = viewMenu->addAction(tr("&Use native window frame"));
    useNativeFrameAction->setToolTip(tr("Use the window frame provided by the window manager"));
    useNativeFrameAction->setCheckable(true);
    useNativeFrameAction->setChecked(m_useNativeWindowFrame);
    connect(useNativeFrameAction, &QAction::triggered, this,
            [this]() { setUseNativeWindowFrame(!m_useNativeWindowFrame); });
#endif

    mainMenu.exec(m_globalSettingsButton->mapToGlobal(QPoint(0, m_globalSettingsButton->height())));
}

/*!
 * \brief MainWindow::updateSelectedOptionsEditorSettings
 */
void MainWindow::updateSelectedOptionsEditorSettings()
{
    QJsonObject dataToSendToView;
    dataToSendToView["currentFontTypeface"] = to_string(m_currentFontTypeface).c_str();
    dataToSendToView["currentTheme"] = to_string(m_currentTheme).c_str();
    //    dataToSendToView["isTextFullWidth"] = m_textEdit->lineWrapMode() ==
    //    QTextEdit::WidgetWidth;
    dataToSendToView["isNoteListCollapsed"] = m_noteListWidget->isHidden();
    dataToSendToView["isFoldersTreeCollapsed"] = m_foldersWidget->isHidden();
    dataToSendToView["isMarkdownDisabled"] = !m_noteEditorLogic->markdownEnabled();
    dataToSendToView["isStayOnTop"] = m_alwaysStayOnTop;
    emit settingsChanged(QVariant(dataToSendToView));
}

/*!
 * \brief MainWindow::changeEditorFontTypeFromStyleButtons
 * Change the font based on the type passed from the Style Editor Window
 */
void MainWindow::changeEditorFontTypeFromStyleButtons(FontTypeface::Value fontTypeface,
                                                      int chosenFontIndex)
{
    if (chosenFontIndex < 0)
        return;

    switch (fontTypeface) {
    case FontTypeface::Mono:
        if (chosenFontIndex > m_listOfMonoFonts.size() - 1)
            return;
        m_chosenMonoFontIndex = chosenFontIndex;
        break;
    case FontTypeface::Serif:
        if (chosenFontIndex > m_listOfSerifFonts.size() - 1)
            return;
        m_chosenSerifFontIndex = chosenFontIndex;
        break;
    case FontTypeface::SansSerif:
        if (chosenFontIndex > m_listOfSansSerifFonts.size() - 1)
            return;
        m_chosenSansSerifFontIndex = chosenFontIndex;
        break;
    }

    setCurrentFontBasedOnTypeface(fontTypeface);

    updateSelectedOptionsEditorSettings();
}

/*!
 * \brief MainWindow::changeEditorFontSizeFromStyleButtons
 * Change the font size based on the button pressed in the Style Editor Window
 * Increase / Decrease
 */
void MainWindow::changeEditorFontSizeFromStyleButtons(FontSizeAction::Value fontSizeAction)
{
    switch (fontSizeAction) {
    case FontSizeAction::FontSizeIncrease:
        m_editorMediumFontSize += 1;
        setCurrentFontBasedOnTypeface(m_currentFontTypeface);
        break;
    case FontSizeAction::FontSizeDecrease:
        m_editorMediumFontSize -= 1;
        setCurrentFontBasedOnTypeface(m_currentFontTypeface);
        break;
    }
    setTheme(m_currentTheme);
}

/*!
 * \brief MainWindow::setTheme
 * Changes the app theme
 */
void MainWindow::setTheme(Theme::Value theme)
{
    m_currentTheme = theme;

    setCSSThemeAndUpdate(this, theme);
    setCSSThemeAndUpdate(ui->verticalSpacer_upSearchEdit, theme);
    setCSSThemeAndUpdate(ui->verticalSpacer_upSearchEdit2, theme);
    setCSSThemeAndUpdate(ui->listviewLabel1, theme);
    setCSSThemeAndUpdate(ui->searchEdit, theme);
    setCSSThemeAndUpdate(ui->verticalSplitterLine_left, theme);
    setCSSThemeAndUpdate(ui->verticalSplitterLine_middle, theme);
    setCSSThemeAndUpdate(ui->frameLeft, m_currentTheme);

    switch (theme) {
    case Theme::Light: {
        QJsonObject themeData{ { "theme", QStringLiteral("Light") },
                               { "backgroundColor", "#f7f7f7" } };
        emit themeChanged(QVariant(themeData));
        m_currentEditorTextColor = QColor(26, 26, 26);
        m_searchButton->setStyleSheet("QToolButton { color: rgb(205, 205, 205) }");
        m_clearButton->setStyleSheet("QToolButton { color: rgb(114, 114, 114) }");
        break;
    }
    case Theme::Dark: {
        QJsonObject themeData{ { "theme", QStringLiteral("Dark") },
                               { "backgroundColor", "#191919" } };
        emit themeChanged(QVariant(themeData));
        m_currentEditorTextColor = QColor(223, 224, 224);
        m_searchButton->setStyleSheet("QToolButton { color: rgb(68, 68, 68) }");
        m_clearButton->setStyleSheet("QToolButton { color: rgb(147, 144, 147) }");
        break;
    }
    case Theme::Sepia: {
        QJsonObject themeData{ { "theme", QStringLiteral("Sepia") },
                               { "backgroundColor", "#fbf0d9" } };
        emit themeChanged(QVariant(themeData));
        m_currentEditorTextColor = QColor(50, 30, 3);
        m_searchButton->setStyleSheet("QToolButton { color: rgb(205, 205, 205) }");
        m_clearButton->setStyleSheet("QToolButton { color: rgb(114, 114, 114) }");
        break;
    }
    }
    m_noteEditorLogic->setTheme(theme, m_currentEditorTextColor, m_editorMediumFontSize);
    m_listViewLogic->setTheme(theme);
    m_aboutWindow.setTheme(theme);
    m_treeViewLogic->setTheme(theme);
    ui->tagListView->setTheme(theme);
    m_blockModel->setTheme(theme);

    updateSelectedOptionsEditorSettings();
}

void MainWindow::deleteSelectedNote()
{
    m_noteEditorLogic->deleteCurrentNote();
    if (m_listModel->rowCount() == 1) {
        emit m_listViewLogic->closeNoteEditor();
    }
}

/*!
 * \brief MainWindow::onClearButtonClicked
 * clears the search and
 * select the note that was selected before searching if it is still valid.
 */
void MainWindow::onClearButtonClicked()
{
    m_listViewLogic->clearSearch();
}

/*!
 * \brief MainWindow::createNewNote
 * create a new note
 * add it to the database
 * add it to the scrollArea
 */
void MainWindow::createNewNote()
{
    m_listView->scrollToTop();
    QModelIndex newNoteIndex;
    if (!m_noteEditorLogic->isTempNote()) {
        // clear the textEdit
        m_blockModel->blockSignals(true);
        m_noteEditorLogic->closeEditor();
        m_blockModel->blockSignals(false);

        NodeData tmpNote;
        tmpNote.setNodeType(NodeData::Note);
        QDateTime noteDate = QDateTime::currentDateTime();
        tmpNote.setCreationDateTime(noteDate);
        tmpNote.setLastModificationDateTime(noteDate);
        tmpNote.setFullTitle(QStringLiteral("New Note"));
        auto inf = m_listViewLogic->listViewInfo();
        if ((!inf.isInTag) && (inf.parentFolderId > SpecialNodeID::RootFolder)) {
            NodeData parent;
            QMetaObject::invokeMethod(m_dbManager, "getNode", Qt::BlockingQueuedConnection,
                                      Q_RETURN_ARG(NodeData, parent),
                                      Q_ARG(int, inf.parentFolderId));
            if (parent.nodeType() == NodeData::Folder) {
                tmpNote.setParentId(parent.id());
                tmpNote.setParentName(parent.fullTitle());
            } else {
                tmpNote.setParentId(SpecialNodeID::DefaultNotesFolder);
                tmpNote.setParentName("Notes");
            }
        } else {
            tmpNote.setParentId(SpecialNodeID::DefaultNotesFolder);
            tmpNote.setParentName("Notes");
        }
        int noteId = SpecialNodeID::InvalidNodeId;
        QMetaObject::invokeMethod(m_dbManager, "nextAvailableNodeId", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(int, noteId));
        tmpNote.setId(noteId);
        tmpNote.setIsTempNote(true);
        if (inf.isInTag) {
            tmpNote.setTagIds(inf.currentTagList);
        }
        // insert the new note to NoteListModel
        newNoteIndex = m_listModel->insertNote(tmpNote, 0);

        // update the editor
        m_noteEditorLogic->showNotesInEditor({ tmpNote });
    } else {
        newNoteIndex = m_listModel->getNoteIndex(m_noteEditorLogic->currentEditingNoteId());
        m_listView->animateAddedRow({ newNoteIndex });
    }
    // update the current selected index
    m_listView->setCurrentIndexC(newNoteIndex);
    m_blockEditorWidget->setFocus();
    emit focusOnEditor();
}

void MainWindow::selectNoteDown()
{
    if (m_listView->hasFocus()) {
        m_listViewLogic->selectNoteDown();
    }
}

/*!
 * \brief MainWindow::setFocusOnText
 * Set focus on editor
 */
void MainWindow::setFocusOnText()
{
    if (m_noteEditorLogic->currentEditingNoteId() != SpecialNodeID::InvalidNodeId) {
        m_listView->setCurrentRowActive(true);
        m_blockEditorWidget->setFocus();
        emit focusOnEditor();
    }
}

void MainWindow::selectNoteUp()
{
    if (m_listView->hasFocus()) {
        m_listViewLogic->selectNoteUp();
    }
}

/*!
 * \brief MainWindow::fullscreenWindow
 * Switch to fullscreen mode
 */
void MainWindow::fullscreenWindow()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (isFullScreen()) {
        if (!isMaximized()) {
            QMargins margins(m_layoutMargin, m_layoutMargin, m_layoutMargin, m_layoutMargin);
            setMargins(margins);
        }

        setWindowState(windowState() & ~Qt::WindowFullScreen);
    } else {
        setWindowState(windowState() | Qt::WindowFullScreen);
        setMargins(QMargins());
    }

#elif _WIN32
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
#endif
}

/*!
 * \brief MainWindow::maximizeWindow
 * Maximize the window
 */
void MainWindow::maximizeWindow()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (isMaximized()) {
        if (!isFullScreen()) {
            QMargins margins(m_layoutMargin, m_layoutMargin, m_layoutMargin, m_layoutMargin);

            setMargins(margins);
            setWindowState(windowState() & ~Qt::WindowMaximized);
        } else {
            setWindowState(windowState() & ~Qt::WindowFullScreen);
        }

    } else {
        setWindowState(windowState() | Qt::WindowMaximized);
        setMargins(QMargins());
    }
#elif _WIN32
    if (isMaximized()) {
        setWindowState(windowState() & ~Qt::WindowMaximized);
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    } else if (isFullScreen()) {
        setWindowState((windowState() | Qt::WindowMaximized) & ~Qt::WindowFullScreen);
        setGeometry(qApp->primaryScreen()->availableGeometry());
    } else {
        setWindowState(windowState() | Qt::WindowMaximized);
        setGeometry(qApp->primaryScreen()->availableGeometry());
    }
#endif
}

/*!
 * \brief MainWindow::minimizeWindow
 * Minimize the window
 */
void MainWindow::minimizeWindow()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QMargins margins(m_layoutMargin, m_layoutMargin, m_layoutMargin, m_layoutMargin);
    setMargins(margins);
#endif

    // BUG : QTBUG-57902 minimize doesn't store the window state before minimizing
    showMinimized();
}

/*!
 * \brief MainWindow::QuitApplication
 * Exit the application
 * Save the geometry of the app to the settings
 * Save the current note if it's note temporary one otherwise remove it from DB
 */
void MainWindow::QuitApplication()
{
    if (windowState() != Qt::WindowFullScreen) {
        m_settingsDatabase->setValue(QStringLiteral("windowGeometry"), saveGeometry());
    }

    m_noteEditorLogic->saveNoteToDB();

#if defined(UPDATE_CHECKER)
    m_settingsDatabase->setValue(QStringLiteral("dontShowUpdateWindow"), m_dontShowUpdateWindow);
#endif
    m_settingsDatabase->setValue(QStringLiteral("splitterSizes"), m_splitter->saveState());

    m_settingsDatabase->setValue(QStringLiteral("isTreeCollapsed"), m_foldersWidget->isHidden());
    m_settingsDatabase->setValue(QStringLiteral("isNoteListCollapsed"),
                                 m_noteListWidget->isHidden());

    m_settingsDatabase->setValue(QStringLiteral("selectedFontTypeface"),
                                 to_string(m_currentFontTypeface).c_str());
    m_settingsDatabase->setValue(QStringLiteral("editorMediumFontSize"), m_editorMediumFontSize);
    //    m_settingsDatabase->setValue(QStringLiteral("isTextFullWidth"),
    //                                 m_textEdit->lineWrapMode() == QTextEdit::WidgetWidth ? true
    //                                                                                      : false);
    m_settingsDatabase->setValue(QStringLiteral("charsLimitPerFontMono"),
                                 m_currentCharsLimitPerFont.mono);
    m_settingsDatabase->setValue(QStringLiteral("charsLimitPerFontSerif"),
                                 m_currentCharsLimitPerFont.serif);
    m_settingsDatabase->setValue(QStringLiteral("charsLimitPerFontSansSerif"),
                                 m_currentCharsLimitPerFont.sansSerif);
    m_settingsDatabase->setValue(QStringLiteral("theme"), to_string(m_currentTheme).c_str());
    m_settingsDatabase->setValue(QStringLiteral("chosenMonoFont"),
                                 m_listOfMonoFonts.at(m_chosenMonoFontIndex));
    m_settingsDatabase->setValue(QStringLiteral("chosenSerifFont"),
                                 m_listOfSerifFonts.at(m_chosenSerifFontIndex));
    m_settingsDatabase->setValue(QStringLiteral("chosenSansSerifFont"),
                                 m_listOfSansSerifFonts.at(m_chosenSansSerifFontIndex));

    m_settingsDatabase->sync();

    m_noteEditorLogic->closeEditor();

    QCoreApplication::quit();
}

/*!
 * \brief MainWindow::checkForUpdates
 * Called when the "Check for Updates" menu item is clicked, this function
 * instructs the updater window to check if there are any updates available
 */
#if defined(UPDATE_CHECKER)
void MainWindow::checkForUpdates()
{
    m_updater.checkForUpdates(true);
}
#endif

/*!
 * \brief MainWindow::importNotesFile
 * Called when the "Import Notes" menu button is clicked. this function will
 * prompt the user to select a file, attempt to load the file, and update the DB
 * if valid.
 * The user is presented with a dialog box if the upload/import fails for any reason.
 */
void MainWindow::importNotesFile()
{
    executeImport(false);
}

/*!
 * \brief MainWindow::restoreNotesFile
 * Called when the "Restore Notes" menu button is clicked. this function will
 * prompt the user to select a file, attempt to load the file, and update the DB
 * if valid.
 * The user is presented with a dialog box if the upload/import/restore fails for any reason.
 */
void MainWindow::restoreNotesFile()
{
    if (m_listModel->rowCount() > 0) {
        QMessageBox msgBox;
        msgBox.setText(tr("Warning: All current notes will be lost. Make sure to create a backup "
                          "copy before proceeding."));
        msgBox.setInformativeText(tr("Would you like to continue?"));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        if (msgBox.exec() != QMessageBox::Yes) {
            return;
        }
    }
    executeImport(true);
}

/*!
 * \brief MainWindow::executeImport
 * Executes the note import process. if replace is true all current notes will be
 * removed otherwise current notes will be kept.
 * \param replace
 */
void MainWindow::executeImport(const bool replace)
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Notes Backup File"), "",
                                                    tr("Notes Backup File (*.nbk)"));
    if (fileName.isEmpty()) {
        return;
    } else {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::information(this, tr("Unable to open file"), file.errorString());
            return;
        }
        file.close();

        setButtonsAndFieldsEnabled(false);
        if (replace) {
            emit requestRestoreNotes(fileName);
        } else {
            emit requestImportNotes(fileName);
        }
        setButtonsAndFieldsEnabled(true);
        //        emit requestNotesList(SpecialNodeID::RootFolder, true);
    }
}

/*!
 * \brief MainWindow::exportNotesFile
 * Called when the "Export Notes" menu button is clicked. this function will
 * prompt the user to select a location for the export file, and then builds
 * the file.
 * The user is presented with a dialog box if the file cannot be opened for any reason.
 * \param clicked
 */
void MainWindow::exportNotesFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Notes"), "notes.nbk",
                                                    tr("Notes Backup File (*.nbk)"));
    if (fileName.isEmpty()) {
        return;
    } else {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::information(this, tr("Unable to open file"), file.errorString());
            return;
        }
        file.close();
        emit requestExportNotes(fileName);
    }
}

void MainWindow::toggleFolderTree()
{
    if (m_foldersWidget->isHidden()) {
        expandFolderTree();
    } else {
        collapseFolderTree();
    }
    updateSelectedOptionsEditorSettings();
}

void MainWindow::collapseFolderTree()
{
    m_toggleTreeViewButton->setText(u8"\ue31c"); // keyboard_tab
    m_foldersWidget->setHidden(true);
    updateFrame();
    updateSelectedOptionsEditorSettings();
#if defined(Q_OS_MACOS)
    if (windowState() != Qt::WindowFullScreen) {
        ui->verticalSpacer_upSearchEdit->setMinimumHeight(25);
        ui->verticalSpacer_upSearchEdit->setMaximumHeight(25);
    }
#else
    if (!m_useNativeWindowFrame && windowState() != Qt::WindowFullScreen) {
        ui->verticalSpacer_upSearchEdit->setMinimumHeight(25);
        ui->verticalSpacer_upSearchEdit->setMaximumHeight(25);
    }
#endif
}

void MainWindow::expandFolderTree()
{
    m_toggleTreeViewButton->setText(u8"\uec73"); // keyboard_tab_rtl
    m_foldersWidget->setHidden(false);
    updateFrame();
    updateSelectedOptionsEditorSettings();
#if defined(Q_OS_MACOS)
    ui->verticalSpacer_upSearchEdit->setMinimumHeight(0);
    ui->verticalSpacer_upSearchEdit->setMaximumHeight(0);
#else
    if (!m_useNativeWindowFrame) {
        ui->verticalSpacer_upSearchEdit->setMinimumHeight(0);
        ui->verticalSpacer_upSearchEdit->setMaximumHeight(0);
    }
#endif
}

void MainWindow::toggleNoteList()
{
    if (m_noteListWidget->isHidden()) {
        expandNoteList();
    } else {
        collapseNoteList();
    }
    updateSelectedOptionsEditorSettings();
}

void MainWindow::collapseNoteList()
{
    m_noteListWidget->setHidden(true);
    updateFrame();
    updateSelectedOptionsEditorSettings();
}

void MainWindow::expandNoteList()
{
    m_noteListWidget->setHidden(false);
    updateFrame();
    updateSelectedOptionsEditorSettings();
}

/*!
 * \brief MainWindow::onGreenMaximizeButtonPressed
 * When the green button is pressed set it's icon accordingly
 */
void MainWindow::onGreenMaximizeButtonPressed()
{
#ifdef _WIN32
    m_greenMaximizeButton->setIcon(QIcon(":images/windows_minimize_pressed.png"));
#else
    if (windowState() == Qt::WindowFullScreen)
        m_greenMaximizeButton->setIcon(QIcon(QStringLiteral(":images/greenInPressed.png")));
    else
        m_greenMaximizeButton->setIcon(QIcon(QStringLiteral(":images/greenPressed.png")));
#endif
}

/*!
 * \brief MainWindow::onYellowMinimizeButtonPressed
 * When the yellow button is pressed set it's icon accordingly
 */
void MainWindow::onYellowMinimizeButtonPressed()
{
#ifdef _WIN32
    if (windowState() == Qt::WindowFullScreen) {
        m_yellowMinimizeButton->setIcon(
                QIcon(QStringLiteral(":images/windows_de-maximize_pressed.png")));
    } else {
        m_yellowMinimizeButton->setIcon(
                QIcon(QStringLiteral(":images/windows_maximize_pressed.png")));
    }
#else
    m_yellowMinimizeButton->setIcon(QIcon(QStringLiteral(":images/yellowPressed.png")));
#endif
}

/*!
 * \brief MainWindow::onRedCloseButtonPressed
 * When the red button is pressed set it's icon accordingly
 */
void MainWindow::onRedCloseButtonPressed()
{
#ifdef _WIN32
    m_redCloseButton->setIcon(QIcon(QStringLiteral(":images/windows_close_pressed.png")));
#else
    m_redCloseButton->setIcon(QIcon(QStringLiteral(":images/redPressed.png")));
#endif
}

/*!
 * \brief MainWindow::onGreenMaximizeButtonClicked
 * When the green button is released the window goes fullscrren
 */
void MainWindow::onGreenMaximizeButtonClicked()
{
#ifdef _WIN32
    m_greenMaximizeButton->setIcon(QIcon(QStringLiteral(":images/windows_minimize_regular.png")));

    minimizeWindow();
    m_restoreAction->setText(tr("&Show Notes"));
#else
    m_greenMaximizeButton->setIcon(QIcon(QStringLiteral(":images/green.png")));

    fullscreenWindow();
#endif
}

/*!
 * \brief MainWindow::onYellowMinimizeButtonClicked
 * When yellow button is released the window is minimized
 */
void MainWindow::onYellowMinimizeButtonClicked()
{
#ifdef _WIN32
    m_yellowMinimizeButton->setIcon(
            QIcon(QStringLiteral(":images/windows_de-maximize_regular.png")));

    fullscreenWindow();
#else
    m_yellowMinimizeButton->setIcon(QIcon(QStringLiteral(":images/yellow.png")));

    minimizeWindow();

#  if !defined(Q_OS_MAC)
    m_restoreAction->setText(tr("&Show Notes"));
#  endif
#endif
}

/*!
 * \brief MainWindow::onRedCloseButtonClicked
 * When red button is released the window get closed
 * If a new note created but wasn't edited, delete it from the database
 */
void MainWindow::onRedCloseButtonClicked()
{
#ifdef _WIN32
    m_redCloseButton->setIcon(QIcon(QStringLiteral(":images/windows_close_regular.png")));
#else
    m_redCloseButton->setIcon(QIcon(QStringLiteral(":images/red.png")));
#endif

    if (m_hideToTray && m_trayIcon->isVisible() && QSystemTrayIcon::isSystemTrayAvailable()) {
        setMainWindowVisibility(false);
    } else {
        QuitApplication();
    }
}

/*!
 * \brief MainWindow::closeEvent
 * Called when the window is about to close
 * \param event
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!event->spontaneous() || !isVisible()) {
        QuitApplication();
        return;
    }
    if (m_hideToTray && m_trayIcon->isVisible() && QSystemTrayIcon::isSystemTrayAvailable()) {
        // don't close the application, just hide to tray
        setMainWindowVisibility(false);
        event->ignore();
    } else {
        // save states and quit application
        QuitApplication();
    }
}

/*!
 * \brief MainWindow::moveEvent
 * \param event
 */
void MainWindow::moveEvent(QMoveEvent *event)
{
    QJsonObject dataToSendToView{ { "parentWindowX", x() }, { "parentWindowY", y() } };
    emit mainWindowMoved(QVariant(dataToSendToView));

    event->accept();
}

#ifndef _WIN32
/*!
 * \brief MainWindow::mousePressEvent
 * Set variables to the position of the window when the mouse is pressed
 * \param event
 */
void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (m_useNativeWindowFrame) {
        MainWindowBase::mousePressEvent(event);
        return;
    }

    m_mousePressX = event->pos().x();
    m_mousePressY = event->pos().y();

    if (event->buttons() == Qt::LeftButton) {
        if (isTitleBar(m_mousePressX, m_mousePressY)) {

#  if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            m_canMoveWindow = !window()->windowHandle()->startSystemMove();
#  else
            m_canMoveWindow = true;
#  endif

#  ifndef __APPLE__
        } else if (!isMaximized() && !isFullScreen()) {
            m_canStretchWindow = true;

            if ((m_mousePressX < width() && m_mousePressX > width() - m_layoutMargin)
                && (m_mousePressY < m_layoutMargin && m_mousePressY > 0)) {
                m_stretchSide = StretchSide::TopRight;
            } else if ((m_mousePressX < width() && m_mousePressX > width() - m_layoutMargin)
                       && (m_mousePressY < height() && m_mousePressY > height() - m_layoutMargin)) {
                m_stretchSide = StretchSide::BottomRight;
            } else if ((m_mousePressX < m_layoutMargin && m_mousePressX > 0)
                       && (m_mousePressY < m_layoutMargin && m_mousePressY > 0)) {
                m_stretchSide = StretchSide::TopLeft;
            } else if ((m_mousePressX < m_layoutMargin && m_mousePressX > 0)
                       && (m_mousePressY < height() && m_mousePressY > height() - m_layoutMargin)) {
                m_stretchSide = StretchSide::BottomLeft;
            } else if (m_mousePressX < width() && m_mousePressX > width() - m_layoutMargin) {
                m_stretchSide = StretchSide::Right;
            } else if (m_mousePressX < m_layoutMargin && m_mousePressX > 0) {
                m_stretchSide = StretchSide::Left;
            } else if (m_mousePressY < height() && m_mousePressY > height() - m_layoutMargin) {
                m_stretchSide = StretchSide::Bottom;
            } else if (m_mousePressY < m_layoutMargin && m_mousePressY > 0) {
                m_stretchSide = StretchSide::Top;
            } else {
                m_stretchSide = StretchSide::None;
            }
#  endif
        }

        event->accept();

    } else {
        MainWindowBase::mousePressEvent(event);
    }
}

/*!
 * \brief MainWindow::mouseMoveEvent
 * Move the window according to the mouse positions
 * \param event
 */
void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_useNativeWindowFrame) {
        MainWindowBase::mouseMoveEvent(event);
        return;
    }

#  ifndef __APPLE__
    if (!m_canStretchWindow && !m_canMoveWindow) {
        m_mousePressX = event->pos().x();
        m_mousePressY = event->pos().y();

        if ((m_mousePressX < width() && m_mousePressX > width() - m_layoutMargin)
            && (m_mousePressY < m_layoutMargin && m_mousePressY > 0)) {
            m_stretchSide = StretchSide::TopRight;
        } else if ((m_mousePressX < width() && m_mousePressX > width() - m_layoutMargin)
                   && (m_mousePressY < height() && m_mousePressY > height() - m_layoutMargin)) {
            m_stretchSide = StretchSide::BottomRight;
        } else if ((m_mousePressX < m_layoutMargin && m_mousePressX > 0)
                   && (m_mousePressY < m_layoutMargin && m_mousePressY > 0)) {
            m_stretchSide = StretchSide::TopLeft;
        } else if ((m_mousePressX < m_layoutMargin && m_mousePressX > 0)
                   && (m_mousePressY < height() && m_mousePressY > height() - m_layoutMargin)) {
            m_stretchSide = StretchSide::BottomLeft;
        } else if (m_mousePressX < width() && m_mousePressX > width() - m_layoutMargin) {
            m_stretchSide = StretchSide::Right;
        } else if (m_mousePressX < m_layoutMargin && m_mousePressX > 0) {
            m_stretchSide = StretchSide::Left;
        } else if (m_mousePressY < height() && m_mousePressY > height() - m_layoutMargin) {
            m_stretchSide = StretchSide::Bottom;
        } else if (m_mousePressY < m_layoutMargin && m_mousePressY > 0) {
            m_stretchSide = StretchSide::Top;
        } else {
            m_stretchSide = StretchSide::None;
        }
    }

    if (!m_canMoveWindow && !isMaximized() && !isFullScreen()) {
        switch (m_stretchSide) {
        case StretchSide::Right:
        case StretchSide::Left:
            ui->centralWidget->setCursor(Qt::SizeHorCursor);
            break;
        case StretchSide::Top:
        case StretchSide::Bottom:
            ui->centralWidget->setCursor(Qt::SizeVerCursor);
            break;
        case StretchSide::TopRight:
        case StretchSide::BottomLeft:
            ui->centralWidget->setCursor(Qt::SizeBDiagCursor);
            break;
        case StretchSide::TopLeft:
        case StretchSide::BottomRight:
            ui->centralWidget->setCursor(Qt::SizeFDiagCursor);
            break;
        default:
            if (!m_canStretchWindow)
                ui->centralWidget->setCursor(Qt::ArrowCursor);
            break;
        }
    }
#  endif

    if (m_canMoveWindow) {
        int dx = event->globalX() - m_mousePressX;
        int dy = event->globalY() - m_mousePressY;
        move(dx, dy);
    }

#  ifndef __APPLE__
    else if (m_canStretchWindow && !isMaximized() && !isFullScreen()) {
        int newX = x();
        int newY = y();
        int newWidth = width();
        int newHeight = height();

        int minY = QApplication::primaryScreen()->availableGeometry().y();

        switch (m_stretchSide) {
        case StretchSide::Right:
            newWidth = abs(event->globalPos().x() - x() + 1);
            newWidth = newWidth < minimumWidth() ? minimumWidth() : newWidth;
            break;
        case StretchSide::Left:
            newX = event->globalPos().x() - m_mousePressX;
            newX = newX > 0 ? newX : 0;
            newX = newX > geometry().bottomRight().x() - minimumWidth()
                    ? geometry().bottomRight().x() - minimumWidth()
                    : newX;
            newWidth = geometry().topRight().x() - newX + 1;
            newWidth = newWidth < minimumWidth() ? minimumWidth() : newWidth;
            break;
        case StretchSide::Top:
            newY = event->globalY() - m_mousePressY;
            newY = newY < minY ? minY : newY;
            newY = newY > geometry().bottomRight().y() - minimumHeight()
                    ? geometry().bottomRight().y() - minimumHeight()
                    : newY;
            newHeight = geometry().bottomLeft().y() - newY + 1;
            newHeight = newHeight < minimumHeight() ? minimumHeight() : newHeight;

            break;
        case StretchSide::Bottom:
            newHeight = abs(event->globalY() - y() + 1);
            newHeight = newHeight < minimumHeight() ? minimumHeight() : newHeight;

            break;
        case StretchSide::TopLeft:
            newX = event->globalPos().x() - m_mousePressX;
            newX = newX < 0 ? 0 : newX;
            newX = newX > geometry().bottomRight().x() - minimumWidth()
                    ? geometry().bottomRight().x() - minimumWidth()
                    : newX;

            newY = event->globalY() - m_mousePressY;
            newY = newY < minY ? minY : newY;
            newY = newY > geometry().bottomRight().y() - minimumHeight()
                    ? geometry().bottomRight().y() - minimumHeight()
                    : newY;

            newWidth = geometry().bottomRight().x() - newX + 1;
            newWidth = newWidth < minimumWidth() ? minimumWidth() : newWidth;

            newHeight = geometry().bottomRight().y() - newY + 1;
            newHeight = newHeight < minimumHeight() ? minimumHeight() : newHeight;

            break;
        case StretchSide::BottomLeft:
            newX = event->globalPos().x() - m_mousePressX;
            newX = newX < 0 ? 0 : newX;
            newX = newX > geometry().bottomRight().x() - minimumWidth()
                    ? geometry().bottomRight().x() - minimumWidth()
                    : newX;

            newWidth = geometry().bottomRight().x() - newX + 1;
            newWidth = newWidth < minimumWidth() ? minimumWidth() : newWidth;

            newHeight = event->globalY() - y() + 1;
            newHeight = newHeight < minimumHeight() ? minimumHeight() : newHeight;

            break;
        case StretchSide::TopRight:
            newY = event->globalY() - m_mousePressY;
            newY = newY > geometry().bottomRight().y() - minimumHeight()
                    ? geometry().bottomRight().y() - minimumHeight()
                    : newY;
            newY = newY < minY ? minY : newY;

            newWidth = event->globalPos().x() - x() + 1;
            newWidth = newWidth < minimumWidth() ? minimumWidth() : newWidth;

            newHeight = geometry().bottomRight().y() - newY + 1;
            newHeight = newHeight < minimumHeight() ? minimumHeight() : newHeight;

            break;
        case StretchSide::BottomRight:
            newWidth = event->globalPos().x() - x() + 1;
            newWidth = newWidth < minimumWidth() ? minimumWidth() : newWidth;

            newHeight = event->globalY() - y() + 1;
            newHeight = newHeight < minimumHeight() ? minimumHeight() : newHeight;

            break;
        default:
            break;
        }

        setGeometry(newX, newY, newWidth, newHeight);
    }
#  endif
    event->accept();
}

/*!
 * \brief MainWindow::mouseReleaseEvent
 * Initialize flags
 * \param event
 */
void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_useNativeWindowFrame) {
        MainWindowBase::mouseReleaseEvent(event);
        return;
    }

    m_canMoveWindow = false;
    m_canStretchWindow = false;
    QApplication::restoreOverrideCursor();
    event->accept();
}
#else
/*!
 * \brief MainWindow::mousePressEvent
 * Set variables to the position of the window when the mouse is pressed
 * \param event
 */
void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (m_useNativeWindowFrame) {
        MainWindowBase::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (isTitleBar(event->pos().x(), event->pos().y())) {

#  if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            m_canMoveWindow = !window()->windowHandle()->startSystemMove();
#  else
            m_canMoveWindow = true;
#  endif
            m_mousePressX = event->pos().x();
            m_mousePressY = event->pos().y();
        }
    }

    event->accept();
}

/*!
 * \brief MainWindow::mouseMoveEvent
 * Move the window according to the mouse positions
 * \param event
 */
void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_useNativeWindowFrame) {
        MainWindowBase::mouseMoveEvent(event);
        return;
    }

    if (m_canMoveWindow) {
        //        setCursor(Qt::ClosedHandCursor);
        int dx = event->globalPos().x() - m_mousePressX;
        int dy = event->globalY() - m_mousePressY;
        move(dx, dy);
    }
}

/*!
 * \brief MainWindow::mouseReleaseEvent
 * Initialize flags
 * \param event
 */
void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_useNativeWindowFrame) {
        MainWindowBase::mouseReleaseEvent(event);
        return;
    }

    m_canMoveWindow = false;
    //    unsetCursor();
    event->accept();
}
#endif

/*!
 * \brief MainWindow::clearSearch
 */
void MainWindow::clearSearch()
{
    m_listView->setFocusPolicy(Qt::StrongFocus);

    m_searchEdit->blockSignals(true);
    m_searchEdit->clear();
    m_searchEdit->blockSignals(false);

    m_clearButton->hide();
    m_searchEdit->setFocus();
}

void MainWindow::showErrorMessage(const QString &title, const QString &content)
{
    QMessageBox::information(this, title, content);
}

void MainWindow::setNoteListLoading()
{
    ui->listviewLabel1->setText("Loading…");
    ui->listviewLabel2->setText("");
}

void MainWindow::selectAllNotesInList()
{
    m_listViewLogic->selectAllNotes();
}

void MainWindow::updateFrame()
{
    if (m_foldersWidget->isHidden() && m_noteListWidget->isHidden()) {
        setWindowButtonsVisible(false);
    } else {
        setWindowButtonsVisible(true);
    }
}

/*!
 * \brief MainWindow::checkMigration
 */
void MainWindow::migrateFromV0_9_0()
{
    QFileInfo fi(m_settingsDatabase->fileName());
    QDir dir(fi.absolutePath());

    QString oldNoteDBPath(dir.path() + QDir::separator() + "Notes.ini");
    if (QFile::exists(oldNoteDBPath)) {
        migrateNoteFromV0_9_0(oldNoteDBPath);
    }

    QString oldTrashDBPath(dir.path() + QDir::separator() + "Trash.ini");
    if (QFile::exists(oldTrashDBPath)) {
        migrateTrashFromV0_9_0(oldTrashDBPath);
    }
}

/*!
 * \brief MainWindow::migrateNote
 * \param notePath
 */
void MainWindow::migrateNoteFromV0_9_0(const QString &notePath)
{
    QSettings notesIni(notePath, QSettings::IniFormat);
    QStringList dbKeys = notesIni.allKeys();
    QVector<NodeData> noteList;

    auto it = dbKeys.begin();
    for (; it < dbKeys.end() - 1; it += 3) {
        QString noteName = it->split(QStringLiteral("/"))[0];
        int id = noteName.split(QStringLiteral("_"))[1].toInt();
        NodeData newNote;
        newNote.setId(id);
        QString createdDateDB =
                notesIni.value(noteName + QStringLiteral("/dateCreated"), "Error").toString();
        newNote.setCreationDateTime(QDateTime::fromString(createdDateDB, Qt::ISODate));
        QString lastEditedDateDB =
                notesIni.value(noteName + QStringLiteral("/dateEdited"), "Error").toString();
        newNote.setLastModificationDateTime(QDateTime::fromString(lastEditedDateDB, Qt::ISODate));
        QString contentText =
                notesIni.value(noteName + QStringLiteral("/content"), "Error").toString();
        newNote.setContent(contentText);
        QString firstLine = NoteEditorLogic::getFirstLine(contentText);
        newNote.setFullTitle(firstLine);
        noteList.append(newNote);
    }

    if (!noteList.isEmpty()) {
        emit requestMigrateNotesFromV0_9_0(noteList);
    }

    QFile oldNoteDBFile(notePath);
    oldNoteDBFile.rename(QFileInfo(notePath).dir().path() + QDir::separator()
                         + QStringLiteral("oldNotes.ini"));
}

/*!
 * \brief MainWindow::migrateTrash
 * \param trashPath
 */
void MainWindow::migrateTrashFromV0_9_0(const QString &trashPath)
{
    QSettings trashIni(trashPath, QSettings::IniFormat);
    QStringList dbKeys = trashIni.allKeys();

    QVector<NodeData> noteList;

    auto it = dbKeys.begin();
    for (; it < dbKeys.end() - 1; it += 3) {
        QString noteName = it->split(QStringLiteral("/"))[0];
        int id = noteName.split(QStringLiteral("_"))[1].toInt();
        NodeData newNote;
        newNote.setId(id);
        QString createdDateDB =
                trashIni.value(noteName + QStringLiteral("/dateCreated"), "Error").toString();
        newNote.setCreationDateTime(QDateTime::fromString(createdDateDB, Qt::ISODate));
        QString lastEditedDateDB =
                trashIni.value(noteName + QStringLiteral("/dateEdited"), "Error").toString();
        newNote.setLastModificationDateTime(QDateTime::fromString(lastEditedDateDB, Qt::ISODate));
        QString contentText =
                trashIni.value(noteName + QStringLiteral("/content"), "Error").toString();
        newNote.setContent(contentText);
        QString firstLine = NoteEditorLogic::getFirstLine(contentText);
        newNote.setFullTitle(firstLine);
        noteList.append(newNote);
    }

    if (!noteList.isEmpty()) {
        emit requestMigrateTrashFromV0_9_0(noteList);
    }
    QFile oldTrashDBFile(trashPath);
    oldTrashDBFile.rename(QFileInfo(trashPath).dir().path() + QDir::separator()
                          + QStringLiteral("oldTrash.ini"));
}

/*!
 * \brief MainWindow::dropShadow
 * \param painter
 * \param type
 * \param side
 */
void MainWindow::dropShadow(QPainter &painter, ShadowType type, MainWindow::ShadowSide side)
{
    int resizedShadowWidth = m_shadowWidth > m_layoutMargin ? m_layoutMargin : m_shadowWidth;

    QRect mainRect = rect();

    QRect innerRect(m_layoutMargin, m_layoutMargin, mainRect.width() - 2 * resizedShadowWidth + 1,
                    mainRect.height() - 2 * resizedShadowWidth + 1);
    QRect outerRect(innerRect.x() - resizedShadowWidth, innerRect.y() - resizedShadowWidth,
                    innerRect.width() + 2 * resizedShadowWidth,
                    innerRect.height() + 2 * resizedShadowWidth);

    QPoint center;
    QPoint topLeft;
    QPoint bottomRight;
    QPoint shadowStart;
    QPoint shadowStop;
    QRadialGradient radialGradient;
    QLinearGradient linearGradient;

    switch (side) {
    case ShadowSide::Left:
        topLeft = QPoint(outerRect.left(), innerRect.top() + 1);
        bottomRight = QPoint(innerRect.left(), innerRect.bottom() - 1);
        shadowStart = QPoint(innerRect.left(), innerRect.top() + 1);
        shadowStop = QPoint(outerRect.left(), innerRect.top() + 1);
        break;
    case ShadowSide::Top:
        topLeft = QPoint(innerRect.left() + 1, outerRect.top());
        bottomRight = QPoint(innerRect.right() - 1, innerRect.top());
        shadowStart = QPoint(innerRect.left() + 1, innerRect.top());
        shadowStop = QPoint(innerRect.left() + 1, outerRect.top());
        break;
    case ShadowSide::Right:
        topLeft = QPoint(innerRect.right(), innerRect.top() + 1);
        bottomRight = QPoint(outerRect.right(), innerRect.bottom() - 1);
        shadowStart = QPoint(innerRect.right(), innerRect.top() + 1);
        shadowStop = QPoint(outerRect.right(), innerRect.top() + 1);
        break;
    case ShadowSide::Bottom:
        topLeft = QPoint(innerRect.left() + 1, innerRect.bottom());
        bottomRight = QPoint(innerRect.right() - 1, outerRect.bottom());
        shadowStart = QPoint(innerRect.left() + 1, innerRect.bottom());
        shadowStop = QPoint(innerRect.left() + 1, outerRect.bottom());
        break;
    case ShadowSide::TopLeft:
        topLeft = outerRect.topLeft();
        bottomRight = innerRect.topLeft();
        center = innerRect.topLeft();
        break;
    case ShadowSide::TopRight:
        topLeft = QPoint(innerRect.right(), outerRect.top());
        bottomRight = QPoint(outerRect.right(), innerRect.top());
        center = innerRect.topRight();
        break;
    case ShadowSide::BottomRight:
        topLeft = innerRect.bottomRight();
        bottomRight = outerRect.bottomRight();
        center = innerRect.bottomRight();
        break;
    case ShadowSide::BottomLeft:
        topLeft = QPoint(outerRect.left(), innerRect.bottom());
        bottomRight = QPoint(innerRect.left(), outerRect.bottom());
        center = innerRect.bottomLeft();
        break;
    }

    QRect zone(topLeft, bottomRight);
    radialGradient = QRadialGradient(center, resizedShadowWidth, center);

    linearGradient.setStart(shadowStart);
    linearGradient.setFinalStop(shadowStop);

    switch (type) {
    case ShadowType::Radial:
        fillRectWithGradient(painter, zone, radialGradient);
        break;
    case ShadowType::Linear:
        fillRectWithGradient(painter, zone, linearGradient);
        break;
    }
}

/*!
 * \brief MainWindow::fillRectWithGradient
 * \param painter
 * \param rect
 * \param gradient
 */
void MainWindow::fillRectWithGradient(QPainter &painter, QRect rect, QGradient &gradient)
{
    double variance = 0.2;
    double xMax = 1.10;
    double q = 70 / gaussianDist(0, 0, sqrt(variance));
    double nPt = 100.0;

    for (int i = 0; i <= nPt; i++) {
        double v = gaussianDist(i * xMax / nPt, 0, sqrt(variance));

        QColor c(168, 168, 168, int(q * v));
        gradient.setColorAt(i / nPt, c);
    }

    painter.fillRect(rect, gradient);
}

/*!
 * \brief MainWindow::gaussianDist
 * \param x
 * \param center
 * \param sigma
 * \return
 */
double MainWindow::gaussianDist(double x, const double center, double sigma) const
{
    return (1.0 / (2 * M_PI * pow(sigma, 2)) * exp(-pow(x - center, 2) / (2 * pow(sigma, 2))));
}

/*!
 * \brief MainWindow::mouseDoubleClickEvent
 * When the blank area at the top of window is double-clicked the window get maximized
 * \param event
 */
void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_useNativeWindowFrame || event->buttons() != Qt::LeftButton
        || !isTitleBar(event->pos().x(), event->pos().y())) {
        MainWindowBase::mouseDoubleClickEvent(event);
        return;
    }

#ifndef __APPLE__
    maximizeWindow();
#else
    maximizeWindowMac();
#endif
    event->accept();
}

/*!
 * \brief MainWindow::leaveEvent
 */
void MainWindow::leaveEvent(QEvent *)
{
    unsetCursor();
}

/*!
 * \brief MainWindow::changeEvent
 */
void MainWindow::changeEvent(QEvent *event)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (event->type() == QEvent::WindowStateChange && !m_useNativeWindowFrame) {
        if (isMaximized())
            setMargins(QMargins());
        else
            setMargins(QMargins(m_layoutMargin, m_layoutMargin, m_layoutMargin, m_layoutMargin));
    }
#endif
    MainWindowBase::changeEvent(event);
}

void MainWindow::setWindowButtonsVisible(bool isVisible)
{
#ifdef __APPLE__
    setStandardWindowButtonsMacVisibility(isVisible);
#else
    bool visible = !m_useNativeWindowFrame && isVisible;
    m_redCloseButton->setVisible(visible);
    m_yellowMinimizeButton->setVisible(visible);
    m_greenMaximizeButton->setVisible(visible);
#endif
}

/*!
 * \brief MainWindow::eventFilter
 * Mostly take care on the event happened on widget whose filter installed to the mainwindow
 * \param object
 * \param event
 * \return
 */
bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Enter: {
        if (qApp->applicationState() == Qt::ApplicationActive) {
#ifdef _WIN32
            if (object == m_redCloseButton) {
                m_redCloseButton->setIcon(
                        QIcon(QStringLiteral(":images/windows_close_hovered.png")));
            }

            if (object == m_yellowMinimizeButton) {
                if (windowState() == Qt::WindowFullScreen) {
                    m_yellowMinimizeButton->setIcon(
                            QIcon(QStringLiteral(":images/windows_de-maximize_hovered.png")));
                } else {
                    m_yellowMinimizeButton->setIcon(
                            QIcon(QStringLiteral(":images/windows_maximize_hovered.png")));
                }
            }

            if (object == m_greenMaximizeButton) {
                m_greenMaximizeButton->setIcon(
                        QIcon(QStringLiteral(":images/windows_minimize_hovered.png")));
            }
#else
            // When hovering one of the traffic light buttons (red, yellow, green),
            // set new icons to show their function
            if (object == m_redCloseButton || object == m_yellowMinimizeButton
                || object == m_greenMaximizeButton) {

                m_redCloseButton->setIcon(QIcon(QStringLiteral(":images/redHovered.png")));
                m_yellowMinimizeButton->setIcon(QIcon(QStringLiteral(":images/yellowHovered.png")));
                if (windowState() == Qt::WindowFullScreen) {
                    m_greenMaximizeButton->setIcon(
                            QIcon(QStringLiteral(":images/greenInHovered.png")));
                } else {
                    m_greenMaximizeButton->setIcon(
                            QIcon(QStringLiteral(":images/greenHovered.png")));
                }
            }
#endif
        }

        if (object == ui->frame) {
            ui->centralWidget->setCursor(Qt::ArrowCursor);
        }

        break;
    }
    case QEvent::Leave: {
        if (qApp->applicationState() == Qt::ApplicationActive) {
            // When not hovering, change back the icons of the traffic lights to their default icon
            if (object == m_redCloseButton || object == m_yellowMinimizeButton
                || object == m_greenMaximizeButton) {

#ifdef _WIN32
                m_redCloseButton->setIcon(
                        QIcon(QStringLiteral(":images/windows_close_regular.png")));
                m_greenMaximizeButton->setIcon(
                        QIcon(QStringLiteral(":images/windows_minimize_regular.png")));

                if (windowState() == Qt::WindowFullScreen) {
                    m_yellowMinimizeButton->setIcon(
                            QIcon(QStringLiteral(":images/windows_de-maximize_regular.png")));
                } else {
                    m_yellowMinimizeButton->setIcon(
                            QIcon(QStringLiteral(":images/windows_maximize_regular.png")));
                }
#else
                m_redCloseButton->setIcon(QIcon(QStringLiteral(":images/red.png")));
                m_yellowMinimizeButton->setIcon(QIcon(QStringLiteral(":images/yellow.png")));
                m_greenMaximizeButton->setIcon(QIcon(QStringLiteral(":images/green.png")));
#endif
            }
        }
        break;
    }
    case QEvent::ActivationChange: {
        if (m_editorSettingsWidget->isHidden()) {
            QApplication::setActiveWindow(
                    this); // TODO: The docs say this function is deprecated but it's the only one
                           // that works in returning the user input from m_editorSettingsWidget
                           // Qt::Popup
            //            m_textEdit->setFocus();
        }
        break;
    }
    case QEvent::WindowDeactivate: {
#if !defined(Q_OS_MACOS)
        if (object == m_editorSettingsWidget) {
            m_editorSettingsWidget->close();
        }
#endif
        emit mainWindowDeactivated();
        m_canMoveWindow = false;
        m_canStretchWindow = false;
        QApplication::restoreOverrideCursor();

#ifndef _WIN32
        m_redCloseButton->setIcon(QIcon(QStringLiteral(":images/unfocusedButton")));
        m_yellowMinimizeButton->setIcon(QIcon(QStringLiteral(":images/unfocusedButton")));
        m_greenMaximizeButton->setIcon(QIcon(QStringLiteral(":images/unfocusedButton")));
#endif
        break;
    }
    case QEvent::WindowActivate: {
#ifdef _WIN32
        m_redCloseButton->setIcon(QIcon(QStringLiteral(":images/windows_close_regular.png")));
        m_greenMaximizeButton->setIcon(
                QIcon(QStringLiteral(":images/windows_minimize_regular.png")));

        if (windowState() == Qt::WindowFullScreen) {
            m_yellowMinimizeButton->setIcon(
                    QIcon(QStringLiteral(":images/windows_de-maximize_regular.png")));
        } else {
            m_yellowMinimizeButton->setIcon(
                    QIcon(QStringLiteral(":images/windows_maximize_regular.png")));
        }
#else
        m_redCloseButton->setIcon(QIcon(QStringLiteral(":images/red.png")));
        m_yellowMinimizeButton->setIcon(QIcon(QStringLiteral(":images/yellow.png")));
        m_greenMaximizeButton->setIcon(QIcon(QStringLiteral(":images/green.png")));
#endif
        break;
    }
        //    case QEvent::FocusIn: {
        //        if (object == m_textEdit) {
        //            if (!m_isOperationRunning) {
        //                if (m_listModel->rowCount() == 0) {
        //                    if (!m_searchEdit->text().isEmpty()) {
        //                        m_listViewLogic->clearSearch(true);
        //                    } else {
        //                        createNewNote();
        //                    }
        //                }
        //            }
        //            m_listView->setCurrentRowActive(true);
        //            m_textEdit->setFocus();
        //        }
        //        break;
        //    }
    case QEvent::FocusOut: {
        break;
    }
    case QEvent::Show:
#if defined(UPDATE_CHECKER)
        if (object == &m_updater) {

            QRect rect = m_updater.geometry();
            QRect appRect = geometry();
            int titleBarHeight = 28;

            int x = int(appRect.x() + (appRect.width() - rect.width()) / 2.0);
            int y = int(appRect.y() + titleBarHeight + (appRect.height() - rect.height()) / 2.0);

            m_updater.setGeometry(QRect(x, y, rect.width(), rect.height()));
        }
#endif
        break;
    case QEvent::KeyPress: {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return && m_searchEdit->text().isEmpty()) {
            setFocusOnText();
        } else if (keyEvent->key() == Qt::Key_Return
                   && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
            setFocusOnText();
            return true;
        } else if (keyEvent->key() == Qt::Key_Escape && isFullScreen()) {
            // exit fullscreen
            fullscreenWindow();
        }
        break;
    }
    default:
        break;
    }

    return QObject::eventFilter(object, event);
}

/*!
 * \brief MainWindow::stayOnTop
 * \param checked
 */
void MainWindow::stayOnTop(bool checked)
{
    m_alwaysStayOnTop = checked;

#if defined(Q_OS_MACOS)
    setWindowAlwaysOnTopMac(checked);
    updateSelectedOptionsEditorSettings();
#endif
}

/*!
 * \brief MainWindow::moveCurrentNoteToTrash
 */
void MainWindow::moveCurrentNoteToTrash()
{
    m_noteEditorLogic->deleteCurrentNote();
    //    m_editorSettingsWidget->close();
}

/*!
 * \brief MainWindow::setUseNativeWindowFrame
 * \param useNativeWindowFrame
 */
void MainWindow::setUseNativeWindowFrame(bool useNativeWindowFrame)
{
    m_useNativeWindowFrame = useNativeWindowFrame;
    m_settingsDatabase->setValue(QStringLiteral("useNativeWindowFrame"), useNativeWindowFrame);

#ifndef __APPLE__
    m_greenMaximizeButton->setVisible(!useNativeWindowFrame);
    m_redCloseButton->setVisible(!useNativeWindowFrame);
    m_yellowMinimizeButton->setVisible(!useNativeWindowFrame);

    // Reset window flags to its initial state.
    Qt::WindowFlags flags = Qt::Window;

    if (!useNativeWindowFrame) {
        flags |= Qt::CustomizeWindowHint;
#  if defined(Q_OS_UNIX)
        flags |= Qt::FramelessWindowHint;
#  endif
    }

    setWindowFlags(flags);
#endif

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (useNativeWindowFrame || isMaximized()) {
        ui->centralWidget->layout()->setContentsMargins(QMargins());
    } else {
        QMargins margins(m_layoutMargin, m_layoutMargin, m_layoutMargin, m_layoutMargin);
        ui->centralWidget->layout()->setContentsMargins(margins);
    }
#endif

    setMainWindowVisibility(true);
}

void MainWindow::setHideToTray(bool enabled)
{
    m_hideToTray = enabled;
    m_settingsDatabase->setValue(QStringLiteral("hideToTray"), enabled);
}

/*!
 * \brief MainWindow::toggleStayOnTop
 */
void MainWindow::toggleStayOnTop()
{
    stayOnTop(!m_alwaysStayOnTop);
}

/*!
 * \brief MainWindow::onSearchEditReturnPressed
 */
void MainWindow::onSearchEditReturnPressed()
{
    if (m_searchEdit->text().isEmpty())
        return;

    //    QString searchText = m_searchEdit->text();
}

/*!
 * \brief MainWindow::setMargins
 * \param margins
 */
void MainWindow::setMargins(QMargins margins)
{
    if (m_useNativeWindowFrame)
        return;

    ui->centralWidget->layout()->setContentsMargins(margins);
    m_trafficLightLayout.setGeometry(QRect(4 + margins.left(), 4 + margins.top(), 56, 16));
}

bool MainWindow::isTitleBar(int x, int y) const
{
    if (m_useNativeWindowFrame)
        return false;

    // The width of the title bar is essentially the width of the main window.
    int titleBarWidth = width();
    int titleBarHeight = ui->globalSettingsButton->height();

    int adjustedX = x;
    int adjustedY = y;

    if (!isMaximized() && !isFullScreen()) {
        titleBarWidth -= m_layoutMargin * 2;
        adjustedX -= m_layoutMargin;
        adjustedY -= m_layoutMargin;
    }

    return (adjustedX >= 0 && adjustedX <= titleBarWidth && adjustedY >= 0
            && adjustedY <= titleBarHeight);
}
