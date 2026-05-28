#include "WeReadCacheStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace WeReadCacheStore {

namespace {

constexpr uint16_t kVersionShelf = 1;
constexpr uint16_t kVersionMeta = 1;
constexpr uint16_t kVersionNotes = 1;
constexpr uint16_t kVersionPublicReviews = 1;
constexpr uint16_t kVersionChapters = 1;
constexpr uint16_t kVersionBestMarks = 1;
constexpr uint16_t kVersionSimilar = 1;

// Hard ceiling on row count when deserializing — defends against a corrupt
// count header swallowing all heap before bailing.
constexpr uint32_t kMaxRows = 10000;

constexpr char kMagicShelf[4] = {'W', 'R', 'S', 'F'};
constexpr char kMagicMeta[4] = {'W', 'R', 'M', 'T'};
constexpr char kMagicNotes[4] = {'W', 'R', 'N', 'T'};
constexpr char kMagicPubRv[4] = {'W', 'R', 'R', 'P'};
constexpr char kMagicChap[4] = {'W', 'R', 'C', 'H'};
constexpr char kMagicBest[4] = {'W', 'R', 'B', 'M'};
constexpr char kMagicSim[4] = {'W', 'R', 'S', 'M'};

std::string bookDir(const std::string& bookId) {
  std::string p(kRoot);
  p += '/';
  p += bookId;
  return p;
}

std::string bookFile(const std::string& bookId, const char* leaf) {
  std::string p = bookDir(bookId);
  p += '/';
  p += leaf;
  return p;
}

bool ensureBookDir(const std::string& bookId) {
  if (!Storage.ensureDirectoryExists(kRoot)) return false;
  return Storage.ensureDirectoryExists(bookDir(bookId).c_str());
}

// Writes magic + version. Returns the open HalFile (or invalid if open failed).
bool openWriteWithHeader(const char* mod, const char* path, const char (&magic)[4], uint16_t version,
                         HalFile& outFile) {
  if (!Storage.openFileForWrite(mod, path, outFile)) {
    LOG_ERR(mod, "openFileForWrite(%s) failed", path);
    return false;
  }
  outFile.write(reinterpret_cast<const uint8_t*>(magic), 4);
  outFile.write(reinterpret_cast<const uint8_t*>(&version), sizeof(version));
  return true;
}

// Opens for read, verifies magic + version, leaves cursor at byte 6.
bool openReadCheckHeader(const char* mod, const char* path, const char (&magic)[4], uint16_t expectVersion,
                         HalFile& outFile) {
  if (!Storage.exists(path)) return false;
  if (!Storage.openFileForRead(mod, path, outFile)) {
    LOG_ERR(mod, "openFileForRead(%s) failed", path);
    return false;
  }
  char m[4];
  if (outFile.read(m, 4) != 4 || std::memcmp(m, magic, 4) != 0) {
    LOG_ERR(mod, "magic mismatch for %s", path);
    return false;
  }
  uint16_t version = 0;
  if (outFile.read(reinterpret_cast<uint8_t*>(&version), sizeof(version)) != sizeof(version) ||
      version != expectVersion) {
    LOG_ERR(mod, "version mismatch for %s (%u != %u)", path, version, expectVersion);
    return false;
  }
  return true;
}

bool readCount(HalFile& file, uint32_t& outCount) {
  if (file.read(reinterpret_cast<uint8_t*>(&outCount), sizeof(outCount)) != sizeof(outCount)) return false;
  if (outCount > kMaxRows) {
    LOG_ERR("WRCACHE", "row count %u exceeds limit", outCount);
    return false;
  }
  return true;
}

// ---- Field-level write helpers (one per Row type) -------------------------
// Kept inline-friendly so a future schema bump just edits these.

void writeBookCard(HalFile& f, const WeReadModels::BookCard& b) {
  serialization::writeString(f, b.bookId);
  serialization::writeString(f, b.title);
  serialization::writeString(f, b.author);
  serialization::writeString(f, b.category);
  serialization::writePod(f, b.readUpdateTime);
  serialization::writePod(f, b.finishReading);
  serialization::writePod(f, b.isTop);
  serialization::writePod(f, b.secret);
  serialization::writePod(f, b.isAlbum);
}

void readBookCard(HalFile& f, WeReadModels::BookCard& b) {
  serialization::readString(f, b.bookId);
  serialization::readString(f, b.title);
  serialization::readString(f, b.author);
  serialization::readString(f, b.category);
  serialization::readPod(f, b.readUpdateTime);
  serialization::readPod(f, b.finishReading);
  serialization::readPod(f, b.isTop);
  serialization::readPod(f, b.secret);
  serialization::readPod(f, b.isAlbum);
}

void writeBookmark(HalFile& f, const WeReadModels::BookmarkRow& b) {
  serialization::writeString(f, b.bookmarkId);
  serialization::writeString(f, b.markText);
  serialization::writeString(f, b.range);
  serialization::writePod(f, b.chapterUid);
  serialization::writePod(f, b.createTime);
}

void readBookmark(HalFile& f, WeReadModels::BookmarkRow& b) {
  serialization::readString(f, b.bookmarkId);
  serialization::readString(f, b.markText);
  serialization::readString(f, b.range);
  serialization::readPod(f, b.chapterUid);
  serialization::readPod(f, b.createTime);
}

void writePublicReview(HalFile& f, const WeReadModels::PublicReviewRow& r) {
  serialization::writeString(f, r.reviewId);
  serialization::writeString(f, r.content);
  serialization::writeString(f, r.authorName);
  serialization::writeString(f, r.chapterName);
  serialization::writePod(f, r.starPercent);
  serialization::writePod(f, r.isFinish);
  serialization::writePod(f, r.createTime);
  serialization::writePod(f, r.idx);
}

void readPublicReview(HalFile& f, WeReadModels::PublicReviewRow& r) {
  serialization::readString(f, r.reviewId);
  serialization::readString(f, r.content);
  serialization::readString(f, r.authorName);
  serialization::readString(f, r.chapterName);
  serialization::readPod(f, r.starPercent);
  serialization::readPod(f, r.isFinish);
  serialization::readPod(f, r.createTime);
  serialization::readPod(f, r.idx);
}

void writeChapter(HalFile& f, const WeReadModels::ChapterRow& c) {
  serialization::writeString(f, c.title);
  serialization::writePod(f, c.chapterUid);
  serialization::writePod(f, c.chapterIdx);
  serialization::writePod(f, c.wordCount);
  serialization::writePod(f, c.level);
  serialization::writePod(f, c.paid);
}

void readChapter(HalFile& f, WeReadModels::ChapterRow& c) {
  serialization::readString(f, c.title);
  serialization::readPod(f, c.chapterUid);
  serialization::readPod(f, c.chapterIdx);
  serialization::readPod(f, c.wordCount);
  serialization::readPod(f, c.level);
  serialization::readPod(f, c.paid);
}

void writeBestMark(HalFile& f, const WeReadModels::BestMarkRow& b) {
  serialization::writeString(f, b.bookmarkId);
  serialization::writeString(f, b.markText);
  serialization::writeString(f, b.range);
  serialization::writePod(f, b.chapterUid);
  serialization::writePod(f, b.totalCount);
}

void readBestMark(HalFile& f, WeReadModels::BestMarkRow& b) {
  serialization::readString(f, b.bookmarkId);
  serialization::readString(f, b.markText);
  serialization::readString(f, b.range);
  serialization::readPod(f, b.chapterUid);
  serialization::readPod(f, b.totalCount);
}

void writeSearchRow(HalFile& f, const WeReadModels::SearchRow& r) {
  serialization::writeString(f, r.bookId);
  serialization::writeString(f, r.title);
  serialization::writeString(f, r.author);
  serialization::writeString(f, r.category);
  serialization::writePod(f, r.newRating);
  serialization::writePod(f, r.newRatingCount);
  serialization::writePod(f, r.readingCount);
  serialization::writePod(f, r.soldout);
  serialization::writePod(f, r.searchIdx);
}

void readSearchRow(HalFile& f, WeReadModels::SearchRow& r) {
  serialization::readString(f, r.bookId);
  serialization::readString(f, r.title);
  serialization::readString(f, r.author);
  serialization::readString(f, r.category);
  serialization::readPod(f, r.newRating);
  serialization::readPod(f, r.newRatingCount);
  serialization::readPod(f, r.readingCount);
  serialization::readPod(f, r.soldout);
  serialization::readPod(f, r.searchIdx);
}

}  // namespace

