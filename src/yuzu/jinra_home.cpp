// SPDX-License-Identifier: GPL-3.0-or-later
#include "yuzu/jinra_home.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDirIterator>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include "common/input.h"
#include "common/settings.h"
#include "input_common/main.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/game_list/game_list_p.h"
#include "yuzu/game/game_list.h"

namespace {
QIcon NavigationIcon(int index) {
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(2);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(32.0 / 24, 32.0 / 24);
    p.setPen(QPen(QColor("#b9d5ef"), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (index == 0) {
        for (int y : {4, 14})
            for (int x : {4, 14})
                p.drawRoundedRect(QRectF(x, y, 6, 6), 1, 1);
    } else if (index == 1) {
        p.drawRoundedRect(QRectF(3, 4, 18, 13), 2, 2);
        p.drawLine(12, 17, 12, 21);
        p.drawLine(8, 21, 16, 21);
    } else if (index == 2) {
        QPainterPath b;
        b.moveTo(7, 7);
        b.cubicTo(3, 6, 1, 18, 4, 19);
        b.cubicTo(6, 20, 8, 15, 10, 15);
        b.lineTo(14, 15);
        b.cubicTo(16, 15, 18, 20, 20, 19);
        b.cubicTo(23, 18, 21, 6, 17, 7);
        b.closeSubpath();
        p.drawPath(b);
        p.drawLine(6, 10, 6, 14);
        p.drawLine(4, 12, 8, 12);
        p.drawEllipse(QPointF(17, 10), 0.8, 0.8);
        p.drawEllipse(QPointF(19, 13), 0.8, 0.8);
    } else {
        QPolygonF gear;
        for (int i = 0; i < 32; ++i) {
            double a = i * 3.141592653589793 / 16;
            double r = (i % 4 == 1 || i % 4 == 2) ? 9 : 7;
            gear << QPointF(12 + r * std::cos(a), 12 + r * std::sin(a));
        }
        p.drawPolygon(gear);
        p.drawEllipse(QPointF(12, 12), 3, 3);
    }
    return QIcon(pixmap);
}

QLabel* Label(const QString& text, const char* name, QWidget* parent = nullptr) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QLatin1String(name));
    label->setWordWrap(true);
    return label;
}

QComboBox* Combo(std::initializer_list<std::pair<const char*, int>> values) {
    auto* box = new QComboBox;
    for (const auto& [label, value] : values)
        box->addItem(QString::fromUtf8(label), value);
    box->setMinimumWidth(220);
    return box;
}

void Choose(QComboBox* box, int value) {
    box->setCurrentIndex(box->findData(value));
}

QFrame* Card(QVBoxLayout* layout) {
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("JinraCard"));
    layout->addWidget(frame);
    return frame;
}

QString PhysicalButton(Common::Input::ButtonNames name) {
    using B = Common::Input::ButtonNames;
    switch (name) {
    case B::ButtonA:
        return QStringLiteral("A");
    case B::ButtonB:
        return QStringLiteral("B");
    case B::ButtonX:
        return QStringLiteral("X");
    case B::ButtonY:
        return QStringLiteral("Y");
    case B::Cross:
        return QString::fromUtf8("×");
    case B::Circle:
        return QString::fromUtf8("○");
    case B::Square:
        return QString::fromUtf8("□");
    case B::Triangle:
        return QString::fromUtf8("△");
    case B::L1:
        return QStringLiteral("L1");
    case B::L2:
        return QStringLiteral("L2");
    case B::R1:
        return QStringLiteral("R1");
    case B::R2:
        return QStringLiteral("R2");
    case B::XboxLB:
        return QStringLiteral("LB");
    case B::XboxLT:
        return QStringLiteral("LT");
    case B::XboxRB:
        return QStringLiteral("RB");
    case B::XboxRT:
        return QStringLiteral("RT");
    case B::TriggerL:
        return QStringLiteral("L");
    case B::TriggerR:
        return QStringLiteral("R");
    case B::TriggerZL:
        return QStringLiteral("ZL");
    case B::TriggerZR:
        return QStringLiteral("ZR");
    default:
        return {};
    }
}
} // namespace

