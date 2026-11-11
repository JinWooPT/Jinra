// SPDX-License-Identifier: GPL-3.0-or-later
#include "yuzu/jinra_home.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include "common/input.h"
#include "common/settings.h"
#include "input_common/main.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/game_list/game_list_p.h"
#include "yuzu/configuration/configure_dialog.h"
#include "yuzu/game/game_list.h"
#include "yuzu/jinra_avatar.h"

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
    } else if (index == 3) {
        QPolygonF gear;
        for (int i = 0; i < 32; ++i) {
            double a = i * 3.141592653589793 / 16;
            double r = (i % 4 == 1 || i % 4 == 2) ? 9 : 7;
            gear << QPointF(12 + r * std::cos(a), 12 + r * std::sin(a));
        }
        p.drawPolygon(gear);
        p.drawEllipse(QPointF(12, 12), 3, 3);
    } else if (index == 4) {
        p.drawLine(6, 12, 18, 12);
        p.drawLine(12, 6, 12, 18);
    } else if (index == 5) {
        p.drawEllipse(QPointF(10, 10), 5, 5);
        p.drawLine(14, 14, 20, 20);
    } else if (index == 6) {
        p.drawArc(QRectF(5, 5, 14, 14), 120 * 16, 300 * 16);
        p.drawLine(12, 3, 12, 11);
    } else if (index == 7) {
        p.drawRoundedRect(QRectF(4, 7, 16, 13), 2, 2);
        p.drawLine(4, 10, 20, 10);
        p.drawLine(10, 14, 14, 14);
    } else if (index == 8) {
        p.drawEllipse(QPointF(12, 8), 3, 3);
        p.drawArc(QRectF(5, 13, 14, 10), 0, 180 * 16);
    } else if (index == 9) {
        QPainterPath folder;
        folder.moveTo(4, 18);
        folder.lineTo(4, 6);
        folder.lineTo(10, 6);
        folder.lineTo(12, 8);
        folder.lineTo(20, 8);
        folder.lineTo(20, 18);
        folder.closeSubpath();
        p.drawPath(folder);
    } else if (index == 10) {
        for (int y : {6, 12, 18}) {
            p.drawLine(4, y, 20, y);
            const int x = y == 12 ? 8 : 15;
            p.drawLine(x, y - 2, x, y + 2);
        }
    } else if (index == 11) {
        p.drawLine(12, 4, 12, 15);
        p.drawLine(8, 11, 12, 15);
        p.drawLine(12, 15, 16, 11);
        p.drawLine(5, 15, 5, 20);
        p.drawLine(5, 20, 19, 20);
        p.drawLine(19, 20, 19, 15);
    } else if (index == 12) {
        p.drawRoundedRect(QRectF(6, 6, 12, 12), 2, 2);
        p.drawRect(QRectF(9, 9, 6, 6));
        for (int pin : {8, 12, 16}) {
            p.drawLine(pin, 3, pin, 6);
            p.drawLine(pin, 18, pin, 21);
            p.drawLine(3, pin, 6, pin);
            p.drawLine(18, pin, 21, pin);
        }
    } else if (index == 13) {
        p.drawPolygon(QPolygonF{{3, 9}, {7, 9}, {12, 5}, {12, 19}, {7, 15}, {3, 15}});
        p.drawArc(QRectF(9, 7, 11, 10), -60 * 16, 120 * 16);
        p.drawArc(QRectF(8, 4, 16, 16), -60 * 16, 120 * 16);
    } else if (index == 14) {
        p.drawPolyline(QPolygonF{{3, 11}, {12, 3.5}, {21, 11}});
        p.drawPolyline(QPolygonF{{5.5, 9}, {5.5, 20}, {18.5, 20}, {18.5, 9}});
        p.drawPolyline(QPolygonF{{10, 20}, {10, 14}, {14, 14}, {14, 20}});
    }
    return QIcon(pixmap);
}

