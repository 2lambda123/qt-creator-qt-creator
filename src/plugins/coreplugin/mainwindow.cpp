// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "mainwindow.h"

#include "actionmanager/actioncontainer.h"
#include "actionmanager/actionmanager.h"
#include "actionmanager/command.h"
#include "coreicons.h"
#include "coreplugintr.h"
#include "dialogs/externaltoolconfig.h"
#include "dialogs/shortcutsettings.h"
#include "documentmanager.h"
#include "editormanager/documentmodel_p.h"
#include "editormanager/editormanager.h"
#include "editormanager/editormanager_p.h"
#include "editormanager/ieditor.h"
#include "editormanager/ieditorfactory.h"
#include "editormanager/systemeditor.h"
#include "externaltoolmanager.h"
#include "fancytabwidget.h"
#include "fileutils.h"
#include "find/basetextfind.h"
#include "findplaceholder.h"
#include "helpmanager.h"
#include "icore.h"
#include "idocumentfactory.h"
#include "inavigationwidgetfactory.h"
#include "iwizardfactory.h"
#include "jsexpander.h"
#include "loggingviewer.h"
#include "manhattanstyle.h"
#include "messagemanager.h"
#include "mimetypesettings.h"
#include "modemanager.h"
#include "navigationwidget.h"
#include "outputpanemanager.h"
#include "plugindialog.h"
#include "progressmanager/progressmanager_p.h"
#include "progressmanager/progressview.h"
#include "rightpane.h"
#include "statusbarmanager.h"
#include "systemsettings.h"
#include "vcsmanager.h"
#include "versiondialog.h"
#include "windowsupport.h"

#include <extensionsystem/pluginmanager.h>

#include <utils/algorithm.h>
#include <utils/fsengine/fileiconprovider.h>
#include <utils/fsengine/fsengine.h>
#include <utils/historycompleter.h>
#include <utils/hostosinfo.h>
#include <utils/mimeutils.h>
#include <utils/proxyaction.h>
#include <utils/qtcassert.h>
#include <utils/stringutils.h>
#include <utils/stylehelper.h>
#include <utils/theme/theme.h>
#include <utils/touchbar/touchbar.h>
#include <utils/checkablemessagebox.h>
#include <utils/terminalcommand.h>
#include <utils/utilsicons.h>

#include <nanotrace/nanotrace.h>

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPrinter>
#include <QStatusBar>
#include <QStyleFactory>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QVersionNumber>

#ifdef Q_OS_LINUX
#include <malloc.h>
#endif

using namespace ExtensionSystem;
using namespace Utils;

namespace Core {
namespace Internal {

const char settingsGroup[] = "MainWindow";
const char colorKey[] = "Color";
const char windowGeometryKey[] = "WindowGeometry";
const char windowStateKey[] = "WindowState";
const char modeSelectorLayoutKey[] = "ModeSelectorLayout";
const char menubarVisibleKey[] = "MenubarVisible";

static bool hideToolsMenu()
{
    return Core::ICore::settings()->value(Constants::SETTINGS_MENU_HIDE_TOOLS, false).toBool();
}

enum { debugMainWindow = 0 };


class MainWindowPrivate : public QObject
{
public:
    explicit MainWindowPrivate(MainWindow *mainWindow)
        : q(mainWindow)
    {}

    ~MainWindowPrivate();

    void init();

    static void openFile();
    static IDocument *openFiles(const FilePaths &filePaths, ICore::OpenFilesFlags flags,
                                const FilePath &workingDirectory = {});
    void aboutToShowRecentFiles();

    static void setFocusToEditor();
    void aboutQtCreator();
    void aboutPlugins();
    void changeLog();
    void contact();
    void updateFocusWidget(QWidget *old, QWidget *now);
    NavigationWidget *navigationWidget(Side side) const;
    void setSidebarVisible(bool visible, Side side);
    void destroyVersionDialog();
    void openDroppedFiles(const QList<Utils::DropSupport::FileSpec> &files);
    void restoreWindowState();

    void openFileFromDevice();

    void updateContextObject(const QList<IContext *> &context);
    void updateContext();

    void registerDefaultContainers();
    void registerDefaultActions();
    void registerModeSelectorStyleActions();

    void readSettings();
    void saveWindowSettings();

    void updateModeSelectorStyleMenu();

    MainWindow *q = nullptr;
    ICore *m_coreImpl = nullptr;
    QTimer m_trimTimer;
    QStringList m_aboutInformation;
    Context m_highPrioAdditionalContexts;
    Context m_lowPrioAdditionalContexts{Constants::C_GLOBAL};
    mutable QPrinter *m_printer = nullptr;
    WindowSupport *m_windowSupport = nullptr;
    EditorManager *m_editorManager = nullptr;
    ExternalToolManager *m_externalToolManager = nullptr;
    MessageManager *m_messageManager = nullptr;
    ProgressManagerPrivate *m_progressManager = nullptr;
    JsExpander *m_jsExpander = nullptr;
    VcsManager *m_vcsManager = nullptr;
    ModeManager *m_modeManager = nullptr;
    FancyTabWidget *m_modeStack = nullptr;
    NavigationWidget *m_leftNavigationWidget = nullptr;
    NavigationWidget *m_rightNavigationWidget = nullptr;
    RightPaneWidget *m_rightPaneWidget = nullptr;
    VersionDialog *m_versionDialog = nullptr;

    QList<IContext *> m_activeContext;

    std::unordered_map<QWidget *, IContext *> m_contextWidgets;

    ShortcutSettings *m_shortcutSettings = nullptr;
    ToolSettings *m_toolSettings = nullptr;
    MimeTypeSettings *m_mimeTypeSettings = nullptr;
    SystemEditor *m_systemEditor = nullptr;

    // actions
    QAction *m_focusToEditor = nullptr;
    QAction *m_newAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_openWithAction = nullptr;
    QAction *m_openFromDeviceAction = nullptr;
    QAction *m_saveAllAction = nullptr;
    QAction *m_exitAction = nullptr;
    QAction *m_optionsAction = nullptr;
    QAction *m_loggerAction = nullptr;
    QAction *m_toggleLeftSideBarAction = nullptr;
    QAction *m_toggleRightSideBarAction = nullptr;
    QAction *m_toggleMenubarAction = nullptr;
    QAction *m_cycleModeSelectorStyleAction = nullptr;
    QAction *m_setModeSelectorStyleIconsAndTextAction = nullptr;
    QAction *m_setModeSelectorStyleHiddenAction = nullptr;
    QAction *m_setModeSelectorStyleIconsOnlyAction = nullptr;
    QAction *m_themeAction = nullptr;

    QToolButton *m_toggleLeftSideBarButton = nullptr;
    QToolButton *m_toggleRightSideBarButton = nullptr;
    QColor m_overrideColor;
    QList<std::function<bool()>> m_preCloseListeners;
};

void MainWindowPrivate::init()
{
    m_coreImpl = new ICore(q);
    m_progressManager = new ProgressManagerPrivate;
    m_jsExpander = JsExpander::createGlobalJsExpander();
    m_vcsManager = new VcsManager;
    m_modeStack = new FancyTabWidget(q);
    m_shortcutSettings = new ShortcutSettings;
    m_toolSettings = new ToolSettings;
    m_mimeTypeSettings = new MimeTypeSettings;
    m_systemEditor = new SystemEditor;
    m_toggleLeftSideBarButton = new QToolButton;
    m_toggleRightSideBarButton = new QToolButton;

    (void) new DocumentManager(q);

    HistoryCompleter::setSettings(PluginManager::settings());

    if (HostOsInfo::isLinuxHost())
        QApplication::setWindowIcon(Icons::QTCREATORLOGO_BIG.icon());
    QString baseName = QApplication::style()->objectName();
    // Sometimes we get the standard windows 95 style as a fallback
    if (HostOsInfo::isAnyUnixHost() && !HostOsInfo::isMacHost()
            && baseName == QLatin1String("windows")) {
        baseName = QLatin1String("fusion");
    }

    // if the user has specified as base style in the theme settings,
    // prefer that
    const QStringList available = QStyleFactory::keys();
    const QStringList styles = Utils::creatorTheme()->preferredStyles();
    for (const QString &s : styles) {
        if (available.contains(s, Qt::CaseInsensitive)) {
            baseName = s;
            break;
        }
    }

    QApplication::setStyle(new ManhattanStyle(baseName));

    m_modeManager = new ModeManager(q, m_modeStack);
    connect(m_modeStack, &FancyTabWidget::topAreaClicked, this, [](Qt::MouseButton, Qt::KeyboardModifiers modifiers) {
        if (modifiers & Qt::ShiftModifier) {
            QColor color = QColorDialog::getColor(StyleHelper::requestedBaseColor(), ICore::dialogParent());
            if (color.isValid())
                StyleHelper::setBaseColor(color);
        }
    });

    registerDefaultContainers();
    registerDefaultActions();

    m_leftNavigationWidget = new NavigationWidget(m_toggleLeftSideBarAction, Side::Left);
    m_rightNavigationWidget = new NavigationWidget(m_toggleRightSideBarAction, Side::Right);
    m_rightPaneWidget = new RightPaneWidget();

    m_messageManager = new MessageManager;
    m_editorManager = new EditorManager(this);
    m_externalToolManager = new ExternalToolManager();

    m_progressManager->progressView()->setParent(q);

    connect(qApp, &QApplication::focusChanged, this, &MainWindowPrivate::updateFocusWidget);

    // Add small Toolbuttons for toggling the navigation widgets
    StatusBarManager::addStatusBarWidget(m_toggleLeftSideBarButton, StatusBarManager::First);
    int childsCount = q->statusBar()->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly).count();
    q->statusBar()->insertPermanentWidget(childsCount - 1, m_toggleRightSideBarButton); // before QSizeGrip

//    setUnifiedTitleAndToolBarOnMac(true);
    //if (HostOsInfo::isAnyUnixHost())
        //signal(SIGINT, handleSigInt);

