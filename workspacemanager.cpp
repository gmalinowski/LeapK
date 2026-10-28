/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "workspacehelper.h"
#include <QApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QLocale>
#include <QMessageBox>
#include <QInputDialog>
#include <iostream>

class MainWorkspaceFrame : public QFrame
{
public:
    explicit MainWorkspaceFrame(QWidget *parent = nullptr) : QFrame(parent)
    {
        setAttribute(Qt::WA_StyledBackground, false);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = rect().adjusted(1, 1, -1, -1);

        // Dark HUD acrylic body
        p.setPen(QPen(QColor(255, 255, 255, 45), 1.5));
        p.setBrush(QColor(18, 21, 26, 248));
        p.drawRoundedRect(r, 16.0, 16.0);

        // Top accent line (Workspace blue/indigo accent)
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(52, 152, 219, 220));
        p.drawRoundedRect(QRectF(r.left() + 24, r.top(), r.width() - 48, 3), 1.5, 1.5);
    }
};

class CloseWorkspaceDialog : public QDialog
{
public:
    enum Action {
        Cancel = 0,
        CloseWindows,
        MoveToStandard,
        MoveToStash
    };

    explicit CloseWorkspaceDialog(const QString &wsName, bool isPolish, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_wsName(wsName)
        , m_isPolish(isPolish)
        , m_selectedAction(Cancel)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        setAttribute(Qt::WA_TranslucentBackground, true);
        resize(500, 370);

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);

        auto *frame = new MainWorkspaceFrame(this);
        rootLayout->addWidget(frame);

        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(22, 20, 22, 20);
        layout->setSpacing(12);

        // Header
        auto *lblTitle = new QLabel(m_isPolish
            ? QStringLiteral("🗑 Zamknij przestrzeń: <b style='color:#e74c3c;'>%1</b>").arg(wsName.toHtmlEscaped())
            : QStringLiteral("🗑 Close Workspace: <b style='color:#e74c3c;'>%1</b>").arg(wsName.toHtmlEscaped()), this);
        lblTitle->setTextFormat(Qt::RichText);
        lblTitle->setStyleSheet(QStringLiteral("font-size: 17px; color: #ffffff;"));
        layout->addWidget(lblTitle);

        auto *lblPrompt = new QLabel(m_isPolish
            ? QStringLiteral("Wybierz, co chcesz zrobić z oknami należącymi do tego workspace:")
            : QStringLiteral("Choose what you want to do with the windows in this workspace:"), this);
        lblPrompt->setStyleSheet(QStringLiteral("font-size: 13px; color: rgba(255,255,255,0.75);"));
        layout->addWidget(lblPrompt);

        layout->addSpacing(4);

        // Option 1: Close all windows
        auto *btnClose = createOptionButton(
            QStringLiteral("[1]"),
            m_isPolish ? QStringLiteral("Zamknij wszystkie okna") : QStringLiteral("Close all windows"),
            m_isPolish ? QStringLiteral("Zamyka aplikacje i procesy powiązane z tym obszarem") : QStringLiteral("Terminates applications and windows in this workspace"),
            QColor(231, 76, 60),
            [this]() { choose(CloseWindows); }
        );
        layout->addWidget(btnClose);

        // Option 2: Move to Standard (keep open)
        auto *btnStd = createOptionButton(
            QStringLiteral("[2]"),
            m_isPolish ? QStringLiteral("Przenieś do Standard (pozostaw otwarte)") : QStringLiteral("Move to Standard (keep open)"),
            m_isPolish ? QStringLiteral("Okna pozostaną otwarte i widoczne na bieżącym pulpicie") : QStringLiteral("Windows stay open and visible on desktop"),
            QColor(52, 152, 219),
            [this]() { choose(MoveToStandard); }
        );
        layout->addWidget(btnStd);

        // Option 3: Move to Stash
        auto *btnStash = createOptionButton(
            QStringLiteral("[3]"),
            m_isPolish ? QStringLiteral("Przenieś do Schowka (Stash)") : QStringLiteral("Move to Stash"),
            m_isPolish ? QStringLiteral("Okna zostaną ukryte w Schowku do późniejszego użycia") : QStringLiteral("Windows are hidden in Stash for later retrieval"),
            QColor(46, 204, 113),
            [this]() { choose(MoveToStash); }
        );
        layout->addWidget(btnStash);

        layout->addStretch(1);

        // Cancel button
        auto *bottomLayout = new QHBoxLayout();
        bottomLayout->addStretch(1);
        auto *btnCancel = new QPushButton(m_isPolish ? QStringLiteral("Anuluj [Esc]") : QStringLiteral("Cancel [Esc]"), this);
        btnCancel->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.1);"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  padding: 8px 20px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(255, 255, 255, 0.2);"
            "}"
        ));
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
        bottomLayout->addWidget(btnCancel);
        layout->addLayout(bottomLayout);
    }

    Action selectedAction() const { return m_selectedAction; }

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape) {
            reject();
            return;
        }
        if (e->key() == Qt::Key_1) {
            choose(CloseWindows);
            return;
        }
        if (e->key() == Qt::Key_2 || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            choose(MoveToStandard);
            return;
        }
        if (e->key() == Qt::Key_3) {
            choose(MoveToStash);
            return;
        }
        QDialog::keyPressEvent(e);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
            m_isDragging = true;
        }
        QDialog::mousePressEvent(e);
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (m_isDragging && (e->buttons() & Qt::LeftButton)) {
            move(e->globalPosition().toPoint() - m_dragPos);
        }
        QDialog::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        m_isDragging = false;
        QDialog::mouseReleaseEvent(e);
    }

