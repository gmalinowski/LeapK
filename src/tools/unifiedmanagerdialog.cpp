/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "unifiedmanagerdialog.h"
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QCursor>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QHelpEvent>
#include <iostream>

static const char *s_presetColors[] = {
    "#3498db", // Blue
    "#2ecc71", // Green
    "#e74c3c", // Red
    "#f39c12", // Orange
    "#9b59b6", // Purple
    "#1abc9c", // Turquoise
    "#e67e22", // Carrot
    "#e91e63", // Pink
};

// =============================================================================
// MainUnifiedFrame
// =============================================================================
MainUnifiedFrame::MainUnifiedFrame(const LeapKTheme::Theme &theme, QWidget *parent)
    : QFrame(parent), m_theme(theme), m_title(QStringLiteral("LeapK Manager"))
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void MainUnifiedFrame::setHeaderTitle(const QString &title)
{
    m_title = title;
    update();
}

void MainUnifiedFrame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    LeapKTheme::paintThemedWindowFrame(&p, rect(), m_theme);
}

// =============================================================================
// InWindowToolTip
// =============================================================================
InWindowToolTip::InWindowToolTip(QWidget *parent)
    : QFrame(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setStyleSheet(QStringLiteral(
        "InWindowToolTip {"
        "  background-color: rgba(18, 22, 30, 0.96);"
        "  border: 1.5px solid rgba(82, 169, 255, 0.55);"
        "  border-radius: 6px;"
        "}"
    ));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 5, 8, 5);
    m_label = new QLabel(this);
    m_label->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 11px; font-weight: normal; background: transparent; border: none;"));
    m_label->setTextFormat(Qt::RichText);
    layout->addWidget(m_label);
    hide();
}

void InWindowToolTip::showTip(const QString &text, const QPoint &globalPos, QWidget *container)
{
    if (text.trimmed().isEmpty() || !container) {
        hide();
        return;
    }
    m_lastGlobalPos = globalPos;
    m_label->setText(text);
    adjustSize();

    const QPoint localPos = container->mapFromGlobal(globalPos);
    const int tipW = width();
    const int tipH = height();

    int x = localPos.x() + 12;
    int y = localPos.y() + 18;

    if (x + tipW > container->width() - 10) {
        x = localPos.x() - tipW - 8;
    }
    if (x < 10) x = 10;

    if (y + tipH > container->height() - 10) {
        y = localPos.y() - tipH - 8;
    }
    if (y < 10) y = 10;

    move(x, y);
    raise();
    show();
}

// =============================================================================
// CloseWorkspaceDialog
// =============================================================================
CloseWorkspaceDialog::CloseWorkspaceDialog(const QString &wsName, const LeapKTheme::Theme &theme, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Close Workspace"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(480, 260);

    const QColor accent = theme.primaryAccentColor();
    const QColor textPrim = theme.textPrimaryColor();
    const int btnRadius = qBound(4, theme.cardCornerRadius(), 8);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto *frame = new MainUnifiedFrame(theme, this);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);

    auto *lblTitle = new QLabel(QStringLiteral("<b>Close Workspace: <span style='color:%1;'>%2</span></b>")
        .arg(accent.name(), wsName.toHtmlEscaped()), frame);
    lblTitle->setTextFormat(Qt::RichText);
    lblTitle->setStyleSheet(QStringLiteral("font-size: 16px; color: %1; border: none; background: transparent;").arg(textPrim.name()));
    layout->addWidget(lblTitle);

    auto *lblDesc = new QLabel(QStringLiteral("What would you like to do with the windows currently open in this workspace?"), frame);
    lblDesc->setWordWrap(true);
    lblDesc->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 0.75); font-size: 13px; border: none; background: transparent;"));
    layout->addWidget(lblDesc);

    auto *btnKeep = new QPushButton(QStringLiteral("Leave windows on current desktop"), frame);
    btnKeep->setFocusPolicy(Qt::StrongFocus);
    btnKeep->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.08); color: #ffffff; border-radius: %1px; padding: 10px; font-weight: bold; border: 2px solid rgba(255,255,255,0.2); }"
        "QPushButton:hover { background: rgba(%2,%3,%4,0.3); border-color: %5; }"
        "QPushButton:focus { border: 2px solid #ffffff; background: rgba(%2,%3,%4,0.45); outline: none; }"
    ).arg(QString::number(btnRadius), QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue()), accent.name()));
    connect(btnKeep, &QPushButton::clicked, this, [this]() {
        m_action = KeepWindows;
        accept();
    });
    layout->addWidget(btnKeep);

    auto *btnStash = new QPushButton(QStringLiteral("Move windows to Stash"), frame);
    btnStash->setFocusPolicy(Qt::StrongFocus);
    btnStash->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(%1,%2,%3,0.15); color: %4; border-radius: %5px; padding: 10px; font-weight: bold; border: 2px dashed %4; }"
        "QPushButton:hover { background: rgba(%1,%2,%3,0.35); color: #ffffff; }"
        "QPushButton:focus { border: 2px solid #ffffff; background: rgba(%1,%2,%3,0.5); color: #ffffff; outline: none; }"
    ).arg(QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue()), accent.name(), QString::number(btnRadius)));
    connect(btnStash, &QPushButton::clicked, this, [this]() {
        m_action = MoveToStash;
        accept();
    });
    layout->addWidget(btnStash);

    auto *btnCloseWins = new QPushButton(QStringLiteral("Close all windows"), frame);
    btnCloseWins->setFocusPolicy(Qt::StrongFocus);
    btnCloseWins->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(231,76,60,0.15); color: #e74c3c; border-radius: %1px; padding: 10px; font-weight: bold; border: 2px solid rgba(231,76,60,0.4); }"
        "QPushButton:hover { background: rgba(231,76,60,0.35); color: #ffffff; }"
        "QPushButton:focus { border: 2px solid #ffffff; background: rgba(231,76,60,0.5); color: #ffffff; outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(btnCloseWins, &QPushButton::clicked, this, [this]() {
        m_action = CloseWindows;
        accept();
    });
    layout->addWidget(btnCloseWins);

    auto *btnCancel = new QPushButton(QStringLiteral("Cancel [Esc]"), frame);
    btnCancel->setFocusPolicy(Qt::StrongFocus);
    btnCancel->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: rgba(255,255,255,0.6); border: 2px solid transparent; border-radius: %1px; padding: 6px 14px; }"
        "QPushButton:hover { color: #ffffff; background: rgba(255,255,255,0.1); }"
        "QPushButton:focus { border: 2px solid #52a9ff; color: #ffffff; background: rgba(82,169,255,0.22); outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(btnCancel, 0, Qt::AlignCenter);

    rootLayout->addWidget(frame);
}

void CloseWorkspaceDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (auto *btn = qobject_cast<QAbstractButton *>(focusWidget())) {
            if (btn->isEnabled()) {
                btn->animateClick();
                return;
            }
        }
    }
    QDialog::keyPressEvent(e);
}

// =============================================================================
// EditWorkspaceDialog
// =============================================================================
EditWorkspaceDialog::EditWorkspaceDialog(const LeapKWorkspace::WorkspaceInfo &info, const LeapKTheme::Theme &theme, QWidget *parent)
    : QDialog(parent), m_color(info.color.isValid() ? info.color : theme.primaryAccentColor()), m_selectedSwatchIndex(0)
{
    setWindowTitle(QStringLiteral("Edit Workspace"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(520, 380);

    const QColor accent = theme.primaryAccentColor();
    const QColor textPrim = theme.textPrimaryColor();
    const int btnRadius = qBound(4, theme.cardCornerRadius(), 8);

    // Initial palette of 8 distinct harmonic colors
    for (int i = 0; i < 8; ++i) {
        m_currentPalette.append(QColor(s_presetColors[i]));
    }
    if (info.color.isValid()) {
        int foundIdx = -1;
        for (int i = 0; i < m_currentPalette.size(); ++i) {
            if (m_currentPalette[i] == info.color) {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx >= 0) {
            m_selectedSwatchIndex = foundIdx;
        } else {
            m_currentPalette[0] = info.color;
            m_selectedSwatchIndex = 0;
        }
    }
    m_color = m_currentPalette[m_selectedSwatchIndex];

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto *frame = new MainUnifiedFrame(theme, this);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    auto *lblTitle = new QLabel(QStringLiteral("<b>Workspace Configuration</b>"), frame);
    lblTitle->setStyleSheet(QStringLiteral("font-size: 16px; color: %1; border: none; background: transparent;").arg(textPrim.name()));
    layout->addWidget(lblTitle);

    auto *nameShortcutRow = new QHBoxLayout();
    nameShortcutRow->setSpacing(12);

    auto *nameCol = new QVBoxLayout();
    nameCol->setSpacing(4);
    auto *lblName = new QLabel(QStringLiteral("Workspace Name:"), frame);
    lblName->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.8); font-size: 12px; border: none; background: transparent;"));
    nameCol->addWidget(lblName);

    m_editName = new QLineEdit(info.name, frame);
    m_editName->setFocusPolicy(Qt::StrongFocus);
    m_editName->setStyleSheet(QStringLiteral(
        "QLineEdit { background: rgba(0,0,0,0.35); color: #ffffff; border: 2px solid rgba(255,255,255,0.25); border-radius: %1px; padding: 8px 12px; font-size: 14px; }"
        "QLineEdit:focus { border: 2px solid %2; background: rgba(0,0,0,0.55); outline: none; }"
    ).arg(QString::number(btnRadius), accent.name()));
    m_editName->selectAll();
    nameCol->addWidget(m_editName);
    nameShortcutRow->addLayout(nameCol, 4);

    auto *shortcutCol = new QVBoxLayout();
    shortcutCol->setSpacing(4);
    auto *lblShortcut = new QLabel(QStringLiteral("Jump Key (HUD):"), frame);
    lblShortcut->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.8); font-size: 12px; border: none; background: transparent;"));
    shortcutCol->addWidget(lblShortcut);

    m_editShortcutKey = new QLineEdit(!info.shortcutKey.isNull() ? QString(info.shortcutKey.toUpper()) : QString(), frame);
    m_editShortcutKey->setMaxLength(1);
    m_editShortcutKey->setAlignment(Qt::AlignCenter);
    m_editShortcutKey->setPlaceholderText(QStringLiteral("–"));
    m_editShortcutKey->setFocusPolicy(Qt::StrongFocus);
    m_editShortcutKey->setToolTip(QStringLiteral("Optional single letter or digit (e.g. 'B', '1') to jump directly to this workspace in HUD"));
    m_editShortcutKey->setStyleSheet(QStringLiteral(
        "QLineEdit { background: rgba(0,0,0,0.35); color: #52a9ff; font-weight: bold; border: 2px solid rgba(255,255,255,0.25); border-radius: %1px; padding: 8px 12px; font-size: 14px; }"
        "QLineEdit:focus { border: 2px solid #52a9ff; background: rgba(0,0,0,0.55); outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(m_editShortcutKey, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (!text.isEmpty()) {
            const QChar ch = text.at(0).toUpper();
            if (ch.isLetterOrNumber()) {
                if (text != QString(ch)) {
                    m_editShortcutKey->setText(QString(ch));
                }
            } else {
                m_editShortcutKey->clear();
            }
        }
    });
    shortcutCol->addWidget(m_editShortcutKey);
    nameShortcutRow->addLayout(shortcutCol, 1);

    layout->addLayout(nameShortcutRow);

    auto *lblColRow = new QHBoxLayout();
    lblColRow->setSpacing(8);
    auto *lblCol = new QLabel(QStringLiteral("Accent Color:"), frame);
    lblCol->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.8); font-size: 12px; border: none; background: transparent;"));
    lblColRow->addWidget(lblCol);

    m_colorPreview = new QWidget(frame);
    m_colorPreview->setFixedSize(22, 22);
    updateColorPreview();
    lblColRow->addWidget(m_colorPreview);
    lblColRow->addStretch(1);
    layout->addLayout(lblColRow);

    // Swatches container
    m_paletteContainer = new QWidget(frame);
    m_paletteContainer->setFocusPolicy(Qt::StrongFocus);
    m_paletteContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *paletteLayout = new QHBoxLayout(m_paletteContainer);
    paletteLayout->setContentsMargins(0, 0, 0, 0);
    paletteLayout->setSpacing(8);

    static const char letters[] = "ABCDEFGH";
    for (int i = 0; i < 8; ++i) {
        auto *btn = new QPushButton(QString(QLatin1Char(letters[i])), m_paletteContainer);
        btn->setFixedSize(36, 36);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(QStringLiteral("Select color [%1]").arg(QLatin1Char(letters[i])));
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            selectColorIndex(i);
        });
        paletteLayout->addWidget(btn);
        m_swatchButtons.append(btn);
    }
    updateSwatchStyles();

    m_btnRandom = new QPushButton(QStringLiteral("[R] Randomize"), frame);
    m_btnRandom->setFixedHeight(36);
    m_btnRandom->setCursor(Qt::PointingHandCursor);
    m_btnRandom->setFocusPolicy(Qt::StrongFocus);
    m_btnRandom->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.12); color: #ffffff; border-radius: %1px; border: 1.5px solid rgba(255,255,255,0.25); font-size: 11px; font-weight: bold; padding: 0 10px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.22); border-color: #ffffff; }"
        "QPushButton:focus { border: 2px solid #52a9ff; outline: none; background: rgba(82,169,255,0.25); }"
    ).arg(QString::number(btnRadius)));
    m_btnRandom->setToolTip(QStringLiteral("Generate fresh harmonious colors [R]"));
    connect(m_btnRandom, &QPushButton::clicked, this, &EditWorkspaceDialog::randomizeColors);
    paletteLayout->addWidget(m_btnRandom);

    m_btnCustom = new QPushButton(QStringLiteral("Custom..."), frame);
    m_btnCustom->setFixedHeight(36);
    m_btnCustom->setCursor(Qt::PointingHandCursor);
    m_btnCustom->setFocusPolicy(Qt::StrongFocus);
    m_btnCustom->setToolTip(QStringLiteral("Open full RGB color picker"));
    m_btnCustom->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.1); color: rgba(255,255,255,0.9); border-radius: %1px; border: 1.5px solid rgba(255,255,255,0.2); font-size: 11px; padding: 0 10px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.2); color: #ffffff; }"
        "QPushButton:focus { border: 2px solid #52a9ff; outline: none; background: rgba(82,169,255,0.25); }"
    ).arg(QString::number(btnRadius)));
    connect(m_btnCustom, &QPushButton::clicked, this, [this]() {
        const QColor c = QColorDialog::getColor(m_color, this, QStringLiteral("Select Workspace Color"));
        if (c.isValid()) {
            m_color = c;
            m_currentPalette[m_selectedSwatchIndex] = c;
            updateSwatchStyles();
            updateColorPreview();
        }
    });
    paletteLayout->addWidget(m_btnCustom);
    paletteLayout->addStretch(1);
    layout->addWidget(m_paletteContainer);

    m_lblHint = new QLabel(QStringLiteral("Shortcuts: <b>[A]..[H]</b> select color &bull; <b>[R]</b> randomize &bull; <b>[Enter]</b> confirm"), frame);
    m_lblHint->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.55); font-size: 11px; border: none; background: transparent;"));
    m_lblHint->setTextFormat(Qt::RichText);
    layout->addWidget(m_lblHint);

    layout->addStretch(1);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    btnRow->addStretch(1);

    auto *btnCancel = new QPushButton(QStringLiteral("Cancel [Esc]"), frame);
    btnCancel->setFocusPolicy(Qt::StrongFocus);
    btnCancel->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.1); color: #ffffff; border-radius: %1px; padding: 8px 16px; border: 1.5px solid rgba(255,255,255,0.2); }"
        "QPushButton:hover { background: rgba(255,255,255,0.2); }"
        "QPushButton:focus { border: 2px solid #52a9ff; outline: none; background: rgba(82,169,255,0.22); }"
    ).arg(QString::number(btnRadius)));
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(btnCancel);

    auto *btnSave = new QPushButton(QStringLiteral("Save [Enter]"), frame);
    btnSave->setFocusPolicy(Qt::StrongFocus);
    btnSave->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: #ffffff; font-weight: bold; border-radius: %2px; padding: 8px 20px; border: 2px solid %1; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:focus { border: 2px solid #ffffff; background: %3; outline: none; }"
    ).arg(accent.name(), QString::number(btnRadius), accent.lighter(115).name()));
    connect(btnSave, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(btnSave);

    layout->addLayout(btnRow);
    rootLayout->addWidget(frame);

    m_tooltipOverlay = new InWindowToolTip(this);
    qApp->installEventFilter(this);

    QWidget::setTabOrder(m_editName, m_editShortcutKey);
    QWidget::setTabOrder(m_editShortcutKey, m_paletteContainer);
    QWidget::setTabOrder(m_paletteContainer, m_btnRandom);
    QWidget::setTabOrder(m_btnRandom, m_btnCustom);
    QWidget::setTabOrder(m_btnCustom, btnCancel);
    QWidget::setTabOrder(btnCancel, btnSave);
}

