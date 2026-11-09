// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <utility>

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLockFile>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryFile>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrentRun>
#include <zlib.h>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#endif

#include "common/fs/path_util.h"
#include "common/scope_exit.h"
#include "video_core/pipeline_cache_files.h"

#include "yuzu/game/shader_sync.h"

namespace {

constexpr qint64 MiB = 1024 * 1024;
constexpr u32 CacheVersion = VideoCommon::VulkanPipelineCacheVersion;
constexpr auto Magic = "yuzucach";
constexpr auto Feed = "https://github.com/JinWooPT/jinra-shader-sync/releases/download/";
constexpr auto Catalog = "shader-catalog-v54";
constexpr qint64 CopyBuffer = 128 * 1024;

struct SyncError {
    QString message;
};
struct Canceled {};
using Report = std::function<void(QString)>;

QString Tr(const char* text) {
    return QCoreApplication::translate("ShaderSync", text);
}

void Require(bool condition, const QString& message) {
    if (!condition) {
        throw SyncError{message};
    }
}

void CheckCanceled(const std::atomic_bool& canceled) {
    if (canceled) {
        throw Canceled{};
    }
}

QString CacheName() {
    return QStringLiteral("vulkan.fork-v%1.bin").arg(CacheVersion);
}

struct Chunk {
    QString file;
    QString tag;
    QByteArray digest;
    qint64 size{};
    qint64 unpacked_size{};
    QList<QByteArray> ids;

    bool IsArchive() const {
        return file.endsWith(QStringLiteral(".7z"));
    }
};

struct Plan {
    bool available{};
    qsizetype missing{};
    qint64 download_size{};
    QList<Chunk> chunks;
};

QUrl Asset(const QString& tag, const QString& filename) {
    const QByteArray url = QByteArray{Feed} + tag.toLatin1() + '/' + QUrl::toPercentEncoding(filename);
    return QUrl::fromEncoded(url);
}

bool Download(QNetworkAccessManager& manager, QUrl url, QIODevice& destination, qint64 limit,
              bool allow_missing, const std::atomic_bool& canceled) {
    static const QSet<QString> hosts{
        QStringLiteral("github.com"), QStringLiteral("release-assets.githubusercontent.com"),
        QStringLiteral("objects.githubusercontent.com"),
        QStringLiteral("github-releases.githubusercontent.com")};
    for (int redirects = 0; redirects <= 4; ++redirects) {
        CheckCanceled(canceled);
        Require(url.scheme() == QStringLiteral("https") && url.port(443) == 443 &&
                    url.userInfo().isEmpty() && hosts.contains(url.host()),
                Tr("GitHub returned an unexpected download address."));
        QNetworkRequest request{url};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Jinra-Shader-Sync"));
        request.setTransferTimeout(30'000);
        auto* reply = manager.get(request);
        SCOPE_EXIT { delete reply; };
        reply->setReadBufferSize(2 * CopyBuffer);

        QEventLoop loop;
        QTimer poll;
        QTimer deadline;
        qint64 received{};
        QString failure;
        const auto drain = [&] {
            if (!failure.isEmpty()) {
                return;
            }
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
                reply->readAll();
                return;
            }
            while (reply->bytesAvailable() > 0) {
                const auto data = reply->read(CopyBuffer);
                if (data.isEmpty()) {
                    break;
                }
                received += data.size();
                if (received > limit || destination.write(data) != data.size()) {
                    failure = Tr("The download exceeds its expected size or could not be saved.");
                    reply->abort();
                    break;
                }
            }
        };
        QObject::connect(reply, &QNetworkReply::readyRead, &loop, drain);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
            if (canceled) {
                reply->abort();
            }
        });
        QObject::connect(&deadline, &QTimer::timeout, &loop, [&] {
            failure = Tr("The GitHub download timed out.");
            reply->abort();
        });
        deadline.setSingleShot(true);
        deadline.start(limit > 64 * MiB ? 30 * 60'000 : 5 * 60'000);
        poll.start(100);
        if (!reply->isFinished()) {
            loop.exec();
        }
        CheckCanceled(canceled);
        drain();
        Require(failure.isEmpty(), failure);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (status >= 300 && status < 400 && !redirect.isEmpty()) {
            url = url.resolved(redirect);
            continue;
        }
        if (allow_missing && status == 404) {
            return false;
        }
        Require(status == 200 && reply->error() == QNetworkReply::NoError,
                Tr("The GitHub download failed (%1).").arg(reply->errorString()));
        return true;
    }
    throw SyncError{Tr("GitHub redirected the download too many times.")};
}