// ---- Shelf -----------------------------------------------------------------

bool saveShelf(const std::vector<WeReadModels::BookCard>& books) {
  if (!Storage.ensureDirectoryExists(kRoot)) return false;
  std::string path = std::string(kRoot) + "/shelf.bin";

  HalFile f;
  if (!openWriteWithHeader("WRCACHE", path.c_str(), kMagicShelf, kVersionShelf, f)) return false;

  uint32_t count = static_cast<uint32_t>(books.size());
  serialization::writePod(f, count);
  for (const auto& b : books) writeBookCard(f, b);
  f.flush();
  LOG_DBG("WRCACHE", "saveShelf: %u books", count);
  return true;
}

bool loadShelf(std::vector<WeReadModels::BookCard>& outBooks) {
  outBooks.clear();
  std::string path = std::string(kRoot) + "/shelf.bin";

  HalFile f;
  if (!openReadCheckHeader("WRCACHE", path.c_str(), kMagicShelf, kVersionShelf, f)) return false;

  uint32_t count = 0;
  if (!readCount(f, count)) return false;
  outBooks.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    WeReadModels::BookCard b;
    readBookCard(f, b);
    outBooks.push_back(std::move(b));
  }
  return true;
}

// ---- Per-book meta ---------------------------------------------------------

