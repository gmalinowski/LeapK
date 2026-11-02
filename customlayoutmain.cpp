/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QDialog>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <iostream>

#include "tilinghelper.h"
#include "themehelper.h"

namespace
{

class ThemedMainFrame : public QFrame
{
public:
    explicit ThemedMainFrame(const LeapKTheme::Theme &theme, QWidget *parent = nullptr)
        : QFrame(parent), m_theme(theme)
    {
        setAttribute(Qt::WA_StyledBackground, false);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        LeapKTheme::paintThemedWindowFrame(&p, rect(), m_theme);
    }

private:
    LeapKTheme::Theme m_theme;
};

class TilePreviewWidget : public QWidget
{
public:
    explicit TilePreviewWidget(const QList<LeapKTiling::TileSlot> &tileSlots,
                               const LeapKTheme::Theme &theme,
                               QWidget *parent = nullptr)
        : QWidget(parent), m_tileSlots(tileSlots), m_theme(theme)
    {
        setFixedHeight(130);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF area = rect().adjusted(10, 8, -10, -8);
        const QColor bg = m_theme.cardBackgroundColor(false);
        const QColor accent = m_theme.primaryAccentColor();

        // Background box
        p.setBrush(bg);
        p.setPen(QPen(m_theme.cardBorderColor(false), 1.0));
        p.drawRoundedRect(area, 6, 6);

        if (m_tileSlots.isEmpty()) {
            p.setPen(Qt::gray);
            p.drawText(area, Qt::AlignCenter, QStringLiteral("Empty layout"));
            return;
        }

        const qreal innerPad = 4.0;
        const QRectF tileArea = area.adjusted(innerPad, innerPad, -innerPad, -innerPad);

        QFont font = p.font();
        font.setBold(true);
        font.setPointSize(10);
        p.setFont(font);

        for (int i = 0; i < m_tileSlots.size(); ++i) {
            const QRectF &rel = m_tileSlots[i].relativeRect;
            const qreal x = tileArea.x() + rel.x() * tileArea.width();
            const qreal y = tileArea.y() + rel.y() * tileArea.height();
            const qreal w = rel.width() * tileArea.width();
            const qreal h = rel.height() * tileArea.height();
            const QRectF slotRect = QRectF(x, y, w, h).adjusted(2, 2, -2, -2);

            // Fill with translucent accent
            QColor fillCol = accent;
            fillCol.setAlpha(45);
            p.setBrush(fillCol);

            QColor borderCol = accent;
            borderCol.setAlpha(160);
            p.setPen(QPen(borderCol, 1.2));
            p.drawRoundedRect(slotRect, 4, 4);

            p.setPen(Qt::white);
            p.drawText(slotRect, Qt::AlignCenter, QString::number(i + 1));
        }
    }

private:
    QList<LeapKTiling::TileSlot> m_tileSlots;
    LeapKTheme::Theme m_theme;
};

class SaveLayoutDialog : public QDialog
{
public:
    SaveLayoutDialog(const QList<LeapKTiling::TileSlot> &tileSlots,
                     const QString &defaultName,
                     const QString &defaultKey,
                     const QMap<QString, QString> &existingKeys,
                     const LeapKTheme::Theme &theme,
                     QWidget *parent = nullptr)
        : QDialog(parent), m_tileSlots(tileSlots), m_existingKeys(existingKeys), m_theme(theme)
    {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setStyleSheet(LeapKTheme::generateDialogStyleSheet(m_theme));
        resize(380, 360);

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);

        auto *frame = new ThemedMainFrame(m_theme, this);
        rootLayout->addWidget(frame);

        auto *mainLayout = new QVBoxLayout(frame);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(12);

        // Header Title
        auto *titleLabel = new QLabel(QStringLiteral("▤ Save Tiling Layout"), frame);
        QFont titleFont = titleLabel->font();
        titleFont.setPointSize(13);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        titleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(m_theme.primaryAccentColor().name()));
        mainLayout->addWidget(titleLabel);