private:
    void choose(Action act)
    {
        m_selectedAction = act;
        accept();
    }

    QWidget *createOptionButton(const QString &keyBadge, const QString &title, const QString &desc, const QColor &accent, const std::function<void()> &callback)
    {
        auto *btn = new QPushButton(this);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(%1, %2, %3, 0.12);"
            "  border: 1.5px solid rgba(%1, %2, %3, 0.4);"
            "  border-radius: 10px;"
            "  padding: 8px 12px;"
            "  text-align: left;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(%1, %2, %3, 0.25);"
            "  border-color: rgba(%1, %2, %3, 0.8);"
            "}"
        ).arg(accent.red()).arg(accent.green()).arg(accent.blue()));

        auto *hlayout = new QHBoxLayout(btn);
        hlayout->setContentsMargins(6, 4, 6, 4);
        hlayout->setSpacing(12);

        auto *lblKey = new QLabel(keyBadge, btn);
        lblKey->setStyleSheet(QStringLiteral(
            "background-color: %1; color: #ffffff; font-weight: bold; font-size: 13px; border-radius: 6px; padding: 4px 8px;"
        ).arg(accent.name(QColor::HexRgb)));
        hlayout->addWidget(lblKey);

        auto *vbox = new QVBoxLayout();
        vbox->setSpacing(2);
        auto *lblTitle = new QLabel(title, btn);
        lblTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #ffffff;"));
        vbox->addWidget(lblTitle);

        auto *lblDesc = new QLabel(desc, btn);
        lblDesc->setStyleSheet(QStringLiteral("font-size: 11px; color: rgba(255, 255, 255, 0.65);"));
        vbox->addWidget(lblDesc);

        hlayout->addLayout(vbox, 1);

        connect(btn, &QPushButton::clicked, this, callback);
        return btn;
    }

    QString m_wsName;
    bool m_isPolish;
    Action m_selectedAction;
    QPoint m_dragPos;
    bool m_isDragging = false;
};

class WorkspaceCard : public QFrame
{
public:
    WorkspaceCard(int index, const LeapKWorkspace::WorkspaceInfo &info, bool isAssigned, bool isSelected, bool isPolish, QWidget *parent = nullptr)
        : QFrame(parent), m_index(index), m_info(info), m_isAssigned(isAssigned), m_isSelected(isSelected), m_isPolish(isPolish)
    {
        setObjectName(QStringLiteral("wsCard"));
        setCursor(Qt::PointingHandCursor);
        updateStyle();

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(12);

        // Shortcut key badge [1], [2], etc.
        m_lblKey = new QLabel(QStringLiteral("[%1]").arg(index + 1), this);
        m_lblKey->setStyleSheet(QStringLiteral(
            "background-color: #3498db; color: #ffffff; font-weight: bold; font-size: 14px; border-radius: 8px; padding: 4px 10px;"
        ));
        layout->addWidget(m_lblKey);

        // Color dot
        auto *lblDot = new QLabel(this);
        lblDot->setFixedSize(14, 14);
        lblDot->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 7px; border: 1.5px solid rgba(255,255,255,0.4);")
            .arg(info.color.name(QColor::HexRgb)));
        layout->addWidget(lblDot);

        // Name and details
        auto *vbox = new QVBoxLayout();
        vbox->setSpacing(2);
        auto *lblName = new QLabel(QStringLiteral("<b>%1</b>").arg(info.name.toHtmlEscaped()), this);
        lblName->setStyleSheet(QStringLiteral("font-size: 15px; color: #ffffff; border: none; background: transparent;"));
        lblName->setTextFormat(Qt::RichText);
        vbox->addWidget(lblName);

        const QString ruleDesc = isPolish
            ? QStringLiteral("Reguły: %1 • Utworzono: %2").arg(info.rules.size()).arg(QDateTime::fromMSecsSinceEpoch(info.createdAt).toString(QStringLiteral("yyyy-MM-dd")))
            : QStringLiteral("Rules: %1 • Created: %2").arg(info.rules.size()).arg(QDateTime::fromMSecsSinceEpoch(info.createdAt).toString(QStringLiteral("yyyy-MM-dd")));
        auto *lblDesc = new QLabel(ruleDesc, this);
        lblDesc->setStyleSheet(QStringLiteral("font-size: 12px; color: rgba(255, 255, 255, 0.6); border: none; background: transparent;"));
        vbox->addWidget(lblDesc);
        layout->addLayout(vbox, 1);

        // Status badge
        m_lblStatus = new QLabel(this);
        updateStatusBadge();
        layout->addWidget(m_lblStatus);