QList<Chunk> ParseCatalog(const QByteArray& bytes, const QString& title) {
    const QJsonObject manifest = QJsonDocument::fromJson(bytes).object();
    const auto list = manifest.value(QStringLiteral("chunks"));
    Require(manifest.value(QStringLiteral("format")).toInt() == 1 &&
                manifest.value(QStringLiteral("source_version")).toInt() == CacheVersion &&
                manifest.value(QStringLiteral("title_id")).toString() == title && list.isArray(),
            Tr("The catalog entry does not match this game or this Jinra cache version."));
    static const QRegularExpression hex64{QStringLiteral("\\A[0-9a-f]{64}\\z")};
    static const QRegularExpression hex32{QStringLiteral("\\A[0-9a-f]{32}\\z")};
    static const QRegularExpression archive_name{
        QStringLiteral("\\A[A-Za-z0-9][A-Za-z0-9 ._-]{0,120}\\.7z\\z")};
    static const QRegularExpression tag_suffix{QStringLiteral("\\A[0-9]{8}[a-z0-9-]*\\z")};
    QList<Chunk> chunks;
    QSet<QByteArray> unique;
    const QString tag_prefix = title + QLatin1Char('-');
    for (const auto& entry : list.toArray()) {
        const auto item = entry.toObject();
        const auto digest = item.value(QStringLiteral("sha256")).toString();
        Chunk chunk{item.value(QStringLiteral("file")).toString(),
                    item.value(QStringLiteral("tag")).toString(),
                    QByteArray::fromHex(digest.toLatin1()),
                    item.value(QStringLiteral("size")).toInteger(-1),
                    item.value(QStringLiteral("unpacked_size")).toInteger(-1), {}};
        const bool tag_valid = chunk.tag == QLatin1String(Catalog) ||
            (chunk.tag.startsWith(tag_prefix) && tag_suffix.match(chunk.tag.mid(17)).hasMatch());
        const bool asset_valid = chunk.IsArchive()
            ? archive_name.match(chunk.file).hasMatch() && chunk.size <= 512 * MiB &&
                  chunk.unpacked_size >= 12 && chunk.unpacked_size <= 8192 * MiB
            : chunk.file == digest.left(16) + QStringLiteral(".gz") && chunk.size <= 64 * MiB;
        Require(tag_valid && asset_valid && chunk.size > 0 && hex64.match(digest).hasMatch(),
                Tr("The catalog lists an invalid download."));
        const auto ids = item.value(QStringLiteral("ids")).toArray();
        Require(!ids.isEmpty() && ids.size() == item.value(QStringLiteral("entries")).toInteger(-1),
                Tr("The catalog entry count is inconsistent."));
        for (const auto& value : ids) {
            const auto id = value.toString();
            const auto hex = id.toLatin1();
            Require(hex32.match(id).hasMatch() && !unique.contains(hex),
                    Tr("The catalog contains an invalid or repeated shader ID."));
            unique.insert(hex);
            chunk.ids.push_back(hex);
        }
        chunks.push_back(std::move(chunk));
    }
    Require(unique.size() == manifest.value(QStringLiteral("total_entries")).toInteger(-1),
            Tr("The catalog entry count is inconsistent."));
    return chunks;
}

struct Record {
    qint64 offset;
    qint64 size;
    QByteArray id;
};