        auto *descLabel = new QLabel(QStringLiteral("Save current arrangement as a custom preset:"), frame);
        descLabel->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 0.7); font-size: 11px;"));
        mainLayout->addWidget(descLabel);

        // Preview Widget
        auto *preview = new TilePreviewWidget(m_tileSlots, m_theme, frame);
        mainLayout->addWidget(preview);

        // Form: Name & Trigger Key
        auto *formLayout = new QVBoxLayout();
        formLayout->setSpacing(8);

        auto *nameHeader = new QLabel(QStringLiteral("Layout Name / Label:"), frame);
        nameHeader->setStyleSheet(QStringLiteral("color: white; font-weight: bold; font-size: 11px;"));
        formLayout->addWidget(nameHeader);

        m_nameEdit = new QLineEdit(defaultName, frame);
        m_nameEdit->setPlaceholderText(QStringLiteral("e.g. 3 Columns Dev"));
        formLayout->addWidget(m_nameEdit);

        auto *keyRow = new QHBoxLayout();
        keyRow->setSpacing(10);

        auto *keyHeader = new QLabel(QStringLiteral("Trigger Key (A–Z):"), frame);
        keyHeader->setStyleSheet(QStringLiteral("color: white; font-weight: bold; font-size: 11px;"));
        keyRow->addWidget(keyHeader);

        m_keyEdit = new QLineEdit(defaultKey, frame);
        m_keyEdit->setFixedWidth(50);
        m_keyEdit->setAlignment(Qt::AlignCenter);
        m_keyEdit->setMaxLength(1);
        m_keyEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z]")), m_keyEdit));
        keyRow->addWidget(m_keyEdit);
        keyRow->addStretch();

        formLayout->addLayout(keyRow);

        m_statusLabel = new QLabel(frame);
        m_statusLabel->setStyleSheet(QStringLiteral("font-size: 11px;"));
        formLayout->addWidget(m_statusLabel);

        mainLayout->addLayout(formLayout);

        // Button row
        auto *btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(10);

        auto *btnCancel = new QPushButton(QStringLiteral("Cancel [Esc]"), frame);
        btnCancel->setAutoDefault(false);
        btnCancel->setCursor(Qt::PointingHandCursor);
        btnCancel->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: rgba(255, 255, 255, 0.1);"
            "  border: 1px solid rgba(255, 255, 255, 0.2);"
            "  border-radius: 4px;"
            "  color: white;"
            "  padding: 6px 14px;"
            "}"
            "QPushButton:hover { background: rgba(255, 255, 255, 0.18); }"
        ));
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
        btnLayout->addWidget(btnCancel);

        m_btnSave = new QPushButton(QStringLiteral("Save Layout [Enter]"), frame);
        m_btnSave->setAutoDefault(false);
        m_btnSave->setCursor(Qt::PointingHandCursor);
        const QColor accent = m_theme.primaryAccentColor();
        m_btnSave->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  border: 1px solid %1;"
            "  border-radius: 4px;"
            "  color: white;"
            "  font-weight: bold;"
            "  padding: 6px 18px;"
            "}"
            "QPushButton:hover { background: %2; }"
        ).arg(accent.name(), accent.lighter(115).name()));
        connect(m_btnSave, &QPushButton::clicked, this, &QDialog::accept);
        btnLayout->addWidget(m_btnSave);

        mainLayout->addLayout(btnLayout);

        connect(m_keyEdit, &QLineEdit::textChanged, this, &SaveLayoutDialog::updateKeyStatus);
        connect(m_nameEdit, &QLineEdit::textChanged, this, &SaveLayoutDialog::updateKeyStatus);

        updateKeyStatus();
        m_nameEdit->setFocus();
        m_nameEdit->selectAll();
    }

    QString layoutName() const { return m_nameEdit->text().trimmed(); }
    QString triggerKey() const { return m_keyEdit->text().trimmed().toUpper(); }

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape) {
            reject();
            return;
        }
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            if (!layoutName().isEmpty() && !triggerKey().isEmpty()) {
                accept();
                return;
            }
        }
        QDialog::keyPressEvent(e);
    }

