#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QProcess>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QDebug>
#include <QUuid>
#include <QtEndian>

#include <KGlobalAccel>
#include <KGlobalShortcutInfo>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <linux/input.h>
#include <functional>
#include <memory>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

namespace {
constexpr qint64 kDebounceMs = 150;
constexpr quint32 kMaxMessageBytes = 1024 * 1024;

bool isLogitechR400(const input_id &device, const QString &name) {
    return device.bustype == BUS_USB
        && device.vendor == 0x046d
        && device.product == 0xc538
        && name == QStringLiteral("Logitech USB Receiver");
}

bool isKeyboardInterface(const QByteArray &number) {
    return number.trimmed() == QByteArrayLiteral("00");
}

bool isKeyboardInterface(dev_t deviceNumber) {
    QString current = QFileInfo(QStringLiteral("/sys/dev/char/%1:%2/device")
        .arg(major(deviceNumber)).arg(minor(deviceNumber))).canonicalFilePath();
    while (!current.isEmpty() && current != QStringLiteral("/")) {
        QFile number(QDir(current).filePath(QStringLiteral("bInterfaceNumber")));
        if (number.open(QIODevice::ReadOnly)) {
            return isKeyboardInterface(number.readAll());
        }
        current = QFileInfo(current).dir().absolutePath();
    }
    return false;
}

bool hasLogitechR400Keys(const unsigned long *bits, std::size_t count) {
    constexpr std::size_t wordBits = sizeof(unsigned long) * 8;
    const auto present = [=](unsigned int key) {
        return key / wordBits < count
            && (bits[key / wordBits] & (1UL << (key % wordBits))) != 0;
    };
    return present(KEY_PAGEUP) && present(KEY_PAGEDOWN);
}

bool isIkkegolFootPedal(const input_id &device) {
    return device.bustype == BUS_USB
        && ((device.vendor == 0x1a86 && device.product == 0xe026)
            || (device.vendor == 0x3553 && device.product == 0xb001));
}

bool hasIkkegolFootPedalKeys(const unsigned long *bits, std::size_t count) {
    constexpr std::size_t wordBits = sizeof(unsigned long) * 8;
    const auto present = [=](unsigned int key) {
        return key / wordBits < count
            && (bits[key / wordBits] & (1UL << (key % wordBits))) != 0;
    };
    const bool abc = present(KEY_A) && present(KEY_B) && present(KEY_C);
    const bool functionKeys = present(KEY_F20) && present(KEY_F21) && present(KEY_F22);
    return abc || functionKeys;
}

enum class HardwareProfile {
    none,
    logitechR400,
    ikkegolFootPedal,
};

HardwareProfile hardwareProfile(const input_id &device, const QString &name) {
    if (isLogitechR400(device, name)) {
        return HardwareProfile::logitechR400;
    }
    if (isIkkegolFootPedal(device)) {
        return HardwareProfile::ikkegolFootPedal;
    }
    return HardwareProfile::none;
}

bool hasProfileKeys(
    HardwareProfile profile, const unsigned long *bits, std::size_t count) {
    if (profile == HardwareProfile::logitechR400) {
        return hasLogitechR400Keys(bits, count);
    }
    if (profile == HardwareProfile::ikkegolFootPedal) {
        return hasIkkegolFootPedalKeys(bits, count);
    }
    return false;
}

QString decodeEvdevName(const char *bytes, int length) {
    while (length > 0 && bytes[length - 1] == '\0') {
        --length;
    }
    return QString::fromLocal8Bit(bytes, length);
}

QString logitechR400Action(
    const input_id &device,
    const QString &name,
    const input_event &event) {
    if (!isLogitechR400(device, name) || event.type != EV_KEY || event.value != 1) {
        return {};
    }
    if (event.code == KEY_PAGEUP) {
        return QStringLiteral("auction");
    }
    if (event.code == KEY_PAGEDOWN) {
        return QStringLiteral("giveaway");
    }
    return {};
}

QString ikkegolFootPedalAction(
    const input_id &device,
    const input_event &event) {
    if (!isIkkegolFootPedal(device) || event.type != EV_KEY || event.value != 1) {
        return {};
    }
    if (event.code == KEY_A || event.code == KEY_F20) {
        return QStringLiteral("auction");
    }
    if (event.code == KEY_B || event.code == KEY_F22) {
        return QStringLiteral("inspect");
    }
    if (event.code == KEY_C || event.code == KEY_F21) {
        return QStringLiteral("giveaway");
    }
    return {};
}

QString hardwareAction(
    HardwareProfile profile,
    const input_id &device,
    const QString &name,
    const input_event &event) {
    if (profile == HardwareProfile::logitechR400) {
        return logitechR400Action(device, name, event);
    }
    if (profile == HardwareProfile::ikkegolFootPedal) {
        return ikkegolFootPedalAction(device, event);
    }
    return {};
}

enum class HardwareReadResult {
    drained,
    disconnected,
    invalid,
};

HardwareReadResult readHardwareEvents(
    int descriptor,
    HardwareProfile profile,
    const input_id &device,
    const QString &name,
    const std::function<void(const QString &)> &onAction) {
    std::array<input_event, 16> events{};
    while (true) {
        const ssize_t count = ::read(descriptor, events.data(), sizeof(events));
        if (count > 0) {
            if (count % static_cast<ssize_t>(sizeof(input_event)) != 0) {
                return HardwareReadResult::invalid;
            }
            const std::size_t eventCount = static_cast<std::size_t>(count)
                / sizeof(input_event);
            for (std::size_t index = 0; index < eventCount; ++index) {
                const QString action = hardwareAction(
                    profile, device, name, events[index]);
                if (!action.isEmpty()) {
                    onAction(action);
                }
            }
            continue;
        }
        if (count == 0) {
            return HardwareReadResult::disconnected;
        }
        if (errno == EINTR) {
            continue;
        }
        return errno == EAGAIN || errno == EWOULDBLOCK
            ? HardwareReadResult::drained
            : HardwareReadResult::disconnected;
    }
}

int runLogitechR400ProfileSelfTest() {
    const input_id r400{BUS_USB, 0x046d, 0xc538, 0x0111};
    const input_id other{BUS_USB, 0x046d, 0xc539, 0x0111};
    const QString name = QStringLiteral("Logitech USB Receiver");
    const auto action = [&](const input_id &device, quint16 code, qint32 value) {
        input_event event{};
        event.type = EV_KEY;
        event.code = code;
        event.value = value;
        return logitechR400Action(device, name, event);
    };
    const QString previous = action(r400, KEY_PAGEUP, 1);
    const QString next = action(r400, KEY_PAGEDOWN, 1);
    constexpr char kernelName[] = "Logitech USB Receiver";
    const bool kernelNameMatched = isLogitechR400(
        r400, decodeEvdevName(kernelName, sizeof(kernelName)));
    const bool interfaceMatched = isKeyboardInterface("00\n");
    const bool otherInterfaceIgnored = !isKeyboardInterface("01\n");
    std::array<unsigned long, (KEY_MAX / (sizeof(unsigned long) * 8)) + 1> keyBits{};
    constexpr std::size_t wordBits = sizeof(unsigned long) * 8;
    keyBits[KEY_PAGEUP / wordBits] |= 1UL << (KEY_PAGEUP % wordBits);
    keyBits[KEY_PAGEDOWN / wordBits] |= 1UL << (KEY_PAGEDOWN % wordBits);
    const bool capabilitiesMatched = hasLogitechR400Keys(keyBits.data(), keyBits.size());
    const bool matchedDevice = previous == QStringLiteral("auction")
        && next == QStringLiteral("giveaway");
    const bool otherDeviceIgnored = action(other, KEY_PAGEUP, 1).isEmpty();
    const bool otherKeyIgnored = action(r400, KEY_F5, 1).isEmpty();
    const bool releaseIgnored = action(r400, KEY_PAGEUP, 0).isEmpty();
    const bool repeatIgnored = action(r400, KEY_PAGEUP, 2).isEmpty();
    const QJsonObject result{
        {QStringLiteral("matchedDevice"), matchedDevice},
        {QStringLiteral("kernelNameMatched"), kernelNameMatched},
        {QStringLiteral("interfaceMatched"), interfaceMatched},
        {QStringLiteral("otherInterfaceIgnored"), otherInterfaceIgnored},
        {QStringLiteral("capabilitiesMatched"), capabilitiesMatched},
        {QStringLiteral("previous"), previous},
        {QStringLiteral("next"), next},
        {QStringLiteral("otherDeviceIgnored"), otherDeviceIgnored},
        {QStringLiteral("otherKeyIgnored"), otherKeyIgnored},
        {QStringLiteral("releaseIgnored"), releaseIgnored},
        {QStringLiteral("repeatIgnored"), repeatIgnored},
    };
    const QByteArray output = QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(output.constData(), 1, output.size(), stdout);
    return matchedDevice && kernelNameMatched && otherDeviceIgnored && otherKeyIgnored
            && releaseIgnored && repeatIgnored
        ? 0
        : 1;
}

int runIkkegolFootPedalProfileSelfTest() {
    const input_id first{BUS_USB, 0x1a86, 0xe026, 0x0111};
    const input_id second{BUS_USB, 0x3553, 0xb001, 0x0111};
    const input_id other{BUS_USB, 0x1a86, 0xe027, 0x0111};
    const auto action = [](const input_id &device, quint16 code, qint32 value) {
        input_event event{};
        event.type = EV_KEY;
        event.code = code;
        event.value = value;
        return ikkegolFootPedalAction(device, event);
    };
    std::array<unsigned long, (KEY_MAX / (sizeof(unsigned long) * 8)) + 1> defaultBits{};
    std::array<unsigned long, (KEY_MAX / (sizeof(unsigned long) * 8)) + 1> optionalBits{};
    constexpr std::size_t wordBits = sizeof(unsigned long) * 8;
    for (const unsigned int key : {KEY_A, KEY_B, KEY_C}) {
        defaultBits[key / wordBits] |= 1UL << (key % wordBits);
    }
    for (const unsigned int key : {KEY_F20, KEY_F21, KEY_F22}) {
        optionalBits[key / wordBits] |= 1UL << (key % wordBits);
    }
    const QString defaultLeft = action(first, KEY_A, 1);
    const QString defaultMiddle = action(first, KEY_B, 1);
    const QString defaultRight = action(first, KEY_C, 1);
    const QString optionalLeft = action(first, KEY_F20, 1);
    const QString optionalMiddle = action(first, KEY_F22, 1);
    const QString optionalRight = action(first, KEY_F21, 1);
    const bool firstIdMatched = isIkkegolFootPedal(first);
    const bool secondIdMatched = isIkkegolFootPedal(second);
    const bool interfaceMatched = isKeyboardInterface("00\n");
    const bool otherInterfaceIgnored = !isKeyboardInterface("01\n");
    const bool defaultCapabilitiesMatched = hasIkkegolFootPedalKeys(
        defaultBits.data(), defaultBits.size());
    const bool optionalCapabilitiesMatched = hasIkkegolFootPedalKeys(
        optionalBits.data(), optionalBits.size());
    const bool otherDeviceIgnored = action(other, KEY_A, 1).isEmpty();
    const bool otherKeyIgnored = action(first, KEY_F19, 1).isEmpty();
    const bool releaseIgnored = action(first, KEY_A, 0).isEmpty();
    const bool repeatIgnored = action(first, KEY_A, 2).isEmpty();
    const QJsonObject result{
        {QStringLiteral("firstIdMatched"), firstIdMatched},
        {QStringLiteral("secondIdMatched"), secondIdMatched},
        {QStringLiteral("interfaceMatched"), interfaceMatched},
        {QStringLiteral("otherInterfaceIgnored"), otherInterfaceIgnored},
        {QStringLiteral("defaultCapabilitiesMatched"), defaultCapabilitiesMatched},
        {QStringLiteral("optionalCapabilitiesMatched"), optionalCapabilitiesMatched},
        {QStringLiteral("defaultLeft"), defaultLeft},
        {QStringLiteral("defaultMiddle"), defaultMiddle},
        {QStringLiteral("defaultRight"), defaultRight},
        {QStringLiteral("optionalLeft"), optionalLeft},
        {QStringLiteral("optionalMiddle"), optionalMiddle},
        {QStringLiteral("optionalRight"), optionalRight},
        {QStringLiteral("otherDeviceIgnored"), otherDeviceIgnored},
        {QStringLiteral("otherKeyIgnored"), otherKeyIgnored},
        {QStringLiteral("releaseIgnored"), releaseIgnored},
        {QStringLiteral("repeatIgnored"), repeatIgnored},
    };
    const QByteArray output = QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(output.constData(), 1, output.size(), stdout);
    return firstIdMatched && secondIdMatched && interfaceMatched
            && otherInterfaceIgnored && defaultCapabilitiesMatched
            && optionalCapabilitiesMatched
            && defaultLeft == QStringLiteral("auction")
            && defaultMiddle == QStringLiteral("inspect")
            && defaultRight == QStringLiteral("giveaway")
            && optionalLeft == QStringLiteral("auction")
            && optionalMiddle == QStringLiteral("inspect")
            && optionalRight == QStringLiteral("giveaway")
            && otherDeviceIgnored && otherKeyIgnored && releaseIgnored && repeatIgnored
        ? 0
        : 1;
}

class HardwareOwnerLease final {
public:
    explicit HardwareOwnerLease(const QByteArray &path) {
        descriptor_ = ::open(
            path.constData(),
            O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW,
            S_IRUSR | S_IWUSR);
        if (descriptor_ < 0) {
            return;
        }
        struct stat metadata{};
        if (::fstat(descriptor_, &metadata) != 0
            || !S_ISREG(metadata.st_mode)
            || metadata.st_uid != ::getuid()
            || (metadata.st_mode & 0777) != (S_IRUSR | S_IWUSR)) {
            ::close(descriptor_);
            descriptor_ = -1;
            return;
        }
        ownerOnly_ = true;
        if (::flock(descriptor_, LOCK_EX | LOCK_NB) != 0) {
            ::close(descriptor_);
            descriptor_ = -1;
        }
    }