class JinraControllerView final : public QWidget {
public:
    explicit JinraControllerView(QWidget* parent)
        : QWidget(parent), xbox(QStringLiteral(":/jinra/xbox.png")),
          dualsense(QStringLiteral(":/jinra/dualsense.png")) {
        setObjectName(QStringLiteral("JinraControllerArt"));
        setMinimumSize(340, 230);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    void SetLayout(int value) {
        if (layout != value) {
            layout = value;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const auto& image = layout == 2 ? dualsense : xbox;
        if (image.isNull())
            return;
        QSizeF target = image.size();
        target.scale(QSizeF(std::min(width() - 32, 620), std::min(height() - 28, 390)),
                     Qt::KeepAspectRatio);
        const QRectF rect((width() - target.width()) / 2, (height() - target.height()) / 2,
                          target.width(), target.height());
        p.drawPixmap(rect, image, QRectF(image.rect()));
    }

private:
    QPixmap xbox, dualsense;
    int layout{1};
};

JinraHome::JinraHome(GameList* games_, QWidget* empty_library, InputCommon::InputSubsystem* input_,
                     QWidget* parent)
    : QWidget(parent), games(games_), input(input_) {
    setObjectName(QStringLiteral("JinraHome"));
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("JinraSidebar"));
    sidebar->setFixedWidth(170);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(16, 24, 16, 18);
    side->setSpacing(8);
    auto* brand = new QHBoxLayout;
    auto* logo = new QLabel;
    logo->setPixmap(QPixmap(QStringLiteral(":/jinra/icon.png"))
                        .scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setFixedSize(48, 48);
    logo->setScaledContents(true);
    brand->addWidget(logo);
    brand->addWidget(Label(QStringLiteral("Jinra"), "JinraBrand"));
    side->addLayout(brand);
    side->addSpacing(20);
    auto* group = new QButtonGroup(this);
    pages = new QStackedWidget;
    for (int i = 0; i < 4; ++i) {
        auto* button = new QPushButton;
        button->setObjectName(QStringLiteral("JinraNav"));
        button->setCheckable(true);
        button->setIcon(NavigationIcon(i));
        button->setIconSize(QSize(22, 22));
        navigation[i] = button;
        group->addButton(button, i);
        side->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i] {
            pages->setCurrentIndex(i);
            RefreshControllers();
        });
    }
    side->addStretch();
    auto* advanced = new QPushButton(tr("All settings"));
    advanced->setObjectName(QStringLiteral("JinraSecondary"));
    connect(advanced, &QPushButton::clicked, this, [this] { emit ConfigureRequested(0); });
    side->addWidget(advanced);
    side->addWidget(Label(QStringLiteral("Jinra · NS2"), "JinraMuted"));
    root->addWidget(sidebar);
    root->addWidget(pages, 1);
    pages->addWidget(CreateLibrary(empty_library));
    pages->addWidget(CreateGraphics());
    pages->addWidget(CreateControls());
    pages->addWidget(CreateSystem());
    navigation[0]->setChecked(true);
    controller_timer = new QTimer(this);
    controller_timer->setInterval(500);
    connect(controller_timer, &QTimer::timeout, this, &JinraHome::RefreshControllers);
    controller_timer->start();
    connect(games, &GameList::GameHighlighted, this, &JinraHome::SelectGame);
    connect(games, &GameList::PopulatingCompleted, this, &JinraHome::SelectFirstGame);
    ReloadSettings();
    Retranslate();
}

