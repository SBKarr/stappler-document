// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/**
Copyright (c) 2017 Roman Katuntsev <sbkarr@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#include "SPDefine.h"
#include "RTCommonSource.h"

#include "SPFilesystem.h"
#include "SPResource.h"
#include "SPThread.h"
#include "SPString.h"
#include "SPTextureCache.h"
#include "SPAssetLibrary.h"

NS_RT_BEGIN

SP_DECLARE_EVENT(CommonSource, "RichTextSource", onError);
SP_DECLARE_EVENT(CommonSource, "RichTextSource", onDocument);

String CommonSource::getPathForUrl(const String &url) {
	auto dir = filesystem::writablePath("Documents");
	filesystem::mkdir(dir);
	return toString(dir, "/", string::stdlibHashUnsigned(url));
}

CommonSource::~CommonSource() { }

bool CommonSource::init() {
	if (!FontController::init(FontFaceMap(), {"fonts/", "common/fonts/"}, [this] (const layout::FontSource *s, const String &str) -> Bytes {
		if (str.compare(0, "document://"_len, "document://") == 0) {
			auto doc = getDocument();
			if (doc) {
				return doc->getFileData(str.substr("document://"_len));
			}
		} else if (str.compare(0, "local://"_len, "local://") == 0) {
			auto path = str.substr("local://"_len);
			if (filesystem::exists(path)) {
				return filesystem::readFile(path);
			} else if (filesystem::exists("fonts/" + path)) {
				return filesystem::readFile("fonts/" + path);
			} else if (filesystem::exists("common/fonts/" + path)) {
				return filesystem::readFile("common/fonts/" + path);
			}
		}
		return Bytes();
	}, false)) {
		return false;
	}

	_documentAsset.setCallback(std::bind(&CommonSource::onDocumentAssetUpdated, this, std::placeholders::_1));

	return true;
}

bool CommonSource::init(const StringDocument &str) {
	if (!init()) {
		return false;
	}

	_data = Bytes((const uint8_t *)str.get().data(), (const uint8_t *)(str.get().data() + str.get().size()));
	updateDocument();
	return true;
}

bool CommonSource::init(const DataReader<ByteOrder::Network> &data) {
	if (!init()) {
		return false;
	}

	_data = Bytes(data.data(), data.data() + data.size());
	updateDocument();
	return true;
}

bool CommonSource::init(const FilePath &file) {
	if (!init()) {
		return false;
	}

	_file = file.get();
	updateDocument();
	return true;
}

bool CommonSource::init(const String &url, const String &path,
		TimeInterval ttl, const String &cacheDir, const Asset::DownloadCallback &cb) {
	if (!init()) {
		return false;
	}

	retain();
	AssetLibrary::getInstance()->getAsset([this] (Asset *a) {
		onDocumentAsset(a);
		release();
	}, url, path, ttl, cacheDir, cb);

	return true;
}
bool CommonSource::init(Asset *a, bool enabled) {
	if (!init()) {
		return false;
	}

	_enabled = enabled;
	onDocumentAsset(a);
	return true;
}

bool CommonSource::init(const AssetCallback &cb, bool enabled) {
	if (!init()) {
		return false;
	}

	_enabled = enabled;

	retain();
	cb([this, enabled] (Asset *a) {
		onDocumentAsset(a);
		release();
	});
	return true;
}

void CommonSource::setAsset(Asset *a) {
	onDocumentAsset(a);
}

void CommonSource::setAsset(const AssetCallback &cb) {
	_enabled = false;

	retain();
	cb([this] (Asset *a) {
		onDocumentAsset(a);
		release();
	});
}

Document *CommonSource::getDocument() const {
	return static_cast<Document *>(_document.get());
}
Asset *CommonSource::getAsset() const {
	return _documentAsset;
}

Map<String, CommonSource::AssetMeta> CommonSource::getExternalAssetMeta() const {
	Map<String, CommonSource::AssetMeta> ret;
	for (auto &it : _networkAssets) {
		ret.emplace(it.first, it.second.meta);
	}
	return ret;
}