EditWorkspaceDialog::~EditWorkspaceDialog()
{
    qApp->removeEventFilter(this);
}

bool EditWorkspaceDialog::eventFilter(QObject *w, QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(e);
        if (auto *widget = qobject_cast<QWidget *>(w)) {
            if (widget->window() == this) {
                QString tip;
                QWidget *cur = widget;
                while (cur && cur != this && tip.isEmpty()) {
                    tip = cur->toolTip();
                    cur = cur->parentWidget();
                }
                if (!tip.isEmpty() && m_tooltipOverlay) {
                    m_tooltipOverlay->showTip(tip, he->globalPos(), this);
                    return true;
                }
            }
        }
    } else if (e->type() == QEvent::Leave || e->type() == QEvent::MouseButtonPress) {
        if (m_tooltipOverlay && m_tooltipOverlay->isVisible()) {
            m_tooltipOverlay->hide();
        }
    }
    return QDialog::eventFilter(w, e);
}

void EditWorkspaceDialog::updateSwatchStyles()
{
    for (int i = 0; i < m_swatchButtons.size() && i < m_currentPalette.size(); ++i) {
        auto *btn = m_swatchButtons.at(i);
        const QColor c = m_currentPalette.at(i);
        const bool isSelected = (i == m_selectedSwatchIndex);

        const double lum = 0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue();
        const QString textCol = lum > 145 ? QStringLiteral("#111111") : QStringLiteral("#ffffff");

        if (isSelected) {
            btn->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background-color: %1;"
                "  color: %2;"
                "  font-weight: bold;"
                "  font-size: 14px;"
                "  border-radius: 6px;"
                "  border: 3px solid #ffffff;"
                "}"
            ).arg(c.name(), textCol));
        } else {
            btn->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background-color: %1;"
                "  color: %2;"
                "  font-weight: bold;"
                "  font-size: 13px;"
                "  border-radius: 6px;"
                "  border: 1.5px solid rgba(255,255,255,0.3);"
                "}"
                "QPushButton:hover {"
                "  border: 2px solid #ffffff;"
                "}"
            ).arg(c.name(), textCol));
        }
    }
}

void EditWorkspaceDialog::selectColorIndex(int index)
{
    if (index < 0 || index >= m_currentPalette.size()) return;
    m_selectedSwatchIndex = index;
    m_color = m_currentPalette.at(index);
    updateSwatchStyles();
    updateColorPreview();
}

void EditWorkspaceDialog::randomizeColors()
{
    m_currentPalette.clear();
    const int baseHue = QRandomGenerator::global()->bounded(360);
    for (int i = 0; i < 8; ++i) {
        const int hue = (baseHue + i * 45 + QRandomGenerator::global()->bounded(-10, 11) + 360) % 360;
        const int sat = QRandomGenerator::global()->bounded(190, 245);
        const int val = QRandomGenerator::global()->bounded(205, 255);
        m_currentPalette.append(QColor::fromHsv(hue, sat, val));
    }
    if (m_selectedSwatchIndex < 0 || m_selectedSwatchIndex >= 8) {
        m_selectedSwatchIndex = 0;
    }
    m_color = m_currentPalette.at(m_selectedSwatchIndex);
    updateSwatchStyles();
    updateColorPreview();
}

void EditWorkspaceDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        reject();
        return;
    }

    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        accept();
        return;
    }

    if (m_editName && m_editName->hasFocus()) {
        if (e->key() == Qt::Key_Down) {
            if (m_editShortcutKey) m_editShortcutKey->setFocus();
            else if (m_paletteContainer) m_paletteContainer->setFocus();
            return;
        }
        if (e->key() == Qt::Key_Right && m_editName->cursorPosition() == m_editName->text().length()) {
            if (m_editShortcutKey) {
                m_editShortcutKey->setFocus();
                m_editShortcutKey->selectAll();
                return;
            }
        }
        if (e->modifiers() & Qt::AltModifier) {
            if (e->key() == Qt::Key_R) {
                randomizeColors();
                return;
            }
            int idx = -1;
            if (e->key() >= Qt::Key_A && e->key() <= Qt::Key_H) {
                idx = e->key() - Qt::Key_A;
            } else if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_8) {
                idx = e->key() - Qt::Key_1;
            }
            if (idx >= 0 && idx < m_currentPalette.size()) {
                selectColorIndex(idx);
                return;
            }
        }
        QDialog::keyPressEvent(e);
        return;
    }

    if (m_editShortcutKey && m_editShortcutKey->hasFocus()) {
        if (e->key() == Qt::Key_Up || (e->key() == Qt::Key_Left && m_editShortcutKey->cursorPosition() == 0)) {
            if (m_editName) {
                m_editName->setFocus();
                return;
            }
        }
        if (e->key() == Qt::Key_Down) {
            if (m_paletteContainer) m_paletteContainer->setFocus();
            return;
        }
        QDialog::keyPressEvent(e);
        return;
    }

    // Focus outside line edit
    if (e->key() == Qt::Key_R) {
        randomizeColors();
        return;
    }

    int idx = -1;
    if (e->key() >= Qt::Key_A && e->key() <= Qt::Key_H) {
        idx = e->key() - Qt::Key_A;
    } else if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_8) {
        idx = e->key() - Qt::Key_1;
    }
    if (idx >= 0 && idx < m_currentPalette.size()) {
        selectColorIndex(idx);
        return;
    }

    if (e->key() == Qt::Key_Left) {
        if (m_selectedSwatchIndex > 0) {
            selectColorIndex(m_selectedSwatchIndex - 1);
            return;
        }
    } else if (e->key() == Qt::Key_Right) {
        if (m_selectedSwatchIndex < m_currentPalette.size() - 1) {
            selectColorIndex(m_selectedSwatchIndex + 1);
            return;
        }
    }

    if (e->key() == Qt::Key_Up) {
        if (m_editName) {
            m_editName->setFocus();
            return;
        }
    }

    QDialog::keyPressEvent(e);
}

QString EditWorkspaceDialog::name() const
{
    return m_editName ? m_editName->text().trimmed() : QString();
}

QColor EditWorkspaceDialog::color() const
{
    return m_color;
}

QChar EditWorkspaceDialog::shortcutKey() const
{
    if (!m_editShortcutKey) {
        return QChar();
    }
    const QString txt = m_editShortcutKey->text().trimmed();
    return txt.isEmpty() ? QChar() : txt.at(0).toUpper();
}

void EditWorkspaceDialog::updateColorPreview()
{
    if (m_colorPreview) {
        m_colorPreview->setStyleSheet(QStringLiteral(
            "background-color: %1; border-radius: 11px; border: 2px solid #ffffff;"
        ).arg(m_color.name()));
    }
}