QIcon HomeKeycap(const QString& key) {
    QPixmap icon(48, 48);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#0b1020"));
    painter.drawEllipse(icon.rect());
    auto font = painter.font();
    font.setPixelSize(24);
    font.setBold(true);
    QPainterPath glyph;
    glyph.addText(QPointF{}, font, key);
    const QPointF center(icon.width() / 2.0, icon.height() / 2.0);
    glyph.translate(center - glyph.boundingRect().center());
    painter.fillPath(glyph, QColor("#e6edf7"));
    painter.end();
    icon.setDevicePixelRatio(2);
    return QIcon(icon);
}

QToolButton* HomeTool(int icon, const QString& label, const char* style, int size) {
    auto* button = new QToolButton;
    button->setIcon(NavigationIcon(icon));
    const int edge = size >= 70 ? 29 : 23;
    button->setIconSize(QSize(edge, edge));
    button->setFixedSize(size, size);
    button->setObjectName(QLatin1String(style));
    button->setAccessibleName(label);
    button->setToolTip(label);
    button->setCursor(Qt::PointingHandCursor);
    return button;
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
    sidebar = new QFrame;
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
    connect(pages, &QStackedWidget::currentChanged, this, [this] {
        emit HomeChromeChanged(IsHomeChromeActive());
        RefreshControllers();
    });
    navigation[0]->setChecked(true);
    controller_timer = new QTimer(this);
    controller_timer->setInterval(500);
    connect(controller_timer, &QTimer::timeout, this, &JinraHome::RefreshControllers);
    controller_timer->start();
    connect(games, &GameList::GameHighlighted, this, &JinraHome::SelectGame);
    connect(games, &GameList::GameChosen, this, [this] {
        if (IsHomeChromeActive())
            home_audio.Play(JinraHomeAudio::Cue::Confirm);
    });
    connect(games, &GameList::PopulatingCompleted, this, &JinraHome::SelectFirstGame);
    connect(games, &GameList::DesktopModeRequested, this, [this] { SetHomeLayout(false); });
    ReloadSettings();
    Retranslate();
    SetHomeLayout(true);
    QTimer::singleShot(250, this, [this] {
        if (isVisible() && IsHomeChromeActive())
            home_audio.Play(JinraHomeAudio::Cue::Startup);
    });
}

