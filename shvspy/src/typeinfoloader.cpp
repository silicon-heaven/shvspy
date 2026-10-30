#include "typeinfoloader.h"

#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/coreqt/log.h>
#include <shv/core/utils/shvpath.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

using namespace shv::chainpack;
using shv::core::utils::ShvTypeInfo;
using shv::iotqt::rpc::RpcCall;

namespace {
const auto SITES_NODE = QStringLiteral("sites");
const QStringList SHV_PATH_PREFIXES = { QStringLiteral("shv/"), QStringLiteral("history/") };

// file hash -> parsed type info, many sites share the same type info file
QHash<QString, ShvTypeInfo> &parsed_type_infos()
{
	static QHash<QString, ShvTypeInfo> s_parsed_type_infos;
	return s_parsed_type_infos;
}

void collect_device_paths(const RpcValue::Map &sites, const QString &path, QStringList &device_paths)
{
	for (const auto &[key, val] : sites) {
		if (key == "_meta") {
			continue;
		}
		auto node_path = path.isEmpty() ? QString::fromStdString(key) : path + '/' + QString::fromStdString(key);
		const auto &node = val.asMap();
		if (node.value("_meta").asMap().hasKey("type")) {
			device_paths << node_path;
		}
		else {
			collect_device_paths(node, node_path, device_paths);
		}
	}
}
}

// connection -> paths of devices in sites, loaded by getSites once per connection
QHash<const shv::iotqt::rpc::ClientConnection*, QStringList> TypeInfoLoader::s_devicePathsForConnection;

TypeInfoLoader::TypeInfoLoader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QObject *parent)
	: QObject(parent)
	, m_connection(conn)
{
	m_sitePath = sitePath(shv_path);
	connect(this, &TypeInfoLoader::finished, this, &TypeInfoLoader::deleteLater);
}

QString TypeInfoLoader::sitePath(const QString &shv_path)
{
	for (const auto &prefix : SHV_PATH_PREFIXES) {
		if (shv_path.startsWith(prefix)) {
			return shv_path.mid(prefix.size());
		}
	}
	return {};
}

void TypeInfoLoader::start()
{
	if (m_sitePath.isEmpty()) {
		finishWithError(tr("path does not start with %1").arg(SHV_PATH_PREFIXES.join(tr(" or "))));
		return;
	}
	if (!m_connection) {
		finishWithError(tr("not connected"));
		return;
	}
	if (auto it = s_devicePathsForConnection.constFind(m_connection.data()); it != s_devicePathsForConnection.constEnd()) {
		findDevice(*it);
		return;
	}
	emit progress(tr("Loading sites"));
	auto *rpc_call = RpcCall::create(m_connection)->setShvPath(SITES_NODE)->setMethod("getSites");
	connect(rpc_call, &RpcCall::maybeResult, this, [this](const RpcValue &result, const RpcError &error) {
		if (error.isValid()) {
			finishWithError(tr("cannot load %1: %2").arg(SITES_NODE, QString::fromStdString(error.toString())));
			return;
		}
		QStringList device_paths;
		collect_device_paths(result.asMap(), {}, device_paths);
		const auto *conn = m_connection.data();
		if (!s_devicePathsForConnection.contains(conn)) {
			connect(m_connection, &QObject::destroyed, m_connection, [conn]() { s_devicePathsForConnection.remove(conn); });
			s_devicePathsForConnection[conn] = device_paths;
		}
		findDevice(device_paths);
	});
	rpc_call->start();
}

void TypeInfoLoader::findDevice(const QStringList &device_paths)
{
	for (const auto &device_path : device_paths) {
		if (shv::core::utils::shvpath::startsWithPath(m_sitePath.toStdString(), device_path.toStdString())) {
			m_devicePath = device_path;
			m_propertyPath = m_sitePath.mid(device_path.size() + 1);
			getTypeInfoFileHash("typeInfo.cpon");
			return;
		}
	}
	finishWithError(tr("device for path %1 not found in %2").arg(m_sitePath, SITES_NODE));
}

void TypeInfoLoader::getTypeInfoFileHash(const QString &name)
{
	m_typeInfoFile = SITES_NODE + '/' + m_devicePath + "/_files/" + name;
	emit progress(tr("Checking hash of %1").arg(m_typeInfoFile));
	auto *rpc_call = RpcCall::create(m_connection)->setShvPath(m_typeInfoFile)->setMethod("hash");
	connect(rpc_call, &RpcCall::maybeResult, this, [this, name](const RpcValue &result, const RpcError &error) {
		if (error.isValid()) {
			if (error.code() != RpcError::MethodNotFound) {
				finishWithError(tr("cannot get hash of %1: %2").arg(m_typeInfoFile, QString::fromStdString(error.toString())));
			}
			// older devices have nodesTree.cpon instead of typeInfo.cpon
			else if (name == "typeInfo.cpon") {
				getTypeInfoFileHash("nodesTree.cpon");
			}
			else {
				finishWithError(tr("typeInfo.cpon or nodesTree.cpon not found in %1/%2/_files").arg(SITES_NODE, m_devicePath));
			}
			return;
		}
		auto hash = QString::fromStdString(result.asString()).toLower();
		auto &parsed = parsed_type_infos();
		if (auto it = parsed.constFind(hash); it != parsed.constEnd()) {
			emit finished(*it, m_propertyPath, {});
			return;
		}
		downloadTypeInfoFile(hash);
	});
	rpc_call->start();
}

void TypeInfoLoader::downloadTypeInfoFile(const QString &hash)
{
	emit progress(tr("Downloading %1").arg(m_typeInfoFile));
	auto *rpc_call = RpcCall::create(m_connection)->setShvPath(m_typeInfoFile)->setMethod("read");
	connect(rpc_call, &RpcCall::maybeResult, this, [this, hash](const RpcValue &result, const RpcError &error) {
		if (error.isValid()) {
			finishWithError(tr("cannot download %1: %2").arg(m_typeInfoFile, QString::fromStdString(error.toString())));
			return;
		}
		QByteArray data;
		if (result.isString()) {
			data = QByteArray::fromStdString(result.asString());
		}
		else {
			const auto &blob = result.asBlob();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
			data = QByteArray::fromRawData(reinterpret_cast<const char*>(blob.data()), blob.size());
#else
			data = QByteArrayView(blob).toByteArray();
#endif
		}
		parseTypeInfo(data, hash);
	});
	rpc_call->start();
}

void TypeInfoLoader::parseTypeInfo(const QByteArray &data, const QString &hash)
{
	std::string err;
	auto rv = RpcValue::fromCpon(data.toStdString(), &err);
	if (!err.empty()) {
		finishWithError(tr("cannot parse %1: %2").arg(m_typeInfoFile, QString::fromStdString(err)));
		return;
	}
	ShvTypeInfo type_info;
	try {
		type_info = ShvTypeInfo::fromRpcValue(rv);
	}
	catch (const std::exception &e) {
		finishWithError(tr("cannot load %1: %2").arg(m_typeInfoFile, e.what()));
		return;
	}
	if (!type_info.isValid()) {
		finishWithError(tr("invalid type info %1").arg(m_typeInfoFile));
		return;
	}
	parsed_type_infos()[hash] = type_info;
	emit finished(type_info, m_propertyPath, {});
}

void TypeInfoLoader::finishWithError(const QString &error)
{
	emit finished({}, m_propertyPath, error);
}
