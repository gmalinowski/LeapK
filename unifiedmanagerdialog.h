/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QFrame>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QColorDialog>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

#include "workspacehelper.h"
#include "persistentmarkhelper.h"
#include "themehelper.h"

struct OpenWindowInfo
{
    QString app;
    QString caption;
    int desktop = 0;
};

enum ManagerTab
{
    TabWorkspaces = 0,
    TabMarkers = 1
};

struct ManagerContext
{
    QString appClass;
    QString windowCaption;
    int desktopNum = 0;
    QString desktopName;
    QString assignedWsId;
    QList<LeapKWorkspace::WorkspaceInfo> workspaces;
    QList<OpenWindowInfo> openWindows;
    LeapKTheme::Theme theme;
    ManagerTab defaultTab = TabWorkspaces;
};

// -----------------------------------------------------------------------------
// Frame Widget that paints themed borders, background, acrylic glass, and glow
// -----------------------------------------------------------------------------
class MainUnifiedFrame : public QFrame
{
    Q_OBJECT
public:
    explicit MainUnifiedFrame(const LeapKTheme::Theme &theme, QWidget *parent = nullptr);
    void setHeaderTitle(const QString &title);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    LeapKTheme::Theme m_theme;
    QString m_title;
};

// -----------------------------------------------------------------------------
// Workspace Helper Dialogs: CloseWorkspaceDialog & EditWorkspaceDialog
// -----------------------------------------------------------------------------
class CloseWorkspaceDialog : public QDialog
{
    Q_OBJECT
public:
    enum Action {
        KeepWindows,
        MoveToStash,
        CloseWindows
    };

    explicit CloseWorkspaceDialog(const QString &wsName, const LeapKTheme::Theme &theme, QWidget *parent = nullptr);
    Action selectedAction() const { return m_action; }

private:
    Action m_action = KeepWindows;
};

class EditWorkspaceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EditWorkspaceDialog(const LeapKWorkspace::WorkspaceInfo &info, const LeapKTheme::Theme &theme, QWidget *parent = nullptr);

    QString name() const;
    QColor color() const;

private:
    void updateColorPreview();

    QLineEdit *m_editName = nullptr;
    QColor m_color;
    QWidget *m_colorPreview = nullptr;
};

// -----------------------------------------------------------------------------
// Marker Helper Dialog: EditMarkerRuleDialog
// -----------------------------------------------------------------------------
class EditMarkerRuleDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EditMarkerRuleDialog(const LeapKMarks::PersistentMarkRule &rule,
                                 const LeapKTheme::Theme &theme,
                                 bool isNew = false,
                                 QWidget *parent = nullptr);

    LeapKMarks::PersistentMarkRule rule() const { return m_rule; }

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void openCustomColorDialog();
    void updateColorPreview();
    void onSaveClicked();

    LeapKMarks::PersistentMarkRule m_rule;
    LeapKTheme::Theme m_theme;
    bool m_isNew = false;
    QColor m_selectedColor;

    QLineEdit *m_letterEdit = nullptr;
    QLineEdit *m_labelEdit = nullptr;
    QLineEdit *m_appEdit = nullptr;
    QLineEdit *m_captionEdit = nullptr;
    QComboBox *m_desktopCombo = nullptr;
    QCheckBox *m_enabledCb = nullptr;
    QWidget *m_colorPreview = nullptr;

    QPoint m_dragPos;
    bool m_isDragging = false;
};

// -----------------------------------------------------------------------------
// Workspace Card Widget
// -----------------------------------------------------------------------------
class WorkspaceCard : public QFrame
{
    Q_OBJECT
public:
    explicit WorkspaceCard(int index,
                          const LeapKWorkspace::WorkspaceInfo &ws,
                          bool isAssigned,
                          bool isSelected,
                          int matchingWindowCount,
                          const LeapKTheme::Theme &theme,
                          QWidget *parent = nullptr);

    void setAssigned(bool assigned);
    void setSelected(bool selected);
    void updateStatusBadge();

    std::function<void(int index)> onEditClicked;
    std::function<void(int index)> onDeleteClicked;
    std::function<void(int index)> onToggleAssignment;

protected:
    void mousePressEvent(QMouseEvent *e) override;

private:
    int m_index;
    LeapKWorkspace::WorkspaceInfo m_ws;
    bool m_isAssigned;
    bool m_isSelected;
    int m_matchingWindowCount;
    LeapKTheme::Theme m_theme;

    QLabel *m_lblNum = nullptr;
    QLabel *m_lblColorDot = nullptr;
    QLabel *m_lblStatus = nullptr;
    QPushButton *m_btnEdit = nullptr;
    QPushButton *m_btnDelete = nullptr;
};

// -----------------------------------------------------------------------------
// Marker Rule Card Widget
// -----------------------------------------------------------------------------
class MarkerRuleCard : public QFrame
{
    Q_OBJECT
public:
    explicit MarkerRuleCard(const LeapKMarks::PersistentMarkRule &rule,
                           int cardIndex,
                           bool isSelected,
                           int matchingCount,
                           const QStringList &matchingTitles,
                           const LeapKTheme::Theme &theme,
                           QWidget *parent = nullptr);

    LeapKMarks::PersistentMarkRule rule() const { return m_rule; }
    void setSelected(bool selected);
    void updateStyle();
    void updateToggleStyle();