// =============================================================================
// EditMarkerRuleDialog
// =============================================================================
EditMarkerRuleDialog::EditMarkerRuleDialog(const LeapKMarks::PersistentMarkRule &rule,
                                           const LeapKTheme::Theme &theme,
                                           bool isNew,
                                           QWidget *parent)
    : QDialog(parent), m_rule(rule), m_theme(theme), m_isNew(isNew),
      m_selectedColor(rule.color.isValid() ? rule.color : theme.primaryAccentColor())
{
    setWindowTitle(isNew ? QStringLiteral("New Marker Rule") : QStringLiteral("Edit Marker Rule"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(520, 500);

    const QColor accent = theme.primaryAccentColor();
    const QColor textPrim = theme.textPrimaryColor();
    const int btnRadius = qBound(4, theme.cardCornerRadius(), 8);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto *frame = new MainUnifiedFrame(theme, this);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(10);

    // Title
    auto *lblTitle = new QLabel(isNew ? QStringLiteral("<b>Add New Marker Rule</b>") : QStringLiteral("<b>Edit Marker Rule</b>"), frame);
    lblTitle->setStyleSheet(QStringLiteral("font-size: 16px; color: %1; border: none; background: transparent;").arg(textPrim.name()));
    layout->addWidget(lblTitle);

    auto *grid = new QGridLayout();
    grid->setSpacing(8);

    // Letter
    auto *lblLetter = new QLabel(QStringLiteral("Letter Badge:"), frame);
    lblLetter->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.8); font-size: 12px; border: none; background: transparent;"));
    m_letterEdit = new QLineEdit(rule.letter.isNull() ? QString() : QString(rule.letter), frame);
    m_letterEdit->setMaxLength(1);
    m_letterEdit->setFixedWidth(60);
    m_letterEdit->setAlignment(Qt::AlignCenter);
    m_letterEdit->setFocusPolicy(Qt::StrongFocus);
    m_letterEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: rgba(0,0,0,0.35); color: #ffffff; border: 2px solid rgba(255,255,255,0.25); border-radius: %1px; font-size: 16px; font-weight: bold; padding: 4px; }"
        "QLineEdit:focus { border: 2px solid %2; background: rgba(0,0,0,0.55); outline: none; }"
    ).arg(QString::number(btnRadius), accent.name()));
    grid->addWidget(lblLetter, 0, 0);
    grid->addWidget(m_letterEdit, 0, 1);

    // Color
    auto *lblCol = new QLabel(QStringLiteral("Badge Color:"), frame);
    lblCol->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.8); font-size: 12px; border: none; background: transparent;"));
    grid->addWidget(lblCol, 1, 0);

    auto *colorRow = new QHBoxLayout();
    colorRow->setSpacing(6);
    m_colorPreview = new QWidget(frame);
    m_colorPreview->setFixedSize(28, 28);
    updateColorPreview();
    colorRow->addWidget(m_colorPreview);

    for (int i = 0; i < 8; ++i) {
        const QColor c(s_presetColors[i]);
        auto *btn = new QPushButton(frame);
        btn->setFixedSize(24, 24);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::StrongFocus);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; border-radius: 12px; border: 2px solid rgba(255,255,255,0.4); }"
            "QPushButton:hover { border: 2px solid #ffffff; }"
            "QPushButton:focus { border: 2.5px solid #ffffff; outline: none; }"
        ).arg(c.name()));
        connect(btn, &QPushButton::clicked, this, [this, c]() {
            m_selectedColor = c;
            updateColorPreview();
        });
        colorRow->addWidget(btn);
    }
    auto *btnCustomCol = new QPushButton(QStringLiteral("Custom..."), frame);
    btnCustomCol->setFixedSize(65, 24);
    btnCustomCol->setCursor(Qt::PointingHandCursor);
    btnCustomCol->setFocusPolicy(Qt::StrongFocus);
    btnCustomCol->setToolTip(QStringLiteral("Open full RGB color picker"));
    btnCustomCol->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.15); border-radius: %1px; border: 1.5px solid rgba(255,255,255,0.3); font-size: 11px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.3); }"
        "QPushButton:focus { border: 2px solid #52a9ff; background: rgba(82,169,255,0.3); outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(btnCustomCol, &QPushButton::clicked, this, &EditMarkerRuleDialog::openCustomColorDialog);
    colorRow->addWidget(btnCustomCol);
    colorRow->addStretch(1);
    grid->addLayout(colorRow, 1, 1);

    // Label
    auto *lblLabel = new QLabel(QStringLiteral("Rule Label:"), frame);
    lblLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.8); font-size: 12px; border: none; background: transparent;"));
    m_labelEdit = new QLineEdit(rule.label, frame);
    m_labelEdit->setPlaceholderText(QStringLiteral("e.g. Browser, Terminal, Editor"));
    m_labelEdit->setFocusPolicy(Qt::StrongFocus);
    m_labelEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: rgba(0,0,0,0.35); color: #ffffff; border: 2px solid rgba(255,255,255,0.25); border-radius: %1px; padding: 6px 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 2px solid %2; background: rgba(0,0,0,0.55); outline: none; }"
    ).arg(QString::number(btnRadius), accent.name()));
    grid->addWidget(lblLabel, 2, 0);
    grid->addWidget(m_labelEdit, 2, 1);

    // App Class
    auto *lblApp = new QLabel(QStringLiteral("Application:"), frame);
    lblApp->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.8); font-size: 12px; border: none; background: transparent;"));
    m_appEdit = new QLineEdit(rule.matchApp, frame);
    m_appEdit->setPlaceholderText(QStringLiteral("e.g. google-chrome, code, org.kde.konsole"));
    m_appEdit->setFocusPolicy(Qt::StrongFocus);
    m_appEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: rgba(0,0,0,0.35); color: #ffffff; border: 2px solid rgba(255,255,255,0.25); border-radius: %1px; padding: 6px 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 2px solid %2; background: rgba(0,0,0,0.55); outline: none; }"
    ).arg(QString::number(btnRadius), accent.name()));
    grid->addWidget(lblApp, 3, 0);
    grid->addWidget(m_appEdit, 3, 1);

    // Caption
    auto *lblCaption = new QLabel(QStringLiteral("Title Match:"), frame);
    lblCaption->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.8); font-size: 12px; border: none; background: transparent;"));
    m_captionEdit = new QLineEdit(rule.matchCaption, frame);
    m_captionEdit->setPlaceholderText(QStringLiteral("Optional regex or substring"));
    m_captionEdit->setFocusPolicy(Qt::StrongFocus);
    m_captionEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: rgba(0,0,0,0.35); color: #ffffff; border: 2px solid rgba(255,255,255,0.25); border-radius: %1px; padding: 6px 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 2px solid %2; background: rgba(0,0,0,0.55); outline: none; }"
    ).arg(QString::number(btnRadius), accent.name()));
    grid->addWidget(lblCaption, 4, 0);
    grid->addWidget(m_captionEdit, 4, 1);

    // Desktop
    auto *lblDesk = new QLabel(QStringLiteral("Virtual Desktop:"), frame);
    lblDesk->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.8); font-size: 12px; border: none; background: transparent;"));
    m_desktopCombo = new QComboBox(frame);
    m_desktopCombo->setFocusPolicy(Qt::StrongFocus);
    m_desktopCombo->addItem(QStringLiteral("Any Desktop"), 0);
    for (int d = 1; d <= 8; ++d) {
        m_desktopCombo->addItem(QStringLiteral("Desktop %1").arg(d), d);
    }
    const int curIdx = m_desktopCombo->findData(rule.matchDesktop);
    if (curIdx >= 0) m_desktopCombo->setCurrentIndex(curIdx);
    m_desktopCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background: rgba(0,0,0,0.35); color: #ffffff; border: 2px solid rgba(255,255,255,0.25); border-radius: %1px; padding: 6px 10px; }"
        "QComboBox:focus { border: 2px solid %2; outline: none; }"
        "QComboBox QAbstractItemView { background: #1e1e1e; color: #ffffff; selection-background-color: %2; }"
    ).arg(QString::number(btnRadius), accent.name()));
    grid->addWidget(lblDesk, 5, 0);
    grid->addWidget(m_desktopCombo, 5, 1);

    // Enabled
    auto *lblEn = new QLabel(QStringLiteral("Status:"), frame);
    lblEn->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.8); font-size: 12px; border: none; background: transparent;"));
    m_enabledCb = new QCheckBox(QStringLiteral("Rule enabled"), frame);
    m_enabledCb->setFocusPolicy(Qt::StrongFocus);
    m_enabledCb->setChecked(rule.enabled);
    m_enabledCb->setStyleSheet(QStringLiteral(
        "QCheckBox { color: #2ecc71; font-weight: bold; border: none; background: transparent; }"
        "QCheckBox:focus { border: 1.5px solid #2ecc71; border-radius: 3px; outline: none; }"
    ));
    grid->addWidget(lblEn, 6, 0);
    grid->addWidget(m_enabledCb, 6, 1);

    layout->addLayout(grid);
    layout->addStretch(1);

    // Bottom buttons
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    btnRow->addStretch(1);

    auto *btnCancel = new QPushButton(QStringLiteral("Cancel [Esc]"), frame);
    btnCancel->setFocusPolicy(Qt::StrongFocus);
    btnCancel->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.1); color: #ffffff; border-radius: %1px; padding: 8px 16px; border: 1.5px solid rgba(255,255,255,0.2); }"
        "QPushButton:hover { background: rgba(255,255,255,0.2); }"
        "QPushButton:focus { border: 2px solid #52a9ff; outline: none; background: rgba(82,169,255,0.22); }"
    ).arg(QString::number(btnRadius)));
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(btnCancel);

    auto *btnSave = new QPushButton(QStringLiteral("Save Rule [Enter]"), frame);
    btnSave->setFocusPolicy(Qt::StrongFocus);
    btnSave->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: #ffffff; font-weight: bold; border-radius: %2px; padding: 8px 20px; border: 2px solid %1; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:focus { border: 2px solid #ffffff; background: %3; outline: none; }"
    ).arg(accent.name(), QString::number(btnRadius), accent.lighter(115).name()));
    connect(btnSave, &QPushButton::clicked, this, &EditMarkerRuleDialog::onSaveClicked);
    btnRow->addWidget(btnSave);

    layout->addLayout(btnRow);
    rootLayout->addWidget(frame);

    m_tooltipOverlay = new InWindowToolTip(this);
    qApp->installEventFilter(this);
}

EditMarkerRuleDialog::~EditMarkerRuleDialog()
{
    qApp->removeEventFilter(this);
}

bool EditMarkerRuleDialog::eventFilter(QObject *w, QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(e);
        if (auto *widget = qobject_cast<QWidget *>(w)) {
            if (widget->window() == this) {
                QString tip;
                QWidget *cur = widget;
                while (cur && cur != this && tip.isEmpty()) {
                    tip = cur->toolTip();
                    cur = cur->parentWidget();
                }
                if (!tip.isEmpty() && m_tooltipOverlay) {
                    m_tooltipOverlay->showTip(tip, he->globalPos(), this);
                    return true;
                }
            }
        }
    } else if (e->type() == QEvent::Leave || e->type() == QEvent::MouseButtonPress) {
        if (m_tooltipOverlay && m_tooltipOverlay->isVisible()) {
            m_tooltipOverlay->hide();
        }
    }
    return QDialog::eventFilter(w, e);
}

void EditMarkerRuleDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (auto *btn = qobject_cast<QAbstractButton *>(focusWidget())) {
            if (btn->isEnabled()) {
                btn->animateClick();
                return;
            }
        }
        onSaveClicked();
        return;
    }
    QDialog::keyPressEvent(e);
}

void EditMarkerRuleDialog::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
        m_isDragging = true;
    }
    QDialog::mousePressEvent(e);
}

void EditMarkerRuleDialog::mouseMoveEvent(QMouseEvent *e)
{
    if (m_isDragging && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPosition().toPoint() - m_dragPos);
    }
    QDialog::mouseMoveEvent(e);
}

void EditMarkerRuleDialog::mouseReleaseEvent(QMouseEvent *e)
{
    m_isDragging = false;
    QDialog::mouseReleaseEvent(e);
}

void EditMarkerRuleDialog::openCustomColorDialog()
{
    const QColor c = QColorDialog::getColor(m_selectedColor, this, QStringLiteral("Select Badge Color"));
    if (c.isValid()) {
        m_selectedColor = c;
        updateColorPreview();
    }
}

void EditMarkerRuleDialog::updateColorPreview()
{
    if (m_colorPreview) {
        m_colorPreview->setStyleSheet(QStringLiteral(
            "background-color: %1; border-radius: 6px; border: 1.5px solid rgba(255,255,255,0.6);"
        ).arg(m_selectedColor.name()));
    }
}

void EditMarkerRuleDialog::onSaveClicked()
{
    const QString letterStr = m_letterEdit ? m_letterEdit->text().trimmed().toUpper() : QString();
    m_rule.letter = letterStr.isEmpty() ? QChar() : letterStr.at(0);
    m_rule.color = m_selectedColor;
    m_rule.label = m_labelEdit ? m_labelEdit->text().trimmed() : QString();
    m_rule.matchApp = m_appEdit ? m_appEdit->text().trimmed() : QString();
    m_rule.matchCaption = m_captionEdit ? m_captionEdit->text().trimmed() : QString();
    m_rule.matchDesktop = m_desktopCombo ? m_desktopCombo->currentData().toInt() : 0;
    m_rule.enabled = m_enabledCb ? m_enabledCb->isChecked() : true;
    if (m_rule.id.isEmpty()) {
        m_rule.id = QStringLiteral("rule-%1").arg(QDateTime::currentMSecsSinceEpoch());
    }
    accept();
}