        // Delete / Close Workspace button with keyboard shortcut hint
        const QString deleteBtnText = (index < 9)
            ? QStringLiteral("🗑 ⇧%1").arg(index + 1)
            : QStringLiteral("🗑");
        m_btnDelete = new QPushButton(deleteBtnText, this);
        m_btnDelete->setFixedSize((index < 9) ? 58 : 36, 28);
        m_btnDelete->setCursor(Qt::PointingHandCursor);
        const QString delTip = (index < 9)
            ? (m_isPolish ? QStringLiteral("Zamknij / Usuń ten workspace [Shift+%1]").arg(index + 1) : QStringLiteral("Close / Delete this workspace [Shift+%1]").arg(index + 1))
            : (m_isPolish ? QStringLiteral("Zamknij / Usuń ten workspace [Shift+Del]") : QStringLiteral("Close / Delete this workspace [Shift+Del]"));
        m_btnDelete->setToolTip(delTip);
        m_btnDelete->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(231, 76, 60, 0.15);"
            "  color: #e74c3c;"
            "  font-size: 12px;"
            "  font-weight: bold;"
            "  border: 1px solid rgba(231, 76, 60, 0.35);"
            "  border-radius: 6px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(231, 76, 60, 0.4);"
            "  color: #ffffff;"
            "}"
        ));
        connect(m_btnDelete, &QPushButton::clicked, this, [this]() {
            if (onDeleteClicked) {
                onDeleteClicked(m_index);
            }
        });
        layout->addWidget(m_btnDelete);
    }

    void setAssigned(bool assigned)
    {
        m_isAssigned = assigned;
        updateStyle();
        updateStatusBadge();
    }

    void setSelected(bool selected)
    {
        m_isSelected = selected;
        updateStyle();
    }

    bool isAssigned() const { return m_isAssigned; }
    bool isSelected() const { return m_isSelected; }
    LeapKWorkspace::WorkspaceInfo info() const { return m_info; }
    int index() const { return m_index; }

    std::function<void(int)> onClicked;
    std::function<void(int)> onDeleteClicked;

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && onClicked) {
            onClicked(m_index);
        }
        QFrame::mousePressEvent(e);
    }

private:
    void updateStyle()
    {
        if (m_isAssigned) {
            const QString border = m_isSelected
                ? QStringLiteral("2px solid #2ecc71")
                : QStringLiteral("1.5px solid #2ecc71");
            const QString bg = m_isSelected
                ? QStringLiteral("rgba(46, 204, 113, 0.32)")
                : QStringLiteral("rgba(46, 204, 113, 0.22)");
            setStyleSheet(QStringLiteral(
                "QFrame#wsCard {"
                "  background-color: %1;"
                "  border: %2;"
                "  border-radius: 12px;"
                "}"
                "QFrame#wsCard:hover {"
                "  background-color: rgba(46, 204, 113, 0.36);"
                "}"
            ).arg(bg, border));
        } else {
            const QString border = m_isSelected
                ? QStringLiteral("2px solid #3498db")
                : QStringLiteral("1px solid rgba(255, 255, 255, 0.15)");
            const QString bg = m_isSelected
                ? QStringLiteral("rgba(52, 152, 219, 0.22)")
                : QStringLiteral("rgba(255, 255, 255, 0.06)");
            setStyleSheet(QStringLiteral(
                "QFrame#wsCard {"
                "  background-color: %1;"
                "  border: %2;"
                "  border-radius: 12px;"
                "}"
                "QFrame#wsCard:hover {"
                "  background-color: rgba(255, 255, 255, 0.14);"
                "  border-color: rgba(255, 255, 255, 0.4);"
                "}"
            ).arg(bg, border));
        }
    }

    void updateStatusBadge()
    {
        if (m_isAssigned) {
            m_lblStatus->setText(m_isPolish ? QStringLiteral("✓ Przypisane") : QStringLiteral("✓ Assigned"));
            m_lblStatus->setStyleSheet(QStringLiteral(
                "background-color: #2ecc71; color: #ffffff; font-weight: bold; font-size: 12px; border-radius: 6px; padding: 4px 10px;"
            ));
        } else {
            m_lblStatus->setText(m_isPolish ? QStringLiteral("+ Dołącz") : QStringLiteral("+ Assign"));
            m_lblStatus->setStyleSheet(QStringLiteral(
                "background-color: rgba(255, 255, 255, 0.12); color: rgba(255, 255, 255, 0.7); font-size: 12px; border-radius: 6px; padding: 4px 8px;"
            ));
        }
    }

    int m_index;
    LeapKWorkspace::WorkspaceInfo m_info;
    bool m_isAssigned;
    bool m_isSelected = false;
    bool m_isPolish;
    QLabel *m_lblKey = nullptr;
    QLabel *m_lblStatus = nullptr;
    QPushButton *m_btnDelete = nullptr;
};

class WorkspaceManagerDialog : public QDialog
{
public:
    WorkspaceManagerDialog(const QString &appClass,
                           const QString &windowCaption,
                           int desktopNum,
                           const QString &desktopName,
                           const QString &assignedWsId,
                           const QList<LeapKWorkspace::WorkspaceInfo> &workspaces,
                           QWidget *parent = nullptr)
        : QDialog(parent)
        , m_appClass(appClass)
        , m_windowCaption(windowCaption)
        , m_desktopNum(desktopNum)
        , m_desktopName(desktopName)
        , m_assignedWsId(assignedWsId)
        , m_workspaces(workspaces)
        , m_isPolish(QLocale().language() == QLocale::Polish)
        , m_groupByApp(true)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        setAttribute(Qt::WA_TranslucentBackground, true);
        resize(580, 530);