QWidget* JinraHome::CreateLibrary(QWidget* empty_library) {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 24, 32, 25);
    layout->setSpacing(18);
    desktop_header = new QWidget;
    auto* header = new QHBoxLayout(desktop_header);
    header->setContentsMargins(0, 0, 0, 0);
    headings[0] = Label({}, "JinraHeading");
    header->addWidget(headings[0]);
    header->addStretch();
    library_search = games->TakeSearchField();
    library_search->setMaximumWidth(350);
    library_search->setMinimumWidth(220);
    desktop_search_layout = new QHBoxLayout;
    desktop_search_layout->addWidget(library_search);
    header->addLayout(desktop_search_layout);
    auto* add = new QPushButton(tr("Add folder"));
    add->setObjectName(QStringLiteral("JinraSecondary"));
    connect(add, &QPushButton::clicked, this, &JinraHome::AddDirectory);
    header->addWidget(add);
    auto* show_home = new QPushButton(tr("Home mode"));
    connect(show_home, &QPushButton::clicked, this, [this] { SetHomeLayout(true); });
    header->addWidget(show_home);
    CreateHomeChrome();
    layout->addWidget(desktop_header);
    layout->addWidget(home_header);
    collection_row = new QHBoxLayout;
    collection_tabs = new QWidget;
    collection_tabs->setObjectName(QStringLiteral("JinraCollections"));
    auto* filters = new QHBoxLayout(collection_tabs);
    filters->setContentsMargins(4, 4, 4, 4);
    filters->setSpacing(3);
    auto* collection_group = new QButtonGroup(this);
    const std::array<QString, 3> collections{tr("All games"), tr("Recent"), tr("Favorites")};
    for (int i : {0, 2, 1}) {
        auto* b = new QPushButton(collections[i]);
        b->setObjectName(QStringLiteral("JinraFilter"));
        b->setCheckable(true);
        collection_group->addButton(b, i);
        filters->addWidget(b);
        if (i == 0)
            b->setChecked(true);
        ConnectHomeSound(b, JinraHomeAudio::Cue::Confirm);
        connect(b, &QPushButton::clicked, this, [this, i] { games->SetCollectionFilter(i); });
    }
    home_sort_button = HomeTool(10, tr("Sort games"), "JinraHomeTool", 54);
    auto* sort_menu = new QMenu(home_sort_button);
    sort_menu->addAction(tr("Title A–Z"), games, [this] { games->SortHomeGames(Qt::AscendingOrder); });
    sort_menu->addAction(tr("Title Z–A"), games, [this] { games->SortHomeGames(Qt::DescendingOrder); });
    home_sort_button->setMenu(sort_menu);
    home_sort_button->setPopupMode(QToolButton::InstantPopup);
    collection_row->addWidget(home_sort_button);
    collection_row->addWidget(collection_tabs);
    home_search_button = HomeTool(5, tr("Search games"), "JinraHomeTool", 54);
    home_search_button->setCheckable(true);
    ConnectHomeSound(home_search_button, JinraHomeAudio::Cue::Confirm);
    collection_row->addWidget(home_search_button);
    layout->addLayout(collection_row);
    home_search = new QWidget;
    home_search_layout = new QHBoxLayout(home_search);
    home_search_layout->setContentsMargins(0, 0, 0, 0);
    home_search_layout->setAlignment(Qt::AlignHCenter);
    layout->addWidget(home_search);
    connect(home_search_button, &QToolButton::toggled, this, [this](bool open) {
        home_search->setVisible(open);
        if (open) {
            library_search->show();
            games->SetFilterFocus();
        } else {
            games->ClearFilter();
            games->FocusLibrary();
        }
    });
    auto* content = new QHBoxLayout;
    auto* list = new QVBoxLayout;
    list->addWidget(games, 1);
    list->addWidget(empty_library, 1);
    content->addLayout(list, 1);
    auto* detail = new QFrame;
    library_detail = detail;
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
    layout->addWidget(home_footer);
    connect(games, &GameList::FilterCountChanged, this, [this](int visible, int total) {
        home_count->setText(visible != total ? tr("%1 of %2 games").arg(visible).arg(total)
                                             : total == 1 ? tr("1 game") : tr("%1 games").arg(total));
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

void JinraHome::CreateHomeChrome() {
    home_header = new QWidget;
    auto* header = new QHBoxLayout(home_header);
    header->setContentsMargins(4, 2, 4, 4);
    header->setSpacing(14);
    auto* logo = new QLabel;
    logo->setFixedSize(53, 53);
    logo->setScaledContents(true);
    logo->setPixmap(QPixmap(QStringLiteral(":/jinra/icon.png")));
    header->addWidget(logo);
    header->addWidget(Label(QStringLiteral("Jinra"), "JinraBrand"));
    auto* add = HomeTool(4, tr("Add folder"), "JinraHomeTool", 54);
    ConnectHomeSound(add, JinraHomeAudio::Cue::Confirm);
    connect(add, &QToolButton::clicked, this, &JinraHome::AddDirectory);
    header->addWidget(add);
    header->addStretch();
    home_profile = HomeTool(8, tr("Profile"), "JinraHomeProfile", 54);
    ConnectHomeSound(home_profile, JinraHomeAudio::Cue::Confirm);
    connect(home_profile, &QToolButton::clicked, this, [this] {
        JinraAvatarDialog dialog(this);
        dialog.exec();
        RefreshHomeProfile();
    });
    header->addWidget(home_profile);
    RefreshHomeProfile();
    home_clock = Label({}, "JinraHomeClock");
    header->addWidget(home_clock);
    auto* controller = new QLabel;
    controller->setPixmap(NavigationIcon(2).pixmap(QSize(21, 21)));
    header->addWidget(controller);
    home_controllers = Label({}, "JinraMuted");
    header->addWidget(home_controllers);
    auto* desktop = HomeTool(1, tr("Desktop mode"), "JinraHomeTool", 54);
    connect(desktop, &QToolButton::clicked, this, [this] { SetHomeLayout(false); });
    header->addWidget(desktop);
    auto* exit = HomeTool(6, tr("Exit Jinra"), "JinraHomeTool", 54);
    connect(exit, &QToolButton::clicked, this, [this] { window()->close(); });
    header->addWidget(exit);

    home_footer = new QWidget;
    auto* footer = new QVBoxLayout(home_footer);
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(21);
    auto* dock = new QHBoxLayout;
    dock->setAlignment(Qt::AlignHCenter);
    dock->setSpacing(24);
    const auto dock_button = [this, dock](int icon, const QString& text, const char* style) {
        auto* column = new QVBoxLayout;
        column->setSpacing(6);
        auto* button = HomeTool(icon, text, style, 78);
        ConnectHomeSound(button, JinraHomeAudio::Cue::Confirm);
        column->addWidget(button, 0, Qt::AlignHCenter);
        auto* label = Label(text, "JinraHomeDockLabel");
        label->setAlignment(Qt::AlignCenter);
        column->addWidget(label);
        dock->addLayout(column);
        return button;
    };
    auto* content = dock_button(11, tr("Install content"), "JinraHomeRound");
    connect(content, &QToolButton::clicked, this, &JinraHome::InstallContentRequested);
    home_save_data = dock_button(7, tr("Save data"), "JinraHomeRound");
    connect(home_save_data, &QToolButton::clicked, games, &GameList::OpenSelectedSaveData);
    home_game_settings = dock_button(9, tr("Game settings"), "JinraHomeRound");
    connect(home_game_settings, &QToolButton::clicked, this, [this] { properties->click(); });
    auto* controls = dock_button(2, tr("Controls"), "JinraHomeRound");
    connect(controls, &QToolButton::clicked, this, [this] { emit ConfigureRequested(5); });
    auto* system = dock_button(1, tr("System"), "JinraHomeRound");
    connect(system, &QToolButton::clicked, this, [this] { emit ConfigureRequested(1); });
    auto* graphics = dock_button(10, tr("Graphics"), "JinraHomeRound");
    connect(graphics, &QToolButton::clicked, this, [this] { emit ConfigureRequested(3); });
    auto* settings = dock_button(3, tr("Settings"), "JinraHomeRoundPrimary");
    connect(settings, &QToolButton::clicked, this, [this] { emit ConfigureRequested(0); });
    footer->addLayout(dock);

    auto* shortcuts = new QHBoxLayout;
    auto* count_left = Label({}, "JinraMuted");
    home_count = count_left;
    count_left->setMinimumWidth(100);
    shortcuts->addWidget(count_left, 1);
    home_options = new QPushButton(tr("Options"));
    auto* desktop_mode = new QPushButton(tr("Desktop mode"));
    home_play = new QPushButton(tr("Play"));
    for (auto* button : {home_options, desktop_mode, home_play}) {
        button->setObjectName(QStringLiteral("JinraHomePill"));
        button->setIconSize(QSize(23, 23));
        shortcuts->addWidget(button);
    }
    home_options->setIcon(HomeKeycap(QStringLiteral("+")));
    desktop_mode->setIcon(HomeKeycap(QStringLiteral("B")));
    home_play->setIcon(HomeKeycap(QStringLiteral("A")));
    home_play->setProperty("primary", true);
    home_play->setEnabled(false);
    home_options->setEnabled(false);
    home_save_data->setEnabled(false);
    home_game_settings->setEnabled(false);
    auto* spacer = new QWidget;
    spacer->setMinimumWidth(100);
    shortcuts->addWidget(spacer, 1);
    ConnectHomeSound(home_options, JinraHomeAudio::Cue::Confirm);
    connect(home_options, &QPushButton::clicked, games, &GameList::OpenSelectedGameMenu);
    connect(home_play, &QPushButton::clicked, games, &GameList::PlaySelectedGame);
    connect(desktop_mode, &QPushButton::clicked, this, [this] { SetHomeLayout(false); });
    footer->addLayout(shortcuts);
}

void JinraHome::ConnectHomeSound(QAbstractButton* button, JinraHomeAudio::Cue cue) {
    connect(button, &QAbstractButton::clicked, this, [this, cue] {
        if (IsHomeChromeActive())
            home_audio.Play(cue);
    });
}

void JinraHome::RefreshHomeProfile() {
    const auto profile = JinraAvatarDialog::ActiveProfile();
    QPixmap icon(80, 80);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath circle;
    circle.addEllipse(QRectF(0, 0, 80, 80));
    painter.setClipPath(circle);
    const auto size = profile.image.size().scaled(76, 76, Qt::KeepAspectRatio);
    QRect target(QPoint{}, size);
    target.moveCenter(QPoint{40, 40});
    painter.drawPixmap(target, profile.image, profile.image.rect());
    painter.end();
    icon.setDevicePixelRatio(2);
    home_profile->setIcon(QIcon(icon));
    home_profile->setIconSize(QSize(38, 38));
    home_profile->setToolTip(tr("Edit profile · %1").arg(profile.name));
    home_profile->setAccessibleName(home_profile->toolTip());
}

class JinraHomeSettings final : public QWidget {
    Q_DECLARE_TR_FUNCTIONS(JinraHomeSettings)

public:
    JinraHomeSettings(std::unique_ptr<ConfigurationPages> configuration, JinraHomeAudio& audio,
                      int first_category, std::function<void(bool)> finished, QWidget* parent)
        : QWidget(parent), settings(std::move(configuration)), home_audio(audio),
          original_volume(audio.Volume()), on_finished(std::move(finished)) {
        setObjectName(QStringLiteral("JinraHomeSettings"));
        settings->UseHomeLayout();
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(28, 20, 28, 20);
        outer->setSpacing(16);

        auto* header = new QHBoxLayout;
        back = new QPushButton;
        back->setObjectName(QStringLiteral("JinraHomePill"));
        back->setIcon(NavigationIcon(14));
        back->setIconSize(QSize(23, 23));
        header->addWidget(back);
        header->addStretch();
        title = Label({}, "JinraHeading");
        header->addWidget(title);
        header->addStretch();
        auto* logo = new QLabel;
        logo->setPixmap(QPixmap(QStringLiteral(":/jinra/icon.png")));
        logo->setScaledContents(true);
        logo->setFixedSize(40, 40);
        header->addWidget(logo);
        outer->addLayout(header);

        auto* body = new QHBoxLayout;
        body->setSpacing(18);
        auto* rail = new QFrame;
        rail->setObjectName(QStringLiteral("JinraSettingsRail"));
        rail->setFixedWidth(170);
        auto* rail_layout = new QVBoxLayout(rail);
        rail_layout->setContentsMargins(10, 10, 10, 10);
        rail_layout->setSpacing(7);
        auto* group = new QButtonGroup(this);

        auto* panel = new QFrame;
        panel->setObjectName(QStringLiteral("JinraSettingsPanel"));
        auto* panel_layout = new QVBoxLayout(panel);
        panel_layout->setContentsMargins(18, 16, 18, 12);
        panel_layout->setSpacing(12);
        section_title = Label({}, "JinraSettingsSection");
        panel_layout->addWidget(section_title);
        tabs = new QTabBar;
        tabs->setObjectName(QStringLiteral("JinraSettingsTabs"));
        tabs->setExpanding(false);
        tabs->setDrawBase(false);
        tabs->setUsesScrollButtons(true);
        panel_layout->addWidget(tabs);
        volume_row = CreateVolumeRow();
        panel_layout->addWidget(volume_row);
        category_pages = new QStackedWidget;
        category_pages->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        panel_layout->addWidget(category_pages, 1);

        const auto titles = ConfigurationPages::CategoryTitles();
        constexpr std::array<int, ConfigurationPages::CategoryCount> icons{3, 1, 12, 10, 13, 2};
        for (int category = 0; category < ConfigurationPages::CategoryCount; ++category) {
            auto* button = new QPushButton(NavigationIcon(icons[category]), titles[category]);
            button->setObjectName(QStringLiteral("JinraSettingsCategory"));
            button->setIconSize(QSize(24, 24));
            button->setCheckable(true);
            group->addButton(button, category);
            rail_layout->addWidget(button);
            category_buttons[category] = button;

            auto* stack = new QStackedWidget;
            auto* waiting = Label(tr("Loading settings…"), "JinraMuted");
            waiting->setAlignment(Qt::AlignCenter);
            stack->addWidget(waiting);
            subpages[category] = stack;
            category_pages->addWidget(stack);
        }
        rail_layout->addStretch();
        body->addWidget(rail);
        body->addWidget(panel, 1);
        outer->addLayout(body, 1);

        auto* footer = new QHBoxLayout;
        footer->addStretch();
        discard = new QPushButton;
        discard->setObjectName(QStringLiteral("JinraHomePill"));
        save = new QPushButton;
        save->setObjectName(QStringLiteral("JinraHomePill"));
        save->setProperty("primary", true);
        footer->addWidget(discard);
        footer->addWidget(save);
        outer->addLayout(footer);

        connect(group, &QButtonGroup::idClicked, this, [this](int category) {
            home_audio.Play(JinraHomeAudio::Cue::Navigate);
            SelectCategory(category);
        });
        connect(tabs, &QTabBar::currentChanged, this, [this](int index) {
            if (index >= 0) {
                subpages[current_category]->setCurrentIndex(index);
                settings->ResetDebugTab();
                home_audio.Play(JinraHomeAudio::Cue::Navigate);
            }
        });
        connect(back, &QPushButton::clicked, this, &JinraHomeSettings::Discard);
        connect(discard, &QPushButton::clicked, this, &JinraHomeSettings::Discard);
        connect(save, &QPushButton::clicked, this, [this] { Finish(true); });
        connect(settings.get(), &ConfigurationPages::ResetRequested, this, &JinraHomeSettings::Discard);
        Retranslate();
        SelectCategory(std::clamp(first_category, 0, ConfigurationPages::CategoryCount - 1));
        category_buttons[current_category]->setFocus(Qt::OtherFocusReason);
    }

    void Discard() {
        Finish(false);
    }

protected:
    void changeEvent(QEvent* event) override {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::LanguageChange) {
            QTimer::singleShot(0, this, [this] { Retranslate(); });
        }
    }

private:
    QWidget* CreateVolumeRow() {
        auto* row = new QFrame;
        row->setObjectName(QStringLiteral("JinraSettingsVolumeRow"));
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(14, 6, 14, 6);
        volume_label = Label({}, "JinraHomeDockLabel");
        layout->addWidget(volume_label);
        volume = new QSlider(Qt::Horizontal);
        volume->setObjectName(QStringLiteral("JinraHomeVolume"));
        volume->setRange(0, 100);
        volume->setValue(original_volume);
        layout->addWidget(volume, 1);
        volume_percent = Label({}, "JinraMuted");
        volume_percent->setMinimumWidth(40);
        volume_percent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(volume_percent);
        connect(volume, &QSlider::valueChanged, this, [this](int percent) {
            home_audio.SetVolume(percent, false);
            volume_percent->setText(tr("%1%").arg(percent));
        });
        connect(volume, &QSlider::sliderReleased, this, [this] {
            home_audio.Play(JinraHomeAudio::Cue::Notice);
        });
        return row;
    }

    void SelectCategory(int category) {
        current_category = category;
        category_buttons[category]->setChecked(true);
        category_pages->setCurrentIndex(category);
        section_title->setText(category_buttons[category]->text());
        volume_row->setVisible(category == 4);
        UpdateTabs();
        if (loaded_pages[category].isEmpty()) {
            QTimer::singleShot(16, this, [this, category] {
                if (current_category == category && on_finished && loaded_pages[category].isEmpty()) {
                    LoadCategory(category);
                }
            });
        }
    }

    void LoadCategory(int category) {
        auto* stack = subpages[category];
        auto* waiting = stack->widget(0);
        loaded_pages[category] = settings->LoadCategory(category);
        for (auto* page : loaded_pages[category]) {
            auto* scroll = new QScrollArea;
            scroll->setObjectName(QStringLiteral("JinraSettingsScroll"));
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setWidget(page);
            stack->addWidget(scroll);
        }
        stack->removeWidget(waiting);
        waiting->deleteLater();
        UpdateTabs();
    }

    void UpdateTabs() {
        const auto& pages = loaded_pages[current_category];
        const QSignalBlocker blocker(tabs);
        while (tabs->count() != 0) {
            tabs->removeTab(tabs->count() - 1);
        }
        for (auto* page : pages) {
            tabs->addTab(page->accessibleName());
        }
        tabs->setVisible(pages.size() > 1);
        tabs->setCurrentIndex(subpages[current_category]->currentIndex());
    }

    void Retranslate() {
        title->setText(tr("Settings"));
        back->setText(tr("Back to Home"));
        discard->setText(tr("Discard changes"));
        save->setText(tr("Save changes"));
        volume_label->setText(tr("Home sound volume"));
        volume->setAccessibleName(volume_label->text());
        volume_percent->setText(tr("%1%").arg(volume->value()));
        const auto titles = ConfigurationPages::CategoryTitles();
        for (int i = 0; i < ConfigurationPages::CategoryCount; ++i) {
            category_buttons[i]->setText(titles[i]);
        }
        if (current_category >= 0) {
            section_title->setText(titles[current_category]);
            const auto& pages = loaded_pages[current_category];
            for (int i = 0; i < tabs->count(); ++i) {
                tabs->setTabText(i, pages[i]->accessibleName());
            }
        }
    }

    void Finish(bool accepted) {
        if (!on_finished) {
            return;
        }
        if (accepted) {
            settings->ApplyConfiguration();
        }
        home_audio.SetVolume(accepted ? volume->value() : original_volume, accepted);
        home_audio.Play(accepted ? JinraHomeAudio::Cue::Confirm : JinraHomeAudio::Cue::Back);
        std::exchange(on_finished, {})(accepted);
    }

    std::unique_ptr<ConfigurationPages> settings;
    JinraHomeAudio& home_audio;
    const int original_volume;
    std::function<void(bool)> on_finished;
    std::array<QPushButton*, ConfigurationPages::CategoryCount> category_buttons{};
    std::array<QStackedWidget*, ConfigurationPages::CategoryCount> subpages{};
    std::array<QList<QWidget*>, ConfigurationPages::CategoryCount> loaded_pages{};
    QStackedWidget* category_pages{};
    QTabBar* tabs{};
    QPushButton* back{};
    QPushButton* discard{};
    QPushButton* save{};
    QLabel* title{};
    QLabel* section_title{};
    QWidget* volume_row{};
    QLabel* volume_label{};
    QLabel* volume_percent{};
    QSlider* volume{};
    int current_category{-1};
};

void JinraHome::ShowSettings(std::unique_ptr<ConfigurationPages> settings, int category,
                             std::function<void(bool)> finished) {
    home_settings = new JinraHomeSettings(
        std::move(settings), home_audio, category,
        [this, finished = std::move(finished)](bool accepted) {
            auto* view = std::exchange(home_settings, nullptr);
            pages->setCurrentIndex(0);
            pages->removeWidget(view);
            view->deleteLater();
            finished(accepted);
            games->FocusLibrary();
        },
        pages);
    pages->addWidget(home_settings);
    pages->setCurrentWidget(home_settings);
}

void JinraHome::SetHomeLayout(bool enabled) {
    if (home_layout != enabled && isVisible() && !emulating)
        home_audio.Play(enabled ? JinraHomeAudio::Cue::Confirm : JinraHomeAudio::Cue::Back);
    home_layout = enabled;
    QFile stylesheet(QStringLiteral(":/jinra/style.qss"));
    setStyleSheet(enabled && stylesheet.open(QIODevice::ReadOnly)
                      ? QString::fromUtf8(stylesheet.readAll()) : QString{});
    setAttribute(Qt::WA_StyledBackground, enabled);
    sidebar->setVisible(!enabled);
    desktop_header->setVisible(!enabled);
    home_header->setVisible(enabled);
    home_footer->setVisible(enabled);
    library_detail->setVisible(!enabled);
    library_status->setVisible(!enabled);
    home_search_button->setVisible(enabled);
    home_sort_button->setVisible(enabled);
    collection_row->setAlignment(enabled ? Qt::AlignHCenter : Qt::AlignLeft);
    if (enabled) {
        home_search_layout->addWidget(library_search);
        if (games->HasSearchFilter())
            home_search_button->setChecked(true);
        home_search->setVisible(home_search_button->isChecked());
    } else {
        desktop_search_layout->addWidget(library_search);
        library_search->setVisible(UISettings::values.show_filter_bar.GetValue());
        home_search->hide();
    }
    games->SetHomeMode(enabled);
    if (enabled)
        games->FocusLibrary();
    emit HomeChromeChanged(IsHomeChromeActive());
    RefreshControllers();
}

bool JinraHome::IsHomeChromeActive() const {
    return home_layout && !emulating &&
           (pages->currentIndex() == 0 || pages->currentWidget() == home_settings);
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
        if (home_settings) {
            home_settings->Discard();
        }
        controller_timer->stop();
        pages->setCurrentIndex(0);
        navigation[0]->setChecked(true);
    } else {
        controller_timer->start();
        ReloadSettings();
    }
    for (int i = 1; i < 4; ++i)
        navigation[i]->setEnabled(!running);
    emit HomeChromeChanged(IsHomeChromeActive());
}