QWidget* JinraHome::CreateLibrary(QWidget* empty_library) {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 22, 26, 18);
    layout->setSpacing(18);
    auto* header = new QHBoxLayout;
    headings[0] = Label({}, "JinraHeading");
    header->addWidget(headings[0]);
    header->addStretch();
    auto* search = games->TakeSearchField();
    search->setMaximumWidth(350);
    search->setMinimumWidth(220);
    header->addWidget(search);
    auto* add = new QPushButton(tr("Add folder"));
    add->setObjectName(QStringLiteral("JinraSecondary"));
    connect(add, &QPushButton::clicked, this, &JinraHome::AddDirectory);
    header->addWidget(add);
    layout->addLayout(header);
    auto* filters = new QHBoxLayout;
    auto* collection_group = new QButtonGroup(this);
    const std::array<QString, 3> collections{tr("All games"), tr("Recent"), tr("Favorites")};
    for (int i = 0; i < 3; ++i) {
        auto* b = new QPushButton(collections[i]);
        b->setObjectName(QStringLiteral("JinraFilter"));
        b->setCheckable(true);
        collection_group->addButton(b, i);
        filters->addWidget(b);
        if (i == 0)
            b->setChecked(true);
        connect(b, &QPushButton::clicked, this, [this, i] { games->SetCollectionFilter(i); });
    }
    filters->addStretch();
    layout->addLayout(filters);
    auto* content = new QHBoxLayout;
    auto* list = new QVBoxLayout;
    list->addWidget(games, 1);
    list->addWidget(empty_library, 1);
    content->addLayout(list, 1);
    auto* detail = new QFrame;
    detail->setObjectName(QStringLiteral("JinraCard"));
    detail->setFixedWidth(245);
    auto* d = new QVBoxLayout(detail);
    d->setContentsMargins(18, 20, 18, 20);
    d->setSpacing(14);
    detail_image = new QLabel;
    detail_image->setAlignment(Qt::AlignCenter);
    detail_image->setFixedHeight(164);
    detail_image->setPixmap(QPixmap(QStringLiteral(":/jinra/icon.png"))
                                .scaled(170, 170, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    detail_title = Label(tr("Select a game"), "JinraGameTitle");
    detail_status = Label(tr("Your library, ready to play."), "JinraMuted");
    play = new QPushButton(tr("Play"));
    play->setObjectName(QStringLiteral("JinraPrimary"));
    play->setEnabled(false);
    properties = new QPushButton(tr("Properties"));
    properties->setEnabled(false);
    connect(play, &QPushButton::clicked, this, [this] {
        if (selected_game.isValid())
            emit BootRequested(selected_game.data(GameListItemPath::FullPathRole).toString());
    });
    connect(properties, &QPushButton::clicked, this, [this] {
        if (selected_game.isValid())
            emit PropertiesRequested(selected_game.data(GameListItemPath::FullPathRole).toString());
    });
    d->addWidget(detail_image);
    d->addWidget(detail_title);
    d->addWidget(detail_status);
    d->addWidget(play);
    d->addWidget(properties);
    d->addStretch(1);
    content->addWidget(detail, 0, Qt::AlignTop);
    layout->addLayout(content, 1);
    library_status = Label({}, "JinraMuted");
    layout->addWidget(library_status);
    connect(games, &GameList::FilterCountChanged, this, [this](int visible, int total) {
        library_status->setText(
            tr("%1 of %2 games  ·  %3  ·  %4  ·  %5")
                .arg(visible)
                .arg(total)
                .arg(renderer ? renderer->currentText() : QStringLiteral("Vulkan"),
                     resolution ? resolution->currentText() : QStringLiteral("2×"),
                     memory ? memory->currentText() : QStringLiteral("12 GB")));
    });
    return page;
}

QWidget* JinraHome::CreateGraphics() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(20);
    headings[1] = Label({}, "JinraHeading");
    layout->addWidget(headings[1]);
    auto* form = new QFormLayout(Card(layout));
    form->setContentsMargins(24, 24, 24, 24);
    form->setSpacing(18);
    renderer = Combo({{"Vulkan", 1}, {"OpenGL", 0}, {"Null", 2}});
    resolution =
        Combo({{"0.5×", 1}, {"0.75×", 2}, {"1×", 3}, {"1.5×", 5}, {"2×", 6}, {"3×", 7}, {"4×", 8}});
    antialiasing = Combo({{"None", 0}, {"FXAA", 1}, {"SMAA", 2}});
    filter = Combo({{"Nearest", 0},
                    {"Bilinear", 1},
                    {"Bicubic", 2},
                    {"Gaussian", 3},
                    {"Lanczos", 4},
                    {"ScaleForce", 5},
                    {"FSR", 6}});
    accuracy = Combo({{"Normal", 0}, {"High", 1}});
    form->addRow(tr("Graphics API"), renderer);
    form->addRow(tr("Resolution scale"), resolution);
    form->addRow(tr("Anti-aliasing"), antialiasing);
    form->addRow(tr("Image filter"), filter);
    form->addRow(tr("GPU accuracy"), accuracy);
    async_shaders = new QCheckBox(tr("Compile shaders in the background"));
    form->addRow(async_shaders);
    auto* advanced = new QPushButton(tr("Advanced graphics settings"));
    connect(advanced, &QPushButton::clicked, this, [this] { emit ConfigureRequested(3); });
    layout->addWidget(advanced, 0, Qt::AlignLeft);
    layout->addStretch();
    auto* footer = new QHBoxLayout;
    footer->addStretch();
    auto* cancel = new QPushButton(tr("Discard changes"));
    auto* apply = new QPushButton(tr("Apply"));
    apply->setObjectName(QStringLiteral("JinraPrimary"));
    connect(cancel, &QPushButton::clicked, this, &JinraHome::ReloadSettings);
    connect(apply, &QPushButton::clicked, this, &JinraHome::ApplySettings);
    footer->addWidget(cancel);
    footer->addWidget(apply);
    layout->addLayout(footer);
    return page;
}