const Map<String, CommonSource::AssetData> &CommonSource::getExternalAssets() const {
	return _networkAssets;
}

bool CommonSource::isReady() const {
	return _document;
}

bool CommonSource::isActual() const {
	if (!_document) {
		return false;
	}

	if (!_documentAsset) {
		return true;
	}

	if (_documentLoading) {
		return false;
	}

	if (_loadedAssetMTime >= _documentAsset->getMTime()) {
		return true;
	} else if (_documentAsset->isUpdateAvailable()) {
		return true;
	}

	return false;
}

bool CommonSource::isDocumentLoading() const {
	return _documentLoading;
}

void CommonSource::refresh() {
	updateDocument();
}

bool CommonSource::tryReadLock(Ref *ref) {
	auto vec = getAssetsVec();
	if (vec.empty()) {
		return true;
	}

	return SyncRWLock::tryReadLock(ref, vec);
}

void CommonSource::retainReadLock(Ref *ref, const Function<void()> &cb) {
	auto vec = getAssetsVec();
	if (vec.empty()) {
		cb();
	} else {
		SyncRWLock::retainReadLock(ref, vec, cb);
	}
}

void CommonSource::releaseReadLock(Ref *ref) {
	auto vec = getAssetsVec();
	if (!vec.empty()) {
		SyncRWLock::releaseReadLock(ref, vec);
	}
}

void CommonSource::setEnabled(bool val) {
	if (_enabled != val) {
		_enabled = val;
		if (_enabled && _documentAsset) {
			onDocumentAssetUpdated(data::Subscription::Flag((uint8_t)Asset::CacheDataUpdated));
		}
	}
}
bool CommonSource::isEnabled() const {
	return _enabled;
}

void CommonSource::onDocumentAsset(Asset *a) {
	_documentAsset = a;
	if (_documentAsset) {
		_loadedAssetMTime = 0;
		_documentAsset->download();
		onDocumentAssetUpdated(data::Subscription::Flag((uint8_t)Asset::FileUpdated));
	}
}

void CommonSource::onDocumentAssetUpdated(data::Subscription::Flags f) {
	if (f.hasFlag((uint8_t)Asset::DownloadFailed)) {
		onError(this, Error::NetworkError);
	}

	if (_documentAsset->isDownloadAvailable() && !_documentAsset->isDownloadInProgress()) {
		if (_enabled) {
			_documentAsset->download();
		}
		onUpdate(this);
	}
	if (_loadedAssetMTime < _documentAsset->getMTime()) {
		tryLoadDocument();
	} else if ((f.initial() && _loadedAssetMTime == 0) || f.hasFlag((uint8_t)Asset::Update::FileUpdated)) {
		_loadedAssetMTime = 0;
		tryLoadDocument();
	}

	if (f.hasFlag((uint8_t)Asset::FileUpdated) || f.hasFlag((uint8_t)Asset::DownloadSuccessful) || f.hasFlag((uint8_t)Asset::DownloadFailed)) {
		onDocument(this, _documentAsset.get());
	}
}

static bool Source_tryLockAsset(Asset *a, uint64_t mtime, CommonSource *source) {
	if (a && a->isReadAvailable() &&
			((a->getMTime() > mtime && !a->isDownloadInProgress())
			|| mtime == 0)) {
		return a->tryReadLock(source);
	} else {
		return false;
	}
}

