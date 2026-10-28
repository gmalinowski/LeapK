/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QDialog>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QColorDialog>
#include <QMessageBox>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QRandomGenerator>
#include <QProcess>
#include <cmath>
#include <functional>
#include <iostream>
#include "badgehelper.h"

static const char* const s_paletteKeys[8] = {"A", "S", "D", "F", "G", "H", "J", "K"};
static const Qt::Key s_paletteQtKeys[8] = {
    Qt::Key_A, Qt::Key_S, Qt::Key_D, Qt::Key_F,
    Qt::Key_G, Qt::Key_H, Qt::Key_J, Qt::Key_K
};
static const Qt::Key s_paletteNumKeys[8] = {
    Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4,
    Qt::Key_5, Qt::Key_6, Qt::Key_7, Qt::Key_8
};

// Main card with LeapK HUD dark acrylic style
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

        // Top accent line
        p.setPen(QPen(QColor(52, 152, 219, 150), 1.5));
        QPainterPath topPath;
        topPath.moveTo(r.left() + 16, r.top());
        topPath.lineTo(r.right() - 16, r.top());
        p.drawPath(topPath);
    }
};

// Clickable HUD Option Card
class MarkerOptionCard : public QFrame
{
public:
    explicit MarkerOptionCard(QWidget *parent = nullptr) : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setObjectName(QStringLiteral("hudCard"));
        setSelected(false);
    }

    void setSelected(bool sel)
    {
        if (sel) {
            setStyleSheet(QStringLiteral(
                "QFrame#hudCard {"
                "  background-color: rgba(52, 152, 219, 0.25);"
                "  border: 1.5px solid #3498db;"
                "  border-radius: 12px;"
                "}"
            ));
        } else {
            setStyleSheet(QStringLiteral(
                "QFrame#hudCard {"
                "  background-color: rgba(255, 255, 255, 0.06);"
                "  border: 1px solid rgba(255, 255, 255, 0.15);"
                "  border-radius: 12px;"
                "}"
                "QFrame#hudCard:hover {"
                "  background-color: rgba(255, 255, 255, 0.11);"
                "  border: 1px solid rgba(52, 152, 219, 0.8);"
                "}"
            ));
        }
    }

    std::function<void()> onClicked;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            if (onClicked) {
                onClicked();
            }
        }
        QFrame::mouseReleaseEvent(event);
    }
};

// Popup dialog for entering child process or caption pattern cleanly
class PatternFilterDialog : public QDialog
{
public:
    PatternFilterDialog(const QString &currentPattern, bool isPolish, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_pattern(currentPattern)
        , m_isPolish(isPolish)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setFixedSize(500, 220);

        auto *dialogLayout = new QVBoxLayout(this);
        dialogLayout->setContentsMargins(4, 4, 4, 4);

        auto *card = new QFrame(this);
        card->setStyleSheet(QStringLiteral(
            "QFrame {"
            "  background-color: rgba(18, 21, 26, 252);"
            "  border: 1.5px solid #e67e22;"
            "  border-radius: 14px;"
            "}"
        ));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(20, 18, 20, 18);
        layout->setSpacing(12);

        auto *title = new QLabel(m_isPolish ? QStringLiteral("<b>Zawęź do procesu lub tytułu okna</b>") : QStringLiteral("<b>Narrow to Child Process or Window Title</b>"), card);
        title->setStyleSheet(QStringLiteral("font-size: 15px; color: #ffffff; border: none; background: transparent;"));
        title->setTextFormat(Qt::RichText);
        layout->addWidget(title);

        auto *desc = new QLabel(m_isPolish
            ? QStringLiteral("Wpisz fragment tytułu lub nazwę procesu potomnego (np. nvim, ssh, vim, dev, bash):")
            : QStringLiteral("Enter title substring or child process name (e.g. nvim, ssh, vim, dev, bash):"), card);
        desc->setStyleSheet(QStringLiteral("font-size: 12px; color: rgba(255, 255, 255, 0.75); border: none; background: transparent;"));
        desc->setWordWrap(true);
        layout->addWidget(desc);

        m_edit = new QLineEdit(m_pattern, card);
        m_edit->setPlaceholderText(m_isPolish ? QStringLiteral("np. nvim, ssh, bash...") : QStringLiteral("e.g. nvim, ssh, bash..."));
        m_edit->setClearButtonEnabled(true);
        m_edit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background-color: rgba(0, 0, 0, 0.55);"
            "  border: 1.5px solid rgba(255, 255, 255, 0.3);"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  padding: 8px 12px;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "}"
            "QLineEdit:focus {"
            "  border: 1.5px solid #e67e22;"
            "}"
        ));
        layout->addWidget(m_edit);

        auto *btnRow = new QHBoxLayout();
        btnRow->setSpacing(10);
        btnRow->addStretch(1);

        auto *btnCancel = new QPushButton(m_isPolish ? QStringLiteral("Anuluj (Esc)") : QStringLiteral("Cancel (Esc)"), card);
        btnCancel->setFocusPolicy(Qt::NoFocus);
        btnCancel->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.08);"
            "  border: 1px solid rgba(255, 255, 255, 0.2);"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  padding: 8px 16px;"
            "  font-size: 13px;"
            "}"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.15); }"
        ));
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
        btnRow->addWidget(btnCancel);

        auto *btnOk = new QPushButton(m_isPolish ? QStringLiteral("✓ Zastosuj (Enter)") : QStringLiteral("✓ Apply (Enter)"), card);
        btnOk->setFocusPolicy(Qt::NoFocus);
        btnOk->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: #e67e22;"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  font-weight: bold;"
            "  padding: 8px 20px;"
            "  font-size: 13px;"
            "}"
            "QPushButton:hover { background-color: #d35400; }"
        ));
        connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
        btnRow->addWidget(btnOk);

        layout->addLayout(btnRow);
        dialogLayout->addWidget(card);

        connect(m_edit, &QLineEdit::returnPressed, this, &QDialog::accept);
    }

    QString pattern() const { return m_edit->text().trimmed(); }

protected:
    void showEvent(QShowEvent *event) override
    {
        QDialog::showEvent(event);
        if (parentWidget()) {
            move(parentWidget()->geometry().center().x() - width() / 2,
                 parentWidget()->geometry().center().y() - height() / 2);
        }
        m_edit->setFocus();
        m_edit->selectAll();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            reject();
            return;
        }
        QDialog::keyPressEvent(event);
    }

private:
    QString m_pattern;
    bool m_isPolish;
    QLineEdit *m_edit;
};

class CustomMarkerDialog : public QDialog
{
public:
    CustomMarkerDialog(const QColor &initialColor,
                       const QString &initialLetter,
                       bool initialUseLetter,
                       bool initialUseColor,
                       const QString &initialLabel,
                       bool initialPersistent,
                       const QString &appClass,
                       const QString &windowCaption,
                       int desktopNum,
                       const QString &desktopName,
                       bool isAlreadyMarked,
                       const QJsonObject &usedLettersMap,
                       QWidget *parent = nullptr)
        : QDialog(parent)
        , m_color(initialColor.isValid() ? initialColor : QColor(QStringLiteral("#3498db")))
        , m_useColor(initialUseColor)
        , m_letter(initialLetter.trimmed().toUpper())
        , m_useLetter(initialUseLetter)
        , m_label(initialLabel.trimmed())
        , m_groupByApp(true)
        , m_persistent(initialPersistent)
        , m_desktopNum(desktopNum)
        , m_desktopName(desktopName)
        , m_matchDesktop(false)
        , m_appClass(appClass)
        , m_windowCaption(windowCaption)
        , m_isAlreadyMarked(isAlreadyMarked)
        , m_markRemoved(false)
        , m_usedLettersMap(usedLettersMap)
        , m_isPolish(QLocale().language() == QLocale::Polish)
        , m_currentStep(0)
        , m_seedOffset(QRandomGenerator::global()->bounded(360))
        , m_rerollCount(0)
        , m_isDragging(false)
    {
        if (m_letter.isEmpty()) {
            m_letter = QStringLiteral("A");
        }

        // Detect default caption filter keyword
        if (!m_windowCaption.isEmpty()) {
            const QString cap = m_windowCaption.trimmed();
            if (cap.contains(QStringLiteral("nvim"), Qt::CaseInsensitive)) {
                m_detectedCaption = QStringLiteral("nvim");
            } else if (cap.contains(QStringLiteral("vim"), Qt::CaseInsensitive)) {
                m_detectedCaption = QStringLiteral("vim");
            } else if (cap.contains(QStringLiteral("ssh"), Qt::CaseInsensitive)) {
                m_detectedCaption = QStringLiteral("ssh");
            } else if (cap.contains(QStringLiteral("bash"), Qt::CaseInsensitive)) {
                m_detectedCaption = QStringLiteral("bash");
            } else {
                m_detectedCaption = cap.left(30);
            }
        }
        m_matchCaption = m_detectedCaption;

        // System font adaptation
        const QFont sysFont = QApplication::font();
        m_basePt = sysFont.pointSizeF() > 0 ? sysFont.pointSizeF() : 10.0;

        // Frameless translucent HUD window with comfortable dimensions
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Dialog);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setFixedSize(600, 480);