QList<Record> Scan(QFileDevice& file, bool has_header, const std::atomic_bool& canceled) {
    const auto invalid = Tr("The shader cache has an invalid or incompatible record.");
    Require(file.seek(0), invalid);
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    if (has_header) {
        const auto magic = file.read(8);
        u32 version{};
        stream >> version;
        Require(magic == Magic && version == CacheVersion, invalid);
    }
    QList<Record> records;
    const qint64 end = file.size();
    while (file.pos() < end) {
        CheckCanceled(canceled);
        const qint64 start = file.pos();
        u32 count{};
        stream >> count;
        Require(count >= 1 && count <= 6, invalid);
        bool compute{};
        for (u32 i = 0; i < count; ++i) {
            std::array<u64, 6> counts{};
            std::array<u32, 7> fields{};
            for (auto& value : counts) {
                stream >> value;
            }
            for (auto& value : fields) {
                stream >> value;
            }
            const auto stage = fields.back();
            compute = stage == 5;
            Require(stream.status() == QDataStream::Ok && stage <= 8 &&
                        (!compute || count == 1) && counts.front() <= 64 * MiB &&
                        counts.front() % 8 == 0,
                    invalid);
            constexpr std::array<u64, 5> sizes{8, 8, 12, 9, 12};
            u64 size = counts.front() + (compute ? 16 : 80);
            for (size_t table = 0; table < sizes.size(); ++table) {
                Require(counts[table + 1] <= 1'000'000, invalid);
                size += counts[table + 1] * sizes[table];
            }
            if (stage == 3) {
                size += 32;
            } else if (stage == 7 || stage == 8) {
                size += 92;
            }
            Require(size <= static_cast<u64>(end - file.pos()) &&
                        file.seek(file.pos() + static_cast<qint64>(size)), invalid);
        }
        const qint64 key_size = compute ? 24 : 1056;
        const auto key = file.read(key_size);
        Require(key.size() == key_size, invalid);
        const auto hash = QCryptographicHash::hash(key, QCryptographicHash::Sha256);
        records.push_back({start, file.pos() - start, hash.first(16).toHex()});
    }
    return records;
}

QSet<QByteArray> ExistingIds(QFile& file, const std::atomic_bool& canceled) {
    QSet<QByteArray> ids;
    if (file.exists()) {
        Require(file.open(QIODevice::ReadOnly), Tr("Could not open the local shader cache."));
        for (const auto& entry : Scan(file, true, canceled)) {
            ids.insert(entry.id);
        }
    }
    return ids;
}

Plan Check(const QString& path, const QString& title, const std::atomic_bool& canceled) {
    static_assert(CacheVersion == 54, "Update the Shader Sync parser and catalog together");
    QNetworkAccessManager manager;
    QBuffer manifest;
    manifest.open(QIODevice::ReadWrite);
    if (!Download(manager, Asset(QLatin1String(Catalog), title + QStringLiteral(".json")), manifest,
                  8 * MiB, true, canceled)) {
        return {};
    }
    QFile local{path};
    const auto existing = ExistingIds(local, canceled);
    Plan plan{.available = true};
    for (auto& chunk : ParseCatalog(manifest.data(), title)) {
        const auto missing = std::ranges::count_if(chunk.ids,
            [&](const auto& id) { return !existing.contains(id); });
        if (missing != 0) {
            plan.missing += missing;
            plan.download_size += chunk.size;
            plan.chunks.push_back(std::move(chunk));
        }
    }
    return plan;
}

void Copy(QIODevice& source, QIODevice& output, qint64 remaining,
          const std::atomic_bool& canceled) {
    while (remaining > 0) {
        CheckCanceled(canceled);
        const auto bytes = source.read(std::min(remaining, CopyBuffer));
        Require(!bytes.isEmpty() && output.write(bytes) == bytes.size(),
                Tr("Could not copy data while merging the shader cache."));
        remaining -= bytes.size();
    }
}

