#include "fileloader.h"

#include <shv/iotqt/rpc/rpccall.h>
#include <shv/coreqt/log.h>

using namespace shv::chainpack;

namespace {
constexpr qsizetype CHUNK_SIZE = 16 * 1024;
}

//=========================================================
// AbstractFileLoader
//=========================================================
AbstractFileLoader::AbstractFileLoader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QByteArray data, QObject *parent)
	: QObject(parent)
	, m_connection(conn)
	, m_shvPath(shv_path)
	, m_data(data)
{
	connect(this, &FileDownloader::finished, this, &FileDownloader::deleteLater);
}

//=========================================================
// FileDownloader
//=========================================================
FileDownloader::FileDownloader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QObject *parent)
	: Super(conn, shv_path, {}, parent)
{
	auto rpc_call = shv::iotqt::rpc::RpcCall::create(m_connection)
			->setShvPath(m_shvPath)
			->setMethod("size");
	connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
		if (error.isValid()) {
			emit finished({}, tr("Get file size error: %1").arg(QString::fromStdString(error.toString())));
			return;
		}
		m_fileSize = result.toInt();
		downloadChunk();
	});
	rpc_call->start();
}

void FileDownloader::downloadChunk()
{
	if (m_data.size() < m_fileSize) {
		emit progress(static_cast<int>(m_data.size() / CHUNK_SIZE), chunkCnt());
		auto bytes_to_read = m_fileSize - m_data.size();
		// Cap bytes to read by file size
		// PLC can return more than file_size bytes if requested
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
			downloadChunk();
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

//=========================================================
// FileUploader
//=========================================================
FileUploader::FileUploader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QByteArray data, QObject *parent)
	: Super(conn, shv_path, data, parent)
{
	uploadChunk();
}

void FileUploader::uploadChunk()
{
	shvInfo() << m_bytesWritten << "/" << m_data.size();
	if (m_bytesWritten < m_data.size()) {
		emit progress(static_cast<int>(m_bytesWritten / CHUNK_SIZE), chunkCnt());
		auto bytes_to_write = std::min(m_data.size() - m_bytesWritten, CHUNK_SIZE);
		RpcValue::Blob blob(m_data.constData() + m_bytesWritten, m_data.constData() + m_bytesWritten + bytes_to_write);
		RpcValue::List params{{RpcValue(m_bytesWritten), blob}};
		auto rpc_call = shv::iotqt::rpc::RpcCall::create(m_connection)
				->setShvPath(m_shvPath)
				->setMethod("write")
				->setParams(params);
		connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this, bytes_to_write](const ::shv::chainpack::RpcValue &, const shv::chainpack::RpcError &error) {
			if (error.isValid()) {
				emit finished({}, tr("Write file chunk error: %1").arg(QString::fromStdString(error.toString())));
				return;
			}
			m_bytesWritten += bytes_to_write;
			uploadChunk();
		});
		rpc_call->start();

	}
	else {
		emit progress(chunkCnt(), chunkCnt());
		emit finished({}, {});
	}
}

int FileUploader::chunkCnt() const
{
	return static_cast<int>(m_data.size() / CHUNK_SIZE) + 1;
}

