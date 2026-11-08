#include <QApplication>
#include <QBuffer>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDialog>
#include <QDir>
#include <QDrag>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMap>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#include <cstdlib>

// ============================================================================
// 1. Settings & Ubuntu Global Shortcut Manager
// ============================================================================
class SettingsManager {
public:
    enum Corner {
        BottomRight = 0,
        BottomLeft  = 1,
        TopRight    = 2,
        TopLeft     = 3
    };

    static QSettings& prefs() {
        static QSettings s("UbuntuTools", "MacShot");
        return s;
    }

    static QKeySequence shortcut() {
        return QKeySequence(prefs().value("shortcut", "Ctrl+Shift+4").toString());
    }

    static void setShortcut(const QKeySequence& seq) {
        prefs().setValue("shortcut", seq.toString());
        registerUbuntuGlobalShortcut(seq);
    }

    static Corner corner() {
        return static_cast<Corner>(prefs().value("corner", BottomRight).toInt());
    }

    static void setCorner(Corner c) {
        prefs().setValue("corner", static_cast<int>(c));
    }

    static int dismissSeconds() {
        return prefs().value("dismissSeconds", 6).toInt();
    }

    static void setDismissSeconds(int sec) {
        prefs().setValue("dismissSeconds", sec);
    }

    static bool runAtStartup() {
        return QFile::exists(autostartFilePath());
    }

    static void setRunAtStartup(bool enable) {
        QString path = autostartFilePath();
        if (!enable) {
            QFile::remove(path);
            return;
        }
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QString content = QString(
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=MacShot\n"
                "Comment=macOS-style Screenshot Utility\n"
                "Exec=\"%1\" --background\n"
                "Terminal=false\n"
                "X-GNOME-Autostart-enabled=true\n"
            ).arg(QCoreApplication::applicationFilePath());
            file.write(content.toUtf8());
        }
    }

    // Converts Qt key sequences (including shifted symbols like Ctrl+Shift+$) to GNOME format
    static QString toGnomeAccelerator(const QKeySequence& seq) {
        if (seq.isEmpty()) return QString();

        QKeyCombination comb = seq[0];
        Qt::KeyboardModifiers mods = comb.keyboardModifiers();
        Qt::Key key = comb.key();

        QString result;
        if (mods & Qt::ControlModifier) result += "<Control>";
        if (mods & Qt::ShiftModifier)   result += "<Shift>";
        if (mods & Qt::AltModifier)     result += "<Alt>";
        if (mods & Qt::MetaModifier)    result += "<Super>";

        static const QMap<Qt::Key, QString> specialMap = {
            {Qt::Key_Exclam, "1"},      {Qt::Key_At, "2"},
            {Qt::Key_NumberSign, "3"},  {Qt::Key_Dollar, "4"},
            {Qt::Key_Percent, "5"},     {Qt::Key_AsciiCircum, "6"},
            {Qt::Key_Ampersand, "7"},   {Qt::Key_Asterisk, "8"},
            {Qt::Key_ParenLeft, "9"},   {Qt::Key_ParenRight, "0"},
            {Qt::Key_Print, "Print"},   {Qt::Key_SysReq, "Print"},
            {Qt::Key_Space, "space"},   {Qt::Key_Return, "Return"},
            {Qt::Key_Enter, "Return"},  {Qt::Key_Escape, "Escape"},
            {Qt::Key_Tab, "Tab"},       {Qt::Key_Backtab, "Tab"},
            {Qt::Key_Backspace, "BackSpace"}, {Qt::Key_Delete, "Delete"},
            {Qt::Key_Home, "Home"},     {Qt::Key_End, "End"},
            {Qt::Key_PageUp, "Page_Up"},{Qt::Key_PageDown, "Page_Down"},
            {Qt::Key_Left, "Left"},     {Qt::Key_Right, "Right"},
            {Qt::Key_Up, "Up"},         {Qt::Key_Down, "Down"}
        };

        if (specialMap.contains(key)) {
            result += specialMap.value(key);
        } else if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            result += QChar(key).toLower();
        } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            result += QChar(key);
        } else if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
            result += QString("F%1").arg(int(key - Qt::Key_F1) + 1);
        } else {
            result += QKeySequence(key).toString(QKeySequence::PortableText).toLower();
        }
        return result;
    }

    static void registerUbuntuGlobalShortcut(const QKeySequence& seq) {
        QString gnomeBinding = toGnomeAccelerator(seq);
        if (gnomeBinding.isEmpty()) return;

        QString schemaPath = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/macshot/";
        QString baseSchema = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:" + schemaPath;

        // Wrap command in single quotes so GVariant parses the inner double quotes + args properly
        QString rawCmd = QString("\"%1\" --capture").arg(QCoreApplication::applicationFilePath());
        QString gvariantCmd = QString("'%1'").arg(rawCmd);
        QString gvariantName = "'MacShot Capture'";
        QString gvariantBinding = QString("'%1'").arg(gnomeBinding);

        QString currentList = runGSettings({"get", "org.gnome.settings-daemon.plugins.media-keys", "custom-keybindings"});

        if (!currentList.contains(schemaPath)) {
            QString newList;
            if (currentList == "@as []" || currentList.isEmpty()) {
                newList = QString("['%1']").arg(schemaPath);
            } else if (currentList.endsWith("]")) {
                newList = currentList.left(currentList.length() - 1) + QString(", '%1']").arg(schemaPath);
            }
            if (!newList.isEmpty()) {
                runGSettings({"set", "org.gnome.settings-daemon.plugins.media-keys", "custom-keybindings", newList});
            }
        }

        runGSettings({"set", baseSchema, "name", gvariantName});
        runGSettings({"set", baseSchema, "command", gvariantCmd});
        runGSettings({"set", baseSchema, "binding", gvariantBinding});
    }

    // Folder inside ~/Pictures so Snap browsers (Firefox/Chromium) and Nautilus can read dragged files
    static QString dragTempDir() {
        QString dirPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/.macshot_temp";
        QDir dir(dirPath);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        return dirPath;
    }

