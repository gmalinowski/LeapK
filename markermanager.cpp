/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "persistentmarkhelper.h"
#include <QApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QLocale>
#include <QMessageBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <iostream>

struct OpenWindowInfo {
    QString app;
    QString caption;
    int desktop = 0;
};

static const QString s_presetColors[] = {
    QStringLiteral("#e74c3c"), // Red
    QStringLiteral("#e67e22"), // Orange
    QStringLiteral("#f1c40f"), // Amber / Yellow
    QStringLiteral("#2ecc71"), // Green
    QStringLiteral("#1abc9c"), // Teal / Cyan
    QStringLiteral("#3498db"), // Blue
    QStringLiteral("#9b59b6"), // Purple
    QStringLiteral("#e84393")  // Pink
};

class MainMarkerFrame : public QFrame
{
public:
    explicit MainMarkerFrame(QWidget *parent = nullptr) : QFrame(parent)
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

        // Top accent line (Marker purple/violet accent)
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(155, 89, 182, 230));
        p.drawRoundedRect(QRectF(r.left() + 24, r.top(), r.width() - 48, 3), 1.5, 1.5);
    }
};

class EditMarkerRuleDialog : public QDialog
{
public:
    EditMarkerRuleDialog(const LeapKMarks::PersistentMarkRule &initialRule, bool isNew, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_rule(initialRule)
        , m_isNew(isNew)
        , m_selectedColor(initialRule.color.isValid() ? initialRule.color : QColor(QStringLiteral("#3498db")))
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        setAttribute(Qt::WA_TranslucentBackground, true);
        resize(480, 520);

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);

        auto *frame = new MainMarkerFrame(this);
        rootLayout->addWidget(frame);

        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(22, 20, 22, 20);
        layout->setSpacing(12);

        // Header
        auto *lblTitle = new QLabel(this);
        if (m_isNew) {
            lblTitle->setText(QStringLiteral("🏷️ New Marker / Group Rule"));
        } else {
            lblTitle->setText(QStringLiteral("✏️ Edit Marker Rule"));
        }
        lblTitle->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: bold; color: #ffffff;"));
        layout->addWidget(lblTitle);

        auto *formLayout = new QGridLayout();
        formLayout->setSpacing(10);
        int row = 0;