QWidget* JinraHome::CreateControls() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);
    headings[2] = Label({}, "JinraHeading");
    layout->addWidget(headings[2]);
    auto* bar = new QHBoxLayout;
    player = new QComboBox;
    for (int i = 0; i < 8; ++i)
        player->addItem(tr("Player %1").arg(i + 1), i);
    controller_layout =
        Combo({{"Automatic", 0}, {"Xbox layout preview", 1}, {"DualSense layout preview", 2}});
    bar->addWidget(player);
    bar->addStretch();
    bar->addWidget(controller_layout);
    layout->addLayout(bar);
    controller_status = Label({}, "JinraAccent");
    layout->addWidget(controller_status);
    auto* card = Card(layout);
    auto* c = new QHBoxLayout(card);
    controller_view = new JinraControllerView(card);
    c->addWidget(controller_view, 1);
    auto* mapping = new QVBoxLayout;
    mapping_heading = Label(tr("Nintendo → physical button"), "JinraMuted");
    mapping->addWidget(mapping_heading);
    for (auto*& label : mappings) {
        label = Label({}, "JinraMapping");
        mapping->addWidget(label);
    }
    mapping->addStretch();
    c->addLayout(mapping);
    layout->setStretchFactor(card, 1);
    mapping_status = Label({}, "JinraMuted");
    layout->addWidget(mapping_status);
    auto* configure = new QPushButton(tr("Configure buttons and profiles"));
    configure->setObjectName(QStringLiteral("JinraPrimary"));
    connect(configure, &QPushButton::clicked, this, [this] { emit ConfigureRequested(5); });
    layout->addWidget(configure, 0, Qt::AlignRight);
    connect(player, &QComboBox::currentIndexChanged, this, [this] { RefreshControllers(); });
    connect(controller_layout, &QComboBox::currentIndexChanged, this,
            [this] { RefreshControllers(); });
    return page;
}