private:
    static QString runGSettings(const QStringList& args) {
        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("GTK_PATH");
        env.remove("LD_LIBRARY_PATH");
        env.remove("GIO_MODULE_DIR");
        proc.setProcessEnvironment(env);
        proc.start("gsettings", args);
        proc.waitForFinished(1500);
        return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    }

    static QString autostartFilePath() {
        return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
               + "/autostart/macshot.desktop";
    }
};

// ============================================================================
// 2. Annotation Canvas & Full Preview Editor Dialog
// ============================================================================
class AnnotationCanvas : public QWidget {
    Q_OBJECT
public:
    enum Tool { Arrow, Rectangle, Pen, Text, Pixelate, Blur };

    struct Annotation {
        Tool tool;
        QColor color;
        int strokeWidth;
        QPointF start;
        QPointF end;
        QList<QPointF> path;
        QString text;
    };

    explicit AnnotationCanvas(const QPixmap& basePixmap, QWidget* parent = nullptr)
        : QWidget(parent), m_basePixmap(basePixmap) {
        setMinimumSize(480, 320);
        setCursor(Qt::CrossCursor);
    }

    void setTool(Tool t) {
        m_currentTool = t;
        setCursor(t == Text ? Qt::IBeamCursor : Qt::CrossCursor);
    }

    void setColor(const QColor& c) { m_currentColor = c; }
    QColor color() const { return m_currentColor; }
    void setStrokeWidth(int w) { m_strokeWidth = w; }

    void undo() {
        if (!m_annotations.isEmpty()) {
            m_annotations.removeLast();
            emit annotationsChanged();
            update();
        }
    }

    void clearAll() {
        if (!m_annotations.isEmpty()) {
            m_annotations.clear();
            emit annotationsChanged();
            update();
        }
    }

    QPixmap renderFinalPixmap() const {
        QPixmap result = m_basePixmap.copy();
        QPainter p(&result);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);

        for (const auto& item : m_annotations) {
            drawAnnotation(p, item, m_basePixmap, false);
        }
        if (m_drawing) {
            drawAnnotation(p, m_activeAnnotation, m_basePixmap, true);
        }
        return result;
    }

signals:
    void annotationsChanged();

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.fillRect(rect(), QColor(30, 30, 30));

        QPixmap composed = renderFinalPixmap();
        p.drawPixmap(imageDisplayRect(), composed);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton || !isInsideImage(event->pos())) return;

        QPointF imgPt = mapToImage(event->pos());

        if (m_currentTool == Text) {
            bool ok = false;
            QString txt = QInputDialog::getText(
                this, "Add Text Annotation", "Text:", QLineEdit::Normal, "", &ok);
            if (ok && !txt.trimmed().isEmpty()) {
                Annotation item{Text, m_currentColor, m_strokeWidth, imgPt, imgPt, {}, txt};
                m_annotations.append(item);
                emit annotationsChanged();
                update();
            }
            return;
        }

        m_drawing = true;
        m_activeAnnotation = Annotation{m_currentTool, m_currentColor, m_strokeWidth, imgPt, imgPt, {imgPt}, ""};
        update();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!m_drawing) return;
        QPointF imgPt = mapToImage(event->pos());
        m_activeAnnotation.end = imgPt;
        if (m_currentTool == Pen) {
            m_activeAnnotation.path.append(imgPt);
        }
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && m_drawing) {
            m_drawing = false;
            m_activeAnnotation.end = mapToImage(event->pos());
            m_annotations.append(m_activeAnnotation);
            emit annotationsChanged();
            update();
        }
    }