void ExtractArchive(const QString& archive, QIODevice& output, qint64 expected,
                    const std::atomic_bool& canceled) {
#ifdef _WIN32
    std::array<wchar_t, MAX_PATH> directory{};
    const UINT size = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
    Require(size > 0 && size < directory.size(), Tr("Could not find the Windows archive utility."));
    const auto tar = QDir{QString::fromWCharArray(directory.data(), size)}.filePath(
        QStringLiteral("tar.exe"));
    QProcess process;
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
    process.setStandardErrorFile(QProcess::nullDevice());
    // Stream the one expected file. Archive paths are never used as extraction destinations.
    process.start(tar, {QStringLiteral("-xOf"), archive, QStringLiteral("--"), CacheName()});
    Require(process.waitForStarted(5000), Tr("Windows could not open this shader archive."));
    SCOPE_EXIT {
        if (process.state() != QProcess::NotRunning) {
            process.kill();
            process.waitForFinished();
        }
    };
    const auto write = [&] {
        while (process.bytesAvailable() > 0) {
            CheckCanceled(canceled);
            const auto data = process.read(CopyBuffer);
            Require(data.size() <= expected && output.write(data) == data.size(),
                    Tr("The extracted shader cache has an unexpected size."));
            expected -= data.size();
        }
    };
    while (process.state() != QProcess::NotRunning) {
        CheckCanceled(canceled);
        process.waitForReadyRead(100);
        write();
    }
    write();
    Require(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 &&
                expected == 0, Tr("The shader archive did not contain the expected cache."));
#else
    throw SyncError{Tr("This shader archive requires the Windows version of Jinra.")};
#endif
}

void ExtractGzip(QIODevice& source, QIODevice& output, const std::atomic_bool& canceled) {
    z_stream stream{};
    Require(inflateInit2(&stream, 16 + MAX_WBITS) == Z_OK, Tr("Could not open the shader data."));
    SCOPE_EXIT { inflateEnd(&stream); };
    QByteArray in;
    std::array<Bytef, CopyBuffer> buffer{};
    qint64 total{};
    for (;;) {
        CheckCanceled(canceled);
        if (stream.avail_in == 0) {
            in = source.read(CopyBuffer);
            stream.next_in = reinterpret_cast<Bytef*>(in.data());
            stream.avail_in = static_cast<uInt>(in.size());
        }
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<uInt>(buffer.size());
        const int result = inflate(&stream, Z_NO_FLUSH);
        const auto count = static_cast<qint64>(buffer.size() - stream.avail_out);
        total += count;
        Require((result == Z_OK || result == Z_STREAM_END) && total <= 256 * MiB &&
                    output.write(reinterpret_cast<const char*>(buffer.data()), count) == count,
                Tr("A compressed shader cache part is invalid or too large."));
        if (result == Z_STREAM_END) {
            Require(stream.avail_in == 0 && source.atEnd(),
                    Tr("The compressed shader cache has unexpected extra data."));
            return;
        }
    }
}

void EnsureExclusiveEmulator() {
#ifdef _WIN32
    const HANDLE processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    Require(processes != INVALID_HANDLE_VALUE, Tr("Could not check for other Jinra instances."));
    SCOPE_EXIT { CloseHandle(processes); };
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    Require(Process32FirstW(processes, &entry), Tr("Could not check for other Jinra instances."));
    do {
        if (entry.th32ProcessID == GetCurrentProcessId()) {
            continue;
        }
        const auto name = QString::fromWCharArray(entry.szExeFile);
        Require(name.compare(QStringLiteral("jinra.exe"), Qt::CaseInsensitive) != 0 &&
                    name.compare(QStringLiteral("jinra-cli.exe"), Qt::CaseInsensitive) != 0,
                Tr("Close the other Jinra instance before syncing shaders."));
    } while (Process32NextW(processes, &entry));
#endif
}