void CommonSource::tryLoadDocument() {
	if (!_enabled) {
		return;
	}
	bool assetLocked = false;
	if (Source_tryLockAsset(_documentAsset, _loadedAssetMTime, this)) {
		_loadedAssetMTime = _documentAsset->getMTime();
		assetLocked = true;
	}

	if (_data.empty() && _file.empty() && !assetLocked) {
		return;
	}

	auto &thread = TextureCache::thread();
	Rc<Document> *doc = new Rc<Document>(nullptr);
	Set<String> *assets = new Set<String>();

	auto filename = (assetLocked)?_documentAsset->getFilePath():_file;
	auto ct = (assetLocked)?_documentAsset->getContentType():String();

	_documentLoading = true;
	onUpdate(this);

	thread.perform([this, doc, filename, ct, assets] (const Task &) -> bool {
		*doc = openDocument(filename, ct);
		if (*doc) {
			auto &pages = (*doc)->getContentPages();
			for (auto &p_it : pages) {
				for (auto &str : p_it.second.assets) {
					assets->emplace(str);
				}
			}
		}
		return true;
	}, [this, doc, assetLocked, filename, assets] (const Task &, bool success) {
		if (success && *doc) {
			auto l = [this, doc, assetLocked] {
				_documentLoading = false;
				if (assetLocked) {
					_documentAsset->releaseReadLock(this);
				}
				onDocumentLoaded(*doc);
				delete doc;
			};
			if (onExternalAssets(*doc, *assets)) {
				l();
			} else {
				waitForAssets(move(l));
			}
		}
		delete assets;
	}, this);
}

void CommonSource::onDocumentLoaded(Document *doc) {
	if (_document != doc) {
		_document = doc;
		if (_document) {
			_dirty = true;
			_dirtyFlags = DirtyFontFace;
			updateSource();
		}
		onDocument(this);
	}
}

void CommonSource::acquireAsset(const String &url, const Function<void(Asset *)> &fn) {
	StringView urlView(url);
	if (urlView.is("http://") || urlView.is("https://")) {
		auto path = getPathForUrl(url);
		auto lib = AssetLibrary::getInstance();
		retain();
		lib->getAsset([this, fn] (Asset *a) {
			fn(a);
			release();
		}, url, path, config::getDocumentAssetTtl());
	} else {
		fn(nullptr);
	}
}

bool CommonSource::isExternalAsset(Document *doc, const String &asset) {
	bool isDocFile = doc->isFileExists(asset);
	if (!isDocFile) {
		StringView urlView(asset);
		if (urlView.is("http://") || urlView.is("https://")) {
			return true;
		}
	}
	return false;
}

bool CommonSource::onExternalAssets(Document *doc, const Set<String> &assets) {
	for (auto &it : assets) {
		if (isExternalAsset(doc, it)) {
			auto n_it = _networkAssets.find(it);
			if (n_it == _networkAssets.end()) {
				auto a_it = _networkAssets.emplace(it, AssetData{it}).first;
				AssetData * data = &a_it->second;
				data->asset.setCallback(std::bind(&CommonSource::onExternalAssetUpdated, this, data, std::placeholders::_1));
				addAssetRequest(data);
				acquireAsset(it, [this, data] (Asset *a) {
					if (a) {
						data->asset = a;
						if (data->asset->isReadAvailable()) {
							if (data->asset->tryReadLock(this)) {
								readExternalAsset(*data);
								data->asset->releaseReadLock(this);
							}
						}
						if (data->asset->isDownloadAvailable()) {
							data->asset->download();
						}
					}
					removeAssetRequest(data);
				});
				log::text("Asset", it);
			}
		}
	}
	return !hasAssetRequests();
}

void CommonSource::onExternalAssetUpdated(AssetData *a, data::Subscription::Flags f) {
	if (f.hasFlag((uint8_t)Asset::Update::FileUpdated)) {
		bool updated = false;
		if (a->asset->tryReadLock(this)) {
			if (readExternalAsset(*a)) {
				updated = true;
			}
			a->asset->releaseReadLock(this);
		}
		if (updated) {
			if (_document) {
				_dirty = true;
			}
			onDocument(this);
		}
	}
}