private:
    QRect imageDisplayRect() const {
        QSize avail = size() - QSize(24, 24);
        QSize scaled = m_basePixmap.size().scaled(avail, Qt::KeepAspectRatio);
        return QRect(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled);
    }

    bool isInsideImage(const QPoint& widgetPt) const {
        return imageDisplayRect().contains(widgetPt);
    }

    QPointF mapToImage(const QPoint& widgetPt) const {
        QRect disp = imageDisplayRect();
        if (disp.width() <= 0 || disp.height() <= 0) return QPointF(0, 0);
        double relX = qBound(0.0, double(widgetPt.x() - disp.left()) / disp.width(), 1.0);
        double relY = qBound(0.0, double(widgetPt.y() - disp.top()) / disp.height(), 1.0);
        return QPointF(relX * m_basePixmap.width(), relY * m_basePixmap.height());
    }

    static void drawAnnotation(QPainter& p,
                               const Annotation& item,
                               const QPixmap& sourcePixmap,
                               bool isActiveDrag) {
        QPen pen(item.color, item.strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        switch (item.tool) {
            case Rectangle:
                p.drawRect(QRectF(item.start, item.end).normalized());
                break;
            case Arrow: {
                p.drawLine(item.start, item.end);
                double angle = std::atan2(item.end.y() - item.start.y(), item.end.x() - item.start.x());
                double headLen = qMax(14.0, item.strokeWidth * 3.8);
                QPointF p1 = item.end - QPointF(std::cos(angle - M_PI / 6.0) * headLen,
                                                std::sin(angle - M_PI / 6.0) * headLen);
                QPointF p2 = item.end - QPointF(std::cos(angle + M_PI / 6.0) * headLen,
                                                std::sin(angle + M_PI / 6.0) * headLen);
                QPolygonF head;
                head << item.end << p1 << p2;
                p.setBrush(item.color);
                p.drawPolygon(head);
                break;
            }
            case Pen: {
                if (item.path.size() > 1) {
                    QPainterPath path(item.path.first());
                    for (int i = 1; i < item.path.size(); ++i) path.lineTo(item.path[i]);
                    p.drawPath(path);
                }
                break;
            }
            case Text: {
                QFont font = p.font();
                font.setPixelSize(qMax(18, item.strokeWidth * 6));
                font.setBold(true);
                p.setFont(font);
                p.setPen(item.color);
                p.drawText(item.start, item.text);
                break;
            }
            case Pixelate: {
                QRect region = QRectF(item.start, item.end).normalized().toRect().intersected(sourcePixmap.rect());
                if (region.width() < 2 || region.height() < 2) break;
                int blockSize = qMax(8, item.strokeWidth * 4);
                QPixmap sub = sourcePixmap.copy(region);
                QPixmap tiny = sub.scaled(qMax(1, region.width() / blockSize),
                                          qMax(1, region.height() / blockSize),
                                          Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                QPixmap pix = tiny.scaled(region.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
                p.save();
                p.setRenderHint(QPainter::SmoothPixmapTransform, false);
                p.drawPixmap(region.topLeft(), pix);
                if (isActiveDrag) {
                    p.setPen(QPen(QColor(255, 255, 255, 180), 1, Qt::DashLine));
                    p.drawRect(region);
                }
                p.restore();
                break;
            }
            case Blur: {
                QRect region = QRectF(item.start, item.end).normalized().toRect().intersected(sourcePixmap.rect());
                if (region.width() < 2 || region.height() < 2) break;
                int scaleFactor = qMax(4, item.strokeWidth * 2);
                QSize smallSize(qMax(1, region.width() / scaleFactor), qMax(1, region.height() / scaleFactor));
                QPixmap blurred = sourcePixmap.copy(region);
                for (int pass = 0; pass < 3; ++pass) {
                    blurred = blurred.scaled(smallSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                     .scaled(region.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }
                p.drawPixmap(region.topLeft(), blurred);
                if (isActiveDrag) {
                    p.setPen(QPen(QColor(255, 255, 255, 180), 1, Qt::DashLine));
                    p.drawRect(region);
                }
                break;
            }
        }
    }

    QPixmap m_basePixmap;
    QList<Annotation> m_annotations;
    Annotation m_activeAnnotation;
    bool m_drawing = false;
    Tool m_currentTool = Arrow;
    QColor m_currentColor = QColor(255, 59, 48);
    int m_strokeWidth = 4;
};

class PreviewEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreviewEditorDialog(const QPixmap& pixmap, QWidget* parent = nullptr)
        : QDialog(parent) {
        setWindowTitle("Annotate & Save Screenshot");
        resize(860, 580);

        auto* layout = new QVBoxLayout(this);
        auto* toolbarLayout = new QHBoxLayout();
        auto* toolGroup = new QButtonGroup(this);
        toolGroup->setExclusive(true);

        auto makeToolBtn = [&](const QString& label, AnnotationCanvas::Tool tool, bool checked = false) {
            auto* btn = new QPushButton(label, this);
            btn->setCheckable(true);
            btn->setChecked(checked);
            toolGroup->addButton(btn, static_cast<int>(tool));
            toolbarLayout->addWidget(btn);
            return btn;
        };

        makeToolBtn("↗ Arrow", AnnotationCanvas::Arrow, true);
        makeToolBtn("▭ Rectangle", AnnotationCanvas::Rectangle);
        makeToolBtn("✎ Pen", AnnotationCanvas::Pen);
        makeToolBtn("T Text", AnnotationCanvas::Text);
        makeToolBtn("▦ Pixelate", AnnotationCanvas::Pixelate);
        makeToolBtn("≈ Blur", AnnotationCanvas::Blur);

        toolbarLayout->addSpacing(10);
        m_colorBtn = new QPushButton("Color", this);
        updateColorButtonStyle(QColor(255, 59, 48));
        toolbarLayout->addWidget(m_colorBtn);

        auto* widthSpin = new QSpinBox(this);
        widthSpin->setRange(2, 16);
        widthSpin->setValue(4);
        widthSpin->setPrefix("Size: ");
        widthSpin->setSuffix(" px");
        toolbarLayout->addWidget(widthSpin);
        toolbarLayout->addStretch();

        auto* undoBtn = new QPushButton("Undo (Ctrl+Z)", this);
        undoBtn->setShortcut(QKeySequence::Undo);
        auto* clearBtn = new QPushButton("Clear", this);
        toolbarLayout->addWidget(undoBtn);
        toolbarLayout->addWidget(clearBtn);
        layout->addLayout(toolbarLayout);

        m_canvas = new AnnotationCanvas(pixmap, this);
        layout->addWidget(m_canvas, 1);

        auto* btnLayout = new QHBoxLayout();
        m_statusLabel = new QLabel(
            QString("%1 x %2 px  •  Changes auto-copy to clipboard")
                .arg(pixmap.width()).arg(pixmap.height()), this);
        m_statusLabel->setStyleSheet("color: #888888;");

        auto* copyBtn = new QPushButton("Copy to Clipboard", this);
        auto* saveBtn = new QPushButton("Save As...", this);
        auto* closeBtn = new QPushButton("Done", this);
        saveBtn->setDefault(true);

        btnLayout->addWidget(m_statusLabel);
        btnLayout->addStretch();
        btnLayout->addWidget(copyBtn);
        btnLayout->addWidget(saveBtn);
        btnLayout->addWidget(closeBtn);
        layout->addLayout(btnLayout);

        connect(toolGroup, &QButtonGroup::idClicked, this, [this](int id) {
            m_canvas->setTool(static_cast<AnnotationCanvas::Tool>(id));
        });
        connect(m_colorBtn, &QPushButton::clicked, this, [this]() {
            QColor chosen = QColorDialog::getColor(m_canvas->color(), this, "Select Annotation Color");
            if (chosen.isValid()) {
                m_canvas->setColor(chosen);
                updateColorButtonStyle(chosen);
            }
        });
        connect(widthSpin, &QSpinBox::valueChanged, m_canvas, &AnnotationCanvas::setStrokeWidth);
        connect(undoBtn, &QPushButton::clicked, m_canvas, &AnnotationCanvas::undo);
        connect(clearBtn, &QPushButton::clicked, m_canvas, &AnnotationCanvas::clearAll);

        connect(m_canvas, &AnnotationCanvas::annotationsChanged, this, [this]() {
            QApplication::clipboard()->setPixmap(m_canvas->renderFinalPixmap());
        });
        connect(copyBtn, &QPushButton::clicked, this, [this]() {
            QApplication::clipboard()->setPixmap(m_canvas->renderFinalPixmap());
            m_statusLabel->setText("Copied annotated image to clipboard!");
        });
        connect(saveBtn, &QPushButton::clicked, this, &PreviewEditorDialog::saveScreenshot);
        connect(closeBtn, &QPushButton::clicked, this, [this]() {
            QApplication::clipboard()->setPixmap(m_canvas->renderFinalPixmap());
            accept();
        });
    }

private:
    void updateColorButtonStyle(const QColor& c) {
        m_colorBtn->setStyleSheet(QString(
            "background-color: %1; color: %2; font-weight: bold; padding: 4px 10px; border-radius: 4px;"
        ).arg(c.name(), c.lightness() > 140 ? "#000000" : "#ffffff"));
    }

    void saveScreenshot() {
        QString defaultName = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
            + "/Screenshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";

        QString filePath = QFileDialog::getSaveFileName(
            this, "Save Annotated Screenshot", defaultName,
            "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;WebP Image (*.webp)"
        );
        if (!filePath.isEmpty()) {
            m_canvas->renderFinalPixmap().save(filePath);
            accept();
        }
    }

    AnnotationCanvas* m_canvas = nullptr;
    QPushButton* m_colorBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
};

// ============================================================================
// 3. macOS-Style Floating Corner Thumbnail (Safe Click & Drag-and-Drop)
// ============================================================================
class FloatingThumbnail : public QWidget {
    Q_OBJECT
public:
    explicit FloatingThumbnail(const QPixmap& pixmap, QWidget* parent = nullptr)
        : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
          m_pixmap(pixmap) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(240, 160);

        // Save inside ~/Pictures/.macshot_temp so Snap browsers (Firefox/Chrome) & Nautilus can access it
        m_tempFilePath = SettingsManager::dragTempDir() + "/Screenshot_"
            + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + ".png";
        m_pixmap.save(m_tempFilePath, "PNG");

        m_dismissTimer = new QTimer(this);
        m_dismissTimer->setSingleShot(true);
        connect(m_dismissTimer, &QTimer::timeout, this, &FloatingThumbnail::animateOutAndClose);
        m_dismissTimer->start(SettingsManager::dismissSeconds() * 1000);

        positionInCorner();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        QRectF cardRect = rect().adjusted(10, 10, -10, -10);
        QPainterPath path;
        path.addRoundedRect(cardRect, 12, 12);
        p.fillPath(path, QColor(30, 30, 30, 235));

        p.save();
        QRectF innerRect = cardRect.adjusted(6, 6, -6, -6);
        QPainterPath clipPath;
        clipPath.addRoundedRect(innerRect, 8, 8);
        p.setClipPath(clipPath);

        QPixmap scaled = m_pixmap.scaled(innerRect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPointF pos(
            innerRect.x() + (innerRect.width() - scaled.width()) / 2.0,
            innerRect.y() + (innerRect.height() - scaled.height()) / 2.0
        );
        p.drawPixmap(pos, scaled);
        p.restore();

        p.setPen(QPen(QColor(255, 255, 255, 90), 1.5));
        p.drawRoundedRect(cardRect, 12, 12);
    }

    void enterEvent(QEnterEvent*) override {
        if (!m_closing) {
            m_dismissTimer->stop();
        }
    }

    void leaveEvent(QEvent*) override {
        // Do NOT restart timer if we are dragging or opening the editor dialog
        if (!m_dragging && !m_closing) {
            m_dismissTimer->start(2500);
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_pressPos = event->pos();
            m_dragging = false;
        } else if (event->button() == Qt::RightButton) {
            m_closing = true;
            m_dismissTimer->stop();
            close();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!(event->buttons() & Qt::LeftButton) || m_dragging || m_closing) return;
        if ((event->pos() - m_pressPos).manhattanLength() < QApplication::startDragDistance()) return;

        m_dragging = true;
        m_dismissTimer->stop();

        auto* drag = new QDrag(this);
        auto* mime = new QMimeData();

        // 1. Local file URI first (Nautilus folders, browser file inputs, Discord, Slack)
        mime->setUrls({QUrl::fromLocalFile(m_tempFilePath)});

        // 2. PNG bytes and QImage data for rich-text editors
        QByteArray pngBytes;
        QBuffer buffer(&pngBytes);
        buffer.open(QIODevice::WriteOnly);
        m_pixmap.save(&buffer, "PNG");
        mime->setData("image/png", pngBytes);
        mime->setImageData(m_pixmap.toImage());

        drag->setMimeData(mime);
        QPixmap dragThumb = m_pixmap.scaled(120, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        drag->setPixmap(dragThumb);
        drag->setHotSpot(QPoint(dragThumb.width() / 2, dragThumb.height() / 2));

        // IMPORTANT: Keep window mapped during drag->exec() so X11/Wayland keeps the pointer grab!
        setWindowOpacity(0.25);
        Qt::DropAction dropResult = drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);

        if (dropResult != Qt::IgnoreAction) {
            m_closing = true;
            close();
        } else {
            // User cancelled drag (e.g. Esc) — keep thumbnail alive
            m_dragging = false;
            setWindowOpacity(1.0);
            m_dismissTimer->start(3000);
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && !m_dragging && !m_closing) {
            m_closing = true;
            m_dismissTimer->stop();

            // Open PreviewEditorDialog asynchronously on the heap so closing it never touches a freed 'this'
            auto* dlg = new PreviewEditorDialog(m_pixmap);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
            dlg->raise();
            dlg->activateWindow();

            close();
        }
    }

private:
    void positionInCorner() {
        QScreen* scr = QGuiApplication::primaryScreen();
        QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1920, 1080);
        const int margin = 20;

        QPoint targetPos;
        switch (SettingsManager::corner()) {
            case SettingsManager::BottomRight:
                targetPos = QPoint(avail.right() - width() - margin, avail.bottom() - height() - margin);
                break;
            case SettingsManager::BottomLeft:
                targetPos = QPoint(avail.left() + margin, avail.bottom() - height() - margin);
                break;
            case SettingsManager::TopRight:
                targetPos = QPoint(avail.right() - width() - margin, avail.top() + margin);
                break;
            case SettingsManager::TopLeft:
                targetPos = QPoint(avail.left() + margin, avail.top() + margin);
                break;
        }

        QPoint startPos = targetPos + QPoint((targetPos.x() > avail.center().x()) ? 60 : -60, 0);
        move(startPos);
        show();

        auto* anim = new QPropertyAnimation(this, "pos", this);
        anim->setDuration(220);
        anim->setStartValue(startPos);
        anim->setEndValue(targetPos);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    void animateOutAndClose() {
        if (m_closing || m_dragging) return;
        m_closing = true;
        auto* fade = new QPropertyAnimation(this, "windowOpacity", this);
        fade->setDuration(200);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        connect(fade, &QPropertyAnimation::finished, this, &QWidget::close);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    }

    QPixmap m_pixmap;
    QString m_tempFilePath;
    QTimer* m_dismissTimer = nullptr;
    QPoint m_pressPos;
    bool m_dragging = false;
    bool m_closing = false;
};

// ============================================================================
// 4. Multi-Monitor Region Selection Overlay (Spans All Connected Displays)
// ============================================================================
class RegionSelectionOverlay : public QWidget {
    Q_OBJECT
public:
    explicit RegionSelectionOverlay(const QPixmap& fullDesktopPixmap,
                                    const QRect& virtualGeometry,
                                    QWidget* parent = nullptr)
        : QWidget(parent,
                  Qt::Window | Qt::FramelessWindowHint |
                  Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint),
          m_desktopPixmap(fullDesktopPixmap),
          m_virtualGeo(virtualGeometry) {
        setAttribute(Qt::WA_DeleteOnClose);
        setCursor(Qt::CrossCursor);

        // Span across the entire combined multi-monitor virtual desktop
        setGeometry(m_virtualGeo);
        show();
        raise();
        activateWindow();
        setFocusPolicy(Qt::StrongFocus);
        setFocus();

        // Ensure keyboard (Esc/Enter) and mouse events are captured across all monitors
        QTimer::singleShot(30, this, [this]() {
            grabKeyboard();
            grabMouse();
        });
    }

    ~RegionSelectionOverlay() override {
        releaseKeyboard();
        releaseMouse();
    }

signals:
    void regionCaptured(const QPixmap& captured);

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.drawPixmap(0, 0, m_desktopPixmap);
        p.fillRect(rect(), QColor(0, 0, 0, 110));

        if (m_selecting && !m_selectionRect.isNull()) {
            QRect norm = m_selectionRect.normalized();
            p.drawPixmap(norm, m_desktopPixmap, norm);
            p.setPen(QPen(QColor(255, 255, 255, 220), 1.5));
            p.drawRect(norm);

            QString dimText = QString("%1 x %2").arg(norm.width()).arg(norm.height());
            QRect badgeRect(norm.left(), qMax(8, norm.top() - 28), 95, 22);
            p.setBrush(QColor(25, 25, 25, 210));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(badgeRect, 5, 5);
            p.setPen(Qt::white);
            p.drawText(badgeRect, Qt::AlignCenter, dimText);
        } else {
            // Draw the help banner on whichever monitor currently has the cursor
            QScreen* activeScreen = QGuiApplication::screenAt(QCursor::pos());
            if (!activeScreen) activeScreen = QGuiApplication::primaryScreen();
            QRect localScreenRect = activeScreen->geometry().translated(-m_virtualGeo.topLeft());

            QRect banner(localScreenRect.center().x() - 270, localScreenRect.top() + 30, 540, 36);
            p.setBrush(QColor(20, 20, 20, 210));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(banner, 8, 8);
            p.setPen(Qt::white);
            p.drawText(banner, Qt::AlignCenter,
                       "Drag across any screen  •  Enter: Current Monitor  •  Shift+Enter: All  •  Esc: Cancel");
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_selecting = true;
            m_origin = event->pos();
            m_selectionRect = QRect(m_origin, QSize());
            update();
        } else if (event->button() == Qt::RightButton) {
            close();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_selecting) {
            m_selectionRect = QRect(m_origin, event->pos());
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && m_selecting) {
            m_selecting = false;
            QRect norm = m_selectionRect.normalized();
            releaseKeyboard();
            releaseMouse();
            close();
            if (norm.width() > 5 && norm.height() > 5) {
                emit regionCaptured(m_desktopPixmap.copy(norm));
            }
        }
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            releaseKeyboard();
            releaseMouse();
            close();
        } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
            releaseKeyboard();
            releaseMouse();
            close();

            // Shift+Enter captures all monitors; Enter captures the monitor under the cursor
            if (event->modifiers() & Qt::ShiftModifier) {
                emit regionCaptured(m_desktopPixmap);
            } else {
                QScreen* activeScreen = QGuiApplication::screenAt(QCursor::pos());
                if (!activeScreen) activeScreen = QGuiApplication::primaryScreen();
                QRect screenLocal = activeScreen->geometry().translated(-m_virtualGeo.topLeft());
                emit regionCaptured(m_desktopPixmap.copy(screenLocal));
            }
        }
    }

private:
    QPixmap m_desktopPixmap;
    QRect m_virtualGeo;
    bool m_selecting = false;
    QPoint m_origin;
    QRect m_selectionRect;
};