    ~HardwareOwnerLease() {
        if (descriptor_ >= 0) {
            ::flock(descriptor_, LOCK_UN);
            ::close(descriptor_);
        }
    }

    HardwareOwnerLease(const HardwareOwnerLease &) = delete;
    HardwareOwnerLease &operator=(const HardwareOwnerLease &) = delete;

    [[nodiscard]] bool acquired() const {
        return descriptor_ >= 0;
    }

    [[nodiscard]] bool ownerOnly() const {
        return ownerOnly_;
    }

private:
    int descriptor_ = -1;
    bool ownerOnly_ = false;
};

int runLogitechR400OwnerSelfTest() {
    QTemporaryDir directory;
    if (!directory.isValid()) {
        return 2;
    }
    const QByteArray path = QFile::encodeName(directory.filePath(
        QStringLiteral("r400-owner.lock")));
    auto first = std::make_unique<HardwareOwnerLease>(path);
    HardwareOwnerLease second(path);
    const bool exclusive = first->acquired() && !second.acquired();
    const bool ownerOnly = first->ownerOnly();
    first.reset();
    HardwareOwnerLease third(path);
    const bool takeover = third.acquired();
    const QJsonObject result{
        {QStringLiteral("exclusive"), exclusive},
        {QStringLiteral("takeover"), takeover},
        {QStringLiteral("ownerOnly"), ownerOnly},
    };
    const QByteArray output = QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(output.constData(), 1, output.size(), stdout);
    return exclusive && takeover && ownerOnly ? 0 : 1;
}

int runLogitechR400StreamSelfTest() {
    int descriptors[2] = {-1, -1};
    if (::pipe2(descriptors, O_CLOEXEC | O_NONBLOCK) != 0) {
        return 2;
    }
    const input_id r400{BUS_USB, 0x046d, 0xc538, 0x0111};
    const QString name = QStringLiteral("Logitech USB Receiver");
    std::array<input_event, 6> events{};
    events[0] = input_event{{}, EV_KEY, KEY_PAGEUP, 0};
    events[1] = input_event{{}, EV_KEY, KEY_PAGEUP, 2};
    events[2] = input_event{{}, EV_KEY, KEY_F5, 1};
    events[3] = input_event{{}, EV_SYN, SYN_REPORT, 0};
    events[4] = input_event{{}, EV_KEY, KEY_PAGEUP, 1};
    events[5] = input_event{{}, EV_KEY, KEY_PAGEDOWN, 1};
    const ssize_t expected = static_cast<ssize_t>(sizeof(events));
    const ssize_t written = ::write(descriptors[1], events.data(), sizeof(events));
    QJsonArray actions;
    const HardwareReadResult readResult = readHardwareEvents(
        descriptors[0], HardwareProfile::logitechR400, r400, name,
        [&actions](const QString &action) {
            actions.append(action);
        });
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    const bool completeFrames = written == expected
        && readResult == HardwareReadResult::drained;
    const bool expectedActions = actions == QJsonArray{
        QStringLiteral("auction"), QStringLiteral("giveaway")};
    const QJsonObject result{
        {QStringLiteral("actions"), actions},
        {QStringLiteral("completeFrames"), completeFrames},
    };
    const QByteArray output = QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(output.constData(), 1, output.size(), stdout);
    return completeFrames && expectedActions ? 0 : 1;
}

int runIkkegolFootPedalStreamSelfTest() {
    int descriptors[2] = {-1, -1};
    if (::pipe2(descriptors, O_CLOEXEC | O_NONBLOCK) != 0) {
        return 2;
    }
    const input_id pedal{BUS_USB, 0x3553, 0xb001, 0x0111};
    std::array<input_event, 9> events{};
    events[0] = input_event{{}, EV_KEY, KEY_A, 0};
    events[1] = input_event{{}, EV_KEY, KEY_A, 2};
    events[2] = input_event{{}, EV_KEY, KEY_F19, 1};
    events[3] = input_event{{}, EV_SYN, SYN_REPORT, 0};
    events[4] = input_event{{}, EV_KEY, KEY_A, 1};
    events[5] = input_event{{}, EV_KEY, KEY_B, 1};
    events[6] = input_event{{}, EV_KEY, KEY_C, 1};
    events[7] = input_event{{}, EV_KEY, KEY_F20, 1};
    events[8] = input_event{{}, EV_KEY, KEY_F22, 1};
    const ssize_t expected = static_cast<ssize_t>(sizeof(events));
    const ssize_t written = ::write(descriptors[1], events.data(), sizeof(events));
    QJsonArray actions;
    const HardwareReadResult readResult = readHardwareEvents(
        descriptors[0], HardwareProfile::ikkegolFootPedal, pedal, {},
        [&actions](const QString &action) { actions.append(action); });
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    const bool completeFrames = written == expected
        && readResult == HardwareReadResult::drained;
    const bool expectedActions = actions == QJsonArray{
        QStringLiteral("auction"), QStringLiteral("inspect"),
        QStringLiteral("giveaway"), QStringLiteral("auction"),
        QStringLiteral("inspect")};
    const QJsonObject result{
        {QStringLiteral("actions"), actions},
        {QStringLiteral("completeFrames"), completeFrames},
    };
    const QByteArray output = QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(output.constData(), 1, output.size(), stdout);
    return completeFrames && expectedActions ? 0 : 1;
}

bool isExpectedOwner(const KGlobalShortcutInfo &shortcut, const QKeySequence &sequence) {
    const QString component = shortcut.componentUniqueName();
    const QString action = shortcut.uniqueName();
    if (component == QStringLiteral("quick-swap-tools")) {
        return (sequence == QKeySequence(Qt::META | Qt::Key_A)
                && action == QStringLiteral("next-auction"))
            || (sequence == QKeySequence(Qt::META | Qt::Key_G)
                && action == QStringLiteral("next-giveaway"));
    }
    if (sequence == QKeySequence(Qt::META | Qt::Key_A)) {
        return component == QStringLiteral("plasmashell")
            && action == QStringLiteral("next activity");
    }
    if (sequence == QKeySequence(Qt::META | Qt::Key_G)) {
        return component == QStringLiteral("kwin")
            && action == QStringLiteral("Grid View");
    }
    return false;
}

bool shortcutsCanBeClaimed() {
    const std::array<QKeySequence, 2> sequences = {
        QKeySequence(Qt::META | Qt::Key_A),
        QKeySequence(Qt::META | Qt::Key_G),
    };
    for (const QKeySequence &sequence : sequences) {
        const auto shortcuts = KGlobalAccel::globalShortcutsByKey(sequence);
        for (const KGlobalShortcutInfo &shortcut : shortcuts) {
            if (isExpectedOwner(shortcut, sequence)) {
                continue;
            }
            qCritical().noquote()
                << QStringLiteral("Refusing to replace %1: already used by %2 / %3")
                       .arg(sequence.toString(QKeySequence::NativeText),
                            shortcut.componentFriendlyName(),
                            shortcut.friendlyName());
            return false;
        }
    }
    return true;
}

struct HardwareInput {
    HardwareProfile profile = HardwareProfile::none;
    int descriptor = -1;
    input_id device{};
    QString name;
    QString path;
    std::unique_ptr<QSocketNotifier> notifier;
};

class NativeHost final : public QObject {
public:
    explicit NativeHost(bool forceRegistration, QObject *parent = nullptr)
        : QObject(parent),
          stdinNotifier_(STDIN_FILENO, QSocketNotifier::Read, this),
          forceRegistration_(forceRegistration) {
        auctionAction_.setObjectName(QStringLiteral("next-auction"));
        auctionAction_.setText(QStringLiteral("Start next Whatnot auction"));
        giveawayAction_.setObjectName(QStringLiteral("next-giveaway"));
        giveawayAction_.setText(QStringLiteral("Start next Whatnot giveaway"));
        inspectAction_.setObjectName(QStringLiteral("inspect-controls"));
        inspectAction_.setText(QStringLiteral("Inspect Whatnot controls (no click)"));

        if (forceRegistration_) {
            auctionRegistered_ = registerShortcut(
                &auctionAction_, QKeySequence(Qt::META | Qt::Key_A));
            giveawayRegistered_ = registerShortcut(
                &giveawayAction_, QKeySequence(Qt::META | Qt::Key_G));
            KGlobalAccel::self()->setShortcut(&inspectAction_, {});
        }

        connect(&auctionAction_, &QAction::triggered, this, [this] {
            trigger(QStringLiteral("auction"), auctionLastRun_);
        });
        connect(&giveawayAction_, &QAction::triggered, this, [this] {
            trigger(QStringLiteral("giveaway"), giveawayLastRun_);
        });
        connect(&inspectAction_, &QAction::triggered, this, [this] {
            writeCommand(QStringLiteral("inspect"));
        });
        connect(&stdinNotifier_, &QSocketNotifier::activated, this, [this] {
            readStdin();
        });
        if (!forceRegistration_) {
            hardwareRetryTimer_.setInterval(1500);
            connect(&hardwareRetryTimer_, &QTimer::timeout, this, [this] {
                ensureHardwareInput();
            });
            ensureHardwareInput();
            hardwareRetryTimer_.start();
        }
    }