        regeneratePalette();
        initUI();
        goToStep(0);
    }

    bool isMarkRemoved() const { return m_markRemoved; }
    bool useLetter() const { return m_useLetter; }
    QString letter() const { return m_letter; }
    bool useColor() const { return m_useColor; }
    QColor color() const { return m_color; }
    QString label() const
    {
        if (m_labelEdit) {
            return m_labelEdit->text().trimmed();
        }
        return m_label;
    }
    bool groupByApp() const { return m_groupByApp; }
    QString matchCaption() const { return (m_groupByApp && !m_matchCaption.isEmpty()) ? m_matchCaption : QString(); }
    bool persistent() const { return m_persistent; }
    int matchDesktop() const { return (m_matchDesktop && m_desktopNum > 0) ? m_desktopNum : 0; }
    bool reassignLetter() const { return m_reassignLetter; }

protected:
    void showEvent(QShowEvent *event) override
    {
        QDialog::showEvent(event);
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen) {
            const QRect geom = screen->availableGeometry();
            move(geom.center().x() - width() / 2, geom.center().y() - height() / 2);
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            m_isDragging = true;
        }
        QDialog::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
            move(event->globalPosition().toPoint() - m_dragPos);
        }
        QDialog::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_isDragging = false;
        QDialog::mouseReleaseEvent(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEv = static_cast<QKeyEvent*>(event);
            if (watched == m_labelEdit) {
                if (keyEv->key() == Qt::Key_Return || keyEv->key() == Qt::Key_Enter) {
                    m_label = m_labelEdit->text().trimmed();
                    tryFinishMark();
                    return true;
                }
                if (keyEv->key() == Qt::Key_Backspace && m_labelEdit->text().isEmpty()) {
                    goToStep(2);
                    return true;
                }
                if (keyEv->key() == Qt::Key_Escape) {
                    m_labelEdit->clearFocus();
                    setFocus();
                    return true;
                }
            }
        }
        return QDialog::eventFilter(watched, event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            reject();
            return;
        }

        // Global Delete handler to remove existing mark across steps (unless currently editing label)
        if (m_isAlreadyMarked && event->key() == Qt::Key_Delete) {
            if (m_currentStep != 3 || !m_labelEdit || !m_labelEdit->hasFocus()) {
                m_markRemoved = true;
                accept();
                return;
            }
        }

        switch (m_currentStep) {
        case 0: { // Step 0: Letter
            if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
                goToStep(1);
                return;
            }
            if (event->key() == Qt::Key_Space || event->key() == Qt::Key_0) {
                m_useLetter = false;
                m_letter.clear();
                updateStep0Display();
                goToStep(1);
                return;
            }
            const QString txt = event->text().trimmed().toUpper();
            if (txt.length() == 1 && txt.at(0).isLetterOrNumber()) {
                m_useLetter = true;
                m_letter = txt;
                updateStep0Display();
                return;
            }
            break;
        }
        case 1: { // Step 1: Color
            // Hotkeys A, S, D, F, G, H, J, K or 1..8
            for (int i = 0; i < 8; ++i) {
                if (event->key() == s_paletteQtKeys[i] || event->key() == s_paletteNumKeys[i]) {
                    handleColorCardSelected(i);
                    return;
                }
            }
            // Reroll: R or Space
            if (event->key() == Qt::Key_R || event->key() == Qt::Key_Space) {
                rerollPalette();
                return;
            }
            // Custom RGB dialog: C
            if (event->key() == Qt::Key_C) {
                openCustomColorDialog();
                return;
            }
            // No custom color: X or 0
            if (event->key() == Qt::Key_X || event->key() == Qt::Key_0) {
                m_useColor = false;
                goToStep(2);
                return;
            }
            // Return accepts current color
            if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
                m_useColor = true;
                goToStep(2);
                return;
            }
            // Backspace goes to Step 0
            if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Left) {
                goToStep(0);
                return;
            }
            break;
        }
        case 2: { // Step 2: Scope (Keys: A, S, D or 1, 2, 3)
            if (event->key() == Qt::Key_A || event->key() == Qt::Key_1) {
                handleScopeSelected(1);
                return;
            }
            if (event->key() == Qt::Key_S || event->key() == Qt::Key_2) {
                handleScopeSelected(2);
                return;
            }
            if (event->key() == Qt::Key_D || event->key() == Qt::Key_3) {
                handleScopeSelected(3);
                return;
            }
            if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
                goToStep(3);
                return;
            }
            if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Left) {
                goToStep(1);
                return;
            }
            break;
        }
        case 3: { // Step 3: Summary / Save
            if (!m_labelEdit->hasFocus()) {
                if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
                    tryFinishMark();
                    return;
                }
                if (event->key() == Qt::Key_L) {
                    m_labelEdit->setFocus();
                    m_labelEdit->selectAll();
                    return;
                }
                if (event->key() == Qt::Key_P && m_groupByApp) {
                    togglePersistence();
                    return;
                }
                if (event->key() == Qt::Key_D && m_desktopNum > 0) {
                    toggleDesktopConstraint();
                    return;
                }
                if (event->key() == Qt::Key_Delete && m_isAlreadyMarked) {
                    m_markRemoved = true;
                    accept();
                    return;
                }
                if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Left) {
                    goToStep(2);
                    return;
                }
            }
            break;
        }
        }

        QDialog::keyPressEvent(event);
    }

