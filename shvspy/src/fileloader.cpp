#include "fileloader.h"

#include <shv/iotqt/rpc/rpccall.h>
#include <shv/coreqt/log.h>

using namespace shv::chainpack;

//=========================================================
// AbstractFileLoader
//=========================================================
namespace {
constexpr qsizetype DOWNLOAD_CHUNK_SIZE = 16 * 1024;
}

AbstractFileLoader::AbstractFileLoader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QObject *parent)
	: QObject(parent)
	, m_connection(conn)
	, m_shvPath(shv_path)
{
	connect(this, &FileDownloader::finished, this, &FileDownloader::deleteLater);
}

//=========================================================
// FileDownloader
//=========================================================
FileDownloader::FileDownloader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QObject *parent)
	: Super(conn, shv_path, parent)
{
}

void FileDownloader::start()
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
		emit progress(static_cast<int>(m_data.size() / DOWNLOAD_CHUNK_SIZE), chunkCnt());
		auto bytes_to_read = m_fileSize - m_data.size();
		// Cap bytes to read by file size
		// PLC can return more than file_size bytes if requested
		RpcValue::List params{{m_data.size(), std::min(bytes_to_read, DOWNLOAD_CHUNK_SIZE)}};
		auto rpc_call = shv::iotqt::rpc::RpcCall::create(m_connection)
				->setShvPath(m_shvPath)
				->setMethod("read")
				->setParams(params);
		connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
			if (error.isValid()) {
				emit finished({}, tr("Get file chunk error: %1").arg(QString::fromStdString(error.toString())));
				return;
			}
			if (!result.isBlob()) {
				emit finished({}, tr("Blob should be received"));
				return;
			}
			const auto &chunk = result.asBlob();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
			auto data = QByteArray::fromRawData(reinterpret_cast<const char*>(chunk.data()), chunk.size());
			m_data.append(data);
#else
			m_data.append(QByteArrayView(chunk));
#endif
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
	return static_cast<int>(m_fileSize / DOWNLOAD_CHUNK_SIZE) + 1;
}

//=========================================================
// FileUploader
//=========================================================
FileUploader::FileUploader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QByteArray data, QObject *parent)
	: Super(conn, shv_path, parent)
{
	m_data = data;
}

void FileUploader::uploadChunk()
{
	shvDebug() << m_bytesWritten << "/" << m_data.size();
	if (m_bytesWritten < m_data.size()) {
		emit progress(static_cast<int>(m_bytesWritten / m_chunkSize), chunkCnt());
		auto bytes_to_write = std::min(m_data.size() - m_bytesWritten, m_chunkSize);
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
	return static_cast<int>(m_data.size() / m_chunkSize) + 1;
}

void FileUploader::start()
{
	auto rpc_call = shv::iotqt::rpc::RpcCall::create(m_connection)
			->setShvPath(m_shvPath)
			->setMethod("stat");
	connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
		if (error.isValid()) {
			emit finished({}, tr("Get file size error: %1").arg(QString::fromStdString(error.toString())));
			return;
		}
		static constexpr int MAX_WRITE = 5;
		m_chunkSize = result.asIMap().value(MAX_WRITE).toInt();
		if (m_chunkSize <= 0) {
			emit finished({}, tr("Get file stat MaxWrite error."));
			return;
		}
		uploadChunk();
	});
	rpc_call->start();
}