qsizetype Apply(const QString& path, const Plan& plan, const std::atomic_bool& canceled,
                const Report& report) {
    EnsureExclusiveEmulator();
    const QFileInfo target{path};
    Require(QDir{}.mkpath(target.absolutePath()), Tr("Could not create the shader cache folder."));
    QLockFile lock{path + QStringLiteral(".sync.lock")};
    lock.setStaleLockTime(0);
    Require(lock.tryLock(), Tr("Another shader sync is already in progress for this game."));
    QFile local{path};
    auto ids = ExistingIds(local, canceled);
    const qint64 original_size = local.isOpen() ? local.size() : -1;
    const auto original_time = QFileInfo{path}.lastModified();
    QSaveFile merged{path};
    Require(merged.open(QIODevice::WriteOnly), Tr("Could not create the merged shader cache."));
    if (local.isOpen()) {
        Require(local.seek(0), Tr("Could not read the local shader cache."));
        Copy(local, merged, original_size, canceled);
    } else {
        QDataStream stream{&merged};
        stream.setByteOrder(QDataStream::LittleEndian);
        Require(merged.write(Magic, 8) == 8, Tr("Could not write the new shader cache."));
        stream << CacheVersion;
        Require(stream.status() == QDataStream::Ok, Tr("Could not write the new shader cache."));
    }

    qsizetype added{};
    QNetworkAccessManager manager;
    for (qsizetype i = 0; i < plan.chunks.size(); ++i) {
        CheckCanceled(canceled);
        const auto& chunk = plan.chunks[i];
        if (std::ranges::all_of(chunk.ids, [&](const auto& id) { return ids.contains(id); })) {
            continue;
        }
        report(Tr("Downloading and checking part %1 of %2…").arg(i + 1).arg(plan.chunks.size()));
        QTemporaryFile compressed{target.absolutePath() + QStringLiteral("/jinra-sync-XXXXXX")};
        Require(compressed.open(), Tr("Could not create the download file."));
        Download(manager, Asset(chunk.tag, chunk.file), compressed, chunk.size, false, canceled);
        QCryptographicHash hash{QCryptographicHash::Sha256};
        Require(compressed.size() == chunk.size && compressed.seek(0) &&
                    hash.addData(&compressed) && hash.result() == chunk.digest &&
                    compressed.seek(0),
                Tr("A shader download failed its SHA-256 check."));

        report(Tr("Extracting and merging shaders…"));
        QTemporaryFile unpacked{target.absolutePath() + QStringLiteral("/jinra-sync-XXXXXX")};
        Require(unpacked.open(), Tr("Could not create the unpacked cache file."));
        if (chunk.IsArchive()) {
            compressed.close();
            ExtractArchive(compressed.fileName(), unpacked, chunk.unpacked_size, canceled);
        } else {
            ExtractGzip(compressed, unpacked, canceled);
        }
        const auto entries = Scan(unpacked, chunk.IsArchive(), canceled);
        QList<QByteArray> actual;
        QSet<QByteArray> seen;
        for (const auto& entry : entries) {
            if (!chunk.IsArchive() || !seen.contains(entry.id)) {
                actual.push_back(entry.id);
            }
            seen.insert(entry.id);
        }
        Require(actual == chunk.ids, Tr("The downloaded shader IDs do not match the catalog."));
        for (const auto& entry : entries) {
            if (ids.contains(entry.id)) {
                continue;
            }
            Require(unpacked.seek(entry.offset), Tr("Could not seek in the downloaded cache."));
            Copy(unpacked, merged, entry.size, canceled);
            ids.insert(entry.id);
            ++added;
        }
    }

    if (added != 0) {
        CheckCanceled(canceled);
        EnsureExclusiveEmulator();
        const QFileInfo current{path};
        Require((original_size < 0 && !current.exists()) ||
                    (current.exists() && current.size() == original_size &&
                     current.lastModified() == original_time),
                Tr("The local shader cache changed during the sync. Try again."));
        local.close();
        Require(merged.commit(), Tr("Could not replace the local shader cache."));
    }
    return added;
}