QWidget* JinraHome::CreateSystem() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(20);
    headings[3] = Label({}, "JinraHeading");
    layout->addWidget(headings[3]);
    auto* form = new QFormLayout(Card(layout));
    form->setContentsMargins(24, 24, 24, 24);
    form->setSpacing(18);
    memory = Combo({{"4 GB", 0}, {"6 GB", 1}, {"8 GB", 2}, {"10 GB", 3}, {"12 GB", 4}});
    console_mode = Combo({{"Handheld", 0}, {"Docked", 1}});
    language = new QComboBox;
    language->addItem(tr("System language"), QString{});
    language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    QDirIterator it(QStringLiteral(":/languages"));
    while (it.hasNext()) {
        const auto file = it.next();
        if (!file.endsWith(QStringLiteral(".qm")))
            continue;
        const auto locale = file.section(QLatin1Char('/'), -1).section(QLatin1Char('.'), 0, 0);
        if (locale == QStringLiteral("en"))
            continue;
        const QLocale qlocale(locale);
        language->addItem(
            QStringLiteral("%1 (%2)").arg(QLocale::languageToString(qlocale.language()),
                                          QLocale::territoryToString(qlocale.territory())),
            locale);
    }
    language->setMinimumWidth(280);
    form->addRow(tr("Emulated memory"), memory);
    form->addRow(tr("Console mode"), console_mode);
    form->addRow(tr("Interface language"), language);
    auto* note = Label(tr("12 GB is the Jinra default for Switch 2 games. Existing game-specific "
                          "settings take priority."),
                       "JinraMuted");
    layout->addWidget(note);
    auto* advanced = new QPushButton(tr("All system and language settings"));
    connect(advanced, &QPushButton::clicked, this, [this] { emit ConfigureRequested(1); });
    layout->addWidget(advanced, 0, Qt::AlignLeft);
    auto* performance = new QVBoxLayout(Card(layout));
    performance->setContentsMargins(20, 18, 20, 20);
    auto* title = new QHBoxLayout;
    title->addWidget(Label(tr("Performance"), "JinraGameTitle"));
    title->addStretch();
    title->addWidget(Label(tr("No game running"), "JinraMuted"));
    performance->addLayout(title);
    auto* stats = new QHBoxLayout;
    for (const auto& name : {tr("FPS"), tr("Frame time"), tr("GPU usage")}) {
        auto* value = new QVBoxLayout;
        value->addWidget(Label(name, "JinraMuted"));
        value->addWidget(Label(QString::fromUtf8("—"), "JinraGameTitle"));
        stats->addLayout(value);
    }
    performance->addSpacing(18);
    performance->addLayout(stats);
    auto* line = new QFrame;
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background:#24507a;"));
    performance->addSpacing(16);
    performance->addWidget(line);
    layout->addStretch();
    auto* footer = new QHBoxLayout;
    footer->addStretch();
    auto* cancel = new QPushButton(tr("Discard changes"));
    auto* apply = new QPushButton(tr("Apply"));
    apply->setObjectName(QStringLiteral("JinraPrimary"));
    connect(cancel, &QPushButton::clicked, this, &JinraHome::ReloadSettings);
    connect(apply, &QPushButton::clicked, this, &JinraHome::ApplySettings);
    footer->addWidget(cancel);
    footer->addWidget(apply);
    layout->addLayout(footer);
    return page;
}

void JinraHome::ReloadSettings() {
    Choose(renderer, static_cast<int>(Settings::values.renderer_backend.GetValue()));
    Choose(resolution, static_cast<int>(Settings::values.resolution_setup.GetValue()));
    Choose(antialiasing, static_cast<int>(Settings::values.anti_aliasing.GetValue()));
    Choose(filter, static_cast<int>(Settings::values.scaling_filter.GetValue()));
    Choose(accuracy, static_cast<int>(Settings::values.gpu_accuracy.GetValue()));
    async_shaders->setChecked(Settings::values.use_asynchronous_shaders.GetValue());
    Choose(memory, static_cast<int>(Settings::values.memory_layout_mode.GetValue()));
    Choose(console_mode, static_cast<int>(Settings::values.use_docked_mode.GetValue()));
    language->setCurrentIndex(
        language->findData(QString::fromStdString(UISettings::values.language.GetValue())));
    library_status->setText(
        tr("%1  ·  Resolution %2  ·  %3")
            .arg(renderer->currentText(), resolution->currentText(), memory->currentText()));
    RefreshControllers();
}