private:
    void initUI()
    {
        auto *dialogLayout = new QVBoxLayout(this);
        dialogLayout->setContentsMargins(6, 6, 6, 6);

        m_mainFrame = new MainMarkerFrame(this);
        auto *mainLayout = new QVBoxLayout(m_mainFrame);
        mainLayout->setContentsMargins(20, 16, 20, 14);
        mainLayout->setSpacing(12);

        setupHeader(mainLayout);

        m_stack = new QStackedWidget(m_mainFrame);
        m_step0Page = new QWidget(m_stack);
        m_step1Page = new QWidget(m_stack);
        m_step2Page = new QWidget(m_stack);
        m_step3Page = new QWidget(m_stack);

        setupStep0(m_step0Page);
        setupStep1(m_step1Page);
        setupStep2(m_step2Page);
        setupStep3(m_step3Page);

        m_stack->addWidget(m_step0Page);
        m_stack->addWidget(m_step1Page);
        m_stack->addWidget(m_step2Page);
        m_stack->addWidget(m_step3Page);
        mainLayout->addWidget(m_stack, 1);

        setupBottomBar(mainLayout);

        dialogLayout->addWidget(m_mainFrame);
    }

    void setupHeader(QVBoxLayout *layout)
    {
        auto *headerWidget = new QWidget(m_mainFrame);
        auto *hLayout = new QVBoxLayout(headerWidget);
        hLayout->setContentsMargins(0, 0, 0, 4);
        hLayout->setSpacing(8);

        // Top context row with window info
        auto *topRow = new QHBoxLayout();
        topRow->setSpacing(10);

        auto *lblLogo = new QLabel(QStringLiteral("🎯"), headerWidget);
        lblLogo->setStyleSheet(QStringLiteral("font-size: 18px;"));
        topRow->addWidget(lblLogo);

        const QString appDisp = m_appClass.isEmpty() ? (m_isPolish ? QStringLiteral("Okno bez nazwy") : QStringLiteral("Unnamed Window")) : m_appClass;
        QString captionSnippet = m_windowCaption;
        if (captionSnippet.length() > 44) {
            captionSnippet = captionSnippet.left(41) + QStringLiteral("…");
        }
        if (captionSnippet.isEmpty()) {
            captionSnippet = QStringLiteral("—");
        }

        QString headerTitle = QStringLiteral("<span style='font-size:15px; font-weight:bold; color:#ffffff;'>Custom Marker: %1</span>").arg(appDisp.toHtmlEscaped());
        if (!m_label.isEmpty()) {
            headerTitle += QStringLiteral(" <span style='font-size:13px; color:#3498db; font-weight:bold;'>[%1]</span>").arg(m_label.toHtmlEscaped());
        }
        headerTitle += QStringLiteral(" <span style='font-size:13px; color:rgba(255,255,255,0.65);'>• %1</span>").arg(captionSnippet.toHtmlEscaped());

        auto *lblContext = new QLabel(headerTitle, headerWidget);
        lblContext->setTextFormat(Qt::RichText);
        topRow->addWidget(lblContext, 1);

        hLayout->addLayout(topRow);

        // HUD Breadcrumbs tracker
        auto *breadRow = new QHBoxLayout();
        breadRow->setSpacing(6);

        const QStringList stepNames = {
            m_isPolish ? QStringLiteral("1. Litera") : QStringLiteral("1. Letter"),
            m_isPolish ? QStringLiteral("2. Kolor") : QStringLiteral("2. Color"),
            m_isPolish ? QStringLiteral("3. Zakres") : QStringLiteral("3. Scope"),
            m_isPolish ? QStringLiteral("4. Zapis") : QStringLiteral("4. Save")
        };

        for (int i = 0; i < 4; ++i) {
            m_stepPills[i] = new QPushButton(stepNames.at(i), headerWidget);
            m_stepPills[i]->setFlat(true);
            m_stepPills[i]->setFocusPolicy(Qt::NoFocus);
            m_stepPills[i]->setCursor(Qt::PointingHandCursor);
            connect(m_stepPills[i], &QPushButton::clicked, this, [this, i]() {
                goToStep(i);
            });
            breadRow->addWidget(m_stepPills[i]);

            if (i < 3) {
                auto *sep = new QLabel(QStringLiteral("➔"), headerWidget);
                sep->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.35); font-size: 11px;"));
                breadRow->addWidget(sep);
            }
        }
        hLayout->addLayout(breadRow);

        layout->addWidget(headerWidget);
    }

    void setupStep0(QWidget *page)
    {
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(12);

        auto *lblTitle = new QLabel(m_isPolish ? QStringLiteral("Krok 1: Litera skrótu HUD (Jump Key)") : QStringLiteral("Step 1: HUD Jump Letter"), page);
        lblTitle->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: bold; color: #ffffff;"));
        layout->addWidget(lblTitle);

        auto *lblSub = new QLabel(m_isPolish
            ? (m_isAlreadyMarked
                ? QStringLiteral("Wciśnij dowolną literę [A-Z] lub cyfrę, albo [Del] aby usunąć obecne oznaczenie.")
                : QStringLiteral("Wciśnij dowolną literę [A-Z] lub cyfrę. Wybór następuje bezpośrednio z klawiatury."))
            : (m_isAlreadyMarked
                ? QStringLiteral("Press any letter [A-Z] or digit on your keyboard, or [Del] to remove current mark.")
                : QStringLiteral("Press any letter [A-Z] or digit on your keyboard.")), page);
        lblSub->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.75); font-size: 13px;"));
        layout->addWidget(lblSub);

        // Center HUD letter badge
        auto *centerBox = new QHBoxLayout();
        centerBox->setAlignment(Qt::AlignCenter);
        centerBox->setContentsMargins(0, 10, 0, 10);

        m_step0BadgeBox = new QLabel(m_letter, page);
        m_step0BadgeBox->setFixedSize(84, 84);
        m_step0BadgeBox->setAlignment(Qt::AlignCenter);
        QFont bf = m_step0BadgeBox->font();
        bf.setPointSize(34);
        bf.setBold(true);
        m_step0BadgeBox->setFont(bf);
        centerBox->addWidget(m_step0BadgeBox);

        layout->addLayout(centerBox);

        // Conflict warning banner
        m_lblStep0Conflict = new QLabel(page);
        m_lblStep0Conflict->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background-color: rgba(230, 126, 34, 0.25);"
            "  border: 1.5px solid #e67e22;"
            "  border-radius: 8px;"
            "  padding: 8px 12px;"
            "  color: #f39c12;"
            "  font-weight: bold;"
            "  font-size: 12px;"
            "}"
        ));
        m_lblStep0Conflict->setWordWrap(true);
        m_lblStep0Conflict->hide();
        layout->addWidget(m_lblStep0Conflict);

        // Quick action buttons
        auto *btnRow = new QHBoxLayout();
        btnRow->setSpacing(10);

        m_btnStep0Keep = new QPushButton(page);
        m_btnStep0Keep->setFocusPolicy(Qt::NoFocus);
        m_btnStep0Keep->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(52, 152, 219, 0.28);"
            "  border: 1.5px solid #3498db;"
            "  border-radius: 10px;"
            "  color: #ffffff;"
            "  font-size: 14px;"
            "  font-weight: bold;"
            "  padding: 10px 18px;"
            "}"
            "QPushButton:hover { background-color: rgba(52, 152, 219, 0.45); }"
        ));
        connect(m_btnStep0Keep, &QPushButton::clicked, this, [this]() {
            goToStep(1);
        });
        btnRow->addWidget(m_btnStep0Keep, 1);

        m_btnStep0NoLetter = new QPushButton(m_isPolish ? QStringLiteral("Bez stałej litery (Spacja / 0)") : QStringLiteral("No fixed letter (Space / 0)"), page);
        m_btnStep0NoLetter->setFocusPolicy(Qt::NoFocus);
        m_btnStep0NoLetter->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.08);"
            "  border: 1px solid rgba(255, 255, 255, 0.2);"
            "  border-radius: 10px;"
            "  color: rgba(255, 255, 255, 0.9);"
            "  font-size: 13px;"
            "  padding: 10px 18px;"
            "}"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.15); }"
        ));
        connect(m_btnStep0NoLetter, &QPushButton::clicked, this, [this]() {
            m_useLetter = false;
            m_letter.clear();
            updateStep0Display();
            goToStep(1);
        });
        btnRow->addWidget(m_btnStep0NoLetter, 1);

        layout->addLayout(btnRow);
        layout->addStretch(1);

        updateStep0Display();
    }

    void updateStep0Display()
    {
        if (m_useLetter && !m_letter.isEmpty()) {
            m_step0BadgeBox->setText(m_letter);
            const QColor textCol = OutlineBadge::contrastingTextColor(m_color);
            m_step0BadgeBox->setStyleSheet(QStringLiteral(
                "QLabel {"
                "  background-color: %1;"
                "  color: %2;"
                "  border-radius: 16px;"
                "  border: 2px solid rgba(255,255,255,0.45);"
                "  font-weight: bold;"
                "}"
            ).arg(m_color.name(), textCol == Qt::black ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
            m_btnStep0Keep->setText(m_isPolish ? QStringLiteral("Użyj litery [%1] (Enter)").arg(m_letter) : QStringLiteral("Use letter [%1] (Enter)").arg(m_letter));
        } else {
            m_step0BadgeBox->setText(QStringLiteral("—"));
            m_step0BadgeBox->setStyleSheet(QStringLiteral(
                "QLabel {"
                "  background-color: rgba(255, 255, 255, 0.09);"
                "  color: rgba(255, 255, 255, 0.45);"
                "  border-radius: 16px;"
                "  border: 2px dashed rgba(255, 255, 255, 0.3);"
                "  font-weight: bold;"
                "}"
            ));
            m_btnStep0Keep->setText(m_isPolish ? QStringLiteral("Bez litery (Dalej)") : QStringLiteral("No letter (Next)"));
        }

        // Shared letter / Smart Workspace info
        if (m_useLetter && !m_letter.isEmpty() && m_usedLettersMap.contains(m_letter)) {
            const QString existingApp = m_usedLettersMap.value(m_letter).toString();
            if (!existingApp.isEmpty() && existingApp.compare(m_appClass, Qt::CaseInsensitive) != 0) {
                m_lblStep0Conflict->setText(m_isPolish
                    ? QStringLiteral("🏷️ Litera '%1' jest już przypisana do: %2.\nMożesz połączyć te okna we wspólną grupę roboczą (Smart Workspace).")
                        .arg(m_letter, existingApp)
                    : QStringLiteral("🏷️ Letter '%1' is already assigned to: %2.\nYou can join these windows into a shared Smart Workspace.")
                        .arg(m_letter, existingApp));
                m_lblStep0Conflict->setStyleSheet(QStringLiteral("color: #3498db; font-size: 13px; font-weight: bold; background: rgba(52, 152, 219, 0.15); border: 1px solid rgba(52, 152, 219, 0.4); border-radius: 8px; padding: 6px 12px;"));
                m_lblStep0Conflict->show();
                return;
            }
        }
        m_lblStep0Conflict->hide();
    }

    void setupStep1(QWidget *page)
    {
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(10);

        auto *lblTitle = new QLabel(m_isPolish ? QStringLiteral("Krok 2: Wybierz kolor oznaczenia") : QStringLiteral("Step 2: Choose Mark Color"), page);
        lblTitle->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: bold; color: #ffffff;"));
        layout->addWidget(lblTitle);

        auto *lblSub = new QLabel(m_isPolish
            ? QStringLiteral("Wciśnij literę przypisaną do barwy, aby natychmiast przejść do zakresu:")
            : QStringLiteral("Press any letter key to pick a color and advance instantly:"), page);
        lblSub->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.75); font-size: 13px;"));
        layout->addWidget(lblSub);

        // 8 Palette Tiles Grid
        auto *gridContainer = new QWidget(page);
        m_paletteGrid = new QGridLayout(gridContainer);
        m_paletteGrid->setSpacing(10);
        m_paletteGrid->setContentsMargins(0, 4, 0, 4);

        for (int i = 0; i < 8; ++i) {
            auto *btn = new QPushButton(gridContainer);
            btn->setFixedHeight(58);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFocusPolicy(Qt::NoFocus);
            connect(btn, &QPushButton::clicked, this, [this, i]() {
                handleColorCardSelected(i);
            });
            m_paletteButtons.append(btn);
            m_paletteGrid->addWidget(btn, i / 4, i % 4);
        }
        layout->addWidget(gridContainer);

        // Secondary Palette Controls: Reroll, Custom Color, No Color
        auto *ctrlLayout = new QHBoxLayout();
        ctrlLayout->setSpacing(10);

        m_btnReroll = new QPushButton(m_isPolish ? QStringLiteral("🎲 Nowe barwy (R / Spacja)") : QStringLiteral("🎲 New Colors (R / Space)"), page);
        m_btnReroll->setFocusPolicy(Qt::NoFocus);
        m_btnReroll->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.1);"
            "  border: 1px solid rgba(255, 255, 255, 0.25);"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  font-size: 13px;"
            "  font-weight: bold;"
            "  padding: 8px 14px;"
            "}"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.18); }"
        ));
        connect(m_btnReroll, &QPushButton::clicked, this, [this]() {
            rerollPalette();
        });
        ctrlLayout->addWidget(m_btnReroll, 1);

        m_btnCustomColor = new QPushButton(m_isPolish ? QStringLiteral("🎨 Własny kolor (C)") : QStringLiteral("🎨 Custom Color (C)"), page);
        m_btnCustomColor->setFocusPolicy(Qt::NoFocus);
        m_btnCustomColor->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.08);"
            "  border: 1px solid rgba(255, 255, 255, 0.18);"
            "  border-radius: 8px;"
            "  color: rgba(255, 255, 255, 0.9);"
            "  font-size: 13px;"
            "  padding: 8px 14px;"
            "}"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.14); }"
        ));
        connect(m_btnCustomColor, &QPushButton::clicked, this, [this]() {
            openCustomColorDialog();
        });
        ctrlLayout->addWidget(m_btnCustomColor, 1);

        m_btnNoColor = new QPushButton(m_isPolish ? QStringLiteral("🚫 Bez koloru (X)") : QStringLiteral("🚫 No Color (X)"), page);
        m_btnNoColor->setFocusPolicy(Qt::NoFocus);
        m_btnNoColor->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.08);"
            "  border: 1px solid rgba(255, 255, 255, 0.18);"
            "  border-radius: 8px;"
            "  color: rgba(255, 255, 255, 0.9);"
            "  font-size: 13px;"
            "  padding: 8px 14px;"
            "}"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.14); }"
        ));
        connect(m_btnNoColor, &QPushButton::clicked, this, [this]() {
            m_useColor = false;
            goToStep(2);
        });
        ctrlLayout->addWidget(m_btnNoColor, 1);

        layout->addLayout(ctrlLayout);
        layout->addStretch(1);

        updatePaletteUI();
    }

    void regeneratePalette()
    {
        m_paletteColors.clear();

        int baseHue = (m_rerollCount == 0 && m_color.isValid())
            ? m_color.hue()
            : (m_seedOffset % 360);
        if (baseHue < 0) baseHue = 0;

        const int count = 8;
        for (int i = 0; i < count; ++i) {
            int hue = (baseHue + i * (360 / count) + (i % 2 == 1 ? 14 : -8) + 360) % 360;
            int sat = 215 + (i % 3) * 18;
            int val = 225 + ((i + 1) % 2) * 25;
            m_paletteColors.append(QColor::fromHsv(hue, sat, val));
        }

        if (m_rerollCount == 0 && m_color.isValid()) {
            m_paletteColors[0] = m_color;
        }
    }

    void updatePaletteUI()
    {
        for (int i = 0; i < 8 && i < m_paletteButtons.size(); ++i) {
            const QColor c = m_paletteColors.value(i);
            const QColor textCol = OutlineBadge::contrastingTextColor(c);
            const QString textColStr = (textCol == Qt::black) ? QStringLiteral("#000000") : QStringLiteral("#ffffff");
            const bool isSelected = m_useColor && (m_color == c);

            auto *btn = m_paletteButtons.at(i);
            btn->setText(QStringLiteral("[%1]").arg(QLatin1String(s_paletteKeys[i])));
            btn->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background-color: %1;"
                "  color: %2;"
                "  font-size: 16px;"
                "  font-weight: bold;"
                "  border-radius: 12px;"
                "  border: %3;"
                "}"
                "QPushButton:hover {"
                "  border: 3.5px solid rgba(255,255,255,0.95);"
                "}"
            ).arg(c.name(), textColStr, isSelected ? QStringLiteral("3.5px solid #ffffff") : QStringLiteral("1.5px solid rgba(0,0,0,0.35)")));
        }
    }

    void rerollPalette()
    {
        m_seedOffset = (m_seedOffset + 73 + QRandomGenerator::global()->bounded(53)) % 360;
        m_rerollCount++;
        regeneratePalette();
        updatePaletteUI();
    }

    void handleColorCardSelected(int index)
    {
        if (index >= 0 && index < m_paletteColors.size()) {
            m_color = m_paletteColors.at(index);
            m_useColor = true;
            updatePaletteUI();
            updateStep0Display();
            goToStep(2);
        }
    }

    void openCustomColorDialog()
    {
        QColorDialog colDlg(m_color, this);
        colDlg.setOption(QColorDialog::ShowAlphaChannel, true);
        colDlg.setWindowTitle(m_isPolish ? QStringLiteral("Wybierz własny kolor") : tr("Select Custom Color"));
        if (colDlg.exec() == QDialog::Accepted) {
            m_color = colDlg.selectedColor();
            m_useColor = true;
            updateStep0Display();
            goToStep(2);
        }
    }

    void setupStep2(QWidget *page)
    {
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(12);

        auto *lblTitle = new QLabel(m_isPolish ? QStringLiteral("Krok 3: Wybierz zakres reguły") : QStringLiteral("Step 3: Choose Rule Scope"), page);
        lblTitle->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: bold; color: #ffffff;"));
        layout->addWidget(lblTitle);

        auto *lblSub = new QLabel(m_isPolish
            ? QStringLiteral("Wciśnij literę [A], [S] lub [D] na klawiaturze:")
            : QStringLiteral("Press letter [A], [S], or [D] on your keyboard:"), page);
        lblSub->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.75); font-size: 13px;"));
        layout->addWidget(lblSub);

        const QString appDisp = m_appClass.isEmpty() ? (m_isPolish ? QStringLiteral("tej aplikacji") : QStringLiteral("this app")) : m_appClass;

        // Card A: All App (Key: A or 1)
        m_cardAllApp = new MarkerOptionCard(page);
        m_cardAllApp->onClicked = [this]() { handleScopeSelected(1); };

        auto *c1Layout = new QHBoxLayout(m_cardAllApp);
        c1Layout->setContentsMargins(16, 12, 16, 12);
        c1Layout->setSpacing(14);

        auto *c1Badge = new QLabel(QStringLiteral("[ A ]"), m_cardAllApp);
        c1Badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        c1Badge->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 17px; color: #3498db;"));
        c1Layout->addWidget(c1Badge);

        auto *c1TextLayout = new QVBoxLayout();
        c1TextLayout->setSpacing(4);
        auto *c1Title = new QLabel(m_isPolish ? QStringLiteral("<b style='font-size:15px; color:#ffffff;'>Cała aplikacja (%1)</b>").arg(appDisp) : QStringLiteral("<b style='font-size:15px; color:#ffffff;'>All windows of this app (%1)</b>").arg(appDisp), m_cardAllApp);
        c1Title->setAttribute(Qt::WA_TransparentForMouseEvents);
        c1Title->setTextFormat(Qt::RichText);
        auto *c1Desc = new QLabel(m_isPolish ? QStringLiteral("Wszystkie obecne i przyszłe okna tego programu będą miały ten mark.") : QStringLiteral("All current and future windows of this app will share this mark."), m_cardAllApp);
        c1Desc->setAttribute(Qt::WA_TransparentForMouseEvents);
        c1Desc->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.75); font-size: 13px;"));
        c1TextLayout->addWidget(c1Title);
        c1TextLayout->addWidget(c1Desc);
        c1Layout->addLayout(c1TextLayout, 1);
        layout->addWidget(m_cardAllApp);

        // Card S: Process / Caption with clean popup prompt (Key: S or 2)
        m_cardCaption = new MarkerOptionCard(page);
        m_cardCaption->onClicked = [this]() { handleScopeSelected(2); };

        auto *c2Layout = new QHBoxLayout(m_cardCaption);
        c2Layout->setContentsMargins(16, 12, 16, 12);
        c2Layout->setSpacing(14);

        auto *c2Badge = new QLabel(QStringLiteral("[ S ]"), m_cardCaption);
        c2Badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        c2Badge->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 17px; color: #e67e22;"));
        c2Layout->addWidget(c2Badge);

        auto *c2TextLayout = new QVBoxLayout();
        c2TextLayout->setSpacing(4);
        auto *c2Title = new QLabel(m_isPolish ? QStringLiteral("<b style='font-size:15px; color:#ffffff;'>Zawęź do procesu potomnego lub tytułu</b>") : QStringLiteral("<b style='font-size:15px; color:#ffffff;'>Narrow to child process or title</b>"), m_cardCaption);
        c2Title->setAttribute(Qt::WA_TransparentForMouseEvents);
        c2Title->setTextFormat(Qt::RichText);

        m_lblCardCaptionDesc = new QLabel(m_cardCaption);
        m_lblCardCaptionDesc->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_lblCardCaptionDesc->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.75); font-size: 13px;"));
        m_lblCardCaptionDesc->setTextFormat(Qt::RichText);

        c2TextLayout->addWidget(c2Title);
        c2TextLayout->addWidget(m_lblCardCaptionDesc);
        c2Layout->addLayout(c2TextLayout, 1);
        layout->addWidget(m_cardCaption);

        // Card D: Single Window (Key: D or 3)
        m_cardSingleWindow = new MarkerOptionCard(page);
        m_cardSingleWindow->onClicked = [this]() { handleScopeSelected(3); };

        auto *c3Layout = new QHBoxLayout(m_cardSingleWindow);
        c3Layout->setContentsMargins(16, 12, 16, 12);
        c3Layout->setSpacing(14);

        auto *c3Badge = new QLabel(QStringLiteral("[ D ]"), m_cardSingleWindow);
        c3Badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        c3Badge->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 17px; color: #2ecc71;"));
        c3Layout->addWidget(c3Badge);

        auto *c3TextLayout = new QVBoxLayout();
        c3TextLayout->setSpacing(4);
        auto *c3Title = new QLabel(m_isPolish ? QStringLiteral("<b style='font-size:15px; color:#ffffff;'>Tylko to jedno okno</b>") : QStringLiteral("<b style='font-size:15px; color:#ffffff;'>This window only</b>"), m_cardSingleWindow);
        c3Title->setAttribute(Qt::WA_TransparentForMouseEvents);
        c3Title->setTextFormat(Qt::RichText);
        auto *c3Desc = new QLabel(m_isPolish ? QStringLiteral("Tymczasowe oznaczenie wyłącznie dla tej jednej instancji.") : QStringLiteral("Temporary mark applied only to this specific window instance."), m_cardSingleWindow);
        c3Desc->setAttribute(Qt::WA_TransparentForMouseEvents);
        c3Desc->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.75); font-size: 13px;"));
        c3TextLayout->addWidget(c3Title);
        c3TextLayout->addWidget(c3Desc);
        c3Layout->addLayout(c3TextLayout, 1);
        layout->addWidget(m_cardSingleWindow);

        layout->addStretch(1);

        updateScopeCardsUI();
    }

    void updateScopeCardsUI()
    {
        const bool isAllApp = m_groupByApp && m_matchCaption.isEmpty();
        const bool isCaption = m_groupByApp && !m_matchCaption.isEmpty();
        const bool isSingle = !m_groupByApp;

        m_cardAllApp->setSelected(isAllApp);
        m_cardCaption->setSelected(isCaption);
        m_cardSingleWindow->setSelected(isSingle);

        if (m_lblCardCaptionDesc) {
            if (m_matchCaption.isEmpty()) {
                m_lblCardCaptionDesc->setText(m_isPolish
                    ? QStringLiteral("Dopasuj do programu (np. nvim, ssh). <span style='color:#e67e22;'>[Wciśnij S, aby ustawić]</span>")
                    : QStringLiteral("Match process or title (e.g. nvim, ssh). <span style='color:#e67e22;'>[Press S to set]</span>"));
            } else {
                m_lblCardCaptionDesc->setText(m_isPolish
                    ? QStringLiteral("Aktywny filtr: <b style='color:#e67e22;'>\"%1\"</b> &bull; <span style='color:rgba(255,255,255,0.6);'>Wciśnij Enter, aby przejść dalej (lub S aby zmienić)</span>").arg(m_matchCaption.toHtmlEscaped())
                    : QStringLiteral("Active filter: <b style='color:#e67e22;'>\"%1\"</b> &bull; <span style='color:rgba(255,255,255,0.6);'>Press Enter to proceed (or S to edit)</span>").arg(m_matchCaption.toHtmlEscaped()));
            }
        }
    }

    void handleScopeSelected(int scopeType)
    {
        if (scopeType == 1) { // All app
            m_groupByApp = true;
            m_matchCaption.clear();
            updateScopeCardsUI();
            goToStep(3);
        } else if (scopeType == 2) { // Process / caption
            // Open clean HUD popup without squeezing cards or trapping focus!
            PatternFilterDialog dlg(m_matchCaption.isEmpty() ? m_detectedCaption : m_matchCaption, m_isPolish, this);
            if (dlg.exec() == QDialog::Accepted) {
                m_matchCaption = dlg.pattern();
                m_groupByApp = true;
                updateScopeCardsUI();
            }
            setFocus();
        } else if (scopeType == 3) { // Single window
            m_groupByApp = false;
            m_matchCaption.clear();
            m_persistent = false;
            updateScopeCardsUI();
            goToStep(3);
        }
    }

    void setupStep3(QWidget *page)
    {
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(12);

        auto *lblTitle = new QLabel(m_isPolish ? QStringLiteral("Krok 4: Podsumowanie i zapis reguły") : QStringLiteral("Step 4: Summary & Save"), page);
        lblTitle->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: bold; color: #ffffff;"));
        layout->addWidget(lblTitle);

        // Live Preview Mockup Card
        m_previewCard = new QFrame(page);
        m_previewCard->setStyleSheet(QStringLiteral(
            "QFrame {"
            "  background-color: rgba(255, 255, 255, 0.08);"
            "  border: 1.5px solid rgba(255, 255, 255, 0.2);"
            "  border-radius: 14px;"
            "}"
        ));
        auto *prevLayout = new QHBoxLayout(m_previewCard);
        prevLayout->setSpacing(16);
        prevLayout->setContentsMargins(14, 12, 14, 12);

        m_previewBadge = new QLabel(m_previewCard);
        m_previewBadge->setFixedSize(62, 62);
        m_previewBadge->setAlignment(Qt::AlignCenter);
        QFont pf = m_previewBadge->font();
        pf.setPointSize(24);
        pf.setBold(true);
        m_previewBadge->setFont(pf);
        prevLayout->addWidget(m_previewBadge);

        auto *prevDetailsLayout = new QVBoxLayout();
        prevDetailsLayout->setSpacing(4);

        m_previewTitle = new QLabel(m_previewCard);
        m_previewTitle->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold; color: #ffffff;"));
        m_previewTitle->setTextFormat(Qt::RichText);
        prevDetailsLayout->addWidget(m_previewTitle);

        m_previewScope = new QLabel(m_previewCard);
        m_previewScope->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 0.85); font-size: 13px;"));
        m_previewScope->setTextFormat(Qt::RichText);
        prevDetailsLayout->addWidget(m_previewScope);

        m_previewConflictWarn = new QLabel(m_previewCard);
        m_previewConflictWarn->setStyleSheet(QStringLiteral("color: #e67e22; font-size: 12px; font-weight: bold;"));
        m_previewConflictWarn->hide();
        prevDetailsLayout->addWidget(m_previewConflictWarn);

        prevLayout->addLayout(prevDetailsLayout, 1);
        layout->addWidget(m_previewCard);

        // Label input field
        m_labelEdit = new QLineEdit(m_label, page);
        m_labelEdit->setPlaceholderText(m_isPolish ? QStringLiteral("🏷 [L] Własna etykieta (np. Backend, Dev, Terminal)...") : QStringLiteral("🏷 [L] Custom label (e.g. Backend, Dev, Terminal)..."));
        m_labelEdit->setClearButtonEnabled(true);
        m_labelEdit->setMaxLength(50);
        m_labelEdit->installEventFilter(this);
        m_labelEdit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background-color: rgba(0, 0, 0, 0.45);"
            "  border: 1.5px solid rgba(255, 255, 255, 0.25);"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  padding: 8px 12px;"
            "  font-size: 13px;"
            "}"
            "QLineEdit:focus {"
            "  border: 1.5px solid #3498db;"
            "}"
        ));
        connect(m_labelEdit, &QLineEdit::textChanged, this, [this](const QString &txt) {
            m_label = txt.trimmed();
            updateStep3Preview();
        });
        layout->addWidget(m_labelEdit);

        // High-Contrast HUD Toggle Buttons
        auto *togglesLayout = new QVBoxLayout();
        togglesLayout->setSpacing(8);

        // 1. Persistent Toggle Button
        m_btnTogglePersistent = new QPushButton(page);
        m_btnTogglePersistent->setFocusPolicy(Qt::NoFocus);
        m_btnTogglePersistent->setCursor(Qt::PointingHandCursor);
        connect(m_btnTogglePersistent, &QPushButton::clicked, this, [this]() {
            togglePersistence();
        });
        togglesLayout->addWidget(m_btnTogglePersistent);

        // 2. Desktop Toggle Button (if desktopNum > 0)
        if (m_desktopNum > 0) {
            m_btnToggleDesktop = new QPushButton(page);
            m_btnToggleDesktop->setFocusPolicy(Qt::NoFocus);
            m_btnToggleDesktop->setCursor(Qt::PointingHandCursor);
            connect(m_btnToggleDesktop, &QPushButton::clicked, this, [this]() {
                toggleDesktopConstraint();
            });
            togglesLayout->addWidget(m_btnToggleDesktop);
        }

        layout->addLayout(togglesLayout);
        layout->addStretch(1);
    }

    void togglePersistence()
    {
        if (!m_groupByApp) return;
        m_persistent = !m_persistent;
        updateStep3Preview();
    }

    void toggleDesktopConstraint()
    {
        if (m_desktopNum <= 0) return;
        m_matchDesktop = !m_matchDesktop;
        updateStep3Preview();
    }

    void updateStep3Preview()
    {
        const QString letDisp = (m_useLetter && !m_letter.isEmpty()) ? m_letter : QStringLiteral("—");
        m_previewBadge->setText(letDisp);

        const QColor effectiveBg = m_useColor ? m_color : QColor(QStringLiteral("#7f8c8d"));
        const QColor textCol = OutlineBadge::contrastingTextColor(effectiveBg);
        m_previewBadge->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background-color: %1;"
            "  color: %2;"
            "  border-radius: 14px;"
            "  border: 2px solid rgba(255,255,255,0.45);"
            "  font-weight: bold;"
            "}"
        ).arg(effectiveBg.name(), textCol == Qt::black ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));

        const QString appDisp = m_appClass.isEmpty() ? (m_isPolish ? QStringLiteral("Bieżące okno") : QStringLiteral("Current Window")) : m_appClass;
        QString titleStr = QStringLiteral("<b style='color:#ffffff; font-size:16px;'>%1</b>").arg(appDisp.toHtmlEscaped());
        if (!m_label.isEmpty()) {
            titleStr += QStringLiteral(" <span style='color: #3498db; font-size:14px; font-weight:bold;'>[%1]</span>").arg(m_label.toHtmlEscaped());
        }
        m_previewTitle->setText(titleStr);

        QString scopeDesc;
        if (!m_groupByApp) {
            scopeDesc = m_isPolish ? QStringLiteral("Zakres: <b style='color:#2ecc71;'>Tylko to jedno okno</b>") : QStringLiteral("Scope: <b style='color:#2ecc71;'>This window only</b>");
        } else if (!m_matchCaption.isEmpty()) {
            scopeDesc = m_isPolish
                ? QStringLiteral("Zakres: <b style='color:#e67e22;'>Aplikacja %1</b> (filtr: '%2')").arg(appDisp.toHtmlEscaped(), m_matchCaption.toHtmlEscaped())
                : QStringLiteral("Scope: <b style='color:#e67e22;'>App %1</b> (filter: '%2')").arg(appDisp.toHtmlEscaped(), m_matchCaption.toHtmlEscaped());
        } else {
            scopeDesc = m_isPolish
                ? QStringLiteral("Zakres: <b style='color:#3498db;'>Wszystkie okna aplikacji %1</b>").arg(appDisp.toHtmlEscaped())
                : QStringLiteral("Scope: <b style='color:#3498db;'>All windows of app %1</b>").arg(appDisp.toHtmlEscaped());
        }
        m_previewScope->setText(scopeDesc);

        if (m_useLetter && !m_letter.isEmpty() && m_usedLettersMap.contains(m_letter)) {
            const QString existingApp = m_usedLettersMap.value(m_letter).toString();
            if (!existingApp.isEmpty() && existingApp.compare(m_appClass, Qt::CaseInsensitive) != 0) {
                m_previewConflictWarn->setText(m_isPolish
                    ? QStringLiteral("🏷️ Dołączy do grupy (Smart Workspace) z: %1 (grupa '%2')").arg(existingApp, m_letter)
                    : QStringLiteral("🏷️ Will join Smart Workspace with: %1 (group '%2')").arg(existingApp, m_letter));
                m_previewConflictWarn->setStyleSheet(QStringLiteral("color: #3498db; font-size: 12px; font-weight: bold;"));
                m_previewConflictWarn->show();
            } else {
                m_previewConflictWarn->hide();
            }
        } else {
            m_previewConflictWarn->hide();
        }

        // Update High-Contrast Persistence Toggle Pill
        if (m_btnTogglePersistent) {
            if (!m_groupByApp) {
                m_persistent = false;
                m_btnTogglePersistent->setEnabled(false);
                m_btnTogglePersistent->setText(m_isPolish
                    ? QStringLiteral("💾 [P] Zapis na stałe (marks.json):  NIE DOTYCZY (Jedno okno)")
                    : QStringLiteral("💾 [P] Save permanently (marks.json):  N/A (Single Window)"));
                m_btnTogglePersistent->setStyleSheet(QStringLiteral(
                    "QPushButton {"
                    "  background-color: rgba(255, 255, 255, 0.04);"
                    "  border: 1px dashed rgba(255, 255, 255, 0.15);"
                    "  border-radius: 8px;"
                    "  color: rgba(255, 255, 255, 0.35);"
                    "  text-align: left;"
                    "  padding: 8px 14px;"
                    "  font-size: 13px;"
                    "}"
                ));
            } else {
                m_btnTogglePersistent->setEnabled(true);
                const QString statusText = m_persistent
                    ? (m_isPolish ? QStringLiteral("WŁĄCZONY (pamiętaj po restarcie)") : QStringLiteral("ENABLED (persist across reboots)"))
                    : (m_isPolish ? QStringLiteral("WYŁĄCZONY (tylko bieżąca sesja)") : QStringLiteral("DISABLED (current session only)"));
                const QString tagColor = m_persistent ? QStringLiteral("#2ecc71") : QStringLiteral("#e74c3c");
                m_btnTogglePersistent->setText(QStringLiteral("💾 [P] Zapis na stałe (marks.json):  %1").arg(statusText));
                m_btnTogglePersistent->setStyleSheet(QStringLiteral(
                    "QPushButton {"
                    "  background-color: rgba(255, 255, 255, 0.07);"
                    "  border: 1.5px solid %1;"
                    "  border-radius: 8px;"
                    "  color: #ffffff;"
                    "  text-align: left;"
                    "  padding: 8px 14px;"
                    "  font-size: 13px;"
                    "  font-weight: bold;"
                    "}"
                    "QPushButton:hover { background-color: rgba(255, 255, 255, 0.12); }"
                ).arg(tagColor));
            }
        }

        // Update High-Contrast Virtual Desktop Toggle Pill
        if (m_btnToggleDesktop && m_desktopNum > 0) {
            const QString deskLabel = m_desktopName.isEmpty() ? QStringLiteral("Pulpit %1").arg(m_desktopNum) : m_desktopName;
            const QString statusText = m_matchDesktop
                ? (m_isPolish ? QStringLiteral("TYLKO TEN PULPIT (%1)").arg(deskLabel) : QStringLiteral("ONLY THIS DESKTOP (%1)").arg(deskLabel))
                : (m_isPolish ? QStringLiteral("WSZYSTKIE PULPITY") : QStringLiteral("ALL VIRTUAL DESKTOPS"));
            const QString tagColor = m_matchDesktop ? QStringLiteral("#3498db") : QStringLiteral("rgba(255,255,255,0.25)");
            m_btnToggleDesktop->setText(QStringLiteral("🖥 [D] Przypisanie pulpitu:  %1").arg(statusText));
            m_btnToggleDesktop->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background-color: rgba(255, 255, 255, 0.07);"
                "  border: 1.5px solid %1;"
                "  border-radius: 8px;"
                "  color: #ffffff;"
                "  text-align: left;"
                "  padding: 8px 14px;"
                "  font-size: 13px;"
                "  font-weight: bold;"
                "}"
                "QPushButton:hover { background-color: rgba(255, 255, 255, 0.12); }"
            ).arg(tagColor));
        }
    }

    void setupBottomBar(QVBoxLayout *layout)
    {
        auto *bottomWidget = new QWidget(m_mainFrame);
        auto *bLayout = new QVBoxLayout(bottomWidget);
        bLayout->setContentsMargins(0, 4, 0, 0);
        bLayout->setSpacing(6);

        auto *btnRow = new QHBoxLayout();
        btnRow->setSpacing(10);

        m_btnBack = new QPushButton(m_isPolish ? QStringLiteral("❮ Wstecz [Backspace]") : QStringLiteral("❮ Back [Backspace]"), bottomWidget);
        m_btnBack->setFocusPolicy(Qt::NoFocus);
        m_btnBack->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.08);"
            "  border: 1px solid rgba(255, 255, 255, 0.18);"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  font-size: 13px;"
            "  padding: 8px 16px;"
            "}"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.16); }"
            "QPushButton:disabled { color: rgba(255,255,255,0.25); border-color: rgba(255,255,255,0.06); }"
        ));
        connect(m_btnBack, &QPushButton::clicked, this, [this]() {
            if (m_currentStep > 0) {
                goToStep(m_currentStep - 1);
            }
        });
        btnRow->addWidget(m_btnBack);

        if (m_isAlreadyMarked) {
            m_btnRemove = new QPushButton(m_isPolish ? QStringLiteral("🗑 Wyczyść [Del]") : QStringLiteral("🗑 Remove [Del]"), bottomWidget);
            m_btnRemove->setFocusPolicy(Qt::NoFocus);
            m_btnRemove->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background-color: rgba(231, 76, 60, 0.22);"
                "  border: 1.5px solid #e74c3c;"
                "  border-radius: 8px;"
                "  color: #e74c3c;"
                "  font-weight: bold;"
                "  font-size: 13px;"
                "  padding: 8px 14px;"
                "}"
                "QPushButton:hover { background-color: rgba(231, 76, 60, 0.38); }"
            ));
            connect(m_btnRemove, &QPushButton::clicked, this, [this]() {
                m_markRemoved = true;
                accept();
            });
            btnRow->addWidget(m_btnRemove);
        }

        auto *btnMgr = new QPushButton(m_isPolish ? QStringLiteral("🏷️ Menedżer markerów…") : QStringLiteral("🏷️ Marker Manager…"), bottomWidget);
        btnMgr->setFocusPolicy(Qt::NoFocus);
        btnMgr->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(155, 89, 182, 0.15);"
            "  border: 1px solid rgba(155, 89, 182, 0.4);"
            "  border-radius: 8px;"
            "  color: #d1b2ea;"
            "  font-size: 12px;"
            "  padding: 8px 12px;"
            "}"
            "QPushButton:hover { background-color: rgba(155, 89, 182, 0.3); color: #ffffff; }"
        ));
        connect(btnMgr, &QPushButton::clicked, this, [this]() {
            QProcess::startDetached(QStringLiteral("leapk-marker-manager"), { m_appClass, m_windowCaption, QString::number(m_desktopNum), m_desktopName });
        });
        btnRow->addWidget(btnMgr);

        btnRow->addStretch(1);

        m_btnCancel = new QPushButton(m_isPolish ? QStringLiteral("Anuluj (Esc)") : QStringLiteral("Cancel (Esc)"), bottomWidget);
        m_btnCancel->setFocusPolicy(Qt::NoFocus);
        m_btnCancel->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.06);"
            "  border: 1px solid rgba(255, 255, 255, 0.18);"
            "  border-radius: 8px;"
            "  color: rgba(255, 255, 255, 0.9);"
            "  font-size: 13px;"
            "  padding: 8px 16px;"
            "}"
            "QPushButton:hover { background-color: rgba(255, 255, 255, 0.12); }"
        ));
        connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
        btnRow->addWidget(m_btnCancel);

        m_btnNext = new QPushButton(m_isPolish ? QStringLiteral("Dalej ❯") : QStringLiteral("Next ❯"), bottomWidget);
        m_btnNext->setFocusPolicy(Qt::NoFocus);
        m_btnNext->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: #3498db;"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  font-weight: bold;"
            "  font-size: 14px;"
            "  padding: 8px 20px;"
            "}"
            "QPushButton:hover { background-color: #2980b9; }"
        ));
        connect(m_btnNext, &QPushButton::clicked, this, [this]() {
            if (m_currentStep < 3) {
                goToStep(m_currentStep + 1);
            } else {
                tryFinishMark();
            }
        });
        btnRow->addWidget(m_btnNext);

        bLayout->addLayout(btnRow);

        // Keyboard hint line explicitly indicating shortcuts
        QString hintText;
        if (m_isPolish) {
            hintText = m_isAlreadyMarked
                ? QStringLiteral("<span style='color: rgba(255,255,255,0.55); font-size: 11px;'>[Enter] Dalej / Zatwierdź   •   [Backspace] Wstecz   •   [Del] Wyczyść oznaczenie   •   [Esc] Anuluj</span>")
                : QStringLiteral("<span style='color: rgba(255,255,255,0.55); font-size: 11px;'>[Enter] Dalej / Zatwierdź   •   [Backspace] Wstecz (cofa krok)   •   [Esc] Anuluj</span>");
        } else {
            hintText = m_isAlreadyMarked
                ? QStringLiteral("<span style='color: rgba(255,255,255,0.55); font-size: 11px;'>[Enter] Next / Confirm   •   [Backspace] Back   •   [Del] Remove mark   •   [Esc] Cancel</span>")
                : QStringLiteral("<span style='color: rgba(255,255,255,0.55); font-size: 11px;'>[Enter] Next / Confirm   •   [Backspace] Back (previous step)   •   [Esc] Cancel</span>");
        }
        auto *lblHint = new QLabel(hintText, bottomWidget);
        lblHint->setTextFormat(Qt::RichText);
        lblHint->setAlignment(Qt::AlignCenter);
        bLayout->addWidget(lblHint);

        layout->addWidget(bottomWidget);
    }

    void goToStep(int step)
    {
        if (step < 0 || step > 3) return;
        m_currentStep = step;
        m_stack->setCurrentIndex(step);

        // Update breadcrumb pills
        for (int i = 0; i < 4; ++i) {
            if (i == step) {
                m_stepPills[i]->setStyleSheet(QStringLiteral(
                    "QPushButton {"
                    "  padding: 4px 12px;"
                    "  border-radius: 12px;"
                    "  font-weight: bold;"
                    "  font-size: 12px;"
                    "  background-color: #3498db;"
                    "  color: #ffffff;"
                    "}"
                ));
            } else if (i < step) {
                m_stepPills[i]->setStyleSheet(QStringLiteral(
                    "QPushButton {"
                    "  padding: 4px 12px;"
                    "  border-radius: 12px;"
                    "  font-weight: bold;"
                    "  font-size: 12px;"
                    "  background-color: rgba(46, 204, 113, 0.25);"
                    "  color: #2ecc71;"
                    "}"
                ));
            } else {
                m_stepPills[i]->setStyleSheet(QStringLiteral(
                    "QPushButton {"
                    "  padding: 4px 12px;"
                    "  border-radius: 12px;"
                    "  font-weight: bold;"
                    "  font-size: 12px;"
                    "  background-color: rgba(255, 255, 255, 0.08);"
                    "  color: rgba(255, 255, 255, 0.65);"
                    "}"
                ));
            }
        }

        m_btnBack->setEnabled(step > 0);

        if (step == 0) {
            m_btnNext->setText(m_isPolish ? QStringLiteral("Dalej (Kolor) ❯") : QStringLiteral("Next (Color) ❯"));
            m_btnNext->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #3498db; border-radius: 8px; color: #ffffff; font-weight: bold; font-size: 14px; padding: 8px 20px; }"
                "QPushButton:hover { background-color: #2980b9; }"
            ));
            setFocus();
        } else if (step == 1) {
            m_btnNext->setText(m_isPolish ? QStringLiteral("Dalej (Zakres) ❯") : QStringLiteral("Next (Scope) ❯"));
            m_btnNext->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #3498db; border-radius: 8px; color: #ffffff; font-weight: bold; font-size: 14px; padding: 8px 20px; }"
                "QPushButton:hover { background-color: #2980b9; }"
            ));
            updatePaletteUI();
            setFocus();
        } else if (step == 2) {
            updateScopeCardsUI();
            m_btnNext->setText(m_isPolish ? QStringLiteral("Dalej (Zapis) ❯") : QStringLiteral("Next (Save) ❯"));
            m_btnNext->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #3498db; border-radius: 8px; color: #ffffff; font-weight: bold; font-size: 14px; padding: 8px 20px; }"
                "QPushButton:hover { background-color: #2980b9; }"
            ));
            setFocus();
        } else if (step == 3) {
            updateStep3Preview();
            m_btnNext->setText(m_isPolish ? QStringLiteral("✓ Zastosuj oznaczenie") : QStringLiteral("✓ Apply Mark"));
            m_btnNext->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background-color: #2ecc71;"
                "  border-radius: 8px;"
                "  color: #ffffff;"
                "  font-weight: bold;"
                "  font-size: 14px;"
                "  padding: 8px 22px;"
                "}"
                "QPushButton:hover { background-color: #27ae60; }"
            ));
            setFocus();
        }
    }

    void tryFinishMark()
    {
        if (m_labelEdit) {
            m_label = m_labelEdit->text().trimmed();
        }
        if (m_useLetter && !m_letter.isEmpty() && m_usedLettersMap.contains(m_letter)) {
            const QString existingApp = m_usedLettersMap.value(m_letter).toString();
            if (!existingApp.isEmpty() && existingApp.compare(m_appClass, Qt::CaseInsensitive) != 0) {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle(m_isPolish ? QStringLiteral("Wspólna litera oznaczenia") : tr("Shared Mark Letter"));
                msgBox.setText(m_isPolish
                    ? QStringLiteral("Litera <b>'%1'</b> jest już przypisana do aplikacji <b>%2</b>.<br><br>Co chcesz zrobić?").arg(m_letter, existingApp)
                    : tr("Letter <b>'%1'</b> is already assigned to <b>%2</b>.<br><br>What would you like to do?").arg(m_letter, existingApp));

                QPushButton *btnGroup = msgBox.addButton(
                    m_isPolish ? QStringLiteral("🏷️ Połącz w grupę (Smart Workspace)") : tr("🏷️ Join Group (Smart Workspace)"),
                    QMessageBox::AcceptRole);
                QPushButton *btnReassign = msgBox.addButton(
                    m_isPolish ? QStringLiteral("🔄 Przepisz (tylko to okno)") : tr("🔄 Reassign (this window only)"),
                    QMessageBox::DestructiveRole);
                QPushButton *btnCancel = msgBox.addButton(
                    m_isPolish ? QStringLiteral("Anuluj") : tr("Cancel"),
                    QMessageBox::RejectRole);

                btnGroup->setStyleSheet(QStringLiteral("background-color: #3498db; color: white; font-weight: bold; padding: 6px 14px; border-radius: 6px;"));
                btnReassign->setStyleSheet(QStringLiteral("background-color: rgba(231, 76, 60, 0.8); color: white; font-weight: bold; padding: 6px 14px; border-radius: 6px;"));
                btnCancel->setStyleSheet(QStringLiteral("background-color: rgba(255, 255, 255, 0.15); color: white; padding: 6px 14px; border-radius: 6px;"));

                msgBox.setDefaultButton(btnGroup);
                msgBox.exec();

                if (msgBox.clickedButton() == btnCancel) {
                    goToStep(0);
                    return;
                }
                m_reassignLetter = (msgBox.clickedButton() == btnReassign);
            }
        }
        accept();
    }