// ============================================================================
// 5. Preferences & Keybinding Dialog
// ============================================================================
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("MacShot Preferences");
        setMinimumWidth(420);

        auto* mainLayout = new QVBoxLayout(this);
        auto* form = new QFormLayout();

        m_keyEdit = new QKeySequenceEdit(SettingsManager::shortcut(), this);
        form->addRow("Capture Shortcut:", m_keyEdit);

        m_cornerCombo = new QComboBox(this);
        m_cornerCombo->addItem("Bottom-Right (macOS Default)", SettingsManager::BottomRight);
        m_cornerCombo->addItem("Bottom-Left", SettingsManager::BottomLeft);
        m_cornerCombo->addItem("Top-Right", SettingsManager::TopRight);
        m_cornerCombo->addItem("Top-Left", SettingsManager::TopLeft);
        m_cornerCombo->setCurrentIndex(static_cast<int>(SettingsManager::corner()));
        form->addRow("Thumbnail Corner:", m_cornerCombo);

        m_durationSpin = new QSpinBox(this);
        m_durationSpin->setRange(2, 30);
        m_durationSpin->setSuffix(" seconds");
        m_durationSpin->setValue(SettingsManager::dismissSeconds());
        form->addRow("Preview Duration:", m_durationSpin);

        m_startupCheck = new QCheckBox("Launch MacShot at Ubuntu login in background", this);
        m_startupCheck->setChecked(SettingsManager::runAtStartup());
        form->addRow("Startup:", m_startupCheck);

        mainLayout->addLayout(form);

        auto* btnLayout = new QHBoxLayout();
        auto* testBtn = new QPushButton("Take Screenshot Now", this);
        auto* saveBtn = new QPushButton("Save Settings", this);
        saveBtn->setDefault(true);

        btnLayout->addWidget(testBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(saveBtn);
        mainLayout->addLayout(btnLayout);

        connect(testBtn, &QPushButton::clicked, this, [this]() {
            hide();
            QTimer::singleShot(250, this, &SettingsDialog::requestCapture);
        });

        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            SettingsManager::setShortcut(m_keyEdit->keySequence());
            SettingsManager::setCorner(
                static_cast<SettingsManager::Corner>(m_cornerCombo->currentData().toInt()));
            SettingsManager::setDismissSeconds(m_durationSpin->value());
            SettingsManager::setRunAtStartup(m_startupCheck->isChecked());
            hide();
        });
    }