        // 1. Jump Letter
        auto *lblLetter = new QLabel(QStringLiteral("Jump Letter (A-Z):"), this);
        lblLetter->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.85); font-size: 13px;"));
        formLayout->addWidget(lblLetter, row, 0);

        m_editLetter = new QLineEdit(this);
        m_editLetter->setMaxLength(1);
        m_editLetter->setText(m_rule.letter.isNull() ? QStringLiteral("A") : QString(m_rule.letter));
        m_editLetter->setFixedWidth(60);
        m_editLetter->setAlignment(Qt::AlignCenter);
        m_editLetter->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: rgba(255,255,255,0.1); border: 1.5px solid #3498db; border-radius: 6px;"
            "  color: #ffffff; font-size: 16px; font-weight: bold; padding: 4px;"
            "}"
        ));
        connect(m_editLetter, &QLineEdit::textChanged, this, [this](const QString &t) {
            if (!t.isEmpty()) {
                m_editLetter->setText(t.toUpper());
                updateColorPreview();
            }
        });
        formLayout->addWidget(m_editLetter, row, 1);
        row++;

        // 2. Kolor obrysu (Color)
        auto *lblColor = new QLabel(QStringLiteral("Outline Color:"), this);
        lblColor->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.85); font-size: 13px;"));
        formLayout->addWidget(lblColor, row, 0);

        auto *colorRow = new QHBoxLayout();
        colorRow->setSpacing(6);

        m_colorPreview = new QLabel(this);
        m_colorPreview->setFixedSize(30, 26);
        updateColorPreview();
        colorRow->addWidget(m_colorPreview);

        for (const auto &cHex : s_presetColors) {
            auto *btnP = new QPushButton(this);
            btnP->setFixedSize(22, 22);
            btnP->setCursor(Qt::PointingHandCursor);
            btnP->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border-radius: 11px; border: 1px solid rgba(255,255,255,0.3); }"
                "QPushButton:hover { border: 2px solid #ffffff; }"
            ).arg(cHex));
            connect(btnP, &QPushButton::clicked, this, [this, cHex]() {
                m_selectedColor = QColor(cHex);
                updateColorPreview();
            });
            colorRow->addWidget(btnP);
        }

        auto *btnCustomColor = new QPushButton(QStringLiteral("More…"), this);
        btnCustomColor->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: rgba(255,255,255,0.1); color: #ffffff; border-radius: 6px; padding: 4px 8px; font-size: 11px;"
            "}"
            "QPushButton:hover { background: rgba(255,255,255,0.2); }"
        ));
        connect(btnCustomColor, &QPushButton::clicked, this, [this]() {
            QColor c = QColorDialog::getColor(m_selectedColor, this, QStringLiteral("Select Outline Color"));
            if (c.isValid()) {
                m_selectedColor = c;
                updateColorPreview();
            }
        });
        colorRow->addWidget(btnCustomColor);
        colorRow->addStretch(1);

        formLayout->addLayout(colorRow, row, 1);
        row++;

        // 3. Etykieta grupy (Label)
        auto *lblLabel = new QLabel(QStringLiteral("Group Label:"), this);
        lblLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.85); font-size: 13px;"));
        formLayout->addWidget(lblLabel, row, 0);

        m_editLabel = new QLineEdit(this);
        m_editLabel->setPlaceholderText(QStringLiteral("e.g. Browser, Terminals, Editor"));
        m_editLabel->setText(m_rule.label);
        m_editLabel->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.2); border-radius: 6px;"
            "  color: #ffffff; font-size: 13px; padding: 6px 10px;"
            "}"
            "QLineEdit:focus { border: 1.5px solid #3498db; }"
        ));
        formLayout->addWidget(m_editLabel, row, 1);
        row++;

        // 4. Aplikacja / Klasa (App Class)
        auto *lblApp = new QLabel(QStringLiteral("Application (Class):"), this);
        lblApp->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.85); font-size: 13px;"));
        formLayout->addWidget(lblApp, row, 0);

        m_editApp = new QLineEdit(this);
        m_editApp->setPlaceholderText(QStringLiteral("e.g. firefox, org.kde.konsole, code"));
        m_editApp->setText(m_rule.matchApp);
        m_editApp->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.2); border-radius: 6px;"
            "  color: #ffffff; font-size: 13px; padding: 6px 10px;"
            "}"
            "QLineEdit:focus { border: 1.5px solid #3498db; }"
        ));
        formLayout->addWidget(m_editApp, row, 1);
        row++;

        // 5. Title / Process Pattern
        auto *lblCaption = new QLabel(QStringLiteral("Title / Process Pattern:"), this);
        lblCaption->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.85); font-size: 13px;"));
        formLayout->addWidget(lblCaption, row, 0);

        m_editCaption = new QLineEdit(this);
        m_editCaption->setPlaceholderText(QStringLiteral("e.g. nvim, localhost, GitHub (optional)"));
        m_editCaption->setText(m_rule.matchCaption);
        m_editCaption->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.2); border-radius: 6px;"
            "  color: #ffffff; font-size: 13px; padding: 6px 10px;"
            "}"
            "QLineEdit:focus { border: 1.5px solid #3498db; }"
        ));
        formLayout->addWidget(m_editCaption, row, 1);
        row++;

        // 6. Pulpit wirtualny (Desktop)
        auto *lblDesktop = new QLabel(QStringLiteral("Virtual Desktop:"), this);
        lblDesktop->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.85); font-size: 13px;"));
        formLayout->addWidget(lblDesktop, row, 0);

        m_comboDesktop = new QComboBox(this);
        m_comboDesktop->addItem(QStringLiteral("All Desktops (Any)"), 0);
        for (int d = 1; d <= 8; ++d) {
            m_comboDesktop->addItem(QStringLiteral("Desktop %1").arg(d), d);
        }
        for (int i = 0; i < m_comboDesktop->count(); ++i) {
            if (m_comboDesktop->itemData(i).toInt() == m_rule.matchDesktop) {
                m_comboDesktop->setCurrentIndex(i);
                break;
            }
        }
        m_comboDesktop->setStyleSheet(QStringLiteral(
            "QComboBox {"
            "  background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.2); border-radius: 6px;"
            "  color: #ffffff; font-size: 13px; padding: 6px 10px;"
            "}"
            "QComboBox QAbstractItemView {"
            "  background: #1e222b; color: #ffffff; selection-background-color: #3498db;"
            "}"
        ));
        formLayout->addWidget(m_comboDesktop, row, 1);
        row++;

        // 7. Enabled
        m_chkEnabled = new QCheckBox(QStringLiteral("Rule active (automatically mark matching windows)"), this);
        m_chkEnabled->setChecked(m_rule.enabled);
        m_chkEnabled->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 13px; padding: 4px 0;"));
        formLayout->addWidget(m_chkEnabled, row, 0, 1, 2);
        row++;

        layout->addLayout(formLayout);
        layout->addStretch(1);

        // Buttons
        auto *bottomLayout = new QHBoxLayout();
        bottomLayout->addStretch(1);

        auto *btnCancel = new QPushButton(QStringLiteral("Cancel [Esc]"), this);
        btnCancel->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: rgba(255, 255, 255, 0.1); border-radius: 8px; color: #ffffff; padding: 8px 16px;"
            "}"
            "QPushButton:hover { background: rgba(255, 255, 255, 0.2); }"
        ));
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
        bottomLayout->addWidget(btnCancel);

        auto *btnSave = new QPushButton(QStringLiteral("✓ Save Rule"), this);
        btnSave->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: #3498db; border-radius: 8px; color: #ffffff; font-weight: bold; padding: 8px 20px;"
            "}"
            "QPushButton:hover { background: #2980b9; }"
        ));
        connect(btnSave, &QPushButton::clicked, this, &EditMarkerRuleDialog::onSaveClicked);
        bottomLayout->addWidget(btnSave);

        layout->addLayout(bottomLayout);
    }

    LeapKMarks::PersistentMarkRule rule() const { return m_rule; }

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape) {
            reject();
            return;
        }
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            onSaveClicked();
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
    void updateColorPreview()
    {
        m_colorPreview->setStyleSheet(QStringLiteral(
            "background-color: %1; border-radius: 6px; border: 1.5px solid rgba(255,255,255,0.6);"
        ).arg(m_selectedColor.name(QColor::HexRgb)));
    }

    void onSaveClicked()
    {
        QString let = m_editLetter->text().trimmed().toUpper();
        if (let.isEmpty()) {
            let = QStringLiteral("A");
        }
        m_rule.letter = let.at(0);
        m_rule.color = m_selectedColor;
        m_rule.label = m_editLabel->text().trimmed();
        m_rule.matchApp = m_editApp->text().trimmed();
        m_rule.matchCaption = m_editCaption->text().trimmed();
        m_rule.matchDesktop = m_comboDesktop->currentData().toInt();
        m_rule.enabled = m_chkEnabled->isChecked();
        if (m_rule.id.isEmpty()) {
            m_rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        if (m_rule.createdAt <= 0) {
            m_rule.createdAt = QDateTime::currentMSecsSinceEpoch();
        }
        accept();
    }

    LeapKMarks::PersistentMarkRule m_rule;
    bool m_isNew;
    QColor m_selectedColor;
    QLineEdit *m_editLetter = nullptr;
    QLabel *m_colorPreview = nullptr;
    QLineEdit *m_editLabel = nullptr;
    QLineEdit *m_editApp = nullptr;
    QLineEdit *m_editCaption = nullptr;
    QComboBox *m_comboDesktop = nullptr;
    QCheckBox *m_chkEnabled = nullptr;
    QPoint m_dragPos;
    bool m_isDragging = false;
};