    q->statusBar()->setProperty("p_styled", true);

    auto dropSupport = new DropSupport(q, [](QDropEvent *event, DropSupport *) {
        return event->source() == nullptr; // only accept drops from the "outside" (e.g. file manager)
    });
    connect(dropSupport, &DropSupport::filesDropped,
            this, &MainWindowPrivate::openDroppedFiles);

    if (HostOsInfo::isLinuxHost()) {
        m_trimTimer.setSingleShot(true);
        m_trimTimer.setInterval(60000);
        // glibc may not actually free memory in free().
#ifdef Q_OS_LINUX
        connect(&m_trimTimer, &QTimer::timeout, this, [] { malloc_trim(0); });
#endif
    }
}

MainWindow::MainWindow()
    : d(new MainWindowPrivate(this))
{
    d->init(); // Separation needed for now as the call triggers other MainWindow calls.

    setWindowTitle(QGuiApplication::applicationDisplayName());
    setDockNestingEnabled(true);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

    setCentralWidget(d->m_modeStack);
}

NavigationWidget *MainWindowPrivate::navigationWidget(Side side) const
{
    return side == Side::Left ? m_leftNavigationWidget : m_rightNavigationWidget;
}

void MainWindowPrivate::setSidebarVisible(bool visible, Side side)
{
    if (NavigationWidgetPlaceHolder::current(side))
        navigationWidget(side)->setShown(visible);
}

void MainWindow::setOverrideColor(const QColor &color)
{
    d->m_overrideColor = color;
}

QStringList MainWindow::additionalAboutInformation() const
{
    return d->m_aboutInformation;
}

void MainWindow::clearAboutInformation()
{
    d->m_aboutInformation.clear();
}

void MainWindow::appendAboutInformation(const QString &line)
{
    d->m_aboutInformation.append(line);
}

void MainWindow::addPreCloseListener(const std::function<bool ()> &listener)
{
    d->m_preCloseListeners.append(listener);
}

MainWindow::~MainWindow()
{
    delete d;
}

MainWindowPrivate::~MainWindowPrivate()
{
    // explicitly delete window support, because that calls methods from ICore that call methods
    // from mainwindow, so mainwindow still needs to be alive
    delete m_windowSupport;
    m_windowSupport = nullptr;

    delete m_externalToolManager;
    m_externalToolManager = nullptr;
    delete m_messageManager;
    m_messageManager = nullptr;
    delete m_shortcutSettings;
    m_shortcutSettings = nullptr;
    delete m_toolSettings;
    m_toolSettings = nullptr;
    delete m_mimeTypeSettings;
    m_mimeTypeSettings = nullptr;
    delete m_systemEditor;
    m_systemEditor = nullptr;
    delete m_printer;
    m_printer = nullptr;
    delete m_vcsManager;
    m_vcsManager = nullptr;
    //we need to delete editormanager and statusbarmanager explicitly before the end of the destructor,
    //because they might trigger stuff that tries to access data from editorwindow, like removeContextWidget

    // All modes are now gone
    OutputPaneManager::destroy();

    delete m_leftNavigationWidget;
    delete m_rightNavigationWidget;
    m_leftNavigationWidget = nullptr;
    m_rightNavigationWidget = nullptr;

    delete m_editorManager;
    m_editorManager = nullptr;
    delete m_progressManager;
    m_progressManager = nullptr;

    delete m_coreImpl;
    m_coreImpl = nullptr;

    delete m_rightPaneWidget;
    m_rightPaneWidget = nullptr;

    delete m_modeManager;
    m_modeManager = nullptr;

    delete m_jsExpander;
    m_jsExpander = nullptr;
}

void MainWindow::init()
{
    d->m_progressManager->init(); // needs the status bar manager
    MessageManager::init();
    OutputPaneManager::create();
}

void MainWindow::extensionsInitialized()
{
    EditorManagerPrivate::extensionsInitialized();
    MimeTypeSettings::restoreSettings();
    d->m_windowSupport = new WindowSupport(this, Context("Core.MainWindow"));
    d->m_windowSupport->setCloseActionEnabled(false);
    OutputPaneManager::initialize();
    VcsManager::extensionsInitialized();
    d->m_leftNavigationWidget->setFactories(INavigationWidgetFactory::allNavigationFactories());
    d->m_rightNavigationWidget->setFactories(INavigationWidgetFactory::allNavigationFactories());

    ModeManager::extensionsInitialized();

    d->readSettings();
    d->updateContext();

    emit d->m_coreImpl->coreAboutToOpen();
    // Delay restoreWindowState, since it is overridden by LayoutRequest event
    QMetaObject::invokeMethod(d, &MainWindowPrivate::restoreWindowState, Qt::QueuedConnection);
    QMetaObject::invokeMethod(d->m_coreImpl, &ICore::coreOpened, Qt::QueuedConnection);
}

static void setRestart(bool restart)
{
    qApp->setProperty("restart", restart);
}

void MainWindow::restart()
{
    setRestart(true);
    exit();
}

void MainWindow::restartTrimmer()
{
    if (HostOsInfo::isLinuxHost() && !d->m_trimTimer.isActive())
        d->m_trimTimer.start();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const auto cancelClose = [event] {
        event->ignore();
        setRestart(false);
    };

    // work around QTBUG-43344
    static bool alreadyClosed = false;
    if (alreadyClosed) {
        event->accept();
        return;
    }

    if (systemSettings().askBeforeExit()
        && (QMessageBox::question(this,
                                  Tr::tr("Exit %1?").arg(QGuiApplication::applicationDisplayName()),
                                  Tr::tr("Exit %1?").arg(QGuiApplication::applicationDisplayName()),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            == QMessageBox::No)) {
        event->ignore();
        return;
    }

    ICore::saveSettings(ICore::MainWindowClosing);

    // Save opened files
    if (!DocumentManager::saveAllModifiedDocuments()) {
        cancelClose();
        return;
    }

    const QList<std::function<bool()>> listeners = d->m_preCloseListeners;
    for (const std::function<bool()> &listener : listeners) {
        if (!listener()) {
            cancelClose();
            return;
        }
    }

    emit d->m_coreImpl->coreAboutToClose();

    d->saveWindowSettings();

    d->m_leftNavigationWidget->closeSubWidgets();
    d->m_rightNavigationWidget->closeSubWidgets();

    event->accept();
    alreadyClosed = true;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    restartTrimmer();
    AppMainWindow::keyPressEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    restartTrimmer();
    AppMainWindow::mousePressEvent(event);
}

void MainWindowPrivate::openDroppedFiles(const QList<DropSupport::FileSpec> &files)
{
    q->raiseWindow();
    const FilePaths filePaths = Utils::transform(files, &DropSupport::FileSpec::filePath);
    q->openFiles(filePaths, ICore::SwitchMode);
}

IContext *MainWindow::currentContextObject() const
{
    return d->m_activeContext.isEmpty() ? nullptr : d->m_activeContext.first();
}

QStatusBar *MainWindow::statusBar() const
{
    return d->m_modeStack->statusBar();
}

InfoBar *MainWindow::infoBar() const
{
    return d->m_modeStack->infoBar();
}

void MainWindowPrivate::registerDefaultContainers()
{
    ActionContainer *menubar = ActionManager::createMenuBar(Constants::MENU_BAR);

    if (!HostOsInfo::isMacHost()) // System menu bar on Mac
        q->setMenuBar(menubar->menuBar());
    menubar->appendGroup(Constants::G_FILE);
    menubar->appendGroup(Constants::G_EDIT);
    menubar->appendGroup(Constants::G_VIEW);
    menubar->appendGroup(Constants::G_TOOLS);
    menubar->appendGroup(Constants::G_WINDOW);
    menubar->appendGroup(Constants::G_HELP);

    // File Menu
    ActionContainer *filemenu = ActionManager::createMenu(Constants::M_FILE);
    menubar->addMenu(filemenu, Constants::G_FILE);
    filemenu->menu()->setTitle(Tr::tr("&File"));
    filemenu->appendGroup(Constants::G_FILE_NEW);
    filemenu->appendGroup(Constants::G_FILE_OPEN);
    filemenu->appendGroup(Constants::G_FILE_SESSION);
    filemenu->appendGroup(Constants::G_FILE_PROJECT);
    filemenu->appendGroup(Constants::G_FILE_SAVE);
    filemenu->appendGroup(Constants::G_FILE_EXPORT);
    filemenu->appendGroup(Constants::G_FILE_CLOSE);
    filemenu->appendGroup(Constants::G_FILE_PRINT);
    filemenu->appendGroup(Constants::G_FILE_OTHER);
    connect(filemenu->menu(), &QMenu::aboutToShow, this, &MainWindowPrivate::aboutToShowRecentFiles);


    // Edit Menu
    ActionContainer *medit = ActionManager::createMenu(Constants::M_EDIT);
    menubar->addMenu(medit, Constants::G_EDIT);
    medit->menu()->setTitle(Tr::tr("&Edit"));
    medit->appendGroup(Constants::G_EDIT_UNDOREDO);
    medit->appendGroup(Constants::G_EDIT_COPYPASTE);
    medit->appendGroup(Constants::G_EDIT_SELECTALL);
    medit->appendGroup(Constants::G_EDIT_ADVANCED);
    medit->appendGroup(Constants::G_EDIT_FIND);
    medit->appendGroup(Constants::G_EDIT_OTHER);

    ActionContainer *mview = ActionManager::createMenu(Constants::M_VIEW);
    menubar->addMenu(mview, Constants::G_VIEW);
    mview->menu()->setTitle(Tr::tr("&View"));
    mview->appendGroup(Constants::G_VIEW_VIEWS);
    mview->appendGroup(Constants::G_VIEW_PANES);

    // Tools Menu
    ActionContainer *ac = ActionManager::createMenu(Constants::M_TOOLS);
    ac->setParent(this);
    if (!hideToolsMenu())
        menubar->addMenu(ac, Constants::G_TOOLS);

    ac->menu()->setTitle(Tr::tr("&Tools"));

    // Window Menu
    ActionContainer *mwindow = ActionManager::createMenu(Constants::M_WINDOW);
    menubar->addMenu(mwindow, Constants::G_WINDOW);
    mwindow->menu()->setTitle(Tr::tr("&Window"));
    mwindow->appendGroup(Constants::G_WINDOW_SIZE);
    mwindow->appendGroup(Constants::G_WINDOW_SPLIT);
    mwindow->appendGroup(Constants::G_WINDOW_NAVIGATE);
    mwindow->appendGroup(Constants::G_WINDOW_LIST);
    mwindow->appendGroup(Constants::G_WINDOW_OTHER);

    // Help Menu
    ac = ActionManager::createMenu(Constants::M_HELP);
    menubar->addMenu(ac, Constants::G_HELP);
    ac->menu()->setTitle(Tr::tr("&Help"));
    Theme::setHelpMenu(ac->menu());
    ac->appendGroup(Constants::G_HELP_HELP);
    ac->appendGroup(Constants::G_HELP_SUPPORT);
    ac->appendGroup(Constants::G_HELP_ABOUT);
    ac->appendGroup(Constants::G_HELP_UPDATES);

    // macOS touch bar
    ac = ActionManager::createTouchBar(Constants::TOUCH_BAR,
                                       QIcon(),
                                       "Main TouchBar" /*never visible*/);
    ac->appendGroup(Constants::G_TOUCHBAR_HELP);
    ac->appendGroup(Constants::G_TOUCHBAR_NAVIGATION);
    ac->appendGroup(Constants::G_TOUCHBAR_EDITOR);
    ac->appendGroup(Constants::G_TOUCHBAR_OTHER);
    ac->touchBar()->setApplicationTouchBar();
}

static QMenuBar *globalMenuBar()
{
    return ActionManager::actionContainer(Constants::MENU_BAR)->menuBar();
}

void MainWindowPrivate::registerDefaultActions()
{
    ActionContainer *mfile = ActionManager::actionContainer(Constants::M_FILE);
    ActionContainer *medit = ActionManager::actionContainer(Constants::M_EDIT);
    ActionContainer *mview = ActionManager::actionContainer(Constants::M_VIEW);
    ActionContainer *mtools = ActionManager::actionContainer(Constants::M_TOOLS);
    ActionContainer *mwindow = ActionManager::actionContainer(Constants::M_WINDOW);
    ActionContainer *mhelp = ActionManager::actionContainer(Constants::M_HELP);

    // File menu separators
    mfile->addSeparator(Constants::G_FILE_SAVE);
    mfile->addSeparator(Constants::G_FILE_EXPORT);
    mfile->addSeparator(Constants::G_FILE_PRINT);
    mfile->addSeparator(Constants::G_FILE_CLOSE);
    mfile->addSeparator(Constants::G_FILE_OTHER);
    // Edit menu separators
    medit->addSeparator(Constants::G_EDIT_COPYPASTE);
    medit->addSeparator(Constants::G_EDIT_SELECTALL);
    medit->addSeparator(Constants::G_EDIT_FIND);
    medit->addSeparator(Constants::G_EDIT_ADVANCED);

    // Return to editor shortcut: Note this requires Qt to fix up
    // handling of shortcut overrides in menus, item views, combos....
    m_focusToEditor = new QAction(Tr::tr("Return to Editor"), this);
    Command *cmd = ActionManager::registerAction(m_focusToEditor, Constants::S_RETURNTOEDITOR);
    cmd->setDefaultKeySequence(QKeySequence(Qt::Key_Escape));
    connect(m_focusToEditor, &QAction::triggered, this, &MainWindowPrivate::setFocusToEditor);

    // New File Action
    QIcon icon = Icon::fromTheme("document-new");

    m_newAction = new QAction(icon, Tr::tr("&New Project..."), this);
    cmd = ActionManager::registerAction(m_newAction, Constants::NEW);
    cmd->setDefaultKeySequence(QKeySequence("Ctrl+Shift+N"));
    mfile->addAction(cmd, Constants::G_FILE_NEW);
    connect(m_newAction, &QAction::triggered, this, [] {
        if (!ICore::isNewItemDialogRunning()) {
            ICore::showNewItemDialog(
                Tr::tr("New Project", "Title of dialog"),
                Utils::filtered(Core::IWizardFactory::allWizardFactories(),
                                Utils::equal(&Core::IWizardFactory::kind,
                                             Core::IWizardFactory::ProjectWizard)),
                FilePath());
        } else {
            ICore::raiseWindow(ICore::newItemDialog());
        }
    });

    auto action = new QAction(icon, Tr::tr("New File..."), this);
    cmd = ActionManager::registerAction(action, Constants::NEW_FILE);
    cmd->setDefaultKeySequence(QKeySequence::New);
    mfile->addAction(cmd, Constants::G_FILE_NEW);
    connect(action, &QAction::triggered, this, [] {
        if (!ICore::isNewItemDialogRunning()) {
            ICore::showNewItemDialog(Tr::tr("New File", "Title of dialog"),
                                     Utils::filtered(Core::IWizardFactory::allWizardFactories(),
                                                     Utils::equal(&Core::IWizardFactory::kind,
                                                                  Core::IWizardFactory::FileWizard)),
                                     FilePath());
        } else {
            ICore::raiseWindow(ICore::newItemDialog());
        }
    });

    // Open Action
    icon = Icon::fromTheme("document-open");
    m_openAction = new QAction(icon, Tr::tr("&Open File or Project..."), this);
    cmd = ActionManager::registerAction(m_openAction, Constants::OPEN);
    cmd->setDefaultKeySequence(QKeySequence::Open);
    mfile->addAction(cmd, Constants::G_FILE_OPEN);
    connect(m_openAction, &QAction::triggered, this, &MainWindowPrivate::openFile);

    // Open With Action
    m_openWithAction = new QAction(Tr::tr("Open File &With..."), this);
    cmd = ActionManager::registerAction(m_openWithAction, Constants::OPEN_WITH);
    mfile->addAction(cmd, Constants::G_FILE_OPEN);
    connect(m_openWithAction, &QAction::triggered, this, &MainWindow::openFileWith);

    if (FSEngine::isAvailable()) {
        // Open From Device Action
        m_openFromDeviceAction = new QAction(Tr::tr("Open From Device..."), this);
        cmd = ActionManager::registerAction(m_openFromDeviceAction, Constants::OPEN_FROM_DEVICE);
        mfile->addAction(cmd, Constants::G_FILE_OPEN);
        connect(m_openFromDeviceAction, &QAction::triggered, this, &MainWindowPrivate::openFileFromDevice);
    }

    // File->Recent Files Menu
    ActionContainer *ac = ActionManager::createMenu(Constants::M_FILE_RECENTFILES);
    mfile->addMenu(ac, Constants::G_FILE_OPEN);
    ac->menu()->setTitle(Tr::tr("Recent &Files"));
    ac->setOnAllDisabledBehavior(ActionContainer::Show);

    // Save Action
    icon = Icon::fromTheme("document-save");
    QAction *tmpaction = new QAction(icon, Tr::tr("&Save"), this);
    tmpaction->setEnabled(false);
    cmd = ActionManager::registerAction(tmpaction, Constants::SAVE);
    cmd->setDefaultKeySequence(QKeySequence::Save);
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDescription(Tr::tr("Save"));
    mfile->addAction(cmd, Constants::G_FILE_SAVE);

    // Save As Action
    icon = Icon::fromTheme("document-save-as");
    tmpaction = new QAction(icon, Tr::tr("Save &As..."), this);
    tmpaction->setEnabled(false);
    cmd = ActionManager::registerAction(tmpaction, Constants::SAVEAS);
    cmd->setDefaultKeySequence(QKeySequence(useMacShortcuts ? Tr::tr("Ctrl+Shift+S") : QString()));
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDescription(Tr::tr("Save As..."));
    mfile->addAction(cmd, Constants::G_FILE_SAVE);

    // SaveAll Action
    DocumentManager::registerSaveAllAction();

    // Print Action
    icon = Icon::fromTheme("document-print");
    tmpaction = new QAction(icon, Tr::tr("&Print..."), this);
    tmpaction->setEnabled(false);
    cmd = ActionManager::registerAction(tmpaction, Constants::PRINT);
    cmd->setDefaultKeySequence(QKeySequence::Print);
    mfile->addAction(cmd, Constants::G_FILE_PRINT);

    // Exit Action
    icon = Icon::fromTheme("application-exit");
    m_exitAction = new QAction(icon, Tr::tr("E&xit"), this);
    m_exitAction->setMenuRole(QAction::QuitRole);
    cmd = ActionManager::registerAction(m_exitAction, Constants::EXIT);
    cmd->setDefaultKeySequence(QKeySequence(Tr::tr("Ctrl+Q")));
    mfile->addAction(cmd, Constants::G_FILE_OTHER);
    connect(m_exitAction, &QAction::triggered, q, &MainWindow::exit);

    // Undo Action
    icon = Icon::fromTheme("edit-undo");
    tmpaction = new QAction(icon, Tr::tr("&Undo"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::UNDO);
    cmd->setDefaultKeySequence(QKeySequence::Undo);
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDescription(Tr::tr("Undo"));
    medit->addAction(cmd, Constants::G_EDIT_UNDOREDO);
    tmpaction->setEnabled(false);

    // Redo Action
    icon = Icon::fromTheme("edit-redo");
    tmpaction = new QAction(icon, Tr::tr("&Redo"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::REDO);
    cmd->setDefaultKeySequence(QKeySequence::Redo);
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDescription(Tr::tr("Redo"));
    medit->addAction(cmd, Constants::G_EDIT_UNDOREDO);
    tmpaction->setEnabled(false);

    // Cut Action
    icon = Icon::fromTheme("edit-cut");
    tmpaction = new QAction(icon, Tr::tr("Cu&t"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::CUT);
    cmd->setDefaultKeySequence(QKeySequence::Cut);
    medit->addAction(cmd, Constants::G_EDIT_COPYPASTE);
    tmpaction->setEnabled(false);

    // Copy Action
    icon = Icon::fromTheme("edit-copy");
    tmpaction = new QAction(icon, Tr::tr("&Copy"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::COPY);
    cmd->setDefaultKeySequence(QKeySequence::Copy);
    medit->addAction(cmd, Constants::G_EDIT_COPYPASTE);
    tmpaction->setEnabled(false);

    // Paste Action
    icon = Icon::fromTheme("edit-paste");
    tmpaction = new QAction(icon, Tr::tr("&Paste"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::PASTE);
    cmd->setDefaultKeySequence(QKeySequence::Paste);
    medit->addAction(cmd, Constants::G_EDIT_COPYPASTE);
    tmpaction->setEnabled(false);

    // Select All
    icon = Icon::fromTheme("edit-select-all");
    tmpaction = new QAction(icon, Tr::tr("Select &All"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::SELECTALL);
    cmd->setDefaultKeySequence(QKeySequence::SelectAll);
    medit->addAction(cmd, Constants::G_EDIT_SELECTALL);
    tmpaction->setEnabled(false);

    // Goto Action
    icon = Icon::fromTheme("go-jump");
    tmpaction = new QAction(icon, Tr::tr("&Go to Line..."), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::GOTO);
    cmd->setDefaultKeySequence(QKeySequence(Tr::tr("Ctrl+L")));
    medit->addAction(cmd, Constants::G_EDIT_OTHER);
    tmpaction->setEnabled(false);

    // Zoom In Action
    icon = Icon::fromTheme("zoom-in");
    tmpaction = new QAction(icon, Tr::tr("Zoom In"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::ZOOM_IN);
    cmd->setDefaultKeySequence(QKeySequence(Tr::tr("Ctrl++")));
    tmpaction->setEnabled(false);

    // Zoom Out Action
    icon = Icon::fromTheme("zoom-out");
    tmpaction = new QAction(icon, Tr::tr("Zoom Out"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::ZOOM_OUT);
    if (useMacShortcuts)
        cmd->setDefaultKeySequences({QKeySequence(Tr::tr("Ctrl+-")), QKeySequence(Tr::tr("Ctrl+Shift+-"))});
    else
        cmd->setDefaultKeySequence(QKeySequence(Tr::tr("Ctrl+-")));
    tmpaction->setEnabled(false);

    // Zoom Reset Action
    icon = Icon::fromTheme("zoom-original");
    tmpaction = new QAction(icon, Tr::tr("Original Size"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::ZOOM_RESET);
    cmd->setDefaultKeySequence(QKeySequence(Core::useMacShortcuts ? Tr::tr("Meta+0") : Tr::tr("Ctrl+0")));
    tmpaction->setEnabled(false);

    // Debug Qt Creator menu
    mtools->appendGroup(Constants::G_TOOLS_DEBUG);
    ActionContainer *mtoolsdebug = ActionManager::createMenu(Constants::M_TOOLS_DEBUG);
    mtoolsdebug->menu()->setTitle(Tr::tr("Debug %1").arg(QGuiApplication::applicationDisplayName()));
    mtools->addMenu(mtoolsdebug, Constants::G_TOOLS_DEBUG);

    m_loggerAction = new QAction(Tr::tr("Show Logs..."), this);
    cmd = ActionManager::registerAction(m_loggerAction, Constants::LOGGER);
    mtoolsdebug->addAction(cmd);
    connect(m_loggerAction, &QAction::triggered, this, [] { LoggingViewer::showLoggingView(); });

    // Options Action
    medit->appendGroup(Constants::G_EDIT_PREFERENCES);
    medit->addSeparator(Constants::G_EDIT_PREFERENCES);

    m_optionsAction = new QAction(Tr::tr("Pr&eferences..."), this);
    m_optionsAction->setMenuRole(QAction::PreferencesRole);
    cmd = ActionManager::registerAction(m_optionsAction, Constants::OPTIONS);
    cmd->setDefaultKeySequence(QKeySequence::Preferences);
    medit->addAction(cmd, Constants::G_EDIT_PREFERENCES);
    connect(m_optionsAction, &QAction::triggered, this, [] { ICore::showOptionsDialog(Id()); });

    mwindow->addSeparator(Constants::G_WINDOW_LIST);

    if (useMacShortcuts) {
        // Minimize Action
        QAction *minimizeAction = new QAction(Tr::tr("Minimize"), this);
        minimizeAction->setEnabled(false); // actual implementation in WindowSupport
        cmd = ActionManager::registerAction(minimizeAction, Constants::MINIMIZE_WINDOW);
        cmd->setDefaultKeySequence(QKeySequence(Tr::tr("Ctrl+M")));
        mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);

        // Zoom Action
        QAction *zoomAction = new QAction(Tr::tr("Zoom"), this);
        zoomAction->setEnabled(false); // actual implementation in WindowSupport
        cmd = ActionManager::registerAction(zoomAction, Constants::ZOOM_WINDOW);
        mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);
    }

    // Full Screen Action
    QAction *toggleFullScreenAction = new QAction(Tr::tr("Full Screen"), this);
    toggleFullScreenAction->setCheckable(!HostOsInfo::isMacHost());
    toggleFullScreenAction->setEnabled(false); // actual implementation in WindowSupport
    cmd = ActionManager::registerAction(toggleFullScreenAction, Constants::TOGGLE_FULLSCREEN);
    cmd->setDefaultKeySequence(QKeySequence(useMacShortcuts ? Tr::tr("Ctrl+Meta+F") : Tr::tr("Ctrl+Shift+F11")));
    if (HostOsInfo::isMacHost())
        cmd->setAttribute(Command::CA_UpdateText);
    mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);

    if (useMacShortcuts) {
        mwindow->addSeparator(Constants::G_WINDOW_SIZE);

        QAction *closeAction = new QAction(Tr::tr("Close Window"), this);
        closeAction->setEnabled(false);
        cmd = ActionManager::registerAction(closeAction, Constants::CLOSE_WINDOW);
        cmd->setDefaultKeySequence(QKeySequence(Tr::tr("Ctrl+Meta+W")));
        mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);

        mwindow->addSeparator(Constants::G_WINDOW_SIZE);
    }

    // Show Left Sidebar Action
    m_toggleLeftSideBarAction = new QAction(Utils::Icons::TOGGLE_LEFT_SIDEBAR.icon(),
                                            Tr::tr(Constants::TR_SHOW_LEFT_SIDEBAR),
                                            this);
    m_toggleLeftSideBarAction->setCheckable(true);
    cmd = ActionManager::registerAction(m_toggleLeftSideBarAction, Constants::TOGGLE_LEFT_SIDEBAR);
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDefaultKeySequence(QKeySequence(useMacShortcuts ? Tr::tr("Ctrl+0") : Tr::tr("Alt+0")));
    connect(m_toggleLeftSideBarAction, &QAction::triggered,
            this, [this](bool visible) { setSidebarVisible(visible, Side::Left); });
    ProxyAction *toggleLeftSideBarProxyAction =
            ProxyAction::proxyActionWithIcon(cmd->action(), Utils::Icons::TOGGLE_LEFT_SIDEBAR_TOOLBAR.icon());
    m_toggleLeftSideBarButton->setDefaultAction(toggleLeftSideBarProxyAction);
    mview->addAction(cmd, Constants::G_VIEW_VIEWS);
    m_toggleLeftSideBarAction->setEnabled(false);

    // Show Right Sidebar Action
    m_toggleRightSideBarAction = new QAction(Utils::Icons::TOGGLE_RIGHT_SIDEBAR.icon(),
                                             Tr::tr(Constants::TR_SHOW_RIGHT_SIDEBAR),
                                             this);
    m_toggleRightSideBarAction->setCheckable(true);
    cmd = ActionManager::registerAction(m_toggleRightSideBarAction, Constants::TOGGLE_RIGHT_SIDEBAR);
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDefaultKeySequence(QKeySequence(useMacShortcuts ? Tr::tr("Ctrl+Shift+0") : Tr::tr("Alt+Shift+0")));
    connect(m_toggleRightSideBarAction, &QAction::triggered,
            this, [this](bool visible) { setSidebarVisible(visible, Side::Right); });
    ProxyAction *toggleRightSideBarProxyAction =
            ProxyAction::proxyActionWithIcon(cmd->action(), Utils::Icons::TOGGLE_RIGHT_SIDEBAR_TOOLBAR.icon());
    m_toggleRightSideBarButton->setDefaultAction(toggleRightSideBarProxyAction);
    mview->addAction(cmd, Constants::G_VIEW_VIEWS);
    m_toggleRightSideBarButton->setEnabled(false);

    // Show Menubar Action
    if (globalMenuBar() && !globalMenuBar()->isNativeMenuBar()) {
        m_toggleMenubarAction = new QAction(Tr::tr("Show Menubar"), this);
        m_toggleMenubarAction->setCheckable(true);
        cmd = ActionManager::registerAction(m_toggleMenubarAction, Constants::TOGGLE_MENUBAR);
        cmd->setDefaultKeySequence(QKeySequence(Tr::tr("Ctrl+Alt+M")));
        connect(m_toggleMenubarAction, &QAction::toggled, this, [cmd](bool visible) {
            if (!visible) {
                CheckableMessageBox::information(
                    Core::ICore::dialogParent(),
                    Tr::tr("Hide Menubar"),
                    Tr::tr(
                        "This will hide the menu bar completely. You can show it again by typing ")
                        + cmd->keySequence().toString(QKeySequence::NativeText),
                    QString("ToogleMenuBarHint"));
            }
            globalMenuBar()->setVisible(visible);
        });
        mview->addAction(cmd, Constants::G_VIEW_VIEWS);
    }

    registerModeSelectorStyleActions();

    // Window->Views
    ActionContainer *mviews = ActionManager::createMenu(Constants::M_VIEW_VIEWS);
    mview->addMenu(mviews, Constants::G_VIEW_VIEWS);
    mviews->menu()->setTitle(Tr::tr("&Views"));

    // "Help" separators
    mhelp->addSeparator(Constants::G_HELP_SUPPORT);
    if (!HostOsInfo::isMacHost())
        mhelp->addSeparator(Constants::G_HELP_ABOUT);

    // About IDE Action
    icon = Icon::fromTheme("help-about");
    if (HostOsInfo::isMacHost())
        tmpaction = new QAction(icon,
                                Tr::tr("About &%1").arg(QGuiApplication::applicationDisplayName()),
                                this); // it's convention not to add dots to the about menu
    else
        tmpaction
            = new QAction(icon,
                          Tr::tr("About &%1...").arg(QGuiApplication::applicationDisplayName()),
                          this);
    tmpaction->setMenuRole(QAction::AboutRole);
    cmd = ActionManager::registerAction(tmpaction, Constants::ABOUT_QTCREATOR);
    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    tmpaction->setEnabled(true);
    connect(tmpaction, &QAction::triggered, this, &MainWindowPrivate::aboutQtCreator);

    //About Plugins Action
    tmpaction = new QAction(Tr::tr("About &Plugins..."), this);
    tmpaction->setMenuRole(QAction::ApplicationSpecificRole);
    cmd = ActionManager::registerAction(tmpaction, Constants::ABOUT_PLUGINS);
    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    tmpaction->setEnabled(true);
    connect(tmpaction, &QAction::triggered, this, &MainWindowPrivate::aboutPlugins);
    // About Qt Action
    //    tmpaction = new QAction(Tr::tr("About &Qt..."), this);
    //    cmd = ActionManager::registerAction(tmpaction, Constants:: ABOUT_QT);
    //    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    //    tmpaction->setEnabled(true);
    //    connect(tmpaction, &QAction::triggered, qApp, &QApplication::aboutQt);

    // Change Log Action
    tmpaction = new QAction(Tr::tr("Change Log..."), this);
    tmpaction->setMenuRole(QAction::ApplicationSpecificRole);
    cmd = ActionManager::registerAction(tmpaction, Constants::CHANGE_LOG);
    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    tmpaction->setEnabled(true);
    connect(tmpaction, &QAction::triggered, this, &MainWindowPrivate::changeLog);

    // Contact
    tmpaction = new QAction(Tr::tr("Contact..."), this);
    cmd = ActionManager::registerAction(tmpaction, "QtCreator.Contact");
    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    tmpaction->setEnabled(true);
    connect(tmpaction, &QAction::triggered, this, &MainWindowPrivate::contact);

    // About sep
    if (!HostOsInfo::isMacHost()) { // doesn't have the "About" actions in the Help menu
        tmpaction = new QAction(this);
        tmpaction->setSeparator(true);
        cmd = ActionManager::registerAction(tmpaction, "QtCreator.Help.Sep.About");
        mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    }
}

void MainWindowPrivate::registerModeSelectorStyleActions()
{
    ActionContainer *mview = ActionManager::actionContainer(Constants::M_VIEW);

    // Cycle Mode Selector Styles
    m_cycleModeSelectorStyleAction = new QAction(Tr::tr("Cycle Mode Selector Styles"), this);
    ActionManager::registerAction(m_cycleModeSelectorStyleAction, Constants::CYCLE_MODE_SELECTOR_STYLE);
    connect(m_cycleModeSelectorStyleAction, &QAction::triggered, this, [this] {
        ModeManager::cycleModeStyle();
        updateModeSelectorStyleMenu();
    });

    // Mode Selector Styles
    ActionContainer *mmodeLayouts = ActionManager::createMenu(Constants::M_VIEW_MODESTYLES);
    mview->addMenu(mmodeLayouts, Constants::G_VIEW_VIEWS);
    QMenu *styleMenu = mmodeLayouts->menu();
    styleMenu->setTitle(Tr::tr("Mode Selector Style"));
    auto *stylesGroup = new QActionGroup(styleMenu);
    stylesGroup->setExclusive(true);

    m_setModeSelectorStyleIconsAndTextAction = stylesGroup->addAction(Tr::tr("Icons and Text"));
    connect(m_setModeSelectorStyleIconsAndTextAction, &QAction::triggered,
                                 [] { ModeManager::setModeStyle(ModeManager::Style::IconsAndText); });
    m_setModeSelectorStyleIconsAndTextAction->setCheckable(true);
    m_setModeSelectorStyleIconsOnlyAction = stylesGroup->addAction(Tr::tr("Icons Only"));
    connect(m_setModeSelectorStyleIconsOnlyAction, &QAction::triggered,
                                 [] { ModeManager::setModeStyle(ModeManager::Style::IconsOnly); });
    m_setModeSelectorStyleIconsOnlyAction->setCheckable(true);
    m_setModeSelectorStyleHiddenAction = stylesGroup->addAction(Tr::tr("Hidden"));
    connect(m_setModeSelectorStyleHiddenAction, &QAction::triggered,
                                 [] { ModeManager::setModeStyle(ModeManager::Style::Hidden); });
    m_setModeSelectorStyleHiddenAction->setCheckable(true);

    styleMenu->addActions(stylesGroup->actions());
}

void MainWindowPrivate::openFile()
{
    openFiles(EditorManager::getOpenFilePaths(), ICore::SwitchMode);
}

static IDocumentFactory *findDocumentFactory(const QList<IDocumentFactory*> &fileFactories,
                                             const FilePath &filePath)
{
    const QString typeName = Utils::mimeTypeForFile(filePath, MimeMatchMode::MatchDefaultAndRemote)
                                 .name();
    return Utils::findOrDefault(fileFactories, [typeName](IDocumentFactory *f) {
        return f->mimeTypes().contains(typeName);
    });
}

/*!
 * \internal
 * Either opens \a filePaths with editors or loads a project.
 *
 *  \a flags can be used to stop on first failure, indicate that a file name
 *  might include line numbers and/or switch mode to edit mode.
 *
 *  \a workingDirectory is used when files are opened by a remote client, since
 *  the file names are relative to the client working directory.
 *
 *  Returns the first opened document. Required to support the \c -block flag
 *  for client mode.
 *
 *  \sa IPlugin::remoteArguments()
 */
IDocument *MainWindow::openFiles(const FilePaths &filePaths,
                                 ICore::OpenFilesFlags flags,
                                 const FilePath &workingDirectory)
{
    return MainWindowPrivate::openFiles(filePaths, flags, workingDirectory);
}

IDocument *MainWindowPrivate::openFiles(const FilePaths &filePaths,
                                        ICore::OpenFilesFlags flags,
                                        const FilePath &workingDirectory)
{
    const QList<IDocumentFactory*> documentFactories = IDocumentFactory::allDocumentFactories();
    IDocument *res = nullptr;

    const FilePath workingDirBase =
            workingDirectory.isEmpty() ? FilePath::currentWorkingPath() : workingDirectory;
    for (const FilePath &filePath : filePaths) {
        const FilePath absoluteFilePath = workingDirBase.resolvePath(filePath);
        if (IDocumentFactory *documentFactory = findDocumentFactory(documentFactories, filePath)) {
            IDocument *document = documentFactory->open(absoluteFilePath);
            if (!document) {
                if (flags & ICore::StopOnLoadFail)
                    return res;
            } else {
                if (!res)
                    res = document;
                if (flags & ICore::SwitchMode)
                    ModeManager::activateMode(Id(Constants::MODE_EDIT));
            }
        } else if (flags & (ICore::SwitchSplitIfAlreadyVisible | ICore::CanContainLineAndColumnNumbers)
                   || !res) {
            QFlags<EditorManager::OpenEditorFlag> emFlags;
            if (flags & ICore::SwitchSplitIfAlreadyVisible)
                emFlags |= EditorManager::SwitchSplitIfAlreadyVisible;
            IEditor *editor = nullptr;
            if (flags & ICore::CanContainLineAndColumnNumbers) {
                const Link &link = Link::fromString(absoluteFilePath.toString(), true);
                editor = EditorManager::openEditorAt(link, {}, emFlags);
            } else {
                editor = EditorManager::openEditor(absoluteFilePath, {}, emFlags);
            }
            if (!editor) {
                if (flags & ICore::StopOnLoadFail)
                    return res;
            } else if (!res) {
                res = editor->document();
            }
        } else {
            auto factory = IEditorFactory::preferredEditorFactories(absoluteFilePath).value(0);
            DocumentModelPrivate::addSuspendedDocument(absoluteFilePath, {},
                                                       factory ? factory->id() : Id());
        }
    }
    return res;
}

void MainWindowPrivate::setFocusToEditor()
{
    EditorManagerPrivate::doEscapeKeyFocusMoveMagic();
}

static void acceptModalDialogs()
{
    const QWidgetList topLevels = QApplication::topLevelWidgets();
    QList<QDialog *> dialogsToClose;
    for (QWidget *topLevel : topLevels) {
        if (auto dialog = qobject_cast<QDialog *>(topLevel)) {
            if (dialog->isModal())
                dialogsToClose.append(dialog);
        }
    }
    for (QDialog *dialog : dialogsToClose)
        dialog->accept();
}

void MainWindow::exit()
{
    // this function is most likely called from a user action
    // that is from an event handler of an object
    // since on close we are going to delete everything
    // so to prevent the deleting of that object we
    // just append it
    QMetaObject::invokeMethod(
        this,
        [this] {
            // Modal dialogs block the close event. So close them, in case this was triggered from
            // a RestartDialog in the settings dialog.
            acceptModalDialogs();
            close();
        },
        Qt::QueuedConnection);
}

void MainWindow::openFileWith()
{
    const FilePaths filePaths = EditorManager::getOpenFilePaths();
    for (const FilePath &filePath : filePaths) {
        bool isExternal;
        const Id editorId = EditorManagerPrivate::getOpenWithEditorId(filePath, &isExternal);
        if (!editorId.isValid())
            continue;
        if (isExternal)
            EditorManager::openExternalEditor(filePath, editorId);
        else
            EditorManagerPrivate::openEditorWith(filePath, editorId);
    }
}

void MainWindowPrivate::openFileFromDevice()
{
    openFiles(EditorManager::getOpenFilePaths(QFileDialog::DontUseNativeDialog), ICore::SwitchMode);
}

IContext *MainWindow::contextObject(QWidget *widget) const
{
    const auto it = d->m_contextWidgets.find(widget);
    return it == d->m_contextWidgets.end() ? nullptr : it->second;
}

void MainWindow::addContextObject(IContext *context)
{
    if (!context)
        return;
    QWidget *widget = context->widget();
    if (d->m_contextWidgets.find(widget) != d->m_contextWidgets.end())
        return;

    d->m_contextWidgets.insert({widget, context});
    connect(context, &QObject::destroyed, this, [this, context] { removeContextObject(context); });
}

void MainWindow::removeContextObject(IContext *context)
{
    if (!context)
        return;

    disconnect(context, &QObject::destroyed, this, nullptr);

    const auto it = std::find_if(d->m_contextWidgets.cbegin(),
                                 d->m_contextWidgets.cend(),
                                 [context](const std::pair<QWidget *, IContext *> &v) {
                                     return v.second == context;
                                 });
    if (it == d->m_contextWidgets.cend())
        return;

    d->m_contextWidgets.erase(it);
    if (d->m_activeContext.removeAll(context) > 0)
        d->updateContextObject(d->m_activeContext);
}

void MainWindowPrivate::updateFocusWidget(QWidget *old, QWidget *now)
{
    Q_UNUSED(old)

    // Prevent changing the context object just because the menu or a menu item is activated
    if (qobject_cast<QMenuBar*>(now) || qobject_cast<QMenu*>(now))
        return;

    QList<IContext *> newContext;
    if (QWidget *p = QApplication::focusWidget()) {
        IContext *context = nullptr;
        while (p) {
            context = q->contextObject(p);
            if (context)
                newContext.append(context);
            p = p->parentWidget();
        }
    }

    // ignore toplevels that define no context, like popups without parent
    if (!newContext.isEmpty() || QApplication::focusWidget() == q->focusWidget())
        updateContextObject(newContext);
}

void MainWindowPrivate::updateContextObject(const QList<IContext *> &context)
{
    emit m_coreImpl->contextAboutToChange(context);
    m_activeContext = context;
    updateContext();
    if (debugMainWindow) {
        qDebug() << "new context objects =" << context;
        for (const IContext *c : context)
            qDebug() << (c ? c->widget() : nullptr) << (c ? c->widget()->metaObject()->className() : nullptr);
    }
}

void MainWindow::aboutToShutdown()
{
    disconnect(qApp, &QApplication::focusChanged, d, &MainWindowPrivate::updateFocusWidget);
    for (auto contextPair : d->m_contextWidgets)
        disconnect(contextPair.second, &QObject::destroyed, this, nullptr);
    d->m_activeContext.clear();
    hide();
}

void MainWindowPrivate::readSettings()
{
    QtcSettings *settings = PluginManager::settings();
    settings->beginGroup(QLatin1String(settingsGroup));

    if (m_overrideColor.isValid()) {
        StyleHelper::setBaseColor(m_overrideColor);
        // Get adapted base color.
        m_overrideColor = StyleHelper::baseColor();
    } else {
        StyleHelper::setBaseColor(settings->value(colorKey,
                                  QColor(StyleHelper::DEFAULT_BASE_COLOR)).value<QColor>());
    }

    {
        ModeManager::Style modeStyle =
                ModeManager::Style(settings->value(modeSelectorLayoutKey, int(ModeManager::Style::IconsAndText)).toInt());

        // Migrate legacy setting from Qt Creator 4.6 and earlier
        static const char modeSelectorVisibleKey[] = "ModeSelectorVisible";
        if (!settings->contains(modeSelectorLayoutKey) && settings->contains(modeSelectorVisibleKey)) {
            bool visible = settings->value(modeSelectorVisibleKey, true).toBool();
            modeStyle = visible ? ModeManager::Style::IconsAndText : ModeManager::Style::Hidden;
        }

        ModeManager::setModeStyle(modeStyle);
        updateModeSelectorStyleMenu();
    }

    if (globalMenuBar() && !globalMenuBar()->isNativeMenuBar()) {
        const bool isVisible = settings->value(menubarVisibleKey, true).toBool();

        globalMenuBar()->setVisible(isVisible);
        if (m_toggleMenubarAction)
            m_toggleMenubarAction->setChecked(isVisible);
    }

    settings->endGroup();

    EditorManagerPrivate::readSettings();
    m_leftNavigationWidget->restoreSettings(settings);
    m_rightNavigationWidget->restoreSettings(settings);
    m_rightPaneWidget->readSettings(settings);
}

void MainWindow::saveSettings()
{
    QtcSettings *settings = PluginManager::settings();
    settings->beginGroup(QLatin1String(settingsGroup));

    if (!(d->m_overrideColor.isValid() && StyleHelper::baseColor() == d->m_overrideColor))
        settings->setValueWithDefault(colorKey,
                                      StyleHelper::requestedBaseColor(),
                                      QColor(StyleHelper::DEFAULT_BASE_COLOR));

    if (globalMenuBar() && !globalMenuBar()->isNativeMenuBar())
        settings->setValue(menubarVisibleKey, globalMenuBar()->isVisible());

    settings->endGroup();

    DocumentManager::saveSettings();
    ActionManager::saveSettings();
    EditorManagerPrivate::saveSettings();
    d->m_leftNavigationWidget->saveSettings(settings);
    d->m_rightNavigationWidget->saveSettings(settings);

    // TODO Remove some time after Qt Creator 11
    // Work around Qt Creator <= 10 writing the default terminal to the settings.
    // TerminalCommand writes the terminal to the settings when changing it, which usually is
    // enough. But because of the bug in Qt Creator <= 10 we want to clean up the settings
    // even if the user never touched the terminal setting.
    if (HostOsInfo::isMacHost())
        TerminalCommand::setTerminalEmulator(TerminalCommand::terminalEmulator());
}

void MainWindowPrivate::saveWindowSettings()
{
    QtcSettings *settings = PluginManager::settings();
    settings->beginGroup(settingsGroup);

    // On OS X applications usually do not restore their full screen state.
    // To be able to restore the correct non-full screen geometry, we have to put
    // the window out of full screen before saving the geometry.
    // Works around QTBUG-45241
    if (Utils::HostOsInfo::isMacHost() && q->isFullScreen())
        q->setWindowState(q->windowState() & ~Qt::WindowFullScreen);
    settings->setValue(windowGeometryKey, q->saveGeometry());
    settings->setValue(windowStateKey, q->saveState());
    settings->setValue(modeSelectorLayoutKey, int(ModeManager::modeStyle()));

    settings->endGroup();
}

void MainWindowPrivate::updateModeSelectorStyleMenu()
{
    switch (ModeManager::modeStyle()) {
    case ModeManager::Style::IconsAndText:
        m_setModeSelectorStyleIconsAndTextAction->setChecked(true);
        break;
    case ModeManager::Style::IconsOnly:
        m_setModeSelectorStyleIconsOnlyAction->setChecked(true);
        break;
    case ModeManager::Style::Hidden:
        m_setModeSelectorStyleHiddenAction->setChecked(true);
        break;
    }
}

void MainWindow::updateAdditionalContexts(const Context &remove, const Context &add,
                                          ICore::ContextPriority priority)
{
    for (const Id id : remove) {
        if (!id.isValid())
            continue;
        int index = d->m_lowPrioAdditionalContexts.indexOf(id);
        if (index != -1)
            d->m_lowPrioAdditionalContexts.removeAt(index);
        index = d->m_highPrioAdditionalContexts.indexOf(id);
        if (index != -1)
            d->m_highPrioAdditionalContexts.removeAt(index);
    }

    for (const Id id : add) {
        if (!id.isValid())
            continue;
        Context &cref = (priority == ICore::ContextPriority::High ? d->m_highPrioAdditionalContexts
                                                                  : d->m_lowPrioAdditionalContexts);
        if (!cref.contains(id))
            cref.prepend(id);
    }

    d->updateContext();
}

void MainWindowPrivate::updateContext()
{
    Context contexts = m_highPrioAdditionalContexts;

    for (IContext *context : std::as_const(m_activeContext))
        contexts.add(context->context());

    contexts.add(m_lowPrioAdditionalContexts);

    Context uniquecontexts;
    for (const Id &id : std::as_const(contexts)) {
        if (!uniquecontexts.contains(id))
            uniquecontexts.add(id);
    }

    ActionManager::setContext(uniquecontexts);
    emit m_coreImpl->contextChanged(uniquecontexts);
}

void MainWindowPrivate::aboutToShowRecentFiles()
{
    ActionContainer *aci = ActionManager::actionContainer(Constants::M_FILE_RECENTFILES);
    QMenu *menu = aci->menu();
    menu->clear();

    const QList<DocumentManager::RecentFile> recentFiles = DocumentManager::recentFiles();
    for (int i = 0; i < recentFiles.count(); ++i) {
        const DocumentManager::RecentFile file = recentFiles[i];

        const QString filePath = Utils::quoteAmpersands(file.first.shortNativePath());
        const QString actionText = ActionManager::withNumberAccelerator(filePath, i + 1);
        QAction *action = menu->addAction(actionText);
        connect(action, &QAction::triggered, this, [file] {
            EditorManager::openEditor(file.first, file.second);
        });
    }

    bool hasRecentFiles = !recentFiles.isEmpty();
    menu->setEnabled(hasRecentFiles);

    // add the Clear Menu item
    if (hasRecentFiles) {
        menu->addSeparator();
        QAction *action = menu->addAction(Tr::tr(Constants::TR_CLEAR_MENU));
        connect(action, &QAction::triggered,
                DocumentManager::instance(), &DocumentManager::clearRecentFiles);
    }
}

void MainWindowPrivate::aboutQtCreator()
{
    if (!m_versionDialog) {
        m_versionDialog = new VersionDialog(q);
        connect(m_versionDialog, &QDialog::finished,
                this, &MainWindowPrivate::destroyVersionDialog);
        ICore::registerWindow(m_versionDialog, Context("Core.VersionDialog"));
        m_versionDialog->show();
    } else {
        ICore::raiseWindow(m_versionDialog);
    }
}

void MainWindowPrivate::destroyVersionDialog()
{
    if (m_versionDialog) {
        m_versionDialog->deleteLater();
        m_versionDialog = nullptr;
    }
}

void MainWindowPrivate::aboutPlugins()
{
    PluginDialog dialog(q);
    dialog.exec();
}

class LogDialog : public QDialog
{
public:
    LogDialog(QWidget *parent)
        : QDialog(parent)
    {}
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ShortcutOverride) {
            auto ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Escape && !ke->modifiers()) {
                ke->accept();
                return true;
            }
        }
        return QDialog::event(event);
    }
};

void MainWindowPrivate::changeLog()
{
    static QPointer<LogDialog> dialog;
    if (dialog) {
        ICore::raiseWindow(dialog);
        return;
    }
    const FilePaths files =
            ICore::resourcePath("changelog").dirEntries({{"changes-*"}, QDir::Files});
    static const QRegularExpression versionRegex("\\d+[.]\\d+[.]\\d+");
    using VersionFilePair = std::pair<QVersionNumber, FilePath>;
    QList<VersionFilePair> versionedFiles = Utils::transform(files, [](const FilePath &fp) {
        const QRegularExpressionMatch match = versionRegex.match(fp.fileName());
        const QVersionNumber version = match.hasMatch()
                                           ? QVersionNumber::fromString(match.captured())
                                           : QVersionNumber();
        return std::make_pair(version, fp);
    });
    Utils::sort(versionedFiles, [](const VersionFilePair &a, const VersionFilePair &b) {
        return a.first > b.first;
    });

    auto versionCombo = new QComboBox;
    for (const VersionFilePair &f : versionedFiles)
        versionCombo->addItem(f.first.toString());
    dialog = new LogDialog(ICore::dialogParent());
    auto versionLayout = new QHBoxLayout;
    versionLayout->addWidget(new QLabel(Tr::tr("Version:")));
    versionLayout->addWidget(versionCombo);
    versionLayout->addStretch(1);
    auto showInExplorer = new QPushButton(FileUtils::msgGraphicalShellAction());
    versionLayout->addWidget(showInExplorer);
    auto textEdit = new QTextBrowser;
    textEdit->setOpenExternalLinks(true);

    auto aggregate = new Aggregation::Aggregate;
    aggregate->add(textEdit);
    aggregate->add(new Core::BaseTextFind(textEdit));

    new MarkdownHighlighter(textEdit->document());

    auto textEditWidget = new QFrame;
    textEditWidget->setFrameStyle(QFrame::NoFrame);
    auto findToolBar = new FindToolBarPlaceHolder(dialog);
    findToolBar->setLightColored(true);
    auto textEditLayout = new QVBoxLayout;
    textEditLayout->setContentsMargins(0, 0, 0, 0);
    textEditLayout->setSpacing(0);
    textEditLayout->addWidget(textEdit);
    textEditLayout->addWidget(findToolBar);
    textEditWidget->setLayout(textEditLayout);
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    auto dialogLayout = new QVBoxLayout;
    dialogLayout->addLayout(versionLayout);
    dialogLayout->addWidget(textEditWidget);
    dialogLayout->addWidget(buttonBox);
    dialog->setLayout(dialogLayout);
    dialog->resize(700, 600);
    dialog->setWindowTitle(Tr::tr("Change Log"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    ICore::registerWindow(dialog, Context("CorePlugin.VersionDialog"));

    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    QPushButton *closeButton = buttonBox->button(QDialogButtonBox::Close);
    if (QTC_GUARD(closeButton))
        closeButton->setDefault(true); // grab from "Open in Explorer" button

    const auto showLog = [textEdit, versionedFiles](int index) {
        if (index < 0 || index >= versionedFiles.size())
            return;
        const FilePath file = versionedFiles.at(index).second;
        QString contents = QString::fromUtf8(file.fileContents().value_or(QByteArray()));
        // (?<![[\/]) == don't replace if it is preceded by "[" or "/"
        // i.e. if it already is part of a link
        static const QRegularExpression bugexpr(R"((?<![[\/])((QT(CREATOR)?BUG|PYSIDE)-\d+))");
        contents.replace(bugexpr, R"([\1](https://bugreports.qt.io/browse/\1))");
        static const QRegularExpression docexpr("https://doc[.]qt[.]io/qtcreator/([.a-zA-Z/_-]*)");
        QList<QRegularExpressionMatch> matches;
        for (const QRegularExpressionMatch &m : docexpr.globalMatch(contents))
            matches.append(m);
        Utils::reverseForeach(matches, [&contents](const QRegularExpressionMatch &match) {
            const QString qthelpUrl = "qthelp://org.qt-project.qtcreator/doc/" + match.captured(1);
            if (!HelpManager::fileData(qthelpUrl).isEmpty())
                contents.replace(match.capturedStart(), match.capturedLength(), qthelpUrl);
        });
        textEdit->setMarkdown(contents);
    };
    connect(versionCombo, &QComboBox::currentIndexChanged, textEdit, showLog);
    showLog(versionCombo->currentIndex());

    connect(showInExplorer, &QPushButton::clicked, this, [versionCombo, versionedFiles] {
        const int index = versionCombo->currentIndex();
        if (index >= 0 && index < versionedFiles.size())
            FileUtils::showInGraphicalShell(ICore::dialogParent(), versionedFiles.at(index).second);
        else
            FileUtils::showInGraphicalShell(ICore::dialogParent(), ICore::resourcePath("changelog"));
    });

    dialog->show();
}

void MainWindowPrivate::contact()
{
    QMessageBox dlg(QMessageBox::Information, Tr::tr("Contact"),
           Tr::tr("<p>Qt Creator developers can be reached at the Qt Creator mailing list:</p>"
              "%1"
              "<p>or the #qt-creator channel on Libera.Chat IRC:</p>"
              "%2"
              "<p>Our bug tracker is located at %3.</p>"
              "<p>Please use %4 for bigger chunks of text.</p>")
                    .arg("<p>&nbsp;&nbsp;&nbsp;&nbsp;"
                            "<a href=\"https://lists.qt-project.org/listinfo/qt-creator\">"
                            "mailto:qt-creator@qt-project.org"
                         "</a></p>")
                    .arg("<p>&nbsp;&nbsp;&nbsp;&nbsp;"
                            "<a href=\"https://web.libera.chat/#qt-creator\">"
                            "https://web.libera.chat/#qt-creator"
                         "</a></p>")
                    .arg("<a href=\"https://bugreports.qt.io/projects/QTCREATORBUG\">"
                            "https://bugreports.qt.io"
                         "</a>")
                    .arg("<a href=\"https://pastebin.com\">"
                            "https://pastebin.com"
                         "</a>"),
           QMessageBox::Ok, q);
    dlg.exec();
}

QPrinter *MainWindow::printer() const
{
    if (!d->m_printer)
        d->m_printer = new QPrinter(QPrinter::HighResolution);
    return d->m_printer;
}

void MainWindowPrivate::restoreWindowState()
{
    NANOTRACE_SCOPE("Core", "MainWindow::restoreWindowState");
    QtcSettings *settings = PluginManager::settings();
    settings->beginGroup(settingsGroup);
    if (!q->restoreGeometry(settings->value(windowGeometryKey).toByteArray()))
        q->resize(1260, 700); // size without window decoration
    q->restoreState(settings->value(windowStateKey).toByteArray());
    settings->endGroup();
    q->show();
    StatusBarManager::restoreSettings();
}

} // namespace Internal
} // namespace Core