bool CommonSource::readExternalAsset(AssetData &data) {
	data.meta.type = data.asset->getContentType();
	if (StringView(data.meta.type).is("image/")) {
		auto tmpImg = data.meta.image;
		size_t w = 0, h = 0;
		if (Bitmap::getImageSize(data.asset->getFilePath(), w, h)) {
			data.meta.image.width = w;
			data.meta.image.height = h;
		}

		if (data.meta.image.width != tmpImg.width || data.meta.image.height != tmpImg.height) {
			return true;
		}
	} else if (StringView(data.meta.type).is("text/css")) {
		auto d = filesystem::readTextFile(data.asset->getFilePath());
		data.meta.css = Rc<layout::CssDocument>::create(StringView(d));
		return true;
		log::format("readAsset", "%s css", data.originalUrl.c_str());
	}
	return false;
}

void CommonSource::updateDocument() {
	if (_documentAsset && _loadedAssetMTime > 0) {
		_loadedAssetMTime = 0;
	}

	tryLoadDocument();
}

Rc<font::FontSource> CommonSource::makeSource(AssetMap && map, bool schedule) {
	if (_document) {
		FontFaceMap faceMap(_fontFaces);
		auto &pages = _document->getContentPages();
		for (auto &it : pages) {
			font::FontSource::mergeFontFace(faceMap, it.second.fonts);
		}
		return Rc<font::FontSource>::create(std::move(faceMap), _callback, _scale, SearchDirs(_searchDir), std::move(map), false);
	}
	return Rc<font::FontSource>::create(FontFaceMap(_fontFaces), _callback, _scale, SearchDirs(_searchDir), std::move(map), false);
}

Rc<Document> CommonSource::openDocument(const String &path, const String &ct) {
	Rc<Document> ret;
	if (!_data.empty()) {
		ret = Document::openDocument(_data, ct);
	} else {
		ret = Document::openDocument(FilePath(path), ct);
	}

	if (ret) {
		if (ret->prepare()) {
			return ret;
		}
	}

	return Rc<Document>();
}

bool CommonSource::hasAssetRequests() const {
	return !_assetRequests.empty();
}
void CommonSource::addAssetRequest(AssetData *data) {
	_assetRequests.emplace(data);
}
void CommonSource::removeAssetRequest(AssetData *data) {
	if (!_assetRequests.empty()) {
		_assetRequests.erase(data);
		if (_assetRequests.empty()) {
			if (!_assetWaiters.empty()) {
				auto w = move(_assetWaiters);
				_assetWaiters.clear();
				for (auto &it : w) {
					it();
				}
			}
		}
	}
}

void CommonSource::waitForAssets(Function<void()> &&fn) {
	_assetWaiters.emplace_back(move(fn));
}

Vector<SyncRWLock *> CommonSource::getAssetsVec() const {
	Vector<SyncRWLock *> ret; ret.reserve(1 + _networkAssets.size());
	if (_documentAsset) {
		ret.emplace_back(_documentAsset);
	}
	for (auto &it : _networkAssets) {
		if (it.second.asset) {
			ret.emplace_back(it.second.asset);
		}
	}
	return ret;
}

bool CommonSource::isFileExists(const String &url) const {
	auto it = _networkAssets.find(url);
	if (it != _networkAssets.end() && it->second.asset) {
		return true;
	}

	if (_document) {
		return _document->isFileExists(url);
	}
	return false;
}

Pair<uint16_t, uint16_t> CommonSource::getImageSize(const String &url) const {
	auto it = _networkAssets.find(url);
	if (it != _networkAssets.end()) {
		if (StringView(it->second.meta.type).is("image/") && it->second.meta.image.width > 0 && it->second.meta.image.height > 0) {
			return pair(it->second.meta.image.width, it->second.meta.image.height);
		}
	}

	if (_document) {
		return _document->getImageSize(url);
	}
	return Pair<uint16_t, uint16_t>(0, 0);
}

Bytes CommonSource::getImageData(const String &url) const {
	auto it = _networkAssets.find(url);
	if (it != _networkAssets.end()) {
		if (StringView(it->second.meta.type).is("image/") && it->second.meta.image.width > 0 && it->second.meta.image.height > 0) {
			return filesystem::readFile(it->second.asset->getFilePath());
		}
	}

	if (_document) {
		return _document->getImageData(url);
	}
	return Bytes();
}

NS_RT_END