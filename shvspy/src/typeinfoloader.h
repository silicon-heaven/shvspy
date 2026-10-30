#pragma once

#include <shv/core/utils/shvtypeinfo.h>

#include <QObject>
#include <QPointer>
#include <QStringList>

namespace shv::iotqt::rpc { class ClientConnection; }

class TypeInfoLoader : public QObject
{
	Q_OBJECT

public:
	TypeInfoLoader(shv::iotqt::rpc::ClientConnection *conn, const QString &shv_path, QObject *parent);

	void start();

	Q_SIGNAL void progress(const QString &message);
	Q_SIGNAL void finished(const shv::core::utils::ShvTypeInfo &type_info, const QString &property_path, const QString &error);

private:
	static QString sitePath(const QString &shv_path);
	void findDevice(const QStringList &device_paths);
	void loadTypeInfoFile(const QString &name);
	void downloadTypeInfoFile(const QString &hash);
	void parseTypeInfo(const QByteArray &data, const QString &hash);
	void finishWithError(const QString &error);

	static QHash<const shv::iotqt::rpc::ClientConnection*, QStringList> s_devicePaths;
	QPointer<shv::iotqt::rpc::ClientConnection> m_connection;
	QString m_sitePath;
	QString m_devicePath;
	QString m_typeInfoFile;
	QString m_propertyPath;
};