signals:
    void requestCapture();

private:
    QKeySequenceEdit* m_keyEdit = nullptr;
    QComboBox* m_cornerCombo = nullptr;
    QSpinBox* m_durationSpin = nullptr;
    QCheckBox* m_startupCheck = nullptr;
};

// ============================================================================
// 6. Background Daemon Controller
// ============================================================================
class ScreenshotDaemon : public QObject {
    Q_OBJECT
public:
    explicit ScreenshotDaemon(QObject* parent = nullptr) : QObject(parent) {
        setupSingleInstanceServer();
        setupTrayIcon();
        SettingsManager::registerUbuntuGlobalShortcut(SettingsManager::shortcut());
    }

public slots:
    void triggerCapture() {
        const QList<QScreen*> screens = QGuiApplication::screens();
        if (screens.isEmpty()) return;

        bool isWaylandSession = (qEnvironmentVariable("XDG_SESSION_TYPE").toLower() == "wayland")
                                || !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty();
        if (isWaylandSession) {
            captureViaPortal();
            return;
        }

        // 1. Compute the bounding rectangle across all connected monitors
        QRect virtualGeo;
        for (QScreen* scr : screens) {
            virtualGeo = virtualGeo.united(scr->geometry());
        }

        // 2. Grab the full X11 root window spanning all monitors
        QPixmap fullDesktop = screens.first()->grabWindow(
            0,
            virtualGeo.x(),
            virtualGeo.y(),
            virtualGeo.width(),
            virtualGeo.height()
        );

        // Fallback: if root window grab fails, stitch each screen individually
        if (fullDesktop.isNull()) {
            fullDesktop = QPixmap(virtualGeo.size());
            fullDesktop.fill(Qt::black);
            QPainter painter(&fullDesktop);
            for (QScreen* scr : screens) {
                QRect g = scr->geometry();
                QPixmap sPix = scr->grabWindow(0);
                painter.drawPixmap(g.topLeft() - virtualGeo.topLeft(), sPix);
            }
        }

        if (fullDesktop.isNull()) {
            captureViaPortal();
            return;
        }

        auto* overlay = new RegionSelectionOverlay(fullDesktop, virtualGeo);
        connect(overlay, &RegionSelectionOverlay::regionCaptured,
                this, &ScreenshotDaemon::handleCapturedPixmap);
    }

