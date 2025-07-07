#pragma once

#include <QObject>

namespace shv::iotqt::rpc { class ClientConnection; }

class FileDownloader : public QObject
{
	Q_OBJECT
public:
	FileDownloader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QObject *parent);

	Q_SIGNAL void progress(int n, int of);
	Q_SIGNAL void finished(QByteArray data, QString error);
private:
	void loadChunk();
	int chunkCnt() const;
private:
	shv::iotqt::rpc::ClientConnection *m_connection = nullptr;
	QString m_shvPath;
	qsizetype m_fileSize = 0;
	QByteArray m_data;
};