class MarkerRuleCard : public QFrame
{
public:
    MarkerRuleCard(const LeapKMarks::PersistentMarkRule &rule,
                   int matchingWindowCount,
                   const QStringList &matchingWindowTitles,
                   QWidget *parent = nullptr)
        : QFrame(parent)
        , m_rule(rule)
        , m_matchingCount(matchingWindowCount)
        , m_matchingTitles(matchingWindowTitles)
    {
        setObjectName(QStringLiteral("ruleCard"));
        updateStyle();

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(14);

        // 1. Big Letter Badge
        m_lblBadge = new QLabel(rule.letter.isNull() ? QStringLiteral("?") : QString(rule.letter), this);
        m_lblBadge->setFixedSize(38, 38);
        m_lblBadge->setAlignment(Qt::AlignCenter);
        const QString badgeBg = rule.color.isValid() ? rule.color.name(QColor::HexRgb) : QStringLiteral("#3498db");
        m_lblBadge->setStyleSheet(QStringLiteral(
            "background-color: %1; color: #ffffff; font-weight: bold; font-size: 18px; border-radius: 10px; border: 1.5px solid rgba(255,255,255,0.4);"
        ).arg(badgeBg));
        layout->addWidget(m_lblBadge);

        // 2. Middle Content: Label, Rules, Match Info
        auto *vbox = new QVBoxLayout();
        vbox->setSpacing(2);

        // Title row (Label / App Name)
        auto *titleRow = new QHBoxLayout();
        titleRow->setSpacing(8);

        const QString displayTitle = !rule.label.trimmed().isEmpty()
            ? rule.label.trimmed()
            : (!rule.matchApp.trimmed().isEmpty() ? rule.matchApp.trimmed() : QStringLiteral("Unnamed Group"));

        auto *lblTitle = new QLabel(QStringLiteral("<b>%1</b>").arg(displayTitle.toHtmlEscaped()), this);
        lblTitle->setTextFormat(Qt::RichText);
        lblTitle->setStyleSheet(QStringLiteral("font-size: 15px; color: #ffffff; border: none; background: transparent;"));
        titleRow->addWidget(lblTitle);

        if (!rule.label.trimmed().isEmpty() && !rule.matchApp.trimmed().isEmpty()) {
            auto *lblSubApp = new QLabel(QStringLiteral("• <span style='color:rgba(255,255,255,0.55);'>%1</span>").arg(rule.matchApp.toHtmlEscaped()), this);
            lblSubApp->setTextFormat(Qt::RichText);
            lblSubApp->setStyleSheet(QStringLiteral("font-size: 12px; border: none; background: transparent;"));
            titleRow->addWidget(lblSubApp);
        }
        titleRow->addStretch(1);
        vbox->addLayout(titleRow);

        // Criteria details
        QStringList criteria;
        if (!rule.matchApp.trimmed().isEmpty()) {
            criteria.append(QStringLiteral("App: <b style='color:#3498db;'>%1</b>").arg(rule.matchApp.toHtmlEscaped()));
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
        lblCriteria->setStyleSheet(QStringLiteral("font-size: 12px; color: rgba(255,255,255,0.7); border: none; background: transparent;"));
        vbox->addWidget(lblCriteria);

        // Live Matching Windows Row
        auto *lblLive = new QLabel(this);
        if (m_matchingCount > 0) {
            const QString winCountStr = (m_matchingCount == 1 ? QStringLiteral("1 open window") : QStringLiteral("%1 open windows").arg(m_matchingCount));
            lblLive->setText(QStringLiteral("<span style='color:#2ecc71; font-weight:bold;'>● %1</span>: <span style='color:rgba(255,255,255,0.65);'>%2</span>")
                .arg(winCountStr, m_matchingTitles.join(QStringLiteral(", ")).left(70).toHtmlEscaped()));
            if (!m_matchingTitles.isEmpty()) {
                lblLive->setToolTip(m_matchingTitles.join(QLatin1Char('\n')));
            }
        } else {
            lblLive->setText(QStringLiteral("<span style='color:rgba(255,255,255,0.35);'>○ 0 open windows in this group</span>"));
        }
        lblLive->setTextFormat(Qt::RichText);
        lblLive->setStyleSheet(QStringLiteral("font-size: 11px; border: none; background: transparent;"));
        vbox->addWidget(lblLive);

        layout->addLayout(vbox, 1);

        // 3. Right Side Actions: Toggle Enable, Edit, Delete
        auto *btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(6);

        m_btnToggle = new QPushButton(this);
        m_btnToggle->setCursor(Qt::PointingHandCursor);
        updateToggleStyle();
        connect(m_btnToggle, &QPushButton::clicked, this, [this]() {
            m_rule.enabled = !m_rule.enabled;
            updateStyle();
            updateToggleStyle();
            if (onToggleEnabled) {
                onToggleEnabled(m_rule);
            }
        });
        btnLayout->addWidget(m_btnToggle);

        auto *btnEdit = new QPushButton(QStringLiteral("✏️"), this);
        btnEdit->setFixedSize(32, 28);
        btnEdit->setCursor(Qt::PointingHandCursor);
        btnEdit->setToolTip(QStringLiteral("Edit this rule"));
        btnEdit->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: rgba(255,255,255,0.1); color: #ffffff; border-radius: 6px; border: 1px solid rgba(255,255,255,0.2);"
            "}"
            "QPushButton:hover { background: rgba(52, 152, 219, 0.4); border-color: #3498db; }"
        ));
        connect(btnEdit, &QPushButton::clicked, this, [this]() {
            if (onEditClicked) {
                onEditClicked(m_rule);
            }
        });
        btnLayout->addWidget(btnEdit);