    void openSettings() {
        if (!m_settingsDialog) {
            m_settingsDialog = new SettingsDialog();
            connect(m_settingsDialog, &SettingsDialog::requestCapture,
                    this, &ScreenshotDaemon::triggerCapture);
        }
        m_settingsDialog->show();
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
    }

private slots:
    void onPortalResponse(uint response, const QVariantMap& results) {
        if (response != 0) return;
        QUrl uri = results.value("uri").toUrl();
        QString localPath = uri.toLocalFile();
        if (!localPath.isEmpty()) {
            QPixmap pix(localPath);
            QFile::remove(localPath);
            if (!pix.isNull()) {
                handleCapturedPixmap(pix);
            }
        }
    }

private:
    void handleCapturedPixmap(const QPixmap& pixmap) {
        if (pixmap.isNull()) return;
        QApplication::clipboard()->setPixmap(pixmap);
        new FloatingThumbnail(pixmap);
    }

    void captureViaPortal() {
        QDBusInterface portal(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Screenshot",
            QDBusConnection::sessionBus()
        );

        QVariantMap options;
        options["interactive"] = true;
        options["modal"] = true;

        QDBusReply<QDBusObjectPath> reply = portal.call("Screenshot", "", options);
        if (reply.isValid()) {
            QDBusConnection::sessionBus().connect(
                "org.freedesktop.portal.Desktop",
                reply.value().path(),
                "org.freedesktop.portal.Request",
                "Response",
                this,
                SLOT(onPortalResponse(uint, QVariantMap))
            );
        }
    }