class TaskDialog final : public QDialog {
public:
    TaskDialog(QWidget* parent, const QString& label) : QDialog{parent} {
        setWindowTitle(Tr("Sync Shaders"));
        setWindowModality(Qt::ApplicationModal);
        auto* layout = new QVBoxLayout(this);
        status = new QLabel(label, this);
        status->setWordWrap(true);
        layout->addWidget(status);
        auto* progress = new QProgressBar(this);
        progress->setRange(0, 0);
        layout->addWidget(progress);
        cancel = new QPushButton(Tr("Cancel"), this);
        layout->addWidget(cancel, 0, Qt::AlignRight);
        connect(cancel, &QPushButton::clicked, this, &TaskDialog::reject);
        setMinimumWidth(380);
    }

    void reject() override {
        canceled = true;
        cancel->setEnabled(false);
        status->setText(Tr("Canceling…"));
    }

    void Update(QString label) {
        if (!canceled) {
            status->setText(std::move(label));
        }
    }

    std::atomic_bool canceled{};

private:
    QLabel* status{};
    QPushButton* cancel{};
};

template <typename T>
struct TaskResult {
    T result{};
    QString error;
    bool canceled{};
};

template <typename T, typename Work>
TaskResult<T> RunTask(QWidget* parent, const QString& label, Work work) {
    TaskDialog dialog{parent, label};
    QFutureWatcher<TaskResult<T>> watcher;
    QObject::connect(&watcher, &QFutureWatcher<TaskResult<T>>::finished, &dialog, &QDialog::accept);
    auto future = QtConcurrent::run([&]() -> TaskResult<T> {
        try {
            const Report report = [&](QString text) {
                QMetaObject::invokeMethod(&dialog, [&dialog, text = std::move(text)]() mutable {
                    dialog.Update(std::move(text));
                }, Qt::QueuedConnection);
            };
            return {.result = work(dialog.canceled, report)};
        } catch (const Canceled&) {
            return {.canceled = true};
        } catch (const SyncError& error) {
            return {.error = error.message};
        } catch (const std::exception& error) {
            return {.error = QString::fromUtf8(error.what())};
        }
    });
    watcher.setFuture(future);
    dialog.exec();
    return watcher.result();
}

} // namespace

void ShowShaderSync(QWidget* parent, u64 program_id) {
    const auto title = QString::number(program_id, 16).rightJustified(16, QLatin1Char('0')).toUpper();
    const auto directory =
        Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir) / title.toLower().toStdString();
    const auto path = QString::fromStdU16String(directory.u16string()) + QLatin1Char('/') + CacheName();
    const auto heading = Tr("Sync Shaders");
    const auto check = RunTask<Plan>(parent, Tr("Checking the Jinra shader catalog…"),
        [&](const auto& canceled, const auto&) { return Check(path, title, canceled); });
    if (check.canceled) {
        return;
    }
    if (!check.error.isEmpty()) {
        QMessageBox::warning(parent, heading, check.error);
        return;
    }
    const auto& plan = check.result;
    if (!plan.available || plan.missing == 0) {
        QMessageBox::information(parent, heading,
            plan.available ? Tr("This game already has all shaders in the shared catalog.")
                           : Tr("The Jinra catalog has no shader pack for this game yet."));
        return;
    }
    const auto answer = QMessageBox::question(parent, heading,
        Tr("The catalog has %1 new entries for this game (%2 MiB to download). Sync them now?")
            .arg(plan.missing).arg(plan.download_size / static_cast<double>(MiB), 0, 'f', 1));
    if (answer != QMessageBox::Yes) {
        return;
    }
    const auto sync = RunTask<qsizetype>(parent, Tr("Preparing to sync shaders…"),
        [&](const auto& canceled, const auto& report) { return Apply(path, plan, canceled, report); });
    if (sync.canceled) {
        return;
    }
    if (!sync.error.isEmpty()) {
        QMessageBox::warning(parent, heading, sync.error);
        return;
    }
    QMessageBox::information(parent, heading,
                             Tr("Added %1 shared shader entries.").arg(sync.result));
}
