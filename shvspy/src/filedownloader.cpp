#include "filedownloader.h"

#include <shv/iotqt/rpc/rpccall.h>
#include <shv/coreqt/log.h>

using namespace shv::chainpack;

namespace {
constexpr qsizetype CHUNK_SIZE = 16 * 1024;
}

FileDownloader::FileDownloader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QObject *parent)
	: QObject(parent)
	, m_connection(conn)
	, m_shvPath(shv_path)
{
	connect(this, &FileDownloader::finished, this, &FileDownloader::deleteLater);

	auto rpc_call = shv::iotqt::rpc::RpcCall::create(m_connection)
			->setShvPath(m_shvPath)
			->setMethod("size");
	connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
		if (error.isValid()) {
			emit finished({}, tr("Get file size error: %1").arg(error.toString()));
			return;
		}
		m_fileSize = result.toInt();
		loadChunk();
	});
	rpc_call->start();
}

void FileDownloader::loadChunk()
{
	if (m_data.size() < m_fileSize) {
		emit progress(static_cast<int>(m_data.size()) / CHUNK_SIZE, chunkCnt());
		auto bytes_to_read = m_fileSize - m_data.size();
		RpcValue::List params{{m_data.size(), std::min(bytes_to_read, CHUNK_SIZE)}};
		auto rpc_call = shv::iotqt::rpc::RpcCall::create(m_connection)
				->setShvPath(m_shvPath)
				->setMethod("read")
				->setParams(params);
		connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
			if (error.isValid()) {
				emit finished({}, tr("Get file chunk error: %1").arg(error.toString()));
				return;
			}
			if (!result.isBlob()) {
				emit finished({}, tr("Blob should be received"));
				return;
			}
			auto chunk = result.asBlob();
			m_data.append(QByteArrayView(chunk));
			loadChunk();
		});
		rpc_call->start();

	}
	else {
		emit progress(chunkCnt(), chunkCnt());
		emit finished(m_data, {});
	}
}

int FileDownloader::chunkCnt() const
{
	return static_cast<int>(m_fileSize / CHUNK_SIZE) + 1;
}