    void setupTrayIcon() {
        m_tray = new QSystemTrayIcon(QApplication::style()->standardIcon(QStyle::SP_DesktopIcon), this);
        m_tray->setToolTip("MacShot Screenshot Utility");

        auto* menu = new QMenu();
        QAction* captureAct = menu->addAction("Take Screenshot");
        QAction* prefsAct   = menu->addAction("Preferences...");
        menu->addSeparator();
        QAction* quitAct    = menu->addAction("Quit MacShot");

        connect(captureAct, &QAction::triggered, this, &ScreenshotDaemon::triggerCapture);
        connect(prefsAct, &QAction::triggered, this, &ScreenshotDaemon::openSettings);
        connect(quitAct, &QAction::triggered, qApp, &QCoreApplication::quit);

        m_tray->setContextMenu(menu);
        m_tray->show();
    }

    void setupSingleInstanceServer() {
        m_ipcServer = new QLocalServer(this);
        QLocalServer::removeServer("macshot_ipc_socket");
        m_ipcServer->listen("macshot_ipc_socket");
        connect(m_ipcServer, &QLocalServer::newConnection, this, [this]() {
            QLocalSocket* client = m_ipcServer->nextPendingConnection();
            connect(client, &QLocalSocket::readyRead, this, [this, client]() {
                QByteArray cmd = client->readAll();
                if (cmd.contains("CAPTURE")) {
                    triggerCapture();
                } else if (cmd.contains("SETTINGS")) {
                    openSettings();
                }
                client->disconnectFromServer();
            });
        });
    }