// =============================================================================
// WorkspaceCard
// =============================================================================
WorkspaceCard::WorkspaceCard(int index,
                             const LeapKWorkspace::WorkspaceInfo &ws,
                             bool isAssigned,
                             bool isSelected,
                             int matchingWindowCount,
                             const LeapKTheme::Theme &theme,
                             QWidget *parent)
    : QFrame(parent), m_index(index), m_ws(ws), m_isAssigned(isAssigned),
      m_isSelected(isSelected), m_matchingWindowCount(matchingWindowCount), m_theme(theme)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    const QColor accent = theme.primaryAccentColor();
    const QColor textPrim = theme.textPrimaryColor();
    const int radius = qBound(4, theme.cardCornerRadius(), 8);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(12);

    // Number indicator
    if (index >= 0 && index < 9) {
        m_lblNum = new QLabel(QStringLiteral("<span style='color:%1; font-weight:bold; font-size:13px;'>[%2]</span>")
            .arg(accent.name(), QString::number(index + 1)), this);
        m_lblNum->setTextFormat(Qt::RichText);
        layout->addWidget(m_lblNum);
    }

    // Color dot
    m_lblColorDot = new QLabel(this);
    m_lblColorDot->setFixedSize(14, 14);
    const QColor dotCol = ws.color.isValid() ? ws.color : accent;
    m_lblColorDot->setStyleSheet(QStringLiteral(
        "background-color: %1; border-radius: 7px; border: 1.5px solid rgba(255,255,255,0.7);"
    ).arg(dotCol.name()));
    layout->addWidget(m_lblColorDot);

    // Name & Desktop Info
    auto *vbox = new QVBoxLayout();
    vbox->setSpacing(2);

    auto *nameRow = new QHBoxLayout();
    nameRow->setSpacing(8);

    auto *lblName = new QLabel(QStringLiteral("<b>%1</b>").arg(ws.name.toHtmlEscaped()), this);
    lblName->setTextFormat(Qt::RichText);
    lblName->setStyleSheet(QStringLiteral("font-size: 15px; color: %1;").arg(textPrim.name()));
    nameRow->addWidget(lblName);

    if (!ws.shortcutKey.isNull()) {
        auto *lblShortcut = new QLabel(QStringLiteral("<span style='background:rgba(82,169,255,0.22); border:1px solid rgba(82,169,255,0.5); border-radius:3px; padding:1px 5px; font-weight:bold; font-size:11px; color:#52a9ff;'>[%1]</span>").arg(ws.shortcutKey.toUpper()), this);
        lblShortcut->setTextFormat(Qt::RichText);
        lblShortcut->setToolTip(QStringLiteral("HUD Jump Shortcut: [%1]").arg(ws.shortcutKey.toUpper()));
        nameRow->addWidget(lblShortcut);
    }

    int wsDesktop = 0;
    for (const auto &r : ws.rules) {
        if (r.matchDesktop > 0) {
            wsDesktop = r.matchDesktop;
            break;
        }
    }
    if (wsDesktop > 0) {
        auto *lblDesk = new QLabel(QStringLiteral("<span style='color:rgba(255,255,255,0.6); font-size:12px;'>[D: %1]</span>").arg(wsDesktop), this);
        lblDesk->setTextFormat(Qt::RichText);
        nameRow->addWidget(lblDesk);
    }
    nameRow->addStretch(1);
    vbox->addLayout(nameRow);

    QString desc;
    if (matchingWindowCount > 0) {
        desc = (matchingWindowCount == 1 ? QStringLiteral("1 open window assigned") : QStringLiteral("%1 open windows assigned").arg(matchingWindowCount));
    } else {
        desc = QStringLiteral("Empty workspace");
    }
    auto *lblDesc = new QLabel(desc, this);
    lblDesc->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.55); font-size: 12px;"));
    vbox->addWidget(lblDesc);

    layout->addLayout(vbox, 1);

    // Status badge
    m_lblStatus = new QLabel(this);
    m_lblStatus->setMinimumWidth(100);
    updateStatusBadge();
    layout->addWidget(m_lblStatus);

    // Edit button [E]
    m_btnEdit = new QPushButton(QStringLiteral("[E] Edit"), this);
    m_btnEdit->setMinimumWidth(64);
    m_btnEdit->setFixedHeight(28);
    m_btnEdit->setCursor(Qt::PointingHandCursor);
    m_btnEdit->setFocusPolicy(Qt::StrongFocus);
    m_btnEdit->setToolTip(QStringLiteral("Edit workspace name and color [E]"));
    m_btnEdit->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: rgba(%1,%2,%3,0.15); color: %4; font-size: 12px; font-weight: bold; border: 1.5px solid rgba(%1,%2,%3,0.35); border-radius: %5px; padding: 4px 8px; }"
        "QPushButton:hover { background-color: rgba(%1,%2,%3,0.4); color: #ffffff; }"
        "QPushButton:focus { border: 2px solid #ffffff; background-color: rgba(%1,%2,%3,0.55); color: #ffffff; outline: none; }"
    ).arg(QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue()), accent.name(), QString::number(radius)));
    connect(m_btnEdit, &QPushButton::clicked, this, [this]() {
        if (onEditClicked) onEditClicked(m_index);
    });
    layout->addWidget(m_btnEdit);

    // Delete button [Shift+Del]
    m_btnDelete = new QPushButton(QStringLiteral("[Del]"), this);
    m_btnDelete->setMinimumWidth(50);
    m_btnDelete->setFixedHeight(28);
    m_btnDelete->setCursor(Qt::PointingHandCursor);
    m_btnDelete->setFocusPolicy(Qt::StrongFocus);
    m_btnDelete->setToolTip(QStringLiteral("Delete workspace [Shift+Del]"));
    m_btnDelete->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: rgba(231,76,60,0.15); color: #e74c3c; font-size: 12px; font-weight: bold; border: 1.5px solid rgba(231,76,60,0.35); border-radius: %1px; padding: 4px 6px; }"
        "QPushButton:hover { background-color: rgba(231,76,60,0.4); color: #ffffff; }"
        "QPushButton:focus { border: 2px solid #ffffff; background-color: rgba(231,76,60,0.55); color: #ffffff; outline: none; }"
    ).arg(QString::number(radius)));
    connect(m_btnDelete, &QPushButton::clicked, this, [this]() {
        if (onDeleteClicked) onDeleteClicked(m_index);
    });
    layout->addWidget(m_btnDelete);

    setSelected(m_isSelected);
}

void WorkspaceCard::setAssigned(bool assigned)
{
    m_isAssigned = assigned;
    updateStatusBadge();
}

void WorkspaceCard::setSelected(bool selected)
{
    m_isSelected = selected;
    updateCardStyle();
}

void WorkspaceCard::updateCardStyle()
{
    const QColor accent = m_theme.primaryAccentColor();
    const int radius = qBound(4, m_theme.cardCornerRadius(), 8);

    if (hasFocus()) {
        setStyleSheet(QStringLiteral(
            "WorkspaceCard {"
            "  background-color: rgba(%1, %2, %3, 0.32);"
            "  border: 2.5px solid #ffffff;"
            "  border-radius: %4px;"
            "}"
        ).arg(QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue()), QString::number(radius)));
    } else if (m_isSelected) {
        setStyleSheet(QStringLiteral(
            "WorkspaceCard {"
            "  background-color: rgba(%1, %2, %3, 0.22);"
            "  border: 2px solid %4;"
            "  border-radius: %5px;"
            "}"
        ).arg(QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue()), accent.name(), QString::number(radius)));
    } else {
        setStyleSheet(QStringLiteral(
            "WorkspaceCard {"
            "  background-color: rgba(255, 255, 255, 0.05);"
            "  border: 1.5px solid rgba(255, 255, 255, 0.12);"
            "  border-radius: %1px;"
            "}"
            "WorkspaceCard:hover {"
            "  background-color: rgba(255, 255, 255, 0.1);"
            "  border-color: rgba(%2, %3, %4, 0.45);"
            "}"
        ).arg(QString::number(radius), QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue())));
    }
}

void WorkspaceCard::updateStatusBadge()
{
    if (!m_lblStatus) return;
    if (m_isAssigned) {
        m_lblStatus->setText(QStringLiteral("<b style='color:#2ecc71;'>● Assigned</b>"));
    } else {
        m_lblStatus->setText(QStringLiteral("<span style='color:rgba(255,255,255,0.35);'>○ Not assigned</span>"));
    }
    m_lblStatus->setTextFormat(Qt::RichText);
}

void WorkspaceCard::triggerToggle()
{
    if (onToggleAssignment) onToggleAssignment(m_index);
}

void WorkspaceCard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        setFocus();
        triggerToggle();
    }
    QFrame::mousePressEvent(e);
}

void WorkspaceCard::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) {
        triggerToggle();
        return;
    }
    QFrame::keyPressEvent(e);
}

void WorkspaceCard::focusInEvent(QFocusEvent *e)
{
    updateCardStyle();
    if (onCardFocused) onCardFocused(m_index);
    QFrame::focusInEvent(e);
}

void WorkspaceCard::focusOutEvent(QFocusEvent *e)
{
    updateCardStyle();
    QFrame::focusOutEvent(e);
}

// =============================================================================
// MarkerRuleCard
// =============================================================================
MarkerRuleCard::MarkerRuleCard(const LeapKMarks::PersistentMarkRule &rule,
                               int cardIndex,
                               bool isSelected,
                               int matchingCount,
                               const QStringList &matchingTitles,
                               const LeapKTheme::Theme &theme,
                               QWidget *parent)
    : QFrame(parent), m_rule(rule), m_cardIndex(cardIndex), m_isSelected(isSelected),
      m_matchingCount(matchingCount), m_matchingTitles(matchingTitles), m_theme(theme)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    const QColor accent = theme.primaryAccentColor();
    const QColor textPrim = theme.textPrimaryColor();
    const int badgeRadius = qBound(6, theme.cardCornerRadius(), 12);
    const int btnRadius = qBound(4, theme.cardCornerRadius(), 8);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(14);

    // Letter badge
    m_lblBadge = new QLabel(rule.letter.isNull() ? QStringLiteral("?") : QString(rule.letter), this);
    m_lblBadge->setFixedSize(40, 40);
    m_lblBadge->setAlignment(Qt::AlignCenter);
    const QString badgeBg = rule.color.isValid() ? rule.color.name() : accent.name();
    m_lblBadge->setStyleSheet(QStringLiteral(
        "background-color: %1; color: #ffffff; font-weight: bold; font-size: 19px; border-radius: %2px; border: 1.5px solid rgba(255,255,255,0.4);"
    ).arg(badgeBg, QString::number(badgeRadius)));
    layout->addWidget(m_lblBadge);

    // Middle details
    auto *vbox = new QVBoxLayout();
    vbox->setSpacing(2);

    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);

    if (cardIndex >= 0 && cardIndex < 9) {
        auto *lblNum = new QLabel(QStringLiteral("<span style='color:%1; font-size:12px; font-weight:bold;'>[%2]</span>")
            .arg(accent.name(), QString::number(cardIndex + 1)), this);
        lblNum->setTextFormat(Qt::RichText);
        titleRow->addWidget(lblNum);
    }

    const QString displayTitle = !rule.label.trimmed().isEmpty()
        ? rule.label.trimmed()
        : (!rule.matchApp.trimmed().isEmpty() ? rule.matchApp.trimmed() : QStringLiteral("Unnamed Group"));

    auto *lblTitle = new QLabel(QStringLiteral("<b>%1</b>").arg(displayTitle.toHtmlEscaped()), this);
    lblTitle->setTextFormat(Qt::RichText);
    lblTitle->setStyleSheet(QStringLiteral("font-size: 15px; color: %1;").arg(textPrim.name()));
    titleRow->addWidget(lblTitle);

    if (!rule.label.trimmed().isEmpty() && !rule.matchApp.trimmed().isEmpty()) {
        auto *lblSubApp = new QLabel(QStringLiteral("• <span style='color:rgba(255,255,255,0.55);'>%1</span>").arg(rule.matchApp.toHtmlEscaped()), this);
        lblSubApp->setTextFormat(Qt::RichText);
        titleRow->addWidget(lblSubApp);
    }
    titleRow->addStretch(1);
    vbox->addLayout(titleRow);

    // Criteria details
    QStringList criteria;
    if (!rule.matchApp.trimmed().isEmpty()) {
        criteria.append(QStringLiteral("App: <b style='color:%1;'>%2</b>").arg(accent.name(), rule.matchApp.toHtmlEscaped()));
    } else {
        criteria.append(QStringLiteral("App: <i>any</i>"));
    }
    if (!rule.matchCaption.trimmed().isEmpty()) {
        criteria.append(QStringLiteral("Title: <b style='color:#2ecc71;'>\"%1\"</b>").arg(rule.matchCaption.toHtmlEscaped()));
    }
    if (rule.matchDesktop > 0) {
        criteria.append(QStringLiteral("Desktop: <b>%1</b>").arg(rule.matchDesktop));
    }

    auto *lblCriteria = new QLabel(criteria.join(QStringLiteral(" &bull; ")), this);
    lblCriteria->setTextFormat(Qt::RichText);
    lblCriteria->setWordWrap(true);
    lblCriteria->setStyleSheet(QStringLiteral("font-size: 12px; color: rgba(255,255,255,0.7);"));
    vbox->addWidget(lblCriteria);

    // Live open matching windows
    auto *lblLive = new QLabel(this);
    lblLive->setWordWrap(true);
    if (m_matchingCount > 0) {
        const QString winCountStr = (m_matchingCount == 1 ? QStringLiteral("1 open window") : QStringLiteral("%1 open windows").arg(m_matchingCount));
        lblLive->setText(QStringLiteral("<span style='color:#2ecc71; font-weight:bold;'>● %1</span>: <span style='color:rgba(255,255,255,0.65);'>%2</span>")
            .arg(winCountStr, m_matchingTitles.join(QStringLiteral(", ")).left(85).toHtmlEscaped()));
        if (!m_matchingTitles.isEmpty()) {
            lblLive->setToolTip(m_matchingTitles.join(QLatin1Char('\n')));
        }
    } else {
        lblLive->setText(QStringLiteral("<span style='color:rgba(255,255,255,0.35);'>○ 0 open windows in this group</span>"));
    }
    lblLive->setTextFormat(Qt::RichText);
    lblLive->setStyleSheet(QStringLiteral("font-size: 11px;"));
    vbox->addWidget(lblLive);

    layout->addLayout(vbox, 1);

    // Right actions
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(6);

    m_btnToggle = new QPushButton(this);
    m_btnToggle->setMinimumWidth(72);
    m_btnToggle->setFixedHeight(28);
    m_btnToggle->setCursor(Qt::PointingHandCursor);
    m_btnToggle->setFocusPolicy(Qt::StrongFocus);
    m_btnToggle->setToolTip(QStringLiteral("Toggle active / disabled state [Space / T]"));
    updateToggleStyle();
    connect(m_btnToggle, &QPushButton::clicked, this, [this]() {
        triggerToggle();
    });
    btnLayout->addWidget(m_btnToggle);

    auto *btnEdit = new QPushButton(QStringLiteral("[E] Edit"), this);
    btnEdit->setMinimumWidth(68);
    btnEdit->setFixedHeight(28);
    btnEdit->setCursor(Qt::PointingHandCursor);
    btnEdit->setFocusPolicy(Qt::StrongFocus);
    btnEdit->setToolTip(QStringLiteral("Edit this rule [E]"));
    btnEdit->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.1); color: #ffffff; border-radius: %1px; border: 1.5px solid rgba(255,255,255,0.2); font-size: 11px; font-weight: bold; padding: 4px 8px; }"
        "QPushButton:hover { background: rgba(%2,%3,%4,0.35); border-color: %5; }"
        "QPushButton:focus { border: 2px solid #ffffff; background: rgba(%2,%3,%4,0.55); outline: none; }"
    ).arg(QString::number(btnRadius), QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue()), accent.name()));
    connect(btnEdit, &QPushButton::clicked, this, [this]() {
        if (onEditClicked) onEditClicked(m_rule);
    });
    btnLayout->addWidget(btnEdit);

    auto *btnDelete = new QPushButton(QStringLiteral("[Del] Delete"), this);
    btnDelete->setMinimumWidth(80);
    btnDelete->setFixedHeight(28);
    btnDelete->setCursor(Qt::PointingHandCursor);
    btnDelete->setFocusPolicy(Qt::StrongFocus);
    btnDelete->setToolTip(QStringLiteral("Delete this rule [D / Del]"));
    btnDelete->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(231,76,60,0.15); color: #e74c3c; border-radius: %1px; border: 1.5px solid rgba(231,76,60,0.35); font-size: 11px; font-weight: bold; padding: 4px 8px; }"
        "QPushButton:hover { background: rgba(231,76,60,0.4); color: #ffffff; }"
        "QPushButton:focus { border: 2px solid #ffffff; background: rgba(231,76,60,0.55); color: #ffffff; outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(btnDelete, &QPushButton::clicked, this, [this]() {
        if (onDeleteClicked) onDeleteClicked(m_rule);
    });
    btnLayout->addWidget(btnDelete);

    layout->addLayout(btnLayout);

    updateStyle();
}