        auto *btnDelete = new QPushButton(QStringLiteral("🗑"), this);
        btnDelete->setFixedSize(32, 28);
        btnDelete->setCursor(Qt::PointingHandCursor);
        btnDelete->setToolTip(QStringLiteral("Delete this rule"));
        btnDelete->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: rgba(231, 76, 60, 0.15); color: #e74c3c; border-radius: 6px; border: 1px solid rgba(231, 76, 60, 0.35);"
            "}"
            "QPushButton:hover { background: rgba(231, 76, 60, 0.4); color: #ffffff; }"
        ));
        connect(btnDelete, &QPushButton::clicked, this, [this]() {
            if (onDeleteClicked) {
                onDeleteClicked(m_rule);
            }
        });
        btnLayout->addWidget(btnDelete);

        layout->addLayout(btnLayout);
    }

    LeapKMarks::PersistentMarkRule rule() const { return m_rule; }

    std::function<void(const LeapKMarks::PersistentMarkRule &)> onToggleEnabled;
    std::function<void(const LeapKMarks::PersistentMarkRule &)> onEditClicked;
    std::function<void(const LeapKMarks::PersistentMarkRule &)> onDeleteClicked;

private:
    void updateStyle()
    {
        if (m_rule.enabled) {
            setStyleSheet(QStringLiteral(
                "QFrame#ruleCard {"
                "  background-color: rgba(255, 255, 255, 0.06);"
                "  border: 1px solid rgba(255, 255, 255, 0.15);"
                "  border-radius: 12px;"
                "}"
                "QFrame#ruleCard:hover {"
                "  background-color: rgba(255, 255, 255, 0.11);"
                "  border-color: rgba(155, 89, 182, 0.6);"
                "}"
            ));
        } else {
            setStyleSheet(QStringLiteral(
                "QFrame#ruleCard {"
                "  background-color: rgba(255, 255, 255, 0.02);"
                "  border: 1px dashed rgba(255, 255, 255, 0.12);"
                "  border-radius: 12px;"
                "}"
                "QFrame#ruleCard:hover {"
                "  background-color: rgba(255, 255, 255, 0.05);"
                "}"
            ));
        }
    }

    void updateToggleStyle()
    {
        if (m_rule.enabled) {
            m_btnToggle->setText(QStringLiteral("✓ Active"));
            m_btnToggle->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background: rgba(46, 204, 113, 0.2); color: #2ecc71; font-weight: bold; font-size: 11px;"
                "  border: 1px solid rgba(46, 204, 113, 0.5); border-radius: 6px; padding: 5px 10px;"
                "}"
                "QPushButton:hover { background: rgba(46, 204, 113, 0.35); }"
            ));
        } else {
            m_btnToggle->setText(QStringLiteral("⏸ Disabled"));
            m_btnToggle->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background: rgba(255, 255, 255, 0.08); color: rgba(255, 255, 255, 0.5); font-size: 11px;"
                "  border: 1px solid rgba(255, 255, 255, 0.15); border-radius: 6px; padding: 5px 10px;"
                "}"
                "QPushButton:hover { background: rgba(255, 255, 255, 0.15); color: #ffffff; }"
            ));
        }
    }

    LeapKMarks::PersistentMarkRule m_rule;
    int m_matchingCount;
    QStringList m_matchingTitles;
    QLabel *m_lblBadge = nullptr;
    QPushButton *m_btnToggle = nullptr;
};