    QSystemTrayIcon* m_tray = nullptr;
    QLocalServer* m_ipcServer = nullptr;
    SettingsDialog* m_settingsDialog = nullptr;
};

// ============================================================================
// 7. Entry Point
// ============================================================================
int main(int argc, char *argv[]) {
    // Strip Snap IDE environment pollution and use XCB so corner positioning & XDND work on Wayland/X11
    qunsetenv("GTK_PATH");
    qunsetenv("GIO_MODULE_DIR");
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }

    QApplication app(argc, argv);
    app.setApplicationName("MacShot");
    app.setOrganizationName("UbuntuTools");
    app.setQuitOnLastWindowClosed(false);

    QStringList args = app.arguments();
    bool triggerCaptureArg = args.contains("--capture");
    bool backgroundArg     = args.contains("--background");

    QLocalSocket socket;
    socket.connectToServer("macshot_ipc_socket");
    if (socket.waitForConnected(150)) {
        socket.write(triggerCaptureArg ? "CAPTURE" : "SETTINGS");
        socket.waitForBytesWritten(150);
        return 0;
    }

    ScreenshotDaemon daemon;
    if (triggerCaptureArg) {
        QTimer::singleShot(100, &daemon, &ScreenshotDaemon::triggerCapture);
    } else if (!backgroundArg) {
        daemon.openSettings();
    }

    return app.exec();
}

#include "main.moc"