    ~NativeHost() override {
        closeHardwareInput(HardwareProfile::logitechR400);
        closeHardwareInput(HardwareProfile::ikkegolFootPedal);
    }

    bool shortcutsRegistered() const {
        return auctionRegistered_ && giveawayRegistered_;
    }

private:
    std::unique_ptr<HardwareInput> &inputSlot(HardwareProfile profile) {
        return profile == HardwareProfile::logitechR400 ? r400Input_ : pedalInput_;
    }

    void ensureHardwareInput() {
        if (!hardwareOwner_) {
            const QString runtimeDirectory = QStandardPaths::writableLocation(
                QStandardPaths::RuntimeLocation);
            if (runtimeDirectory.isEmpty()) {
                return;
            }
            const QByteArray lockPath = QFile::encodeName(
                QDir(runtimeDirectory).filePath(
                    QStringLiteral("quick-swap-tools-input-owner.lock")));
            auto owner = std::make_unique<HardwareOwnerLease>(lockPath);
            if (!owner->acquired()) {
                return;
            }
            hardwareOwner_ = std::move(owner);
        }
        if (!auctionRegistered_) {
            auctionRegistered_ = registerShortcut(
                &auctionAction_, QKeySequence(Qt::META | Qt::Key_A));
        }
        if (!giveawayRegistered_) {
            giveawayRegistered_ = registerShortcut(
                &giveawayAction_, QKeySequence(Qt::META | Qt::Key_G));
        }
        KGlobalAccel::self()->setShortcut(&inspectAction_, {});
        if (qEnvironmentVariableIsSet("QUICK_SWAP_NO_HARDWARE_INPUT")) {
            return;
        }

        QDir inputDirectory(QStringLiteral("/dev/input"));
        const QStringList candidates = inputDirectory.entryList(
            {QStringLiteral("event*")}, QDir::System | QDir::Files, QDir::Name);
        for (const QString &candidate : candidates) {
            const QString candidatePath = inputDirectory.filePath(candidate);
            if ((r400Input_ && r400Input_->path == candidatePath)
                || (pedalInput_ && pedalInput_->path == candidatePath)) {
                continue;
            }
            const QByteArray path = QFile::encodeName(candidatePath);
            const int descriptor = ::open(
                path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
            if (descriptor < 0) {
                continue;
            }
            struct stat metadata{};
            input_id device{};
            std::array<char, 256> nameBytes{};
            std::array<unsigned long, (KEY_MAX / (sizeof(unsigned long) * 8)) + 1> keyBits{};
            const int nameLength = ::ioctl(
                descriptor, EVIOCGNAME(nameBytes.size()), nameBytes.data());
            if (::fstat(descriptor, &metadata) != 0
                || !S_ISCHR(metadata.st_mode)
                || ::ioctl(descriptor, EVIOCGID, &device) != 0
                || ::ioctl(descriptor, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits.data()) < 0
                || nameLength <= 0) {
                ::close(descriptor);
                continue;
            }
            const QString name = decodeEvdevName(
                nameBytes.data(), std::min<int>(nameLength, nameBytes.size()));
            const HardwareProfile profile = hardwareProfile(device, name);
            if (profile == HardwareProfile::none
                || (profile == HardwareProfile::logitechR400
                    && qEnvironmentVariableIsSet("QUICK_SWAP_NO_R400"))
                || inputSlot(profile)
                || !isKeyboardInterface(metadata.st_rdev)
                || !hasProfileKeys(profile, keyBits.data(), keyBits.size())) {
                ::close(descriptor);
                continue;
            }
            auto input = std::make_unique<HardwareInput>();
            input->profile = profile;
            input->descriptor = descriptor;
            input->device = device;
            input->name = name;
            input->path = candidatePath;
            HardwareInput *observed = input.get();
            input->notifier = std::make_unique<QSocketNotifier>(
                descriptor, QSocketNotifier::Read);
            connect(input->notifier.get(), &QSocketNotifier::activated, this,
                [this, observed] { handleHardwareInput(observed); });
            inputSlot(profile) = std::move(input);
        }
    }

    void handleHardwareInput(HardwareInput *input) {
        const HardwareProfile profile = input->profile;
        const HardwareReadResult result = readHardwareEvents(
            input->descriptor, profile, input->device, input->name,
            [this](const QString &action) {
                if (action == QStringLiteral("auction")) {
                    trigger(action, auctionLastRun_);
                } else if (action == QStringLiteral("giveaway")) {
                    trigger(action, giveawayLastRun_);
                } else if (action == QStringLiteral("inspect")) {
                    trigger(action, inspectLastRun_);
                }
            });
        if (result != HardwareReadResult::drained) {
            closeHardwareInput(profile);
        }
    }

    void closeHardwareInput(HardwareProfile profile) {
        auto &input = inputSlot(profile);
        if (!input) {
            return;
        }
        input->notifier.reset();
        if (input->descriptor >= 0) {
            ::close(input->descriptor);
        }
        input.reset();
    }

    bool registerShortcut(QAction *action, const QKeySequence &sequence) {
        if (qEnvironmentVariableIsSet("QUICK_SWAP_NO_SHORTCUTS")) {
            return false;
        }
        if (forceRegistration_) {
            KGlobalAccel::stealShortcutSystemwide(sequence);
            KGlobalAccel::self()->setDefaultShortcut(
                action, {sequence}, KGlobalAccel::NoAutoloading);
            return KGlobalAccel::self()->setShortcut(
                action, {sequence}, KGlobalAccel::NoAutoloading);
        }
        KGlobalAccel::self()->setDefaultShortcut(action, {sequence});
        return KGlobalAccel::self()->setShortcut(action, {sequence});
    }

    void trigger(const QString &action, QElapsedTimer &timer) {
        if (timer.isValid() && timer.elapsed() < kDebounceMs) {
            return;
        }
        timer.restart();
        writeCommand(action);
    }

    void writeCommand(const QString &action) {
        writeMessage({
            {QStringLiteral("type"), QStringLiteral("command")},
            {QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {QStringLiteral("action"), action},
        });
    }

    void readStdin() {
        std::array<char, 65536> chunk{};
        const ssize_t count = ::read(STDIN_FILENO, chunk.data(), chunk.size());
        if (count <= 0) {
            QCoreApplication::quit();
            return;
        }
        inputBuffer_.append(chunk.data(), static_cast<qsizetype>(count));
        processFrames();
    }

    void processFrames() {
        while (inputBuffer_.size() >= 4) {
            const auto *bytes = reinterpret_cast<const uchar *>(inputBuffer_.constData());
            const quint32 length = qFromLittleEndian<quint32>(bytes);
            if (length > kMaxMessageBytes) {
                QCoreApplication::exit(2);
                return;
            }
            if (inputBuffer_.size() < static_cast<qsizetype>(4 + length)) {
                return;
            }
            const QByteArray payload = inputBuffer_.mid(4, length);
            inputBuffer_.remove(0, 4 + length);
            const QJsonDocument document = QJsonDocument::fromJson(payload);
            if (!document.isObject()) {
                continue;
            }
            handleMessage(document.object());
        }
    }

    void handleMessage(const QJsonObject &message) {
        const QString type = message.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("hello")) {
            writeMessage({
                {QStringLiteral("type"), QStringLiteral("ready")},
                {QStringLiteral("auctionShortcut"), auctionRegistered_},
                {QStringLiteral("giveawayShortcut"), giveawayRegistered_},
                {QStringLiteral("logitechR400"), r400Input_ != nullptr},
                {QStringLiteral("ikkegolFootPedal"), pedalInput_ != nullptr},
            });
            return;
        }
        if (type == QStringLiteral("result") && !message.value(QStringLiteral("ok")).toBool()) {
            const QString error = message.value(QStringLiteral("error")).toString(
                QStringLiteral("Unknown Whatnot control error"));
            QProcess::startDetached(
                QStringLiteral("notify-send"),
                {QStringLiteral("--urgency"), QStringLiteral("critical"),
                 QStringLiteral("Quick Swap failed"), error});
        }
        if (type == QStringLiteral("result")) {
            QJsonObject persistedMessage{
                {QStringLiteral("type"), message.value(QStringLiteral("type"))},
                {QStringLiteral("id"), message.value(QStringLiteral("id"))},
                {QStringLiteral("action"), message.value(QStringLiteral("action"))},
                {QStringLiteral("ok"), message.value(QStringLiteral("ok"))},
            };
            if (message.contains(QStringLiteral("error"))) {
                QString error = message.value(QStringLiteral("error")).toString();
                error.replace(
                    QRegularExpression(QStringLiteral(R"(https?://\S+)")),
                    QStringLiteral("[redacted-url]"));
                persistedMessage.insert(QStringLiteral("error"), error);
            }
            const QString stateRoot = qEnvironmentVariable(
                "XDG_STATE_HOME", QDir::homePath() + QStringLiteral("/.local/state"));
            const QString stateDirectory = stateRoot + QStringLiteral("/quick-swap-tools");
            QDir().mkpath(stateDirectory);
            QFile resultFile(stateDirectory + QStringLiteral("/last-result.json"));
            if (resultFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                resultFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
                resultFile.write(QJsonDocument(persistedMessage).toJson(QJsonDocument::Indented));
            }
        }
    }

    void writeMessage(const QJsonObject &message) {
        const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
        const quint32 littleEndianLength = qToLittleEndian<quint32>(payload.size());
        std::fwrite(&littleEndianLength, sizeof(littleEndianLength), 1, stdout);
        std::fwrite(payload.constData(), 1, payload.size(), stdout);
        std::fflush(stdout);
    }

    // Declared first so it is destroyed last, after QAction unregisters inputs.
    std::unique_ptr<HardwareOwnerLease> hardwareOwner_;
    QAction auctionAction_;
    QAction giveawayAction_;
    QAction inspectAction_;
    QSocketNotifier stdinNotifier_;
    QTimer hardwareRetryTimer_;
    std::unique_ptr<HardwareInput> r400Input_;
    std::unique_ptr<HardwareInput> pedalInput_;
    QByteArray inputBuffer_;
    QElapsedTimer auctionLastRun_;
    QElapsedTimer giveawayLastRun_;
    QElapsedTimer inspectLastRun_;
    bool forceRegistration_ = false;
    bool auctionRegistered_ = false;
    bool giveawayRegistered_ = false;
};
} // namespace

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("quick-swap-tools"));
    QCoreApplication::setOrganizationName(QStringLiteral("OniByts"));
    QGuiApplication::setDesktopFileName(QStringLiteral("quick-swap-tools"));

    const bool checkOnly = app.arguments().contains(QStringLiteral("--check-shortcuts"));
    const bool registerOnly = app.arguments().contains(QStringLiteral("--register-only"));
    if (app.arguments().contains(QStringLiteral("--r400-profile-self-test"))) {
        return runLogitechR400ProfileSelfTest();
    }
    if (app.arguments().contains(QStringLiteral("--r400-owner-self-test"))) {
        return runLogitechR400OwnerSelfTest();
    }
    if (app.arguments().contains(QStringLiteral("--r400-stream-self-test"))) {
        return runLogitechR400StreamSelfTest();
    }
    if (app.arguments().contains(QStringLiteral("--ikkegol-profile-self-test"))) {
        return runIkkegolFootPedalProfileSelfTest();
    }
    if (app.arguments().contains(QStringLiteral("--ikkegol-stream-self-test"))) {
        return runIkkegolFootPedalStreamSelfTest();
    }
    if ((checkOnly || registerOnly) && !shortcutsCanBeClaimed()) {
        return 2;
    }
    if (checkOnly) {
        return 0;
    }
    NativeHost host(registerOnly);
    if (registerOnly) {
        return host.shortcutsRegistered() ? 0 : 1;
    }
    return app.exec();
}