bool saveBookMeta(const std::string& bookId, const WeReadModels::BookCard& card) {
  if (bookId.empty()) return false;
  if (!ensureBookDir(bookId)) return false;
  std::string path = bookFile(bookId, "meta.bin");
  HalFile f;
  if (!openWriteWithHeader("WRCACHE", path.c_str(), kMagicMeta, kVersionMeta, f)) return false;
  writeBookCard(f, card);
  f.flush();
  return true;
}

bool loadBookMeta(const std::string& bookId, WeReadModels::BookCard& outCard) {
  outCard = WeReadModels::BookCard{};
  if (bookId.empty()) return false;
  std::string path = bookFile(bookId, "meta.bin");
  HalFile f;
  if (!openReadCheckHeader("WRCACHE", path.c_str(), kMagicMeta, kVersionMeta, f)) return false;
  readBookCard(f, outCard);
  return true;
}

// ---- Notes -----------------------------------------------------------------

bool saveNotes(const std::string& bookId, const std::vector<WeReadModels::BookmarkRow>& rows) {
  if (bookId.empty()) return false;
  if (!ensureBookDir(bookId)) return false;
  std::string path = bookFile(bookId, "notes.bin");
  HalFile f;
  if (!openWriteWithHeader("WRCACHE", path.c_str(), kMagicNotes, kVersionNotes, f)) return false;
  uint32_t count = static_cast<uint32_t>(rows.size());
  serialization::writePod(f, count);
  for (const auto& r : rows) writeBookmark(f, r);
  f.flush();
  LOG_DBG("WRCACHE", "saveNotes(%s): %u bookmarks", bookId.c_str(), count);
  return true;
}

bool loadNotes(const std::string& bookId, std::vector<WeReadModels::BookmarkRow>& outRows) {
  outRows.clear();
  if (bookId.empty()) return false;
  std::string path = bookFile(bookId, "notes.bin");
  HalFile f;
  if (!openReadCheckHeader("WRCACHE", path.c_str(), kMagicNotes, kVersionNotes, f)) return false;
  uint32_t count = 0;
  if (!readCount(f, count)) return false;
  outRows.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    WeReadModels::BookmarkRow r;
    readBookmark(f, r);
    outRows.push_back(std::move(r));
  }
  return true;
}

// ---- Public reviews --------------------------------------------------------

bool savePublicReviews(const std::string& bookId, const std::vector<WeReadModels::PublicReviewRow>& rows) {
  if (bookId.empty()) return false;
  if (!ensureBookDir(bookId)) return false;
  std::string path = bookFile(bookId, "reviews_public.bin");
  HalFile f;
  if (!openWriteWithHeader("WRCACHE", path.c_str(), kMagicPubRv, kVersionPublicReviews, f)) return false;
  uint32_t count = static_cast<uint32_t>(rows.size());
  serialization::writePod(f, count);
  for (const auto& r : rows) writePublicReview(f, r);
  f.flush();
  return true;
}

bool loadPublicReviews(const std::string& bookId, std::vector<WeReadModels::PublicReviewRow>& outRows) {
  outRows.clear();
  if (bookId.empty()) return false;
  std::string path = bookFile(bookId, "reviews_public.bin");
  HalFile f;
  if (!openReadCheckHeader("WRCACHE", path.c_str(), kMagicPubRv, kVersionPublicReviews, f)) return false;
  uint32_t count = 0;
  if (!readCount(f, count)) return false;
  outRows.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    WeReadModels::PublicReviewRow r;
    readPublicReview(f, r);
    outRows.push_back(std::move(r));
  }
  return true;
}

// ---- Chapters --------------------------------------------------------------