void MarkerRuleCard::setSelected(bool selected)
{
    m_isSelected = selected;
    updateStyle();
}

void MarkerRuleCard::updateStyle()
{
    const QColor accent = m_theme.primaryAccentColor();
    const int radius = qBound(4, m_theme.cardCornerRadius(), 8);

    if (hasFocus()) {
        setStyleSheet(QStringLiteral(
            "MarkerRuleCard {"
            "  background-color: rgba(%1, %2, %3, 0.32);"
            "  border: 2.5px solid #ffffff;"
            "  border-radius: %4px;"
            "}"
        ).arg(QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue()), QString::number(radius)));
    } else if (m_isSelected) {
        setStyleSheet(QStringLiteral(
            "MarkerRuleCard {"
            "  background-color: rgba(%1, %2, %3, 0.22);"
            "  border: 2px solid %4;"
            "  border-radius: %5px;"
            "}"
        ).arg(QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue()), accent.name(), QString::number(radius)));
    } else if (!m_rule.enabled) {
        setStyleSheet(QStringLiteral(
            "MarkerRuleCard {"
            "  background-color: rgba(255, 255, 255, 0.02);"
            "  border: 1.5px dashed rgba(255, 255, 255, 0.1);"
            "  border-radius: %1px;"
            "  opacity: 0.65;"
            "}"
        ).arg(QString::number(radius)));
    } else {
        setStyleSheet(QStringLiteral(
            "MarkerRuleCard {"
            "  background-color: rgba(255, 255, 255, 0.05);"
            "  border: 1.5px solid rgba(255, 255, 255, 0.12);"
            "  border-radius: %1px;"
            "}"
            "MarkerRuleCard:hover {"
            "  background-color: rgba(255, 255, 255, 0.1);"
            "  border-color: rgba(%2, %3, %4, 0.45);"
            "}"
        ).arg(QString::number(radius), QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue())));
    }
}

void MarkerRuleCard::updateToggleStyle()
{
    if (!m_btnToggle) return;
    const int radius = qBound(4, m_theme.cardCornerRadius(), 8);
    if (m_rule.enabled) {
        m_btnToggle->setText(QStringLiteral("● Active"));
        m_btnToggle->setStyleSheet(QStringLiteral(
            "QPushButton { background: rgba(46,204,113,0.18); color: #2ecc71; border-radius: %1px; border: 1.5px solid rgba(46,204,113,0.45); font-size: 11px; font-weight: bold; padding: 4px 8px; }"
            "QPushButton:hover { background: rgba(46,204,113,0.35); }"
            "QPushButton:focus { border: 2px solid #ffffff; background: rgba(46,204,113,0.5); outline: none; }"
        ).arg(QString::number(radius)));
    } else {
        m_btnToggle->setText(QStringLiteral("○ Off"));
        m_btnToggle->setStyleSheet(QStringLiteral(
            "QPushButton { background: rgba(255,255,255,0.06); color: rgba(255,255,255,0.4); border-radius: %1px; border: 1.5px solid rgba(255,255,255,0.15); font-size: 11px; font-weight: bold; padding: 4px 8px; }"
            "QPushButton:hover { background: rgba(255,255,255,0.15); color: #ffffff; }"
            "QPushButton:focus { border: 2px solid #52a9ff; color: #ffffff; background: rgba(82,169,255,0.25); outline: none; }"
        ).arg(QString::number(radius)));
    }
}

void MarkerRuleCard::triggerToggle()
{
    m_rule.enabled = !m_rule.enabled;
    updateStyle();
    updateToggleStyle();
    if (onToggleEnabled) onToggleEnabled(m_rule);
}

void MarkerRuleCard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        setFocus();
        triggerToggle();
    }
    QFrame::mousePressEvent(e);
}

void MarkerRuleCard::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) {
        triggerToggle();
        return;
    }
    QFrame::keyPressEvent(e);
}

void MarkerRuleCard::focusInEvent(QFocusEvent *e)
{
    updateStyle();
    if (onCardFocused) onCardFocused(m_cardIndex);
    QFrame::focusInEvent(e);
}

void MarkerRuleCard::focusOutEvent(QFocusEvent *e)
{
    updateStyle();
    QFrame::focusOutEvent(e);
}

// =============================================================================
// WorkspaceManagerView
// =============================================================================
WorkspaceManagerView::WorkspaceManagerView(const ManagerContext &ctx, QWidget *parent)
    : QWidget(parent), m_ctx(ctx), m_appClass(ctx.appClass), m_windowCaption(ctx.windowCaption),
      m_desktopNum(ctx.desktopNum), m_desktopName(ctx.desktopName), m_assignedWsId(ctx.assignedWsId),
      m_workspaces(ctx.workspaces), m_theme(ctx.theme)
{
    const QColor accent = m_theme.primaryAccentColor();
    const int btnRadius = qBound(4, m_theme.cardCornerRadius(), 8);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 8, 16, 12);
    mainLayout->setSpacing(10);

    // Scope switcher
    auto *scopeLayout = new QHBoxLayout();
    scopeLayout->setSpacing(8);

    m_btnScopeApp = new QPushButton(QStringLiteral("[A] App Scope (all windows)"), this);
    m_btnScopeApp->setFocusPolicy(Qt::StrongFocus);
    m_btnScopeDirect = new QPushButton(QStringLiteral("[S] Single Window Scope"), this);
    m_btnScopeDirect->setFocusPolicy(Qt::StrongFocus);

    connect(m_btnScopeApp, &QPushButton::clicked, this, [this]() { setScope(true); });
    connect(m_btnScopeDirect, &QPushButton::clicked, this, [this]() { setScope(false); });

    scopeLayout->addWidget(m_btnScopeApp);
    scopeLayout->addWidget(m_btnScopeDirect);
    scopeLayout->addStretch(1);
    mainLayout->addLayout(scopeLayout);

    setScope(true);

    // Scroll Area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 8px; background: transparent; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.2); border-radius: 4px; }"
    ));

    m_cardsContainer = new QWidget(m_scrollArea);
    m_cardsContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(8);

    rebuildCards();
    m_scrollArea->setWidget(m_cardsContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    // New workspace buttons
    auto *newWsLayout = new QHBoxLayout();
    newWsLayout->setSpacing(10);

    m_btnNewWs = new QPushButton(QStringLiteral("[N] Create with Window"), this);
    m_btnNewWs->setFocusPolicy(Qt::StrongFocus);
    m_btnNewWs->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: rgba(%1, %2, %3, 0.15);"
        "  border: 2px dashed %4;"
        "  border-radius: %5px;"
        "  color: %4;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "  padding: 8px;"
        "}"
        "QPushButton:hover { background-color: rgba(%1, %2, %3, 0.3); }"
        "QPushButton:focus { border: 2px solid #ffffff; background-color: rgba(%1, %2, %3, 0.45); color: #ffffff; outline: none; }"
    ).arg(QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue()), accent.name(), QString::number(btnRadius)));
    connect(m_btnNewWs, &QPushButton::clicked, this, &WorkspaceManagerView::onNewWorkspace);
    newWsLayout->addWidget(m_btnNewWs);

    m_btnNewEmptyWs = new QPushButton(QStringLiteral("[C] Create Empty Workspace"), this);
    m_btnNewEmptyWs->setFocusPolicy(Qt::StrongFocus);
    m_btnNewEmptyWs->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: rgba(46, 204, 113, 0.15);"
        "  border: 2px dashed #2ecc71;"
        "  border-radius: %1px;"
        "  color: #2ecc71;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "  padding: 8px;"
        "}"
        "QPushButton:hover { background-color: rgba(46, 204, 113, 0.3); }"
        "QPushButton:focus { border: 2px solid #ffffff; background-color: rgba(46, 204, 113, 0.45); color: #ffffff; outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(m_btnNewEmptyWs, &QPushButton::clicked, this, &WorkspaceManagerView::onNewEmptyWorkspace);
    newWsLayout->addWidget(m_btnNewEmptyWs);

    mainLayout->addLayout(newWsLayout);

    // Bottom action bar
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);

    m_btnUnassign = new QPushButton(QStringLiteral("[Del] Unassign"), this);
    m_btnUnassign->setFocusPolicy(Qt::StrongFocus);
    m_btnUnassign->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(231,76,60,0.12); border: 1.5px solid rgba(231,76,60,0.35); border-radius: %1px; color: #e74c3c; font-weight: bold; padding: 7px 14px; }"
        "QPushButton:hover { background: rgba(231,76,60,0.25); }"
        "QPushButton:focus { border: 2px solid #ffffff; background: rgba(231,76,60,0.45); color: #ffffff; outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(m_btnUnassign, &QPushButton::clicked, this, &WorkspaceManagerView::onUnassign);
    bottomLayout->addWidget(m_btnUnassign);

    m_btnEditWs = new QPushButton(QStringLiteral("[E] Edit"), this);
    m_btnEditWs->setFocusPolicy(Qt::StrongFocus);
    m_btnEditWs->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(%1,%2,%3,0.15); border: 1.5px solid rgba(%1,%2,%3,0.4); border-radius: %4px; color: %5; font-weight: bold; padding: 7px 14px; }"
        "QPushButton:hover { background: rgba(%1,%2,%3,0.3); }"
        "QPushButton:focus { border: 2px solid #ffffff; background: rgba(%1,%2,%3,0.5); color: #ffffff; outline: none; }"
    ).arg(QString::number(accent.red()), QString::number(accent.green()), QString::number(accent.blue()), QString::number(btnRadius), accent.name()));
    connect(m_btnEditWs, &QPushButton::clicked, this, &WorkspaceManagerView::editSelectedWorkspace);
    bottomLayout->addWidget(m_btnEditWs);

    m_btnDeleteWs = new QPushButton(QStringLiteral("[Shift+Del] Delete Workspace"), this);
    m_btnDeleteWs->setFocusPolicy(Qt::StrongFocus);
    m_btnDeleteWs->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(231,76,60,0.15); border: 1.5px solid rgba(231,76,60,0.4); border-radius: %1px; color: #e74c3c; font-weight: bold; padding: 7px 14px; }"
        "QPushButton:hover { background: rgba(231,76,60,0.3); }"
        "QPushButton:focus { border: 2px solid #ffffff; background: rgba(231,76,60,0.5); color: #ffffff; outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(m_btnDeleteWs, &QPushButton::clicked, this, &WorkspaceManagerView::deleteSelectedOrAssignedWorkspace);
    bottomLayout->addWidget(m_btnDeleteWs);

    bottomLayout->addStretch(1);

    m_btnCancel = new QPushButton(QStringLiteral("Cancel [Esc]"), this);
    m_btnCancel->setFocusPolicy(Qt::StrongFocus);
    m_btnCancel->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.1); border-radius: %1px; color: #ffffff; padding: 7px 14px; border: 1.5px solid rgba(255,255,255,0.2); }"
        "QPushButton:hover { background: rgba(255,255,255,0.2); }"
        "QPushButton:focus { border: 2px solid #52a9ff; outline: none; background: rgba(82,169,255,0.22); }"
    ).arg(QString::number(btnRadius)));
    connect(m_btnCancel, &QPushButton::clicked, this, &WorkspaceManagerView::requestReject);
    bottomLayout->addWidget(m_btnCancel);

    m_btnDone = new QPushButton(QStringLiteral("Done [Enter]"), this);
    m_btnDone->setFocusPolicy(Qt::StrongFocus);
    m_btnDone->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2ecc71; border-radius: %1px; color: #ffffff; font-weight: bold; padding: 7px 22px; border: 2px solid #2ecc71; }"
        "QPushButton:hover { background: #27ae60; }"
        "QPushButton:focus { border: 2px solid #ffffff; background: #27ae60; outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(m_btnDone, &QPushButton::clicked, this, &WorkspaceManagerView::requestAccept);
    bottomLayout->addWidget(m_btnDone);

    mainLayout->addLayout(bottomLayout);
}