private:
    void updateKeyStatus()
    {
        const QString key = m_keyEdit->text().trimmed().toUpper();
        const QString name = m_nameEdit->text().trimmed();

        if (name.isEmpty()) {
            m_statusLabel->setText(QStringLiteral("Enter a name for the layout"));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-size: 11px;"));
            m_btnSave->setEnabled(false);
            return;
        }

        if (key.isEmpty()) {
            m_statusLabel->setText(QStringLiteral("Enter a single letter [A–Z]"));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-size: 11px;"));
            m_btnSave->setEnabled(false);
            return;
        }

        if (key == QStringLiteral("X")) {
            m_statusLabel->setText(QStringLiteral("Key 'X' is reserved for Float / Turn Off"));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-size: 11px;"));
            m_btnSave->setEnabled(false);
            return;
        }

        m_btnSave->setEnabled(true);
        if (m_existingKeys.contains(key)) {
            const QString existName = m_existingKeys.value(key);
            m_statusLabel->setText(QStringLiteral("⚠️ Overwrites existing: \"%1\"").arg(existName));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #f39c12; font-size: 11px;"));
        } else {
            m_statusLabel->setText(QStringLiteral("✓ New shortcut key [%1]").arg(key));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #2ecc71; font-size: 11px;"));
        }
    }

    QList<LeapKTiling::TileSlot> m_tileSlots;
    QMap<QString, QString> m_existingKeys;
    LeapKTheme::Theme m_theme;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_keyEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_btnSave = nullptr;
};

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("leapk-custom-layout"));

    QList<LeapKTiling::TileSlot> tileSlotList;
    QString defaultName = QStringLiteral("Custom Layout");
    QString defaultKey = QStringLiteral("H");
    QMap<QString, QString> existingKeys;
    QString themeId = QStringLiteral("default-breeze");

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromUtf8(argv[i]);
        if (arg == QStringLiteral("--help") || arg == QStringLiteral("-h")) {
            std::cout << "Usage: leapk-custom-layout [options]\n"
                      << "  --slots <json>      JSON array of normalized slot rects [{x,y,w,h}]\n"
                      << "  --name <name>       Default name for the layout\n"
                      << "  --key <letter>      Default single letter shortcut [A-Z]\n"
                      << "  --existing <json>   JSON map of existing key -> name\n"
                      << "  --theme <id>        Theme ID (e.g. default-breeze)\n";
            return 0;
        } else if (arg == QStringLiteral("--slots") && (i + 1) < argc) {
            const QString val = QString::fromUtf8(argv[++i]);
            const QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
            if (doc.isArray()) {
                for (const auto &item : doc.array()) {
                    if (item.isObject()) {
                        const QJsonObject obj = item.toObject();
                        const qreal x = obj.value(QStringLiteral("x")).toDouble();
                        const qreal y = obj.value(QStringLiteral("y")).toDouble();
                        const qreal w = obj.value(QStringLiteral("w")).toDouble();
                        const qreal h = obj.value(QStringLiteral("h")).toDouble();
                        tileSlotList.append(LeapKTiling::TileSlot{QRectF(x, y, w, h)});
                    }
                }
            }
        } else if (arg == QStringLiteral("--name") && (i + 1) < argc) {
            defaultName = QString::fromUtf8(argv[++i]).trimmed();
        } else if (arg == QStringLiteral("--key") && (i + 1) < argc) {
            defaultKey = QString::fromUtf8(argv[++i]).trimmed().toUpper();
        } else if (arg == QStringLiteral("--theme") && (i + 1) < argc) {
            themeId = QString::fromUtf8(argv[++i]).trimmed();
        } else if (arg == QStringLiteral("--existing") && (i + 1) < argc) {
            const QString val = QString::fromUtf8(argv[++i]);
            const QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
            if (doc.isObject()) {
                const QJsonObject obj = doc.object();
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    existingKeys.insert(it.key().toUpper(), it.value().toString());
                }
            }
        }
    }

    if (tileSlotList.isEmpty()) {
        // Fallback default: 2 columns
        tileSlotList.append(LeapKTiling::TileSlot{QRectF(0.0, 0.0, 0.5, 1.0)});
        tileSlotList.append(LeapKTiling::TileSlot{QRectF(0.5, 0.0, 0.5, 1.0)});
    }

    LeapKTheme::Theme theme = LeapKTheme::findThemeById(themeId);
    SaveLayoutDialog dlg(tileSlotList, defaultName, defaultKey, existingKeys, theme);

    if (dlg.exec() == QDialog::Accepted) {
        QJsonObject resp;
        resp[QStringLiteral("name")] = dlg.layoutName();
        resp[QStringLiteral("triggerKey")] = dlg.triggerKey();
        resp[QStringLiteral("action")] = QStringLiteral("save");
        std::cout << QJsonDocument(resp).toJson(QJsonDocument::Compact).toStdString() << std::endl;
        return 0;
    }

    return 1;
}