class MarkerManagerDialog : public QDialog
{
public:
    MarkerManagerDialog(const QString &appClass,
                        const QString &windowCaption,
                        int desktopNum,
                        const QString &desktopName,
                        const QList<OpenWindowInfo> &openWindows,
                        QWidget *parent = nullptr)
        : QDialog(parent)
        , m_appClass(appClass)
        , m_windowCaption(windowCaption)
        , m_desktopNum(desktopNum)
        , m_desktopName(desktopName)
        , m_openWindows(openWindows)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        setAttribute(Qt::WA_TranslucentBackground, true);
        resize(640, 600);

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);

        m_mainFrame = new MainMarkerFrame(this);
        rootLayout->addWidget(m_mainFrame);

        auto *mainLayout = new QVBoxLayout(m_mainFrame);
        mainLayout->setContentsMargins(22, 20, 22, 20);
        mainLayout->setSpacing(14);

        // Header
        auto *headerLayout = new QVBoxLayout();
        headerLayout->setSpacing(4);

        auto *lblTitle = new QLabel(QStringLiteral("🏷️ Marker & Group Manager"), this);
        lblTitle->setStyleSheet(QStringLiteral("font-size: 19px; font-weight: bold; color: #ffffff;"));
        headerLayout->addWidget(lblTitle);

        if (!m_appClass.isEmpty()) {
            const QString subText = QStringLiteral("Current window: <b style='color:#3498db;'>%1</b> &bull; <span style='color:rgba(255,255,255,0.7);'>%2</span> (Desktop %3)")
                    .arg(m_appClass.toHtmlEscaped(), m_windowCaption.left(30).toHtmlEscaped(), QString::number(m_desktopNum));
            auto *lblSub = new QLabel(subText, this);
            lblSub->setTextFormat(Qt::RichText);
            lblSub->setStyleSheet(QStringLiteral("font-size: 12px;"));
            headerLayout->addWidget(lblSub);
        } else {
            auto *lblSub = new QLabel(QStringLiteral("Browse, create, and edit persistent rules for letters, colors, and window groups"), this);
            lblSub->setStyleSheet(QStringLiteral("font-size: 12px; color: rgba(255,255,255,0.65);"));
            headerLayout->addWidget(lblSub);
        }
        mainLayout->addLayout(headerLayout);

        // Top Action Bar: Search + Add Rule + Assign Current
        auto *actionLayout = new QHBoxLayout();
        actionLayout->setSpacing(8);

        m_searchEdit = new QLineEdit(this);
        m_searchEdit->setPlaceholderText(QStringLiteral("🔍 Filter rules, letters, apps..."));
        m_searchEdit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.18); border-radius: 8px;"
            "  color: #ffffff; font-size: 13px; padding: 6px 12px;"
            "}"
            "QLineEdit:focus { border: 1.5px solid #9b59b6; }"
        ));
        connect(m_searchEdit, &QLineEdit::textChanged, this, &MarkerManagerDialog::rebuildCardList);
        actionLayout->addWidget(m_searchEdit, 1);

        if (!m_appClass.isEmpty()) {
            auto *btnAssignCurrent = new QPushButton(QStringLiteral("+ Assign Window"), this);
            btnAssignCurrent->setCursor(Qt::PointingHandCursor);
            btnAssignCurrent->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background: rgba(46, 204, 113, 0.2); border: 1.5px solid #2ecc71; border-radius: 8px;"
                "  color: #2ecc71; font-weight: bold; font-size: 12px; padding: 6px 12px;"
                "}"
                "QPushButton:hover { background: rgba(46, 204, 113, 0.35); }"
            ));
            connect(btnAssignCurrent, &QPushButton::clicked, this, &MarkerManagerDialog::onAssignCurrentClicked);
            actionLayout->addWidget(btnAssignCurrent);
        }

        auto *btnAddRule = new QPushButton(QStringLiteral("+ New Rule"), this);
        btnAddRule->setCursor(Qt::PointingHandCursor);
        btnAddRule->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: #9b59b6; border-radius: 8px; color: #ffffff; font-weight: bold; font-size: 12px; padding: 6px 14px;"
            "}"
            "QPushButton:hover { background: #8e44ad; }"
        ));
        connect(btnAddRule, &QPushButton::clicked, this, &MarkerManagerDialog::onAddNewRuleClicked);
        actionLayout->addWidget(btnAddRule);

        mainLayout->addLayout(actionLayout);

        // Scroll Area with Rule Cards
        auto *scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setStyleSheet(QStringLiteral(
            "QScrollArea { border: none; background: transparent; }"
            "QScrollBar:vertical { background: rgba(255,255,255,0.05); width: 8px; border-radius: 4px; }"
            "QScrollBar::handle:vertical { background: rgba(255,255,255,0.25); border-radius: 4px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        ));

        m_cardsContainer = new QWidget();
        m_cardsContainer->setStyleSheet(QStringLiteral("background: transparent;"));
        m_cardsLayout = new QVBoxLayout(m_cardsContainer);
        m_cardsLayout->setContentsMargins(0, 4, 0, 4);
        m_cardsLayout->setSpacing(8);

        scrollArea->setWidget(m_cardsContainer);
        mainLayout->addWidget(scrollArea, 1);

        // Bottom Bar
        auto *bottomLayout = new QHBoxLayout();
        m_lblStats = new QLabel(this);
        m_lblStats->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.6); font-size: 12px;"));
        bottomLayout->addWidget(m_lblStats);
        bottomLayout->addStretch(1);

        auto *btnClearAll = new QPushButton(QStringLiteral("Clear All…"), this);
        btnClearAll->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: rgba(231, 76, 60, 0.12); color: #e74c3c; border-radius: 8px; font-size: 12px; padding: 7px 12px;"
            "}"
            "QPushButton:hover { background: rgba(231, 76, 60, 0.25); }"
        ));
        connect(btnClearAll, &QPushButton::clicked, this, &MarkerManagerDialog::onClearAllClicked);
        bottomLayout->addWidget(btnClearAll);

        auto *btnClose = new QPushButton(QStringLiteral("Close [Esc]"), this);
        btnClose->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: rgba(255,255,255,0.12); border-radius: 8px; color: #ffffff; padding: 7px 18px; font-size: 12px;"
            "}"
            "QPushButton:hover { background: rgba(255,255,255,0.22); }"
        ));
        connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
        bottomLayout->addWidget(btnClose);

        mainLayout->addLayout(bottomLayout);

        loadRules();
        rebuildCardList();
    }

    QList<LeapKMarks::PersistentMarkRule> rules() const { return m_rules; }

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape) {
            accept();
            return;
        }
        if (e->modifiers() & Qt::ControlModifier && e->key() == Qt::Key_N) {
            onAddNewRuleClicked();
            return;
        }
        if (e->modifiers() & Qt::ControlModifier && e->key() == Qt::Key_F) {
            m_searchEdit->setFocus();
            m_searchEdit->selectAll();
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
    void loadRules()
    {
        m_rules = LeapKMarks::loadPersistentMarks();
    }

    void saveRules()
    {
        LeapKMarks::savePersistentMarks(m_rules);
    }

    int countMatchesForRule(const LeapKMarks::PersistentMarkRule &rule, QStringList *matchingTitles = nullptr) const
    {
        int count = 0;
        for (const auto &win : m_openWindows) {
            const int score = LeapKMarks::scoreWindowMatch(rule, win.app, win.caption, win.desktop);
            if (score > 0) {
                count++;
                if (matchingTitles) {
                    const QString t = win.caption.isEmpty() ? win.app : win.caption;
                    matchingTitles->append(t);
                }
            }
        }
        return count;
    }

    void rebuildCardList()
    {
        // Clear layout
        QLayoutItem *item;
        while ((item = m_cardsLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }

        const QString filter = m_searchEdit->text().trimmed().toLower();
        int activeCount = 0;
        int displayedCount = 0;

        for (int i = 0; i < m_rules.size(); ++i) {
            const auto &rule = m_rules.at(i);
            if (rule.enabled) {
                activeCount++;
            }

            // Filter check
            if (!filter.isEmpty()) {
                const QString letStr = QString(rule.letter).toLower();
                const QString appStr = rule.matchApp.toLower();
                const QString capStr = rule.matchCaption.toLower();
                const QString lblStr = rule.label.toLower();
                if (!letStr.contains(filter) && !appStr.contains(filter) && !capStr.contains(filter) && !lblStr.contains(filter)) {
                    continue;
                }
            }

            QStringList titles;
            const int matchCount = countMatchesForRule(rule, &titles);

            auto *card = new MarkerRuleCard(rule, matchCount, titles, m_cardsContainer);
            card->onToggleEnabled = [this](const LeapKMarks::PersistentMarkRule &updated) {
                LeapKMarks::upsertPersistentMark(m_rules, updated);
                rebuildCardList();
            };
            card->onEditClicked = [this](const LeapKMarks::PersistentMarkRule &r) {
                EditMarkerRuleDialog dlg(r, false, this);
                if (dlg.exec() == QDialog::Accepted) {
                    LeapKMarks::upsertPersistentMark(m_rules, dlg.rule());
                    rebuildCardList();
                }
            };
            card->onDeleteClicked = [this](const LeapKMarks::PersistentMarkRule &r) {
                const QString msg = QStringLiteral("Are you sure you want to delete the rule for group [%1]?").arg(r.letter.isNull() ? r.label : QString(r.letter));
                if (QMessageBox::question(this, QStringLiteral("Delete Rule"), msg) == QMessageBox::Yes) {
                    LeapKMarks::deletePersistentMark(m_rules, r.id);
                    rebuildCardList();
                }
            };

            m_cardsLayout->addWidget(card);
            displayedCount++;
        }

        if (displayedCount == 0) {
            auto *lblEmpty = new QLabel(this);
            lblEmpty->setAlignment(Qt::AlignCenter);
            if (m_rules.isEmpty()) {
                lblEmpty->setText(QStringLiteral("<p style='color:rgba(255,255,255,0.6); font-size:14px; margin-top:40px;'>"
                                                 "No marker rules defined yet.<br><br>"
                                                 "Click <b>[+ New Rule]</b> or press <b>Meta+Ctrl+Space</b> on any window to create one.</p>"));
            } else {
                lblEmpty->setText(QStringLiteral("<p style='color:rgba(255,255,255,0.5); font-size:13px; margin-top:30px;'>"
                                                 "No rules match the current search filter.</p>"));
            }
            lblEmpty->setTextFormat(Qt::RichText);
            m_cardsLayout->addWidget(lblEmpty);
        }

        m_cardsLayout->addStretch(1);

        // Update stats
        m_lblStats->setText(QStringLiteral("Total rules: %1 (active: %2)").arg(m_rules.size()).arg(activeCount));
    }

    void onAddNewRuleClicked()
    {
        LeapKMarks::PersistentMarkRule newRule;
        newRule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        newRule.letter = nextAvailableLetter();
        newRule.color = QColor(QStringLiteral("#3498db"));
        newRule.enabled = true;
        newRule.createdAt = QDateTime::currentMSecsSinceEpoch();

        EditMarkerRuleDialog dlg(newRule, true, this);
        if (dlg.exec() == QDialog::Accepted) {
            LeapKMarks::upsertPersistentMark(m_rules, dlg.rule());
            rebuildCardList();
        }
    }

    void onAssignCurrentClicked()
    {
        LeapKMarks::PersistentMarkRule newRule;
        newRule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        newRule.letter = nextAvailableLetter();
        newRule.color = QColor(QStringLiteral("#2ecc71"));
        newRule.matchApp = m_appClass;
        newRule.label = m_appClass;
        newRule.matchCaption = QString();
        newRule.matchDesktop = 0;
        newRule.enabled = true;
        newRule.createdAt = QDateTime::currentMSecsSinceEpoch();

        EditMarkerRuleDialog dlg(newRule, true, this);
        if (dlg.exec() == QDialog::Accepted) {
            LeapKMarks::upsertPersistentMark(m_rules, dlg.rule());
            rebuildCardList();
        }
    }

    void onClearAllClicked()
    {
        const QString msg = QStringLiteral("Are you sure you want to delete all (%1) marker rules? This action cannot be undone.").arg(m_rules.size());
        if (QMessageBox::question(this, QStringLiteral("Clear All Rules"), msg) == QMessageBox::Yes) {
            m_rules.clear();
            saveRules();
            rebuildCardList();
        }
    }

    QChar nextAvailableLetter() const
    {
        QSet<QChar> used;
        for (const auto &r : m_rules) {
            if (!r.letter.isNull()) {
                used.insert(r.letter.toUpper());
            }
        }
        const QString pool = QStringLiteral("ASDFGHJKLZXCVBNMQWERTYUIOP123456789");
        for (const QChar &ch : pool) {
            if (!used.contains(ch)) {
                return ch;
            }
        }
        return QChar('A');
    }

    QString m_appClass;
    QString m_windowCaption;
    int m_desktopNum;
    QString m_desktopName;
    QList<OpenWindowInfo> m_openWindows;
    QList<LeapKMarks::PersistentMarkRule> m_rules;

    MainMarkerFrame *m_mainFrame = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QWidget *m_cardsContainer = nullptr;
    QVBoxLayout *m_cardsLayout = nullptr;
    QLabel *m_lblStats = nullptr;
    QPoint m_dragPos;
    bool m_isDragging = false;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("leapk-marker-manager"));
    app.setApplicationDisplayName(QObject::tr("LeapK - Marker Manager"));

    // CLI list flag
    if (argc > 1 && QString::fromUtf8(argv[1]) == QLatin1String("--list")) {
        const auto rules = LeapKMarks::loadPersistentMarks();
        QJsonArray arr;
        for (const auto &r : rules) {
            arr.append(LeapKMarks::ruleToJson(r));
        }
        std::cout << QJsonDocument(arr).toJson(QJsonDocument::Indented).toStdString() << std::endl;
        return 0;
    }

    // CLI delete flag
    if (argc > 2 && QString::fromUtf8(argv[1]) == QLatin1String("--delete")) {
        const QString delId = QString::fromUtf8(argv[2]).trimmed();
        auto rules = LeapKMarks::loadPersistentMarks();
        bool ok = LeapKMarks::deletePersistentMark(rules, delId);
        if (ok) {
            std::cout << "Deleted rule: " << delId.toStdString() << std::endl;
            return 0;
        } else {
            std::cerr << "Rule not found: " << delId.toStdString() << std::endl;
            return 1;
        }
    }

    QString appClass;
    QString windowCaption;
    int desktopNum = 0;
    QString desktopName;
    QList<OpenWindowInfo> openWindows;

    // Parse CLI parameters:
    // [appClass] [caption] [desktopNum] [desktopName] [--open-windows <json>]
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromUtf8(argv[i]);
        if (arg == QLatin1String("--open-windows") && (i + 1) < argc) {
            const QString winJson = QString::fromUtf8(argv[++i]);
            const QJsonDocument doc = QJsonDocument::fromJson(winJson.toUtf8());
            if (doc.isArray()) {
                for (const auto &val : doc.array()) {
                    if (val.isObject()) {
                        const QJsonObject obj = val.toObject();
                        OpenWindowInfo info;
                        info.app = obj.value(QStringLiteral("app")).toString();
                        info.caption = obj.value(QStringLiteral("caption")).toString();
                        info.desktop = obj.value(QStringLiteral("desktop")).toInt(0);
                        openWindows.append(info);
                    }
                }
            }
        } else if (appClass.isEmpty()) {
            appClass = arg.trimmed();
        } else if (windowCaption.isEmpty()) {
            windowCaption = arg.trimmed();
        } else if (desktopNum == 0 && arg.toInt() > 0) {
            desktopNum = arg.toInt();
        } else if (desktopName.isEmpty()) {
            desktopName = arg.trimmed();
        }
    }

    MarkerManagerDialog dlg(appClass, windowCaption, desktopNum, desktopName, openWindows);
    if (dlg.exec() == QDialog::Accepted) {
        return 0;
    }
    return 0;
}