void JinraHome::ApplySettings() {
    if (emulating)
        return;
    Settings::SetConfiguringGlobal(true);
    if (renderer->currentIndex() >= 0)
        Settings::values.renderer_backend.SetValue(
            static_cast<Settings::RendererBackend>(renderer->currentData().toInt()));
    if (resolution->currentIndex() >= 0)
        Settings::values.resolution_setup.SetValue(
            static_cast<Settings::ResolutionSetup>(resolution->currentData().toInt()));
    if (antialiasing->currentIndex() >= 0)
        Settings::values.anti_aliasing.SetValue(
            static_cast<Settings::AntiAliasing>(antialiasing->currentData().toInt()));
    if (filter->currentIndex() >= 0)
        Settings::values.scaling_filter.SetValue(
            static_cast<Settings::ScalingFilter>(filter->currentData().toInt()));
    if (accuracy->currentIndex() >= 0)
        Settings::values.gpu_accuracy.SetValue(
            static_cast<Settings::GpuAccuracy>(accuracy->currentData().toInt()));
    Settings::values.use_asynchronous_shaders.SetValue(async_shaders->isChecked());
    if (memory->currentIndex() >= 0)
        Settings::values.memory_layout_mode.SetValue(
            static_cast<Settings::MemoryLayout>(memory->currentData().toInt()));
    if (console_mode->currentIndex() >= 0)
        Settings::values.use_docked_mode.SetValue(
            static_cast<Settings::ConsoleMode>(console_mode->currentData().toInt()));
    const auto locale = language->currentData().toString();
    if (language->currentIndex() >= 0 &&
        locale.toStdString() != UISettings::values.language.GetValue())
        emit LanguageRequested(locale);
    emit SaveSettings();
    ReloadSettings();
}

void JinraHome::SetEmulating(bool running) {
    emulating = running;
    if (running) {
        controller_timer->stop();
        pages->setCurrentIndex(0);
        navigation[0]->setChecked(true);
    } else {
        controller_timer->start();
        ReloadSettings();
    }
    for (int i = 1; i < 4; ++i)
        navigation[i]->setEnabled(!running);
}