    std::function<void(const LeapKMarks::PersistentMarkRule &)> onEditClicked;
    std::function<void(const LeapKMarks::PersistentMarkRule &)> onDeleteClicked;
    std::function<void(const LeapKMarks::PersistentMarkRule &)> onToggleEnabled;

protected:
    void mousePressEvent(QMouseEvent *e) override;

private:
    LeapKMarks::PersistentMarkRule m_rule;
    int m_cardIndex;
    bool m_isSelected;
    int m_matchingCount;
    QStringList m_matchingTitles;
    LeapKTheme::Theme m_theme;

    QLabel *m_lblBadge = nullptr;
    QPushButton *m_btnToggle = nullptr;
};

// -----------------------------------------------------------------------------
// WorkspaceManagerView (Page 0 in unified stack)
// -----------------------------------------------------------------------------
class WorkspaceManagerView : public QWidget
{
    Q_OBJECT
public:
    explicit WorkspaceManagerView(const ManagerContext &ctx, QWidget *parent = nullptr);

    QString assignedWorkspaceId() const { return m_assignedWsId; }
    bool groupByApp() const { return m_groupByApp; }
    QList<LeapKWorkspace::WorkspaceInfo> workspaces() const { return m_workspaces; }
    QJsonArray deletedWorkspaces() const { return m_deletedWorkspaces; }

    void rebuildCards();
    void selectCard(int idx);
    void selectNextCard();
    void selectPrevCard();

    void onNewWorkspace();
    void onNewEmptyWorkspace();
    void editSelectedWorkspace();
    void deleteSelectedOrAssignedWorkspace();
    void onUnassign();
    void setScope(bool appScope);
    void toggleWorkspaceAssignment(int idx);

    bool handleKeyEvent(QKeyEvent *e);

signals:
    void requestAccept();
    void requestReject();

private:
    ManagerContext m_ctx;
    QString m_appClass;
    QString m_windowCaption;
    int m_desktopNum;
    QString m_desktopName;
    QString m_assignedWsId;
    QList<LeapKWorkspace::WorkspaceInfo> m_workspaces;
    LeapKTheme::Theme m_theme;
    bool m_groupByApp = true;
    int m_selectedCardIndex = -1;
    QJsonArray m_deletedWorkspaces;

    QPushButton *m_btnScopeApp = nullptr;
    QPushButton *m_btnScopeDirect = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_cardsContainer = nullptr;
    QVBoxLayout *m_cardsLayout = nullptr;
    QList<WorkspaceCard *> m_cards;

    QPushButton *m_btnNewWs = nullptr;
    QPushButton *m_btnNewEmptyWs = nullptr;
    QPushButton *m_btnUnassign = nullptr;
    QPushButton *m_btnEditWs = nullptr;
    QPushButton *m_btnDeleteWs = nullptr;
};

// -----------------------------------------------------------------------------
// MarkerManagerView (Page 1 in unified stack)
// -----------------------------------------------------------------------------
class MarkerManagerView : public QWidget
{
    Q_OBJECT
public:
    explicit MarkerManagerView(const ManagerContext &ctx, QWidget *parent = nullptr);

    bool isSearchFocused() const;
    void clearSearchFocus();
    void focusSearch();

    void loadRules();
    void saveRules();
    void rebuildCards();
    void selectCard(int idx);
    void selectNextCard();
    void selectPrevCard();

    void onAddNewRuleClicked();
    void onAssignCurrentClicked();
    void onClearAllClicked();
    void onEditRuleClicked(const LeapKMarks::PersistentMarkRule &rule);
    void onDeleteRuleClicked(const LeapKMarks::PersistentMarkRule &rule);
    void onToggleRuleEnabled(const LeapKMarks::PersistentMarkRule &rule);

    bool handleKeyEvent(QKeyEvent *e);

private:
    ManagerContext m_ctx;
    QString m_appClass;
    QString m_windowCaption;
    int m_desktopNum;
    QString m_desktopName;
    QList<OpenWindowInfo> m_openWindows;
    LeapKTheme::Theme m_theme;

    QList<LeapKMarks::PersistentMarkRule> m_rules;
    QList<MarkerRuleCard *> m_cards;
    int m_selectedCardIndex = -1;

    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_btnNewRule = nullptr;
    QPushButton *m_btnAssignCurrent = nullptr;
    QPushButton *m_btnClearAll = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_cardsContainer = nullptr;
    QVBoxLayout *m_cardsLayout = nullptr;
    QLabel *m_emptyLabel = nullptr;
};

// -----------------------------------------------------------------------------
// UnifiedManagerDialog
// -----------------------------------------------------------------------------
class UnifiedManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UnifiedManagerDialog(const ManagerContext &ctx, QWidget *parent = nullptr);

    void switchTab(ManagerTab tab);
    ManagerTab currentTab() const { return m_currentTab; }

    // Workspaces results
    QString assignedWorkspaceId() const;
    bool groupByApp() const;
    QList<LeapKWorkspace::WorkspaceInfo> workspaces() const;
    QJsonArray deletedWorkspaces() const;

protected:
    void showEvent(QShowEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void updateTabButtonStyles();
    void updateFooterHints();

    ManagerContext m_ctx;
    LeapKTheme::Theme m_theme;
    ManagerTab m_currentTab;

    MainUnifiedFrame *m_frame = nullptr;
    QPushButton *m_tabBtnWorkspaces = nullptr;
    QPushButton *m_tabBtnMarkers = nullptr;
    QPushButton *m_btnClose = nullptr;
    QStackedWidget *m_stack = nullptr;
    WorkspaceManagerView *m_workspaceView = nullptr;
    MarkerManagerView *m_markerView = nullptr;
    QLabel *m_lblShortcutHints = nullptr;

    QPoint m_dragPos;
    bool m_isDragging = false;
};