void WorkspaceManagerView::setScope(bool appScope)
{
    m_groupByApp = appScope;
    const QColor accent = m_theme.primaryAccentColor();
    const int btnRadius = qBound(4, m_theme.cardCornerRadius(), 8);

    const QString activeStyle = QStringLiteral(
        "QPushButton { background-color: %1; color: #ffffff; font-weight: bold; border-radius: %2px; padding: 6px 14px; border: 2px solid %1; }"
        "QPushButton:focus { border: 2px solid #ffffff; outline: none; }"
    ).arg(accent.name(), QString::number(btnRadius));

    const QString inactiveStyle = QStringLiteral(
        "QPushButton { background-color: rgba(255, 255, 255, 0.08); color: rgba(255, 255, 255, 0.6); border-radius: %1px; padding: 6px 14px; border: 2px solid rgba(255, 255, 255, 0.15); }"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 0.15); color: #ffffff; }"
        "QPushButton:focus { border: 2px solid #52a9ff; color: #ffffff; background-color: rgba(82, 169, 255, 0.22); outline: none; }"
    ).arg(QString::number(btnRadius));

    m_btnScopeApp->setStyleSheet(appScope ? activeStyle : inactiveStyle);
    m_btnScopeDirect->setStyleSheet(appScope ? inactiveStyle : activeStyle);
}

void WorkspaceManagerView::rebuildCards()
{
    QLayoutItem *item;
    while ((item = m_cardsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_cards.clear();

    for (int i = 0; i < m_workspaces.size(); ++i) {
        const auto &ws = m_workspaces.at(i);
        const bool isAssigned = (ws.id == m_assignedWsId);
        const bool isSelected = (i == m_selectedCardIndex);

        int winCount = 0;
        for (const auto &w : m_ctx.openWindows) {
            if (LeapKWorkspace::matchesWorkspace(ws, w.app, w.caption, w.desktop)) {
                winCount++;
            }
        }

        auto *card = new WorkspaceCard(i, ws, isAssigned, isSelected, winCount, m_theme, m_cardsContainer);
        card->onEditClicked = [this](int idx) {
            m_selectedCardIndex = idx;
            editSelectedWorkspace();
        };
        card->onDeleteClicked = [this](int idx) {
            m_selectedCardIndex = idx;
            deleteSelectedOrAssignedWorkspace();
        };
        card->onToggleAssignment = [this](int idx) {
            toggleWorkspaceAssignment(idx);
        };
        card->onCardFocused = [this](int idx) {
            m_selectedCardIndex = idx;
            for (int j = 0; j < m_cards.size(); ++j) {
                m_cards.at(j)->setSelected(j == m_selectedCardIndex);
            }
            if (idx >= 0 && idx < m_cards.size()) {
                m_scrollArea->ensureWidgetVisible(m_cards.at(idx));
            }
        };
        m_cardsLayout->addWidget(card);
        m_cards.append(card);
    }
    m_cardsLayout->addStretch(1);

    if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_cards.size()) {
        selectCard(m_selectedCardIndex);
    }
}

void WorkspaceManagerView::selectCard(int idx)
{
    if (m_cards.isEmpty()) {
        m_selectedCardIndex = -1;
        return;
    }
    const bool hadCardFocus = (focusWidget() && qobject_cast<WorkspaceCard *>(focusWidget()));
    m_selectedCardIndex = qBound(0, idx, m_cards.size() - 1);
    for (int i = 0; i < m_cards.size(); ++i) {
        m_cards.at(i)->setSelected(i == m_selectedCardIndex);
    }
    if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_cards.size()) {
        auto *card = m_cards.at(m_selectedCardIndex);
        m_scrollArea->ensureWidgetVisible(card);
        if (hadCardFocus) {
            card->setFocus();
        }
    }
}

void WorkspaceManagerView::selectNextCard()
{
    if (m_cards.isEmpty()) return;
    selectCard((m_selectedCardIndex + 1) % m_cards.size());
}

void WorkspaceManagerView::selectPrevCard()
{
    if (m_cards.isEmpty()) return;
    int prev = m_selectedCardIndex - 1;
    if (prev < 0) prev = m_cards.size() - 1;
    selectCard(prev);
}

void WorkspaceManagerView::toggleWorkspaceAssignment(int idx)
{
    if (idx < 0 || idx >= m_workspaces.size()) return;
    const QString targetId = m_workspaces.at(idx).id;
    if (m_assignedWsId == targetId) {
        m_assignedWsId.clear();
    } else {
        m_assignedWsId = targetId;
    }
    selectCard(idx);
    for (int i = 0; i < m_cards.size(); ++i) {
        m_cards.at(i)->setAssigned(m_workspaces.at(i).id == m_assignedWsId);
    }
}

void WorkspaceManagerView::onUnassign()
{
    m_assignedWsId.clear();
    for (auto *card : m_cards) {
        card->setAssigned(false);
    }
}

void WorkspaceManagerView::onNewWorkspace()
{
    const QString defaultName = !m_appClass.isEmpty() ? m_appClass : QStringLiteral("Workspace %1").arg(m_workspaces.size() + 1);
    LeapKWorkspace::WorkspaceInfo info;
    info.id = QStringLiteral("ws-%1").arg(QDateTime::currentMSecsSinceEpoch());
    info.name = defaultName;
    info.color = m_theme.primaryAccentColor();

    EditWorkspaceDialog dlg(info, m_theme, this);
    if (dlg.exec() == QDialog::Accepted) {
        info.name = dlg.name().isEmpty() ? defaultName : dlg.name();
        info.color = dlg.color();
        info.shortcutKey = dlg.shortcutKey();
        LeapKWorkspace::WindowMatchRule r;
        if (m_groupByApp) {
            r.matchApp = m_appClass;
        } else {
            r.matchCaption = m_windowCaption;
        }
        r.matchDesktop = m_desktopNum;
        info.rules.append(r);

        m_workspaces.append(info);
        m_assignedWsId = info.id;
        rebuildCards();
        selectCard(m_workspaces.size() - 1);
    }
}

void WorkspaceManagerView::onNewEmptyWorkspace()
{
    const QString defaultName = QStringLiteral("Workspace %1").arg(m_workspaces.size() + 1);
    LeapKWorkspace::WorkspaceInfo info;
    info.id = QStringLiteral("ws-%1").arg(QDateTime::currentMSecsSinceEpoch());
    info.name = defaultName;
    info.color = m_theme.secondaryAccentColor();

    EditWorkspaceDialog dlg(info, m_theme, this);
    if (dlg.exec() == QDialog::Accepted) {
        info.name = dlg.name().isEmpty() ? defaultName : dlg.name();
        info.color = dlg.color();
        info.shortcutKey = dlg.shortcutKey();
        m_workspaces.append(info);
        rebuildCards();
        selectCard(m_workspaces.size() - 1);
    }
}

void WorkspaceManagerView::editSelectedWorkspace()
{
    int targetIdx = m_selectedCardIndex;
    if (targetIdx < 0 && !m_assignedWsId.isEmpty()) {
        for (int i = 0; i < m_workspaces.size(); ++i) {
            if (m_workspaces.at(i).id == m_assignedWsId) {
                targetIdx = i;
                break;
            }
        }
    }
    if (targetIdx < 0 || targetIdx >= m_workspaces.size()) return;

    auto ws = m_workspaces.at(targetIdx);
    EditWorkspaceDialog dlg(ws, m_theme, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (!dlg.name().isEmpty()) ws.name = dlg.name();
        ws.color = dlg.color();
        ws.shortcutKey = dlg.shortcutKey();
        m_workspaces[targetIdx] = ws;
        rebuildCards();
        selectCard(targetIdx);
    }
}

void WorkspaceManagerView::deleteSelectedOrAssignedWorkspace()
{
    int targetIdx = m_selectedCardIndex;
    if (targetIdx < 0 && !m_assignedWsId.isEmpty()) {
        for (int i = 0; i < m_workspaces.size(); ++i) {
            if (m_workspaces.at(i).id == m_assignedWsId) {
                targetIdx = i;
                break;
            }
        }
    }
    if (targetIdx < 0 || targetIdx >= m_workspaces.size()) return;

    const auto ws = m_workspaces.at(targetIdx);
    CloseWorkspaceDialog dlg(ws.name, m_theme, this);
    if (dlg.exec() == QDialog::Accepted) {
        QString actStr = QStringLiteral("standard");
        if (dlg.selectedAction() == CloseWorkspaceDialog::CloseWindows) {
            actStr = QStringLiteral("close");
        } else if (dlg.selectedAction() == CloseWorkspaceDialog::MoveToStash) {
            actStr = QStringLiteral("stash");
        }

        QJsonObject delObj;
        delObj[QStringLiteral("id")] = ws.id;
        delObj[QStringLiteral("name")] = ws.name;
        delObj[QStringLiteral("action")] = actStr;
        m_deletedWorkspaces.append(delObj);

        if (m_assignedWsId == ws.id) {
            m_assignedWsId.clear();
        }
        m_workspaces.removeAt(targetIdx);
        rebuildCards();
        selectCard(qMin(targetIdx, m_workspaces.size() - 1));
    }
}

bool WorkspaceManagerView::handleKeyEvent(QKeyEvent *e)
{
    // If a button or card currently has focus, do NOT hijack Enter or Space
    if (qobject_cast<QAbstractButton *>(focusWidget()) || qobject_cast<WorkspaceCard *>(focusWidget())) {
        return false;
    }

    // [E] or [F2] or [R] -> Edit
    if (e->key() == Qt::Key_E || e->key() == Qt::Key_F2 || e->key() == Qt::Key_R) {
        editSelectedWorkspace();
        return true;
    }
    // [Enter] / [Return] -> Done
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        emit requestAccept();
        return true;
    }
    // [A] -> App scope
    if (e->key() == Qt::Key_A) {
        setScope(true);
        return true;
    }
    // [S] or [D] -> Single window scope (or Shift+D for delete)
    if (e->key() == Qt::Key_S || e->key() == Qt::Key_D) {
        if (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) {
            deleteSelectedOrAssignedWorkspace();
            return true;
        }
        setScope(false);
        return true;
    }
    // [N] -> Create with window
    if (e->key() == Qt::Key_N && !(e->modifiers() & Qt::ControlModifier)) {
        onNewWorkspace();
        return true;
    }
    // [C] -> Create empty workspace
    if (e->key() == Qt::Key_C || ((e->modifiers() & Qt::ShiftModifier) && e->key() == Qt::Key_N)) {
        onNewEmptyWorkspace();
        return true;
    }
    // [X] -> Delete workspace
    if (e->key() == Qt::Key_X) {
        deleteSelectedOrAssignedWorkspace();
        return true;
    }
    // [J] / [Down] -> next card
    if (e->key() == Qt::Key_Down || e->key() == Qt::Key_J) {
        selectNextCard();
        return true;
    }
    // [K] / [Up] -> prev card
    if (e->key() == Qt::Key_Up || e->key() == Qt::Key_K) {
        selectPrevCard();
        return true;
    }
    // [Space] -> Toggle assignment
    if (e->key() == Qt::Key_Space) {
        if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_workspaces.size()) {
            toggleWorkspaceAssignment(m_selectedCardIndex);
            return true;
        }
    }
    // [Del] -> Unassign or Delete
    if (e->key() == Qt::Key_Delete) {
        if (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) {
            deleteSelectedOrAssignedWorkspace();
            return true;
        }
        onUnassign();
        return true;
    }
    // [1..9] -> Select workspace card
    if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_9) {
        const int idx = e->key() - Qt::Key_1;
        if (idx < m_workspaces.size()) {
            if (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) {
                m_selectedCardIndex = idx;
                deleteSelectedOrAssignedWorkspace();
            } else {
                toggleWorkspaceAssignment(idx);
            }
            return true;
        }
    }

    return false;
}

// =============================================================================
// MarkerManagerView
// =============================================================================
MarkerManagerView::MarkerManagerView(const ManagerContext &ctx, QWidget *parent)
    : QWidget(parent), m_ctx(ctx), m_appClass(ctx.appClass), m_windowCaption(ctx.windowCaption),
      m_desktopNum(ctx.desktopNum), m_desktopName(ctx.desktopName), m_openWindows(ctx.openWindows),
      m_theme(ctx.theme)
{
    const QColor accent = m_theme.primaryAccentColor();
    const int btnRadius = qBound(4, m_theme.cardCornerRadius(), 8);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 8, 16, 12);
    mainLayout->setSpacing(10);

    // Top action bar
    auto *topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setFocusPolicy(Qt::StrongFocus);
    m_searchEdit->setPlaceholderText(QStringLiteral("Filter rules... [/]"));
    m_searchEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: rgba(0,0,0,0.3); color: #ffffff; border: 1.5px solid rgba(255,255,255,0.25); border-radius: %1px; padding: 7px 12px; font-size: 13px; }"
        "QLineEdit:focus { border: 2px solid #52a9ff; background: rgba(0,0,0,0.65); outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MarkerManagerView::rebuildCards);
    topBar->addWidget(m_searchEdit, 1);

    m_btnNewRule = new QPushButton(QStringLiteral("[N] New Rule"), this);
    m_btnNewRule->setFocusPolicy(Qt::StrongFocus);
    m_btnNewRule->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: #ffffff; font-weight: bold; border-radius: %2px; padding: 7px 14px; border: 2px solid %1; font-size: 12px; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:focus { border: 2px solid #ffffff; background: %3; outline: none; }"
    ).arg(accent.name(), QString::number(btnRadius), accent.lighter(115).name()));
    connect(m_btnNewRule, &QPushButton::clicked, this, &MarkerManagerView::onAddNewRuleClicked);
    topBar->addWidget(m_btnNewRule);

    if (!m_appClass.isEmpty()) {
        m_btnAssignCurrent = new QPushButton(QStringLiteral("[W] Current Window"), this);
        m_btnAssignCurrent->setFocusPolicy(Qt::StrongFocus);
        m_btnAssignCurrent->setToolTip(QStringLiteral("Create a persistent rule for currently focused window: %1").arg(m_appClass));
        m_btnAssignCurrent->setStyleSheet(QStringLiteral(
            "QPushButton { background: rgba(46,204,113,0.18); color: #2ecc71; border: 1.5px solid rgba(46,204,113,0.45); border-radius: %1px; padding: 7px 14px; font-weight: bold; font-size: 12px; }"
            "QPushButton:hover { background: rgba(46,204,113,0.35); }"
            "QPushButton:focus { border: 2px solid #ffffff; background: rgba(46,204,113,0.5); color: #ffffff; outline: none; }"
        ).arg(QString::number(btnRadius)));
        connect(m_btnAssignCurrent, &QPushButton::clicked, this, &MarkerManagerView::onAssignCurrentClicked);
        topBar->addWidget(m_btnAssignCurrent);
    }

    m_btnClearAll = new QPushButton(QStringLiteral("[X] Clear All"), this);
    m_btnClearAll->setFocusPolicy(Qt::StrongFocus);
    m_btnClearAll->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(231,76,60,0.12); color: #e74c3c; border: 1.5px solid rgba(231,76,60,0.3); border-radius: %1px; padding: 7px 12px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background: rgba(231,76,60,0.25); }"
        "QPushButton:focus { border: 2px solid #ffffff; background: rgba(231,76,60,0.45); color: #ffffff; outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(m_btnClearAll, &QPushButton::clicked, this, &MarkerManagerView::onClearAllClicked);
    topBar->addWidget(m_btnClearAll);

    mainLayout->addLayout(topBar);

    // Scroll Area for cards
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 8px; background: transparent; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.2); border-radius: 4px; }"
    ));

    m_cardsContainer = new QWidget(m_scrollArea);
    m_cardsContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(8);

    loadRules();
    rebuildCards();

    m_scrollArea->setWidget(m_cardsContainer);
    mainLayout->addWidget(m_scrollArea, 1);
}