void JinraHome::SelectGame(const QModelIndex& index) {
    const auto item = index.siblingAtColumn(0);
    const bool has_game = item.isValid() && !item.data(GameListItemPath::FullPathRole).toString().isEmpty();
    home_play->setEnabled(has_game);
    home_options->setEnabled(has_game);
    home_game_settings->setEnabled(has_game);
    home_save_data->setEnabled(has_game && item.data(GameListItemPath::ProgramIdRole).toULongLong() != 0);
    if (!has_game) {
        selected_game = QPersistentModelIndex();
        detail_title->setText(tr("Select a game"));
        detail_status->setText(tr("Choose a game from the library."));
        detail_image->clear();
        play->setEnabled(false);
        properties->setEnabled(false);
        return;
    }
    if (selected_game.isValid() && selected_game != item && isVisible() && IsHomeChromeActive())
        home_audio.Play(JinraHomeAudio::Cue::Navigate);
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
    if (home_layout)
        return;
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
    if (!controller_view || !isVisible() || emulating)
        return;
    const auto page = pages->currentIndex();
    const bool library_home = page == 0 && home_layout;
    if (page != 2 && !library_home)
        return;
    const auto devices = input->GetInputDevices();
    const Common::ParamPackage* active = nullptr;
    u64 newest{};
    int connected = 0;
    for (const auto& device : devices) {
        if (device.Get("engine", "") != "sdl" || device.Get("guid2", "") != "")
            continue;
        ++connected;
        const auto activity = QString::fromStdString(device.Get("last_active", "0")).toULongLong();
        if (!active || activity > newest) {
            active = &device;
            newest = activity;
        }
    }
    if (library_home) {
        home_clock->setText(QLocale().toString(QTime::currentTime(), QLocale::ShortFormat));
        home_controllers->setText(connected == 1 ? tr("1 controller")
                                                  : tr("%1 controllers").arg(connected));
        return;
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