        m_selectedCardIndex = -1;
        for (int i = 0; i < m_workspaces.size(); ++i) {
            if (m_workspaces.at(i).id == m_assignedWsId) {
                m_selectedCardIndex = i;
                break;
            }
        }
        if (m_selectedCardIndex < 0 && !m_workspaces.isEmpty()) {
            m_selectedCardIndex = 0;
        }

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);

        m_mainFrame = new MainWorkspaceFrame(this);
        rootLayout->addWidget(m_mainFrame);

        auto *mainLayout = new QVBoxLayout(m_mainFrame);
        mainLayout->setContentsMargins(20, 18, 20, 18);
        mainLayout->setSpacing(14);

        // Header
        auto *headerLayout = new QVBoxLayout();
        headerLayout->setSpacing(4);

        auto *lblTitle = new QLabel(m_isPolish ? QStringLiteral("📁 Zarządzanie Przestrzeniami (Workspaces)") : QStringLiteral("📁 Workspace Manager"), this);
        lblTitle->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #ffffff;"));
        headerLayout->addWidget(lblTitle);

        const QString subText = m_isPolish
            ? QStringLiteral("Bieżące okno: <b style='color:#3498db;'>%1</b> &bull; <span style='color:rgba(255,255,255,0.7);'>%2</span>")
                .arg(m_appClass.isEmpty() ? QStringLiteral("Brak klasy") : m_appClass.toHtmlEscaped(), m_windowCaption.left(35).toHtmlEscaped())
            : QStringLiteral("Current window: <b style='color:#3498db;'>%1</b> &bull; <span style='color:rgba(255,255,255,0.7);'>%2</span>")
                .arg(m_appClass.isEmpty() ? QStringLiteral("Unknown") : m_appClass.toHtmlEscaped(), m_windowCaption.left(35).toHtmlEscaped());
        auto *lblSub = new QLabel(subText, this);
        lblSub->setTextFormat(Qt::RichText);
        lblSub->setStyleSheet(QStringLiteral("font-size: 13px; color: rgba(255, 255, 255, 0.85);"));
        headerLayout->addWidget(lblSub);
        mainLayout->addLayout(headerLayout);

        // Scope Switcher Capsule (All App vs Single Window)
        auto *scopeLayout = new QHBoxLayout();
        scopeLayout->setSpacing(10);
        auto *lblScopeTitle = new QLabel(m_isPolish ? QStringLiteral("Zakres reguły:") : QStringLiteral("Rule Scope:"), this);
        lblScopeTitle->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 0.7); font-size: 13px; font-weight: bold;"));
        scopeLayout->addWidget(lblScopeTitle);

        m_btnScopeApp = new QPushButton(m_isPolish ? QStringLiteral("[A] Cała aplikacja") : QStringLiteral("[A] Entire App"), this);
        m_btnScopeApp->setCheckable(true);
        m_btnScopeApp->setChecked(m_groupByApp);
        connect(m_btnScopeApp, &QPushButton::clicked, this, [this]() { setScope(true); });
        scopeLayout->addWidget(m_btnScopeApp);

        m_btnScopeSingle = new QPushButton(m_isPolish ? QStringLiteral("[D] Tylko to okno") : QStringLiteral("[D] Single Window"), this);
        m_btnScopeSingle->setCheckable(true);
        m_btnScopeSingle->setChecked(!m_groupByApp);
        connect(m_btnScopeSingle, &QPushButton::clicked, this, [this]() { setScope(false); });
        scopeLayout->addWidget(m_btnScopeSingle);
        scopeLayout->addStretch(1);

        updateScopeButtons();
        mainLayout->addLayout(scopeLayout);

        // Scroll area with workspace cards
        m_scrollArea = new QScrollArea(this);
        m_scrollArea->setWidgetResizable(true);
        m_scrollArea->setStyleSheet(QStringLiteral(
            "QScrollArea { border: none; background: transparent; }"
            "QScrollBar:vertical { width: 8px; background: transparent; }"
            "QScrollBar::handle:vertical { background: rgba(255,255,255,0.2); border-radius: 4px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        ));

        m_cardsContainer = new QWidget(m_scrollArea);
        m_cardsContainer->setStyleSheet(QStringLiteral("background: transparent;"));
        m_cardsLayout = new QVBoxLayout(m_cardsContainer);
        m_cardsLayout->setContentsMargins(0, 0, 0, 0);
        m_cardsLayout->setSpacing(8);

        rebuildCards();
        m_scrollArea->setWidget(m_cardsContainer);
        mainLayout->addWidget(m_scrollArea, 1);

        // New Workspace Button Cards
        auto *newWsLayout = new QHBoxLayout();
        newWsLayout->setSpacing(10);

        m_btnNewWs = new QPushButton(m_isPolish ? QStringLiteral("➕ [N] Utwórz z tym oknem") : QStringLiteral("➕ [N] Create with window"), this);
        m_btnNewWs->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(52, 152, 219, 0.15);"
            "  border: 1.5px dashed #3498db;"
            "  border-radius: 12px;"
            "  color: #3498db;"
            "  font-weight: bold;"
            "  font-size: 13px;"
            "  padding: 10px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(52, 152, 219, 0.3);"
            "}"
        ));
        connect(m_btnNewWs, &QPushButton::clicked, this, &WorkspaceManagerDialog::onNewWorkspace);
        newWsLayout->addWidget(m_btnNewWs);

        m_btnNewEmptyWs = new QPushButton(m_isPolish ? QStringLiteral("✨ [E] Utwórz pusty Workspace") : QStringLiteral("✨ [E] Create Empty Workspace"), this);
        m_btnNewEmptyWs->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(46, 204, 113, 0.15);"
            "  border: 1.5px dashed #2ecc71;"
            "  border-radius: 12px;"
            "  color: #2ecc71;"
            "  font-weight: bold;"
            "  font-size: 13px;"
            "  padding: 10px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(46, 204, 113, 0.3);"
            "}"
        ));
        connect(m_btnNewEmptyWs, &QPushButton::clicked, this, &WorkspaceManagerDialog::onNewEmptyWorkspace);
        newWsLayout->addWidget(m_btnNewEmptyWs);

        mainLayout->addLayout(newWsLayout);

        // Bottom Action Bar
        auto *bottomLayout = new QHBoxLayout();
        bottomLayout->setSpacing(10);

        m_btnUnassign = new QPushButton(m_isPolish ? QStringLiteral("🗑 Odepnij [Del]") : QStringLiteral("🗑 Unassign [Del]"), this);
        m_btnUnassign->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(231, 76, 60, 0.12);"
            "  border: 1px solid rgba(231, 76, 60, 0.35);"
            "  border-radius: 8px;"
            "  color: #e74c3c;"
            "  font-weight: bold;"
            "  padding: 8px 14px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(231, 76, 60, 0.25);"
            "}"
        ));
        connect(m_btnUnassign, &QPushButton::clicked, this, &WorkspaceManagerDialog::onUnassign);
        bottomLayout->addWidget(m_btnUnassign);

        m_btnDeleteWs = new QPushButton(m_isPolish ? QStringLiteral("❌ Usuń workspace [Shift+Del]") : QStringLiteral("❌ Delete workspace [Shift+Del]"), this);
        m_btnDeleteWs->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(231, 76, 60, 0.15);"
            "  border: 1px solid rgba(231, 76, 60, 0.4);"
            "  border-radius: 8px;"
            "  color: #e74c3c;"
            "  font-weight: bold;"
            "  padding: 8px 14px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(231, 76, 60, 0.3);"
            "}"
        ));
        connect(m_btnDeleteWs, &QPushButton::clicked, this, &WorkspaceManagerDialog::deleteSelectedOrAssignedWorkspace);
        bottomLayout->addWidget(m_btnDeleteWs);
        bottomLayout->addStretch(1);

        auto *btnCancel = new QPushButton(m_isPolish ? QStringLiteral("Anuluj [Esc]") : QStringLiteral("Cancel [Esc]"), this);
        btnCancel->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.1);"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  padding: 8px 16px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(255, 255, 255, 0.2);"
            "}"
        ));
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
        bottomLayout->addWidget(btnCancel);

        auto *btnSave = new QPushButton(m_isPolish ? QStringLiteral("Gotowe [Enter]") : QStringLiteral("Done [Enter]"), this);
        btnSave->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: #2ecc71;"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  font-weight: bold;"
            "  padding: 8px 24px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #27ae60;"
            "}"
        ));
        connect(btnSave, &QPushButton::clicked, this, &QDialog::accept);
        bottomLayout->addWidget(btnSave);

        mainLayout->addLayout(bottomLayout);
    }

    QString assignedWorkspaceId() const { return m_assignedWsId; }
    bool groupByApp() const { return m_groupByApp; }
    QList<LeapKWorkspace::WorkspaceInfo> workspaces() const { return m_workspaces; }
    QJsonArray deletedWorkspaces() const { return m_deletedWorkspaces; }

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape) {
            reject();
            return;
        }
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            accept();
            return;
        }
        if (e->key() == Qt::Key_A) {
            setScope(true);
            return;
        }
        if (e->key() == Qt::Key_D) {
            if (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) {
                deleteSelectedOrAssignedWorkspace();
                return;
            }
            setScope(false);
            return;
        }
        if (e->key() == Qt::Key_N && !(e->modifiers() & Qt::ControlModifier)) {
            onNewWorkspace();
            return;
        }
        if (e->key() == Qt::Key_E || ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_N)) {
            onNewEmptyWorkspace();
            return;
        }
        if (e->key() == Qt::Key_X) {
            deleteSelectedOrAssignedWorkspace();
            return;
        }
        if (e->key() == Qt::Key_Down || e->key() == Qt::Key_J) {
            selectNextCard();
            return;
        }
        if (e->key() == Qt::Key_Up || e->key() == Qt::Key_K) {
            selectPrevCard();
            return;
        }
        if (e->key() == Qt::Key_Space) {
            if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_workspaces.size()) {
                toggleWorkspaceAssignment(m_selectedCardIndex);
                return;
            }
        }
        if (e->key() == Qt::Key_Delete) {
            if (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) {
                deleteSelectedOrAssignedWorkspace();
                return;
            }
            onUnassign();
            return;
        }

        // 1..9 shortcuts to select workspace or Shift+1..9 to delete workspace
        int targetWsIdx = -1;
        bool isDeleteAction = false;

        if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_9) {
            targetWsIdx = (e->key() - Qt::Key_1);
            if (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier)) {
                isDeleteAction = true;
            }
        } else if (e->key() == Qt::Key_Exclam)     { targetWsIdx = 0; isDeleteAction = true; }
        else if (e->key() == Qt::Key_At)         { targetWsIdx = 1; isDeleteAction = true; }
        else if (e->key() == Qt::Key_NumberSign) { targetWsIdx = 2; isDeleteAction = true; }
        else if (e->key() == Qt::Key_Dollar)     { targetWsIdx = 3; isDeleteAction = true; }
        else if (e->key() == Qt::Key_Percent)    { targetWsIdx = 4; isDeleteAction = true; }
        else if (e->key() == Qt::Key_AsciiCircum){ targetWsIdx = 5; isDeleteAction = true; }
        else if (e->key() == Qt::Key_Ampersand)  { targetWsIdx = 6; isDeleteAction = true; }
        else if (e->key() == Qt::Key_Asterisk)   { targetWsIdx = 7; isDeleteAction = true; }
        else if (e->key() == Qt::Key_ParenLeft)  { targetWsIdx = 8; isDeleteAction = true; }
        else if (!e->text().isEmpty()) {
            const QString shiftedNums = QStringLiteral("!@#$%^&*(");
            const int symIdx = shiftedNums.indexOf(e->text().at(0));
            if (symIdx >= 0) {
                targetWsIdx = symIdx;
                isDeleteAction = true;
            }
        }

        if (targetWsIdx >= 0 && targetWsIdx < m_workspaces.size()) {
            m_selectedCardIndex = targetWsIdx;
            updateCardSelection();
            if (isDeleteAction) {
                closeWorkspace(targetWsIdx);
            } else {
                toggleWorkspaceAssignment(targetWsIdx);
            }
            return;
        }

        QDialog::keyPressEvent(e);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
            m_isDragging = true;
        }
        QDialog::mousePressEvent(e);
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (m_isDragging && (e->buttons() & Qt::LeftButton)) {
            move(e->globalPosition().toPoint() - m_dragPos);
        }
        QDialog::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        m_isDragging = false;
        QDialog::mouseReleaseEvent(e);
    }