bool MarkerManagerView::isSearchFocused() const
{
    return m_searchEdit && m_searchEdit->hasFocus();
}

void MarkerManagerView::clearSearchFocus()
{
    if (m_searchEdit) {
        m_searchEdit->clearFocus();
    }
}

void MarkerManagerView::focusSearch()
{
    if (m_searchEdit) {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    }
}

void MarkerManagerView::loadRules()
{
    m_rules = LeapKMarks::loadPersistentMarks();
}

void MarkerManagerView::saveRules()
{
    LeapKMarks::savePersistentMarks(m_rules);
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QStringLiteral("reconfigureEffect")
    );
    msg << QStringLiteral("leapk");
    QDBusConnection::sessionBus().send(msg);
}

void MarkerManagerView::rebuildCards()
{
    QLayoutItem *item;
    while ((item = m_cardsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_cards.clear();

    const QString filter = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : QString();

    int displayedCount = 0;
    for (int i = 0; i < m_rules.size(); ++i) {
        const auto &rule = m_rules.at(i);
        if (!filter.isEmpty()) {
            bool matches = rule.label.toLower().contains(filter)
                || rule.matchApp.toLower().contains(filter)
                || rule.matchCaption.toLower().contains(filter)
                || QString(rule.letter).toLower().contains(filter);
            if (!matches) continue;
        }

        // Live open windows matching this rule
        int matchCount = 0;
        QStringList matchTitles;
        for (const auto &win : m_openWindows) {
            bool matches = true;
            if (!rule.matchApp.isEmpty()) {
                if (!win.app.contains(rule.matchApp, Qt::CaseInsensitive)) matches = false;
            }
            if (matches && !rule.matchCaption.isEmpty()) {
                const QRegularExpression re(rule.matchCaption, QRegularExpression::CaseInsensitiveOption);
                if (!re.match(win.caption).hasMatch()) matches = false;
            }
            if (matches && rule.matchDesktop > 0) {
                if (win.desktop != rule.matchDesktop) matches = false;
            }
            if (matches) {
                matchCount++;
                matchTitles.append(win.caption.isEmpty() ? win.app : win.caption);
            }
        }

        const bool isSelected = (displayedCount == m_selectedCardIndex);
        auto *card = new MarkerRuleCard(rule, displayedCount, isSelected, matchCount, matchTitles, m_theme, m_cardsContainer);
        card->onEditClicked = [this](const LeapKMarks::PersistentMarkRule &r) { onEditRuleClicked(r); };
        card->onDeleteClicked = [this](const LeapKMarks::PersistentMarkRule &r) { onDeleteRuleClicked(r); };
        card->onToggleEnabled = [this](const LeapKMarks::PersistentMarkRule &r) { onToggleRuleEnabled(r); };
        card->onCardFocused = [this](int idx) {
            m_selectedCardIndex = idx;
            for (int j = 0; j < m_cards.size(); ++j) {
                m_cards.at(j)->setSelected(j == m_selectedCardIndex);
            }
            if (idx >= 0 && idx < m_cards.size()) {
                m_scrollArea->ensureWidgetVisible(m_cards.at(idx));
            }
        };

        m_cardsLayout->addWidget(card);
        m_cards.append(card);
        displayedCount++;
    }

    if (displayedCount == 0) {
        m_emptyLabel = new QLabel(filter.isEmpty()
            ? QStringLiteral("<div style='text-align:center; padding:40px; color:rgba(255,255,255,0.4); font-size:14px;'>"
                             "No marker rules defined yet.<br><br>Press <b>[N]</b> to add a rule or <b>[W]</b> to assign the active window.</div>")
            : QStringLiteral("<div style='text-align:center; padding:40px; color:rgba(255,255,255,0.4); font-size:14px;'>"
                             "No marker rules matched your search filter.</div>"),
            m_cardsContainer);
        m_emptyLabel->setTextFormat(Qt::RichText);
        m_cardsLayout->addWidget(m_emptyLabel);
    }

    m_cardsLayout->addStretch(1);

    if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_cards.size()) {
        selectCard(m_selectedCardIndex);
    }
}

void MarkerManagerView::selectCard(int idx)
{
    if (m_cards.isEmpty()) {
        m_selectedCardIndex = -1;
        return;
    }
    const bool hadCardFocus = (focusWidget() && qobject_cast<MarkerRuleCard *>(focusWidget()));
    m_selectedCardIndex = qBound(0, idx, m_cards.size() - 1);
    for (int i = 0; i < m_cards.size(); ++i) {
        m_cards.at(i)->setSelected(i == m_selectedCardIndex);
    }
    if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_cards.size()) {
        auto *card = m_cards.at(m_selectedCardIndex);
        m_scrollArea->ensureWidgetVisible(card);
        if (hadCardFocus) {
            card->setFocus();
        }
    }
}

void MarkerManagerView::selectNextCard()
{
    if (m_cards.isEmpty()) return;
    selectCard((m_selectedCardIndex + 1) % m_cards.size());
}

void MarkerManagerView::selectPrevCard()
{
    if (m_cards.isEmpty()) return;
    int prev = m_selectedCardIndex - 1;
    if (prev < 0) prev = m_cards.size() - 1;
    selectCard(prev);
}

void MarkerManagerView::onAddNewRuleClicked()
{
    LeapKMarks::PersistentMarkRule r;
    r.id = QStringLiteral("rule-%1").arg(QDateTime::currentMSecsSinceEpoch());
    r.color = m_theme.primaryAccentColor();
    r.enabled = true;

    EditMarkerRuleDialog dlg(r, m_theme, true, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_rules.append(dlg.rule());
        saveRules();
        rebuildCards();
        selectCard(m_cards.size() - 1);
    }
}

void MarkerManagerView::onAssignCurrentClicked()
{
    if (m_appClass.isEmpty()) return;
    LeapKMarks::PersistentMarkRule r;
    r.id = QStringLiteral("rule-%1").arg(QDateTime::currentMSecsSinceEpoch());
    r.matchApp = m_appClass;
    r.label = m_appClass;
    r.letter = m_appClass.at(0).toUpper();
    r.color = m_theme.primaryAccentColor();
    r.enabled = true;

    EditMarkerRuleDialog dlg(r, m_theme, true, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_rules.append(dlg.rule());
        saveRules();
        rebuildCards();
        selectCard(m_cards.size() - 1);
    }
}

void MarkerManagerView::onClearAllClicked()
{
    if (m_rules.isEmpty()) return;
    m_rules.clear();
    saveRules();
    rebuildCards();
}

void MarkerManagerView::onEditRuleClicked(const LeapKMarks::PersistentMarkRule &rule)
{
    int foundIdx = -1;
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules.at(i).id == rule.id) {
            foundIdx = i;
            break;
        }
    }
    if (foundIdx < 0) return;

    EditMarkerRuleDialog dlg(rule, m_theme, false, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_rules[foundIdx] = dlg.rule();
        saveRules();
        rebuildCards();
        selectCard(m_selectedCardIndex);
    }
}

void MarkerManagerView::onDeleteRuleClicked(const LeapKMarks::PersistentMarkRule &rule)
{
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules.at(i).id == rule.id) {
            m_rules.removeAt(i);
            break;
        }
    }
    saveRules();
    rebuildCards();
    selectCard(qMin(m_selectedCardIndex, m_cards.size() - 1));
}

void MarkerManagerView::onToggleRuleEnabled(const LeapKMarks::PersistentMarkRule &rule)
{
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules.at(i).id == rule.id) {
            m_rules[i].enabled = rule.enabled;
            break;
        }
    }
    saveRules();
}

bool MarkerManagerView::handleKeyEvent(QKeyEvent *e)
{
    // If a button or card currently has focus, do NOT hijack Enter or Space
    if (qobject_cast<QAbstractButton *>(focusWidget()) || qobject_cast<MarkerRuleCard *>(focusWidget())) {
        return false;
    }

    // [/] or [Ctrl+F] -> Focus search field
    if (e->key() == Qt::Key_Slash || ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_F)) {
        focusSearch();
        return true;
    }
    // [N] -> Add New Rule
    if (e->key() == Qt::Key_N) {
        onAddNewRuleClicked();
        return true;
    }
    // [W] -> Current Window rule
    if (e->key() == Qt::Key_W && !m_appClass.isEmpty()) {
        onAssignCurrentClicked();
        return true;
    }
    // [X] -> Clear all
    if (e->key() == Qt::Key_X) {
        onClearAllClicked();
        return true;
    }
    // [J] / [Down] -> next card
    if (e->key() == Qt::Key_J || e->key() == Qt::Key_Down) {
        selectNextCard();
        return true;
    }
    // [K] / [Up] -> prev card
    if (e->key() == Qt::Key_K || e->key() == Qt::Key_Up) {
        selectPrevCard();
        return true;
    }
    // [E] / [Enter] -> Edit selected card
    if (e->key() == Qt::Key_E || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_cards.size()) {
            onEditRuleClicked(m_cards.at(m_selectedCardIndex)->rule());
            return true;
        }
    }
    // [Space] / [T] -> Toggle enabled state
    if (e->key() == Qt::Key_Space || e->key() == Qt::Key_T) {
        if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_cards.size()) {
            auto *card = m_cards.at(m_selectedCardIndex);
            auto r = card->rule();
            r.enabled = !r.enabled;
            card->onToggleEnabled(r);
            card->updateStyle();
            card->updateToggleStyle();
            return true;
        }
    }
    // [D] / [Del] -> Delete rule
    if (e->key() == Qt::Key_D || e->key() == Qt::Key_Delete) {
        if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_cards.size()) {
            onDeleteRuleClicked(m_cards.at(m_selectedCardIndex)->rule());
            return true;
        }
    }
    // [1..9] -> Select rule card
    if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_9) {
        const int idx = e->key() - Qt::Key_1;
        if (idx < m_cards.size()) {
            selectCard(idx);
            return true;
        }
    }

    return false;
}