private:
    bool m_reassignLetter = false;
    QColor m_color;
    bool m_useColor;
    QString m_letter;
    bool m_useLetter;
    QString m_label;
    bool m_groupByApp;
    QString m_matchCaption;
    QString m_detectedCaption;
    bool m_persistent;
    int m_desktopNum;
    QString m_desktopName;
    bool m_matchDesktop;
    QString m_appClass;
    QString m_windowCaption;
    bool m_isAlreadyMarked;
    bool m_markRemoved;
    QJsonObject m_usedLettersMap;
    bool m_isPolish;
    int m_currentStep;
    int m_seedOffset;
    int m_rerollCount;
    qreal m_basePt;
    QList<QColor> m_paletteColors;

    QPoint m_dragPos;
    bool m_isDragging;

    MainMarkerFrame *m_mainFrame;
    QPushButton *m_stepPills[4];
    QStackedWidget *m_stack;
    QWidget *m_step0Page;
    QWidget *m_step1Page;
    QWidget *m_step2Page;
    QWidget *m_step3Page;

    // Step 0 widgets
    QLabel *m_step0BadgeBox;
    QLabel *m_lblStep0Conflict;
    QPushButton *m_btnStep0Keep;
    QPushButton *m_btnStep0NoLetter;

    // Step 1 widgets
    QGridLayout *m_paletteGrid;
    QList<QPushButton*> m_paletteButtons;
    QPushButton *m_btnReroll;
    QPushButton *m_btnCustomColor;
    QPushButton *m_btnNoColor;

    // Step 2 widgets
    MarkerOptionCard *m_cardAllApp;
    MarkerOptionCard *m_cardCaption;
    QLabel *m_lblCardCaptionDesc = nullptr;
    MarkerOptionCard *m_cardSingleWindow;

    // Step 3 widgets
    QFrame *m_previewCard;
    QLabel *m_previewBadge;
    QLabel *m_previewTitle;
    QLabel *m_previewScope;
    QLabel *m_previewConflictWarn;
    QLineEdit *m_labelEdit;
    QPushButton *m_btnTogglePersistent = nullptr;
    QPushButton *m_btnToggleDesktop = nullptr;

    // Bottom bar
    QPushButton *m_btnBack;
    QPushButton *m_btnRemove = nullptr;
    QPushButton *m_btnCancel;
    QPushButton *m_btnNext;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("leapk-custom-marker"));
    app.setApplicationDisplayName(QObject::tr("LeapK - Custom Marker"));

    QColor currentColor(QStringLiteral("#3498db"));
    if (argc > 1 && argv[1]) {
        const QColor parsed(QString::fromUtf8(argv[1]));
        if (parsed.isValid()) {
            currentColor = parsed;
        }
    }

    QString currentLetter = QStringLiteral("A");
    if (argc > 2 && argv[2]) {
        currentLetter = QString::fromUtf8(argv[2]).trimmed().toUpper();
        if (currentLetter.isEmpty()) {
            currentLetter = QStringLiteral("A");
        }
    }

    QSettings settings(QStringLiteral("leapk"), QStringLiteral("colorpicker"));
    if (!settings.contains(QStringLiteral("MarkWithLetter"))) {
        QSettings oldSettings(QStringLiteral("kwin-active-window-outline"), QStringLiteral("colorpicker"));
        if (oldSettings.contains(QStringLiteral("MarkWithLetter"))) {
            settings.setValue(QStringLiteral("MarkWithLetter"), oldSettings.value(QStringLiteral("MarkWithLetter")));
            settings.setValue(QStringLiteral("MarkWithColor"), oldSettings.value(QStringLiteral("MarkWithColor")));
            settings.setValue(QStringLiteral("MarkPersistent"), oldSettings.value(QStringLiteral("MarkPersistent"), true));
        }
    }
    bool useLetter = settings.value(QStringLiteral("MarkWithLetter"), true).toBool();
    bool useColor = settings.value(QStringLiteral("MarkWithColor"), true).toBool();
    bool usePersistent = settings.value(QStringLiteral("MarkPersistent"), true).toBool();

    if (argc > 3 && argv[3]) {
        const QString arg3 = QString::fromUtf8(argv[3]).trimmed();
        if (arg3 == QStringLiteral("1") || arg3 == QStringLiteral("true")) {
            useLetter = true;
        } else if (arg3 == QStringLiteral("0") || arg3 == QStringLiteral("false")) {
            useLetter = false;
        }
    }
    if (argc > 4 && argv[4]) {
        const QString arg4 = QString::fromUtf8(argv[4]).trimmed();
        if (arg4 == QStringLiteral("1") || arg4 == QStringLiteral("true")) {
            useColor = true;
        } else if (arg4 == QStringLiteral("0") || arg4 == QStringLiteral("false")) {
            useColor = false;
        }
    }

    QString currentLabel;
    if (argc > 5 && argv[5]) {
        currentLabel = QString::fromUtf8(argv[5]).trimmed();
    }
    if (argc > 6 && argv[6]) {
        const QString arg6 = QString::fromUtf8(argv[6]).trimmed();
        if (arg6 == QStringLiteral("1") || arg6 == QStringLiteral("true")) {
            usePersistent = true;
        } else if (arg6 == QStringLiteral("0") || arg6 == QStringLiteral("false")) {
            usePersistent = false;
        }
    }

    QString appClass;
    if (argc > 7 && argv[7]) {
        appClass = QString::fromUtf8(argv[7]).trimmed();
    }

    QString windowCaption;
    if (argc > 8 && argv[8]) {
        windowCaption = QString::fromUtf8(argv[8]).trimmed();
    }

    int desktopNum = 0;
    if (argc > 9 && argv[9]) {
        desktopNum = QString::fromUtf8(argv[9]).trimmed().toInt();
    }

    QString desktopName;
    if (argc > 10 && argv[10]) {
        desktopName = QString::fromUtf8(argv[10]).trimmed();
    }

    bool isAlreadyMarked = false;
    if (argc > 11 && argv[11]) {
        const QString mStr = QString::fromUtf8(argv[11]).trimmed();
        isAlreadyMarked = (mStr == QStringLiteral("1") || mStr == QStringLiteral("true"));
    }

    QJsonObject usedLettersMap;
    if (argc > 12 && argv[12]) {
        const QString arg12 = QString::fromUtf8(argv[12]).trimmed();
        if (arg12.startsWith(QLatin1Char('{'))) {
            const QJsonDocument doc = QJsonDocument::fromJson(arg12.toUtf8());
            if (doc.isObject()) {
                usedLettersMap = doc.object();
            }
        }
    }

    CustomMarkerDialog dlg(currentColor,
                           currentLetter,
                           useLetter,
                           useColor,
                           currentLabel,
                           usePersistent,
                           appClass,
                           windowCaption,
                           desktopNum,
                           desktopName,
                           isAlreadyMarked,
                           usedLettersMap);

    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.isMarkRemoved()) {
            QJsonObject resp;
            resp[QStringLiteral("action")] = QStringLiteral("remove");
            resp[QStringLiteral("removePersistent")] = dlg.persistent();
            std::cout << QJsonDocument(resp).toJson(QJsonDocument::Compact).toStdString() << std::endl;
            return 0;
        }

        const bool finalUseLetter = dlg.useLetter();
        const bool finalUseColor = dlg.useColor();
        const bool finalPersistent = dlg.persistent();
        const bool finalGroupByApp = dlg.groupByApp();
        const QString finalCaptionPattern = dlg.matchCaption();

        settings.setValue(QStringLiteral("MarkWithLetter"), finalUseLetter);
        settings.setValue(QStringLiteral("MarkWithColor"), finalUseColor);
        settings.setValue(QStringLiteral("MarkPersistent"), finalPersistent);
        settings.setValue(QStringLiteral("MarkGroupByApp"), finalGroupByApp);

        QString finalLetter = QStringLiteral("none");
        if (finalUseLetter) {
            finalLetter = dlg.letter();
            if (finalLetter.isEmpty()) {
                finalLetter = currentLetter;
            }
        }

        QString finalColor = QStringLiteral("none");
        if (finalUseColor) {
            const QColor c = dlg.color();
            finalColor = (c.alpha() < 255) ? c.name(QColor::HexArgb) : c.name(QColor::HexRgb);
        }

        QString finalLabel = dlg.label();
        if (finalLabel.isEmpty()) {
            finalLabel = QStringLiteral("-");
        }

        QJsonObject resp;
        resp[QStringLiteral("color")] = finalColor;
        resp[QStringLiteral("letter")] = finalLetter;
        resp[QStringLiteral("label")] = finalLabel;
        resp[QStringLiteral("persist")] = finalPersistent;
        resp[QStringLiteral("groupByApp")] = finalGroupByApp;
        resp[QStringLiteral("matchCaption")] = finalCaptionPattern;
        resp[QStringLiteral("matchDesktop")] = dlg.matchDesktop();
        resp[QStringLiteral("reassign")] = dlg.reassignLetter();

        std::cout << QJsonDocument(resp).toJson(QJsonDocument::Compact).toStdString() << std::endl;
        return 0;
    }

    return 1;
}