bool saveChapters(const std::string& bookId, const std::vector<WeReadModels::ChapterRow>& rows) {
  if (bookId.empty()) return false;
  if (!ensureBookDir(bookId)) return false;
  std::string path = bookFile(bookId, "chapters.bin");
  HalFile f;
  if (!openWriteWithHeader("WRCACHE", path.c_str(), kMagicChap, kVersionChapters, f)) return false;
  uint32_t count = static_cast<uint32_t>(rows.size());
  serialization::writePod(f, count);
  for (const auto& r : rows) writeChapter(f, r);
  f.flush();
  return true;
}

bool loadChapters(const std::string& bookId, std::vector<WeReadModels::ChapterRow>& outRows) {
  outRows.clear();
  if (bookId.empty()) return false;
  std::string path = bookFile(bookId, "chapters.bin");
  HalFile f;
  if (!openReadCheckHeader("WRCACHE", path.c_str(), kMagicChap, kVersionChapters, f)) return false;
  uint32_t count = 0;
  if (!readCount(f, count)) return false;
  outRows.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    WeReadModels::ChapterRow r;
    readChapter(f, r);
    outRows.push_back(std::move(r));
  }
  return true;
}

// ---- Best marks ------------------------------------------------------------

bool saveBestMarks(const std::string& bookId, const std::vector<WeReadModels::BestMarkRow>& rows) {
  if (bookId.empty()) return false;
  if (!ensureBookDir(bookId)) return false;
  std::string path = bookFile(bookId, "bestmarks.bin");
  HalFile f;
  if (!openWriteWithHeader("WRCACHE", path.c_str(), kMagicBest, kVersionBestMarks, f)) return false;
  uint32_t count = static_cast<uint32_t>(rows.size());
  serialization::writePod(f, count);
  for (const auto& r : rows) writeBestMark(f, r);
  f.flush();
  return true;
}

bool loadBestMarks(const std::string& bookId, std::vector<WeReadModels::BestMarkRow>& outRows) {
  outRows.clear();
  if (bookId.empty()) return false;
  std::string path = bookFile(bookId, "bestmarks.bin");
  HalFile f;
  if (!openReadCheckHeader("WRCACHE", path.c_str(), kMagicBest, kVersionBestMarks, f)) return false;
  uint32_t count = 0;
  if (!readCount(f, count)) return false;
  outRows.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    WeReadModels::BestMarkRow r;
    readBestMark(f, r);
    outRows.push_back(std::move(r));
  }
  return true;
}

// ---- Similar ---------------------------------------------------------------

bool saveSimilar(const std::string& bookId, const std::vector<WeReadModels::SearchRow>& rows) {
  if (bookId.empty()) return false;
  if (!ensureBookDir(bookId)) return false;
  std::string path = bookFile(bookId, "similar.bin");
  HalFile f;
  if (!openWriteWithHeader("WRCACHE", path.c_str(), kMagicSim, kVersionSimilar, f)) return false;
  uint32_t count = static_cast<uint32_t>(rows.size());
  serialization::writePod(f, count);
  for (const auto& r : rows) writeSearchRow(f, r);
  f.flush();
  return true;
}

bool loadSimilar(const std::string& bookId, std::vector<WeReadModels::SearchRow>& outRows) {
  outRows.clear();
  if (bookId.empty()) return false;
  std::string path = bookFile(bookId, "similar.bin");
  HalFile f;
  if (!openReadCheckHeader("WRCACHE", path.c_str(), kMagicSim, kVersionSimilar, f)) return false;
  uint32_t count = 0;
  if (!readCount(f, count)) return false;
  outRows.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    WeReadModels::SearchRow r;
    readSearchRow(f, r);
    outRows.push_back(std::move(r));
  }
  return true;
}

// ---- Queries / management --------------------------------------------------

bool hasBookCached(const std::string& bookId) {
  if (bookId.empty()) return false;
  return Storage.exists(bookFile(bookId, "notes.bin").c_str());
}

bool removeBookCache(const std::string& bookId) {
  if (bookId.empty()) return false;
  static const char* kLeaves[] = {"meta.bin",     "notes.bin",     "reviews_public.bin",
                                  "chapters.bin", "bestmarks.bin", "similar.bin"};
  for (const char* leaf : kLeaves) {
    std::string p = bookFile(bookId, leaf);
    if (Storage.exists(p.c_str())) Storage.remove(p.c_str());
  }
  return Storage.rmdir(bookDir(bookId).c_str());
}

}  // namespace WeReadCacheStore