// =============================================================================
// UnifiedManagerDialog
// =============================================================================
UnifiedManagerDialog::UnifiedManagerDialog(const ManagerContext &ctx, QWidget *parent)
    : QDialog(parent), m_ctx(ctx), m_theme(ctx.theme), m_currentTab(ctx.defaultTab)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    // Spacious dimensions (920x680) to give criteria and cards ample horizontal breathing room
    resize(920, 680);
    setMinimumSize(880, 580);

    const QColor accent = m_theme.primaryAccentColor();
    const QColor textPrim = m_theme.textPrimaryColor();
    const int btnRadius = qBound(4, m_theme.cardCornerRadius(), 8);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_frame = new MainUnifiedFrame(m_theme, this);
    auto *frameLayout = new QVBoxLayout(m_frame);
    frameLayout->setContentsMargins(20, 16, 20, 14);
    frameLayout->setSpacing(8);

    // Header layout
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(14);

    // Title / Brand
    auto *lblBrand = new QLabel(QStringLiteral("<b><span style='color:%1; font-size:16px;'>LeapK</span> <span style='color:%2; font-size:16px;'>Manager</span></b>")
        .arg(accent.name(), textPrim.name()), m_frame);
    lblBrand->setTextFormat(Qt::RichText);
    headerLayout->addWidget(lblBrand);

    headerLayout->addSpacing(10);

    // Center Tabs
    auto *tabContainer = new QWidget(m_frame);
    tabContainer->setStyleSheet(QStringLiteral("background: rgba(0,0,0,0.3); border-radius: %1px;").arg(QString::number(btnRadius + 2)));
    auto *tabLayout = new QHBoxLayout(tabContainer);
    tabLayout->setContentsMargins(4, 4, 4, 4);
    tabLayout->setSpacing(6);

    m_tabBtnWorkspaces = new QPushButton(QStringLiteral("Workspaces  [Ctrl+1]"), tabContainer);
    m_tabBtnWorkspaces->setCursor(Qt::PointingHandCursor);
    m_tabBtnWorkspaces->setFocusPolicy(Qt::StrongFocus);
    m_tabBtnWorkspaces->setMinimumWidth(175);
    m_tabBtnWorkspaces->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_tabBtnWorkspaces->setToolTip(QStringLiteral("Switch to Workspaces [Ctrl+Tab / Ctrl+1 / Alt+1 / []"));
    connect(m_tabBtnWorkspaces, &QPushButton::clicked, this, [this]() { switchTab(TabWorkspaces); });
    tabLayout->addWidget(m_tabBtnWorkspaces);

    m_tabBtnMarkers = new QPushButton(QStringLiteral("Markers & Groups  [Ctrl+2]"), tabContainer);
    m_tabBtnMarkers->setCursor(Qt::PointingHandCursor);
    m_tabBtnMarkers->setFocusPolicy(Qt::StrongFocus);
    m_tabBtnMarkers->setMinimumWidth(215);
    m_tabBtnMarkers->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_tabBtnMarkers->setToolTip(QStringLiteral("Switch to Markers & Groups [Ctrl+Tab / Ctrl+2 / Alt+2 / ]]"));
    connect(m_tabBtnMarkers, &QPushButton::clicked, this, [this]() { switchTab(TabMarkers); });
    tabLayout->addWidget(m_tabBtnMarkers);

    headerLayout->addWidget(tabContainer);
    headerLayout->addStretch(1);

    // Right side: Target window badge (if present) + Close button
    if (!m_ctx.appClass.isEmpty()) {
        auto *lblTarget = new QLabel(m_frame);
        QFontMetrics fm(lblTarget->font());
        const QString elidedApp = fm.elidedText(m_ctx.appClass, Qt::ElideMiddle, 160);
        lblTarget->setText(QStringLiteral("<span style='color:rgba(255,255,255,0.65); font-size:11px;'>Window:</span> <b style='color:%1; font-size:12px;'>%2</b>")
            .arg(accent.name(), elidedApp.toHtmlEscaped()));
        lblTarget->setTextFormat(Qt::RichText);
        lblTarget->setStyleSheet(QStringLiteral("background: rgba(255,255,255,0.06); padding: 4px 8px; border-radius: 4px; border: 1px solid rgba(255,255,255,0.12);"));
        lblTarget->setToolTip(QStringLiteral("Focused Window: %1").arg(m_ctx.appClass));
        lblTarget->setMaximumWidth(220);
        headerLayout->addWidget(lblTarget);
    }

    m_btnClose = new QPushButton(QStringLiteral("[Esc] Close"), m_frame);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setFocusPolicy(Qt::StrongFocus);
    m_btnClose->setMinimumWidth(85);
    m_btnClose->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.08); color: rgba(255,255,255,0.8); border: 2px solid rgba(255,255,255,0.18); border-radius: %1px; font-size: 11px; font-weight: bold; padding: 5px 12px; }"
        "QPushButton:hover { background: rgba(231,76,60,0.3); color: #ffffff; border-color: #e74c3c; }"
        "QPushButton:focus { border: 2px solid #ff7675; background: rgba(231,76,60,0.35); color: #ffffff; outline: none; }"
    ).arg(QString::number(btnRadius)));
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    headerLayout->addWidget(m_btnClose);

    frameLayout->addLayout(headerLayout);

    // Thin separator
    auto *sep = new QFrame(m_frame);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.08); margin-top: 2px; margin-bottom: 2px;"));
    frameLayout->addWidget(sep);

    // Body: Stacked views
    m_stack = new QStackedWidget(m_frame);
    m_workspaceView = new WorkspaceManagerView(m_ctx, m_stack);
    m_markerView = new MarkerManagerView(m_ctx, m_stack);

    connect(m_workspaceView, &WorkspaceManagerView::requestAccept, this, &QDialog::accept);
    connect(m_workspaceView, &WorkspaceManagerView::requestReject, this, &QDialog::reject);

    m_stack->addWidget(m_workspaceView);
    m_stack->addWidget(m_markerView);
    frameLayout->addWidget(m_stack, 1);

    // Footer shortcut hints
    m_lblShortcutHints = new QLabel(m_frame);
    m_lblShortcutHints->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.5); font-size: 11px; padding: 4px 6px; border: none; background: transparent;"));
    frameLayout->addWidget(m_lblShortcutHints);

    rootLayout->addWidget(m_frame);

    // Establish predictable Tab chain
    QWidget::setTabOrder(m_tabBtnWorkspaces, m_tabBtnMarkers);
    QWidget::setTabOrder(m_tabBtnMarkers, m_btnClose);
    QWidget::setTabOrder(m_btnClose, m_stack);

    m_tooltipOverlay = new InWindowToolTip(this);
    qApp->installEventFilter(this);

    switchTab(m_currentTab);
}

UnifiedManagerDialog::~UnifiedManagerDialog()
{
    qApp->removeEventFilter(this);
}

bool UnifiedManagerDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(event);
        if (auto *w = qobject_cast<QWidget *>(watched)) {
            if (w->window() == this) {
                QString tip;
                QWidget *cur = w;
                while (cur && cur != this && tip.isEmpty()) {
                    tip = cur->toolTip();
                    cur = cur->parentWidget();
                }
                if (!tip.isEmpty() && m_tooltipOverlay) {
                    m_tooltipOverlay->showTip(tip, he->globalPos(), this);
                    return true;
                }
            }
        }
    } else if (event->type() == QEvent::Leave || event->type() == QEvent::MouseButtonPress ||
               event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::Wheel) {
        if (m_tooltipOverlay && m_tooltipOverlay->isVisible()) {
            m_tooltipOverlay->hide();
        }
    } else if (event->type() == QEvent::MouseMove) {
        if (m_tooltipOverlay && m_tooltipOverlay->isVisible()) {
            auto *me = static_cast<QMouseEvent *>(event);
            if ((me->globalPosition().toPoint() - m_tooltipOverlay->lastGlobalPos()).manhattanLength() > 28) {
                m_tooltipOverlay->hide();
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void UnifiedManagerDialog::switchTab(ManagerTab tab)
{
    m_currentTab = tab;
    m_stack->setCurrentIndex(static_cast<int>(tab));
    updateTabButtonStyles();
    updateFooterHints();

    if (m_frame) {
        m_frame->setHeaderTitle(tab == TabWorkspaces ? QStringLiteral("Workspaces") : QStringLiteral("Marker Manager"));
    }
}

void UnifiedManagerDialog::updateTabButtonStyles()
{
    const QColor accent = m_theme.primaryAccentColor();
    const int btnRadius = qBound(4, m_theme.cardCornerRadius(), 8);

    const QString activeStyle = QStringLiteral(
        "QPushButton { background: %1; color: #ffffff; font-weight: bold; border-radius: %2px; padding: 7px 18px; border: 2px solid %1; font-size: 13px; }"
        "QPushButton:focus { border: 2px solid #ffffff; outline: none; }"
    ).arg(accent.name(), QString::number(btnRadius));

    const QString inactiveStyle = QStringLiteral(
        "QPushButton { background: transparent; color: rgba(255,255,255,0.65); font-weight: normal; border-radius: %1px; padding: 7px 18px; border: 2px solid transparent; font-size: 13px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); color: #ffffff; }"
        "QPushButton:focus { border: 2px solid #52a9ff; color: #ffffff; background: rgba(82,169,255,0.22); outline: none; }"
    ).arg(QString::number(btnRadius));

    m_tabBtnWorkspaces->setStyleSheet(m_currentTab == TabWorkspaces ? activeStyle : inactiveStyle);
    m_tabBtnMarkers->setStyleSheet(m_currentTab == TabMarkers ? activeStyle : inactiveStyle);
}

void UnifiedManagerDialog::updateFooterHints()
{
    if (m_currentTab == TabWorkspaces) {
        m_lblShortcutHints->setText(QStringLiteral(
            "[Ctrl+Tab / Ctrl+1/2] Switch Tab  •  [Tab] Jump Controls  •  [1..9] Select  •  [Space] Assign  •  [E] Edit  •  [N] New  •  [C] Empty  •  [Del] Unassign  •  [Esc] Close"
        ));
    } else {
        m_lblShortcutHints->setText(QStringLiteral(
            "[Ctrl+Tab / Ctrl+1/2] Switch Tab  •  [Tab] Jump Controls  •  [1..9] Select  •  [Space] Toggle  •  [E] Edit  •  [N] New Rule  •  [D] Delete  •  [/] Search  •  [Esc] Close"
        ));
    }
}

QString UnifiedManagerDialog::assignedWorkspaceId() const
{
    return m_workspaceView ? m_workspaceView->assignedWorkspaceId() : m_ctx.assignedWsId;
}

bool UnifiedManagerDialog::groupByApp() const
{
    return m_workspaceView ? m_workspaceView->groupByApp() : true;
}

QList<LeapKWorkspace::WorkspaceInfo> UnifiedManagerDialog::workspaces() const
{
    return m_workspaceView ? m_workspaceView->workspaces() : m_ctx.workspaces;
}

QJsonArray UnifiedManagerDialog::deletedWorkspaces() const
{
    return m_workspaceView ? m_workspaceView->deletedWorkspaces() : QJsonArray();
}

void UnifiedManagerDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    if (m_markerView) {
        m_markerView->clearSearchFocus();
    }
    setFocus();
}

void UnifiedManagerDialog::keyPressEvent(QKeyEvent *e)
{
    // 1. If search edit has focus on markers tab:
    // First Esc or Return defocuses search edit, leaving the window open!
    if (m_currentTab == TabMarkers && m_markerView && m_markerView->isSearchFocused()) {
        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            m_markerView->clearSearchFocus();
            setFocus();
            return;
        }
        QDialog::keyPressEvent(e);
        return;
    }

    // 2. Focused button / card activation:
    // When a control has keyboard focus via Tab, pressing Enter / Return or Space activates it!
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (auto *btn = qobject_cast<QAbstractButton *>(focusWidget())) {
            if (btn->isEnabled()) {
                btn->animateClick();
                return;
            }
        } else if (auto *card = qobject_cast<WorkspaceCard *>(focusWidget())) {
            card->triggerToggle();
            return;
        } else if (auto *mcard = qobject_cast<MarkerRuleCard *>(focusWidget())) {
            mcard->triggerToggle();
            return;
        }
    }
    if (e->key() == Qt::Key_Space) {
        if (auto *btn = qobject_cast<QAbstractButton *>(focusWidget())) {
            if (btn->isEnabled()) {
                btn->animateClick();
                return;
            }
        }
    }

    // 3. Global Close: Esc or Ctrl+W or Ctrl+Q
    if (e->key() == Qt::Key_Escape || ((e->modifiers() & Qt::ControlModifier) && (e->key() == Qt::Key_W || e->key() == Qt::Key_Q))) {
        accept();
        return;
    }

    // 4. Dedicated tab switching shortcuts:
    // [Ctrl+Tab] / [Ctrl+Shift+Tab] / [Ctrl+Backtab]
    if ((e->modifiers() & Qt::ControlModifier) && (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab)) {
        switchTab(m_currentTab == TabWorkspaces ? TabMarkers : TabWorkspaces);
        return;
    }
    // [Ctrl+1] or [Alt+1] or [Ctrl+PageUp] -> Workspaces tab
    if (((e->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) && e->key() == Qt::Key_1) ||
        ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_PageUp)) {
        switchTab(TabWorkspaces);
        return;
    }
    // [Ctrl+2] or [Alt+2] or [Ctrl+PageDown] -> Markers tab
    if (((e->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) && e->key() == Qt::Key_2) ||
        ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_PageDown)) {
        switchTab(TabMarkers);
        return;
    }
    // [[] or []] -> Quick bracket switching when not typing in search field
    if (!(focusWidget() && qobject_cast<QLineEdit *>(focusWidget()))) {
        if (e->key() == Qt::Key_BracketLeft) {
            switchTab(TabWorkspaces);
            return;
        }
        if (e->key() == Qt::Key_BracketRight) {
            switchTab(TabMarkers);
            return;
        }
    }

    // 5. Delegate to active tab view
    if (m_currentTab == TabWorkspaces && m_workspaceView) {
        if (m_workspaceView->handleKeyEvent(e)) {
            return;
        }
    } else if (m_currentTab == TabMarkers && m_markerView) {
        if (m_markerView->handleKeyEvent(e)) {
            return;
        }
    }

    // 6. Default dialog handler (handles Tab / Shift+Tab for control navigation!)
    QDialog::keyPressEvent(e);
}

void UnifiedManagerDialog::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
        m_isDragging = true;
    }
    QDialog::mousePressEvent(e);
}

void UnifiedManagerDialog::mouseMoveEvent(QMouseEvent *e)
{
    if (m_isDragging && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPosition().toPoint() - m_dragPos);
    }
    QDialog::mouseMoveEvent(e);
}

void UnifiedManagerDialog::mouseReleaseEvent(QMouseEvent *e)
{
    m_isDragging = false;
    QDialog::mouseReleaseEvent(e);
}