void JinraHome::SelectGame(const QModelIndex& index) {
    const auto item = index.siblingAtColumn(0);
    if (!item.isValid() || item.data(GameListItemPath::FullPathRole).toString().isEmpty()) {
        selected_game = QPersistentModelIndex();
        detail_title->setText(tr("Select a game"));
        detail_status->setText(tr("Choose a game from the library."));
        detail_image->clear();
        play->setEnabled(false);
        properties->setEnabled(false);
        return;
    }
    selected_game = item;
    detail_title->setText(item.data(GameListItemPath::TitleRole).toString());
    const auto image = item.data(Qt::DecorationRole).value<QPixmap>();
    if (!image.isNull()) {
        const auto dpr = devicePixelRatioF();
        const int edge = std::min(164 * qRound(dpr), std::max(image.width(), image.height()));
        auto display = image.scaled(edge, edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        display.setDevicePixelRatio(dpr);
        detail_image->setPixmap(display);
    }
    detail_status->setText(item.data(Qt::ToolTipRole).toString());
    play->setEnabled(true);
    properties->setEnabled(true);
}

void JinraHome::SelectFirstGame() {
    auto* model = games->GetModel();
    std::function<bool(const QModelIndex&)> visit = [&](const QModelIndex& parent) {
        for (int i = 0; i < model->rowCount(parent); ++i) {
            const auto item = model->index(i, 0, parent);
            if (!item.data(GameListItemPath::FullPathRole).toString().isEmpty()) {
                SelectGame(item);
                return true;
            }
            if (visit(item))
                return true;
        }
        return false;
    };
    if (!selected_game.isValid() && !visit({})) {
        play->setEnabled(false);
        properties->setEnabled(false);
        detail_title->setText(tr("Select a game"));
        detail_status->setText(tr("Add a folder to build your library."));
    }
}

void JinraHome::RefreshControllers() {
    if (!controller_view || !isVisible() || emulating || pages->currentIndex() != 2)
        return;
    auto devices = input->GetInputDevices();
    const Common::ParamPackage* active = nullptr;
    u64 newest{};
    for (const auto& device : devices) {
        if (device.Get("engine", "") != "sdl" || device.Get("guid2", "") != "")
            continue;
        const auto activity = QString::fromStdString(device.Get("last_active", "0")).toULongLong();
        if (!active || activity > newest) {
            active = &device;
            newest = activity;
        }
    }
    const auto name = active ? QString::fromStdString(active->Get("display", "Controller"))
                             : tr("No controller connected");
    const bool sony = active && (name.contains(QStringLiteral("DualSense"), Qt::CaseInsensitive) ||
                                 name.contains(QStringLiteral("DualShock"), Qt::CaseInsensitive));
    const int preview = controller_layout->currentData().toInt();
    controller_view->SetLayout(preview ? preview : (sony ? 2 : 1));
    controller_status->setText(
        preview ? tr("Layout preview — %1")
                      .arg(preview == 2 ? QStringLiteral("DualSense") : QStringLiteral("Xbox"))
                : (active ? name : tr("No controller connected — Xbox layout preview")));
    static constexpr std::array<int, 8> buttons{
        Settings::NativeButton::A,  Settings::NativeButton::B, Settings::NativeButton::X,
        Settings::NativeButton::Y,  Settings::NativeButton::L, Settings::NativeButton::R,
        Settings::NativeButton::ZL, Settings::NativeButton::ZR};
    static constexpr std::array<const char*, 8> names{"A", "B", "X", "Y", "L", "R", "ZL", "ZR"};
    const auto& bindings =
        Settings::values.players.GetValue()[player->currentData().toInt()].buttons;
    const bool reference = preview != 0 || active == nullptr;
    const bool reference_sony = preview == 2;
    const std::array<QString, 8> reference_buttons =
        reference_sony ? std::array<QString, 8>{QString::fromUtf8("○"), QString::fromUtf8("×"),
                                                QString::fromUtf8("△"), QString::fromUtf8("□"),
                                                QStringLiteral("L1"),   QStringLiteral("R1"),
                                                QStringLiteral("L2"),   QStringLiteral("R2")}
                       : std::array<QString, 8>{QStringLiteral("B"),  QStringLiteral("A"),
                                                QStringLiteral("Y"),  QStringLiteral("X"),
                                                QStringLiteral("LB"), QStringLiteral("RB"),
                                                QStringLiteral("LT"), QStringLiteral("RT")};
    mapping_heading->setText(reference ? tr("Nintendo → reference layout")
                                       : tr("Nintendo → physical button"));
    for (int i = 0; i < 8; ++i) {
        const Common::ParamPackage params(bindings[buttons[i]]);
        QString physical = PhysicalButton(input->GetButtonName(params));
        if (reference)
            physical = reference_buttons[i];
        if (physical.isEmpty()) {
            if (params.Get("engine", "") == "keyboard")
                physical = QKeySequence(params.Get("code", 0)).toString();
            else if (params.Has("button"))
                physical = tr("Button %1").arg(params.Get("button", 0));
            else if (params.Has("axis"))
                physical = tr("Axis %1").arg(params.Get("axis", 0));
            else
                physical = QString::fromUtf8("—");
        }
        mappings[i]->setText(QString::fromUtf8("%1  →  %2").arg(QLatin1String(names[i]), physical));
    }
    mapping_status->setText(
        reference ? tr("Layout reference. Connect a controller and select Automatic to show its "
                       "saved button mappings.")
                  : tr("Saved button mappings for Player %1.").arg(player->currentIndex() + 1));
}

void JinraHome::Retranslate() {
    const std::array<QString, 4> labels{tr("Library"),
                                        QCoreApplication::translate("ConfigureDialog", "Graphics"),
                                        QCoreApplication::translate("ConfigureDialog", "Controls"),
                                        QCoreApplication::translate("ConfigureDialog", "System")};
    for (int i = 0; i < 4; ++i) {
        navigation[i]->setText(labels[i]);
        headings[i]->setText(labels[i]);
    }
    play->setText(tr("Play"));
    properties->setText(QCoreApplication::translate("GameList", "Properties"));
}

void JinraHome::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        Retranslate();
}