private:
    void setScope(bool byApp)
    {
        m_groupByApp = byApp;
        updateScopeButtons();
    }

    void updateScopeButtons()
    {
        m_btnScopeApp->setChecked(m_groupByApp);
        m_btnScopeSingle->setChecked(!m_groupByApp);

        const QString activeStyle = QStringLiteral(
            "QPushButton { background-color: #3498db; color: #ffffff; font-weight: bold; border-radius: 6px; padding: 6px 14px; border: none; }"
        );
        const QString inactiveStyle = QStringLiteral(
            "QPushButton { background-color: rgba(255,255,255,0.08); color: rgba(255,255,255,0.6); border-radius: 6px; padding: 6px 14px; border: 1px solid rgba(255,255,255,0.15); }"
            "QPushButton:hover { background-color: rgba(255,255,255,0.15); }"
        );
        m_btnScopeApp->setStyleSheet(m_groupByApp ? activeStyle : inactiveStyle);
        m_btnScopeSingle->setStyleSheet(!m_groupByApp ? activeStyle : inactiveStyle);
    }

    void rebuildCards()
    {
        // Clear old cards
        m_cards.clear();
        QLayoutItem *item;
        while ((item = m_cardsLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }

        if (m_selectedCardIndex >= m_workspaces.size()) {
            m_selectedCardIndex = m_workspaces.size() - 1;
        }
        if (m_selectedCardIndex < 0 && !m_workspaces.isEmpty()) {
            m_selectedCardIndex = 0;
        }

        for (int i = 0; i < m_workspaces.size(); ++i) {
            const auto &ws = m_workspaces.at(i);
            const bool isAssigned = (ws.id == m_assignedWsId);
            const bool isSelected = (i == m_selectedCardIndex);
            auto *card = new WorkspaceCard(i, ws, isAssigned, isSelected, m_isPolish, m_cardsContainer);
            card->onClicked = [this](int idx) {
                m_selectedCardIndex = idx;
                updateCardSelection();
                toggleWorkspaceAssignment(idx);
            };
            card->onDeleteClicked = [this](int idx) {
                m_selectedCardIndex = idx;
                updateCardSelection();
                closeWorkspace(idx);
            };
            m_cards.append(card);
            m_cardsLayout->addWidget(card);
        }

        m_cardsLayout->addStretch(1);
    }

    void updateCardSelection()
    {
        for (auto *card : m_cards) {
            if (card) {
                card->setSelected(card->index() == m_selectedCardIndex);
            }
        }
    }

    void selectNextCard()
    {
        if (m_workspaces.isEmpty()) return;
        m_selectedCardIndex = (m_selectedCardIndex + 1) % m_workspaces.size();
        updateCardSelection();
        ensureSelectedVisible();
    }

    void selectPrevCard()
    {
        if (m_workspaces.isEmpty()) return;
        m_selectedCardIndex = (m_selectedCardIndex - 1 + m_workspaces.size()) % m_workspaces.size();
        updateCardSelection();
        ensureSelectedVisible();
    }

    void ensureSelectedVisible()
    {
        if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_cards.size() && m_scrollArea) {
            auto *card = m_cards.at(m_selectedCardIndex);
            if (card) {
                m_scrollArea->ensureWidgetVisible(card);
            }
        }
    }

    void deleteSelectedOrAssignedWorkspace()
    {
        if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_workspaces.size()) {
            closeWorkspace(m_selectedCardIndex);
            return;
        }
        if (!m_assignedWsId.isEmpty()) {
            for (int i = 0; i < m_workspaces.size(); ++i) {
                if (m_workspaces.at(i).id == m_assignedWsId) {
                    closeWorkspace(i);
                    return;
                }
            }
        }
        if (m_workspaces.size() == 1) {
            closeWorkspace(0);
            return;
        }
    }

    void closeWorkspace(int idx)
    {
        if (idx < 0 || idx >= m_workspaces.size()) {
            return;
        }
        const auto ws = m_workspaces.at(idx);
        CloseWorkspaceDialog dlg(ws.name, m_isPolish, this);
        if (dlg.exec() == QDialog::Accepted) {
            const auto act = dlg.selectedAction();
            QString actStr = QStringLiteral("standard");
            if (act == CloseWorkspaceDialog::CloseWindows) {
                actStr = QStringLiteral("close");
            } else if (act == CloseWorkspaceDialog::MoveToStash) {
                actStr = QStringLiteral("stash");
            }

            QJsonObject delInfo;
            delInfo[QStringLiteral("id")] = ws.id;
            delInfo[QStringLiteral("name")] = ws.name;
            delInfo[QStringLiteral("action")] = actStr;
            m_deletedWorkspaces.append(delInfo);

            if (m_assignedWsId == ws.id) {
                m_assignedWsId.clear();
            }

            m_workspaces.removeAt(idx);
            LeapKWorkspace::saveWorkspaces(m_workspaces);
            rebuildCards();
        }
    }

    void toggleWorkspaceAssignment(int idx)
    {
        if (idx < 0 || idx >= m_workspaces.size()) {
            return;
        }
        const QString targetId = m_workspaces.at(idx).id;
        if (m_assignedWsId == targetId) {
            // Already assigned: toggle unassign
            m_assignedWsId.clear();
        } else {
            // Assign to this workspace
            m_assignedWsId = targetId;
        }

        // Update persistent rules on the workspace
        applyRulesToWorkspaces();
        rebuildCards();
    }

    void applyRulesToWorkspaces()
    {
        // Remove current window from all workspace rules
        for (auto &ws : m_workspaces) {
            for (int i = ws.rules.size() - 1; i >= 0; --i) {
                const auto &r = ws.rules.at(i);
                if (r.matchApp.compare(m_appClass, Qt::CaseInsensitive) == 0) {
                    ws.rules.removeAt(i);
                }
            }
        }

        // If assigned, add rule to selected workspace
        if (!m_assignedWsId.isEmpty()) {
            for (auto &ws : m_workspaces) {
                if (ws.id == m_assignedWsId) {
                    LeapKWorkspace::WindowMatchRule r;
                    r.matchApp = m_appClass;
                    if (!m_groupByApp) {
                        r.matchCaption = m_windowCaption;
                    }
                    ws.rules.append(r);
                    break;
                }
            }
        }
    }

    void onNewWorkspace()
    {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this,
            m_isPolish ? QStringLiteral("Nowa przestrzeń robocza") : QStringLiteral("New Workspace"),
            m_isPolish ? QStringLiteral("Wpisz nazwę dla nowego Workspace (np. Backend, Frontend, Docs):") : QStringLiteral("Enter a name for the new Workspace:"),
            QLineEdit::Normal,
            QString(),
            &ok
        );

        if (!ok || name.trimmed().isEmpty()) {
            return;
        }

        static const QList<QColor> s_colors = {
            QColor(52, 152, 219),  // Blue
            QColor(46, 204, 113),  // Green
            QColor(155, 89, 182),  // Purple
            QColor(230, 126, 34),  // Orange
            QColor(241, 196, 15),  // Yellow
            QColor(231, 76, 60),   // Red
            QColor(26, 188, 156)   // Turquoise
        };

        LeapKWorkspace::WorkspaceInfo ws;
        ws.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ws.name = name.trimmed();
        ws.color = s_colors.at(m_workspaces.size() % s_colors.size());
        ws.createdAt = QDateTime::currentMSecsSinceEpoch();

        m_workspaces.append(ws);

        // Immediately assign current window to the newly created workspace
        m_assignedWsId = ws.id;
        applyRulesToWorkspaces();
        rebuildCards();
    }

    void onNewEmptyWorkspace()
    {
        bool ok = false;
        const QString defaultName = m_isPolish
            ? QStringLiteral("Obszar %1").arg(m_workspaces.size() + 1)
            : QStringLiteral("Workspace %1").arg(m_workspaces.size() + 1);

        const QString name = QInputDialog::getText(
            this,
            m_isPolish ? QStringLiteral("Nowy pusty workspace") : QStringLiteral("New Empty Workspace"),
            m_isPolish ? QStringLiteral("Wpisz nazwę dla nowego pustego Workspace (np. Backend, Frontend, Docs):") : QStringLiteral("Enter a name for the new empty Workspace:"),
            QLineEdit::Normal,
            defaultName,
            &ok
        );

        if (!ok || name.trimmed().isEmpty()) {
            return;
        }

        LeapKWorkspace::createEmptyWorkspace(m_workspaces, name.trimmed());
        rebuildCards();
    }

    void onUnassign()
    {
        m_assignedWsId.clear();
        applyRulesToWorkspaces();
        rebuildCards();
    }

    QString m_appClass;
    QString m_windowCaption;
    int m_desktopNum;
    QString m_desktopName;
    QString m_assignedWsId;
    QList<LeapKWorkspace::WorkspaceInfo> m_workspaces;
    bool m_isPolish;
    bool m_groupByApp;

    QPoint m_dragPos;
    bool m_isDragging = false;

    MainWorkspaceFrame *m_mainFrame = nullptr;
    QPushButton *m_btnScopeApp = nullptr;
    QPushButton *m_btnScopeSingle = nullptr;
    QWidget *m_cardsContainer = nullptr;
    QVBoxLayout *m_cardsLayout = nullptr;
    QPushButton *m_btnNewWs = nullptr;
    QPushButton *m_btnNewEmptyWs = nullptr;
    QPushButton *m_btnUnassign = nullptr;
    QPushButton *m_btnDeleteWs = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    int m_selectedCardIndex = -1;
    QList<WorkspaceCard *> m_cards;
    QJsonArray m_deletedWorkspaces;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    if (argc > 1 && QString::fromUtf8(argv[1]) == QLatin1String("--prompt-new-workspace")) {
        const bool isPolish = (QLocale().language() == QLocale::Polish);
        QList<LeapKWorkspace::WorkspaceInfo> workspaces = LeapKWorkspace::loadWorkspaces();
        const int nextNum = workspaces.size() + 1;
        const QString defaultName = isPolish
            ? QStringLiteral("Obszar %1").arg(nextNum)
            : QStringLiteral("Workspace %1").arg(nextNum);

        bool ok = false;
        const QString name = QInputDialog::getText(
            nullptr,
            isPolish ? QStringLiteral("Nowy Workspace") : QStringLiteral("New Workspace"),
            isPolish ? QStringLiteral("Wpisz nazwę dla nowego Workspace:") : QStringLiteral("Enter a name for the new Workspace:"),
            QLineEdit::Normal,
            defaultName,
            &ok
        );

        if (!ok || name.trimmed().isEmpty()) {
            return 1;
        }

        const auto ws = LeapKWorkspace::createEmptyWorkspace(workspaces, name.trimmed());
        QJsonObject resp;
        resp[QStringLiteral("action")] = QStringLiteral("createWorkspace");
        resp[QStringLiteral("newWorkspaceId")] = ws.id;
        resp[QStringLiteral("newWorkspaceName")] = ws.name;
        std::cout << QJsonDocument(resp).toJson(QJsonDocument::Compact).toStdString() << std::endl;
        return 0;
    }

    if (argc > 1 && QString::fromUtf8(argv[1]) == QLatin1String("--create-empty")) {
        QString name = (argc > 2 && argv[2]) ? QString::fromUtf8(argv[2]).trimmed() : QString();
        QList<LeapKWorkspace::WorkspaceInfo> workspaces = LeapKWorkspace::loadWorkspaces();
        const auto ws = LeapKWorkspace::createEmptyWorkspace(workspaces, name);
        std::cout << "Created empty workspace: " << ws.name.toStdString() << " (" << ws.id.toStdString() << ")" << std::endl;
        return 0;
    }

    if (argc > 1 && QString::fromUtf8(argv[1]) == QLatin1String("--close-workspace")) {
        const QString targetWsId = (argc > 2 && argv[2]) ? QString::fromUtf8(argv[2]).trimmed() : QString();
        QList<LeapKWorkspace::WorkspaceInfo> workspaces = LeapKWorkspace::loadWorkspaces();
        int foundIdx = -1;
        for (int i = 0; i < workspaces.size(); ++i) {
            if (workspaces.at(i).id == targetWsId) {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx < 0) {
            std::cerr << "Workspace not found: " << targetWsId.toStdString() << std::endl;
            return 1;
        }

        const auto ws = workspaces.at(foundIdx);
        const bool isPolish = (QLocale().language() == QLocale::Polish);
        CloseWorkspaceDialog dlg(ws.name, isPolish);
        if (dlg.exec() == QDialog::Accepted) {
            const auto act = dlg.selectedAction();
            QString actStr = QStringLiteral("standard");
            if (act == CloseWorkspaceDialog::CloseWindows) {
                actStr = QStringLiteral("close");
            } else if (act == CloseWorkspaceDialog::MoveToStash) {
                actStr = QStringLiteral("stash");
            }

            workspaces.removeAt(foundIdx);
            LeapKWorkspace::saveWorkspaces(workspaces);

            QJsonObject resp;
            resp[QStringLiteral("action")] = QStringLiteral("deleteWorkspace");
            resp[QStringLiteral("deletedWorkspaceId")] = ws.id;
            resp[QStringLiteral("deletedWorkspaceName")] = ws.name;
            resp[QStringLiteral("windowAction")] = actStr;

            QJsonArray delArr;
            QJsonObject delInfo;
            delInfo[QStringLiteral("id")] = ws.id;
            delInfo[QStringLiteral("name")] = ws.name;
            delInfo[QStringLiteral("action")] = actStr;
            delArr.append(delInfo);
            resp[QStringLiteral("deletedWorkspaces")] = delArr;

            std::cout << QJsonDocument(resp).toJson(QJsonDocument::Compact).toStdString() << std::endl;
            return 0;
        }
        return 1;
    }

    QString appClass;
    QString windowCaption;
    int desktopNum = 0;
    QString desktopName;
    QString assignedWsId;

    if (argc > 1 && argv[1]) appClass = QString::fromUtf8(argv[1]).trimmed();
    if (argc > 2 && argv[2]) windowCaption = QString::fromUtf8(argv[2]).trimmed();
    if (argc > 3 && argv[3]) desktopNum = QString::fromUtf8(argv[3]).toInt();
    if (argc > 4 && argv[4]) desktopName = QString::fromUtf8(argv[4]).trimmed();
    if (argc > 5 && argv[5]) assignedWsId = QString::fromUtf8(argv[5]).trimmed();

    QList<LeapKWorkspace::WorkspaceInfo> workspaces = LeapKWorkspace::loadWorkspaces();

    WorkspaceManagerDialog dlg(appClass, windowCaption, desktopNum, desktopName, assignedWsId, workspaces);
    if (dlg.exec() == QDialog::Accepted) {
        // Save workspaces permanently
        const auto updatedList = dlg.workspaces();
        LeapKWorkspace::saveWorkspaces(updatedList);

        QJsonObject resp;
        resp[QStringLiteral("action")] = dlg.assignedWorkspaceId().isEmpty() ? QStringLiteral("unassign") : QStringLiteral("assign");
        resp[QStringLiteral("assignedWorkspaceId")] = dlg.assignedWorkspaceId();
        resp[QStringLiteral("groupByApp")] = dlg.groupByApp();

        QJsonArray arr;
        for (const auto &ws : updatedList) {
            arr.append(LeapKWorkspace::workspaceToJson(ws));
        }
        resp[QStringLiteral("workspaces")] = arr;
        resp[QStringLiteral("deletedWorkspaces")] = dlg.deletedWorkspaces();

        std::cout << QJsonDocument(resp).toJson(QJsonDocument::Compact).toStdString() << std::endl;
        return 0;
    }

    return 1;
}
