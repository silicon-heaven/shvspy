#pragma once

#include <QObject>

namespace shv::iotqt::rpc { class ClientConnection; }

class AbstractFileLoader : public QObject
{
	Q_OBJECT
public:
	AbstractFileLoader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QByteArray data, QObject *parent);

	Q_SIGNAL void progress(int n, int of);
	Q_SIGNAL void finished(QByteArray data, QString error);
protected:
	shv::iotqt::rpc::ClientConnection *m_connection = nullptr;
	QString m_shvPath;
	QByteArray m_data;
};

class FileDownloader : public AbstractFileLoader
{
	Q_OBJECT

	using Super = AbstractFileLoader;
public:
	FileDownloader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QObject *parent);
	void start();
private:
	void downloadChunk();
	int chunkCnt() const;
private:
	qsizetype m_fileSize = 0;
};

class FileUploader : public AbstractFileLoader
{
	Q_OBJECT

	using Super = AbstractFileLoader;
public:
	FileUploader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QByteArray data, QObject *parent);
	int chunkCnt() const;
	void start();
private:
	void uploadChunk();
private:
	qsizetype m_bytesWritten = 0;
};

